"""Entrance art: CC0 cliff scans, dense fir boughs, and closed sculpted ice.

All outputs are private to Grandstone. Original downloaded scans, character
poses, and the castle/jungle projects remain immutable.
"""
from pathlib import Path
import copy,hashlib,io,json,math,tempfile,subprocess
import numpy as np
from PIL import Image
from scipy.ndimage import map_coordinates
from compose_visual_upgrade import accessor,geometry,node_matrix
from build_cinematic_diversity import Writer

ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT/'projects/valwood_stronghold';OUT=PROJECT/'models/stronghold'
SOURCE=ROOT/'assets/polyhaven/stronghold_cliffs'

def winter(material,snow=0,ice=0,thickness=0):
    material.setdefault('extensions',{})['VALWOOD_materials_winter']={'parameters':[1,snow,ice,thickness]}
    return material

def write(name,parts,materials,textures=None,images=None):
    w=Writer(copy.deepcopy(materials),copy.deepcopy(textures or []),copy.deepcopy(images or []))
    w.g['extensionsUsed']=['VALWOOD_materials_winter']
    for i,(p,n,uv,idx,mat)in enumerate(parts):
        p=np.asarray(p);n=np.asarray(n);uv=np.asarray(uv);idx=np.asarray(idx,dtype=np.uint32)
        assert np.isfinite(p).all()and np.isfinite(n).all()and np.isfinite(uv).all()
        n=n/np.maximum(np.linalg.norm(n,axis=1,keepdims=True),1e-9)
        assert idx.max()<len(p)
        w.mesh(name+str(i),p,n,uv,idx,mat)
    target=OUT/(name+'.glb');temp=target.with_suffix('.cooking');w.save(temp,embedded=True);temp.replace(target)
    return dict(file=str(target.relative_to(ROOT)),triangles=sum(len(p[3])for p in parts),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest())

def cliffs():
    records=[]
    for asset,name in [('rock_face_01','cliff_layer_a'),('rock_face_02','cliff_layer_b'),('namaqualand_cliff_01','cliff_layer_c')]:
        folder=SOURCE/asset;d=json.loads((folder/(asset+'.gltf')).read_text());blob=(folder/d['buffers'][0]['uri']).read_bytes();parts=[]
        def visit(i,parent):
            node=d['nodes'][i];m=parent@node_matrix(node)
            if 'mesh'in node:
                for p in d['meshes'][node['mesh']]['primitives']:
                    a=p['attributes'];pos=accessor(d,blob,a['POSITION']);pos=(np.c_[pos,np.ones(len(pos))]@m.T)[:,:3]
                    n=accessor(d,blob,a['NORMAL'])@np.linalg.inv(m[:3,:3]);uv=accessor(d,blob,a['TEXCOORD_0']);idx=accessor(d,blob,p['indices']).reshape(-1,3)
                    if np.linalg.det(m[:3,:3])<0:idx=idx[:,[0,2,1]]
                    parts.append([pos,n,uv,idx,p.get('material',0)])
            for j in node.get('children',[]):visit(j,m)
        for i in d['scenes'][d.get('scene',0)]['nodes']:visit(i,np.eye(4))
        source_triangles=sum(len(p[3])for p in parts)
        if asset=='namaqualand_cliff_01':
            cache=SOURCE/'prepared';cache.mkdir(exist_ok=True)
            lod=cache/(asset+'-32.glb');receipt=lod.with_suffix('.json')
            digest=hashlib.sha256(blob).hexdigest()
            if not lod.exists()or not receipt.exists()or json.loads(receipt.read_text()).get('source_sha256')!=digest:
                subprocess.run(['blender','--background','--factory-startup','--python',str(ROOT/'tools/decimate_stronghold_cliff.py'),'--',str(folder/(asset+'.gltf')),str(lod)],check=True)
                receipt.write_text(json.dumps(dict(source_sha256=digest,ratio=.32))+'\n')
            _,_,reduced=geometry(lod);parts=[[p,n,uv,idx,0]for p,n,uv,idx,_ in reduced]
            assert 20000<sum(len(p[3])for p in parts)<32000
        pts=np.concatenate([p[0]for p in parts]);lo=pts.min(0);hi=pts.max(0)
        for p in parts:p[0]-=np.array([(lo[0]+hi[0])*.5,lo[1],hi[2]])
        materials=copy.deepcopy(d['materials'])
        for mat in materials:
            winter(mat,.93);mat['pbrMetallicRoughness']['baseColorFactor']=[.82,.87,.93,1]
        roles={}
        for mat in materials:
            for role,info in [('base',mat['pbrMetallicRoughness'].get('baseColorTexture')),('normal',mat.get('normalTexture')),('arm',mat['pbrMetallicRoughness'].get('metallicRoughnessTexture'))]:
                if info:roles[d['textures'][info['index']]['source']]=role
        with tempfile.TemporaryDirectory(prefix='grandstone-cliff-')as temp:
            images=[]
            for i,im in enumerate(d['images']):
                image=Image.open(folder/im['uri']).convert('RGB');role=roles.get(i,'base');size=2048 if role=='base'else 1024 if role=='normal'else 512
                image.thumbnail((size,size),Image.Resampling.LANCZOS);path=Path(temp)/(str(i)+'.png');image.save(path)
                images.append(dict(uri=str(path),mimeType='image/png'))
            record=write(name,parts,materials,d['textures'],images)
        record.update(source=asset,source_triangles=source_triangles,license='CC0-1.0',url='https://polyhaven.com/a/'+asset,source_bounds=[lo.tolist(),hi.tolist()]);records.append(record)
    return records

def append_part(parts,mat,p,n,uv,idx):
    record=parts.setdefault(mat,[[],[],[],[]]);offset=len(record[0]);record[0].extend(np.asarray(p).tolist());record[1].extend(np.asarray(n).tolist());record[2].extend(np.asarray(uv).tolist());record[3].extend((np.asarray(idx)+offset).tolist())

def tube(parts,mat,points,radii,sides=7):
    points=np.asarray(points);p=[];n=[];uv=[];faces=[]
    for j,c in enumerate(points):
        direction=points[min(j+1,len(points)-1)]-points[max(j-1,0)];direction/=np.linalg.norm(direction)
        a=np.cross(direction,[0,1,0])
        if np.linalg.norm(a)<.01:a=np.cross(direction,[1,0,0])
        a/=np.linalg.norm(a);b=np.cross(direction,a)
        for k in range(sides):
            axis=a*math.cos(k*math.tau/sides)+b*math.sin(k*math.tau/sides)
            p.append(c+axis*radii[j]);n.append(axis);uv.append([k/sides,j*.4])
    for j in range(len(points)-1):
        for k in range(sides):
            a=j*sides+k;b=j*sides+(k+1)%sides;c=a+sides;d=b+sides;faces.extend([(a,b,c),(b,d,c)])
    for end in [0,len(points)-1]:
        center=len(p);p.append(points[end]);direction=points[min(end+1,len(points)-1)]-points[max(end-1,0)];direction/=np.linalg.norm(direction)
        n.append(direction*(-1 if end==0 else 1));uv.append([.5,.5])
        for k in range(sides):faces.append((center,end*sides+k,end*sides+(k+1)%sides))
    # Correct outward orientation using the authored radial normals.
    p=np.asarray(p);n=np.asarray(n);faces=np.asarray(faces)
    c=np.cross(p[faces[:,1]]-p[faces[:,0]],p[faces[:,2]]-p[faces[:,0]])
    flip=np.sum(c*n[faces].mean(1),axis=1)<0;faces[flip]=faces[flip][:,[0,2,1]]
    append_part(parts,mat,p,n,uv,faces)

def firs():
    d,blob,source_parts=geometry(PROJECT/'models/cinematic_fir_near.glb');records=[]
    leaf_id=next(i for i,m in enumerate(d['materials'])if m.get('alphaMode')=='MASK')
    trunk_id=next(i for i,m in enumerate(d['materials'])if 'trunk'in m.get('name',''))
    wood_id=next(i for i,m in enumerate(d['materials'])if 'bark'in m.get('name',''))
    with tempfile.TemporaryDirectory(prefix='grandstone-fir-')as temp:
        images=[];roles={}
        for mat in d['materials']:
            for role,info in [('base',mat['pbrMetallicRoughness'].get('baseColorTexture')),('normal',mat.get('normalTexture')),('arm',mat['pbrMetallicRoughness'].get('metallicRoughnessTexture'))]:
                if info:roles[d['textures'][info['index']]['source']]=role
        for i,spec in enumerate(d['images']):
            v=d['bufferViews'][spec['bufferView']];raw=blob[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']];im=Image.open(io.BytesIO(raw)).convert('RGBA')
            size=1024 if roles.get(i)=='base'else 512;im.thumbnail((size,size),Image.Resampling.LANCZOS);path=Path(temp)/(str(i)+'.png');im.save(path);images.append(dict(uri=str(path),mimeType='image/png'))
        for name,height,radius,tiers in [('winter_pine',22.,4.5,15),('winter_sapling',4.4,1.25,9)]:
            for lod,clusters in [('near',10),('mid',4)]:
                rng=np.random.default_rng(88172);parts={};materials=copy.deepcopy(d['materials']);snow_id=len(materials)
                materials.append(winter(dict(name='Snow pillows on drooping fir boughs',pbrMetallicRoughness=dict(baseColorFactor=[.77,.84,.91,1],roughnessFactor=.88,metallicFactor=0)),0))
                materials[leaf_id]['alphaCutoff']=.28;materials[leaf_id]['doubleSided']=True
                materials[leaf_id]['pbrMetallicRoughness']['baseColorFactor']=[.68,.79,.79,1]
                # Keep the scanned trunk. Its former sparse upper canopy is
                # replaced with irregular whorls using the same scanned shoots.
                for p,n,uv,idx,mat in source_parts:
                    if mat==trunk_id:append_part(parts,mat,p*(height/22),n,uv,idx)
                for tier in range(tiers):
                    h=height*(.12+.82*tier/(tiers-1));length=radius*(1-h/height)**.7
                    branches=max(4,int(10-tier*.35));phase=float(rng.uniform(0,math.tau))
                    for branch in range(branches):
                        angle=phase+branch*math.tau/branches+float(rng.uniform(-.15,.15));out=np.array([math.cos(angle),0,math.sin(angle)]);side=np.array([-out[2],0,out[0]])
                        extent=length*float(rng.uniform(.78,1.18));drop=extent*float(rng.uniform(.11,.24));y=h+float(rng.uniform(-.018,.018))*height
                        axis=[np.array([0,y,0])+out*(extent*t)+np.array([0,-drop*t*t,0])for t in [0,.33,.67,1]]
                        r=height*.0032*(1-tier/(tiers+2));tube(parts,wood_id,axis,[r,r*.7,r*.4,r*.08])
                        for j in range(clusters):
                            t=(j+.8)/(clusters+.6);center=out*extent*t+np.array([0,y-drop*t*t,0]);width=extent*(.14+.13*(1-t));long=extent*.30
                            for offset in [-.65,0,.65]:
                                c=center+side*width*offset+np.array([0,float(rng.uniform(-.03,.03))*height,0]);across=side*width;along=out*long+np.array([0,long*.15,0])
                                for a,b in [(across,along),(across*.4+np.array([0,width*.7,0]),along)]:
                                    p=[c-a-b,c+a-b,c+a+b,c-a+b];normal=np.array([out[0]*.7,.6,out[2]*.7]);normal/=np.linalg.norm(normal)
                                    append_part(parts,leaf_id,p,[normal]*4,[[0,1],[1,1],[1,0],[0,0]],[[0,1,2],[0,2,3]])
                        # Snow has a real shaded cross-section, sagging with
                        # the branch rather than a whitened leaf texture.
                        if branch%3!=1 or tier<4:
                            p=[];normals=[];uv=[];faces=[];steps=8 if lod=='near'else 5
                            for j in range(steps+1):
                                t=.18+.76*j/steps;c=out*extent*t+np.array([0,y-drop*t*t+height*.003,0]);w=extent*.19*math.sin(math.pi*(j+.7)/(steps+1.4))**.7
                                for k in range(7):
                                    q=k/6*2-1;thickness=height*.010*(1-q*q)**.7
                                    p.append(c+side*w*q+[0,thickness,0]);nn=side*q*.8+[0,1,0];nn/=np.linalg.norm(nn);normals.append(nn);uv.append([t*3,q*.5+.5])
                            for j in range(steps):
                                for k in range(6):
                                    a=j*7+k;b=a+1;c=a+7;e=c+1;faces.extend([[a,b,c],[b,e,c]])
                            p=np.array(p);normals=np.array(normals);faces=np.array(faces);cross=np.cross(p[faces[:,1]]-p[faces[:,0]],p[faces[:,2]]-p[faces[:,0]])
                            if np.mean(cross[:,1])<0:faces=faces[:,[0,2,1]]
                            append_part(parts,snow_id,p,normals,uv,faces)
                result=write(name+'_'+lod,[[np.array(a)for a in record]+[mat]for mat,record in parts.items()],materials,d['textures'],images);result.update(source='fir_tree_01 scanned trunk and needles; original snow-laden branch assemblies',height_m=height);records.append(result)
    return records

def marching_tetra(field,origin,spacing):
    # Deterministic, offline iso-surface extraction, with no runtime dependency.
    corners=np.array([[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,0,1],[1,0,1],[1,1,1],[0,1,1]])
    tetra=[[0,5,1,6],[0,1,2,6],[0,2,3,6],[0,3,7,6],[0,7,4,6],[0,4,5,6]]
    edges=[[0,1],[1,2],[2,0],[0,3],[1,3],[2,3]]
    table=[[],[0,3,2],[0,1,4],[1,4,2,2,4,3],[1,2,5],[0,3,5,0,5,1],[0,2,5,0,5,4],[5,4,3],[3,4,5],[4,5,0,5,2,0],[1,5,0,5,3,0],[5,2,1],[3,4,2,2,4,1],[4,1,0],[2,3,0],[]]
    shape=np.array(field.shape)-1;grid=np.stack(np.meshgrid(*[np.arange(s)for s in shape],indexing='ij'),axis=-1).reshape(-1,3)
    values=np.stack([field[c[0]:c[0]+shape[0],c[1]:c[1]+shape[1],c[2]:c[2]+shape[2]].reshape(-1)for c in corners],axis=1)
    surface=[]
    for tet in tetra:
        v=values[:,tet];case=np.sum((v>0)*np.array([1,2,4,8]),axis=1)
        for k,ids in enumerate(table):
            if not ids:continue
            selected=case==k
            if not np.any(selected):continue
            g=grid[selected];vv=v[selected];vertices=[]
            for edge in ids:
                a,b=edges[edge];t=vv[:,a]/(vv[:,a]-vv[:,b]);c0=corners[tet[a]];c1=corners[tet[b]]
                vertices.append(g+c0+(c1-c0)*t[:,None])
            surface.append(np.stack(vertices,axis=1).reshape(-1,3))
    flat=np.concatenate(surface);unique,inverse=np.unique(np.round(flat,5),axis=0,return_inverse=True);faces=inverse.reshape(-1,3)
    gradients=np.gradient(field,*spacing);normals=-np.stack([map_coordinates(g,unique.T,order=1,mode='nearest')for g in gradients],axis=1);normals/=np.maximum(np.linalg.norm(normals,axis=1,keepdims=True),1e-8)
    pos=unique*np.array(spacing)+origin
    cross=np.cross(pos[faces[:,1]]-pos[faces[:,0]],pos[faces[:,2]]-pos[faces[:,0]]);flip=np.sum(cross*normals[faces].mean(1),axis=1)<0;faces[flip]=faces[flip][:,[0,2,1]]
    keep=np.linalg.norm(cross,axis=1)>.000001;return pos,normals,faces[keep]

def icefall():
    # Joined frozen flows have variable width, depth, undercuts and a curved
    # source. No single rectangle, copied full-height pipe, or painted sheet.
    spacing=np.array([.29,.29,.29]);origin=np.array([-11.,-1.,-14.])
    axes=[np.arange(a,b,s)for a,b,s in zip(origin,[11.,36.,8.],spacing)];x,y,z=np.meshgrid(*axes,indexing='ij');field=np.full(x.shape,-20.)
    for i,(cx,width,phase)in enumerate([(-3.8,2.6,.3),(.2,3.4,1.9),(4.1,2.2,3.1)]):
        center=cx+np.sin(y*.16+phase)*(.8+i*.13);cz=2.2+1.5*np.exp(-np.maximum(y,0)*.16)-np.maximum(y-22,0)*1.05
        rx=width*(.8+.18*np.sin(y*.43+phase)+.14*np.sin(y*.91+phase))
        rz=1.3+.33*np.sin(y*.38+phase)+.25*np.sin(y*.81+phase)
        value=1-((x-center)/rx)**2-((z-cz)/rz)**2
        value=np.minimum(value,np.minimum(y*.9,(33+i*.6-y)*.9));field=np.maximum(field,value)
    # Narrow, staggered frozen streams join the broad body and interrupt its
    # round silhouette. Their front ridges and grooves are actual geometry.
    rng=np.random.default_rng(74922)
    for i in range(23):
        cx=-6.8+i*.59+float(rng.uniform(-.12,.12));bottom=float(rng.uniform(.15,5));top=float(rng.uniform(26,34))
        center=cx+.24*np.sin(y*.22+i)
        cz=3.25+.6*np.exp(-np.maximum(y,0)*.12)-np.maximum(y-22,0)*1.05
        width=float(rng.uniform(.26,.52))*(.82+.18*np.sin(y*.58+i))
        value=1-((x-center)/width)**2-((z-cz)/.68)**2
        value=np.minimum(value,np.minimum((y-bottom)*1.4,(top-y)*1.4));field=np.maximum(field,value)
    # Fractured channels and compact bubbles create meso relief in the volume.
    field+=.13*np.sin(x*8+np.sin(y*.7)*.25)+.065*np.sin(x*15+y*.10)+.045*np.sin(y*2.3+x*.7)*np.cos(z*3.7)
    pos,normals,faces=marching_tetra(field,origin,spacing)
    pbr=lambda name,c,r,snow=0,ice=0,depth=0:winter(dict(name=name,pbrMetallicRoughness=dict(baseColorFactor=[*c,1],roughnessFactor=r,metallicFactor=0)),snow,ice,depth)
    mats=[pbr('Deep compact glacier ice',[.21,.39,.46],.23,.04,1,.8),pbr('Clear tapered icicles',[.36,.58,.64],.17,.02,1,.28),pbr('Packed snow on ice shelves',[.76,.84,.91],.86)]
    parts={};append_part(parts,0,pos,normals,np.c_[pos[:,0]*.3,-pos[:,1]*.15],faces)
    rng=np.random.default_rng(74922)
    for i in range(72):
        yy=float(rng.uniform(6,30));cx=float(rng.uniform(-6.4,6.4));zz=2.5+1.5*math.exp(-yy*.16)-max(yy-22,0)*1.05
        length=float(rng.uniform(.65,3.8));r=float(rng.uniform(.055,.17))
        tube(parts,1,[[cx,yy-length,zz],[cx+.04,yy-length*.55,zz+.08],[cx,yy,zz]],[.008,r*.6,r],9)
    return write('frozen_waterfall',[[np.array(a)for a in record]+[mat]for mat,record in parts.items()],mats)

def build():
    OUT.mkdir(parents=True,exist_ok=True);records=cliffs()+firs()+[icefall()]
    (PROJECT/'assets/entrance-art.json').write_text(json.dumps(dict(version=1,records=records,attribution='Cliff geometry and fir photographs: Poly Haven, CC0. Powered by Poly Haven. Snow boughs and ice volumes: original Valwood geometry.'),indent=2)+'\n')
    print('Built entrance cliff scans, dense snow firs and closed ice volumes',flush=True)
if __name__=='__main__':build()
