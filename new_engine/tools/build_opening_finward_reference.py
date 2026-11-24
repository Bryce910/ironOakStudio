#!/usr/bin/env python3
"""Build a private playable opening from the supplied Finward demo scene.

The source scene's shell, furnishings, transforms and baked illumination are
data inputs. Story props, room switches, routes and cameras are new authored
additions. Original source assets and previous opening projects stay intact.
"""
from pathlib import Path
import argparse
import copy
import json
import math
import shutil
import tempfile
import numpy as np

from check_opening_prologue import isolate_project, mutable_snapshot, sha
from build_opening_art_upgrade import private_opening_models
from build_opening_finward import private_scenery
from build_opening_prologue import Glb, read_glb, array
from opening_home_finward import _copy_materials
from opening_home_story import _paper_atlas, _paper
from opening_home_reference_shader import patch_reference_shader
from opening_home_reference_enclosure import build_enclosure, append_enclosure_physics

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'assets/purchased/finward_house_props'
HOME=np.array([-3500.,600.,-11000.])
MINIMUM=[-11.4,-.4,-9.4]
MAXIMUM=[38.9,3.1,3.4]
PAPER=[1.915,.549,-2.102]
CONSOLE=[.38,.57,-1.37]
LED=[.539,.56,-1.37]
TV=dict(minimum=[.625,.537,-2.471],maximum=[.626,1.067,-1.609])


def write_json(path,value):path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
def values(a):return ' '.join(format(float(x),'.17g') for x in a)
def world(p):return (HOME+np.asarray(p)).tolist()


def story_mesh(source,switches):
    g=Glb()
    atlas,_=_paper_atlas()
    mat=g.material('Valwood | original household bills',[.91,.90,.86],1.)
    g.doc['materials'][mat]['pbrMetallicRoughness']['baseColorTexture']={'index':g.texture(atlas)}
    edge=g.material('Valwood | thin paper edges',[.66,.63,.54],1.)
    g.box([PAPER[0],PAPER[1]-.0035,PAPER[2]],[.255,.006,.225],edge)
    _paper(g,PAPER,[.25,.22],mat,[0,0,.5,1])
    # Retain the already integrated console mesh, fitted on a free end of the
    # source TV cabinet. No source Finward model is altered for this addition.
    doc,binary=read_glb(source/'models/opening/home.glb')
    primitives=[p for mesh in doc['meshes'] for p in mesh['primitives']
                if doc['materials'][p['material']].get('name')=='xbox worn material']
    if len(primitives)!=1:raise ValueError('Expected one existing console primitive')
    selected={p['material'] for p in primitives}
    remap,_=_copy_materials(g,doc,binary,selected,512,'Valwood console')
    for old,new in remap.items():g.doc['materials'][new]['name']='Valwood | existing console'
    for p in primitives:
        a=p['attributes'];pos=array(doc,binary,a['POSITION']);normal=array(doc,binary,a['NORMAL'])
        lo,hi=pos.min(0),pos.max(0);scale=.30/max(hi[0]-lo[0],hi[2]-lo[2])
        pos=(pos-(lo+hi)*.5)*scale+np.array([CONSOLE[0],.51+(hi[1]-lo[1])*scale*.5,CONSOLE[2]])
        g.mesh(pos,normal,array(doc,binary,a['TEXCOORD_0']),array(doc,binary,p['indices']).ravel(),remap[p['material']])
    plastic=g.material('Valwood | switch plates',[.66,.64,.59],.58)
    rocker=g.material('Valwood | switch rockers',[.82,.80,.75],.52)
    for s in switches:
        p=np.array(s['position_local']);n=np.array(s['normal']);size=[.10,.14,.014] if abs(n[2])>.5 else [.014,.14,.10]
        g.box(p,size,plastic)
        g.box(p+n*.01,[v*.55 if i!=1 else .085 for i,v in enumerate(size)],rocker)
    led=g.material('Valwood | switched console power',[.02,.03,.02],.8,emissive=[.08,.25,.10])
    g.box(LED,[.004,.006,.006],led)
    material=g.material('VALWOOD opening television | animated original pixel adventure',[.006,.009,.014],.95,emissive=[.2137,.4271,.6419])
    x=TV['maximum'][0];y0,y1=TV['minimum'][1],TV['maximum'][1];z0,z1=TV['minimum'][2],TV['maximum'][2]
    g.mesh([[x,y0,z1],[x,y0,z0],[x,y1,z0],[x,y1,z1]],[[1,0,0]]*4,[[0,0],[1,0],[1,1],[0,1]],[0,1,2,0,2,3],material)
    # The native loader base-aligns each model at minY. Bake that convention
    # into the GLB and register its original height explicitly, so the TV,
    # paper, console and wall switches retain their authored world positions.
    base_y=min(float(p[0][:,1].min()) for parts in g.geometry.values() for p in parts)
    for material,parts in g.geometry.items():
        g.geometry[material]=[(p-np.array([0.,base_y,0.]),n,uv,i) for p,n,uv,i in parts]
    return g.finish(),[0.,base_y,0.]


def densify(points,step=.5):
    out=[list(points[0])]
    for a,b in zip(points,points[1:]):
        n=max(1,math.ceil(math.hypot(b[0]-a[0],b[2]-a[2])/step))
        out.extend((np.array(a)+(np.array(b)-a)*i/n).tolist() for i in range(1,n+1))
    return out


def build(destination,cooked,nav):
    source=ROOT/'build/opening-finward/v6/project'
    destination=destination.resolve()
    if destination.exists():raise ValueError('Choose a new output root')
    cook=json.loads((cooked/'COOK_REVIEW.json').read_bytes());assert cook['passed']
    catalog=json.loads((cooked/'catalog.json').read_bytes())
    navigation=json.loads((nav.parent/'nav-v2/NAVIGATION.json').read_bytes());assert navigation['passed']
    switch_data=json.loads((nav/'SWITCHES.json').read_bytes());assert switch_data['passed']
    switches=switch_data['switches'];assert len(switches)==4
    before=mutable_snapshot(source)
    destination.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='.reference-',dir=destination.parent))
    try:
        project=stage/'project';isolate_project(source,project);private_opening_models(project,source);private_scenery(project,source)
        bank=project/'models/opening'
        for p in bank.glob('home*.glb'):p.unlink()
        rows=(project/'scenery/authored_instances.txt').read_text().splitlines()
        anchor=next(row.split() for row in rows if row.startswith('nm_opening_home '))
        old_aliases={'nm_opening_home','nm_opening_finward_living','nm_opening_finward_bedroom','nm_opening_finward_kitchen','nm_opening_finward_bathroom'}
        studio=[row for row in (project/'world.studio').read_text().splitlines()
                if not (row.startswith('asset ') and row.split()[1] in old_aliases)
                and not (row.startswith('light ') and float(row.split()[2])>590)]
        rows=[r for r in rows if not r.split() or r.split()[0] not in old_aliases]
        meshes=[]
        def register(stem,offset,first=False):
            alias='nm_opening_home' if first else 'nm_opening_reference_'+stem
            studio.append(f'asset {alias} opening/{stem} 2 0 0 240 75 1')
            row=anchor.copy();row[0]=alias;row[1]=format(float(anchor[1])+offset[0]/100800.,'.17g');row[2]=format(float(anchor[2])+offset[2]/56700.,'.17g');row[-1]=format(HOME[1]+offset[1],'.17g');rows.append(' '.join(row))
            meshes.append(dict(path='models/opening/'+stem+'.glb',offset=list(offset)))
        for index,item in enumerate(catalog):
            stem='home' if index==0 else item['name']
            for model in item['models']:
                p=cooked/'chunks'/model['file'];assert sha(p)==model['sha256']
                shutil.copyfile(p,bank/(stem+('_mid' if model['lod'] else '')+'.glb'))
            register(stem,item['origin_native'],index==0)
        raw,story_offset=story_mesh(source,switches)
        for suffix in ['', '_mid']:(bank/('home_story'+suffix+'.glb')).write_bytes(raw)
        register('home_story',story_offset)
        enclosure_raw,enclosure=build_enclosure(project,cooked)
        for suffix in ['', '_mid']:(bank/('home_enclosure'+suffix+'.glb')).write_bytes(enclosure_raw)
        register('home_enclosure',enclosure['offset_native'])
        write_json(project/'opening/enclosure.json',enclosure)
        # Actual-triangle audits and native rendering must share coordinates.
        # A nonzero raw minY would silently shift the model in the renderer.
        for item in meshes:
            for suffix in ['', '_mid']:
                path=project/(item['path'][:-4]+suffix+'.glb')
                doc,binary=read_glb(path)
                low=min(float(array(doc,binary,p['attributes']['POSITION'])[:,1].min())
                        for mesh in doc['meshes'] for p in mesh['primitives'])
                if abs(low)>1e-6:raise ValueError('Model violates native minY convention: '+str(path))
        assert len(meshes)<=16
        exported=json.loads((DATA/'scene_export/scene.json').read_bytes())
        practicals=[]
        for light in exported['lights']:
            xyz=np.array(light['matrix'][12:15]);xyz[2]*=-1
            if light['bake_type']=='Realtime':
                forward=np.array(light['matrix'][8:11]);forward[2]*=-1;forward/=np.linalg.norm(forward)
                yaw=math.degrees(math.atan2(forward[0],-forward[2]));pitch=math.degrees(math.asin(np.clip(forward[1],-1,1)))
                studio.append('light '+values([*world(xyz),light['range'],light['intensity'],*[light['color'][k] for k in 'rgb'],yaw,pitch,light['spot_angle'],0]))
            elif light['type']=='Point' or (light['type']=='Spot' and xyz[0]<-6):
                room=1 if xyz[0]<-6 else 3 if 27<xyz[0]<31 else 0
                practicals.append((room,world(xyz)))
                studio.append('light '+values([*world(xyz),2.,light['intensity']*.10,*[light['color'][k] for k in 'rgb'],0,-45,180,0]))
        # Source bedroom lamps illuminate through their authored emission/bake.
        bedroom=world([15.5,1.7,-.65]);practicals.append((2,bedroom));studio.append('light '+values([*bedroom,2.,.05,1,.78,.5,0,-45,180,0]))
        (project/'world.studio').write_text('\n'.join(studio)+'\n');(project/'scenery/authored_instances.txt').write_text('\n'.join(rows)+'\n')
        physics=(nav.parent/'nav-v2/runtime/scene.physics').read_bytes()
        (project/'opening/home.physics').write_bytes(append_enclosure_physics(physics,enclosure,HOME.tolist()))
        spawn=world([2,0,-5]);yaw=math.pi
        (project/'opening/home.settings').write_text('VALWOOD_PLAYTEST_WALK 1\nphysics "home.physics"\nbounds '+values([HOME[0]-11,HOME[0]+38.5,HOME[2]-9,HOME[2]+3,599.8,603.1])+'\nspawn '+values([*spawn,yaw,-.03])+'\n')
        controls=['VALWOOD_OPENING_ROOM_CONTROLS 1']
        for s in switches:controls.append('switch '+str(s['room'])+' '+values([*world(s['position_local']),*s['normal'],*world(s['room_min_local']),*world(s['room_max_local'])]))
        for room,position in practicals:controls.append('practical '+str(room)+' '+values(position))
        for i,item in enumerate(switch_data['routes']):
            path=item['points_local']
            controls.extend('path '+str(i)+' '+values(world(p)) for p in path)
        controls.append('end');(project/'opening/room-controls.settings').write_text('\n'.join(controls)+'\n')
        interior=['VALWOOD_OPENING_INTERIOR 1','bounds '+values([*world(MINIMUM),*world(MAXIMUM)])]
        # Use source room capture positions, clamping their influence boxes to
        # real floor/ceiling height rather than the publisher's oversized boxes.
        chosen=[1,2,3,4,5,6,7,8]
        for i in chosen:
            probe=exported['reflection_probes'][i];point=np.array(probe['matrix'][12:15]);point[2]*=-1
            size=np.array([probe['size'][k] for k in 'xyz']);a=point-size*.5;b=point+size*.5
            a=np.maximum(a,[-11.2,.02,-9.2]);b=np.minimum(b,[38.7,2.58,3.2]);point[1]=min(1.5,b[1]-.1)
            interior.append('probe '+values([*world(point),*world(a),*world(b)]))
        interior+=['release_city_fallbacks 1','end'];(project/'opening/interior.settings').write_text('\n'.join(interior)+'\n')
        source_scene=json.loads((source/'opening.scene').read_bytes());home=source_scene['home']
        route1=next(r['points_local'] for r in navigation['routes'] if r['from']=='spawn' and r['to']=='bills')
        route2=next(r['points_local'] for r in navigation['routes'] if r['from']=='bills' and r['to']=='console')
        # Reach the console at the free cabinet end opposite the source vase.
        # Retain the proved route and append a short approach, verified again
        # by the production capsule checker on the completed private project.
        route2=route2+[[1.1,route2[-1][1],-2.8]]
        # Existing ordinary-input story QA interacts at node1. The source
        # capsule proof establishes a direct, clear spawn-to-bills segment.
        assert len(route1)==2
        route=route1+densify(route2,.2)[1:];assert len(route)>=5
        home['walk_path']=[world(p) for p in route]
        home['spawn']=[spawn[0],spawn[1]+1.75,spawn[2],yaw,-.03]
        home['inspect'].update(position=world(PAPER),approach=world(route1[-1]))
        home['console'].update(position=world(CONSOLE),approach=world(route2[-1]))
        home['blocking_checks']=[]
        for s in [switches[0],next(x for x in switches if x['room']==2)]:
            a=list(s['approach_local']);b=np.array(s['position_local'])-np.array(s['normal'])*.7;b[1]=a[1]
            home['blocking_checks'].append([world(a),world(b)])
        (project/'opening.scene').write_text(json.dumps(source_scene,indent=2)+'\n')
        lines=(source/'opening.config').read_text().splitlines();lines=[l for l in lines if not l.startswith('house_waypoint ')]
        replace={'house_spawn':[*spawn,yaw,-.03],'house_focus':world(PAPER),'house_exit':world(CONSOLE),
                 'house_rail':[*world([2,1.55,-5]),*world([2,1.75,-5]),*world([.6,1.1,-1.3]),*world([2,1.75+math.tan(-.03),-4]),1.02,1.0471975512],
                 'transport_rail':[*world([1.5,.82,-2.04]),*world([.83,.82,-2.04]),*world([.626,.82,-2.04]),*world([.626,.82,-2.04]),.9,.7]}
        lines=[l.split()[0]+' '+values(replace[l.split()[0]]) if l.split() and l.split()[0] in replace else l for l in lines]
        lines+=['house_waypoint '+values(world(p)) for p in route];(project/'opening.config').write_text('\n'.join(lines)+'\n')
        layout=dict(house_origin=HOME.tolist(),floor_world_y=600.,collision_boxes=[],collision_bodies=13+enclosure['collision_bodies_added'],render_meshes=meshes,
                    source_scene=str(DATA/'scene_export/scene.json'),source_scene_sha256=sha(DATA/'scene_export/scene.json'),
                    copied_demo_layout=True,source_assemblies=701,story_additions=['household bills','existing console and animated screen','four working room switches','closed perimeter around the supplied display rooms'])
        write_json(project/'opening/layout.json',layout)
        shots=[]
        def shot(name,duration,a,b,c,d,f0,f1,subtitle):shots.append(dict(name=name,duration=duration,eye_from=world(a),eye_to=world(b),target_from=world(c),target_to=world(d),fov_from=f0,fov_to=f1,subtitle=subtitle))
        shot('publisher-room-arrival',3.2,[2,1.55,-5],[1.9,1.50,-4.8],[.6,1.1,-1.3],[.7,1.1,-1.3],1.02,1.,'Another late shift.')
        shot('household-bills',4.1,[1.915,1.2,-3],[1.915,1.13,-2.95],PAPER,PAPER,.69,.64,"Rent. Power. Food. Somehow I'm still behind.")
        shot('quiet-television',4.2,[2.2,1.35,-3.8],[2.1,1.3,-3.65],[.626,.82,-2.04],[.626,.82,-2.04],.86,.82,'Just one quiet night.')
        shot('playable-handoff',4.5,[1.9,1.60,-4.8],[2,1.75,-5],[.2,1.35,-2],[2,1.75+math.tan(-.03),-4],.96,1.0471975512,'')
        cinematic=['VALWOOD_HOME_CINEMATIC 1']
        for s in shots:cinematic.append('shot '+values([s['duration'],*s['eye_from'],*s['eye_to'],*s['target_from'],*s['target_to'],s['fov_from'],s['fov_to']])+' '+json.dumps(s['subtitle']))
        cinematic.append('end');(project/'opening/cinematic.settings').write_text('\n'.join(cinematic)+'\n')
        write_json(project/'opening/cinematic.json',dict(schema='valwood.home-cinematic',version=1,seconds=16.,shots=shots,endpoint_exact_spawn=True,scope='Source-room compositions; actual-triangle and native review remain required.'))
        windows=[]
        for r in exported['placements']:
            if Path(r['source_prefab']).stem.startswith(('Window_','Clock_')):
                lo=[r['bounds_min']['x'],r['bounds_min']['y'],-r['bounds_max']['z']];hi=[r['bounds_max']['x'],r['bounds_max']['y'],-r['bounds_min']['z']];windows.append((lo,hi))
        shader=patch_reference_shader(project,HOME.tolist(),MINIMUM,MAXIMUM,windows,TV,LED)
        views=[]
        for name,eye,target in [('main-room',[2,1.45,-5],[.6,1.15,-1.3]),('kitchen',[-7.2,1.6,-7.5],[-9.1,1.1,-4.5]),('bedroom',[15.4,1.6,-4.6],[15.5,.8,-1.2]),('bathroom',[29.4,1.6,-.2],[28.1,.65,-1.7])]:
            direction=np.array(target)-eye;views.append(dict(name=name,eye=world(eye),target=world(target),yaw=math.atan2(direction[0],-direction[2]),pitch=math.atan2(direction[1],math.hypot(direction[0],direction[2])),fov=1.0471975512))
        write_json(project/'opening/inspection.views.json',dict(views=views))
        # Keep historical metadata explicitly historical rather than naming
        # old furniture as the currently rendered house.
        write_json(project/'opening/reference.json',dict(schema='valwood.finward-reference-opening',version=1,source_layout_copied=True,cook_root=str(cooked),cook_review_sha256=sha(cooked/'COOK_REVIEW.json'),navigation=str(nav),shader=shader,practicals=len(practicals),render_meshes=meshes,scope='CPU staging; native appearance/gameplay acceptance pending.'))
        for name in ['finward','art-upgrade']:
            path=project/'opening'/(name+'.json')
            if path.exists():
                data=json.loads(path.read_bytes());data['historical_only']=True
                data['superseded_by']='opening/reference.json';write_json(path,data)
        generated={p.relative_to(project).as_posix():sha(p) for p in project.rglob('*') if p.is_file() and not p.is_symlink()}
        after=mutable_snapshot(source)
        changes=[k for k,v in before.items() if after.get(k)!=v and not k.startswith('saves/') and k!='graphics.settings']
        assert not changes,changes
        write_json(stage/'opening-origin.json',dict(passed=True,project=str(destination/'project'),source_project=str(source),generated=generated,source_assets_unchanged=True,reference_scene_copied=True,cook_review=sha(cooked/'COOK_REVIEW.json'),scope='Private supplied-demo opening rebuild; runtime/visual/route acceptance is separate.'))
        stage.rename(destination)
        return dict(passed=True,project=str(destination/'project'),groups=len(meshes),practicals=len(practicals),scope='Staged; native validation pending.')
    finally:
        if stage.exists():shutil.rmtree(stage)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root',type=Path,default=ROOT/'build/opening-finward-reference/v11')
    parser.add_argument('--cooked',type=Path,default=DATA/'scene-cooked-v3')
    parser.add_argument('--navigation',type=Path,default=ROOT/'docs/opening-finward-reference/2026-11-03/nav-v3')
    args=parser.parse_args();print(json.dumps(build(args.output_root,args.cooked,args.navigation),indent=2))
