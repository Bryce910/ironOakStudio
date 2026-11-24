#!/usr/bin/env python3
"""Stage layered arrival/first-road vegetation; only explicit --apply writes C13."""
from pathlib import Path
import argparse, copy, hashlib, json, math
import numpy as np
from build_arrival_camp_assets import terrain
from countryside_contact import root_points, fit_plane, errors
from build_village_showcase_project import decompose
from build_opening_prologue import read_glb, array
from stage_camp_visual_polish import write_glb
from visual_ground_finish import segment_distance

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'build/journey-opening/candidate-v12/project'
OUT=ROOT/'docs/scenery-polish/2026-11-05/landscape-v1'
CENTER=np.array([-1486.37664868499,18.933204601097398,-10224.9106110642])
W=100800.;D=W*2818/5009

def sha(p):
    with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def dump(p,x):
    with p.open('x')as f:json.dump(x,f,indent=2,allow_nan=False);f.write('\n')

def stage():
    assert not (OUT/'LANDSCAPE_REVIEW.json').exists() and not (OUT/'PLACEMENTS.json').exists(),'Fresh bank required'
    OUT.mkdir(parents=True,exist_ok=True);(OUT/'models').mkdir()
    protected=['world.studio','scenery/authored_instances.txt','city.physics','ground/countryside.height','ground/countryside.coverage','opening/arrival-camp.settings','opening/first-journey.settings','opening/first-journey.routes.json']
    before={p:sha(SOURCE/p)for p in protected}
    surface,catalog_path=terrain(SOURCE);catalog=json.loads(catalog_path.read_text());origin=np.asarray(json.loads((SOURCE/'city_showcase.json').read_text())['origin'])
    assets={a.get('native',{}).get('alias'):a for a in catalog['assets']if a.get('native',{}).get('alias','').startswith('bep_nature_')}
    grass=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-grass-clumps/source-audit.json').read_text());sites={a['alias']:a['tuft_sites_native']for a in grass['geometry']}
    roots={alias:root_points(a,sites)for alias,a in assets.items()}
    definitions={s[1]:s for line in(SOURCE/'world.studio').read_text().splitlines()if len(s:=line.split())>3 and s[0]=='asset'}
    routes=json.loads((SOURCE/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
    for points in routes.values():
        p=np.asarray(points)[:,[0,2]];segments.extend(np.stack((p[:-1],p[1:]),axis=1))
    qa=np.array([[-2.15,1.8],[2.2,1.8],[3.6,1],[3.6,-2],[1.9,-2],[1.9,-2.9],[1.9,-2],[3.6,-2],[3.6,3.5],[0,5.2]])+CENTER[[0,2]]
    segments.extend(np.stack((qa[:-1],qa[1:]),axis=1));segments=np.asarray(segments)
    actors=np.array([[-2.15,-.65],[2.2,-.65],[.1,-3.2]])+CENTER[[0,2]];landing=CENTER[[0,2]]+[0,5.2]
    existing=[]
    for file in(SOURCE/'scenery').glob('*.txt'):
        for line in file.read_text().splitlines():
            s=line.split()
            if len(s)>4 and s[0]in ['bep_nature_oak','bep_nature_beech']:
                existing.append([(float(s[1])-.5)*W,(float(s[2])-.5)*D])
    existing=np.asarray(existing);records=[];rejected=[];rng=np.random.default_rng(20261105)
    def add(alias,x,z,scale,yaw,purpose):
        a=assets[alias];root=roots[alias];angle=math.radians(yaw);c,s=math.cos(angle),math.sin(angle);m=np.eye(4);m[:3,:3]=np.array([[c,0,s],[0,1,0],[-s,0,c]])*scale
        m[:3,3]=[CENTER[0]+x-origin[0],float(surface.sample([[CENTER[0]+x,CENTER[2]+z]])[0])-origin[1],CENTER[2]+z-origin[2]]
        is_low=alias in ['bep_nature_fern','bep_nature_clover','bep_nature_grass_long','bep_nature_grass_broad']
        if is_low:m=fit_plane(m,root,surface,np.full(3,scale))
        else:
            e=errors(dict(matrix_native=m.T.ravel().tolist()),root,surface)[0]
            m[1,3]-=max(float(e.max()if'canopy'in purpose else np.percentile(e,90))+.025,0)
        world_root=m[:3,3]+origin;footprint=float(np.linalg.norm((root@m[:3,:3].T)[:,[0,2]],axis=1).max())
        lane=float(segment_distance(world_root[[0,2]][None,:],segments)[0]-footprint)
        npc=float(np.linalg.norm(actors-world_root[[0,2]],axis=1).min()-footprint)
        land=float(np.linalg.norm(landing-world_root[[0,2]])-footprint)
        collision=int(definitions[alias][4]);solid=collision!=0
        need_lane=2.0 if solid else .70;need_npc=3.0 if solid else 1.35
        if lane<need_lane or npc<need_npc or (solid and land<5.0)or(np.linalg.norm(world_root[[0,2]]-CENTER[[0,2]])<4.8):
            rejected.append(dict(alias=alias,local_xz=[x,z],purpose=purpose,lane=lane,npc=npc,landing=land));return False
        if'canopy'in purpose and len(existing)and np.linalg.norm(existing-world_root[[0,2]],axis=1).min()<3.7:
            rejected.append(dict(alias=alias,local_xz=[x,z],purpose=purpose,reason='existing trunk spacing'));return False
        # Rooted plants keep their authored underground stem plane. The
        # explicit pivot then reproduces exactly the loaded model transform.
        pp=m.copy();pp[:3,3]+=m[:3,:3]@np.asarray(a['pivot_offset_native'])+origin
        yy,pi,ro,sc,ex=decompose(pp);wx,wy,wz=pp[:3,3]
        row=f'{alias} {wx/W+.5:.17g} {wz/D+.5:.17g} {yy:.12g} {sc:.12g} 0 0 0 {pi:.12g} {ro:.12g} '+' '.join(format(float(v),'.12g')for v in ex)+f' {wy:.12g}'
        f=row.split();pose=dict(alias=alias,x=(float(f[1])-.5)*W,y=float(f[13]),z=(float(f[2])-.5)*D,yaw=float(f[3]),pitch=float(f[8]),roll=float(f[9]),scale=float(f[4]),sx=float(f[10]),sy=float(f[11]),sz=float(f[12]),burial=0,elevation=0,offset_y=0,angle_units='degrees')
        stat=a['stats'][0];bounds=a['bounds'];err=errors(dict(matrix_native=m.T.ravel().tolist()),root,surface)
        record=dict(**pose,row=row,purpose=purpose,local_xz=[x,z],collision=collision,minimum=bounds[0],maximum=bounds[1],model=str(SOURCE/'models'/(definitions[alias][2]+('_near.glb'if int(definitions[alias][5])==2 else'.glb'))),
            root_footprint_m=footprint,route_footprint_clearance_m=lane,npc_footprint_clearance_m=npc,landing_footprint_clearance_m=land,
            high_triangles=stat['triangles'],mid_triangles=a['stats'][1]['triangles'],high_contact_min=float(err[0].min()),high_contact_max=float(err[0].max()),mid_contact_min=float(err[1].min()),mid_contact_max=float(err[1].max()))
        records.append(record);return True
    canopy=[(-26,-18),(-15,-18),(-2,-21),(12,-30),(21,-30),(-30,-30),(-18,-45),(-10,-49),(2,-44),(13,-45),(-26,-80),(-12,-77),(1,-79),(18,-91),(26,-95),(-30,-100),(-5,-103),(9,-108)]
    for i,(x,z)in enumerate(canopy):add('bep_nature_beech'if i%4==1 else'bep_nature_oak',x,z,(.29+.02*(i%3))if i%4==1 else(.48+.035*(i%5)),(i*73+19)%360,'layered_canopy')
    anchors=[(-8,-1),(-8,-8),(-3,-10),(5,-10),(-11,-22),(0,-25),(14,-27),(25,-30),(8,3),(20,-7),(29,-10),(35,-25),(44,-20),(49,-7)]
    for i,(x,z)in enumerate(anchors):
        for j in range(2):add('bep_nature_bush_a'if(i+j)%2 else'bep_nature_bush_b',x+(j-.5)*1.45,z+(.6 if j else-.5),.16+.015*((i+j)%4),(i*81+j*67)%360,'woodland_understory')
    rock=[(8.2,5.6),(10.2,6.4),(7.3,7.6),(28,-25.3),(31,-25.9),(33,-24.4),(-8,1.5),(-10,-1),(-9,-3),(5,-10.8)]
    for i,(x,z)in enumerate(rock):add('bep_nature_rock',x,z,.32+.055*(i%4),(i*97)%360,'buried_rock_group')
    # Two scales of vegetation: dense organic pockets and thinner verge ends.
    low_anchors=anchors+[(-6,4),(-3,7.7),(1.5,8.0),(5.3,7.6),(10,-8),(17,-21),(23,-22),(31,-22)]
    targets={'bep_nature_fern':32,'bep_nature_clover':40,'grass':240}
    for family,count in targets.items():
        accepted=0
        for attempt in range(count*12):
            ax,az=low_anchors[attempt%len(low_anchors)];dx,dz=rng.normal(0,1.55,2);x,z=ax+dx,az+dz
            alias=('bep_nature_grass_long'if attempt%2 else'bep_nature_grass_broad')if family=='grass'else family
            scale=float(rng.uniform(.43,.82)if family=='grass'else rng.uniform(.48,.72)if family.endswith('fern')else rng.uniform(.32,.62))
            if add(alias,x,z,scale,float(rng.uniform(0,360)),'verge_'+family):accepted+=1
            if accepted==count:break
        assert accepted==count,(family,accepted,count)
    assert surface.fallback_queries==[0,0],surface.fallback_queries
    total=sum(r['high_triangles']for r in records);assert total<=320000,total
    # Remove the opaque orange litter islands entirely. Retain only the
    # original hearth char/ash/datum and their exact existing image bytes.
    source_model=SOURCE/'models/opening/camp_ground_finish.glb';doc,binary=read_glb(source_model);new=copy.deepcopy(doc);removed=0
    for mesh in new['meshes']:
        kept=[]
        for primitive in mesh['primitives']:
            name=new['materials'][primitive['material']].get('name','').lower()
            if'litter'in name:removed+=doc['accessors'][primitive['indices']]['count']//3
            else:kept.append(primitive)
        mesh['primitives']=kept
    assert removed>0
    quiet=OUT/'models/camp_ground_finish.glb';write_glb(quiet,new,binary)
    assert read_glb(quiet)[1]==binary
    paths={'schema':'valwood.arrival-landscape-placements','version':1,'source_project':str(SOURCE),'routes':routes,'landing':list(landing),'npcs_xz':actors.tolist(),'placements':records,'added_instance_collision_by_alias':{a:int(definitions[a][4])for a in sorted({r['alias']for r in records})},'new_solid_count':sum(r['collision']!=0 for r in records),'source_world_scenery_sha256':sha(ROOT/'engine/world_scenery.cpp')}
    dump(OUT/'PLACEMENTS.json',paths)
    (OUT/'appended_instances.txt').write_text('\n'.join(r['row']for r in records)+'\n')
    assert before=={p:sha(SOURCE/p)for p in protected}
    summary=dict(staged=True,source_project=str(SOURCE),source_inputs_sha256={str(SOURCE/p):d for p,d in before.items()},source_catalog=str(catalog_path),source_catalog_sha256=sha(catalog_path),counts={a:sum(r['alias']==a for r in records)for a in sorted({r['alias']for r in records})},instances=len(records),high_triangles=total,mid_triangles=sum(r['mid_triangles']for r in records),new_texture_bytes=0,new_assets=0,new_asset_definitions=0,
        quiet_ground_finish=dict(path=str(quiet),sha256=sha(quiet),source=str(source_model.resolve()),source_sha256=sha(source_model),binary_image_payload_exact=True,litter_triangles_removed=removed,ash_char_geometry_exact=True),
        placements_file=str(OUT/'PLACEMENTS.json'),placements_sha256=sha(OUT/'PLACEMENTS.json'),appended_rows_sha256=sha(OUT/'appended_instances.txt'),minimum_route_root_clearance_m=min(r['route_footprint_clearance_m']for r in records),minimum_solid_route_clearance_m=min(r['route_footprint_clearance_m']for r in records if r['collision']),fallback_queries=surface.fallback_queries,rejected_candidates=len(rejected),source_c12_unchanged=True,cpp_shader_physics_height_routes_unchanged=True,recipe_sha256=sha(Path(__file__)),runtime_collision_proof_pending=True,visual_acceptance_pending=True,gpu_used=False)
    dump(OUT/'LANDSCAPE_REVIEW.json',summary)
    print(json.dumps({k:summary[k]for k in ['staged','instances','high_triangles','mid_triangles','counts','new_texture_bytes','minimum_solid_route_clearance_m']}))

def apply(project):
    project=Path(project).resolve(strict=True);assert project==ROOT/'build/journey-opening/candidate-v13/project'
    review=json.loads((OUT/'LANDSCAPE_REVIEW.json').read_text());assert review['staged'] and not review['runtime_collision_proof_pending']
    for p,d in review['review_proofs_sha256'].items():assert sha(p)==d
    assert json.loads(Path(review['runtime_collision_proof']).read_text())['passed']
    assert json.loads((OUT/'CPU_MODELS.json').read_text())['passed']
    for p,d in review['source_inputs_sha256'].items():assert sha(p)==d
    target=project/'scenery/authored_instances.txt';assert target.is_file()and not target.is_symlink();source=(SOURCE/'scenery/authored_instances.txt').read_bytes();added=(OUT/'appended_instances.txt').read_bytes();assert sha(OUT/'appended_instances.txt')==review['appended_rows_sha256'];before=target.read_bytes();expected=source.rstrip()+b'\n'+added
    assert source.endswith(b'\n') and expected.startswith(source),'Original rows must stay byte-exact prefix'
    assert before in [source,expected],'Unexpected C13 authored scenery delta'
    alias=project/'models/opening/camp_ground_finish.glb';assert alias.is_symlink();asset=Path(review['quiet_ground_finish']['path']);assert sha(asset)==review['quiet_ground_finish']['sha256'];assert sha(alias)in [review['quiet_ground_finish']['source_sha256'],review['quiet_ground_finish']['sha256']]
    alias.unlink();alias.symlink_to(asset);target.write_bytes(expected)
    return dict(applied=True,source_c12_unchanged=True,only_project_file_changed='scenery/authored_instances.txt',appended_instances=review['instances'],model_alias_repointed='models/opening/camp_ground_finish.glb',existing_rows_exact_prefix=expected.startswith(source.rstrip()+b'\n'),gpu_used=False)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--apply',type=Path);a=p.parse_args()
    if a.apply:print(json.dumps(apply(a.apply),indent=2))
    else:stage()
