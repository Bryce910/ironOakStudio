#!/usr/bin/env python3
"""Authored castle courtyard kit with exact resident photographic image reuse.

No network, Blender or source edits. --cook publishes only the six new GLBs
and their separate manifest after auditing actual buffers and image policies.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,sys
import numpy as np
from build_village_keep_hero import Mesh as BaseMesh,GLBWriter,accessor,image_policies,triangles
from build_starting_area_assets import glb
from build_purchased_village import encode_glb
ROOT=Path(__file__).resolve().parents[1]
MODELS=ROOT/'projects/valwood_world/models'
OUT=ROOT/'assets/purchased/castle_expansion'
HOUSE=MODELS/'purchased_village_house_a_near.glb'
KNIGHT=MODELS/'village_character_knight_idle_00.glb'
MATERIALS={
    'Stone':('Purchased_Tile_T_Tileable_Wall_Stone_01',[.80,.83,.81,1],.98),
    'Wood':('Purchased_Source_Wood',[.72,.62,.45,1],.92),
    'DarkWood':('Purchased_Source_Wood',[.25,.22,.18,1],.98),
    'Cloth':('Purchased_Source_Wood',[.83,.70,.39,1],1),
    'Red':('Purchased_Source_Wood',[.64,.085,.035,1],.92),
    'Cream':('Purchased_Source_Wood',[.92,.86,.68,1],.98),
    'Metal':('Purchased_Source_Wood',[.19,.23,.24,1],.55),
    'Bronze':('Purchased_Source_Wood',[.45,.31,.10,1],.67)}
FAMILIES=['castle_monument','castle_training_dummy','castle_archery_target',
          'castle_weapon_rack','castle_yard_bench','castle_training_fence']

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

class Mesh(BaseMesh):
    def __init__(self):super().__init__('near');self.parts={name:[[],[],[],[]]for name in MATERIALS}
    def face(self,mat,points,hint=None,uv=None,normals=None):
        p=np.asarray(points,float)
        if len(p)<3:return
        if uv is None:
            normal=np.cross(p[1]-p[0],p[2]-p[0]);axis=int(np.argmax(np.abs(normal)))
            a,b=[(2,1),(0,2),(0,1)][axis];tile=1.3 if mat!='Stone'else 1.7
            uv=[[v[a]/tile,v[b]/tile]for v in p]
        super().face(mat,points,hint,uv,normals)
    def beam(self,mat,a,b,width=.08,depth=None):
        a,b=np.asarray(a,float),np.asarray(b,float);d=b-a;d/=np.linalg.norm(d)
        u=np.cross(d,[0,1,0]if abs(d[1])<.9 else[1,0,0]);u/=np.linalg.norm(u)
        v=np.cross(d,u);u*=width/2;v*=(depth or width)/2
        p=[c+du*u+dv*v for c in[a,b]for du,dv in[(-1,-1),(1,-1),(1,1),(-1,1)]]
        center=(a+b)/2
        for ids in[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:
            q=[p[i]for i in ids];self.face(mat,q,np.mean(q,axis=0)-center)
    def lathe(self,mat,profile,c=(0,0,0),segments=32):
        cx,cy,cz=c
        for (y,r),(yy,rr)in zip(profile,profile[1:]):
            for i in range(segments):
                a,b=math.tau*i/segments,math.tau*(i+1)/segments
                p=[(cx+r*math.cos(a),cy+y,cz+r*math.sin(a)),(cx+r*math.cos(b),cy+y,cz+r*math.sin(b)),
                   (cx+rr*math.cos(b),cy+yy,cz+rr*math.sin(b)),(cx+rr*math.cos(a),cy+yy,cz+rr*math.sin(a))]
                self.face(mat,p,[math.cos((a+b)/2),0,math.sin((a+b)/2)])
    def disk(self,mat,cx,cy,z,inner,outer,segments=48):
        for i in range(segments):
            a,b=math.tau*i/segments,math.tau*(i+1)/segments
            p=lambda r,t:(cx+r*math.cos(t),cy+r*math.sin(t),z)
            self.face(mat,[p(inner,a),p(outer,a),p(outer,b),p(inner,b)],[0,0,1])
    def sphere(self,mat,c,r,segments=24,rings=12):
        cx,cy,cz=c;rx,ry,rz=r
        for j in range(rings):
            a,b=-math.pi/2+math.pi*j/rings,-math.pi/2+math.pi*(j+1)/rings
            for i in range(segments):
                u,v=math.tau*i/segments,math.tau*(i+1)/segments
                points=[(cx+rx*math.cos(t)*math.cos(w),cy+ry*math.sin(t),cz+rz*math.cos(t)*math.sin(w))for t,w in[(a,u),(a,v),(b,v),(b,u)]]
                self.face(mat,points,np.mean(points,axis=0)-np.asarray(c))

def ornament(m,x,y,z,scale=1):
    # Raised eight-rayed crown crest and branching tree on a recessed plaque.
    for a in range(8):
        t=math.tau*a/8
        m.beam('Bronze',(x+.11*scale*math.sin(t),y+.11*scale*math.cos(t),z),
               (x+.34*scale*math.sin(t),y+.34*scale*math.cos(t),z),.034*scale)
    m.beam('Bronze',(x,y-.35*scale,z),(x,y+.18*scale,z),.055*scale)
    for sign in[-1,1]:
        for h in[-.08,.06,.18]:m.beam('Bronze',(x,y+h*scale,z),(x+sign*.22*scale,y+(h+.15)*scale,z),.035*scale)

def monument():
    m=Mesh()
    # Cut-stone stepped footing, chamfered cornices, engraved shield recess.
    for y0,y1,r in[(0,.16,2.75),(.16,.31,2.58),(.31,.42,2.40),(.42,1.66,2.10),(1.66,1.77,2.20),(1.77,1.91,2.30),(1.91,2.03,2.18)]:
        m.lathe('Stone',[(y0,r),(y1,r)],segments=8)
        m.lathe('Stone',[(y1,r),(y1,0)],segments=8)
    # Joint cuts and corner reliefs read at normal walking eye height.
    for x in[-1.25,0,1.25]:
        m.box('DarkWood',x-.012,x+.012,.44,1.63,2.085,2.105)
    m.box('DarkWood',-.65,.65,.75,1.48,2.108,2.132)
    m.box('Stone',-.72,.72,.70,.76,2.1,2.18);m.box('Stone',-.72,.72,1.48,1.54,2.1,2.18)
    for x in[-.69,.69]:m.box('Stone',x-.035,x+.035,.76,1.48,2.1,2.18)
    ornament(m,0,1.12,2.17,.9)
    # Real purchased Defender geometry, baked and rigid: no skin/animation load.
    d,b=glb(KNIGHT)
    for primitive in [p for mesh in d['meshes']for p in mesh['primitives']]:
        p=accessor(d,b,primitive['attributes']['POSITION']);n=accessor(d,b,primitive['attributes']['NORMAL'])
        ix=accessor(d,b,primitive['indices']).ravel();scale=2.65
        for face in ix.reshape(-1,3):
            q=p[face]*scale+np.asarray([0,2.03,0]);normals=n[face]
            m.face('Stone',q,normals.mean(axis=0),normals=normals.tolist())
    # Four carved shields on the pedestal give the monument a designed base.
    for sign in[-1,1]:
        for z in[-.9,.9]:
            m.box('Stone',sign*1.95-.08,sign*1.95+.08,.64,1.47,z-.33,z+.33)
    return m

def dummy():
    m=Mesh()
    for angle in[0,math.pi/2]:
        c,s=math.cos(angle),math.sin(angle);m.beam('Wood',[-.7*c,.075,-.7*s],[.7*c,.075,.7*s],.15,.13)
    m.beam('DarkWood',[0,.08,0],[0,1.98,0],.13)
    m.lathe('Cloth',[(.80,.15),(.87,.26),(1.20,.30),(1.48,.31),(1.56,.20)],segments=32)
    m.lathe('Cloth',[(1.56,.20),(1.59,0)],segments=32)
    m.sphere('Cloth',(0,1.80,0),(.20,.24,.19))
    for sign in[-1,1]:
        m.beam('Wood',(0,1.42,0),(sign*.80,1.35,0),.12)
        m.sphere('Cloth',(sign*.59,1.37,0),(.27,.13,.14),16,8)
        for j in range(5):m.beam('DarkWood',(sign*(.41+j*.08),1.29,.11),(sign*(.41+j*.08),1.47,.11),.012)
    # Real rope loops and lacing; no alpha cards or new cloth image.
    for y in[.9,1.12,1.42]:
        profile=[(y-.018,.272 if y<1 else .312),(y+.018,.272 if y<1 else .312)]
        m.lathe('DarkWood',profile,segments=32)
    for j in range(8):
        y=.86+j*.075;m.beam('DarkWood',(-.055,y,.30),(.055,y+.035,.30),.012)
    m.disk('Red',0,1.22,.319,0,.075,24);m.disk('Cream',0,1.22,.317,.08,.145,32)
    return m

def target():
    m=Mesh()
    for x in[-.47,.47]:
        m.beam('Wood',(x,.02,-.45),(x,2.02,.03),.115)
        m.beam('Wood',(x,.02,.48),(x,1.8,.03),.095)
    m.beam('Wood',(-.60,.48,-.33),(.60,.48,-.33),.10)
    # Segmented edge makes a substantial sewn straw target, with concentric
    # painted rings instead of z-fighting coplanar circle overlays.
    for i in range(64):
        a,b=math.tau*i/64,math.tau*(i+1)/64
        p=lambda z,t:(.69*math.cos(t),1.50+.69*math.sin(t),z)
        m.face('Cloth',[p(-.09,a),p(.14,a),p(.14,b),p(-.09,b)],[(math.cos(a+b)/2),math.sin((a+b)/2),0])
    for r0,r1,mat in[(0,.10,'Red'),(.10,.23,'Cream'),(.23,.36,'Red'),(.36,.49,'Cream'),(.49,.60,'Red'),(.60,.69,'Cloth')]:
        m.disk(mat,0,1.5,.142,r0,r1,64)
    # Three arrows at modest angles remain bounded within the source envelope.
    for x,y in[(-.07,1.52),(.16,1.39),(-.25,1.61)]:
        m.beam('Wood',(x,y,.14),(x+.045,y-.035,.67),.014)
        for dx in[-1,1]:m.face('Cream',[(x+.045,y-.035,.65),(x+.045+dx*.045,y-.035,.58),(x+.038,y-.026,.51)],[0,1,0])
    return m

def rack():
    m=Mesh()
    for x in[-1.28,1.28]:
        m.beam('Wood',(x,.04,0),(x,1.95,0),.16)
        m.beam('Wood',(x,.08,-.45),(x,.08,.45),.15)
    for y in[.47,1.49]:m.beam('Wood',(-1.33,y,0),(1.33,y,0),.15)
    for i,x in enumerate([-.98,-.59,-.20,.20,.59,.98]):
        m.beam('DarkWood',(x,.25,.13),(x,1.72+.10*(i%2),.13),.045)
        if i%2==0:
            m.box('Metal',x-.047,x+.047,.71,1.66,.102,.158)
            m.beam('Bronze',(x-.19,.70,.13),(x+.19,.70,.13),.055)
            m.sphere('Bronze',(x,.30,.13),(.064,.064,.064),12,6)
        else:
            m.box('Metal',x-.16,x+.16,1.56,1.78,.108,.152)
            m.beam('Metal',(x,1.76,.13),(x,1.94,.13),.035)
    for x in[-1.28,1.28]:
        for y in[.47,1.49]:m.sphere('Metal',(x,y,.09),(.035,.035,.025),12,6)
    return m

def bench():
    m=Mesh()
    for x in[-1.10,1.10]:
        m.box('Stone',x-.18,x+.18,0,.43,-.34,.34)
        m.beam('Wood',(x,.37,-.29),(x,1.10,-.35),.10)
    for z in[-.25,-.08,.09,.26]:m.box('Wood',-1.48,1.48,.44,.51,z-.075,z+.075)
    for y in[.80,1.05]:m.box('Wood',-1.45,1.45,y-.075,y+.075,-.38,-.30)
    for x in[-1.10,1.10]:
        for z in[-.25,.26]:m.sphere('Metal',(x,.516,z),(.025,.01,.025),10,5)
    return m

def fence():
    m=Mesh()
    for x in[-.80,.80]:
        m.box('Wood',x-.065,x+.065,0,1.08,-.065,.065)
        m.lathe('Wood',[(1.08,.094),(1.16,.025)],(x,0,0),8)
    for y in[.39,.79]:m.box('Wood',-.80,.80,y-.055,y+.055,-.045,.045)
    m.beam('DarkWood',(-.75,.30,0),(.75,.90,0),.065)
    return m

BUILDERS=dict(zip(FAMILIES,[monument,dummy,target,rack,bench,fence]))

def save(path,name,mesh):
    writer=GLBWriter();writer.doc['asset']['generator']='Valwood authored castle expansion 1'
    writer.doc['nodes'][0]['name']=name
    for mat,(source,factor,roughness)in MATERIALS.items():
        p,n,uv,ix=mesh.parts[mat]
        if not ix:continue
        index=len(writer.doc['materials']);writer.material(HOUSE,source,mat)
        material=writer.doc['materials'][-1];material['name']='Castle_'+mat
        material['pbrMetallicRoughness'].update(baseColorFactor=factor,metallicFactor=0,roughnessFactor=roughness)
        writer.doc['meshes'][0]['primitives'].append(dict(mode=4,material=index,
            attributes=dict(POSITION=writer.accessor(p,3),NORMAL=writer.accessor(n,3),TEXCOORD_0=writer.accessor(uv,2)),
            indices=writer.accessor(ix,1,5125)))
    writer.doc['buffers'][0]['byteLength']=len(writer.binary)
    path.write_bytes(encode_glb(writer.doc,bytes(writer.binary)))
    return writer.provenance

def audit(directory):
    donor=image_policies(HOUSE);report=[];sourceimages=set()
    for name in FAMILIES:
        path=directory/(name+'.glb');d,p,t=triangles(path)
        assert not d.get('skins')and not d.get('animations')
        assert len(t)<(18000 if name=='castle_monument'else 12000),(name,len(t))
        assert not(image_policies(path)-donor),(name,'New image/sampler policy')
        assert np.isfinite(p).all()and abs(float(p[:,1].min()))<1e-6
        if name=='castle_monument':
            # Every real source vertex (including the separate poleaxe mesh)
            # must survive the rigid enlargement; a generic soldier silhouette
            # or a body-only export is an invalid monument replacement.
            source,binary=glb(KNIGHT)
            expected=[]
            for mesh in source['meshes']:
                for primitive in mesh['primitives']:
                    expected.extend(accessor(source,binary,primitive['attributes']['POSITION'])*2.65+np.asarray([0,2.03,0]))
            actual={tuple(v)for v in np.round(p.astype(float),5)}
            expected=np.asarray(expected,dtype=np.float32).astype(float)
            assert all(tuple(v)in actual for v in np.round(expected,5)), 'Incomplete stone Defender source geometry'
            assert abs(float(p[:,1].max())-(2.03+1.8673497438430786*2.65))<1e-5
        # Solid colliders follow actual measured envelopes, no giant decorative
        # blanket across training entrances or the pedestrian forecourt.
        low=p.min(axis=0).tolist();high=p.max(axis=0).tolist()
        report.append(dict(asset=name,family=name,lod='near',bytes=path.stat().st_size,
            sha256=sha(path),triangles=len(t),bounds=[low,high],
            collision_boxes=[[low[0],high[0],low[2],high[2],low[1],high[1]]],
            source='authored courtyard geometry'+(' and rigid resident Defender pose'if name=='castle_monument'else''),
            textures='exact resident purchased masonry/wood encoded bytes and sampler policies',
            new_image_bytes=0))
    return dict(schema_version=1,title='Valdoa castle training court and Defender monument',
        recipe_sha256=sha(Path(__file__)),source_files_sha256={str(p.relative_to(ROOT)):sha(p)for p in[HOUSE,KNIGHT]},
        image_policy_delta=0,new_gpu_image_bytes=0,assets=report)

def validate():
    manifest=json.loads((OUT/'castle_expansion_asset_manifest.json').read_text())
    assert manifest['recipe_sha256']==sha(Path(__file__)), 'Stale castle asset recipe'
    for source,digest in manifest['source_files_sha256'].items():
        assert sha(ROOT/source)==digest, ('Resident source changed',source)
    actual=audit(MODELS)
    assert actual==manifest, 'Published castle kit differs from validated manifest'
    return actual


def cook():
    OUT.mkdir(parents=True,exist_ok=True);stage=OUT/'stage';stage.mkdir(exist_ok=True)
    for name,builder in BUILDERS.items():
        mesh=builder()
        low=min(p[1]for points,_,_,_ in mesh.parts.values()for p in points)
        for points,_,_,_ in mesh.parts.values():
            for point in points:point[1]-=low
        save(stage/(name+'.glb'),name,mesh)
    manifest=audit(stage)
    for name in FAMILIES:(MODELS/(name+'.glb')).write_bytes((stage/(name+'.glb')).read_bytes())
    (OUT/'castle_expansion_asset_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cook',action='store_true');p.add_argument('--validate',action='store_true');a=p.parse_args()
    assert a.cook or a.validate
    result=cook()if a.cook else validate()
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
