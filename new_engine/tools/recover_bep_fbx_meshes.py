#!/usr/bin/env python3
"""Selective BEP source FBX geometry recovery; Blender background, no rendering.

python3 tools/recover_bep_fbx_meshes.py --prepare
blender -b -t 2 --disable-autoexec --factory-startup --python-exit-code 1 \
    --python tools/recover_bep_fbx_meshes.py
"""
from pathlib import Path
import hashlib
import json
import sys
import zipfile

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.local-tools/bep-source'
NAMES=['Rock_01','Rock_02','Bush01_01','Bush03_01']


def prepare():
    records=[]
    with zipfile.ZipFile('/home/computer/Downloads/Nature.zip')as archive:
        for name in NAMES:
            member='Nature/'+name+'.fbx'
            info=archive.getinfo(member)
            assert info.file_size<8*1024*1024
            data=archive.read(info)
            path=OUT/'fbx-source'/member
            path.parent.mkdir(parents=True,exist_ok=True)
            if path.exists():assert path.read_bytes()==data
            else:path.write_bytes(data)
            records.append(dict(archive_member=member,bytes=len(data),crc32=f'{info.CRC:08x}',sha256=hashlib.sha256(data).hexdigest()))
    (OUT/'fbx-source'/'provenance.json').write_text(json.dumps(records,indent=2)+'\n')
    print('READY preserved',sum(r['bytes']for r in records),'original FBX bytes')


def export():
    import bpy
    import numpy as np
    dest=OUT/'fbx-meshes';dest.mkdir(exist_ok=True)
    basis=np.array([[-1,0,0],[0,0,1],[0,-1,0]],dtype=np.float64)
    result=[]
    for name in NAMES:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(OUT/'fbx-source/Nature'/f'{name}.fbx'),use_anim=False)
        for obj in bpy.data.objects:
            if obj.type!='MESH':continue
            mesh=obj.data;mesh.calc_loop_triangles()
            assert mesh.uv_layers.active is not None
            mesh.calc_tangents(uvmap=mesh.uv_layers.active.name)
            world=np.array(obj.matrix_world,dtype=np.float64)
            linear=basis@world[:3,:3]
            normal_matrix=np.linalg.inv(linear).T
            position=[];normal=[];uv0=[];tangent=[];color=[];indices_by_slot={};uv1=[]
            for tri in mesh.loop_triangles:
                indices=[]
                for li in tri.loops:
                    loop=mesh.loops[li];v=np.array(mesh.vertices[loop.vertex_index].co)
                    p=basis@(world[:3,:3]@v+world[:3,3]);n=normal_matrix@np.array(loop.normal);n/=max(np.linalg.norm(n),1e-20)
                    t=linear@np.array(loop.tangent);t-=n*np.dot(n,t);t/=max(np.linalg.norm(t),1e-20)
                    position.append(p);normal.append(n);uv0.append(list(mesh.uv_layers.active.data[li].uv))
                    uv1.append(list(mesh.uv_layers[1].data[li].uv)if len(mesh.uv_layers)>1 else [0,0])
                    tangent.append([*t,-loop.bitangent_sign])
                    c=[1,1,1,1]
                    if mesh.color_attributes:
                        attr=mesh.color_attributes[0]
                        if attr.domain=='CORNER':c=list(attr.data[li].color)
                        elif attr.domain=='POINT':c=list(attr.data[loop.vertex_index].color)
                    color.append(c);indices.append(len(position)-1)
                indices_by_slot.setdefault(tri.material_index,[]).append(indices)
            values={k:np.ascontiguousarray(v,dtype=np.float32)for k,v in dict(position=position,normal=normal,uv0=uv0,uv1=uv1,tangent=tangent,color=color).items()}
            for slot,indices in indices_by_slot.items():values['indices_'+str(slot)]=np.array(indices,dtype=np.int32)
            safe=''.join(c if c.isalnum()or c in '_-'else'_'for c in obj.name)
            path=dest/f'{name}-{safe}.npz';np.savez_compressed(path,**values)
            p=values['position'];native=p*np.array([1,1,-1],dtype=np.float32);c=values['color']
            result.append(dict(source=name+'.fbx',object=obj.name,mesh_name=mesh.name,arrays=str(path.relative_to(OUT)),
                               vertices=len(p),triangles=sum(len(v)for v in indices_by_slot.values()),
                               materials=[m.name if m else None for m in mesh.materials],
                               bounds_unity=[p.min(0).tolist(),p.max(0).tolist()],bounds_native_unpivoted=[native.min(0).tolist(),native.max(0).tolist()],
                               vertex_color_min=c.min(0).tolist(),vertex_color_max=c.max(0).tolist(),vertex_color_mean=c.mean(0).tolist(),
                               sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    report=dict(coordinates='Unity source axes from Blender FBX world: (-BlenderX, BlenderZ, -BlenderY), metres. No grounding/target-height adaptation. UVs unflipped; tangentW reflects basis determinant.',
                note='Original supplied Nature.zip FBX geometry; map by source object/mesh/material names. Validate against embedded Unity prefab bounds before substitution.',meshes=result)
    (dest/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print('READY',dest/'manifest.json',len(result),'source mesh objects')


if __name__=='__main__':
    if '--prepare'in sys.argv:prepare()
    else:export()
