"""Scalp tint for the civilian story hero. Head-skin texels that lie beneath the
hair cards (head vertex within 4-11 mm of a hair vertex, bind pose) are graded
toward the hair-root colour so gaps between strands read as dense hair, not
pale stubble. Face, ears and neck stay byte-identical in colour. Writes a new
character-quality root next to this file; the source build is untouched."""
import numpy as np,json,struct,io,shutil,hashlib,sys
from pathlib import Path
from PIL import Image,ImageFilter
from scipy.spatial import cKDTree
N=Path(__file__).resolve().parents[1]
SRC=Path('/home/computer/Desktop/final_engine/new_engine/build/thomas-character-finish-2026-11-21/assets/character-quality')
OUT=N/'character-quality'
def read(path):
    b=open(path,'rb').read();l=struct.unpack('<I',b[12:16])[0];j=json.loads(b[20:20+l])
    bl=struct.unpack('<I',b[20+l:24+l])[0];bo=28+l;return j,bytearray(b[bo:bo+bl])
def acc(j,bin_,i):
    a=j['accessors'][i];bv=j['bufferViews'][a['bufferView']]
    n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];dt={5126:np.float32,5125:np.uint32,5123:np.uint16,5121:np.uint8}[a['componentType']]
    assert bv.get('byteStride',n*np.dtype(dt).itemsize)==n*np.dtype(dt).itemsize
    arr=np.frombuffer(bytes(bin_),dtype=dt,count=a['count']*n,offset=bv.get('byteOffset',0)+a.get('byteOffset',0))
    return arr.reshape(a['count'],n) if n>1 else arr
def image(j,bin_,i):
    bv=j['bufferViews'][j['images'][i]['bufferView']];o=bv.get('byteOffset',0);return Image.open(io.BytesIO(bytes(bin_[o:o+bv['byteLength']])))
def mask_for(j,bin_,size):
    ps=j['meshes'][0]['primitives'];mats=[m['name'] for m in j['materials']]
    head=next(p for p in ps if mats[p['material']].endswith('Thomas_Head'));hair=next(p for p in ps if 'Hair' in mats[p['material']])
    hp=acc(j,bin_,head['attributes']['POSITION']);huv=acc(j,bin_,head['attributes']['TEXCOORD_0']);hi=acc(j,bin_,head['indices']).reshape(-1,3)
    hn=acc(j,bin_,head['attributes']['NORMAL']).astype(float)
    rp=acc(j,bin_,hair['attributes']['POSITION']).astype(float);tree=cKDTree(rp)
    # Covered: hair strands lie directly outward along the skin normal
    # (within 2.5 cm out, 4 mm lateral), not merely nearby at an edge.
    covered=np.zeros(len(hp),bool)
    for i,(p,n) in enumerate(zip(hp,hn/np.linalg.norm(hn,axis=1,keepdims=True))):
        hits=0
        for t in (.003,.008,.014,.020):
            if tree.query_ball_point(p+n*t,.004,return_length=True)>0:hits+=1
        covered[i]=hits>=3
    # Keep every visible edge (hairline, sideburns, nape) as authored: only
    # scalp at least 1 cm inside the covered region is graded, fully by 2.5 cm.
    inner,_=cKDTree(hp[~covered]).query(hp)
    inner[~covered]=0
    x=np.clip((inner-.010)/(.025-.010),0,1);w=x*x*(3-2*x)
    # The crown whorl parts the strands; grade the top-back skull beneath any
    # strand there too (face is +Z; the front hairline is never included).
    near,_=tree.query(hp)
    crown=np.clip((hp[:,1]-1.755)/.02,0,1)*np.clip((-hp[:,2]-.005)/.02,0,1)*(near<.010)
    w=np.maximum(w,crown)
    S=2048;mask=np.zeros((S,S),np.float32);uv=huv*S
    for tri in hi:
        ww=w[tri]
        if ww.max()<1e-3 or huv[tri][:,1].mean()>.80:continue        # ear islands
        p=uv[tri];x0,y0=np.maximum(np.floor(p.min(0)).astype(int),0);x1,y1=np.minimum(np.ceil(p.max(0)).astype(int),S-1)
        if x1<x0 or y1<y0:continue
        ys,xs=np.mgrid[y0:y1+1,x0:x1+1]+.5;(ax,ay),(bx,by),(cx,cy)=p
        den=(by-cy)*(ax-cx)+(cx-bx)*(ay-cy)
        if abs(den)<1e-9:continue
        l0=((by-cy)*(xs-cx)+(cx-bx)*(ys-cy))/den;l1=((cy-ay)*(xs-cx)+(ax-cx)*(ys-cy))/den;l2=1-l0-l1
        inside=(l0>=-.01)&(l1>=-.01)&(l2>=-.01);v=l0*ww[0]+l1*ww[1]+l2*ww[2]
        sub=mask[y0:y1+1,x0:x1+1];sub[inside]=np.maximum(sub[inside],v[inside])
    m=Image.fromarray((mask*255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(5)).resize(size,Image.LANCZOS)
    return np.asarray(m,np.float32)/255
def fix(src,dst):
    j,bin_=read(src);mats=[m['name'] for m in j['materials']]
    head_mat=j['materials'][mats.index(next(n for n in mats if n.endswith('Thomas_Head')))]
    hair_mat=j['materials'][mats.index(next(n for n in mats if 'Hair' in n))]
    hi_img=j['textures'][head_mat['pbrMetallicRoughness']['baseColorTexture']['index']]['source']
    hr_img=j['textures'][hair_mat['pbrMetallicRoughness']['baseColorTexture']['index']]['source']
    base_im=image(j,bin_,hi_img);base=np.asarray(base_im.convert('RGB'),np.float32)/255
    hair=np.asarray(image(j,bin_,hr_img).convert('RGBA'),np.float32)/255
    root=hair[...,:3][hair[...,3]>.6].mean(0)
    m=mask_for(j,bin_,base_im.size)
    ratio=(root*.80)/base[m>.8].mean(0);k=(m*.92)[...,None]
    out=Image.fromarray((np.clip(base*(1-k+k*ratio),0,1)*255+.5).astype(np.uint8))
    buf=io.BytesIO();out.save(buf,'JPEG',quality=95,subsampling=0);data=buf.getvalue()
    while len(bin_)%4:bin_.append(0)
    j['bufferViews'].append({'buffer':0,'byteOffset':len(bin_),'byteLength':len(data)});bin_+=data
    while len(bin_)%4:bin_.append(0)
    j['images'][hi_img]={'name':j['images'][hi_img].get('name','head')+'_scalp','mimeType':'image/jpeg','bufferView':len(j['bufferViews'])-1}
    j['buffers'][0]['byteLength']=len(bin_)
    js=json.dumps(j,separators=(',',':')).encode();js+=b' '*((4-len(js)%4)%4)
    total=12+8+len(js)+8+len(bin_)
    with open(dst,'wb') as f:
        f.write(struct.pack('<III',0x46546C67,2,total));f.write(struct.pack('<II',len(js),0x4E4F534A));f.write(js)
        f.write(struct.pack('<II',len(bin_),0x004E4942));f.write(bin_)
    print(dst,'scalp texels',int((m>.5).sum()),'root',root.round(3))
if OUT.exists():shutil.rmtree(OUT)
OUT.mkdir()
for f in SRC.iterdir():
    if f.is_file():(OUT/f.name).symlink_to(f)
for tier in ('Low','Medium','High','Ultra'):
    (OUT/tier/'story_hero').mkdir(parents=True)
    for kind in (SRC/tier/'story_hero').iterdir():
        if kind.name!='civilian':(OUT/tier/'story_hero'/kind.name).symlink_to(kind);continue
        d=OUT/tier/'story_hero'/'civilian';d.mkdir()
        for f in kind.iterdir():
            if f.name=='body.glb':fix(f,d/'body.glb')
            else:shutil.copy(f,d/f.name)
    for other in (SRC/tier).iterdir():
        if other.name!='story_hero':(OUT/tier/other.name).symlink_to(other)
