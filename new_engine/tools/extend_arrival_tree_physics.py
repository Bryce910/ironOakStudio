#!/usr/bin/env python3
"""Append final grove proxies to the validated private tree-physics payload."""
import json,hashlib,math,re
from pathlib import Path
import numpy as np
from stage_arrival_tree_physics import ROOT,HERE,PROJECT,SOURCE,sha,save,values,body,native_pose
from visual_ground_finish import GLB
BASE=HERE/'tree-physics-v2'
FINISH=HERE/'finish-v1/PLACEMENTS.json'
OUT=HERE/'tree-physics-v3'

def main():
    assert not OUT.exists();OUT.mkdir()
    baseline=json.loads((BASE/'PROXIES.json').read_text());raw=(BASE/'city.physics').read_bytes()
    assert sha(BASE/'city.physics')==baseline['physics_sha256']=='98005752699c9245d6f77e5b9f7ca6e3aad0cb8f8e39a2fd0b43ac61cf2804e5'
    assert raw.startswith((PROJECT/'city.physics').read_bytes())
    sources=json.loads(SOURCE.read_text());source_by_alias={s['alias']:s for s in sources['rows']}
    finish=json.loads(FINISH.read_text());assert finish['existing64tree_rows_unchanged'] and Path(finish['source_project'])==PROJECT
    poses=[p for p in finish['placements']if p['alias']in source_by_alias];assert len(poses)==24
    old_poses=[p['row']for s in sources['rows']for p in s['final_placements']]
    actual=[]
    for path in (PROJECT/'scenery').glob('*.txt'):
        actual.extend(line for line in path.read_text().splitlines()if line.split()and line.split()[0]in source_by_alias)
    assert set(old_poses)<=set(actual) and len(actual)in(64,88)
    assert set(actual)-set(old_poses)<=set(p['row']for p in poses)
    identifiers=list(map(int,re.findall(rb'^body\s+(\d+)\s',raw,re.M)));assert len(identifiers)==128
    rows=[];report=[];identifier=max(identifiers);added_v=added_t=0;models={}
    for p in poses:
        source=source_by_alias[p['alias']];alias=p['alias'];identifier+=1
        translation,rotation,scale=native_pose(p);shift=source['common_glb_y_shift']
        path=PROJECT/('models/arrival-upgrade/'+alias+'_near.glb');models[str(path.resolve())]=sha(path)
        assert models[str(path.resolve())] in source['cooked_lods_sha256'].values()
        item=dict(body_id=identifier,alias=alias,placement_row=p['row'],translation_native=translation.tolist(),rotation_native=rotation.tolist(),scale_native_float32=scale,source_guid=source['source_guid'],source_common_glb_y_shift=shift)
        if source['source_capsules']:
            capsule=source['source_capsules'][0];assert capsule['m_Direction']==1
            cc=capsule['m_Center'];local=np.array([cc['x'],cc['y']+shift,-cc['z']]);center=translation+rotation@(local*scale)
            radius=capsule['m_Radius']*scale;half=(capsule['m_Height']/2-capsule['m_Radius'])*scale
            rows.append(body(identifier,f'Arrival source capsule {alias}',2,center,[radius,half,radius]))
            item.update(shape='source_capsule',position=center.tolist(),radius=radius,cylinder_half_height=half,source_capsule=capsule,probe_xz=center[[0,2]].tolist(),source_root_world_y=float(translation[1]+shift*scale),minimum_y=float(center[1]-half-radius),maximum_y=float(center[1]+half+radius))
        else:
            assert alias=='nm_arrival_oak04';g=GLB(path);primitive=g.doc['meshes'][0]['primitives'][0]
            assert g.doc['materials'][primitive['material']]['alphaCutoff']==0
            vertices=g.read(primitive['attributes']['POSITION']).astype(float);triangles=g.read(primitive['indices']).reshape(-1,3).astype(int)
            local=(vertices*scale)@rotation.T;face=local[triangles];area=np.linalg.norm(np.cross(face[:,1]-face[:,0],face[:,2]-face[:,0]),axis=1)
            kept=area>=1e-8;collision=triangles[kept];assert int((~kept).sum())==172 and len(collision)==2240
            rows.append(body(identifier,'Arrival exact opaque Oak04 bark',3,translation,[.5,.5,.5]))
            rows.extend(f'vertex {identifier} '+values(v)for v in local)
            rows.extend(f'triangle {identifier} '+' '.join(str(int(i))for i in t)for t in collision)
            added_v+=len(local);added_t+=len(collision);cross=[]
            for tri in vertices[triangles]:
                for a,b in zip(tri,np.roll(tri,-1,axis=0)):
                    if (a[1]-shift)*(b[1]-shift)<0:cross.append(a+(b-a)*((shift-a[1])/(b[1]-a[1])))
            assert len(cross)>4;cross=np.asarray(cross);root=cross.mean(0);root[1]=shift;world_root=translation+rotation@(root*scale)
            item.update(shape='exact_source_bark_mesh',position=translation.tolist(),vertices=len(local),triangles=len(collision),source_triangle_count=len(triangles),omitted_degenerate_source_triangle_indices=np.flatnonzero(~kept).tolist(),omitted_degenerate_face_count=int((~kept).sum()),production_area_threshold=1e-8,minimum_kept_cross_length=float(area[kept].min()),mesh_model_sha256=sha(path),source_bark_primitive=0,source_nondegenerate_indices_exact=True,source_root_world_y=float(translation[1]+shift*scale),probe_xz=world_root[[0,2]].tolist(),soil_bark_crossings=len(cross),minimum_world=(translation+local.min(0)).tolist(),maximum_world=(translation+local.max(0)).tolist())
        report.append(item)
    result=raw+('\n'.join(rows)+'\n').encode('ascii');assert result.startswith(raw)
    assert len(result)<=16*1024**2 and len(re.findall(rb'^body\s',result,re.M))==152
    assert len(re.findall(rb'^vertex\s',result,re.M))<=147456 and len(re.findall(rb'^triangle\s',result,re.M))<=294912
    assert sha(BASE/'city.physics')==baseline['physics_sha256']
    (OUT/'city.physics').write_bytes(result)
    save(OUT/'PROXIES.json',dict(schema='valwood.arrival-tree-physics',version=3,staged=True,applied=False,project=str(PROJECT),source_records=str(SOURCE),source_records_sha256=sha(SOURCE),finish_placements=str(FINISH),finish_placements_sha256=sha(FINISH),base_proxies=str(BASE/'PROXIES.json'),base_proxies_sha256=sha(BASE/'PROXIES.json'),source_physics=str(BASE/'city.physics'),source_physics_sha256=sha(BASE/'city.physics'),source_physics_bytes=len(raw),original_city_physics_sha256=sha(PROJECT/'city.physics'),source_models_sha256=models,physics=str(OUT/'city.physics'),physics_sha256=sha(OUT/'city.physics'),old_body_count=128,new_body_count=152,old_body_ids=identifiers,old_physics_byte_exact_prefix=True,added_vertices=added_v,added_triangles=added_t,new_bodies=report,all_prior64_tree_rows_unchanged=True,source_tree_transform_policy='Same source identity/common minY and native float32 full placement TRS as accepted tree-physics-v2; no second terrain offset.',tool_sha256=sha(Path(__file__)),gpu_used=False))
    print(json.dumps(dict(staged=True,physics_sha256=sha(OUT/'city.physics'),bodies=152,added_source_capsules=sum(r['shape']=='source_capsule'for r in report),added_exact_bark_meshes=sum(r['shape']=='exact_source_bark_mesh'for r in report),added_vertices=added_v,added_triangles=added_t,bytes=len(result))))
if __name__=='__main__':main()
