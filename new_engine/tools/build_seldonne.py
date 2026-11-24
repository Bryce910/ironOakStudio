#!/usr/bin/env python3
"""Create the isolated Seldonne sample from owner-supplied assets.

Re-running preserves existing authored data unless --reset is supplied.
Generated model copies retain the original binary payload and materials.
"""
import argparse
import json
import math
import os
from pathlib import Path
import random
import struct
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'projects' / 'seldonne'
parser = argparse.ArgumentParser()
parser.add_argument('--reset', action='store_true', help='Regenerate the sample scene data')
args = parser.parse_args()
PROJECT.mkdir(parents=True, exist_ok=True)
for folder in ['models', 'imports/models', 'captures', 'saves']:
    (PROJECT / folder).mkdir(parents=True, exist_ok=True)

def shared(source, destination=None):
    destination = PROJECT / (destination or source)
    source = ROOT / source
    if source.exists() and not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.symlink_to(os.path.relpath(source, destination.parent))

for name in ['shaders','textures','fonts','background_music','sound_effects',
             'nighttime_skybox.png','daytime_skybox.png','tree_billboard.png',
             'tree_billboard_snow.png','bird_sprite.png']:
    shared(name)
shared('models/main_guy.glb')
shared('models/main_character_final/Meshy_AI__villain_Dark_Knigh_biped/cool_knight.glb','models/traveler.glb')
for suffix,animation in [('idle','Idle_11'),('attack','Double_Combo_Attack'),('attack2','Triple_Combo_Attack'),('jump','Basic_Jump')]:
    shared('models/main_character_final/Meshy_AI__villain_Dark_Knigh_biped/Meshy_AI__villain_Dark_Knigh_biped_Animation_'+animation+'_withSkin.glb','models/traveler_'+suffix+'.glb')
for name in ['female_staff','hobb_drinking','white_knight_brown_armor']:
    shared('models/npc/'+name+'.glb')

# Resolve the real material library into project-local terrain slots.
terrain_dir=PROJECT/'ground'
if terrain_dir.is_symlink(): terrain_dir.unlink()
terrain_dir.mkdir(exist_ok=True)
material_sources={
    'terrain_grass.png':'grass.jpg',
    'terrain_dirt.png':'models/Textures/T_Soil_BC.PNG',
    'terrain_sand.png':'models/Textures/T_Soil_BC.PNG',
    'terrain_rock.png':'models/Textures/T_Soil_Stone_Mix_BC.PNG',
    'terrain_grass_nor.png':'models/Textures/T_Soil_N.PNG',
    'terrain_dirt_nor.png':'models/Textures/T_Soil_N.PNG',
    'terrain_sand_nor.png':'models/Textures/T_Soil_N.PNG',
    'terrain_rock_nor.png':'models/Textures/T_Soil_Stone_Mix_N.PNG',
    'terrain_grass_rgh.png':'models/Textures/T_Soil_ARM.PNG',
    'terrain_dirt_rgh.png':'models/Textures/T_Soil_ARM.PNG',
    'terrain_sand_rgh.png':'models/Textures/T_Soil_ARM.PNG',
    'terrain_rock_rgh.png':'models/Textures/T_Soil_Stone_Mix_ARM.PNG',
    'cobblestone_path.png':'models/Textures/T_Soil_Stone_Mix_BC.PNG',
    'cobblestone_path_nor.png':'models/Textures/T_Soil_Stone_Mix_N.PNG',
    'cobblestone_path_rgh.png':'models/Textures/T_Soil_Stone_Mix_ARM.PNG',
}
for destination,source in material_sources.items(): shared(source,'ground/'+destination)


def read_glb(path):
    with path.open('rb') as f:
        magic, version, length = struct.unpack('<III', f.read(12))
        assert magic == 0x46546C67 and version == 2
        n, kind = struct.unpack('<II', f.read(8))
        assert kind == 0x4E4F534A
        data = json.loads(f.read(n))
        rest = f.read()
    return data, rest

def node_matrix(node):
    if 'matrix' in node:
        return np.array(node['matrix'], dtype=float).reshape(4,4).T
    x,y,z,w = node.get('rotation',[0,0,0,1])
    r=np.array([[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],
                [2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],
                [2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]])
    m=np.eye(4); m[:3,:3]=r@np.diag(node.get('scale',[1,1,1])); m[:3,3]=node.get('translation',[0,0,0])
    return m

def bounds(data):
    values=[]
    def visit(index,parent):
        node=data['nodes'][index]; m=parent@node_matrix(node)
        if 'mesh' in node:
            for primitive in data['meshes'][node['mesh']]['primitives']:
                acc=data['accessors'][primitive['attributes']['POSITION']]
                lo,hi=acc['min'],acc['max']
                for x in [lo[0],hi[0]]:
                    for y in [lo[1],hi[1]]:
                        for z in [lo[2],hi[2]]: values.append((m@np.array([x,y,z,1]))[:3])
        for child in node.get('children',[]): visit(child,m)
    scene=data['scenes'][data.get('scene',0)]
    for index in scene['nodes']: visit(index,np.eye(4))
    return np.min(values,axis=0), np.max(values,axis=0)

catalog={}
sources={
    'canopy_tree': ('models/shire/trees/nice_tree.glb',15),
    'tree_rock': ('models/shire/trees/tree_on_rock.glb',13),
    'cottage': ('models/shire/house/house_1.glb',5.2),
    'tall_house': ('models/shire/house/tall_house.glb',8),
    'stone_hall': ('models/shire/house/house_stone_exterior.glb',6.8),
    'portal': ('models/shire/entry_portal.glb',5),
    'campfire': ('models/bon_fire.glb',1.2),
    'boulders': ('models/boulders.glb',3.0),
    'bush': ('models/shire/bush.glb',1.6),
    'fern': ('models/shire/plants/big_plant.glb',0.9),
    'sign': ('models/wooden_sign.glb',2.3),
    'crate': ('models/wooden_crate.glb',0.85),
    'barrel': ('models/props/single_barrel.glb',1.0),
    'fence': ('models/wooden_fence.glb',1.1),
    'bridge': ('models/bridge.glb',2.0),
    'lantern_post': ('models/wooden_post.glb',3.6),
}
for name,(source,target_height) in sources.items():
    src=ROOT/source; data,rest=read_glb(src)
    # Collision proxies must not be rendered as overlapping visible meshes.
    for node in data['nodes']:
        if node.get('name','').upper().startswith(('UCX_','UBX_','USP_','UCP_')):
            node.pop('mesh',None)
    lo,hi=bounds(data)
    center=(lo+hi)*0.5
    scale=target_height/max(float(hi[1]-lo[1]),0.01)
    if name=='bridge': scale=22/max(float(hi[0]-lo[0]),0.01)
    root_nodes=list(data['scenes'][data.get('scene',0)]['nodes'])
    wrapper={'name':'Valwood import root','children':root_nodes,
             'translation':[-float(center[0])*scale,-float(lo[1])*scale,-float(center[2])*scale],
             'scale':[scale,scale,scale]}
    data['nodes'].append(wrapper)
    data['scenes'][data.get('scene',0)]['nodes']=[len(data['nodes'])-1]
    dest=PROJECT/'models'/f'{name}.glb'
    if not dest.exists():
        payload=json.dumps(data,separators=(',',':')).encode()
        payload+=b' '*((-len(payload))%4)
        with dest.open('wb') as f:
            f.write(struct.pack('<III',0x46546c67,2,20+len(payload)+len(rest)))
            f.write(struct.pack('<II',len(payload),0x4e4f534a)); f.write(payload); f.write(rest)
    catalog[name]={'source':source,'path':f'models/{name}.glb','height':float((hi[1]-lo[1])*scale),'extent':((hi-lo)*scale).tolist()}

def write(name, text):
    path=PROJECT/name
    if args.reset or not path.exists():
        path.write_text(text)

write('project.valwood', '# Valwood project version 1\nname=Valwood Studio | Seldonne\nspawn=-14 42 3.14\ncamera=0 0.16 7\nday_fraction=0.32\n')
write('config.ini', 'master_vol=60\nmusic_vol=30\nsfx_vol=55\nmouse_sens=100\npad_look_sens=100\npad_deadzone=20\nfullscreen=0\nshow_fps=1\nvsync=1\nbloom=1\ndifficulty=1\nres_w=1600\nres_h=900\n')
write('player_characters.txt', 'active=0\npreset_begin\nname=Valwood Traveler\nmodel=models/traveler.glb\nscale=1\nanim_idle=Idle_11\nanim_walk=Walking\nanim_run=Running\nanim_attack=Double_Combo_Attack\nanim_attack2=Triple_Combo_Attack\nanim_jump=Basic_Jump\npreset_end\n')
write('visual_profile.txt', 'toon_shading=0\nweather_override=0\nexposure_mul=1.12\nbloom_enabled=1\nbloom_strength=0.14\nssao_strength=0.48\nfxaa_enabled=1\nheight_fog_density=0.012\nheight_fog_base=8\nfog_density=0.0016\nwetness=0.10\novercast=0.12\ndof_aperture=0\n')
write('world_lighting.txt', 'override=1\nsun_yaw=48\nsun_pitch=28\nsun_color=2.10 1.75 1.28\nambient=0.10 0.15 0.20\n')
write('asset_catalog.json', json.dumps(catalog,indent=2))

# Landscape is an editable height field, not a mesh that bypasses terrain tools.
res=512
axis=np.linspace(-250,250,res,dtype=np.float32)
x,z=np.meshgrid(axis,axis)
channel=43+7*np.sin(z*.016)
river_distance=np.abs(x-channel)
heights=4.6+1.7*np.sin(x*.019)*np.cos(z*.024)+.42*np.sin(x*.067+z*.038)
for cx,cz,height,width in [(-165,-125,38,68),(150,-155,44,65),(-180,125,30,72),(160,130,29,65),(15,-210,40,62)]:
    heights+=height*np.exp(-((x-cx)**2+(z-cz)**2)/(width**2))
village=np.clip(1-np.sqrt((x+3)**2+(z+12)**2)/42,0,1)
village=village*village*(3-2*village)
heights=heights*(1-village)+5.4*village
bank=np.clip((river_distance-4)/12,0,1); bank=bank*bank*(3-2*bank)
heights=np.minimum(heights,(-2.2+bank*7.2)+np.clip((river_distance-16)/18,0,1)*70)
for name,values in [('terrain_sculpt.bin',(heights-3).astype('<f4'))]:
    path=PROJECT/name
    if args.reset or not path.exists(): values.tofile(path)

route=[(-14,55),(-14,42),(-10,27),(-2,10),(0,-9),(8,-22),(28,-22),(58,-22),(77,-4)]
distance=np.full_like(x,1000)
for (ax,az),(bx,bz) in zip(route,route[1:]):
    t=np.clip(((x-ax)*(bx-ax)+(z-az)*(bz-az))/((bx-ax)**2+(bz-az)**2),0,1)
    distance=np.minimum(distance,np.sqrt((x-ax-t*(bx-ax))**2+(z-az-t*(bz-az))**2))
grass=np.clip((distance-1.3)/3.4,0,1)*np.clip((river_distance-6)/7,0,1)*np.clip((heights-1)/2,0,1)
for name,values in [('grass_mask.bin',(grass*200).astype('u1')),
                    ('grass_style_mask.bin',np.full_like(x,0,dtype='u1')),
                    ('grass_tint_mask.bin',np.full_like(x,0,dtype='u1')),
                    ('flower_mask.bin',(grass*(.3+.25*np.sin(x*.17)*np.sin(z*.15))*60).astype('u1')),
                    ('flower_type_mask.bin',np.zeros_like(x,dtype='u1'))]:
    path=PROJECT/name
    if args.reset or not path.exists(): values.tofile(path)

def height(px,pz):
    u=int(np.clip((px+250)/500*(res-1),0,res-1)); v=int(np.clip((pz+250)/500*(res-1),0,res-1))
    return float(heights[v,u])

props=[]
def prop(kind,px,pz,scale=1,yaw=0,coll=0,py=None,prompt=None):
    if kind=='canopy_tree' and (PROJECT/'models/chestnut.glb').exists(): kind='chestnut'
    uid=1000+len(props)
    py=height(px,pz) if py is None else py
    # Optional final values have backwards compatible defaults in the loader.
    text=f'PROP models/{kind}.glb {px:.3f} {py:.3f} {pz:.3f} 0 {yaw:.2f} 0 {scale:.3f} {coll} 0 0 1 0 0 2.5 90 {uid}\n'
    if prompt:
        text+='ACT 8 4 -1 0 0 0 80\nREAD '+prompt+'\n'
    props.append(text+'END\n')

for px,pz,s,yaw in [(-17,-8,1,25),(-20,-31,1.15,15),(12,-42,.95,-20),(-6,-53,1.1,150),(23,4,.95,-90)]:
    prop('cottage',px,pz,s,yaw,1)
prop('tall_house',-31,-19,1,70,1)
prop('stone_hall',2,-34,1,180,1)
prop('bridge',43+7*math.sin(-22*.016),-22,1,0,2,3.5)
prop('campfire',-21,37,1,0,0)
prop('portal',-15,64,1,180,0,prompt='An ancient waystone hums beneath the moss. Beyond the trees lies Seldonne and the river crossing.')
prop('sign',-7,17,.9,15,0,prompt='Seldonne Crossing. North to the village. East across the bridge toward Kerone.')
for px,pz in [(-18,-4),(-15,-1),(12,-38),(14,-38),(20,1),(-32,-14)]:
    prop('crate' if int(px)%2 else 'barrel',px,pz,yaw=px*7)
for px,pz in [(-14,29),(5,3),(26,-19),(59,-19)]:
    prop('lantern_post',px,pz,.9,0)
for px,pz,yaw in [(-13,-1,0),(-13,-6,0),(-13,-11,0),(-23,-25,90),(-18,-25,90),(18,-2,0)]:
    prop('fence',px,pz,1,yaw)
rng=random.Random(4217)
for i in range(24):
    px=rng.uniform(-60,85); pz=rng.uniform(-65,70)
    if height(px,pz)<2 or (abs(px)<12 and pz<20) or (abs(px+14)<6 and pz>15): continue
    prop('bush' if i%3 else 'boulders',px,pz,rng.uniform(.6,1.3),rng.uniform(0,360))
for px,pz in [(-25,34),(-29,20),(10,28),(20,42),(-37,-1),(64,-40),(-40,-50),(25,-62)]:
    prop('canopy_tree',px,pz,rng.uniform(.85,1.2),rng.uniform(0,360))
for i in range(32):
    px=rng.uniform(-42,75); pz=rng.uniform(-60,62)
    if height(px,pz)<2 or abs(px+14)<5 and pz>20: continue
    prop('fern',px,pz,rng.uniform(.7,1.6),rng.uniform(0,360))
write('props_custom.txt', '# Seldonne hand authored starting area\n'+''.join(props))

trees=[]
for i in range(460):
    px=rng.uniform(-225,225); pz=rng.uniform(-220,220)
    if height(px,pz)<2 or abs(px-(43+7*math.sin(pz*.016)))<13: continue
    if -45<px<35 and -65<pz<68: continue
    if abs(pz+22)<8 and 20<px<88: continue
    trees.append(f'{px:.2f} {height(px,pz):.2f} {pz:.2f} {rng.uniform(.65,1.35):.3f} {rng.uniform(0,6.28):.3f} 0\n')
write('forest_custom.txt','# x y z scale yaw model\n'+''.join(trees))
write('paths_custom.txt','# path 0\n# width 2.6\n# style 1 Worn\n# tint 0.82 0.76 0.61\n'+'\n'.join(f'{px} {pz}' for px,pz in route)+'\n')
write('river_custom.txt','# Custom river\n# width 6.5\n# bank_softness 0.8\n# water_preset 1\n'+'\n'.join(f'{43+7*math.sin(pz*.016):.2f} {pz} 1 1 0.7 0.3' for pz in [-245,-150,-80,-22,40,120,245])+'\n')
write('water_zones.txt','# River water uses its authored ribbon\n')
write('spawns_custom.txt','# No hostile enemies in the engine sample\n')
actors=[]
for model,name,px,pz,text,waypoints in [
    (0,'Maera the Waykeeper',-7,22,'The old waystone still watches this road. Follow the lanterns to Seldonne, then cross the river to find the old trading trail.',[]),
    (1,'Tovin Reed',-11,-7,'Seldonne began where two rivers met. Rest here, traveler. The crossing is just beyond the hall.',[]),
    (2,'Warden Elric',24,-18,'Keep to the bridge. The current runs fast below these banks.',[(22,-19),(29,-22),(22,-19),(10,-20)])]:
    actors.append(f'n {model} {px} {height(px,pz):.3f} {pz} 3.14 1 1.1 3\na {name}\nd {text}\n')
    for wx,wz in waypoints: actors.append(f'w {wx} {height(wx,wz):.3f} {wz}\n')
write('npcs_custom.txt','# Seldonne residents: waykeeper, resident, patrolling warden\n'+''.join(actors))
write('surface_pieces.txt','# Empty; author through Surface Pieces\n')
write('collision_volumes.txt','# Additional collision authoring\n')
lights=[]
for i,(px,pz) in enumerate([(-14,29),(5,3),(26,-19),(59,-19)]):
    lights.append(f'{5000+i} {px} {height(px,pz)+2.8:.3f} {pz} 1 0.65 0.28 9 1.8 0 1 3 1 0 0 -35 18 32\n')
write('placed_lights.txt','# Authored warm lantern lights\n'+''.join(lights))
fx=[]
for px,pz in [(-14,29),(5,3),(26,-19),(59,-19)]:
    fx.append(f'2 {px} {height(px,pz)+2.7:.3f} {pz} .16 .28 .24 1 .6 .18 0 1 .6 .2 7 .4\n')
fx.append(f'7 -15 {height(-15,64):.3f} 64 .85 3 .65 .25 .75 1 1 .18 .55 1 10 .6\n')
fx.append(f'5 -21 {height(-21,30)+.5:.3f} 30 4 1 .25 .6 .75 .35 0 .7 .9 .4 6 .2\n')
write('world_fx_custom.txt','# Lantern flames, waystone aura, and woodland fireflies\n'+''.join(fx))
write('world_markers.txt','# Author markers from the editor\n')
write('quests_custom.txt','# Starting area engine sample\nq Welcome to Seldonne\nd Explore the village and reach the river crossing.\ng 0\nx 25\no reach 43 4 -22 8 Discover the crossing\n')
print(f'Seldonne project ready: {PROJECT}\n{len(props)} authored props, {len(trees)} forest trees, {len(catalog)} curated models')
