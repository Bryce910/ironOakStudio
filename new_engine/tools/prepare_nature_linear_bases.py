#!/usr/bin/env python3
"""Rebake verified Unity Color-property RGB in an isolated base-only cache.

Normals, ARM, emissive textures, source geometry and initial catalogs are reused
without alteration. The returned material metadata is ready for GI GLB repack.
"""
from pathlib import Path
import argparse,copy,hashlib,json,types
from cook_nature_showcase import Source,logical_units,local_material_parts,DEFAULT_SOURCE
from bake_nature_material_fidelity import FidelityBaker,ROOT
from repair_nature_cutout_alpha import repair_material_alpha


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,default=DEFAULT_SOURCE)
    p.add_argument('--original-materials',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase/materials.json')
    p.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-linear')
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    original=json.loads(args.original_materials.read_text());source=Source(args.source)
    units,_=logical_units(source,types.SimpleNamespace(group=[],exclude_group=[]))
    parts=local_material_parts(source,units)
    size=max(info['size']for info in original.values()if info.get('status')=='candidate')
    arm=max(info['arm_size']for info in original.values()if info.get('status')=='candidate')
    baker=FidelityBaker(parts,args.output/'materials',size,arm);results={}
    for path,old in sorted(original.items()):
        if old.get('status')!='candidate':results[path]=old;continue
        period=6 if 'M_Marble_path_snow'in path or 'M_Marble_path_sand_black'in path else None
        base=baker.make(path,planar_period=period,base_only=True)
        if base['status']!='candidate':raise ValueError('Source base repair failed '+path+' '+str(base))
        info=copy.deepcopy(old);info['paths'].update(base['paths']);info['linear_color_properties']=base['linear_color_properties']
        info['original_material_fingerprint']=old['fingerprint'];info['base_color_fingerprint']=base['fingerprint']
        info['fingerprint']=hashlib.sha256((old['fingerprint']+base['fingerprint']).encode()).hexdigest()
        info['base_color_repair']='ShaderLab Color RGB decoded sRGB→linear; alpha/vector/float properties retained; normals/ARM/emissive reused'
        info=repair_material_alpha(path,info);results[path]=info
        target=args.output/'materials.json';temporary=target.with_suffix('.tmp');temporary.write_text(json.dumps(results,indent=2)+'\n');temporary.replace(target)
        print('LinearBase',info['name'],flush=True)
    print('Correctedmaterialmetadata',args.output/'materials.json',flush=True)


if __name__=='__main__':main()
