"""Replace only the shelter fabric and timber in the current owned camp.

Existing CC0 hessian/wood maps are retained. Real surface normals, seams,
rolled hems, pegged ropes and pulled-back entrance cloth supply close detail.
Outputs are private; source files and placement/collision data are read-only.
"""
from pathlib import Path
import copy, hashlib, json, math, sys
import numpy as np

N = Path(__file__).resolve().parents[1]
E = N.parents[1]
sys.path.insert(0, str(E/'tools'))
from build_opening_prologue import Glb, read_glb, array

SOURCE = E/'projects/valwood_current/models/camp_atmosphere'
OUT = N/'tent-work'
TX,TZ,W,D = -3.55,-4.1,1.7,1.45
RIDGE = 2.01837

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()

def remove_old(name, p):
    if name in ('Weathered photographed burlap canvas','Hand repaired burlap seams','Hemp rope'):
        return np.ones(len(p),dtype=bool)
    if name == 'Weathered peeled camp branches':
        return ((p[:,:,2]<-2.45)&(p[:,:,0]<-1.0)).all(1)
    return np.zeros(len(p),dtype=bool)

def build(lod):
    src = SOURCE/('camp_hearth'+('_mid' if lod=='mid' else '')+'.glb')
    d,b=read_glb(src)
    g=Glb();g.doc=copy.deepcopy(d);g.binary=bytearray(b)
    ids={m['name']:i for i,m in enumerate(g.doc['materials'])}
    cloth=ids['Weathered photographed burlap canvas'];hem=ids['Hand repaired burlap seams']
    wood=ids['Weathered peeled camp branches'];rope=ids['Hemp rope']
    # A restrained matte cloth value. No painted light or baked bright seams.
    g.doc['materials'][cloth]['pbrMetallicRoughness']['baseColorFactor']=[.80,.81,.77,1]
    g.doc['materials'][hem]['pbrMetallicRoughness']['baseColorFactor']=[.64,.65,.59,1]
    stitch=len(g.doc['materials']);g.doc['materials'].append(dict(name='Camp tent hemp stitching',pbrMetallicRoughness=dict(baseColorFactor=[.40,.36,.27,1],roughnessFactor=1,metallicFactor=0)))
    retained=[]
    for mesh in d['meshes']:
        for q in mesh['primitives']:
            p=array(d,b,q['attributes']['POSITION']);n=array(d,b,q['attributes']['NORMAL']);uv=array(d,b,q['attributes']['TEXCOORD_0']);ix=array(d,b,q['indices']).reshape(-1,3).astype(int)
            name=d['materials'][q['material']]['name'];kill=remove_old(name,p[ix]);keep=ix[~kill]
            retained.append(dict(material=name,removed=int(kill.sum()),retained=len(keep)))
            if len(keep):
                used=np.unique(keep);remap=np.full(len(p),-1);remap[used]=np.arange(len(used));g.mesh(p[used],n[used],uv[used],remap[keep].ravel(),q['material'])

    def tri(a,b,c,mat,uv=None):
        p=np.asarray([a,b,c],float);nn=np.cross(p[1]-p[0],p[2]-p[0]);length=np.linalg.norm(nn)
        if length<1e-10:return
        nn/=length
        g.mesh(p,np.tile(nn,(3,1)),uv if uv is not None else p[:,[0,2]]/.3,[0,1,2],mat)

    def quad(points,mat,uv=None,normal=None):
        p=np.asarray(points,float);nn=np.cross(p[1]-p[0],p[2]-p[0]);nn/=np.linalg.norm(nn)
        if normal is not None and np.dot(nn,normal)<0:
            p=p[::-1];nn=-nn
            if uv is not None:uv=np.asarray(uv)[::-1]
        g.mesh(p,np.tile(nn,(4,1)),uv if uv is not None else p[:,[0,2]]/.3,[0,1,2,0,2,3],mat)

    def beam(a,b,r,mat,ns=10):
        a,b=np.asarray(a,float),np.asarray(b,float);axis=b-a;length=np.linalg.norm(axis);axis/=length
        u=np.cross(axis,[0,1,0] if abs(axis[1])<.9 else [1,0,0]);u/=np.linalg.norm(u);v=np.cross(axis,u)
        for j in range(ns):
            t,tt=j*math.tau/ns,(j+1)*math.tau/ns
            na=u*math.cos(t)+v*math.sin(t);nb=u*math.cos(tt)+v*math.sin(tt)
            pp=[a+na*r,a+nb*r,b+nb*r,b+na*r]
            nn=[na,nb,nb,na];uv=[[j/ns,0],[(j+1)/ns,0],[(j+1)/ns,length/.3],[j/ns,length/.3]]
            ii=[0,1,2,0,2,3]
            if np.dot(np.cross(pp[1]-pp[0],pp[2]-pp[0]),na)<0:ii=[0,2,1,0,3,2]
            g.mesh(pp,nn,uv,ii,mat)
        for c,n,rev in [(a,-axis,True),(b,axis,False)]:
            pp=[c]+[c+r*(u*math.cos(j*math.tau/ns)+v*math.sin(j*math.tau/ns)) for j in range(ns)]
            ix=[]
            for j in range(ns):ix.extend([0,1+(j+1)%ns,1+j] if rev else [0,1+j,1+(j+1)%ns])
            g.mesh(pp,np.tile(n,(ns+1,1)),np.asarray(pp)[:,[0,2]]/.3,ix,mat)

    def ground(x,z):
        # Measured original shelter contacts; no terrain edits in this pass.
        return .183+.009*(x-TX)+.002*(z-TZ)

    def roof(sign,u,v):
        x=TX+sign*W*u;z=TZ-D+2*D*v;edge=ground(x,z)+.041
        y=RIDGE*(1-u)+edge*u-.105*math.sin(math.pi*u)
        # Tension folds gather toward the pegged corners and vanish at ridge.
        gather=.032*math.sin(v*math.tau*5+u*2.1)+.012*math.sin(v*math.tau*11-u*5)
        y+=gather*math.sin(math.pi*u)*(.45+.55*u)
        y-=.027*math.sin(math.pi*v)*u
        return np.array([x,y,z])

    def normal(sign,u,v):
        h=.0001;du=roof(sign,u+h,v)-roof(sign,u-h,v);dv=roof(sign,u,v+h)-roof(sign,u,v-h)
        n=np.cross(dv,du)*sign;return n/np.linalg.norm(n)

    nx,nz=(22,36) if lod=='near' else (12,20)
    for sign in [-1,1]:
        pp=[];nn=[];uv=[]
        for i in range(nx+1):
            for j in range(nz+1):
                u,v=i/nx,j/nz;pp.append(roof(sign,u,v));nn.append(normal(sign,u,v));uv.append([u*math.hypot(W,RIDGE-.22)/.3,v*2*D/.3])
        pp=np.asarray(pp);nn=np.asarray(nn);ix=[]
        for i in range(nx):
            for j in range(nz):
                a=i*(nz+1)+j;b=a+nz+1
                ix.extend([a,b,b+1,a,b+1,a+1] if sign<0 else [a,b+1,b,a,a+1,b+1])
        g.mesh(pp,nn,uv,ix,cloth)
        # Separate inner surface has correct normals and seven-mm separation.
        g.mesh(pp-nn*.007,-nn,uv,np.asarray(ix).reshape(-1,3)[:,::-1].ravel(),cloth)
        # Reinforced longitudinal panels follow the true curved cloth surface.
        for v in [0,.32,.67,1]:
            for i in range(nx):
                pts=[]
                for u,vv in [(i/nx,max(0,v-.003)),((i+1)/nx,max(0,v-.003)),((i+1)/nx,min(1,v+.003)),(i/nx,min(1,v+.003))]:
                    pts.append(roof(sign,u,vv)+normal(sign,u,vv)*.0035)
                quad(pts,hem,normal=normal(sign,(i+.5)/nx,v))
        # Low dark splash skirt has depth and wavy edge, rather than a flat tint.
        for j in range(nz):
            pts=[]
            for u,v in [(.93,j/nz),(1,j/nz),(1,(j+1)/nz),(.93,(j+1)/nz)]:pts.append(roof(sign,u,v)+normal(sign,u,v)*.0045)
            quad(pts,hem,normal=normal(sign,.96,(j+.5)/nz))
        # Folded front/back hems, ridge piping, a small sewn repair per side.
        for v in [0,1]:
            for i in range(nx):beam(roof(sign,i/nx,v)+normal(sign,i/nx,v)*.006,roof(sign,(i+1)/nx,v)+normal(sign,(i+1)/nx,v)*.006,.008,hem,6)
        for i in range(6):
            a=.36+i*.10/6;b=.36+(i+1)*.10/6
            pts=[roof(sign,u,v)+normal(sign,u,v)*.007 for u,v in [(a,.51),(b,.51),(b,.64),(a,.64)]]
            quad(pts,hem,normal=normal(sign,(a+b)/2,.57))
        if lod=='near':
            for v in [.32,.67]:
                for j in range(33):
                    u=.045+j*.027
                    a=roof(sign,u,v-.005)+normal(sign,u,v)*.008
                    b=roof(sign,u+.006,v+.005)+normal(sign,u+.006,v)*.008
                    beam(a,b,.0018,stitch,4)
        for v in [0,1]:
            x=TX+sign*W;z=TZ-D+2*D*v
            # Timber follows original footprint and remains under the cloth.
            beam([x,ground(x,z)-.04,z],[TX,RIDGE-.036,z],.031,wood,12 if lod=='near' else 8)
            end=np.array([TX+sign*(W+.65),ground(TX+sign*(W+.65),z+.12)+.04,z+.12])
            start=roof(sign,1,v);mid=(start+end)*.5;mid[1]-=.015
            beam(start,mid,.009,rope,8);beam(mid,end,.009,rope,8)
            beam(end+[0,-.09,0],end+[.04,.10,0],.021,wood,8)
            # Rope knots at the peg and canvas eyelet; no new route footprint.
            for o in [-.018,0,.018]:beam(end+[0,o,-.025],end+[0,o,.025],.011,rope,6)
        # Front flaps are drawn back, leaving a visible human-sized opening.
        # A curved grid makes small gathered drapery along the sides.
        apex=np.array([TX,RIDGE-.018,TZ+D+.012])
        bottom=np.array([TX+sign*(W-.05),ground(TX+sign*W,TZ+D)+.05,TZ+D+.015])
        tie=np.array([TX+sign*1.36,.77,TZ+D+.12])
        steps=14 if lod=='near' else 8
        for i in range(steps):
            a,b=i/steps,(i+1)/steps
            def edge(t):
                p=apex*(1-t)+bottom*t
                p[2]+=.035*math.sin(t*math.pi)*math.sin(t*math.tau*5)
                return p
            pts=[edge(a),edge(b),tie]
            tri(*pts,hem);tri(*pts[::-1],hem)
        for dy in [-.015,.015]:beam(tie+[0,dy,-.04],tie+[0,dy,.04],.012,rope,8)
    beam([TX,RIDGE-.025,TZ-D-.10],[TX,RIDGE-.025,TZ+D+.10],.039,wood,12)
    beam([TX,RIDGE+.004,TZ-D],[TX,RIDGE+.004,TZ+D],.009,hem,8)
    # Rear cloth gathers toward the ridge, with smooth physical normals rather
    # than a single flat triangular panel. The original three rolls stay exact.
    def rear(u,v):
        x=TX+u*W*(1-v);base=ground(x,TZ-D)+.045
        y=base*(1-v)+(RIDGE-.020)*v
        z=TZ-D+.042*math.sin(math.pi*v)*(1-v)*math.sin(u*math.pi*4+v*1.3)
        z+=.010*math.sin(math.pi*v)*(1-v)*math.sin(u*math.pi*6-v*2)
        return np.array([x,y,z])
    def rear_normal(u,v):
        h=.0001;du=rear(u+h,v)-rear(u-h,v);dv=rear(u,v+h)-rear(u,v-h)
        n=np.cross(du,dv);length=np.linalg.norm(n)
        return n/length if length>1e-8 else np.array([0.,0.,1.])
    nu,nv=(28,20) if lod=='near' else (16,12)
    pp=[];nn=[];uv=[]
    for i in range(nu+1):
        for j in range(nv+1):
            u,v=-1+2*i/nu,j/nv;p=rear(u,v)
            pp.append(p);nn.append(rear_normal(u,v));uv.append([(p[0]-TX)/.3,p[1]/.3])
    pp=np.asarray(pp);nn=np.asarray(nn);ix=[]
    for i in range(nu):
        for j in range(nv):
            a=i*(nv+1)+j;b=a+nv+1
            ix.extend([a,b,a+1])
            if j<nv-1:ix.extend([b,b+1,a+1])
    g.mesh(pp,nn,uv,ix,cloth)
    g.mesh(pp-nn*.007,-nn,uv,np.asarray(ix).reshape(-1,3)[:,::-1].ravel(),cloth)
    for j in range(nv):
        pts=[rear(u,v)+rear_normal(u,v)*.004 for u,v in [(-.006,j/nv),(.006,j/nv),(.006,(j+1)/nv),(-.006,(j+1)/nv)]]
        if j<nv-1:quad(pts,hem,normal=[0,0,1])
    g.doc.setdefault('extras',{})['camp_prop_pass']={'source_sha256':sha(src),'changes':'Shelter only: physically normaled tensioned canvas, hems, gathered entrance, timber and guy ropes. Original provision/stool/bedroll meshes retained.'}
    out=OUT/('camp_tent'+('_mid' if lod=='mid' else '')+'.glb');out.write_bytes(g.finish())
    doc,buf=read_glb(out);pos=np.concatenate([array(doc,buf,q['attributes']['POSITION']) for mesh in doc['meshes'] for q in mesh['primitives']])
    return dict(lod=lod,path=str(out),sha256=sha(out),source=str(src.resolve()),source_sha256=sha(src),triangles=sum(doc['accessors'][q['indices']]['count']//3 for mesh in doc['meshes'] for q in mesh['primitives']),images=len(doc['images']),materials=len(doc['materials']),minimum=pos.min(0).tolist(),maximum=pos.max(0).tolist(),original_triangles=retained)

if __name__=='__main__':
    OUT.mkdir(exist_ok=True,parents=True)
    results=[build(lod) for lod in ['near','mid']]
    (OUT/'TENT_BUILD.json').write_text(json.dumps(dict(models=results,new_texture_bytes=0,collision_changed=False,placement_changed=False),indent=2)+'\n')
    print(json.dumps(results,indent=2))
