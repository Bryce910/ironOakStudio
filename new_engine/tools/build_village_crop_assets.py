#!/usr/bin/env python3
"""Author reproducible, rowed maize and wheat clusters for Valwood.

python3 tools/build_village_crop_assets.py --build
python3 tools/build_village_crop_assets.py --validate
CPU preview of actual cooked files:
    blender -b -t 2 --disable-autoexec --factory-startup \
      --python tools/build_village_crop_assets.py -- --preview

No external assets, purchased source, Blender scripts, alpha grass cards or
runtime scatter are used. Every stalk/leaf/grain is authored geometry. Both
LODs retain the same roots and plant heights; only surface detail is reduced.
All coordinates below are metres, Y up. Generated source recipes and images
are retained under assets/authored/village_crops/.
"""
from pathlib import Path
import hashlib
import io
import json
import math
import os
import random
import struct
import sys
import tempfile
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/authored/village_crops'
OUT=ROOT/'projects/valwood_world/models'
MANIFEST='village_crops_manifest.json'
SEED=0x56414C57
ROW_X={'corn':[-1.5,-.5,.5,1.5], 'wheat':[-1.875+i*.25 for i in range(16)]}
INSET={'corn':.20,'wheat':.05}
BUDGET={'corn':{'near':8000,'mid':1500},'wheat':{'near':8000,'mid':3000}}
STEMS_PER_ROW={'corn':5,'wheat':22}
JITTER_Z={'corn':.027,'wheat':.012}
LEAN_Z={'corn':.045,'wheat':.008}

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def unit(v):
    v=np.asarray(v,dtype=float);length=np.linalg.norm(v)
    assert length>1e-12
    return v/length

def prepare():
    SOURCE.mkdir(parents=True,exist_ok=True)
    rng=random.Random(SEED)
    recipe={'version':1,'seed':SEED,'authoring':'Original deterministic geometry and PBR surface authoring for this project.',
        'coordinate_system':'Metres, Y up, rootsY0; cluster centres on a4m grid, rows parallelZ.',
        'cluster_footprint_m':[4,4],'geometry_inset_m':INSET,'plants':{}}
    for crop in ['corn','wheat']:
        # Periodic spacing through the 4 m boundary avoids a visible seam in
        # repeated clusters. Jitter is deliberately much smaller than spacing.
        plants=[];count=STEMS_PER_ROW[crop];step=4./count
        for row,x in enumerate(ROW_X[crop]):
            for j in range(count):
                plants.append(dict(row=row,x=x,z=(j-(count-1)*.5)*step+rng.uniform(-JITTER_Z[crop],JITTER_Z[crop]),
                    height=rng.uniform(1.83,2.01) if crop=='corn' else rng.uniform(.74,.87),
                    phase=rng.uniform(0,math.tau),variation=rng.randrange(3),
                    lean_x=rng.uniform(-.012,.012),lean_z=rng.uniform(-LEAN_Z[crop],LEAN_Z[crop]),
                    leaf_turn=rng.uniform(-.25,.25),head=rng.uniform(.15,.19)))
        recipe['plants'][crop]=plants
    (SOURCE/'crop_recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    # One original, opaque, shared4x4 atlas. UVs select physical surface types
    # and stable color variations; no embedded illumination or cutout shapes.
    side=512;y,x=np.mgrid[:side,:side];u=(x%128)/127.;v=(y%128)/127.
    base=np.zeros((side,side,3),dtype=float);height=np.zeros((side,side),dtype=float)
    rough=np.full((side,side),.82,dtype=float)
    palettes=[
        [(57,94,25),(71,105,30),(83,112,35),(152,127,60)],
        [(101,128,49),(121,140,57),(103,125,44),(126,84,39)],
        [(171,137,61),(184,148,67),(164,143,73),(184,158,90)],
        [(193,153,81),(181,137,60),(210,175,104),(176,145,77)]
    ]
    for row in range(4):
        for col in range(4):
            region=(slice(row*128,(row+1)*128),slice(col*128,(col+1)*128))
            a,b=u[region],v[region];color=np.asarray(palettes[row][col],dtype=float)
            veins=np.sin(a*math.tau*22.+.20*np.sin(b*14.))
            fibers=np.sin(a*math.tau*67.+b*7.)*.25+np.sin(a*13.+b*97.)*.12
            if row==0 and col<3:
                tip=np.clip((b-.60)/.4,0,1)
                color_field=color[None,None,:]*(.80+.20*np.sin(a*math.pi))[...,None]
                color_field=color_field*(1-tip[...,None]*.22)+np.array([110,97,38])*tip[...,None]*.22
                h=.016*veins+.007*fibers+.025*np.exp(-((a-.5)/.018)**2)
                rough[region]=.76+.07*b
            elif row==3 and col<3:
                color_field=color[None,None,:]*(.91+.08*np.cos(a*math.tau)+.04*np.sin(b*17.))[...,None]
                h=.025*np.cos(a*math.tau)+.008*np.sin(a*13.+b*31.)
                rough[region]=.72
            else:
                color_field=color[None,None,:]*(.90+.06*veins+.04*fibers)[...,None]
                h=.013*veins+.006*fibers
            base[region]=color_field;height[region]=h
    dx=np.gradient(height,axis=1)*2.0;dy=np.gradient(height,axis=0)*2.0
    normals=np.stack((-dx,dy,np.ones_like(dx)),axis=-1);normals/=np.linalg.norm(normals,axis=2,keepdims=True)
    Image.fromarray(np.uint8(np.clip(np.rint(base),0,255))).save(SOURCE/'crop_basecolor.png',compress_level=6)
    Image.fromarray(np.uint8(np.clip(np.rint((normals+1)*127.5),0,255))).save(SOURCE/'crop_normal_opengl.png',compress_level=6)
    orm=np.stack((np.full_like(rough,255),rough*255,np.zeros_like(rough)),axis=-1)
    Image.fromarray(np.uint8(np.clip(np.rint(orm),0,255))).save(SOURCE/'crop_orm.png',compress_level=6)
    provenance={'schema_version':1,'source':'Original project-authored meshes, crop recipes and procedural PBR surface images.',
        'external_sources':[],'license_note':'Project-authored content; no third-party content or redistribution restrictions introduced.',
        'seed':SEED,'script':'tools/build_village_crop_assets.py','recipe_sha256':sha(SOURCE/'crop_recipe.json'),
        'images':[{'file':p.name,'sha256':sha(p),'bytes':p.stat().st_size,'size':[512,512]}
            for p in sorted(SOURCE.glob('crop_*.png'))],
        'material_semantics':'Basecolor sRGB; OpenGL tangent normal linear; ORM linear(R=unoccluded1,G=physical roughness,B=nonmetal0). No emissive, alpha mask or baked light.'}
    (SOURCE/'PROVENANCE.json').write_text(json.dumps(provenance,indent=2)+'\n')
    return recipe

def atlas_uv(region,u,v):
    col,row=region
    # 3px inset avoids neighboring material colors at the common near mips.
    return ((col*128+3+u*122)/512.,(row*128+3+v*122)/512.)

class Mesh:
    def __init__(self):self.positions=[];self.normals=[];self.uv=[];self.indices=[]
    def grid(self,points,region):
        p=np.asarray(points,dtype=float);rows,cols=p.shape[:2];start=len(self.positions)
        normals=np.zeros_like(p);faces=[]
        for j in range(rows-1):
            for i in range(cols-1):
                for tri in [(j*cols+i,(j+1)*cols+i,(j+1)*cols+i+1),(j*cols+i,(j+1)*cols+i+1,j*cols+i+1)]:
                    # Tubes are parameterized along their axis then around
                    # their cross-section; leaf upper surfaces use the same
                    # grid convention. Reverse this order for outward normals.
                    tri=tuple(reversed(tri))
                    flat=p.reshape(-1,3);n=np.cross(flat[tri[1]]-flat[tri[0]],flat[tri[2]]-flat[tri[0]])
                    if np.linalg.norm(n)<1e-12:continue
                    faces.append(tri)
                    for k in tri:normals.reshape(-1,3)[k]+=n
        for j in range(rows):
            for i in range(cols):
                self.positions.append(p[j,i]);self.normals.append(unit(normals[j,i]))
                self.uv.append(atlas_uv(region,i/(cols-1),j/(rows-1)))
        for tri in faces:self.indices.extend(start+k for k in tri)
    def triangle(self,points,region,desired=None):
        p=np.asarray(points,dtype=float);normal=np.cross(p[1]-p[0],p[2]-p[0])
        if np.linalg.norm(normal)<1e-12:return
        if desired is not None and np.dot(normal,desired)<0:p=p[[0,2,1]];normal=-normal
        start=len(self.positions)
        for a,uv in zip(p,[(.08,.08),(.92,.08),(.5,.92)]):
            self.positions.append(a);self.normals.append(unit(normal));self.uv.append(atlas_uv(region,*uv))
        self.indices.extend([start,start+1,start+2])
    def tube(self,path,radii,sides,region):
        p=np.asarray(path);axis=unit(p[-1]-p[0]);u=unit(np.cross(axis,[1,0,0] if abs(axis[0])<.8 else [0,0,1]));v=np.cross(axis,u)
        rings=[]
        for centre,radius in zip(p,radii):
            ring=[centre+radius*(u*math.cos(i*math.tau/sides)+v*math.sin(i*math.tau/sides))for i in range(sides+1)]
            if centre[1]==0:
                for point in ring:point[1]=0
            rings.append(ring)
        self.grid(rings,region)
    def grain(self,a,b,width,region):
        a=np.asarray(a);b=np.asarray(b);axis=unit(b-a);u=unit(np.cross(axis,[1,0,0] if abs(axis[0])<.8 else [0,0,1]));v=np.cross(axis,u);c=(a+b)*.5
        ring=[c+width*(u*math.cos(i*math.tau/3)+v*math.sin(i*math.tau/3))for i in range(3)]
        for i in range(3):
            j=(i+1)%3
            self.triangle([a,ring[i],ring[j]],region,(ring[i]+ring[j])*.5-c)
            self.triangle([b,ring[j],ring[i]],region,(ring[i]+ring[j])*.5-c)
    def tetra_grain(self,a,b,width,region):
        a=np.asarray(a);b=np.asarray(b);axis=unit(b-a);u=unit(np.cross(axis,[1,0,0]if abs(axis[0])<.8 else [0,0,1]));v=np.cross(axis,u)
        c=(a+b)*.5+u*width;d=(a+b)*.5+v*width;centre=(a+b+c+d)*.25
        for points in [[a,b,c],[a,d,b],[a,c,d],[b,d,c]]:
            self.triangle(points,region,np.mean(points,axis=0)-centre)

def leaf(mesh,origin,angle,length,width,rise,droop,segments,cross,region,limit=1.798):
    origin=np.asarray(origin,dtype=float)
    # Fit the authored leaf inside its source cell. Both LODs use the same
    # high-detail control curve when computing this fit, never camera data.
    def points(L,segs,cols):
        result=[]
        for j in range(segs+1):
            t=j/segs;turn=angle+.08*math.sin(t*math.pi);direction=np.array([math.cos(turn),0,math.sin(turn)])
            side=np.array([-direction[2],0,direction[0]])
            centre=origin+direction*(L*math.sin(t*math.pi/2))+[0,rise*L*math.sin(t*math.pi)-droop*L*t*t,0]
            w=width*(.035+.965*math.sin(t*math.pi)**.72)
            row=[]
            for i in range(cols):
                s=i/(cols-1)*2-1
                row.append(centre+side*w*s+[0,(1-abs(s))*.010*math.sin(t*math.pi),0])
            result.append(row)
        return result
    for attempt in range(32):
        trial=np.asarray(points(length,8,3))
        if np.max(np.abs(trial[:,:,[0,2]]))<=limit:break
        length*=.92
    assert np.max(np.abs(trial[:,:,[0,2]]))<=limit+.002
    mesh.grid(points(length,segments,cross),region)

def corn(mesh,plant,lod):
    x,z,h=plant['x'],plant['z'],plant['height'];phase=plant['phase'];variation=plant['variation'];near=lod=='near'
    root=np.array([x,0,z]);lean=np.array([plant['lean_x'],0,plant['lean_z']])
    stem=lambda t:root+lean*t*t+[0,h*t,0]
    path=[stem(t)for t in ([0,.35,.70,1]if near else [0,.5,1])]
    mesh.tube(path,[.019*(1-.5*t)for t in ([0,.35,.70,1]if near else [0,.5,1])],5 if near else 3,(variation%2,1))
    for i in range(8):
        t=.18+i*.096;angle=math.pi/2+(math.pi if i%2 else 0)+plant['leaf_turn']+.06*math.sin(phase+i)
        length=.60+.24*math.sin(i*.46+.3)+.07*math.sin(phase+i)
        leaf(mesh,stem(t),angle,length,.052+.022*math.sin((i+1)*.39),.38,.29,4 if near else 2,3 if near else 2,(variation,0))
    angle=phase;direction=np.array([math.cos(angle),.9,math.sin(angle)]);direction=unit(direction)
    ear=stem(.57)
    # Edge plants turn ears inward so the husk and silk stay inside the
    # authored cell without clipping their geometry or changing stalk roots.
    for axis in [0,2]:
        if abs(ear[axis]+direction[axis]*.27)>1.74:direction[axis]*=-1
    angle=math.atan2(direction[2],direction[0]);tip=ear+direction*.27
    if near:
        mid=ear+(tip-ear)*.54
        mesh.tube([ear,mid,tip],[.008,.046,.006],6,(2,1))
        # Exposed silk curls and a warm kernel tip separate ears from leaves.
        for j in range(3):
            side=np.array([math.cos(angle+j*1.7),0,math.sin(angle+j*1.7)])
            silk=[tip,tip+side*.025+[0,.045,0],tip+side*.04+[0,.055,0]]
            mesh.tube(silk,[.0018,.0014,.0008],3,(3,1))
    else:mesh.grain(ear,tip,.040,(2,1))
    top=stem(1);tassel_tip=top+[0,.24,0]
    mesh.tube([top,tassel_tip],[.004,.001],3,(3,0))
    for j in (range(5)if near else [0,2,4]):
        a=phase+j*math.tau/5;start=top+[0,.045+.014*(j%2),0]
        end=start+[math.cos(a)*.115,.125+.018*math.sin(j+phase),math.sin(a)*.115]
        mesh.tube([start,end],[.003,.001],3,(3,0))

def wheat(mesh,plant,lod):
    x,z,h=plant['x'],plant['z'],plant['height'];phase=plant['phase'];variation=plant['variation'];near=lod=='near';head=plant['head']
    root=np.array([x,0,z]);lean=np.array([plant['lean_x'],0,plant['lean_z']])
    origin=root+lean+[0,h,0];axis=unit([plant['lean_x']*.25,1,plant['lean_z']*.7]);top=origin+axis*head
    if near:
        mesh.tube([root,top],[.0048,.0018],3,(variation,2))
        for j in range(2):
            angle=math.pi/2+plant['leaf_turn']+(math.pi if j else 0)
            leaf(mesh,root+lean*.5+[0,h*(.29+j*.39),0],angle,.26+.035*math.sin(phase+j),.014+.004*j,
                 .4,.15,2 if j==0 else 1,2,(3,2),1.948)
    else:
        # A three-sided cone preserves each rooted stem silhouette with
        # three faces; it is still volume geometry, not a grass billboard.
        ring=[root+np.array([math.cos(i*math.tau/3)*.0048,0,math.sin(i*math.tau/3)*.0048])for i in range(3)]
        for i in range(3):
            radial=(ring[i]+ring[(i+1)%3])*.5-root
            mesh.triangle([ring[i],ring[(i+1)%3],top],(variation,2),radial)
        leafroot=root+lean*.5+[0,h*.56,0];angle=math.pi/2+plant['leaf_turn']
        tip=leafroot+np.array([math.cos(angle)*.16,.02,math.sin(angle)*.16])
        if max(abs(tip[0]),abs(tip[2]))>1.93:tip=leafroot-(tip-leafroot)
        side=np.array([-math.sin(angle)*.014,0,math.cos(angle)*.014])
        mesh.triangle([leafroot-side,leafroot+side,tip],(3,2),[0,1,0])
    out=unit([math.cos(phase)*.35,0,math.sin(phase)])
    # Boundary seed heads turn sideways along the field, leaving awns and
    # physical grain volumes inside the source cell without cut geometry.
    if abs(z)>1.75:out=np.array([1 if math.cos(phase)>=0 else -1,0,0])
    upper=top+axis*.08
    if near:
        # Two full staggered grain lobes and fine awns provide a readable ear
        # at walking distance; denser planting gets the triangle budget.
        for sign in [-1,1]:
            start=origin+axis*(head*.12);end=origin+axis*(head*.9)+out*sign*.019
            mesh.tetra_grain(start,end,.014,(variation,3))
            awn=upper+out*sign*.012;side=unit(np.cross(axis,out))*.00065
            mesh.triangle([end-side,end+side,awn],(3,3))
    else:
        # Keep the complete head/awn height in a four-face tapered volume.
        mesh.tetra_grain(origin,upper,.012,(variation,3))

def glb(path):
    raw=Path(path).read_bytes();assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw))
    size,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a
    data=json.loads(raw[20:20+size]);offset=20+size;length,kind=struct.unpack_from('<II',raw,offset);assert kind==0x004e4942
    return data,raw[offset+8:offset+8+length]

def export(mesh,path):
    arrays=[np.asarray(mesh.positions,dtype='<f4'),np.asarray(mesh.normals,dtype='<f4'),np.asarray(mesh.uv,dtype='<f4'),np.asarray(mesh.indices,dtype='<u4')]
    output=bytearray();views=[]
    def add(raw,target=None):
        output.extend(b'\0'*(-len(output)%4));v={'buffer':0,'byteOffset':len(output),'byteLength':len(raw)}
        if target:v['target']=target
        views.append(v);output.extend(raw);return len(views)-1
    for i,a in enumerate(arrays):add(a.tobytes(),34963 if i==3 else 34962)
    images=[]
    for name in ['crop_basecolor.png','crop_normal_opengl.png','crop_orm.png']:
        images.append({'name':name,'bufferView':add((SOURCE/name).read_bytes()),'mimeType':'image/png'})
    lo=arrays[0].min(axis=0).tolist();hi=arrays[0].max(axis=0).tolist()
    data={'asset':{'version':'2.0','generator':'Valwood original authored crop geometry v1'},'scene':0,'scenes':[{'nodes':[0]}],
        'nodes':[{'name':path.stem,'mesh':0}], 'meshes':[{'name':path.stem,'primitives':[{'attributes':{'POSITION':0,'NORMAL':1,'TEXCOORD_0':2},'indices':3,'material':0}]}],
        'buffers':[{'byteLength':len(output)}],'bufferViews':views,
        'accessors':[{'bufferView':0,'componentType':5126,'count':len(arrays[0]),'type':'VEC3','min':lo,'max':hi},
            {'bufferView':1,'componentType':5126,'count':len(arrays[1]),'type':'VEC3'},
            {'bufferView':2,'componentType':5126,'count':len(arrays[2]),'type':'VEC2'},
            {'bufferView':3,'componentType':5125,'count':len(arrays[3]),'type':'SCALAR'}],
        'images':images,'samplers':[{'minFilter':9987,'magFilter':9729,'wrapS':10497,'wrapT':10497}],
        'textures':[{'sampler':0,'source':i}for i in range(3)],
        'materials':[{'name':'Original crop surfaces shared PBR atlas','pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],
            'baseColorTexture':{'index':0},'metallicFactor':0,'roughnessFactor':1,'metallicRoughnessTexture':{'index':2}},
            'normalTexture':{'index':1},'alphaMode':'OPAQUE','doubleSided':True,'emissiveFactor':[0,0,0]}]}
    text=json.dumps(data,separators=(',',':')).encode();text+=b' '*(-len(text)%4);output+=b'\0'*(-len(output)%4)
    path.write_bytes(struct.pack('<III',0x46546c67,2,28+len(text)+len(output))+struct.pack('<II',len(text),0x4e4f534a)+text+
        struct.pack('<II',len(output),0x004e4942)+output)

def inspect(path):
    data,binary=glb(path);assert len(data['meshes'])==1 and len(data['materials'])==1 and len(data['images'])==3
    assert not data.get('skins')and not data.get('animations')and not data.get('extensionsRequired')
    assert len(data['meshes'][0]['primitives'])==1
    p=data['meshes'][0]['primitives'][0];assert p.get('mode',4)==4 and set(p['attributes'])=={'POSITION','NORMAL','TEXCOORD_0'}
    def values(index):
        a=data['accessors'][index];v=data['bufferViews'][a['bufferView']];n={'SCALAR':1,'VEC2':2,'VEC3':3}[a['type']]
        return np.frombuffer(binary,dtype='<f4'if a['componentType']==5126 else '<u4',count=a['count']*n,offset=v.get('byteOffset',0)).reshape(a['count'],n)
    position=values(0);normal=values(1);uv=values(2);index=values(3).reshape(-1,3)
    assert len(position)==len(normal)==len(uv)and np.isfinite(position).all()and np.isfinite(normal).all()and np.isfinite(uv).all()
    assert index.max()<len(position)and np.max(np.abs(np.linalg.norm(normal,axis=1)-1))<.003
    faces=np.cross(position[index[:,1]]-position[index[:,0]],position[index[:,2]]-position[index[:,0]])
    nondegenerate=np.linalg.norm(faces,axis=1)>1e-11;assert nondegenerate.all()
    alignment=np.einsum('ij,ij->i',faces,normal[index].sum(axis=1));assert (alignment>0).all()
    assert uv.min()>=0 and uv.max()<=1
    lo=position.min(axis=0);hi=position.max(axis=0)
    limit=1.95 if 'wheat'in path.name else 1.8
    assert abs(lo[1])<1e-7 and np.max(np.abs(position[:,[0,2]]))<=limit+.00001,(path,lo,hi)
    assert not data['nodes'][0].get('matrix')and not data['nodes'][0].get('translation')and not data['nodes'][0].get('scale')
    material=data['materials'][0];assert material['alphaMode']=='OPAQUE'and material['doubleSided']and material['emissiveFactor']==[0,0,0]
    assert material['pbrMetallicRoughness']['metallicFactor']==0
    images=[]
    for record in data['images']:
        v=data['bufferViews'][record['bufferView']];encoded=binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
        im=Image.open(io.BytesIO(encoded));assert im.size==(512,512)
        assert hashlib.sha256(encoded).hexdigest()==sha(SOURCE/record['name'])
        images.append({'name':record['name'],'size':list(im.size),'sha256':hashlib.sha256(encoded).hexdigest(),'bytes':len(encoded)})
    return {'triangles':len(index),'vertices':len(position),'primitive_count':1,'material_count':1,
        'bounds_min':lo.tolist(),'bounds_max':hi.tolist(),'dimensions_m':(hi-lo).tolist(),'images':images,
        'sha256':sha(path),'bytes':path.stat().st_size,
        'geometry_validation':{'finite':True,'unit_normals':True,'in_range_indices':True,'all_triangles_nondegenerate':True,
            'all_normals_agree_with_winding':True,'alpha_cards':False}}

def validate_surface_orientation():
    # Independent physical orientation checks catch consistently reversed
    # indices AND normals, which a winding/agreement check cannot detect.
    tube=Mesh();tube.tube([[0,0,0],[0,1,0]],[.1,.1],8,(0,1))
    p=np.asarray(tube.positions);indices=np.asarray(tube.indices).reshape(-1,3)
    face=np.cross(p[indices[:,1]]-p[indices[:,0]],p[indices[:,2]]-p[indices[:,0]])
    centre=p[indices].mean(axis=1);radial=centre.copy();radial[:,1]=0
    assert (np.einsum('ij,ij->i',face,radial)>0).all(),'Tube surface must face outward.'
    blade=Mesh();leaf(blade,[0,.6,0],math.pi/2,.5,.06,.15,.05,4,3,(0,0))
    assert np.asarray(blade.normals)[:,1].min()>0,'Leaf upper surface must face upward.'

def validate(directory=OUT):
    validate_surface_orientation()
    directory=Path(directory);manifest=json.loads((directory/MANIFEST).read_text());recipe=json.loads((SOURCE/'crop_recipe.json').read_text())
    assert manifest['source_recipe_sha256']==sha(SOURCE/'crop_recipe.json')
    assert len(manifest['assets'])==4;keys=set()
    for record in manifest['assets']:
        actual=inspect(directory/record['file']);assert actual['sha256']==record['sha256']
        assert actual['triangles']<=BUDGET[record['alias']][record['lod']]
        crop=record['alias'];plants=recipe['plants'][crop]
        assert record['plant_count']==len(plants)==len(ROW_X[crop])*STEMS_PER_ROW[crop]
        root_hash=hashlib.sha256(json.dumps([[p['x'],p['z'],p['height']]for p in plants],separators=(',',':')).encode()).hexdigest()
        assert record['plant_roots_sha256']==root_hash
        assert record['stems_per_row']==STEMS_PER_ROW[crop]and record['stem_spacing_z_m']==4/STEMS_PER_ROW[crop]
        for row,x in enumerate(ROW_X[crop]):
            row_plants=[p for p in plants if p['row']==row];assert len(row_plants)==STEMS_PER_ROW[crop]
            for j,plant in enumerate(row_plants):
                nominal=(j-(STEMS_PER_ROW[crop]-1)*.5)*4/STEMS_PER_ROW[crop]
                assert plant['x']==x and abs(plant['z']-nominal)<=JITTER_Z[crop]+1e-10
        assert record['cluster_footprint_m']==[4,4]and record['wind_reach_m']==.45
        for image in actual['images']:keys.add(image['sha256'])
    for crop in ['corn','wheat']:
        pair=[a for a in manifest['assets']if a['alias']==crop]
        assert pair[0]['plant_roots_sha256']==pair[1]['plant_roots_sha256']
        assert max(abs(pair[0]['bounds_min'][i]-pair[1]['bounds_min'][i])for i in range(3))<.06
        assert max(abs(pair[0]['bounds_max'][i]-pair[1]['bounds_max'][i])for i in range(3))<.06
        assert all(pair[0]['bounds_min'][i]<=pair[1]['bounds_min'][i]and pair[0]['bounds_max'][i]>=pair[1]['bounds_max'][i]for i in range(3))
    assert len(keys)==3
    budget={'unique_images':3,'max_image_side':512,'estimated_rgba8_mips_mib':3*512*512*4*4/3/1048576,
        'model_bytes':sum(a['bytes']for a in manifest['assets']),
        'near_triangles':{a['alias']:a['triangles']for a in manifest['assets']if a['lod']=='near'},
        'mid_triangles':{a['alias']:a['triangles']for a in manifest['assets']if a['lod']=='mid'}}
    print('VALIDATED authored crop models:',json.dumps(budget),flush=True);return budget

def build():
    recipe=prepare();OUT.mkdir(parents=True,exist_ok=True);records=[]
    with tempfile.TemporaryDirectory(prefix='.village-crops-',dir=OUT)as temporary:
        stage=Path(temporary)
        for crop in ['corn','wheat']:
            plants=recipe['plants'][crop];root_hash=hashlib.sha256(json.dumps([[p['x'],p['z'],p['height']]for p in plants],separators=(',',':')).encode()).hexdigest()
            for lod in ['near','mid']:
                mesh=Mesh()
                for plant in plants:(corn if crop=='corn'else wheat)(mesh,plant,lod)
                name='village_crop_'+crop+'_'+lod+'.glb';export(mesh,stage/name)
                record={'alias':crop,'runtime_alias':'village_crop_'+crop,'lod':lod,'file':name,'origin':'XZclustercentre; source rootsY0; metresYup.',
                    'cluster_footprint_m':[4,4],'geometry_inset_m':INSET[crop],'wind_reach_m':.45,
                    'plant_count':len(plants),'plant_roots_sha256':root_hash,'row_axis':'Z','row_x_m':ROW_X[crop],
                    'row_spacing_m':1 if crop=='corn'else .25,'row_phase_x_m':.5 if crop=='corn'else .125,
                    'stems_per_row':STEMS_PER_ROW[crop],'stem_spacing_z_m':4/STEMS_PER_ROW[crop],
                    'stem_jitter_z_m':JITTER_Z[crop],'periodic_length_z_m':4,
                    **inspect(stage/name)}
                records.append(record);print('COOKED',name,record['triangles'],record['bounds_min'],record['bounds_max'],flush=True)
        manifest={'schema_version':1,'provenance':'assets/authored/village_crops/PROVENANCE.json',
            'recipe':'assets/authored/village_crops/crop_recipe.json','source_recipe_sha256':sha(SOURCE/'crop_recipe.json'),
            'source':'Original authored curved leaves, cylindrical stalks, tassels/husked ears and3D wheat grains/awns; no external models or alpha rectangles.',
            'instance_contract':'Clustercentre4mgrid, yaw0orpi preserves rows; rootY0. BothLODs retainallsourceplants/rootpositions/heights. Corn20plants,wheat352headspercluster.',
            'lod_contract':'Samephysicalplants; midleaves/tassels/heads simplifygeometry, no thinning. Runtimeowns complementary world-stable56–64m near/mid transition and280–320m distantfade.',
            'assets':records}
        (stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n');manifest['budget']=validate(stage)
        (stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n')
        for record in records:os.replace(stage/record['file'],OUT/record['file'])
        os.replace(stage/MANIFEST,OUT/MANIFEST)
    validate()

def preview():
    """CPU-render actual cooked GLBs. This is an asset QA image, not gameplay."""
    import bpy
    from mathutils import Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for crop,centres in [('corn',[(-4,0),(0,0),(-4,4),(0,4)]),('wheat',[(7,0),(11,0),(7,4),(11,4)])]:
        before=set(bpy.context.scene.objects)
        bpy.ops.import_scene.gltf(filepath=str(OUT/('village_crop_'+crop+'_near.glb')))
        roots=[o for o in bpy.context.scene.objects if o not in before and o.parent is None]
        assert len(roots)==1
        root=roots[0]
        for i,(x,z)in enumerate(centres):
            obj=root if i==0 else root.copy()
            if i:bpy.context.collection.objects.link(obj)
            obj.location=(x,-z,0)
    bpy.ops.mesh.primitive_plane_add(size=60,location=(3,-2,-.007))
    mat=bpy.data.materials.new('Preview only matte soil');mat.use_nodes=True
    b=mat.node_tree.nodes.get('Principled BSDF');b.inputs['Base Color'].default_value=(.072,.045,.021,1);b.inputs['Roughness'].default_value=.94
    noise=mat.node_tree.nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=18
    bump=mat.node_tree.nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.35;bump.inputs['Distance'].default_value=.025
    mat.node_tree.links.new(noise.outputs['Fac'],bump.inputs['Height']);mat.node_tree.links.new(bump.outputs['Normal'],b.inputs['Normal'])
    bpy.context.object.data.materials.append(mat)
    world=bpy.data.worlds.new('Crop QA soft daylight');world.use_nodes=True
    world.node_tree.nodes['Background'].inputs[0].default_value=(.62,.73,.91,1)
    world.node_tree.nodes['Background'].inputs[1].default_value=.6;bpy.context.scene.world=world
    bpy.ops.object.light_add(type='SUN',location=(0,-6,10));sun=bpy.context.object
    sun.rotation_euler=(.45,-.4,-.7);sun.data.energy=2.1;sun.data.angle=.12;sun.data.color=(1,.87,.65)
    bpy.ops.object.camera_add(location=(15,17,11));camera=bpy.context.object
    camera.rotation_euler=(Vector((3.5,-1.7,.7))-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO';camera.data.ortho_scale=23
    scene=bpy.context.scene;scene.camera=camera;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=20;scene.cycles.use_denoising=False
    scene.render.resolution_x=1500;scene.render.resolution_y=850;scene.render.resolution_percentage=100
    scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG'
    scene.render.filepath=str(SOURCE/'cooked-crop-preview.png');bpy.ops.render.render(write_still=True)
    print('CPU preview of actual cooked GLBs:',scene.render.filepath,flush=True)

if __name__=='__main__':
    if '--preview'in sys.argv:preview()
    elif '--validate'in sys.argv:validate()
    else:build()
