#!/usr/bin/env python3
"""Stage the optional first-contact campsite in a private opening revision."""
from pathlib import Path
import json, math, shutil, sys
import numpy as np
from check_opening_prologue import isolate_project, mutable_snapshot, sha
from build_visual_cohesion import private_path, write_json
from build_city_countryside import readfield, sample

ROOT=Path(__file__).resolve().parents[1]
DOC=ROOT/'docs/arrival-campsite/2026-11-04'
ART=ROOT/'docs/arrival-camp/2026-11-04'
DEST=ROOT/'build/opening-finward-reference/v15'
WIDTH=100800.;DEPTH=WIDTH*2818/5009

def values(row):return ' '.join(format(float(x),'.17g') for x in row)

def stage():
    source=ROOT/'build/opening-finward-reference/v14/project'
    assert not DEST.exists(),'Use a new private revision'
    art=json.loads((ART/'art/ART_CANDIDATE.json').read_text())
    assert json.loads((ART/'art/ACCEPTANCE.json').read_text())['passed']
    ground=json.loads((ART/'ground/GROUND_REVIEW.json').read_text())
    before=mutable_snapshot(source);project=DEST/'project';isolate_project(source,project)
    models=[]
    for row in art['models']:
        assert sha(Path(row['path']))==row['sha256']
        name='arrival_roast' if 'roast' in Path(row['path']).name else 'arrival_camp'
        names=[name+'.glb',name+'_near.glb'] if row['lod']=='near' else [name+'_mid.glb']
        for target in names:
            p=private_path(project,'models/opening/'+target);shutil.copyfile(row['path'],p)
            models.append(dict(relative_path='models/opening/'+target,sha256=sha(p),candidate=row['path']))
    for row in ground['replacements']:
        p=private_path(project,row['relative_path']);shutil.copyfile(row['candidate'],p)
        assert sha(p)==row['sha256'];models.append(row)
    center=art['fire']['world_position'].copy();center[1]=art['terrain_review']['center_ground_world_y']
    # Retained people face local +Z; the art inspection camera faces -Z.
    for a in art['actors']:
        a['source_yaw_radians']=a['yaw_radians']
        a['yaw_radians']=math.atan2(center[0]-a['world_feet'][0],center[2]-a['world_feet'][2])
    rows=['VALWOOD_ARRIVAL_CAMP 1','center '+values(center)]
    for i,a in enumerate(art['actors']):
        rows.append('actor '+str(i)+' '+values(a['world_feet']+[a['yaw_radians'],0]))
    (project/'opening/arrival-camp.settings').write_text('\n'.join(rows)+'\n')
    studio=(project/'world.studio').read_text().splitlines()
    studio+=['asset nm_opening_arrival_camp opening/arrival_camp 2 0 0 210 50 1',
             'asset nm_opening_arrival_roast opening/arrival_roast 2 0 0 100 30 1']
    fire=art['fire']['world_position'];studio.append('light '+values([fire[0],fire[1]+.5,fire[2],5.5,2.,1.,.52,.19,0.,-90.,180.,0.]))
    # Thin smoke above the hearth, using the existing particle editor format.
    studio.append('particle '+values([-1,0,1,fire[0],fire[1]+.55,fire[2],fire[1]+4.5,.16,.8,.18,.2,.55,1.,.6,.7,0.])+' 379 '+values([.48,.46,.43,0.]))
    assert sum(r.startswith('asset ') for r in studio)<=128
    (project/'world.studio').write_text('\n'.join(studio)+'\n')
    flames=(project/'showcase.flames').read_text().rstrip()+'\n'
    flames+=values(fire+[art['fire']['height_m'],art['fire']['width_m'],1.])+' 379\n'
    (project/'showcase.flames').write_text(flames)
    # Copy-on-write only affected scenery files; no old model bank is edited.
    origin=art['origin_world'];clear=art['clearing'];route=np.array(art['connection_world'])[:,[0,2]]
    def segment_distance(p):
        a=route[:-1];d=route[1:]-a;t=np.clip(((p-a)*d).sum(1)/(d*d).sum(1),0,1)
        return float(np.linalg.norm(p-(a+d*t[:,None]),axis=1).min())
    def in_clearing(x,z):
        dx,dz=x-origin[0],z-origin[2]
        return ((dx/8.0)**2+(dz/8.4)**2<1 or
                (-6.6<dx<-.5 and -6.5<dz<-1.5) or segment_distance(np.array([x,z]))<2.1)
    groundcover={'bep_nature_grass_broad','bep_nature_grass_long','bep_nature_clover','bep_nature_fern','bep_nature_bush_a','bep_nature_bush_b'}
    clearing=[]
    for path in sorted((project/'scenery').glob('*.txt')):
        output=[];removed=[]
        for line in path.read_text().splitlines():
            f=line.split()
            if len(f)>=5 and f[0] in groundcover:
                x=(float(f[1])-.5)*WIDTH;z=(float(f[2])-.5)*DEPTH
                if in_clearing(x,z):removed.append(line);continue
            output.append(line)
        if removed:
            rel=path.relative_to(project).as_posix();target=private_path(project,rel);target.write_text('\n'.join(output)+'\n')
            clearing.append(dict(path=rel,removed=len(removed),removed_rows=removed))
    path=project/'scenery/authored_instances.txt';data=path.read_text().rstrip()+'\n'
    def instance(alias,x,y,z):return f'{alias} {x/WIDTH+.5:.17g} {z/DEPTH+.5:.17g} 0 1 0 0 0 0 0 1 1 1 {y:.17g}\n'
    data+=instance('nm_opening_arrival_camp',*np.asarray(origin)[[0,1,2]])
    roast=art['roast'];data+=instance('nm_opening_arrival_roast',roast['center_world_m'][0],roast['instance_bottom_world_y'],roast['center_world_m'][2])
    private_path(project,'scenery/authored_instances.txt').write_text(data)
    # Measured small proxies, rather than a single impassable camp AABB.
    physics=(project/'city.physics').read_text().rstrip()+'\n';bodies=[]
    for i,b in enumerate(art['physics']):
        size=b.get('half_extents',[b.get('radius',.25),b.get('height',.5)/2,b.get('radius',.25)])
        bodies.append(dict(id=200+i,name=b['id'],center=b['center_world'],half_extents=size))
    for i,a in enumerate(art['actors']):
        p=a['world_feet'];bodies.append(dict(id=210+i,name=['Rowan','Bram','Ellis'][i],center=[p[0],p[1]+.85,p[2]],half_extents=[.32,.85,.32]))
    for b in bodies:
        physics+=f'body {b["id"]} "Arrival camp {b["name"]}" 0 0 0 0 '+values(b['center'])+' 0 0 0 1 '+values(b['half_extents'])+' .6 .01 1000 0 1 0 0 0 0 0 0 1\n'
    (project/'city.physics').write_text(physics)
    # The controller walks the existing heightfield; props use actual triangles.
    grid,fo,step=readfield(project/'ground/countryside.height')
    def feet(x,z):return float(sample(grid,np.array([[x,z]]),fo,step)[0])
    spawn=art['landing']['world_feet'].copy();spawn[1]=feet(spawn[0],spawn[2]);pose=spawn+[0.,-.04]
    connector=[p.copy() for p in art['connection_world']]
    for p in connector:p[1]=feet(p[0],p[2])
    scene=json.loads((project/'opening.scene').read_text());original_path=scene['meadow']['walk_path']
    walk=connector+original_path[1:]
    scene['meadow']['arrival']=spawn.copy();scene['meadow']['arrival'][1]+=1.75;scene['meadow']['arrival']+=[0.,-.04]
    scene['meadow']['walk_path']=walk;scene['meadow']['arrival_camp']=dict(settings='opening/arrival-camp.settings',people=['Rowan','Bram','Ellis'],center=center)
    eye=[spawn[0],spawn[1]+1.75,spawn[2]];eye0=[eye[0]-1.25,eye[1]+.55,eye[2]+.8]
    target0=[center[0],center[1]+1.2,center[2]];target1=[eye[0],eye[1]-.2,eye[2]-5.]
    scene['meadow']['cinematic']=[dict(eye=eye0,target=target0,seconds=0,fov=1.05),dict(eye=eye,target=target1,seconds=4.5,fov=1.0471975512)]
    write_json(project/'opening.scene',scene)
    cfg=[]
    for line in (project/'opening.config').read_text().splitlines():
        if line.startswith('meadow_spawn '):cfg.append('meadow_spawn '+values(pose))
        elif line.startswith('meadow_rail '):cfg.append('meadow_rail '+values(eye0+eye+target0+target1+[1.05,1.0471975512]))
        elif line.startswith('meadow_waypoint '):continue
        else:cfg.append(line)
    cfg+=['meadow_waypoint '+values(p) for p in walk]
    (project/'opening.config').write_text('\n'.join(cfg)+'\n')
    metadata=dict(passed=True,models=models,center=center,meadow_spawn=pose,actors=art['actors'],physics_bodies=bodies,groundcover_clearing=clearing,
                  source_art=str(ART/'art/ART_CANDIDATE.json'),source_art_sha256=sha(ART/'art/ART_CANDIDATE.json'),soil_review=str(ART/'ground/GROUND_REVIEW.json'),source_project_unchanged=before==mutable_snapshot(source))
    assert metadata['source_project_unchanged']
    write_json(project/'opening/arrival-campsite.json',metadata)
    old_origin=json.loads((source.parent/'opening-origin.json').read_text())
    generated={p.relative_to(project).as_posix():sha(p) for p in project.rglob('*') if p.is_file() and not p.is_symlink()}
    write_json(DEST/'opening-origin.json',dict(passed=True,project=str(project),source_project=str(source),generated=generated,cook_review=old_origin['cook_review'],source_assets_unchanged=True,scope='Private campsite arrival, first-contact dialogue and camp-to-waystone route; runtime and visual acceptance recorded separately.'))
    write_json(DOC/'STAGED.json',metadata)
    return str(project)

if __name__=='__main__':print(stage())
