"""Original Grandstone architecture, alpine ridges and winter ground dressing.

Uses only already-local owned/CC0 material maps and meshes. No character pose
is modified. Geometry is ordinary editable Studio scenery, with bounded LODs.
"""
from pathlib import Path
import copy,io,json,math,tempfile
import numpy as np
from PIL import Image
from scipy.spatial import ConvexHull
from compose_visual_upgrade import geometry
from build_stronghold_entrance_art import write,winter,tube

ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT/'projects/valwood_stronghold'
TEXT=ROOT/'assets/purchased/dwarven_stronghold/textures'

class Sculpture:
    def __init__(self):self.parts={};self.materials=[];self.images=[];self.textures=[];self.image_ids={}
    def material(self,name,color,rough=.8,metal=0,maps=None,snow=0):
        mat=dict(name=name,pbrMetallicRoughness=dict(baseColorFactor=[*color,1],roughnessFactor=rough,metallicFactor=metal))
        for role,path in (maps or {}).items():
            path=str(path)
            if path not in self.image_ids:
                self.image_ids[path]=len(self.textures);self.images.append(dict(uri=path,mimeType='image/jpeg'if path.endswith('.jpg')else'image/png'));self.textures.append(dict(source=len(self.images)-1,sampler=0))
            info=dict(index=self.image_ids[path])
            if role=='base':mat['pbrMetallicRoughness']['baseColorTexture']=info
            elif role=='normal':mat['normalTexture']=info
            elif role=='arm':mat['pbrMetallicRoughness']['metallicRoughnessTexture']=info;mat['occlusionTexture']=info
        if snow:winter(mat,snow)
        self.materials.append(mat);return len(self.materials)-1
    def face(self,mat,points,uv=None,normals=None,scale=.26):
        p=np.array(points,dtype=float);n=np.cross(p[1]-p[0],p[2]-p[0]);length=np.linalg.norm(n)
        if length<1e-10:return
        n/=length
        if uv is None:uv=p[:,[i for i in range(3)if i!=int(np.argmax(np.abs(n)))]]*scale
        r=self.parts.setdefault(mat,[[],[],[],[]]);offset=len(r[0]);r[0].extend(p.tolist());r[1].extend(np.array(normals).tolist()if normals is not None else[n.tolist()]*len(p));r[2].extend(np.array(uv).tolist());r[3].extend([[offset,offset+j,offset+j+1]for j in range(1,len(p)-1)])
    def polygon(self,mat,outline,back,front,bevel=.10):
        p=np.array(outline,dtype=float)
        if np.sum(p[:,0]*np.roll(p[:,1],-1)-p[:,1]*np.roll(p[:,0],-1))<0:p=p[::-1]
        edge=np.roll(p,-1,axis=0)-p;length=np.linalg.norm(edge,axis=1)
        e=min(bevel,float(length.min())*.18,(front-back)*.22)
        outward=np.c_[edge[:,1],-edge[:,0]]/length[:,None];constant=np.sum(outward*p,axis=1)-e
        inside=np.array([np.linalg.solve(np.stack([outward[i-1],outward[i]]),[constant[i-1],constant[i]])for i in range(len(p))])
        a=np.c_[p,np.full(len(p),back)];b=np.c_[p,np.full(len(p),front-e)];c=np.c_[inside,np.full(len(p),front)]
        self.face(mat,a[::-1]);self.face(mat,c)
        for i in range(len(p)):
            j=(i+1)%len(p);self.face(mat,[a[i],a[j],b[j],b[i]]);self.face(mat,[b[i],b[j],c[j],c[i]])
    def box(self,mat,lo,hi,bevel=.08):
        x,y,z=lo;xx,yy,zz=hi;self.polygon(mat,[(x,y),(xx,y),(xx,yy),(x,yy)],z,zz,bevel)
    def beam(self,mat,a,b,width,back,front,bevel=.08):
        a=np.array(a);b=np.array(b);side=np.array([-(b-a)[1],(b-a)[0]]);side*=width*.5/np.linalg.norm(side)
        self.polygon(mat,[a-side,b-side,b+side,a+side],back,front,bevel)
    def save(self,name):
        parts=[[np.array(a)for a in r]+[mat]for mat,r in self.parts.items()]
        return write(name,parts,self.materials,self.textures,self.images)

def stone_maps():return {role:TEXT/('granite_'+role+'.png')for role in ['base','normal','arm']}

def gate():
    m=Sculpture();rng=np.random.default_rng(449823);maps=stone_maps()
    stone=[m.material('Hand-dressed granite '+str(i),np.array([.66,.69,.70])*float(rng.uniform(.88,1.1)),.80,maps=maps,snow=.44)for i in range(6)]
    pale=m.material('Worn granite arrises',[.79,.79,.75],.73,maps=maps,snow=.5)
    dark=m.material('Deep cut stone recess',[.20,.24,.27],.89,maps=maps)
    bronze=m.material('Worn bronze clan inlay',[.42,.29,.12],.43,.82)
    iron=m.material('Blackened forged chain',[.11,.13,.14],.48,.85)
    # An open pointed, polygonal entrance built from individual voussoirs.
    inner=np.array([[-9.,0],[-9.,16.8],[-5.2,22.4],[0.,25.8],[5.2,22.4],[9.,16.8],[9.,0]])
    outer=np.array([[-16.8,0],[-16.8,20.2],[-10.0,28.9],[0.,34.8],[10.,28.9],[16.8,20.2],[16.8,0]])
    for j in range(len(inner)-1):
        count=9 if j in [0,5]else 4
        for k in range(count):
            t0=(k+.006)/count;t1=(k+.994)/count
            a=inner[j]*(1-t0)+inner[j+1]*t0;b=inner[j]*(1-t1)+inner[j+1]*t1
            c=outer[j]*(1-t1)+outer[j+1]*t1;d=outer[j]*(1-t0)+outer[j+1]*t0
            m.polygon(stone[(j+k)%6],[a,b,c,d],-16,4.4+float(rng.uniform(-.025,.025)),.10)
        m.beam(pale,outer[j],outer[j+1],.62,-16.2,4.85,.12)
        inset=inner[j]*.18+outer[j]*.82;end=inner[j+1]*.18+outer[j+1]*.82
        m.beam(dark,inset,end,.42,4.42,4.49,.055)
        m.beam(bronze,inset,end,.10,4.50,4.55,.018)
    # Broad battered flanks are real layered masonry, with deep inset panels.
    for side in [-1,1]:
        for row in range(15):
            y=row*2.12;left=17.3;right=28.-y*.11
            lo,hi=sorted([side*left,side*right]);m.box(stone[row%6],(lo,y,-16),(hi,y+2.065,2.75),.19)
        for y in [0.,6.1,27.8,31.5]:
            lo,hi=sorted([side*16.5,side*(28.7-y*.10)]);m.box(pale,(lo,y,-3.1),(hi,y+.7,3.5),.13)
        center=side*21.6
        m.box(dark,(center-2.45,9.1,2.78),(center+2.45,24.0,2.88),.1)
        for x in [center-2.6,center+2.6]:m.box(pale,(x-.20,8.8,2.8),(x+.20,24.3,3.55),.08)
        for y in [8.8,23.9]:m.box(pale,(center-2.8,y,2.8),(center+2.8,y+.4,3.55),.08)
        # Forged hammer and anvil marks; every stroke has a bevel and depth.
        for y in [12.8,20.1]:
            m.beam(bronze,[center-1.15,y-1.45],[center+1.15,y+1.4],.20,2.90,3.08,.04)
            m.beam(bronze,[center-1.1,y+.5],[center+.45,y+1.8],.57,2.90,3.20,.10)
        m.polygon(bronze,[(center-1.6,15.5),(center+1.6,15.5),(center+.8,14.9),(center-.8,14.9)],2.9,3.13,.08)
        m.box(bronze,(center-.55,14.2,2.9),(center+.55,15.0,3.13),.05)
        m.polygon(bronze,[(center-.9,14.0),(center+.9,14.0),(center+.55,14.4),(center-.55,14.4)],2.9,3.13,.05)
        # Suspended, interlocking oval chain links flank the crown.
        for k in range(20):
            cy=30.3-k*.72;cx=side*18.4;cz=5.25;ring=[]
            for a in np.linspace(0,math.tau,17):
                if k%2:ring.append([cx,cy+math.cos(a)*.47,cz+math.sin(a)*.27])
                else:ring.append([cx+math.sin(a)*.27,cy+math.cos(a)*.47,cz])
            tube(m.parts,iron,ring,[.075]*len(ring),6)
        glass=m.material('Amber glass in the suspended lantern',[.42,.24,.08],.35)
        m.materials[glass]['emissiveFactor']=[1.8,.76,.22]
        cx=side*18.4
        m.box(glass,(cx-.52,13.2,4.9),(cx+.52,15.7,5.6),.1)
        for xx in [cx-.63,cx+.63]:m.box(bronze,(xx-.07,13.0,4.8),(xx+.07,15.9,5.8),.045)
        for yy in [13.,14.4,15.8]:m.box(bronze,(cx-.70,yy,4.8),(cx+.70,yy+.14,5.8),.045)
        m.polygon(bronze,[(cx-.84,15.95),(cx+.84,15.95),(cx,16.55)],4.6,6.0,.07)
    # Keystone and a stepped crest make the opening read from the approach.
    m.polygon(pale,[(-1.8,28.8),(1.8,28.8),(2.55,34.0),(0,37.0),(-2.55,34.0)],-4.2,5.15,.22)
    for y,half in [(35.7,9.6),(36.8,7.7),(37.9,5.8)]:m.box(pale,(-half,y,-3.5),(half,y+.72,3.5),.14)
    # A coherent carved clan crest replaces the unrelated imported arch prop.
    center=np.array([0.,42.1]);angles=np.arange(8)*math.tau/8+math.pi/8
    outer=np.c_[np.cos(angles)*5.35,np.sin(angles)*5.35]+center
    inner=(outer-center)*.80+center
    m.polygon(pale,outer,-2.8,2.9,.28);m.polygon(dark,inner,2.91,3.0,.12)
    for j in range(8):m.beam(bronze,inner[j],inner[(j+1)%8],.12,3.01,3.12,.025)
    for side in [-1,1]:
        m.beam(bronze,[side*1.65,39.6],[-side*1.6,44.0],.25,3.02,3.26,.05)
        m.beam(bronze,[-side*2.4,43.7],[-side*.9,44.8],.76,3.03,3.44,.12)
    # The tall stone vault steps down to the hall's 18m ceiling at its rear.
    # Close that transition with a real tympanum rather than an exterior gap.
    half=5.2+(22.4-18.)*3.8/5.6
    m.polygon(dark,[(-half,18.),(half,18.),(5.2,22.4),(0.,25.8),(-5.2,22.4)],-16.05,-15.84,.025)
    # Sheltered, downward-facing vault stone has a dry rough response. The
    # exterior ARM finish reflected the bright sky as a pale band at grazing
    # angles, making the inside of the vault resemble accumulated snow.
    vault=m.material('Dry rough granite inside the pointed vault',[.43,.46,.47],.96,maps={k:v for k,v in maps.items()if k!='arm'})
    for mat in stone:
        p,n,uv,indices=[np.asarray(a)for a in m.parts[mat]]
        center=p[indices].mean(axis=1);normal=n[indices].mean(axis=1)
        selected=(normal[:,1]<-.50)&(np.abs(center[:,0])<10)&(center[:,1]>16)&(center[:,1]<27)
        if not selected.any():continue
        ids,local=np.unique(indices[selected],return_inverse=True)
        from build_stronghold_entrance_art import append_part
        append_part(m.parts,vault,p[ids],n[ids],uv[ids],local.reshape(-1,3))
        m.parts[mat][3]=indices[~selected].tolist()
    return m.save('mountain_gate')

def gate_supports():
    records=[]
    for name,side in [('mountain_gate_pier',0),('mountain_gate_wing_left',-1),('mountain_gate_wing_right',1)]:
        m=Sculpture();stone=m.material('Recessed structural granite',[.61,.65,.67],.86,maps=stone_maps())
        if side==0:m.box(stone,(-3.9,0,-16),(3.9,16.8,4.23),.08)
        else:
            outline=np.array([[-5.2,0],[5.2,0],[1.8,31.5],[-5.2,31.5]])
            if side<0:outline[:,0]*=-1
            m.polygon(stone,outline,-16,2.60,.10)
        records.append(m.save(name))
    return records

def masonry():
    records=[];maps=stone_maps()
    for name,floor in [('dressed_hall_wall',False),('dressed_floor',True)]:
        m=Sculpture();rng=np.random.default_rng(8835)
        mats=[m.material('Masonry variation '+str(i),np.array([.68,.70,.69])*float(rng.uniform(.87,1.09)),.83,maps=maps)for i in range(5)]
        if floor:
            for j in range(3):
                for k in range(2):
                    x=-3+k*3;z=-3+j*2;m.box(mats[(j+k)%5],(x+.018,0,z+.018),(x+2.982,.018,z+1.982),.006)
        else:
            # Continuous stone behind the dressed blocks closes the joints.
            # A chamfered module without backing leaked exterior light through
            # long horizontal slots in the otherwise enclosed mountain hall.
            m.box(mats[0],(-3,0,-1.28),(3,14.97,1.28),0)
            for j in range(9):
                for k in range(2):
                    x=-3+k*3;m.box(mats[(j*3+k)%5],(x+.027,j*1.67,-1.4),(x+2.973,j*1.67+1.61,1.4),.11)
        records.append(m.save(name))
    return records

def hash_noise(x,z):
    def h(a,b):
        u=(a.astype(np.int64)*374761393+b.astype(np.int64)*668265263)&0xffffffff
        u=((u^(u>>13))*1274126177)&0xffffffff;return ((u^(u>>16))&0xffffff)/16777215.
    i=np.floor(x);j=np.floor(z);u=x-i;v=z-j;u=u*u*(3-2*u);v=v*v*(3-2*v)
    return (h(i,j)*(1-u)+h(i+1,j)*u)*(1-v)+(h(i,j+1)*(1-u)+h(i+1,j+1)*u)*v

def ridge_height(x,z):
    # Two branching ridgelines, rather than a row of radial cone primitives.
    # Lateral drainage varies the long faces; the crest has resolved breaks
    # at the 7m/14m mesh tiers, without sub-triangle displacement spikes.
    xx=x+(hash_noise(x*.004+31,z*.004)-.5)*48
    h=np.zeros_like(xx)
    for offset,seed,scale in [(-520,3.,1.),(-800,47.,.91)]:
        crest_z=offset+(hash_noise(xx*.003+seed,xx*0+19)-.5)*125
        crest=(210+150*hash_noise(xx*.004+seed,xx*0+7)
               +65*hash_noise(xx*.014+seed,xx*0+31)
               +22*hash_noise(xx*.036+seed,xx*0+11))*scale
        lateral=xx+(z-crest_z)*.28
        drainage=(hash_noise(lateral*.013+seed,z*.002)-.5)*100
        q=np.abs(z-crest_z+drainage)/340
        face=np.maximum(0,1-q)**1.40
        rock=(1-np.abs(hash_noise(xx*.021+seed,z*.014)*2-1))-.55
        ridge=crest*face+rock*24*np.clip(face*4,0,1)
        h=np.maximum(h,ridge)
    edge=np.clip((800-np.abs(x))/165,0,1)
    return np.maximum(0,h*edge)

def mountains():
    records=[]
    for step,name in [(7,'alpine_backdrop'),(14,'alpine_backdrop_mid')]:
        # Far rock needs broad tonal structure, not a near-field normal map
        # repeated over hundreds of metres of slope.
        m=Sculpture();mat=m.material('Wind-scoured alpine granite',[.26,.33,.41],.93,maps={'base':stone_maps()['base']},snow=.80)
        xs=np.arange(-800,801,step);zs=np.arange(-1030,-119,step);x,z=np.meshgrid(xs,zs);y=ridge_height(x,z)
        dx=(ridge_height(x+1,z)-ridge_height(x-1,z))*.5;dz=(ridge_height(x,z+1)-ridge_height(x,z-1))*.5
        normals=np.stack([-dx,np.ones_like(x),-dz],axis=-1);normals/=np.linalg.norm(normals,axis=-1,keepdims=True)
        pos=np.stack([x,y,z],axis=-1)
        for j in range(len(zs)-1):
            for i in range(len(xs)-1):
                for indices in [[(j,i),(j+1,i),(j+1,i+1)],[(j,i),(j+1,i+1),(j,i+1)]]:
                    p=np.array([pos[a,b]for a,b in indices]);n=np.array([normals[a,b]for a,b in indices])
                    uv=np.c_[p[:,0]*.012,p[:,2]*.012+p[:,1]*.005]
                    m.face(mat,p,normals=n,uv=uv)
        records.append(m.save(name))
    return records

def winter_variant(source,name):
    d,b,parts=geometry(PROJECT/'models'/(source+'.glb'));mats=copy.deepcopy(d['materials']);images=[]
    for mat in mats:winter(mat,.82)
    with tempfile.TemporaryDirectory(prefix='grandstone-winter-detail-')as temp:
        for i,spec in enumerate(d['images']):
            view=d['bufferViews'][spec['bufferView']];raw=b[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
            path=Path(temp)/(str(i)+'.png');im=Image.open(io.BytesIO(raw));im.thumbnail((1024,1024),Image.Resampling.LANCZOS);im.save(path);images.append(dict(uri=str(path),mimeType='image/png'))
        return write(name,parts,mats,d['textures'],images)

def grass_and_talus():
    records=[];rng=np.random.default_rng(97283);m=Sculpture()
    mats=[m.material('Dry alpine sedge '+str(i),c,.93)for i,c in enumerate([[.23,.21,.15],[.35,.29,.18],[.46,.39,.25],[.18,.20,.16]])]
    for mat in m.materials:mat['doubleSided']=True
    for k in range(165):
        a=float(rng.uniform(0,math.tau));rad=float(rng.uniform(0,1))**.7*.85;root=np.array([math.cos(a)*rad,0,math.sin(a)*rad]);angle=float(rng.uniform(0,math.tau))
        direction=np.array([math.cos(angle),0,math.sin(angle)]);side=np.array([-direction[2],0,direction[0]])
        height=float(rng.uniform(.16,.67));bend=float(rng.uniform(.08,.45));width=float(rng.uniform(.004,.013));mat=k%4
        points=[]
        for j in range(5):
            t=j/4;c=root+direction*bend*t*t+[0,height*(t-.16*t**3),0];w=width*(1-t)**.7+.0001;points.extend([c-side*w,c+side*w])
        for j in range(4):m.face(mat,[points[j*2],points[j*2+1],points[j*2+3],points[j*2+2]])
    records.append(m.save('winter_sedge'))
    m=Sculpture();mats=[m.material('Scoured granite fragment '+str(i),np.array([.56,.59,.60])*q,.91,maps=stone_maps(),snow=.38)for i,q in enumerate([.75,.88,1.])]
    for k in range(18):
        center=np.array([float(rng.uniform(-1.2,1.2)),0,float(rng.uniform(-1.2,1.2))]);radius=float(rng.uniform(.045,.18))
        p=rng.normal(size=(28,3));p/=np.linalg.norm(p,axis=1,keepdims=True);p*=rng.uniform(.75,1.1,size=(28,1))*[radius,radius*.45,radius*.8];p+=center
        hull=ConvexHull(p)
        for ids,eq in zip(hull.simplices,hull.equations):
            q=p[ids]
            if np.cross(q[1]-q[0],q[2]-q[0])@eq[:3]<0:q=q[[0,2,1]]
            m.face(mats[k%3],q,scale=.5)
    records.append(m.save('winter_talus'));return records

def banner():
    m=Sculpture();iron=m.material('Forged banner pole',[.16,.17,.16],.44,.8)
    cloth=m.material('Weathered mountain clan wool',[.045,.115,.17],.97,maps={'normal':ROOT/'assets/polyhaven/milestone/fabric_pattern_07/textures/fabric_pattern_07_nor_gl_1k.jpg'})
    m.materials[cloth]['doubleSided']=True;gold=m.material('Dull gold embroidered clan mark',[.48,.34,.13],.65,.24)
    tube(m.parts,iron,[[0,0,0],[0,7.1,0]],[.075,.055],9)
    tube(m.parts,iron,[[0,6.7,0],[2.22,6.7,0]],[.05,.05],7)
    def surface(x,y):return [x,y,.10*math.sin(x*3.1+y*.8)+.08*math.sin(y*4.3-x)*x]
    for j in range(24):
        for i in range(18):
            x0=.10+i*2/18;x1=.10+(i+1)*2/18;y0=3.25+j*3.3/24;y1=3.25+(j+1)*3.3/24
            if j<4 and abs(i+.5-9)<4-j:continue
            m.face(cloth,[surface(x0,y0),surface(x1,y0),surface(x1,y1),surface(x0,y1)],uv=[[x0,y0],[x1,y0],[x1,y1],[x0,y1]],scale=1)
    def stitch(a,b,width):
        a=np.array(a);b=np.array(b);v=b-a;side=np.array([-v[1],v[0]])*width*.5/np.linalg.norm(v)
        points=[surface(*q)for q in [a-side,b-side,b+side,a+side]]
        for q in points:q[2]+=.004
        m.face(gold,points)
    for a,b,w in [((.7,4.15),(1.5,5.8),.095),((.55,5.5),(1.19,5.92),.29),((.56,4.51),(1.65,4.51),.13),((.78,4.34),(1.4,4.34),.23)]:stitch(a,b,w)
    return m.save('clan_banner')

def build():
    records=[gate(),*gate_supports(),*masonry(),*mountains(),winter_variant('cinematic_deadwood','winter_deadfall'),winter_variant('starting_stump','winter_cut_stump'),*grass_and_talus(),banner()]
    (PROJECT/'assets/mountain-finish.json').write_text(json.dumps(dict(version=1,description='Original angular stone architecture, ridged alpine terrain and winter understory, using existing local material/mesh sources',records=records),indent=2)+'\n')
    print('Built',len(records),'mountain architecture and winter detail assets',flush=True)
if __name__=='__main__':build()
