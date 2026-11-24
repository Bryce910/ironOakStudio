#!/usr/bin/env python3
"""Cook actual purchased Orc cut surfaces and posed capsules from body exchange.

The FBX importer owns the body bank and exact UV/index/skin correspondence.
This offline tool only writes the separate anatomy directory and its receipts.
"""
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS','2')
from pathlib import Path
import argparse,hashlib,json,struct
import numpy as np
from scipy.spatial import ConvexHull

PARTS=['Head','Torso','LeftArm','RightArm','LeftHand','RightHand','LeftLeg','RightLeg']
HERE=Path(__file__).resolve().parent
def sha(path):
    with Path(path).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def fnv(raw):
    h=14695981039346656037
    for v in raw:h=((h^v)*1099511628211)&((1<<64)-1)
    return h
def read_glb(path):
    raw=Path(path).read_bytes();magic,version,length=struct.unpack_from('<III',raw)
    assert magic==0x46546c67 and version==2 and length==len(raw)
    at=12;doc=None;blob=None
    while at<len(raw):
        n,kind=struct.unpack_from('<II',raw,at);at+=8
        if kind==0x4e4f534a:doc=json.loads(raw[at:at+n])
        if kind==0x004e4942:blob=raw[at:at+n]
        at+=n
    assert doc is not None and blob is not None and len(doc['meshes'])==1
    def arr(accessor):
        a=doc['accessors'][accessor];v=doc['bufferViews'][a['bufferView']]
        cols={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        dtype=np.dtype({5121:'u1',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']])
        offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',dtype.itemsize*cols)
        return np.ndarray((a['count'],cols),dtype=dtype,buffer=blob,offset=offset,strides=(stride,dtype.itemsize)).copy()
    prims=[]
    for p in doc['meshes'][0]['primitives']:
        prims.append(dict(attrs={k:arr(i)for k,i in p['attributes'].items()},indices=arr(p['indices']).astype('<u4').reshape(-1,3),material=p.get('material',0)))
    return doc,prims
def write_cap(path,arrays):
    blob=bytearray();views=[];acc=[];primitives=[];materials=[]
    def add(values,typ):
        a=np.asarray(values,dtype='<u4'if typ=='SCALAR'else'<f4');blob.extend(b'\0'*(-len(blob)%4))
        views.append(dict(buffer=0,byteOffset=len(blob),byteLength=a.nbytes));blob.extend(a.tobytes())
        info=dict(bufferView=len(views)-1,componentType=5125 if typ=='SCALAR'else 5126,count=len(a),type=typ)
        if typ=='VEC3':info.update(min=a.min(0).tolist(),max=a.max(0).tolist())
        acc.append(info);return len(acc)-1
    for k,(pos,norm,uv,tangent,indices)in enumerate(arrays):
        attributes={name:add(a,typ)for name,a,typ in [('POSITION',pos,'VEC3'),('NORMAL',norm,'VEC3'),('TEXCOORD_0',uv,'VEC2'),('TANGENT',tangent,'VEC4')]}
        primitives.append(dict(attributes=attributes,indices=add(np.asarray(indices).reshape(-1),'SCALAR'),material=k))
        materials.append(dict(name='Owned Unity Orc Gnur actual cut '+PARTS[k],doubleSided=True,pbrMetallicRoughness=dict(baseColorFactor=[.22,.012,.019,1],metallicFactor=0,roughnessFactor=.65)))
    doc=dict(asset=dict(version='2.0',generator='Valwood actual Unity Gnur anatomy v2'),scene=0,scenes=[dict(nodes=[0])],nodes=[dict(mesh=0)],meshes=[dict(primitives=primitives)],materials=materials,accessors=acc,bufferViews=views,buffers=[dict(byteLength=len(blob))])
    j=json.dumps(doc,separators=(',',':')).encode();j+=b' '*(-len(j)%4);blob+=b'\0'*(-len(blob)%4)
    path.write_bytes(struct.pack('<III',0x46546c67,2,len(j)+len(blob)+28)+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(blob),0x004e4942)+blob)

def cut_loops(prims,mapping):
    loops=[[]for _ in PARTS];records=[]
    for k,part in enumerate(PARTS):
        groups={}
        for pi,(p,m)in enumerate(zip(prims,mapping)):
            if m['rigid']:continue
            # Skin seams across source material boundaries are welded only in
            # the exact rest-space position shared by their source vertices.
            adj,specs=groups.setdefault(m['family'],({},{}));w=m['semantic'][:,k]
            for tri in p['indices']:
                crossing=[]
                for i,j in zip(tri,np.roll(tri,-1)):
                    if(w[i]>.5)==(w[j]>.5):continue
                    t=float((.5-w[i])/(w[j]-w[i]));q=m['rest'][i]+t*(m['rest'][j]-m['rest'][i])
                    key=tuple(np.rint(q*1e6).astype(int));specs.setdefault(key,(pi,int(i),int(j),t));crossing.append(key)
                if len(crossing)==2 and crossing[0]!=crossing[1]:
                    a,b=crossing;adj.setdefault(a,set()).add(b);adj.setdefault(b,set()).add(a)
        removed=np.concatenate([m['rest'][m['semantic'][:,k]>.9]for m in mapping if not m['rigid'] and np.any(m['semantic'][:,k]>.9)])
        for family,(adj,specs)in groups.items():
            remaining=set(adj)
            while remaining:
                component=set();stack=[min(remaining)]
                while stack:
                    q=stack.pop()
                    if q in component:continue
                    component.add(q);stack.extend(adj[q]-component)
                remaining-=component
                assert all(len(adj[q])<=2 for q in component),(part,family,'branched source cut',[(q,len(adj[q]))for q in component if len(adj[q])>2][:8])
                ends=[q for q in component if len(adj[q])==1];start=min(ends or component);seq=[start];prev=None;cur=start
                while True:
                    candidates=sorted(adj[cur]-({prev}if prev is not None else set()))
                    if not candidates:break
                    nxt=candidates[0]
                    if nxt==start:break
                    assert nxt not in seq;seq.append(nxt);prev,cur=cur,nxt
                if len(seq)<3:continue
                specsring=[specs[q]for q in seq];rest=np.asarray(seq,dtype=float)/1e6;c=rest.mean(0)
                normal=np.cross(rest-c,np.roll(rest,-1,axis=0)-c).sum(0)
                if np.dot(normal,removed.mean(0)-c)<0:specsring.reverse();rest=rest[::-1]
                loops[k].append(specsring);records.append(dict(part=part,family=int(family),vertices=len(seq),original_open_path=bool(ends),closing_chord_m=float(np.linalg.norm(rest[0]-rest[-1]))if ends else 0))
        assert loops[k],(part,'No actual source cut contour')
    return loops,records

def bone_roles(names):
    def norm(n):return ''.join(c for c in n.lower()if c.isalnum())
    lookup={norm(n):i for i,n in enumerate(names)}
    aliases={'pelvis':['pelvis','hips','bip001pelvis'],'chest':['spine03','spine2','chest','spine02','spine'],'neck':['neck01','neck','neck1'],'head':['head']}
    for side,short in [('Left','l'),('Right','r')]:
        aliases[side+'Arm']=['upperarm'+short,side+'Arm','arm'+short,'bip001'+short+'upperarm']
        aliases[side+'ForeArm']=['lowerarm'+short,side+'ForeArm','forearm'+short]
        aliases[side+'Hand']=['hand'+short,side+'Hand','wrist'+short]
        aliases[side+'Index']=['index01'+short,side+'HandIndex1','index1'+short]
        aliases[side+'UpLeg']=['thigh'+short,side+'UpLeg','upleg'+short]
        aliases[side+'Leg']=['calf'+short,side+'Leg','leg'+short,'lowerleg'+short]
        aliases[side+'Foot']=['foot'+short,side+'Foot']
        aliases[side+'Toe']=['ball'+short,side+'ToeBase','toe'+short]
    roles={}
    for role,choices in aliases.items():
        hit=next((lookup[norm(n)]for n in choices if norm(n)in lookup),None)
        if hit is not None:roles[role]=hit
        elif not(role.endswith('Index')or role.endswith('Toe')):raise ValueError('Missing source bone role '+role+'; actual bones: '+str(names))
    return roles
def segment_distance(points,a,b):
    delta=b-a;t=np.clip(((points-a)@delta)/max(1e-15,float(delta@delta)),0,1)
    return np.linalg.norm(points-(a+t[:,None]*delta),axis=1)
def hierarchy_masks(names,parents,roles):
    """Actual source ancestry includes jaw/fingers/armor attachments correctly."""
    assert len(parents)==len(names) and all(-1<=int(p)<len(names)for p in parents)
    ancestors=[]
    for i in range(len(names)):
        chain=set();at=i
        while at>=0:
            assert at not in chain,('Cyclic source skeleton',names[i])
            chain.add(at);at=int(parents[at])
        ancestors.append(chain)
    torso=roles['chest']
    while int(parents[torso])>=0 and int(parents[torso])!=roles['pelvis']:
        torso=int(parents[torso])
    assert int(parents[torso])==roles['pelvis'],'Source spine must descend from actual pelvis'
    anchors=[roles['head'],torso,roles['LeftArm'],roles['RightArm'],roles['LeftHand'],roles['RightHand'],roles['LeftUpLeg'],roles['RightUpLeg']]
    masks=np.asarray([[anchor in family for anchor in anchors]for family in ancestors],dtype='<f4')
    assert np.all(masks[:,2]>=masks[:,4])and np.all(masks[:,3]>=masks[:,5])
    assert np.all(masks[:,1]>=masks[:,0])and np.all(masks[:,1]>=masks[:,2])and np.all(masks[:,1]>=masks[:,3])
    jaw=[i for i,n in enumerate(names)if n.lower().split(':')[-1]=='b_jaw']
    assert len(jaw)==1 and masks[jaw[0],0]==1,'Actual Gnur jaw must remain in detached head'
    return masks,anchors

def source_semantics(weights,names,masks,rigid):
    """Cumulative cut masks from actual normalized source bone weights."""
    assert weights.ndim==2 and weights.shape[1]==len(names)and np.isfinite(weights).all()and weights.min()>=0 and weights.max()<=1.00001
    result=np.zeros((len(weights),8),dtype='<f4')
    if rigid:return result
    result=np.clip(weights@masks,0,1)
    assert np.all(result[:,2]+1e-5>=result[:,4])and np.all(result[:,3]+1e-5>=result[:,5])
    return result
def fit_radius(cloud,a,b):
    assert len(cloud)>=4
    return float(np.clip(np.quantile(segment_distance(cloud,a,b),.80),.025,.65))
def capsules_for(pos,mapping,joints,roles):
    def J(n):return joints[roles[n]]
    def cloud(k,exclude=()):
        rows=[]
        for pi,m in enumerate(mapping):
            if m['rigid']:continue
            keep=m['semantic'][:,k]>.5
            for other in exclude:keep&=m['semantic'][:,other]<.5
            if keep.any():rows.append(pos[pi][keep])
        assert rows,(k,'No source flesh points');return np.concatenate(rows)
    rows=[]
    def add(part,a,b,c):rows.append([PARTS.index(part)+1,*np.asarray(a,dtype=float),*np.asarray(b,dtype=float),fit_radius(c,np.asarray(a),np.asarray(b))])
    h=cloud(0);axis=J('head')-J('neck');axis/=max(1e-12,np.linalg.norm(axis));center=np.median(h,axis=0)
    longitudinal=(h-center)@axis;half=float(max(.02,(np.quantile(longitudinal,.85)-np.quantile(longitudinal,.15))*.22))
    add('Head',center-half*axis,center+half*axis,h)
    torso=cloud(1,exclude=(0,2,3,4,5));add('Torso',J('pelvis')+(J('chest')-J('pelvis'))*.12,J('chest'),torso)
    for side,arm,hand,leg in [('Left',2,4,6),('Right',3,5,7)]:
        h=cloud(hand);endpoint=J(side+'Index')if side+'Index'in roles else np.median(h,axis=0)
        add(side+'Hand',J(side+'Hand'),endpoint,h)
        for part,k,bones in [(side+'Arm',arm,[side+'Arm',side+'ForeArm',side+'Hand']),(side+'Leg',leg,[side+'UpLeg',side+'Leg',side+'Foot']+([side+'Toe']if side+'Toe'in roles else[]))]:
            c=cloud(k,exclude=(hand,)if k==arm else());segments=[(J(a),J(b))for a,b in zip(bones,bones[1:])]
            distances=np.stack([segment_distance(c,a,b)for a,b in segments],axis=1);owners=np.argmin(distances,axis=1)
            for index,(a,b)in enumerate(segments):
                subset=c[owners==index];add(part,a,b,subset if len(subset)>=4 else c)
    assert 1<=len(rows)<=16
    return rows

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--body',type=Path,default=HERE/'arena_orcs/body');parser.add_argument('--exchange',type=Path);parser.add_argument('--output',type=Path,default=HERE/'arena_orcs/anatomy');parser.add_argument('--update-presentation',action='store_true');args=parser.parse_args()
    exchange=args.exchange or args.body/'body-semantic.npz';out=args.output
    assert exchange.is_file(),'Source body-semantic.npz exchange is required'
    assert not out.exists(),'Never overwrite a reviewed anatomy bank; choose a fresh output directory'
    manifest_path=args.body/'combat_motion_manifest.json'
    body_manifest=json.loads(manifest_path.read_text())
    origin_policy=body_manifest.get('pose_origin_policy','legacy_floor_before_108')
    assert origin_policy in ['preserve','legacy_floor_before_108'],('Unsupported source origin policy',origin_policy)
    doc,prims=read_glb(args.body/'combat_body_000.glb');data=np.load(exchange,allow_pickle=False);mapping=[]
    names=[str(n)for n in data['bone_names']];roles=bone_roles(names);alljoints=np.asarray(data['joints'],dtype=float)
    assert 'bone_parent_indices'in data,'Actual source bone parent hierarchy is required'
    parents=np.asarray(data['bone_parent_indices'],dtype=np.int64);masks,anchors=hierarchy_masks(names,parents,roles)
    assert alljoints.shape==(172,len(names),3)and np.isfinite(alljoints).all()
    for pi,p in enumerate(prims):
        rest=np.asarray(data[f'rest_{pi}'],dtype=float);rigid=bool(np.asarray(data[f'rigid_{pi}']).reshape(-1)[0])
        assert f'weights_{pi}'in data,'Exact source bone weights are required for cumulative arm/hand and torso cut labels'
        semantic=source_semantics(np.asarray(data[f'weights_{pi}'],dtype='<f4'),names,masks,rigid)
        assert rest.shape==p['attrs']['POSITION'].shape and semantic.shape==(len(rest),8)and np.isfinite(rest).all()and np.isfinite(semantic).all()and semantic.min()>=0 and semantic.max()<=1
        assert np.array_equal(np.asarray(data[f'uv_{pi}'],dtype='<f4'),p['attrs']['TEXCOORD_0'])and np.array_equal(np.asarray(data[f'indices_{pi}'],dtype='<u4').reshape(-1,3),p['indices'])
        if rigid:assert not semantic.any(),'Held rigid weapon cannot be severed as flesh'
        family=int(np.asarray(data[f'family_{pi}']).reshape(-1)[0])if f'family_{pi}'in data else pi
        mapping.append(dict(rest=rest,semantic=semantic,rigid=rigid,family=family))
    assert 0<len(prims)<=32 and sum(len(m['rest'])for m in mapping)<=100000
    loops,contours=cut_loops(prims,mapping)
    before={str(exchange.resolve()):sha(exchange),str(Path(__file__).resolve()):sha(__file__),str(manifest_path.resolve()):sha(manifest_path)}
    for pose in range(172):p=args.body/f'combat_body_{pose:03}.glb';before[str(p.resolve())]=sha(p)
    meta=bytearray(b'VWANA002'+struct.pack('<III',172,len(prims),8));layout=[]
    for pi,(p,m)in enumerate(zip(prims,mapping)):
        key=fnv(p['attrs']['TEXCOORD_0'].astype('<f4').tobytes()+p['indices'].astype('<u4').tobytes());meta+=struct.pack('<IQ',len(m['semantic']),key)+m['semantic'].tobytes();layout.append(dict(mesh=pi,vertices=len(m['semantic']),key=str(key),rigid=m['rigid']))
    (out/'caps').mkdir(parents=True);pose_rows=[];idle_height=0.;idle_radius=0.
    for pose in range(172):
        path=args.body/f'combat_body_{pose:03}.glb';d,ps=read_glb(path);assert len(ps)==len(prims)
        for a,b in zip(ps,prims):assert np.array_equal(a['indices'],b['indices'])and np.array_equal(a['attrs']['TEXCOORD_0'],b['attrs']['TEXCOORD_0'])
        # Preserve the supplied source frame when the native opponent loader
        # does the same, so airborne run phases retain their actual height.
        floor=float(min(p['attrs']['POSITION'][:,1].min()for p in ps))if origin_policy=='legacy_floor_before_108'and pose<108 else 0.
        pos=[p['attrs']['POSITION'].astype(float)-[0,floor,0]for p in ps];joints=alljoints[pose]-[0,floor,0];arrays=[];wounds=[]
        preferred=['neck','pelvis','LeftArm','RightArm','LeftHand','RightHand','LeftUpLeg','RightUpLeg']
        for k in range(8):
            points=[];normals=[];uv=[];tangents=[];indices=[];candidates=[]
            for ring in loops[k]:
                q=np.asarray([pos[pi][i]+t*(pos[pi][j]-pos[pi][i])for pi,i,j,t in ring]);c=q.mean(0);nn=np.cross(q-c,np.roll(q,-1,axis=0)-c).sum(0);nn/=max(1e-12,np.linalg.norm(nn))
                assert np.linalg.norm(nn)>.9,(pose,k,'Collapsed cut surface')
                tt=np.cross([0,1,0]if abs(nn[1])<.9 else[1,0,0],nn);tt/=max(1e-12,np.linalg.norm(tt));start=len(points)
                points.extend(q);points.append(c);normals.extend([nn]*(len(q)+1));uv.extend([[0,0]]*(len(q)+1));tangents.extend([[*tt,1]]*(len(q)+1))
                indices.extend([[start+len(q),start+i,start+(i+1)%len(q)]for i in range(len(q))]);candidates.append((float(np.linalg.norm(c-joints[roles[preferred[k]]])),c,nn))
            arrays.append((points,normals,uv,tangents,indices));_,c,nn=min(candidates,key=lambda x:x[0]);wounds.append([*c,*nn])
        write_cap(out/f'caps/cap_{pose:03}.glb',arrays);caps=capsules_for(pos,mapping,joints,roles)
        if pose<4:
            idle_height=max(idle_height,max(float(pos[pi][m['semantic'][:,0]>.5,1].max())for pi,m in enumerate(mapping)if not m['rigid']and np.any(m['semantic'][:,0]>.5)))
            torso=next(c for c in caps if c[0]==2)
            idle_radius=max(idle_radius,float(torso[-1])+max(float(np.hypot(torso[1],torso[3])),float(np.hypot(torso[4],torso[6]))))
        meta+=np.asarray(wounds,dtype='<f4').tobytes()+struct.pack('<I',len(caps))
        for row in caps:meta+=struct.pack('<I7f',int(row[0]),*row[1:])
        hull_counts=[]
        for k in range(8):
            clouds=[pos[pi][m['semantic'][:,k]>.5]for pi,m in enumerate(mapping)if not m['rigid'] and np.any(m['semantic'][:,k]>.5)]
            clouds.append(np.asarray(arrays[k][0]));cloud=np.unique(np.round(np.concatenate(clouds),8),axis=0)
            try:hull=cloud[ConvexHull(cloud).vertices]
            except Exception:hull=cloud
            assert 4<=len(hull)<=2048,(pose,k,len(hull));meta+=struct.pack('<I',len(hull))+hull.astype('<f4').tobytes();hull_counts.append(len(hull))
        pose_rows.append(dict(pose=pose,loader_floor=floor,wounds=wounds,capsules=caps,hull_points=hull_counts))
        if pose%24==0:print('ORC ANATOMY',pose,flush=True)
    assert before=={p:sha(p)for p in before},'Body exchange changed during anatomy cook'
    (out/'anatomy.bin').write_bytes(meta);(out/'ANATOMY_LANDMARKS.json').write_text(json.dumps(pose_rows,indent=2)+'\n')
    files={str(p.resolve()):sha(p)for p in sorted(out.rglob('*'))if p.is_file()}
    profile=dict(basis='Maximum of the four actual idle meshes and fitted source torso capsules in loader-local metres.',body_height_m=float(np.ceil(idle_height*100)/100),body_radius_m=float(np.ceil(idle_radius*100)/100),movement_radius_m=float(np.ceil((idle_radius+.04)*100)/100))
    presentation_update=None
    if args.update_presentation:
        cfg=args.body/'combat_presentation.cfg';old_hash=sha(cfg);tokens=cfg.read_text().split()
        assert tokens[:2]==['VALWOOD_COMBAT_PRESENTATION','1']and tokens[-1]=='end'
        for field in ['body_radius_m','body_height_m']:
            assert tokens.count(field)==1
            at=tokens.index(field);tokens[at+1]=format(profile[field],'.2f')
        cfg.write_text(' '.join(tokens)+'\n');files[str(cfg.resolve())]=sha(cfg)
        presentation_update=dict(path=str(cfg.resolve()),before_sha256=old_hash,after_sha256=sha(cfg),measured_profile=profile,preserves_source_walk_stride=True)
    proof=dict(schema='valwood.unity-orc.gnur-source-anatomy.v2',source_model='Gnur',ready=True,passed=True,poses=172,parts=PARTS,body_directory=str(args.body.resolve()),directory=str(out.resolve()),exact_source_layout=layout,bone_roles={k:names[v]for k,v in roles.items()},source_hierarchy_parts={part:[n for i,n in enumerate(names)if masks[i,k]]for k,part in enumerate(PARTS)},jaw_in_head=True,armor_attachments_follow_actual_parent=True,contours=contours,collision_profile=profile,dependencies_sha256=before,outputs_sha256=files,
        limitations=['Per-pose source skeletal capsules approximate the actual imported Orc volume; body labels never derive from attack direction or a human model.','Layered source clothing may contain open cut contours; closing chords are recorded.','Full native bank loading and combat/visual checks must accept these actual source outputs before Orc selection is released.'])
    proof['presentation_profile_update']=presentation_update
    proof['pose_origin_policy']=origin_policy
    (out/'ANATOMY_READY.json').write_text(json.dumps(proof,indent=2,sort_keys=True)+'\n');print(json.dumps(dict(ready=True,bytes=len(meta),receipt=str(out/'ANATOMY_READY.json'))),flush=True)
if __name__=='__main__':main()
