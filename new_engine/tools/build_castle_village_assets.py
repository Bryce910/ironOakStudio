#!/usr/bin/env python3
"""Author bounded, textured castle-village GLBs for Valwood's native renderer.

Python --prepare downloads/tiers CC0 PBR channels into a new source folder.
Blender --background --threads 2 --python this_file cooks the kit (buildings first).
Python --validate audits the actual GLBs and shared-image memory budget.
Powered by Poly Haven. Earlier source and cooked assets are read-only inputs.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import math
import os
import shutil
import struct
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/polyhaven/castle_village'
OUT=ROOT/'projects/valwood_world/models'
sys.path.insert(0,str(ROOT/'tools'))
from build_cinematic_assets import fetch,sha
from build_starting_area_assets import glb

KIT=[('village_house_a',10,12,12),('village_house_b',12,14,14),
     ('village_inn',18,14,16),('village_smithy',12,10,10),('village_keep',36,28,35),
     ('village_stall_red',4,3,3.3),('village_stall_blue',4,3,3.3),
     ('village_stall_gold',4,3,3.3),('village_well',3,3,3.8),
     ('village_banner',1.6,.5,4.8),('village_cart',3.2,1.8,2)]


COLORS={'cloth_red':(.55,.065,.045,1),'cloth_blue':(.07,.16,.38,1),'cloth_gold':(.7,.43,.09,1),'cloth_cream':(.8,.74,.56,1),
        'dark':(.055,.05,.035,1),'metal':(.09,.105,.115,1),'produce_red':(.46,.085,.035,1),'produce_gold':(.66,.38,.07,1),
        'shutter':(.18,.28,.25,1)}

def fix_material_factors(path):
    # Blender 4's glTF exporter may ignore a MixRGB multiply constant. Preserve
    # one shared photographic image and explicitly write its standard glTF tint.
    data,binary=glb(path)
    for material in data['materials']:
        key=material.get('name','').split('.')[0]
        if key in COLORS:material.setdefault('pbrMetallicRoughness',{})['baseColorFactor']=list(COLORS[key])
    encoded=json.dumps(data,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
    binary+=b'\0'*((-len(binary))%4)
    path.write_bytes(struct.pack('<III',0x46546c67,2,12+8+len(encoded)+8+len(binary))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary)


def prepare():
    from PIL import Image
    SOURCE.mkdir(parents=True,exist_ok=True)
    for asset in ['roof_slates_02','brown_planks_08']:
        folder=SOURCE/asset;folder.mkdir(exist_ok=True)
        fetch('https://api.polyhaven.com/files/'+asset,folder/'metadata.json')
        metadata=json.loads((folder/'metadata.json').read_text())
        for kind in ['diff','nor_gl','arm']:
            key='Diffuse' if kind=='diff' and 'diff' not in metadata else kind
            record=metadata[key]['2k' if kind=='diff' else '1k']['jpg']
            fetch(record['url'],folder/(kind+'.jpg'),record)
    materials={}
    for name,index,base in [('stone',0,2048),('trim',1,1024),('plaster',2,1024)]:
        folder=ROOT/'assets/polyhaven/starting_area/modular_fort_01'
        materials[name]={}
        for kind,limit in [('base',base),('normal',base//2),('arm',base//4)]:
            src=folder/f'runtime_{index}_{kind}.png';dst=SOURCE/f'{name}_{kind}.png'
            image=Image.open(src)
            if max(image.size)<=limit:shutil.copyfile(src,dst)
            else:
                image.thumbnail((limit,limit),Image.Resampling.LANCZOS);image.save(dst,optimize=True)
            materials[name][kind]=dst.name
    for name,asset,base in [('wood','brown_planks_08',1024),('roof','roof_slates_02',2048)]:
        materials[name]={}
        for kind,source_kind,limit in [('base','diff',base),('normal','nor_gl',base//2),('arm','arm',base//4)]:
            dst=SOURCE/f'{name}_{kind}.png';image=Image.open(SOURCE/asset/(source_kind+'.jpg')).convert('RGB')
            image.thumbnail((limit,limit),Image.Resampling.LANCZOS);image.save(dst,optimize=True);materials[name][kind]=dst.name
    # Authored coarse woven fabric: all colored cloth shares one image. Geometry
    # carries seams, sag and scallops; this texture supplies close surface grain.
    import numpy as np
    y,x=np.mgrid[:512,:512];weave=(((x%8)<3).astype(float)+((y%8)<3).astype(float))*10
    fibers=(np.sin(x*1.77+y*.31)+np.sin(y*2.31-x*.17))*2
    image=np.uint8(np.clip(205+weave+fibers,0,255));Image.fromarray(np.repeat(image[:,:,None],3,axis=2)).save(SOURCE/'cloth_base.png')
    materials['cloth']={'base':'cloth_base.png'}
    (SOURCE/'materials.json').write_text(json.dumps(materials,indent=2)+'\n')
    provenance=[]
    for file in sorted(SOURCE.rglob('*')):
        if file.is_file() and file.name!='provenance.json':
            provenance.append(dict(path=str(file.relative_to(SOURCE)),bytes=file.stat().st_size,sha256=sha(file),
                source=('https://polyhaven.com/a/'+file.parent.name if file.parent.name in ['roof_slates_02','brown_planks_08'] else
                        ('authored woven cloth' if file.name=='cloth_base.png' else 'derived from retained CC0 Poly Haven sources')),
                license='API reference' if file.name=='metadata.json' else 'CC0-1.0'))
    (SOURCE/'provenance.json').write_text(json.dumps(dict(license_url='https://polyhaven.com/license',
        api_terms='https://github.com/Poly-Haven/Public-API',credit='Powered by Poly Haven',files=provenance,
        prior_inputs=[dict(path=p,sha256=sha(ROOT/p)) for p in ['projects/valwood_world/models/starting_gate.glb',
            'projects/valwood_world/models/starting_tower.glb']]),indent=2)+'\n')
    print('Prepared village materials',flush=True)


class Builder:
    """Y-up authoring; export converter alone handles Blender's Z-up basis."""
    def __init__(self,lod):self.lod=lod;self.geometry={};self.collisions=[];self.openings=[]
    def face(self,mat,points):
        vertices,faces=self.geometry.setdefault(mat,([],[]));start=len(vertices)
        vertices.extend(points);faces.append(tuple(range(start,start+len(points))))
    def box(self,mat,x0,x1,y0,y1,z0,z1,collision=False):
        if min(x1-x0,y1-y0,z1-z0)<=0:return
        p=[(x,y,z) for y in [y0,y1] for z in [z0,z1] for x in [x0,x1]]
        for ids in [(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]:self.face(mat,[p[i] for i in reversed(ids)])
        if collision:self.collisions.append([x0,x1,z0,z1,y0,y1])
    def beam(self,mat,a,b,width=.16):
        import numpy as np
        a=np.asarray(a);b=np.asarray(b);axis=b-a;axis/=np.linalg.norm(axis)
        up=np.asarray([0.,1.,0.]) if abs(axis[1])<.9 else np.asarray([1.,0.,0.])
        u=np.cross(axis,up);u/=np.linalg.norm(u);v=np.cross(axis,u);u*=width*.5;v*=width*.5
        p=[tuple(center+du*u+dv*v) for center in [a,b] for du,dv in [(-1,-1),(1,-1),(1,1),(-1,1)]]
        for ids in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:self.face(mat,[p[i] for i in ids])
    def cylinder(self,mat,cx,cz,y0,y1,radius,segments=20,inner=0):
        for i in range(segments):
            a,b=i*math.tau/segments,(i+1)*math.tau/segments
            p=lambda r,y,t:(cx+r*math.cos(t),y,cz+r*math.sin(t))
            self.face(mat,[p(radius,y0,a),p(radius,y1,a),p(radius,y1,b),p(radius,y0,b)])
            if inner:
                self.face(mat,[p(inner,y0,b),p(inner,y1,b),p(inner,y1,a),p(inner,y0,a)])
                self.face(mat,[p(inner,y1,b),p(radius,y1,b),p(radius,y1,a),p(inner,y1,a)])
            else:self.face(mat,[(cx,y1,cz),p(radius,y1,b),p(radius,y1,a)])
    def cone(self,mat,cx,cz,y0,y1,radius,segments=16):
        for i in range(segments):
            a,b=i*math.tau/segments,(i+1)*math.tau/segments
            self.face(mat,[(cx+radius*math.cos(a),y0,cz+radius*math.sin(a)),(cx,y1,cz),(cx+radius*math.cos(b),y0,cz+radius*math.sin(b))])
    def roof(self,w,d,eave,ridge,center=(0,0),mat='roof'):
        x,z=center;x0,x1=x-w/2,x+w/2;z0,z1=z-d/2,z+d/2
        self.face(mat,list(reversed([(x0,eave,z0),(x,ridge,z0),(x,ridge,z1),(x0,eave,z1)])))
        self.face(mat,list(reversed([(x,ridge,z0),(x1,eave,z0),(x1,eave,z1),(x,ridge,z1)])))
        self.face('wood',list(reversed([(x0,eave-.12,z1),(x,ridge-.12,z1),(x,ridge-.12,z0),(x0,eave-.12,z0)])))
        self.face('wood',list(reversed([(x,ridge-.12,z1),(x1,eave-.12,z1),(x1,eave-.12,z0),(x,ridge-.12,z0)])))
        for end in [z0,z1]:
            self.beam('wood',(x0,eave-.12,end),(x,ridge,end),.22);self.beam('wood',(x,ridge,end),(x1,eave-.12,end),.22)
        self.beam('roof',(x,ridge+.03,z0),(x,ridge+.03,z1),.18)
        # Real overlapping course lips supply edge parallax at walking range.
        for side in [-1,1]:
            for row in range(1,13 if self.lod=='near' else 6):
                t=row/(13 if self.lod=='near' else 6);xx=x+side*w*.5*t;yy=ridge+(eave-ridge)*t+.016
                self.beam(mat,(xx,yy,z0+.03),(xx,yy,z1-.03),.045 if self.lod=='near' else .025)
    def window(self,x,y,z,side='front',width=1.25,height=1.6):
        # Deep dark casement, timber lintel/sill, shutters and visible muntins.
        def rect(mat,dx0,dx1,dy0,dy1,d0,d1):
            if side=='front':self.box(mat,x+dx0,x+dx1,y+dy0,y+dy1,z+d0,z+d1)
            else:self.box(mat,x+d0,x+d1,y+dy0,y+dy1,z+dx0,z+dx1)
        rect('dark',-width/2,width/2,0,height,-.035,.035)
        for xx in [-width/2-.09,width/2+.09]:rect('wood',xx-.08,xx+.08,-.12,height+.12,-.09,.09)
        for yy in [-.1,height+.1]:rect('wood',-width/2-.2,width/2+.2,yy-.08,yy+.08,-.13,.13)
        if self.lod=='near':
            rect('wood',-.035,.035,0,height,-.1,.1);rect('wood',-width/2,width/2,height*.53-.035,height*.53+.035,-.1,.1)
            for sign in [-1,1]:
                c=sign*(width*.5+.43);rect('shutter',c-.25,c+.25,0,height,-.02,.10)
                for yy in [.18,height-.18]:rect('metal',c-.23,c+.23,yy-.025,yy+.025,-.04,.13)
    def shell(self,w,d,top,doorw,doorh,material='plaster',thickness=.25):
        x0,x1=-w/2,w/2;z0,z1=-d/2,d/2
        self.box(material,x0,x1,0,top,z1-thickness,z1,True)
        self.box(material,x0,x0+thickness,0,top,z0,z1,True)
        self.box(material,x1-thickness,x1,0,top,z0,z1,True)
        self.box(material,x0,-doorw/2,0,top,z0,z0+thickness,True)
        self.box(material,doorw/2,x1,0,top,z0,z0+thickness,True)
        self.box(material,-doorw/2,doorw/2,doorh,top,z0,z0+thickness,True)
        self.box('wood',x0+thickness,x1-thickness,.01,.02,z0,z1-thickness)
        self.openings.append(dict(face='front',axis='-Z',center_x=0,plane_z=z0,width=doorw,height=doorh,sill_y=.02))
        # Recessed jamb/lintel framing leaves the declared void intact.
        for sign in [-1,1]:self.box('wood',sign*doorw/2-(.18 if sign<0 else 0),sign*doorw/2+(.18 if sign>0 else 0),0,doorh+.2,z0-.15,z0+.1)
        self.box('wood',-doorw/2-.18,doorw/2+.18,doorh,doorh+.2,z0-.15,z0+.1)
        for xx in [x0+.08,x1-.08]:
            for zz in [z0-.06,z1+.06]:self.box('wood',xx-.12,xx+.12,0,top+.04,zz-.12,zz+.12)
        for yy in [1.0,top*.49,top-.12]:
            if yy<doorh:
                for a,b in [(x0-.04,-doorw/2),(doorw/2,x1+.04)]:self.box('wood',a,b,yy,yy+.18,z0-.13,z0+.04)
            else:self.box('wood',x0-.04,x1+.04,yy,yy+.18,z0-.13,z0+.04)
            self.box('wood',x0-.04,x1+.04,yy,yy+.18,z1-.04,z1+.13)
        return x0,x1,z0,z1


def house(builder,name,w,d,height):
    keep=name=='village_keep';smith=name=='village_smithy';inn=name=='village_inn'
    bodyw,bodyd=(24.,18.) if keep else (w-.9,d-.9)
    eave=22. if keep else height*.63
    doorw,doorh=(4.6,6.) if keep else ((3.,3.2) if smith else ((2.4,3.) if inn else (1.8,2.6)))
    x0,x1,z0,z1=builder.shell(bodyw,bodyd,eave,doorw,doorh,'stone' if keep or smith else 'plaster',.4 if keep else .25)
    if not keep:
        # Foundation course is interrupted at the entrance, never a plinth.
        for a,b in [(x0,-doorw/2),(doorw/2,x1)]:builder.box('stone',a,b,0,.75,z0-.02,z0+.28)
        builder.box('stone',x0-.02,x1+.02,0,.75,z1-.26,z1+.02)
        for xx in [x0,x1-.24]:builder.box('stone',xx,xx+.24,0,.75,z0,z1)
    ceiling=7. if keep else 3.45
    builder.box('wood',x0+.25,x1-.25,ceiling,ceiling+.15,z0+.25,z1-.25)
    ridge=30. if keep else height-.35
    builder.roof(w-.25 if not keep else 25.,d-.25 if not keep else 19.,eave,ridge)
    for end in [z0,z1]:
        points=[(x0,eave,end),(0,ridge-.1,end),(x1,eave,end)]
        builder.face('plaster' if not keep else 'stone',points if end==z0 else list(reversed(points)))
    columns=5 if inn or keep else 3
    for i in range(columns):
        xx=x0+(i+1)*bodyw/(columns+1)
        for yy in ([2.1,5.2,9.2,13.5,17.] if keep else [1.55,eave*.60]):
            if abs(xx)<doorw/2+1 and yy<doorh:continue
            builder.window(xx,yy,z0-.06,width=1.3 if not keep else 1.1,height=1.45 if not keep else 1.8)
        if builder.lod=='near' and not keep:
            for zz in [z0-.14,z1+.14]:
                builder.beam('wood',(xx-.9,3.5,zz),(xx+.8,eave-.18,zz),.16)
    for sign in [-1,1]:
        for zz in [-bodyd*.25,bodyd*.25]:builder.window(sign*(bodyw/2+.06),2.,zz,'side',1.25,1.6)
        # Continuous exposed side sill and fascia protect the silhouette.
        builder.box('wood',sign*bodyw/2-.1,sign*bodyw/2+.1,eave-.2,eave+.05,z0,z1)
    if not keep:
        chimney_x=bodyw*.28;chimney_z=bodyd*.23
        builder.box('stone',chimney_x-.45,chimney_x+.45,eave*.75,height-.08,chimney_z-.45,chimney_z+.45)
        builder.box('trim',chimney_x-.53,chimney_x+.53,height-.2,height,chimney_z-.53,chimney_z+.53)
        builder.box('dark',chimney_x-.32,chimney_x+.32,height-.015,height,chimney_z-.32,chimney_z+.32)
        if builder.lod=='near' and not smith:
            # Front roof dormer, physically inset into the pitched roof.
            dx=-bodyw*.24;dz=-bodyd*.12;low=eave+(ridge-eave)*.52
            builder.box('plaster',dx-.85,dx+.85,low,low+1.3,dz-.7,dz+.7)
            builder.roof(2.,1.8,low+1.25,low+2.2,(dx,dz));builder.window(dx,low+.15,dz-.75,width=.78,height=.9)
        # A table gives the ground-floor interior human scale while leaving
        # the entrance/center clear; its collision is explicit.
        builder.box('wood',x1-2.8,x1-.8,.7,.85,z1-2.1,z1-.8,True)
        for xx in [x1-2.65,x1-.95]:
            for zz in [z1-1.95,z1-.95]:builder.box('wood',xx-.07,xx+.07,.02,.7,zz-.07,zz+.07)
    else:
        for xx in [-7,7]:
            for zz in [-4,4]:builder.box('stone',xx-.4,xx+.4,.02,7.,zz-.4,zz+.4,True)
        for xx in [-13.8,13.8]:
            for zz in [-9.8,9.8]:
                builder.cylinder('stone',xx,zz,0,26.5,3.7,24 if builder.lod=='near' else 16)
                builder.collisions.append([xx-3.7,xx+3.7,zz-3.7,zz+3.7,0,26.5])
                builder.cone('roof',xx,zz,27.0,35.,4.2,20 if builder.lod=='near' else 12)
                for yy in [1.2,8.,16.,24.5,26.3]:builder.cylinder('trim',xx,zz,yy,yy+.25,3.82,24 if builder.lod=='near' else 16)
                for yy in [5.,12.,19.]:builder.window(xx,yy,zz-3.72,width=.55,height=1.7)


def stall(builder,name):
    color=name.rsplit('_',1)[1];cloth='cloth_'+color
    for xx in [-1.75,1.75]:
        for zz in [-1.18,1.18]:builder.box('wood',xx-.065,xx+.065,0,2.75,zz-.065,zz+.065,True)
    builder.box('wood',-1.7,1.7,.82,.96,-.9,.9,True)
    for zz in [-1.15,1.15]:builder.beam('wood',(-1.8,2.65,zz),(1.8,2.65,zz),.15)
    for i in range(16):
        a,b=-2+i*.25,-2+(i+1)*.25
        ya,yb=2.7+.6*math.sin((a+2)*math.pi/4),2.7+.6*math.sin((b+2)*math.pi/4)
        mat=cloth if i%4<3 else 'cloth_cream'
        builder.face(mat,[(a,ya,1.5),(b,yb,1.5),(b,yb,-1.5),(a,ya,-1.5)])
        for zz in [-1.5,1.5]:
            points=[(a,ya,zz),(a,ya-.20,zz),(a+.125,(ya+yb)*.5-.28,zz),(b,yb-.20,zz),(b,yb,zz)]
            builder.face(mat,list(reversed(points)) if zz<0 else points)
    for index,xx in enumerate([-1.15,0,1.15]):
        builder.box('wood',xx-.46,xx+.46,.96,1.08,-.63,.63)
        for side in [-1,1]:builder.box('wood',xx-.46,xx+.46,1.08,1.3,side*.63-.035,side*.63+.035)
        # Products are independently authored measured model placements. The
        # former bright cone placeholders overwhelmed the purchased scenery.


def prop(builder,name):
    if name=='village_well':
        builder.cylinder('stone',0,0,0,1.1,1.15,28 if builder.lod=='near' else 14,.86)
        builder.cylinder('trim',0,0,1.1,1.25,1.25,28 if builder.lod=='near' else 14,.84)
        builder.cylinder('dark',0,0,.02,.03,.85,20)
        for xx in [-1.15,1.15]:builder.box('wood',xx-.12,xx+.12,0,2.8,-.12,.12)
        builder.beam('wood',(-1.23,2.1,0),(1.23,2.1,0),.18)
        builder.beam('metal',(0,1.0,0),(0,2.12,0),.025)
        builder.roof(2.7,2.3,2.7,3.7)
        builder.collisions.append([-1.25,1.25,-1.25,1.25,0,1.25])
    elif name=='village_banner':
        builder.box('wood',-.8,-.64,0,4.8,-.08,.08)
        builder.beam('metal',(-.8,4.55,0),(.8,4.55,0),.08)
        for i in range(10):
            a,b=-.62+i*.14,-.62+(i+1)*.14
            za,zb=.11*math.sin(i*.7),.11*math.sin((i+1)*.7)
            builder.face('cloth_blue',[(a,4.5,za),(b,4.5,zb),(b,2.25,zb),(a,2.25,za)])
        # Raised gold heraldic chevron; geometric, visible at a distance.
        builder.face('produce_gold',[(-.39,3.05,-.15),(.08,3.83,-.15),(.55,3.05,-.15),(.35,3.05,-.15),(.08,3.5,-.15),(-.19,3.05,-.15)])
    elif name=='village_cart':
        builder.box('wood',-1.3,1.3,.64,.78,-.62,.62)
        for zz in [-.65,.65]:
            for row in range(3):builder.box('wood',-1.3,1.3,.82+row*.22,1.+row*.22,zz-.035,zz+.035)
        for xx in [-.95,.95]:
            for zz in [-.82,.82]:
                # Wheel rims in localXY, actual spokes and iron tires.
                for i in range(16):
                    a,b=i*math.tau/16,(i+1)*math.tau/16
                    builder.beam('metal',(xx+.55*math.cos(a),.60+.55*math.sin(a),zz),(xx+.55*math.cos(b),.60+.55*math.sin(b),zz),.10)
                    if i%2==0:builder.beam('wood',(xx,.60,zz),(xx+.50*math.cos(a),.60+.50*math.sin(a),zz),.06)
        builder.box('wood',-1.6,1.6,.44,.55,-.3,-.19)
        for xx in [-.65,.45]:
            builder.box('wood',xx-.43,xx+.43,.78,1.7,-.45,.45)
            for yy in [1.,1.5]:builder.box('metal',xx-.44,xx+.44,yy,yy+.035,-.46,.46)
        builder.collisions.append([-1.6,1.6,-.9,.9,0,1.75])


def cook(selected=None):
    import bpy
    import bmesh
    from mathutils import Matrix,Vector
    from bake_gltf_uv import bake_uv_transforms
    materials=json.loads((SOURCE/'materials.json').read_text());OUT.mkdir(exist_ok=True)
    families={k[0] for k in KIT};selected=set(selected or families)
    assert selected and selected<=families,('Unknown selected families',selected-families)
    previous=json.loads((OUT/'village_asset_manifest.json').read_text()) if (OUT/'village_asset_manifest.json').exists() else None
    if selected!=families:assert previous and len(previous['assets'])==len(KIT)*2,'A full source kit is required before a selective cook'
    entries=[entry for entry in previous['assets'] if entry['family'] not in selected] if previous else []
    preserved={entry['asset']:sha(OUT/(entry['asset']+'.glb')) for entry in entries}
    staging=tempfile.TemporaryDirectory(prefix='.village-kit-',dir=OUT);stage=Path(staging.name)
    def make_material(name):
        base=name.split('_')[0] if name.startswith('cloth_') else name
        base={'shutter':'wood','dark':'wood'}.get(base,base)
        color=COLORS.get(name,(1,1,1,1))
        mat=bpy.data.materials.new(name);mat.use_nodes=True;mat.use_backface_culling=False
        nodes=mat.node_tree.nodes;links=mat.node_tree.links;shader=nodes.get('Principled BSDF')
        shader.inputs['Base Color'].default_value=color;shader.inputs['Roughness'].default_value=.78;shader.inputs['Metallic'].default_value=.65 if name=='metal' else 0
        channels=materials.get(base,{})
        for kind,file in channels.items():
            tex=nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(SOURCE/file),check_existing=True)
            if kind!='base':tex.image.colorspace_settings.name='Non-Color'
            if kind=='base':
                if color!=(1,1,1,1):
                    multiply=nodes.new('ShaderNodeMixRGB');multiply.blend_type='MULTIPLY';multiply.inputs[0].default_value=1;multiply.inputs[2].default_value=color
                    links.new(tex.outputs['Color'],multiply.inputs[1]);links.new(multiply.outputs['Color'],shader.inputs['Base Color'])
                else:links.new(tex.outputs['Color'],shader.inputs['Base Color'])
            elif kind=='normal':
                normal=nodes.new('ShaderNodeNormalMap');links.new(tex.outputs['Color'],normal.inputs['Color']);links.new(normal.outputs['Normal'],shader.inputs['Normal'])
            else:
                separate=nodes.new('ShaderNodeSeparateColor');links.new(tex.outputs['Color'],separate.inputs['Color']);links.new(separate.outputs['Green'],shader.inputs['Roughness'])
        return mat
    def original_fort(kind,builder,material_map):
        # Reuse authentic fort crenellations and the arched gate surround;
        # custom shells supply building-scale interiors and clear entrances.
        source=ROOT/'projects/valwood_world/models'/('starting_tower.glb' if kind=='crown' else 'starting_gate.glb')
        existing=set(bpy.context.scene.objects);bpy.ops.import_scene.gltf(filepath=str(source))
        imported=[o for o in bpy.context.scene.objects if o not in existing and o.type=='MESH']
        for obj in imported:
            obj.data=obj.data.copy();obj.data.transform(obj.matrix_world);obj.matrix_world=Matrix.Identity(4)
            bm=bmesh.new();bm.from_mesh(obj.data)
            bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.00001,plane_co=(0,0,8.8 if kind=='crown' else .28),plane_no=(0,0,1),clear_inner=True)
            bm.to_mesh(obj.data);bm.free()
            if builder.lod=='mid':
                bpy.context.view_layer.objects.active=obj;mod=obj.modifiers.new('Bounded source detail','DECIMATE');mod.ratio=.5;bpy.ops.object.modifier_apply(modifier=mod.name)
            for index,mat in enumerate(obj.data.materials):
                name='plaster' if 'plaster' in mat.name else ('trim' if 'trim' in mat.name else 'stone');obj.data.materials[index]=material_map.setdefault(name,make_material(name))
            if kind=='crown':
                prototype=obj.data.copy();bpy.data.objects.remove(obj,do_unlink=True)
                for xx in [-13.8,13.8]:
                    for zz in [-9.8,9.8]:
                        copy=bpy.data.objects.new('original_fort_crenellations',prototype.copy());bpy.context.collection.objects.link(copy)
                        copy.data.transform(Matrix.Translation(Vector([xx,-zz,26.2-8.8*.47]))@Matrix.Diagonal(Vector([.47,.47,.47,1])))
            else:obj.data.transform(Matrix.Translation(Vector([0,9.6,-.28*1.65]))@Matrix.Diagonal(Vector([4.6/3.50133,.4,1.65,1])))
    for name,w,d,height in KIT:
        if name not in selected:continue
        for lod in ['near','mid']:
            bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.scene.render.threads_mode='FIXED';bpy.context.scene.render.threads=2
            builder=Builder(lod)
            if name in [k[0] for k in KIT[:5]]:house(builder,name,w,d,height)
            elif name.startswith('village_stall'):stall(builder,name)
            else:prop(builder,name)
            material_map={}
            for matname,(vertices,faces) in builder.geometry.items():
                mesh=bpy.data.meshes.new(matname);mesh.from_pydata([(x,-z,y) for x,y,z in vertices],[],faces);mesh.update()
                # Recalculate outward normals from authored solids, preserving
                # two-sided cloth/roof interiors without blended materials.
                uv=mesh.uv_layers.new(name='UVMap');tile={'stone':2.4,'trim':2.,'plaster':3.,'roof':3.,'wood':1.6}.get(matname,1.)
                for polygon in mesh.polygons:
                    normal=polygon.normal;axis=max(range(3),key=lambda k:abs(normal[k]))
                    for li in polygon.loop_indices:
                        p=mesh.vertices[mesh.loops[li].vertex_index].co
                        coords=(p.x,p.y) if axis==2 else ((p.x,p.z) if axis==1 else (p.y,p.z))
                        uv.data[li].uv=(coords[0]/tile,coords[1]/tile)
                obj=bpy.data.objects.new(matname,mesh);bpy.context.collection.objects.link(obj)
                obj.data.materials.append(material_map.setdefault(matname,make_material(matname)))
            if name=='village_keep':
                original_fort('crown',builder,material_map);original_fort('gate',builder,material_map)
            # Merge each material into one mesh: predictable draw counts even
            # though authoring contains hundreds of wall/trim/roof components.
            all_meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
            for obj in all_meshes:obj.select_set(True)
            bpy.context.view_layer.objects.active=all_meshes[0];bpy.ops.object.join()
            obj=bpy.context.object;obj.name=name
            points=[v.co for v in obj.data.vertices]
            low=[min(p[k] for p in points) for k in range(3)];high=[max(p[k] for p in points) for k in range(3)]
            sx,sz,sy=-(low[0]+high[0])*.5,(low[1]+high[1])*.5,-low[2]
            obj.data.transform(Matrix.Translation(Vector([sx,-sz,sy])))
            for box in builder.collisions:
                box[0]+=sx;box[1]+=sx;box[2]+=sz;box[3]+=sz;box[4]+=sy;box[5]+=sy
            for opening in builder.openings:
                opening['center_x']+=sx;opening['plane_z']+=sz;opening['sill_y']+=sy
            # Degenerate cleanup is restricted to numerical zero-area faces.
            bm=bmesh.new();bm.from_mesh(obj.data);bad=[f for f in bm.faces if f.calc_area()<1.e-11]
            if bad:bmesh.ops.delete(bm,geom=bad,context='FACES')
            bm.to_mesh(obj.data);bm.free();obj.data.update()
            for material in obj.data.materials:
                if material.node_tree:
                    for node in material.node_tree.nodes:
                        if node.type=='TEX_IMAGE' and node.image:node.image.pack()
            stem=name+('_mid' if lod=='mid' else '');path=stage/(stem+'.glb')
            bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',export_yup=True,export_animations=False,export_cameras=False,export_lights=False,export_image_format='AUTO')
            bake_uv_transforms(path);fix_material_factors(path)
            entry=dict(asset=stem,family=name,lod=lod,bytes=path.stat().st_size,sha256=sha(path),triangles=sum(len(p.vertices)-2 for p in obj.data.polygons),
                source='authored architecture plus CC0 Poly Haven fort geometry/PBR',license='CC0-1.0',collision_boxes=builder.collisions,door_openings=builder.openings,
                front_axis='-Z',floor_y=.02,lod_split_m=180 if name=='village_keep' else (90 if name in [k[0] for k in KIT[:4]] else 50),
                maximum_design_dimensions=[w,height,d],materials=len([m for m in obj.data.materials if m]))
            entries.append(entry)
            print('COOKED',stem,entry['triangles'],'triangles',flush=True)
    assert len(entries)==len(KIT)*2
    for asset,digest in preserved.items():assert sha(OUT/(asset+'.glb'))==digest,'Unselected source model changed'
    order={name:i for i,(name,*_) in enumerate(KIT)}
    entries.sort(key=lambda e:(order[e['family']],e['lod']!='near'))
    manifest=dict(schema_version=1,assets=entries,credit='Powered by Poly Haven',
        license_url='https://polyhaven.com/license',source_provenance='assets/polyhaven/castle_village/provenance.json')
    staged_manifest=stage/'village_asset_manifest.json';staged_manifest.write_text(json.dumps(manifest,indent=2)+'\n')
    for entry in entries:
        if entry['family'] not in selected:os.symlink(OUT/(entry['asset']+'.glb'),stage/(entry['asset']+'.glb'))
    # Audit actual mesh buffers/PBR/images/doorways for the complete staged kit
    # before publishing any selected replacement. Unselected files are links
    # to their unchanged originals and are never written by validation.
    validate(stage)
    for entry in entries:
        if entry['family'] in selected:os.replace(stage/(entry['asset']+'.glb'),OUT/(entry['asset']+'.glb'))
    os.replace(staged_manifest,OUT/'village_asset_manifest.json');staging.cleanup()


def door_clear(triangles,opening,z_start):
    import numpy as np
    triangles=np.asarray(triangles);e1=triangles[:,1]-triangles[:,0];e2=triangles[:,2]-triangles[:,0]
    direction=np.asarray([0.,0.,1.]);p=np.cross(direction,e2);det=np.einsum('ij,ij->i',e1,p)
    active=np.abs(det)>1.e-9;inverse=np.zeros_like(det);inverse[active]=1/det[active]
    width=opening['width'];height=opening['height'];cx=opening['center_x'];end=opening['plane_z']+1
    for x in [cx-width/2+.16,cx,cx+width/2-.16]:
        for y in [.15,.6,1.,1.75,min(2.4,height-.15)]:
            offset=np.asarray([x,y,z_start])-triangles[:,0];u=np.einsum('ij,ij->i',offset,p)*inverse
            q=np.cross(offset,e1);v=q[:,2]*inverse;t=np.einsum('ij,ij->i',e2,q)*inverse
            if np.any(active&(u>=-1.e-8)&(v>=-1.e-8)&(u+v<=1+1.e-8)&(t>=0)&(t<=end-z_start)):return False
    return True


def validate(directory=None):
    import numpy as np
    from PIL import Image
    from build_starting_area import glb_bounds
    directory=Path(directory) if directory else OUT
    manifest=json.loads((directory/'village_asset_manifest.json').read_text());assert len(manifest['assets'])==len(KIT)*2
    unique={};families={};resident_keep_images=set()
    # The hero keep reuses the already-resident purchased masonry/timber maps.
    # Credit only exact encoded channels AND sampler policies from the actual
    # donor GLB, rather than raising the private prototype-kit image budget.
    donor=OUT/'purchased_village_house_a_near.glb'
    donor_data,donor_binary=glb(donor)
    def channel_identity(data,binary,material,key):
        parent=material.get('pbrMetallicRoughness',{}) if key in ['baseColorTexture','metallicRoughnessTexture'] else material
        record=parent[key];texture=data['textures'][record['index']]
        image=data['images'][texture['source']];view=data['bufferViews'][image['bufferView']]
        encoded=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
        sampler=data.get('samplers',[{}])[texture.get('sampler',0)]
        return hashlib.sha256(encoded).hexdigest(),json.dumps(sampler,sort_keys=True),record.get('texCoord',0)
    donor_materials={m['name']:m for m in donor_data['materials']}
    for entry in manifest['assets']:
        path=directory/(entry['asset']+'.glb');data,binary=glb(path);assert sha(path)==entry['sha256'];count=0
        assert not data.get('skins') and not data.get('animations')
        assert all(not node.get('translation') and not node.get('rotation') and not node.get('scale') and not node.get('matrix') for node in data['nodes'])
        all_triangles=[];wall_checks=roof_checks=0
        def accessor(index):
            ac=data['accessors'][index];view=data['bufferViews'][ac['bufferView']]
            dimensions={'VEC2':2,'VEC3':3,'VEC4':4,'SCALAR':1}[ac['type']]
            dtype=np.dtype({5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[ac['componentType']])
            return np.ndarray((ac['count'],dimensions),dtype,binary,offset=view.get('byteOffset',0)+ac.get('byteOffset',0),strides=(view.get('byteStride',dimensions*dtype.itemsize),dtype.itemsize))
        for mesh in data['meshes']:
            for primitive in mesh['primitives']:
                assert primitive.get('mode',4)==4 and 'TEXCOORD_0' in primitive['attributes'];count+=data['accessors'][primitive['indices']]['count']//3
                for index in primitive['attributes'].values():
                    ac=data['accessors'][index];view=data['bufferViews'][ac['bufferView']];dimensions={'VEC2':2,'VEC3':3,'VEC4':4}[ac['type']]
                    values=np.ndarray((ac['count'],dimensions),'<f4',binary,offset=view.get('byteOffset',0)+ac.get('byteOffset',0),strides=(view.get('byteStride',dimensions*4),4))
                    assert np.isfinite(values).all()
                positions=accessor(primitive['attributes']['POSITION']);normals=accessor(primitive['attributes']['NORMAL'])
                indices=accessor(primitive['indices']).ravel().reshape(-1,3);assert indices.max()<len(positions)
                triangles=positions[indices];all_triangles.extend(triangles.tolist())
                cross=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]);area=np.linalg.norm(cross,axis=1)
                assert np.all(area>1.e-10),(entry['asset'],'degenerate triangles')
                assert np.max(np.abs(np.linalg.norm(normals,axis=1)-1))<.002
                geometric=cross/area[:,None];normal=normals[indices].mean(axis=1);center=triangles.mean(axis=1)
                material=data['materials'][primitive['material']]['name'].lower();name=entry['family'];design=entry['maximum_design_dimensions']
                if name in [k[0] for k in KIT[:5]]:
                    keep=name=='village_keep';w,d=(24.,18.) if keep else (design[0]-.9,design[2]-.9)
                    top=22. if keep else design[1]*.63
                    if material==('stone' if keep or name=='village_smithy' else 'plaster'):
                        for axis,bound,sign in [(2,-d/2,-1),(2,d/2,1),(0,-w/2,-1),(0,w/2,1)]:
                            mask=np.all(np.abs(triangles[:,:,axis]-bound)<.002,axis=1)&(center[:,1]<top*.9)
                            if np.any(mask):
                                assert np.all(normal[mask,axis]*sign>.99) and np.all(geometric[mask,axis]*sign>.99),(entry['asset'],'outward wall normal',axis,sign)
                                wall_checks+=int(mask.sum())
                    if material=='roof':
                        mask=(area>4)&(center[:,1]>top)
                        if np.any(mask):
                            assert np.all(normal[mask,1]>0) and np.all(geometric[mask,1]>0),(entry['asset'],'upward roof normal')
                            roof_checks+=int(mask.sum())
        assert count==entry['triangles'] and 0<count<=65000,(entry['asset'],count,entry['triangles'])
        assert len(data['materials'])<=16
        for material in data['materials']:
            assert material.get('alphaMode','OPAQUE')=='OPAQUE'
            key=material.get('name','').split('.')[0].lower();pbr=material['pbrMetallicRoughness']
            if key in ['stone','trim','plaster','wood','roof','shutter','dark']:
                assert 'baseColorTexture' in pbr and 'metallicRoughnessTexture' in pbr and 'normalTexture' in material,(entry['asset'],key)
            if key in COLORS:assert np.max(np.abs(np.asarray(pbr.get('baseColorFactor',[1,1,1,1]))-COLORS[key]))<1.e-6,(entry['asset'],key)
            if key.startswith('cloth_'):assert 'baseColorTexture' in pbr
            if entry.get('hero_recipe') and entry['family']=='village_keep' and key in ['stone','wood','keep_dark']:
                donor_name='Purchased_Tile_T_Tileable_Wall_Stone_01' if key=='stone' else 'Purchased_Source_Wood'
                for channel in ['baseColorTexture','normalTexture','metallicRoughnessTexture']:
                    actual=channel_identity(data,binary,material,channel)
                    assert actual==channel_identity(donor_data,donor_binary,donor_materials[donor_name],channel),(entry['asset'],'resident keep channel differs',key,channel)
                    resident_keep_images.add(actual[0])

        low,high=glb_bounds(path);assert abs(low[1])<.001
        if entry['door_openings']:
            assert wall_checks>=8 and roof_checks>=4,(entry['asset'],'normal audit missing exterior surfaces')
            for opening in entry['door_openings']:
                assert door_clear(all_triangles,opening,low[2]-1),(entry['asset'],'actual doorway blocked')
                cx=opening['center_x'];z=opening['plane_z']-.1;half=opening['width']*.5
                blocker=[[[cx-half,.9,z],[cx+half,.9,z],[cx+half,1.1,z]],[[cx-half,.9,z],[cx+half,1.1,z],[cx-half,1.1,z]]]
                assert not door_clear(all_triangles+blocker,opening,low[2]-1),'doorway negative control missed crossbar'
        validation=dict(outward_wall_triangles=wall_checks,upward_roof_triangles=roof_checks,doorways_clear=len(entry['door_openings']),crossbar_negative_control=bool(entry['door_openings']))
        if entry.get('hero_recipe'):validation={**entry.get('validation',{}),**validation}
        entry['validation']=validation
        entry['bounds_y_up']=[low,high];entry['draw_materials']=len(data['materials'])
        image_set=set()
        for image in data['images']:
            view=data['bufferViews'][image['bufferView']];encoded=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
            digest=hashlib.sha256(encoded).hexdigest();im=Image.open(io.BytesIO(encoded));w,h=im.size;memory=0;assert max(w,h)<=2048
            while True:
                memory+=w*h*4
                if w==h==1:break
                w,h=max(1,w//2),max(1,h//2)
            unique[digest]=memory;image_set.add(digest)
        families.setdefault(entry['family'],[]).append(image_set)
        print('VALIDATED',entry['asset'],count,'bounds',low,high,flush=True)
    for family,images in families.items():assert images[0]==images[1],family
    memory=sum(unique.values());credited=sum(unique[h] for h in resident_keep_images)
    private_memory=memory-credited
    assert private_memory<=128*1048576,('private prototype-kit images',private_memory/1048576)
    manifest['summary']=dict(glbs=len(manifest['assets']),families=len(KIT),encoded_glb_bytes=sum(e['bytes'] for e in manifest['assets']),
        unique_images=len(unique),rgba8_mip_bytes=memory,rgba8_mip_mib=memory/1048576,near_mid_shared=True,
        private_rgba8_mip_bytes=private_memory,private_rgba8_mip_mib=private_memory/1048576,private_budget_mib=128,
        resident_keep_credit=dict(donor=str(donor.relative_to(ROOT)),donor_sha256=sha(donor),
            exact_encoded_channel_hashes=sorted(resident_keep_images),images=len(resident_keep_images),
            rgba8_mip_bytes=credited,rgba8_mip_mib=credited/1048576,incremental_scene_image_mib=0))
    (directory/'village_asset_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('PASS village asset validation',json.dumps(manifest['summary']),flush=True)


if __name__=='__main__':
    if '--python' in sys.argv:
        parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--families',help='Comma-separated kit families; all other models remain byte-identical')
        args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
        cook(args.families.split(',') if args.families else None)
    else:
        parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare',action='store_true');parser.add_argument('--validate',action='store_true');args=parser.parse_args()
        if not(args.prepare or args.validate):parser.print_help();raise SystemExit(2)
        if args.prepare:prepare()
        if args.validate:validate()
