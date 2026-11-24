#!/usr/bin/env python3
"""One bounded source rock silhouette with the selected BEP terrain rock PBR.

The source Rock_01 authored LOD2/3 are retained exactly, with original UV0 and
pivot. This is an explicitly new material assignment, not a claim to reproduce
the vendor's separate SpecGloss/detail/parallax rock material. It reuses all
three previously prepared countryside rock texture pages (zero new textures).
"""
from pathlib import Path
import json
import numpy as np
from cook_bep_reforged_vegetation import Cook,SOURCE,ROOT,normalize

OUT=ROOT/'assets/purchased/bep_reforged/countryside-rock'

def main():
    c=Cook(OUT);manifest=json.loads((SOURCE/'fbx-meshes/manifest.json').read_text());row=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-materials/ground-materials.json').read_text())['rocky']
    c.rows['BEP_Terrain_RockWall']=row;geometry=[];sources=[]
    for name in ['Rock_01_LOD_02','Rock_01_LOD_03']:
        record=next(m for m in manifest['meshes']if m['object']==name);z=np.load(SOURCE/record['arrays']);v={k:z[k].copy()for k in z.files if not k.startswith('indices_')}
        for key in ['position','normal']:v[key][:,2]*=-1
        v['normal']=normalize(v['normal']);v['tangent'][:,2]*=-1;v['tangent'][:,:3]=normalize(v['tangent'][:,:3]);v['uv0'][:,1]=1-v['uv0'][:,1];v['uv1'][:,1]=1-v['uv1'][:,1]
        ix=z['indices_0'].copy();p=v['position'][ix];bad=np.einsum('ij,ij->i',np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]),v['normal'][ix].sum(1))<0;ix[bad]=ix[bad,::-1]
        geometry.append((v,[(ix,'BEP_Terrain_RockWall')]));sources.append(record)
    miny=min(float(v['position'][:,1].min())for v,_ in geometry);stats=[];files=[]
    for i,(v,parts)in enumerate(geometry):
        file=OUT/'models'/('bep_nature_rock_'+('near'if i==0 else'mid')+'.glb');stats.append(c.emit(file,v,parts,miny,source_ao=False));files.append(str(file.resolve()))
    asset=dict(name='BEP_Reforged_medium_rock',files=files,pivot_offset_native=[0,miny,0],bounds=stats[0]['bounds'],stats=stats,
        native=dict(alias='bep_nature_rock',vegetation=0,collision=1,distance=320,split=50),source_geometry='Exact Rock_01 source authored LOD2/3; no custom decimation',
        material_assignment='New dressing material: selected source BEP RockWall TerrainLayer PBR, original source rock UV0 retained; original separate rock SpecGloss/detail/parallax shader not used')
    doc=dict(schema=1,assets=[asset],placements=[],cameras=[],additional_texture_GPU_bytes=0,shared_material_row=row,source_geometry=sources,source_bounds_validation=manifest['unity_prefab_bounds_validation'][0])
    (OUT/'catalog.json').write_text(json.dumps(doc,indent=2,allow_nan=False)+'\n');print('ROCK READY',OUT/'catalog.json',[s['triangles']for s in stats],flush=True)

if __name__=='__main__':main()
