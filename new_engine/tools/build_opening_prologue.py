#!/usr/bin/env python3
"""Build an isolated modern-home -> Meadows of Valois opening workspace.

Original environments, source GLBs and saves are read-only inputs. The small
house is a new authored composition of existing modern apartment furniture and
already downloaded CC0 plaster/wood textures. It is not a purchased house pack.
An optional reviewed Finward hook replaces selected furnishings without
changing the default composition. No game is launched by this tool.
"""
from pathlib import Path
import argparse
import collections
import copy
import hashlib
import io
import json
import math
import os
import shutil
import struct
import tempfile

import numpy as np
from PIL import Image

from prepare_playtest_slices import inventory, digest, checked_copy, canonical
from opening_home_materials import build_materials
from opening_home_set import primary_furnishings, dress_home, HOME_WINDOW_Z
from opening_home_gaming import dress_gaming_corner
from opening_home_bathroom import dress_bathroom
from opening_home_shadows import patch_house_shadow
from opening_home_tv import patch_home_tv
from opening_home_light_shader import patch_home_lights
from opening_home_story import dress_opening_story, RENT_NOTICE_CENTER

ROOT = Path(__file__).resolve().parents[1]
HOME = np.array([-3500., 600., -11000.])
FLOOR = .18
ALIAS = 'nm_opening_home'


class Glb:
    def __init__(self):
        self.doc = dict(asset=dict(version='2.0', generator='Valwood authored opening composition'),
                        buffers=[], bufferViews=[], accessors=[], images=[], textures=[],
                        samplers=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)],
                        materials=[], meshes=[], nodes=[], scenes=[dict(nodes=[0])], scene=0)
        self.binary = bytearray()
        self.geometry = collections.defaultdict(list)
        self.images = {}

    def view(self, raw, target=None):
        self.binary.extend(b'\0' * (-len(self.binary) % 4))
        v = dict(buffer=0, byteOffset=len(self.binary), byteLength=len(raw))
        if target: v['target'] = target
        self.doc['bufferViews'].append(v); self.binary.extend(raw)
        return len(self.doc['bufferViews']) - 1

    def accessor(self, values, kind):
        a = np.asarray(values, dtype='<u4' if kind == 'SCALAR' else '<f4')
        item = dict(bufferView=self.view(a.tobytes(),34963 if kind=='SCALAR' else 34962),
                    componentType=5125 if kind=='SCALAR' else 5126, count=len(a), type=kind)
        if kind == 'VEC3': item.update(min=a.min(0).tolist(), max=a.max(0).tolist())
        self.doc['accessors'].append(item)
        return len(self.doc['accessors']) - 1

    def texture(self, raw, size=1024):
        key = (hashlib.sha256(raw).hexdigest(),size)
        if key in self.images: return self.images[key]
        im = Image.open(io.BytesIO(raw)); im.load()
        im.thumbnail((size,size),Image.Resampling.LANCZOS)
        im = im.convert('RGBA' if 'A' in im.getbands() else 'RGB')
        out=io.BytesIO(); im.save(out,format='PNG',optimize=True)
        self.doc['images'].append(dict(bufferView=self.view(out.getvalue()),mimeType='image/png'))
        self.doc['textures'].append(dict(sampler=0,source=len(self.doc['images'])-1))
        index=len(self.doc['textures'])-1;self.images[key]=index
        return index

    def material(self, name, rgb, roughness=.9, base=None, normal=None, arm=None, emissive=None):
        p=dict(baseColorFactor=[*rgb,1],metallicFactor=0,roughnessFactor=roughness)
        if base:p['baseColorTexture']=dict(index=self.texture(Path(base).read_bytes()))
        if arm:p['metallicRoughnessTexture']=dict(index=self.texture(Path(arm).read_bytes()))
        m=dict(name=name,pbrMetallicRoughness=p,doubleSided=False)
        if normal:m['normalTexture']=dict(index=self.texture(Path(normal).read_bytes()),scale=.55)
        if emissive:m['emissiveFactor']=emissive
        self.doc['materials'].append(m)
        return len(self.doc['materials'])-1

    def mesh(self, positions, normals, uv, indices, material):
        self.geometry[material].append((np.asarray(positions),np.asarray(normals),np.asarray(uv),np.asarray(indices)))

    def box(self, center, size, material, tile=1.):
        c=np.asarray(center);s=np.asarray(size)/2
        # Each face owns its normal and UV seam. Winding is outward CCW.
        vertices=[];normals=[];uv=[];indices=[]
        for axis in range(3):
            a=(axis+1)%3;b=(axis+2)%3
            for sign in [-1,1]:
                offsets=[(-1,-1),(1,-1),(1,1),(-1,1)]
                if sign<0:offsets=offsets[::-1]
                start=len(vertices)
                for aa,bb in offsets:
                    p=c.copy();p[axis]+=sign*s[axis];p[a]+=aa*s[a];p[b]+=bb*s[b]
                    n=np.zeros(3);n[axis]=sign
                    vertices.append(p);normals.append(n);uv.append(((aa+1)*s[a]/tile,(bb+1)*s[b]/tile))
                indices.extend([start,start+1,start+2,start,start+2,start+3])
        self.mesh(vertices,normals,uv,indices,material)

    def finish(self):
        primitives=[]
        for material,parts in self.geometry.items():
            offsets=np.cumsum([0]+[len(p[0]) for p in parts[:-1]])
            pos=np.concatenate([p[0] for p in parts]);normal=np.concatenate([p[1] for p in parts]);uv=np.concatenate([p[2] for p in parts])
            indices=np.concatenate([p[3]+offset for p,offset in zip(parts,offsets)])
            primitives.append(dict(attributes=dict(POSITION=self.accessor(pos,'VEC3'),NORMAL=self.accessor(normal,'VEC3'),TEXCOORD_0=self.accessor(uv,'VEC2')),indices=self.accessor(indices,'SCALAR'),material=material,mode=4))
        self.doc['meshes']=[dict(name='Worn modern home and furnishings',primitives=primitives)]
        self.doc['nodes']=[dict(mesh=0,name='Opening home; floor slab bottom at zero')]
        self.doc['buffers']=[dict(byteLength=len(self.binary))]
        payload=json.dumps(self.doc,separators=(',',':')).encode();payload+=b' '*(-len(payload)%4)
        raw=bytes(self.binary);raw+=b'\0'*(-len(raw)%4)
        return struct.pack('<III',0x46546c67,2,28+len(payload)+len(raw))+struct.pack('<II',len(payload),0x4e4f534a)+payload+struct.pack('<II',len(raw),0x004e4942)+raw


def read_glb(path):
    raw=path.read_bytes();magic,version,total=struct.unpack_from('<III',raw)
    if magic!=0x46546c67 or version!=2 or total!=len(raw):raise ValueError('Invalid GLB '+str(path))
    n,t=struct.unpack_from('<II',raw,12)
    if t!=0x4e4f534a:raise ValueError('Missing JSON')
    d=json.loads(raw[20:20+n]);b,t=struct.unpack_from('<II',raw,20+n)
    if t!=0x004e4942:raise ValueError('Missing embedded binary')
    return d,raw[28+n:28+n+b]


def array(doc,binary,index):
    item=doc['accessors'][index];view=doc['bufferViews'][item['bufferView']]
    width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[item['type']]
    dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[item['componentType']]
    count=item['count'];size=np.dtype(dtype).itemsize
    return np.ndarray((count,width),dtype,binary,view.get('byteOffset',0)+item.get('byteOffset',0),
                      strides=(view.get('byteStride',width*size),size)).copy()


def furniture(glb,path,x,z,width=None,height=None,y=FLOOR,yaw=0,roughness=.72,y_scale=1.):
    d,b=read_glb(path)
    if any(any(k in n for k in ('matrix','translation','rotation','scale','children'))for n in d['nodes']):
        raise ValueError('Apartment source requires explicit transform support: '+str(path))
    prims=[p for n in d['nodes']if'mesh'in n for p in d['meshes'][n['mesh']]['primitives']]
    points=np.concatenate([array(d,b,p['attributes']['POSITION'])for p in prims]);lo=points.min(0);hi=points.max(0)
    scale=(width/(hi[0]-lo[0]))if width is not None else height/(hi[1]-lo[1])
    axis_scale=np.array([scale,scale*y_scale,scale]);assert np.all(axis_scale>0)
    angle=math.radians(yaw);c,s=math.cos(angle),math.sin(angle);rot=np.array([[c,0,s],[0,1,0],[-s,0,c]])
    shift=np.array([-(lo[0]+hi[0])/2,-lo[1],-(lo[2]+hi[2])/2])
    remapped={}
    def image_bytes(index):
        texture=d['textures'][index];image=d['images'][texture['source']];v=d['bufferViews'][image['bufferView']]
        return b[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
    def tex(index,arm=False,occlusion=None):
        raw=image_bytes(index)
        if arm:
            # glTF metallic-roughness uses G/B; R is not ambient occlusion.
            # Native materials use packed AO/Roughness/Metallic. Several owned
            # apartment sources store zero in the unused R channel, which
            # otherwise turns the whole couch/rug into a black silhouette.
            im=Image.open(io.BytesIO(raw));im.thumbnail((512,512),Image.Resampling.LANCZOS)
            im=im.convert('RGB')
            ao=Image.new('L',im.size,255)
            if occlusion is not None:
                source_ao=Image.open(io.BytesIO(image_bytes(occlusion))).convert('RGB').getchannel('R')
                ao=source_ao.resize(im.size,Image.Resampling.LANCZOS)
            im=Image.merge('RGB',(ao,im.getchannel('G'),im.getchannel('B')))
            out=io.BytesIO();im.save(out,format='PNG');raw=out.getvalue()
        return glb.texture(raw,size=512)
    repaired_maps=0
    for i,source in enumerate(d.get('materials',[])):
        m=copy.deepcopy(source);m['name']=path.stem+' worn material'
        p=m.setdefault('pbrMetallicRoughness',{});p['roughnessFactor']=max(roughness,p.get('roughnessFactor',1.))
        # These furnishings are painted wood, cloth and plastic. Unspecified
        # glTF metallicFactor defaults to1, which made several old imports shine.
        p['metallicFactor']=0
        for parent,key in [(p,'baseColorTexture'),(p,'metallicRoughnessTexture'),(m,'normalTexture'),(m,'occlusionTexture'),(m,'emissiveTexture')]:
            if key in parent:
                arm=key=='metallicRoughnessTexture'
                parent[key]['index']=tex(parent[key]['index'],arm=arm,occlusion=m.get('occlusionTexture',{}).get('index')if arm else None)
                repaired_maps+=int(arm)
        glb.doc['materials'].append(m);remapped[i]=len(glb.doc['materials'])-1
    for p in prims:
        pos=(array(d,b,p['attributes']['POSITION'])+shift)*axis_scale@rot.T+np.array([x,y,z])
        normal=(array(d,b,p['attributes']['NORMAL'])/axis_scale)@rot.T
        normal/=np.maximum(np.linalg.norm(normal,axis=1,keepdims=True),1e-12)
        uv=array(d,b,p['attributes']['TEXCOORD_0'])
        idx=array(d,b,p['indices']).reshape(-1)
        glb.mesh(pos,normal,uv,idx,remapped[p.get('material',0)])
    corners=np.array([[xx,yy,zz]for xx in [lo[0],hi[0]]for yy in [lo[1],hi[1]]for zz in [lo[2],hi[2]]])
    bounds=(corners+shift)*axis_scale@rot.T+np.array([x,y,z])
    return dict(source=str(path),sha256=digest(path),position=[x,y,z],yaw=yaw,uniform_scale=float(scale),axis_scale=axis_scale.tolist(),native_arm_repairs=repaired_maps,
                minimum=bounds.min(0).tolist(),maximum=bounds.max(0).tolist())


def physics(boxes):
    rows=['VALWOOD_PHYSICS 1','gravity 0 -9.81 0']+[f'layer {i} 255'for i in range(8)]
    for i,(name,center,size)in enumerate(boxes,1):
        center=np.asarray(center)+HOME;half=np.asarray(size)/2
        values=[*center,0,0,0,1,*half,.6,.01,1000,0,1,0,0,0,0,0,0,1]
        rows.append(f'body {i} "{name}" 0 0 0 0 '+' '.join(format(float(v),'.12g')for v in values))
    return ('\n'.join(rows)+'\n').encode()


def ground_height(project,x,z):
    for name in ['showcase.height','countryside.height']:
        raw=(project/'ground'/name).read_bytes();magic,w,h,ox,oz,dx,dz=struct.unpack_from('<8sIIdddd',raw)
        if magic!=b'VWHGT1\0\0':raise ValueError('Invalid height service')
        u=(x-ox)/dx;v=(z-oz)/dz
        if 0<=u<=w-1 and 0<=v<=h-1:
            f=np.frombuffer(raw,dtype='<f4',offset=48).reshape(h,w);i=min(int(u),w-2);j=min(int(v),h-2);a=u-i;b=v-j
            return float(f[j,i]*(1-a)*(1-b)+f[j,i+1]*a*(1-b)+f[j+1,i]*(1-a)*b+f[j+1,i+1]*a*b)
    raise ValueError('Opening point outside installed source terrain')


def author_house(output,game_start_story=False,art_upgrade=False,finward_tier=None):
    g=Glb();maps=build_materials(ROOT/'build/opening-apartment-refresh/materials')
    def domestic(key,name):
        m=maps[key]
        return g.material(name,m['rgb'],m['roughness'],base=m['base'],normal=m['normal'],arm=m['arm'])
    plaster=domestic('wall','Soft aged off-white painted plaster')
    stained=domestic('lower_wall','Subtle utility corner paint')
    floor=domestic('floor','Dry muted oak narrow-plank floor')
    trim=domestic('trim','Satin off-white painted casing')
    ceiling=domestic('ceiling','Neutral matte plaster ceiling')
    kitchen_tile=domestic('tile','Matte small-joint kitchen tile')
    inputs=[m[k]for key,m in maps.items()if key!='wear'for k in ('base','normal','arm')]
    steel=g.material('Dull old hardware',[.13,.14,.15],.8)
    amber=g.material('Warm practical lamps',[.6,.36,.12],.6,emissive=[1.1,.59,.19])
    night=g.material('Soft daylight behind recessed window',[.53,.62,.68],.94,emissive=[.24,.32,.39])
    paper=g.material('Notebook paper',[.68,.63,.49],1)
    cover=g.material('Worn notebook cover',[.12,.18,.16],.95)
    boxes=[]
    def box(name,center,size,mat,collide=True,tile=1.):
        g.box(center,size,mat,tile)
        if collide:boxes.append((name,center,size))
    box('Home floor',[0,.09,0],[11,.18,10],floor,tile=maps['floor']['tile'])
    # A shallow flush visual tile inset defines the modest working kitchen.
    g.box([-.20,FLOOR+.0015,-3.22],[2.5,.003,3.10],kitchen_tile,tile=maps['tile']['tile'])
    box('Home ceiling',[0,3.26,0],[11.2,.16,10.2],ceiling,tile=maps['ceiling']['tile'])
    # Outer walls include an actual framed front door and one window opening.
    walls=[('West wall',[-5.5,1.68,0],[.2,3,10]),('Rear wall',[0,1.68,-5],[11,3,.2]),
           ('Front wall left',[-1.225,1.68,5],[8.55,3,.2]),('Front wall right',[4.875,1.68,5],[1.25,3,.2]),
           ('Front door lintel',[3.65,2.88,5],[1.2,.6,.2]),
           ('East wall rear',[5.5,1.68,(HOME_WINDOW_Z-.85-5)/2],[.2,3,HOME_WINDOW_Z-.85+5]),
           ('East wall front',[5.5,1.68,(HOME_WINDOW_Z+.85+5)/2],[.2,3,5-HOME_WINDOW_Z-.85]),
           ('East wall under window',[5.5,.61,HOME_WINDOW_Z],[.2,.86,1.7]),('East wall above window',[5.5,2.95,HOME_WINDOW_Z],[.2,.46,1.7]),
           ('Bedroom partition rear',[1.25,1.68,-3.25],[.16,3,3.5]),
           ('Bedroom partition left',[1.85,1.68,-1.5],[1.2,3,.16]),('Bedroom partition right',[4.6,1.68,-1.5],[1.8,3,.16]),
           ('Bedroom door lintel',[3.075,2.88,-1.5],[1.25,.6,.16])]
    for name,c,s in walls:box(name,c,s,plaster,tile=maps['wall']['tile'])
    for x in [3.105,4.195]:g.box([x,1.36,4.87],[.11,2.36,.08],trim)
    # Restrained crown and skirting give real edges without a thick stripe.
    for name,c,s in [('West crown',[-5.37,3.115,0],[.12,.13,9.95]),
                     ('East crown',[5.37,3.115,0],[.12,.13,9.95]),
                     ('Rear crown',[0,3.115,-4.87],[10.8,.13,.12]),
                     ('Front crown',[0,3.115,4.87],[10.8,.13,.12])]:box(name,c,s,trim)
    for x in [-5.37,5.37]:g.box([x,.25,0],[.08,.14,9.85],trim)
    for z in [-4.87,4.87]:
        if z<0:g.box([0,.25,z],[10.7,.14,.08],trim)
        else:g.box([-1.45,.25,z],[7.8,.14,.08],trim)
    # Small localized utility wear; broad domestic walls remain quietly painted.
    g.box([5.389,.39,HOME_WINDOW_Z],[.009,.25,.9],stained,tile=2.4)
    source=ROOT/'models/apartment';items=[]
    specs=primary_furnishings()
    for name,x,z,kw in specs:
        item=furniture(g,source/(name+'.glb'),x,z,**kw);items.append(item)
        if name not in {'window','rug','coffee_cup','lamp','trash'}:
            lo=np.asarray(item['minimum']);hi=np.asarray(item['maximum'])
            # Furniture box proxies retain a small margin inside actual bounds;
            # static source details do not become ankle-height triangle snags.
            size=hi-lo;size[[0,2]]=np.maximum(.04,size[[0,2]]-.08)
            boxes.append((name+' furniture',(lo+hi)/2,size))
    # Framed dirty blue window and warm practical detail retain shape in low light.
    # Cover the complete 1.04–2.72 m shell opening behind the recessed frame.
    # A small overlap prevents the unrelated exterior sky leaking above it.
    g.box([5.62,1.88,HOME_WINDOW_Z],[.03,1.72,1.74],night)
    # Neutral personal item; its contents are intentionally not a story decision.
    g.box([-2.1,.835,1.35],[.29,.025,.21],paper)
    g.box([-2.1,.851,1.35],[.31,.009,.23],cover)
    g.box([-2.1,.819,1.35],[.31,.009,.23],cover)
    g.box([3.22,1.21,4.87],[.16,.035,.035],steel)
    dressing=dress_home(g,furniture,source,floor=FLOOR,boxes=boxes)
    dressing['gaming_corner']=dress_gaming_corner(g,furniture,source)
    dressing['bathroom']=dress_bathroom(g,furniture,source,boxes=boxes,floor=FLOOR)
    if game_start_story:dressing['story']=dress_opening_story(g)
    if art_upgrade:
        from opening_home_art_upgrade import upgrade_home_art
        dressing['art_upgrade']=upgrade_home_art(g)
    if finward_tier is not None:
        from opening_home_finward_set import upgrade_finward_set
        dressing['finward']=upgrade_finward_set(g,dressing,boxes,tier=finward_tier)
        items=[item for item in items if Path(item['source']).stem not in {'couch','nice_bed'}]
        items.extend(dressing['finward']['placements'])
    raw=g.finish();(output/'models/opening').mkdir(parents=True)
    (output/'models/opening/home.glb').write_bytes(raw)
    # Same geometry is a pinned mid tier and independent shadow proxy; at this
    # bounded room scale no lossy simplification is needed.
    (output/'models/opening/home_mid.glb').write_bytes(raw)
    (output/'opening').mkdir()
    (output/'opening/home.physics').write_bytes(physics(boxes))
    settings='VALWOOD_PLAYTEST_WALK 1\nphysics "home.physics"\nbounds -3505.5 -3494.5 -11005 -10995 599.9 603.45\nspawn -3500.4 600.18 -10996.8 -.65 -.08\n'
    (output/'opening/home.settings').write_text(settings)
    (output/'opening/room-lights.settings').write_text('VALWOOD_ROOM_LIGHTS 1\n')
    if game_start_story:(output/'opening/game-start.settings').write_text('VALWOOD_GAME_START 1\n')
    return dict(glb_bytes=len(raw),primitives=len(g.doc['meshes'][0]['primitives']),images=len(g.doc['images']),
                vertices=sum(a['count']for a in g.doc['accessors']if a['type']=='VEC3')//2,
                collision_bodies=len(boxes),floor_world_y=600.18,house_origin=HOME.tolist(),window_center_z=HOME_WINDOW_Z,
                material_policy='Legacy furniture glTF G/B metallic-roughness maps are repacked to native ARM. Optional Finward furniture uses the reviewed active Unity URP material conversion documented in dressing.finward; native ARM is R=AO,G=roughness,B=metallic. Original source files remain unchanged.',
                source_materials={str(p):digest(p)for p in inputs},furnishings=items,dressing=dressing,
                collision_boxes=[dict(name=n,center=np.asarray(c).tolist(),size=np.asarray(s).tolist())for n,c,s in boxes])


def scene_document(project,game_start_story=False):
    # Follow a verified stretch of the existing authored countryside road back
    # toward the city. These are actual source points, not invented flat ground.
    road=np.load(ROOT/'assets/purchased/bep_reforged/countryside/landscape-review.npz')['road_native']
    origin=json.loads((project/'city_showcase.json').read_text())['origin']
    indices=list(range(450,119,-6))
    points=[]
    for i in indices:
        x=float(road[i,0]+origin[0]);z=float(road[i,1]+origin[2]);points.append([x,ground_height(project,x,z),z])
    if indices[-1]!=120:
        x=float(road[120,0]+origin[0]);z=float(road[120,1]+origin[2]);points.append([x,ground_height(project,x,z),z])
    arrival=points[0];yaw=math.atan2(points[1][0]-arrival[0],-(points[1][2]-arrival[2]))
    def world(x,y,z):return [float(HOME[0]+x),float(HOME[1]+y),float(HOME[2]+z)]
    home_route=[world(-.4,FLOOR,3.2),world(-.7,FLOOR,1.35),world(.6,FLOOR,2.45),world(3.65,FLOOR,2.45),world(3.65,FLOOR,3.75)]
    result=dict(schema='valwood.opening-scene',version=1,name='An Ordinary Morning / Meadows of Valois',
                canonical_scope='Modern father in a run-down house, transported into the Meadows of Valois. Neutral inspectable notebook and door are provisional demo bindings; no family events or transport cause are declared.',
                home=dict(physics='opening/home.settings',origin=HOME.tolist(),floor_y=600.18,
                          bounds=[-3505.5,-3494.5,-11005,-10995],spawn=[-3500.4,601.93,-10996.8,-.65,-.08],
                          inspect=dict(id='notebook',name='Notebook',position=world(-2.1,.85,1.35),approach=home_route[1],radius=1.8),
                          door=dict(id='front_door',name='Front door',position=world(3.65,1.25,4.95),approach=home_route[-1],radius=1.9),
                          walk_path=home_route,
                          cinematic=[dict(seconds=0,eye=world(.65,1.65,1.9),target=world(-2.6,1.2,3.0)),
                                     dict(seconds=3.8,eye=world(.3,1.6,2.8),target=world(-2.1,.85,1.35))]),
                meadow=dict(name='Meadows of Valois',arrival=[arrival[0],arrival[1]+1.75,arrival[2],yaw,-.025],
                            landmark=dict(id='follow_meadow_path',name='Follow the meadow path',position=points[-1],radius=3),
                            walk_path=points,path_length_m=sum(math.dist(a,b)for a,b in zip(points,points[1:])),
                            support='Installed original source primary height and countryside fallback; real scenery/static collision remains active',
                            cinematic=[dict(seconds=0,eye=[arrival[0]-2.5,arrival[1]+2.8,arrival[2]+4],target=[arrival[0]+2,arrival[1]+1,arrival[2]-2]),
                                       dict(seconds=4.5,eye=[arrival[0],arrival[1]+1.75,arrival[2]],target=[points[6][0],points[6][1]+1.75,points[6][2]])]))
    result['qa']=dict(home_inspect_path=home_route[:2],home_door_path=home_route[1:],meadow_path=points)
    if game_start_story:
        home=result['home']
        home_route=[world(-.4,FLOOR,3.2),world(-.7,FLOOR,1.35),world(-.7,FLOOR,.25),world(-1.85,FLOOR,.25),world(-1.85,FLOOR,.15)]
        home.update(walk_path=home_route,
            inspect=dict(id='household_bills',name='Household bills',position=world(*RENT_NOTICE_CENTER),approach=home_route[1],radius=2.),
            console=dict(id='console',name='Game console',position=world(-1.91,.773,-.6386),approach=home_route[-1],radius=1.7))
        home['cinematic'][0].update(eye=world(-.65,1.56,2.30),target=world(-2.55,.838,1.055))
        result.update(name='An Ordinary Evening / A New Game / Meadows of Valois',game_start_story=True,
            canonical_scope='An exhausted, overworked father struggling with living costs returns to his apartment. Reading the bills establishes his burden; turning on his console opens VALOIS, and choosing New Game causes the transport. Armor timing and larger family story remain unwritten.')
        result['qa']=dict(home_inspect_path=home_route[:2],home_console_path=home_route[1:],meadow_path=points)
    # The final rail frame is the actual gameplay camera pose, including its
    # facing and FOV; releasing cinematic control never cuts to another view.
    for sector,spawn in [(result['home'],result['home']['spawn']),(result['meadow'],result['meadow']['arrival'])]:
        eye=np.array(spawn[:3]);yaw,pitch=spawn[3:5]
        direction=np.array([math.sin(yaw)*math.cos(pitch),math.sin(pitch),-math.cos(yaw)*math.cos(pitch)])
        sector['cinematic'][-1].update(eye=eye.tolist(),target=(eye+direction*5).tolist(),fov=1.0471975512)
    reveal=result['meadow']['cinematic'][0]
    eye=np.array([arrival[0]-2.5,arrival[1]+3.8,arrival[2]+4.])
    yaw,pitch=1.75,.055
    direction=np.array([math.sin(yaw)*math.cos(pitch),math.sin(pitch),-math.cos(yaw)*math.cos(pitch)])
    reveal.update(eye=eye.tolist(),target=(eye+direction*5).tolist(),fov=1.12)
    return result


def author_waystone(project,scene):
    """A small authored weathered marker beside the existing clear road."""
    path=scene['meadow']['walk_path'];last=np.asarray(path[-1]);previous=np.asarray(path[-2])
    tangent=(last-previous)[[0,2]];tangent/=np.linalg.norm(tangent)
    side=np.array([-tangent[1],tangent[0]])*2.35
    x,z=last[[0,2]]+side;feet=ground_height(project,float(x),float(z))
    g=Glb();rock=ROOT/'assets/polyhaven/rock_boulder_dry'
    material=g.material('Weathered dry waystone',[.62,.67,.60],.95,
        base=rock/'rock_boulder_dry_diff_2k.jpg',normal=rock/'rock_boulder_dry_nor_gl_2k.jpg',arm=rock/'rock_boulder_dry_arm_2k.jpg')
    cut=g.material('Faint carved marker',[.11,.23,.22],.9,emissive=[.12,.25,.23])
    # Uneven beveled polygon rather than a rectangular pedestal.
    outline=np.array([[-.54,.0],[-.59,.22],[-.40,1.72],[-.22,1.98],[.19,1.91],[.43,1.68],[.52,.26],[.43,.0]])
    front=np.column_stack((outline,np.full(len(outline),-.24)))
    back=np.column_stack((outline,np.full(len(outline),.23)))
    positions=[];normals=[];uv=[];idx=[]
    def tri(a,b,c):
        n=np.cross(b-a,c-a);n/=np.linalg.norm(n);start=len(positions)
        for p in [a,b,c]:positions.append(p);normals.append(n);uv.append([p[0]+.6,p[1]*.7+p[2]])
        idx.extend([start,start+1,start+2])
    center_front=front.mean(0);center_back=back.mean(0)
    for i in range(len(front)):
        j=(i+1)%len(front)
        tri(center_front,front[i],front[j]);tri(center_back,back[j],back[i])
        tri(front[i],back[j],front[j]);tri(front[i],back[i],back[j])
    g.mesh(positions,normals,uv,idx,material)
    # The marker is provisional environmental dressing, without lore text.
    for center,size in [([0,1.30,-.244],[.035,.54,.012]),([0,1.55,-.244],[.32,.035,.012]),([0,1.06,-.244],[.25,.035,.012])]:g.box(center,size,cut)
    raw=g.finish();(project/'models/opening/waystone.glb').write_bytes(raw);(project/'models/opening/waystone_mid.glb').write_bytes(raw)
    with (project/'world.studio').open('a')as out:out.write('asset nm_opening_waystone opening/waystone 2 0 0 280 70 1\n')
    u=x/100800+.5;v=z/(100800*2818/5009)+.5
    with (project/'scenery/authored_instances.txt').open('a')as out:out.write(f'nm_opening_waystone {u:.17g} {v:.17g} 0 1 0 0 0 0 0 1 1 1 {feet:.12g}\n')
    # Match the small marker with one explicit static proxy; the shared source
    # city collision file is a private ordinary copy in this workspace.
    with (project/'city.physics').open('a')as out:out.write(f'body 100 "Opening waystone" 0 0 0 0 {x:.12g} {feet+.98:.12g} {z:.12g} 0 0 0 1 .54 .98 .24 .6 .01 1000 0 1 0 0 0 0 0 0 1\n')
    scene['meadow']['landmark'].update(id='waystone',name='Waystone',position=[float(x),feet,float(z)],approach=path[-1],radius=3.)
    return dict(position=[float(x),feet,float(z)],approach=path[-1],glb_bytes=len(raw),
                source_materials={str(p):digest(p)for p in rock.glob('*.jpg')})


def config_bytes(scene):
    def values(row):return ' '.join(format(float(v),'.15g')for v in row)
    home=scene['home'];meadow=scene['meadow'];hs=home['spawn'];ms=meadow['arrival']
    rows=['VALWOOD_OPENING_SCENE 1',
          'house_spawn '+values([hs[0],hs[1]-1.75,hs[2],hs[3],hs[4]]),
          'meadow_spawn '+values([ms[0],ms[1]-1.75,ms[2],ms[3],ms[4]]),
          'house_focus '+values(home['inspect']['position']),
          'house_exit '+values(home['console' if scene.get('game_start_story') else 'door']['position']),
          'meadow_goal '+values(meadow['landmark']['position'])]
    def rail(name,a,b,fov_a=.95,fov_b=1.0471975512):
        rows.append(name+' '+values([*a['eye'],*b['eye'],*a['target'],*b['target'],fov_a,fov_b]))
    rail('house_rail',home['cinematic'][0],home['cinematic'][-1])
    if scene.get('game_start_story'):
        rail('transport_rail',dict(eye=[-3502.799567,601.267020,-10999.1883],target=[-3502.799567,601.267020,-11000.789]),
             dict(eye=[-3502.799567,601.267020,-11000.24],target=[-3502.799567,601.267020,-11000.789]),.90,.70)
    else:
        door=home['door']['position'];ap=home['door']['approach']
        rail('transport_rail',dict(eye=[ap[0],ap[1]+1.75,ap[2]],target=door),
             dict(eye=[ap[0],ap[1]+1.66,ap[2]+.25],target=[door[0],door[1]+.15,door[2]]),1.0471975511966,.90)
    rail('meadow_rail',meadow['cinematic'][0],meadow['cinematic'][-1],meadow['cinematic'][0].get('fov',.87),1.0471975512)
    rows+=['house_waypoint '+values(p)for p in home['walk_path']]
    rows+=['meadow_waypoint '+values(p)for p in meadow['walk_path']]
    return ('\n'.join(rows)+'\n').encode()


def build(destination,game_start_story=False):
    source=(ROOT/'projects/valwood_city_showcase').resolve(strict=True)
    destination=Path(destination).resolve()
    if destination.exists()or destination.is_symlink():raise ValueError('Output must be a new directory')
    if destination.is_relative_to(ROOT/'projects'):raise ValueError('Never write source projects')
    files,banks=inventory(source);guard={name:digest(path)for name,path in files.items()}
    if any(destination==bank or destination.is_relative_to(bank)for bank in banks.values()):
        raise ValueError('Output cannot enter an immutable source bank')
    destination.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='.opening-',dir=destination.parent))
    try:
        project=stage/'project';project.mkdir()
        for name,path in files.items():checked_copy(path,project/name,guard[name])
        for name,target in banks.items():
            if name not in {'models','scenery'}:(project/name).symlink_to(target,target_is_directory=True)
            else:
                (project/name).mkdir()
                for entry in target.iterdir():
                    if name=='scenery'and entry.name=='authored_instances.txt':continue
                    (project/name/entry.name).symlink_to(entry.resolve(strict=True),target_is_directory=entry.is_dir())
        (project/'saves').mkdir()
        house=author_house(project,game_start_story)
        house['indoor_shadow_policy']=patch_house_shadow(project)
        house['tv_animation_shader']=patch_home_tv(project)
        house['room_light_emission_shader']=patch_home_lights(project)
        original=(banks['scenery']/'authored_instances.txt').read_text()
        u=HOME[0]/100800+.5;v=HOME[2]/(100800*2818/5009)+.5
        (project/'scenery/authored_instances.txt').write_text(original+f'\n# Isolated authored modern opening home; collision uses exact separate support.\n{ALIAS} {u:.17g} {v:.17g} 0 1 0 0 0 0 0 1 1 1 600\n')
        studio=(project/'world.studio').read_text()
        studio+='\nasset '+ALIAS+' opening/home 2 0 0 240 75 1\n'
        # Warm domestic practicals with a broad cool key inside the window.
        studio+='light -3504.69 601.60 -10996.48 4.5 1.35 1 .78 .55 0 -45 180 0\n'
        studio+=f'light -3495.3 602.15 {HOME[2]+HOME_WINDOW_Z:.12g} 8.0 2.4 .72 .84 1 0 -45 180 0\n'
        studio+='light -3501 602.95 -10998.7 8 .55 1 .91 .78 0 -45 180 0\n'
        studio+='light -3500.22 601.58 -11004.48 3.8 .9 1 .88 .69 0 -45 180 0\n'
        studio+='light -3500.22 602.45 -11001.25 4.8 .9 1 .82 .61 0 -45 180 0\n'
        studio+='light -3494.97 600.99 -11003.97 2.6 .65 1 .79 .54 0 -45 180 0\n'
        studio+='light -3495.65 602.55 -10999.55 3.4 .85 1 .90 .78 0 -45 180 0\n'
        (project/'world.studio').write_text(studio)
        (project/'project.valwood').write_text('name=Legends of Valwood | '+('An Ordinary Evening' if game_start_story else 'An Ordinary Morning')+'\nterrain=valwood_atlas\nspawn=-3500.4 -10996.8 -.65\ncamera=0 0 7\nday_fraction=0.6\n')
        scene=scene_document(project,game_start_story)
        waystone=author_waystone(project,scene)
        (project/'opening.scene').write_bytes(canonical(scene))
        (project/'opening.config').write_bytes(config_bytes(scene))
        (project/'opening/layout.json').write_bytes(canonical(house))
        inspection=ROOT/'docs/opening-apartment-refresh/2026-11-02/inspection.views.json'
        if inspection.is_file():(project/'opening/inspection.views.json').write_bytes(inspection.read_bytes())
        (project/'showcase.views').write_text((project/'showcase.views').read_text()+'\nopening-home -3500.4 601.93 -10996.8 -.65 -.08 1.0471975511966\n')
        # Opening progress belongs to this private workspace; existing authoring
        # saves and the environment playtests are never copied or refreshed.
        generated={str(p.relative_to(project)):digest(p)for p in project.rglob('*')if p.is_file()and not p.is_symlink()}
        after_files,after_banks=inventory(source)
        if {name:digest(path)for name,path in after_files.items()}!=guard or banks!=after_banks:raise ValueError('Source project changed during build')
        if any(digest(Path(row['source']))!=row['sha256']for row in house['furnishings']):raise ValueError('Apartment source changed')
        manifest=dict(schema='valwood.opening-local-copy',version=1,source_project=str(source),source_guard=guard,
                      source_project_unchanged=True,shared_banks={n:str(p)for n,p in banks.items()},
                      generated=generated,house=house,waystone=waystone,
                      game_start_story=game_start_story,
                      policy='Private root/AI/shaders/saves, local models/scenery overlays, read-only linked asset banks; launching requires the opening-aware runtime. Story mode implements the requested father, financial pressure and console New Game transport; no extra family events are asserted.')
        (stage/'opening-origin.json').write_bytes(canonical(manifest))
        stage.rename(destination)
        return dict(passed=True,project=str(destination/'project'),manifest=str(destination/'opening-origin.json'),
                    house_bytes=house['glb_bytes'],house_collision_bodies=house['collision_bodies'],
                    source_unchanged=True,home_spawn=scene['home']['spawn'],meadow_arrival=scene['meadow']['arrival'],
                    meadow_path_m=scene['meadow']['path_length_m'])
    finally:
        if stage.exists():shutil.rmtree(stage)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output-root',type=Path,default=ROOT/'build/opening-prologue');p.add_argument('--game-start-story',action='store_true');args=p.parse_args()
    try:result=build(args.output_root,args.game_start_story)
    except (OSError,ValueError,KeyError)as error:p.exit(1,f'Opening build failed: {error}\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
