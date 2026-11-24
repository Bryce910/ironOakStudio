from pathlib import Path
import copy, hashlib, json, struct
import numpy as np

STAGE=Path(__file__).resolve().parents[1]
OUT=STAGE/'assets/road_life'
ROOT=STAGE.parents[1]

def sha(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def read_glb(path):
    raw=Path(path).read_bytes();magic,version,size=struct.unpack_from('<III',raw)
    assert magic==0x46546c67 and version==2 and size==len(raw)
    n,k=struct.unpack_from('<II',raw,12);assert k==0x4e4f534a
    d=json.loads(raw[20:20+n]);n2,k=struct.unpack_from('<II',raw,20+n);assert k==0x004e4942
    return d,raw[28+n:28+n+n2]

def accessor(d,b,i):
    a=d['accessors'][i];v=d['bufferViews'][a['bufferView']]
    typ={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']]
    n={'VEC2':2,'VEC3':3,'VEC4':4,'SCALAR':1}[a['type']]
    return np.ndarray((a['count'],n),dtype=typ,buffer=b,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',np.dtype(typ).itemsize*n),np.dtype(typ).itemsize)).copy()

def tangents(p,n,uv,ix,sign=None):
    tri=ix.reshape(-1,3);v=p[tri];u=uv[tri];e1=v[:,1]-v[:,0];e2=v[:,2]-v[:,0]
    a=u[:,1]-u[:,0];b=u[:,2]-u[:,0];den=a[:,0]*b[:,1]-a[:,1]*b[:,0]
    inv=np.divide(1.,den,out=np.zeros_like(den),where=np.abs(den)>1e-12)
    t=(e1*b[:,1,None]-e2*a[:,1,None])*inv[:,None]
    z=(e2*a[:,0,None]-e1*b[:,0,None])*inv[:,None]
    ts=np.zeros_like(p);bs=np.zeros_like(p)
    for k in range(3):np.add.at(ts,tri[:,k],t);np.add.at(bs,tri[:,k],z)
    ts-=n*np.sum(n*ts,axis=1)[:,None];length=np.linalg.norm(ts,axis=1)
    bad=length<1e-8
    if bad.any():
        fallback=np.cross(n[bad],np.where((np.abs(n[bad,1])<.9)[:,None],[0.,1.,0.],[1.,0.,0.]))
        ts[bad]=fallback;length=np.linalg.norm(ts,axis=1)
    ts/=length[:,None]
    if sign is None:sign=np.where(np.sum(np.cross(n,ts)*bs,axis=1)<0,-1.,1.)
    return np.column_stack((ts,sign)).astype('<f4')

def write_glb(path,primitives,material_pack,extras=None):
    d={'asset':{'version':'2.0','generator':'Valwood owned road life source cooker'},'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'mesh':0,'name':Path(path).stem}],'meshes':[{'name':Path(path).stem,'primitives':[]}],'buffers':[{}],'bufferViews':[],'accessors':[]}
    pack=copy.deepcopy(material_pack['gltf']);binary=bytearray()
    def view(data,target=None):
        binary.extend(b'\0'*((-len(binary))%4));v={'buffer':0,'byteOffset':len(binary),'byteLength':len(data)}
        if target:v['target']=target
        d['bufferViews'].append(v);binary.extend(data);return len(d['bufferViews'])-1
    def acc(array,kind,component=5126):
        array=np.asarray(array,dtype='<f4' if component==5126 else '<u4');v=view(array.tobytes(),34963 if component==5125 else 34962)
        a={'bufferView':v,'componentType':component,'count':len(array),'type':kind}
        if kind=='VEC3':a.update(min=array.min(0).tolist(),max=array.max(0).tolist())
        d['accessors'].append(a);return len(d['accessors'])-1
    for q in primitives:
        assert all(np.isfinite(q[k]).all() for k in ('p','n','uv','t'))
        assert np.max(np.abs(np.linalg.norm(q['n'],axis=1)-1))<1e-3
        attrs={s:acc(q[k],ty) for k,s,ty in [('p','POSITION','VEC3'),('n','NORMAL','VEC3'),('uv','TEXCOORD_0','VEC2'),('t','TANGENT','VEC4')]}
        # Original publisher wood uses its canonical RGB8 source paint, not
        # a guessed tint. Keep the graph's real per-vertex data after reindexing.
        for key,semantic,kind in [('uv1','TEXCOORD_1','VEC2'),('paint','_VALWOOD_SOURCE_PAINT','VEC3')]:
            if key in q:
                assert len(q[key])==len(q['p']) and np.isfinite(q[key]).all()
                attrs[semantic]=acc(q[key],kind)
        d['meshes'][0]['primitives'].append({'attributes':attrs,'indices':acc(q['ix'].ravel(),'SCALAR',5125),'material':q['material'],'mode':4})
    for image in pack.get('images',[]):
        data=Path(image.pop('source_path')).read_bytes();image['bufferView']=view(data)
    d.update(pack)
    if extras:d['extras']=extras
    d['buffers'][0]['byteLength']=len(binary)
    js=json.dumps(d,separators=(',',':')).encode();js+=b' '*((-len(js))%4);binary+=b'\0'*((-len(binary))%4)
    data=struct.pack('<III',0x46546c67,2,28+len(js)+len(binary))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(binary),0x004e4942)+binary
    Path(path).write_bytes(data)
    p=np.concatenate([q['p'] for q in primitives]);layout=hashlib.sha256()
    for q in primitives:
        layout.update(np.asarray(q['uv'],dtype='<f4').tobytes())
        layout.update(np.asarray(q['ix'],dtype='<u4').tobytes())
        layout.update(np.asarray(q['t'][:,3],dtype='<f4').tobytes());layout.update(str(q['material']).encode())
    return {'path':str(Path(path).relative_to(OUT)),'sha256':sha(path),'bytes':len(data),'vertices':len(p),'triangles':sum(q['ix'].size//3 for q in primitives),'primitives':len(primitives),'bounds':[p.min(0).tolist(),p.max(0).tolist()],'layout_sha256':layout.hexdigest()}
