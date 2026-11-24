#!/usr/bin/env python3
"""Stage exact owned tree collision in private city physics; never applies it."""
from pathlib import Path
import hashlib,json,math,re,sys
import numpy as np
from visual_ground_finish import GLB

ROOT=Path(__file__).resolve().parents[1]
HERE=ROOT/'docs/arrival-upgrade/2026-11-05'
PROJECT=ROOT/'build/journey-opening/candidate-v15/project'
SOURCE=HERE/'SOURCE_TREE_PROXIES.json'
OUT=HERE/'tree-physics-v2'

def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(p,v):
    with p.open('x') as f:json.dump(v,f,indent=2,allow_nan=False);f.write('\n')
def values(v):return ' '.join(format(float(x),'.17g') for x in v)
def body(identifier,name,shape,position,size):
    return f'body {identifier} "{name}" {shape} 0 0 0 '+values(position)+' 0 0 0 1 '+values(size)+' .6 .01 1000 0 1 0 0 0 0 0 0 1'
def native_pose(p):
    # Match the production placement reader's float32 pose fields; XZ stay
    # double. Current selected tree rows are upright and uniformly scaled.
    f=p['row'].split();assert len(f)==14
    scale=float(np.float32(float(f[4])));yaw=float(np.float32(float(f[3])*math.pi/180))
    sx,sy,sz=[float(np.float32(float(f[i])))for i in (10,11,12)]
    assert abs(float(f[8]))+abs(float(f[9]))==0 and sx==sy==sz==1
    c,s=math.cos(yaw),math.sin(yaw)
    rotation=np.array([[c,0,s],[0,1,0],[-s,0,c]],float)
    translation=np.array([p['x'],float(np.float32(float(f[13]))),p['z']],float)
    return translation,rotation,scale

def main():
    assert not OUT.exists();OUT.mkdir(parents=True)
    assert sha(SOURCE)=='f748c79a0664a3b1a2955893aecd315c8b57bd5e1ceed63b1853bbeeea78e9b7'
    data=json.loads(SOURCE.read_text());assert Path(data['art_candidate'])==PROJECT
    protected=['city.physics','city.scene','city.sky','world.studio','opening/arrival-camp.settings','opening/first-journey.settings','opening/first-journey.routes.json','ground/countryside.height','ground/countryside.coverage']
    protected.extend(str(p.relative_to(PROJECT))for p in sorted((PROJECT/'scenery').glob('*.txt')))
    inputs={n:sha(PROJECT/n)for n in protected}
    raw=(PROJECT/'city.physics').read_bytes();assert raw.startswith(b'VALWOOD_PHYSICS 1\n') and raw.endswith(b'\n')
    ids=list(map(int,re.findall(rb'^body\s+(\d+)\s',raw,re.M)));assert len(ids)==64 and len(set(ids))==len(ids)
    old_v=len(re.findall(rb'^vertex\s',raw,re.M));old_t=len(re.findall(rb'^triangle\s',raw,re.M))
    rows=[];report=[];model_inputs={};identifier=max(ids);added_v=added_t=0
    # Check that the exact sealed64 authored rows are the actual C15 rows.
    actual=[]
    aliases={r['alias'] for r in data['rows']}
    for f in sorted((PROJECT/'scenery').glob('*.txt')):
        actual.extend(line for line in f.read_text().splitlines()if line.split()and line.split()[0]in aliases)
    sealed=[p['row']for r in data['rows']for p in r['final_placements']]
    assert sorted(actual)==sorted(sealed) and len(sealed)==64
    for source in data['rows']:
        for transform in source['source_transform_objects']:
            t=transform['data'];assert t['m_LocalPosition']==dict(x=0.,y=0.,z=0.) and t['m_LocalScale']==dict(x=1.,y=1.,z=1.) and t['m_LocalRotation']==dict(x=0.,y=0.,z=0.,w=1.)
        alias=source['alias'];path=PROJECT/('models/arrival-upgrade/'+alias+'_near.glb');model_inputs[str(path.resolve())]=sha(path)
        assert model_inputs[str(path.resolve())] in source['cooked_lods_sha256'].values() if isinstance(source['cooked_lods_sha256'],dict) else model_inputs[str(path.resolve())] in source['cooked_lods_sha256']
        mesh=GLB(path);primitive=mesh.doc['meshes'][0]['primitives'][0]
        assert mesh.doc['materials'][primitive['material']]['alphaCutoff']==0
        vertices=mesh.read(primitive['attributes']['POSITION']).astype(float);triangles=mesh.read(primitive['indices']).reshape(-1,3).astype(int)
        shift=source['common_glb_y_shift'];assert shift==-source['pivot_offset_native'][1]
        for p in source['final_placements']:
            identifier+=1;translation,rotation,scale=native_pose(p)
            item=dict(body_id=identifier,alias=alias,placement_row=p['row'],translation_native=translation.tolist(),rotation_native=rotation.tolist(),scale_native_float32=scale,source_guid=source['source_guid'],source_common_glb_y_shift=shift)
            if source['source_capsules']:
                assert len(source['source_capsules'])==1
                capsule=source['source_capsules'][0];assert capsule['m_Enabled'] and not capsule['m_IsTrigger'] and capsule['m_Direction']==1
                cc=capsule['m_Center'];local=np.array([cc['x'],cc['y']+shift,-cc['z']]);center=translation+rotation@(local*scale)
                radius=capsule['m_Radius']*scale;half=(capsule['m_Height']/2-capsule['m_Radius'])*scale;assert radius>.01 and half>.01
                rows.append(body(identifier,f'Arrival source capsule {alias}',2,center,[radius,half,radius]))
                item.update(shape='source_capsule',position=center.tolist(),radius=radius,cylinder_half_height=half,source_capsule=capsule,probe_xz=center[[0,2]].tolist(),source_root_world_y=float(translation[1]+shift*scale),minimum_y=float(center[1]-half-radius),maximum_y=float(center[1]+half+radius))
            else:
                assert alias=='nm_arrival_oak04'
                local=(vertices*scale)@rotation.T
                # Render-source bark includes repeated-index/coincident faces.
                # The production CPU physics validator requires cross length
                # >=1e-8. Omit only those zero-area faces after final scaling.
                face=local[triangles]
                area=np.linalg.norm(np.cross(face[:,1]-face[:,0],face[:,2]-face[:,0]),axis=1)
                kept=area>=1e-8;collision_triangles=triangles[kept]
                assert int((~kept).sum())==172 and len(collision_triangles)==2240
                rows.append(body(identifier,'Arrival exact opaque Oak04 bark',3,translation,[.5,.5,.5]))
                rows.extend(f'vertex {identifier} '+values(v)for v in local)
                rows.extend(f'triangle {identifier} '+' '.join(str(int(i))for i in t)for t in collision_triangles)
                added_v+=len(local);added_t+=len(collision_triangles)
                # Intersection of actual opaque bark with the source soil
                # plane identifies a useful independent near-trunk query XZ.
                soil=shift;cross=[]
                for tri in vertices[triangles]:
                    for a,b in zip(tri,np.roll(tri,-1,axis=0)):
                        if (a[1]-soil)*(b[1]-soil)<0:cross.append(a+(b-a)*((soil-a[1])/(b[1]-a[1])))
                assert len(cross)>4;cross=np.asarray(cross);root=cross.mean(0);root[1]=soil;world_root=translation+rotation@(root*scale)
                item.update(shape='exact_source_bark_mesh',position=translation.tolist(),vertices=len(local),triangles=len(collision_triangles),source_triangle_count=len(triangles),omitted_degenerate_source_triangle_indices=np.flatnonzero(~kept).tolist(),omitted_degenerate_face_count=int((~kept).sum()),production_area_threshold=1e-8,minimum_kept_cross_length=float(area[kept].min()),mesh_model_sha256=sha(path),source_bark_primitive=0,source_nondegenerate_indices_exact=True,source_root_world_y=float(translation[1]+shift*scale),probe_xz=world_root[[0,2]].tolist(),soil_bark_crossings=len(cross),minimum_world=(translation+local.min(0)).tolist(),maximum_world=(translation+local.max(0)).tolist())
            report.append(item)
    assert len(report)==64 and sum(r['shape']=='source_capsule'for r in report)==41
    assert len(ids)+len(report)<=512 and old_v+added_v<=147456 and old_t+added_t<=294912
    result=raw+('\n'.join(rows)+'\n').encode('ascii');assert result.startswith(raw) and len(result)<=16*1024*1024
    (OUT/'city.physics').write_bytes(result)
    assert inputs=={n:sha(PROJECT/n)for n in protected}
    assert all(sha(p)==s for p,s in model_inputs.items())
    save(OUT/'PROXIES.json',dict(schema='valwood.arrival-tree-physics',version=1,staged=True,applied=False,project=str(PROJECT),source_records=str(SOURCE),source_records_sha256=sha(SOURCE),source_files_sha256=inputs,source_models_sha256=model_inputs,source_physics_sha256=sha(PROJECT/'city.physics'),source_physics_bytes=len(raw),physics=str(OUT/'city.physics'),physics_sha256=sha(OUT/'city.physics'),old_body_count=len(ids),new_body_count=len(ids)+len(report),old_body_ids=ids,old_physics_byte_exact_prefix=True,added_vertices=added_v,added_triangles=added_t,new_bodies=report,ordinary_player_collision_owner='world_viewer.cpp city_physics load2921; move_camera capsule sweep652',viewer_sha256=sha(ROOT/'engine/world_viewer.cpp'),gpu_used=False))
    print(json.dumps(dict(staged=True,physics_sha256=sha(OUT/'city.physics'),bodies=len(ids)+len(report),source_capsules=41,exact_bark_meshes=23,added_vertices=added_v,added_triangles=added_t,bytes=len(result))))
if __name__=='__main__':main()
