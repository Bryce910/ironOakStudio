"""Winter variants of existing owned conifers and original frozen-cascade mesh."""
from pathlib import Path
import copy,io,json,math,tempfile
import numpy as np
from PIL import Image
import build_visual_upgrade_assets as mesh
from compose_visual_upgrade import geometry
from build_cinematic_diversity import Writer

ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT/'projects/valwood_stronghold'
OUT=PROJECT/'models/stronghold'

def trees():
    records=[]
    with tempfile.TemporaryDirectory(prefix='grandstone-winter-trees-')as temp:
        for source,name in [('cinematic_pine_near','winter_pine'),('cinematic_pine_young_near','winter_sapling')]:
            doc,blob,parts=geometry(PROJECT/'models'/(source+'.glb'))
            materials=copy.deepcopy(doc['materials']);images=[];textures=[]
            roles={}
            for material in materials:
                pbr=material['pbrMetallicRoughness'];leaf=material.get('alphaMode')=='MASK'
                for role,info in [('base',pbr.get('baseColorTexture')),('normal',material.get('normalTexture')),('arm',pbr.get('metallicRoughnessTexture'))]:
                    if info:roles[doc['textures'][info['index']]['source']]=(role,leaf)
                if leaf:pbr.update(baseColorFactor=[1,1,1,1],roughnessFactor=.96)
            for i,spec in enumerate(doc['images']):
                v=doc['bufferViews'][spec['bufferView']];raw=blob[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
                im=Image.open(io.BytesIO(raw)).convert('RGBA');role,leaf=roles.get(i,('base',False))
                im.thumbnail((1024,1024)if role=='base'else(512,512)if role=='normal'else(256,256),Image.Resampling.LANCZOS)
                if role=='base'and leaf:
                    a=np.array(im);rgb=a[:,:,:3].astype(float)/255;lum=rgb@np.array([.2126,.7152,.0722])
                    # Keep the photographed needle silhouettes, alpha and fine
                    # structure. Uneven powder leaves dark needles beneath it.
                    frost=np.clip(.68+lum*.45,.68,.96)
                    snow=np.stack([.78+lum*.13,.85+lum*.1,.91+lum*.07],axis=2)
                    a[:,:,:3]=np.uint8(np.clip(rgb*(1-frost[:,:,None])+snow*frost[:,:,None],0,1)*255)
                    im=Image.fromarray(a)
                p=Path(temp)/(name+str(i)+'.png');im.save(p);images.append(dict(uri=str(p),mimeType='image/png'))
            textures=copy.deepcopy(doc['textures'])
            writer=Writer(materials,textures,images);writer.g['samplers']=doc.get('samplers',writer.g['samplers'])
            for j,(pos,n,uv,idx,mat)in enumerate(parts):writer.mesh(name+str(j),pos,n,uv,idx,mat)
            destination=OUT/(name+'.glb');writer.save(destination,embedded=True)
            records.append(dict(file=str(destination.relative_to(ROOT)),source=source,triangles=sum(len(p[3])for p in parts),bytes=destination.stat().st_size))
    return records

def waterfall():
    mesh.OUT=OUT;m=mesh.Model()
    textures=ROOT/'assets/purchased/dwarven_stronghold/textures'
    maps={'base':textures/'snow_02_diff_1k.jpg','normal':textures/'snow_02_nor_gl_1k.jpg','arm':textures/'snow_02_arm_1k.jpg'}
    ice=m.material('Dense blue glacier ice',[.43,.57,.65],.27,maps=maps)
    pale=m.material('Bubbled opaque cascade ice',[.62,.73,.78],.34,maps=maps)
    crack=m.material('Frost in frozen fissures',[.78,.88,.94],.51)
    snow=m.material('Powder on ice ledges',[.80,.87,.94],.86)
    def tube(mat,x,z,bottom,top,rx,rz,seed,tip=False):
        # Closed elliptical lobes with smooth radial normals and irregular
        # vertical striations. Icicles end in genuinely tapered points.
        rings=15;sides=12;record=m.parts.setdefault(mat,[[],[],[],[]]);start=len(record[0])
        for j in range(rings+1):
            t=j/rings;y=bottom+(top-bottom)*t
            taper=max(.015,math.sin(t*math.pi*.5)**.7)if tip else .63+.37*math.sin(t*math.pi)
            for k in range(sides):
                a=k*math.tau/sides;r=taper*(1+.055*math.sin(t*19+seed+a*3))
                px=x+math.cos(a)*rx*r;pz=z+math.sin(a)*rz*r+.1*math.sin(t*7+seed)
                n=np.array([math.cos(a)/rx,0,math.sin(a)/rz]);n/=np.linalg.norm(n)
                record[0].append([px,y,pz]);record[1].append(n.tolist());record[2].append([k/sides,t*3])
        for j in range(rings):
            for k in range(sides):
                a=start+j*sides+k;b=start+j*sides+(k+1)%sides;c=a+sides;d=b+sides
                record[3].extend([(a,c,b),(b,c,d)])
        for j,up in [(0,False),(rings,True)]:
            center=len(record[0]);record[0].append([x,bottom if j==0 else top,z]);record[1].append([0,1 if up else-1,0]);record[2].append([.5,.5])
            for k in range(sides):
                a=start+j*sides+k;b=start+j*sides+(k+1)%sides
                record[3].append((center,b,a)if up else(center,a,b))
    def sheet(x,y):
        z=1.8+.31*math.sin(x*3.1+y*.04)+.19*math.sin(x*7.3-y*.09)
        z+=.24*math.sin(x*.7+y*.63)+.12*math.sin(x*1.8-y*1.12)
        z+=1.8*math.exp(-y*.23)+.5*math.sin(y*.15+x*.3)
        z-=max(0,y-30)*3.0 # bury the source fully inside the rock face
        return np.array([x,y,z])
    def width(y):return 7.4+2.7*math.exp(-y*.24)+.7*math.sin(y*.26)+.6*math.sin(y*.71)+.25*math.sin(y*1.32)
    record=m.parts.setdefault(ice,[[],[],[],[]]);cols=112;rows=180
    for j in range(rows+1):
        y=j/rows*35
        for k in range(cols+1):
            u=k/cols*2-1;x=u*width(y);p=sheet(x,y)
            dx=sheet(x+.01,y)-sheet(x-.01,y);dy=sheet(x,y+.01)-sheet(x,y-.01);n=np.cross(dx,dy);n/=np.linalg.norm(n)
            record[0].append(p.tolist());record[1].append(n.tolist());record[2].append([x*.32,-y*.16])
    for j in range(rows):
        for k in range(cols):
            a=j*(cols+1)+k;b=a+1;c=a+cols+1;d=c+1
            record[3].extend([(a,b,c),(b,d,c)])
    # Ice grows in irregular lobes at the foot, with a ragged fringe of short
    # tapered icicles along the upper overhangs rather than full-height pipes.
    rng=np.random.default_rng(12045)
    for i in range(25):
        x=float(rng.uniform(-8.5,8.5));y=float(rng.uniform(.7,3.1));p=sheet(x,y)
        tube(pale,x,float(p[2])-.2,.05,y,float(rng.uniform(.25,.8)),.6,i)
    for i in range(41):
        x=float(rng.uniform(-7.1,7.1));top=float(rng.uniform(24,31));p=sheet(x,top)
        tube(pale if i%4 else crack,x,float(p[2])+.04,top-float(rng.uniform(1.7,5.2)),top,float(rng.uniform(.07,.19)),.14,i+44,True)
    # Broken, branching frost fractures follow the actual surface relief.
    for i in range(13):
        x=float(rng.uniform(-7,7));start=float(rng.uniform(1,22));length=float(rng.uniform(3,10))
        for j in range(20):
            y=start+j*length/20;y1=y+length/20
            x0=x+math.sin(y*1.4+i)*.16;x1=x+math.sin(y1*1.4+i)*.16
            a,b=sheet(x0,y),sheet(x1,y1);a[2]+=.016;b[2]+=.016
            m.quad(crack,[a+[-.012,0,0],a+[.012,0,0],b+[.012,0,0],b+[-.012,0,0]])
    record=m.save('frozen_waterfall')
    trunk=mesh.Model();wood=trunk.material('Existing conifer trunk collision base',[.14,.12,.10],.96)
    trunk.box(wood,(-.28,0,-.28),(.28,4,.28));trunk.save('winter_trunk')
    return record

def build():
    records=trees()+[waterfall()]
    (PROJECT/'assets/winter-details.json').write_text(json.dumps(dict(version=1,trees='Winter variants of existing Poly Haven conifers; source geometry/alpha retained',waterfall='Original static frozen-cascade geometry, opaque PBR ice',records=records),indent=2)+'\n')
    print('Built snow conifers and frozen waterfall',flush=True)
if __name__=='__main__':build()
