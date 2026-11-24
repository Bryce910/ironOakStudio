#!/usr/bin/env python3
"""Isolated 2K candidates for the three stone materials used by hero columns."""
from pathlib import Path
import collections,json,re
from cook_nature_showcase import Source,DEFAULT_SOURCE,DEFAULT_EXCLUDED,ROOT
from bake_nature_material_fidelity import FidelityBaker

def main():
    source=Source(DEFAULT_SOURCE);names={'M_Pillar_stone_01','M_Pillar_stone_02','M_Wall_03_Main'}
    output=ROOT/'assets/purchased/nature_castle/showcase-closeup-materials';output.mkdir(exist_ok=True)
    parts=collections.defaultdict(list);seen=set();slots=collections.defaultdict(list)
    # Match logical_units/local_material_parts' exact first-coverage order.
    # Avoid loading all unrelated scene geometry merely to sort placements.
    for p in source.doc['placements']:
        if p['group']in DEFAULT_EXCLUDED:continue
        if p['materials']and all('M_Lava_River_'in path for path in p['materials']):continue
        key=(p['group'],re.sub(r'(?i)LOD[ _]?[0-9]+','LOD',p['hierarchy']),tuple(round(float(v),5)for v in p['matrix']))
        slots[key].append(p)
    ordered=(p for key,ps in sorted(slots.items())for p in ps)
    order={}
    for p in ordered:
        if not p['lod_mask']&1:continue
        for sub,path in enumerate(p['materials']):
            if Path(path).stem not in names:continue
            key=(p['mesh'],sub,path)
            if key in seen:continue
            v,indices=source.mesh(p['mesh']);parts[path].append((v,indices[sub]));seen.add(key)
            order.setdefault(path,[]).append({'mesh':p['mesh'],'submesh':sub,'hierarchy':p['hierarchy']})
    (output/'representative-order.json').write_text(json.dumps(order,indent=2)+'\n')
    baker=FidelityBaker(parts,output,2048,1024);results={}
    for path in sorted(parts):
        results[path]=baker.make(path);print('DETAIL2K',Path(path).stem,results[path]['status'],flush=True)
        (output/'materials.json').write_text(json.dumps(results,indent=2)+'\n')
    print('READY',output/'materials.json',flush=True)

if __name__=='__main__':main()
