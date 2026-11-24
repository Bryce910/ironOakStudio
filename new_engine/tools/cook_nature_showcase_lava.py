#!/usr/bin/env python3
"""Export original lava river/lake/emission geometry as four native extras.

Preserves original per-renderer placements and UVs. The caller supplies source
RAM-shader material candidates baked into native PBR/emissive maps.
"""
from pathlib import Path
import argparse,collections,json,numpy as np
import cook_nature_showcase as base
from align_nature_showcase_lods import align
from prepare_nature_lava_materials import route_triangles

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--materials',type=Path,required=True);parser.add_argument('--source',type=Path,default=base.DEFAULT_SOURCE);parser.add_argument('--output',type=Path,default=base.ROOT/'assets/purchased/nature_castle/showcase_lava');args=parser.parse_args()
    args.output.mkdir(exist_ok=True);source=base.Source(args.source);results=json.loads(args.materials.read_text());
    # Constantredemissionisalreadyconvertedfaithfullyinthearchitecturecache.
    cached=json.loads((base.DEFAULT_OUTPUT/'materials.json').read_text())
    for key,info in cached.items():
        if key.endswith('/M_Emissive_Red.mat'):results[key]=info
    emission_path='Assets/NatureManufacture Assets/Castle and Dungeon/Architecture/Global Materials/M_Emissive_Red.mat'
    if emission_path not in results:
        smoke=json.loads((base.ROOT/'docs/stronghold/2026-11-01/showcase/projection-smoke/report.json').read_text())
        results[emission_path]=smoke['M_Emissive_Red']
    selected=collections.defaultdict(list);placements=collections.Counter()
    for p in source.doc['placements']:
        if p['group']!='Emission'and not any('M_Lava_River_'in path for path in p['materials']):continue
        original,indices=source.mesh(p['mesh']);matrix=np.asarray(p['matrix'],np.float64).reshape(4,4,order='F');v=original.copy();v[:,:3]=original[:,:3]@matrix[:3,:3].T+matrix[:3,3];v[:,3:6]=original[:,3:6]@np.linalg.inv(matrix[:3,:3]);v[:,3:6]/=np.maximum(np.linalg.norm(v[:,3:6],axis=1,keepdims=True),1e-10)
        for sub,index in enumerate(indices):
            path=source.material(p,sub)
            if p['group']!='Emission'and'M_Lava_River_'not in path:continue
            routes=route_triangles(path,v,index)if'M_Lava_River_'in path else[(path,index)]
            assert sum(len(routed)for variant,routed in routes)==len(index),'Lava routing changed triangle coverage'
            for variant,routed in routes:
                info=results[variant]
                if info['status']!='candidate':raise ValueError('Lava material is not ready: '+variant)
                vertex=v.copy();vertex[:,6:8]=vertex[:,6:8]*np.asarray(info.get('uv_scale',[1,1]),np.float32)+np.asarray(info.get('uv_offset',[0,0]),np.float32)
                for lod in[0,1]:
                    if p['lod_mask']&(1<<lod):selected[path].append(({'name':p['name'],'lod':lod,'material':variant},vertex,routed))
            placements[path]+=1
    base.install_deduplicated_writer(results);catalog=[]
    for path,parts in sorted(selected.items()):
        name='nms_lava_'+Path(path).stem.lower().replace('m_','',1)
        record=base.emit_native_asset({'name':name},parts,results,args.output/'chunks',winter=False);record.update(source_material=path,source_placements=placements[path],source_flow_adaptation='Original heatpaint routed pertriangle to source-tiled cold/medium/hot PBR and emissive layers; staticflow');align(record);catalog.append(record);print('LAVA',name,record['triangles'],flush=True)
    base.atomic_json(args.output/'catalog.json',catalog);base.atomic_json(args.output/'READY.json',{'catalog':str(args.output/'catalog.json'),'assets':len(catalog),'native_static_emissive_adaptation':True});print('LAVA READY',len(catalog),'assets')
if __name__=='__main__':main()
