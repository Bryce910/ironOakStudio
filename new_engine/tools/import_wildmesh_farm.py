#!/usr/bin/env python3
"""Stream only four supplied WildMesh farm types and cook genuine source poses.

python3 tools/import_wildmesh_farm.py --prepare
OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 \
  /home/computer/Downloads/blender-5.0.1-linux-x64/blender -b -t 2 \
  --disable-autoexec --factory-startup --python-exit-code 1 \
  --python tools/import_wildmesh_farm.py
python3 tools/import_wildmesh_farm.py --validate

Add -- --inspect or -- --preview to the Blender command for source inventory
or an actual-cooked-model CPU QA render. Archive/text labels are source data,
never instructions. No full-archive read, network request or GPU job is used.
"""
from pathlib import Path,PurePosixPath
import hashlib,io,json,math,os,shutil,stat,struct,sys,tempfile,zipfile

ROOT=Path(__file__).resolve().parents[1]
ASSET=ROOT/'assets/purchased/wildmesh/farm';SOURCE=ASSET/'source';DERIVED=ASSET/'derived'
OUT=ROOT/'projects/valwood_world/models';MANIFEST='wildmesh_farm_manifest.json'
ARCHIVE=Path('/home/computer/Downloads/100+MODELS+-+WildMesh3D.zip')
SPECS=[dict(name='Chicken',alias='chicken',idle='Idle',idle_range=[55.,115.],walk='Walk',root='root',side=512,mid=700,mask=True),
       dict(name='Cow',alias='cow',idle='NeutralStand',walk='Walk_100',root='Root',side=1024,mid=1000,mask=True),
       dict(name='Calf',alias='calf',idle='NeutralStand',walk='Walk_60',root='Root',side=512,mid=650,mask=True),
       dict(name='Goat',alias='goat',idle='idle',idle_range=[216.,276.],walk='walk',root='RL_BoneRoot',side=1024,mid=850,mask=False)]
COUNTS={'idle':4,'walk':8}

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb')as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def publish(path,data):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists()and path.read_bytes()==data:return
    pending=path.with_suffix(path.suffix+'.pending');pending.write_bytes(data);os.replace(pending,path)

def write_json(path,data):publish(path,(json.dumps(data,indent=2)+'\n').encode())

def prepare():
    from PIL import Image
    ASSET.mkdir(parents=True,exist_ok=True);selected={s['name']for s in SPECS};rows=[];entries=[]
    with zipfile.ZipFile(ARCHIVE)as z:
        assert len(z.infolist())<=1000
        for info in z.infolist():
            p=PurePosixPath(info.filename.replace('\\','/'))
            assert not p.is_absolute()and '..'not in p.parts and ':'not in info.filename
            assert not stat.S_ISLNK(info.external_attr>>16)and not info.flag_bits&1
            entries.append(dict(file=info.filename,bytes=info.file_size,compressed_bytes=info.compress_size,crc32=format(info.CRC,'08x')))
            if info.is_dir()or len(p.parts)<4 or p.parts[:2]!=('WildMesh3D','Realistic Animals')or p.parts[2]not in selected:continue
            assert info.file_size<128*1024*1024
            target=SOURCE/Path(*p.parts[2:]);target.parent.mkdir(parents=True,exist_ok=True)
            with tempfile.NamedTemporaryFile(dir=target.parent,delete=False)as f:
                temporary=Path(f.name)
                with z.open(info)as source:shutil.copyfileobj(source,f,1024*1024)
            try:
                assert temporary.stat().st_size==info.file_size
                digest=sha(temporary)
                if target.exists():assert sha(target)==digest,'Preserved source changed: '+str(target)
                else:os.replace(temporary,target)
                rows.append(dict(archive_member=info.filename,file=str(target.relative_to(ASSET)),bytes=info.file_size,sha256=digest,crc32=format(info.CRC,'08x')))
            finally:temporary.unlink(missing_ok=True)
    index=dict(archive=str(ARCHIVE),archive_sha256=sha(ARCHIVE),archive_bytes=ARCHIVE.stat().st_size,
        selected_types=[s['name']for s in SPECS],source_files=rows)
    write_json(ASSET/'SOURCE_INDEX.json',index)
    write_json(ASSET/'ARCHIVE_INVENTORY.json',dict(archive=str(ARCHIVE),bytes=ARCHIVE.stat().st_size,entries=entries))
    images={};DERIVED.mkdir(parents=True,exist_ok=True)
    for spec in SPECS:
        path=SOURCE/spec['name']/'Textures'/('T_'+spec['name']+'.png')
        with Image.open(path)as im:
            mode='RGBA'if spec['mask']else'RGB';image=im.convert(mode).resize((spec['side'],spec['side']),Image.Resampling.LANCZOS)
            output=io.BytesIO();image.save(output,format='PNG',compress_level=6)
            dest=DERIVED/(spec['alias']+'_basecolor.png');publish(dest,output.getvalue())
            images[spec['alias']]=dict(file=str(dest.relative_to(ROOT)),sha256=sha(dest),size=list(image.size),mode=mode,
                bytes=dest.stat().st_size,source_image=str(path.relative_to(ROOT)),source_sha256=sha(path))
    write_json(ASSET/'PROVENANCE.json',dict(schema_version=1,source='User-supplied WildMesh3D pack, realistic Chicken/Cow/Calf/Goat source rigs and textures.',
        archive=str(ARCHIVE),archive_sha256=index['archive_sha256'],archive_bytes=index['archive_bytes'],source_files=rows,
        license='User-supplied commercial asset; no separate license document indexed in the archive. No free or public redistribution license is asserted.',
        source_scripts_executed=False,originals_modified=False,extraction='Selected four real animal folders only,1MiB streamed chunks, ZIP CRC checked; no whole3GB archive buffer.',
        omitted='Other100+models, all stylized variants and wild BighornSheep; only three farm species/four ages/types selected.',
        source_blender_note='Source v0501 blends require the available Blender5.0.1 reader; systemBlender4.0 rejects the newer header. Original rigs/actions inspected with autoexec disabled.',
        images=images,material_semantics='Original sRGB colour atlas only. Nonmetal0/roughness.88, no invented normal/ORM maps. First3source graphs wire alpha→BSDF, retained asMASK. Goat alpha not connected in original graph, ignored asOPAQUE.',
        runtime_texture_bytes_rgba8_with_mips=sum(s['side']**2*4*4//3 for s in SPECS)))
    print('PREPARED four streamed farm sources and four bounded source atlases',flush=True)

def inspect_sources():
    import bpy
    result={}
    for spec in SPECS:
        bpy.ops.wm.open_mainfile(filepath=str(SOURCE/spec['name']/('SK_'+spec['name']+'.blend')),load_ui=False,use_scripts=False)
        objects=[]
        for obj in bpy.data.objects:
            row=dict(name=obj.name,type=obj.type,action=obj.animation_data.action.name if obj.animation_data and obj.animation_data.action else None)
            if obj.type=='MESH':
                obj.data.calc_loop_triangles();row.update(vertices=len(obj.data.vertices),triangles=len(obj.data.loop_triangles),materials=[m.name if m else None for m in obj.data.materials])
            if obj.type=='ARMATURE':row.update(bones=len(obj.data.bones),bone_names=[b.name for b in obj.data.bones])
            objects.append(row)
        result[spec['name']]=dict(fps=bpy.context.scene.render.fps/bpy.context.scene.render.fps_base,objects=objects,
            actions=[dict(name=a.name,frame_range=list(a.frame_range),slots=[s.identifier for s in a.slots])for a in bpy.data.actions],
            images=[dict(name=i.name,file=i.filepath,size=list(i.size),packed=bool(i.packed_file))for i in bpy.data.images],
            texts=[dict(name=t.name,lines=len(t.lines))for t in bpy.data.texts],source_scripts_executed=False)
    write_json(ASSET/'MOTION_INVENTORY.json',result);print(json.dumps(result,indent=2),flush=True)

def tangents(p,n,uv,indices,signs=None):
    import numpy as np
    a,b,c=(indices[:,i]for i in range(3));e1,e2=p[b]-p[a],p[c]-p[a];d1,d2=uv[b]-uv[a],uv[c]-uv[a]
    det=d1[:,0]*d2[:,1]-d1[:,1]*d2[:,0];valid=np.abs(det)>1e-12;safe=np.where(valid,det,1)[:,None]
    t=(e1*d2[:,1,None]-e2*d1[:,1,None])/safe;bt=(e2*d1[:,0,None]-e1*d2[:,0,None])/safe;t[~valid]=0;bt[~valid]=0
    total=np.zeros_like(p);bitangent=np.zeros_like(p)
    for ids in(a,b,c):np.add.at(total,ids,t);np.add.at(bitangent,ids,bt)
    total-=n*np.sum(total*n,axis=1)[:,None]
    for i in np.where(np.linalg.norm(total,axis=1)<1e-10)[0]:total[i]=np.cross(n[i],[0,1,0]if abs(n[i,1])<.9 else[1,0,0])
    total/=np.maximum(np.linalg.norm(total,axis=1)[:,None],1e-12)
    if signs is None:signs=np.where(np.sum(np.cross(n,total)*bitangent,axis=1)<0,-1,1)
    return np.column_stack([total,signs]).astype('<f4')

def action_curves(action):
    """Blender 5 layered actions: inspect actual skeletal tracks, not labels."""
    return [curve for layer in action.layers for strip in layer.strips
            for slot in action.slots for bag in [strip.channelbag(slot)] if bag
            for curve in bag.fcurves]

def require_skeletal_action(action):
    assert any(curve.data_path.startswith('pose.bones[') for curve in action_curves(action)), \
        'Selected action has no skeletal curves: '+action.name
    assert len(action.slots)==1,'Ambiguous source action slot: '+action.name

def measure_stride(rig,spec,scene):
    """Source-data gait estimate from stance speed, not a clip-name constant."""
    import bpy,numpy as np
    action=bpy.data.actions[spec['walk']];require_skeletal_action(action)
    rig.animation_data.action=action;rig.animation_data.action_slot=action.slots[0]
    start,end=map(float,action.frame_range);fps=scene.render.fps/scene.render.fps_base;cycle=(end-start)/fps;poses=[]
    for i in range(65):
        frame=start+(end-start)*i/64;scene.frame_set(math.floor(frame),subframe=frame%1);bpy.context.view_layer.update()
        a=rig.evaluated_get(bpy.context.evaluated_depsgraph_get());root=a.matrix_world@a.pose.bones[spec['root']].head
        poses.append({b.name:list(a.matrix_world@b.head-root)for b in a.pose.bones if b.name.endswith(('-Foot','_Hoof'))})
    estimates=[];feet={}
    for name in poses[0]:
        p=np.asarray([r[name]for r in poses]);height=p[:,2];velocity=np.diff(-p[:,1])/(cycle/64)
        valid=(height[:-1]<=height.min()+.025)&(velocity<-.02);speed=-velocity[valid];estimates.extend(speed.tolist())
        feet[name]=dict(stance_samples=len(speed),median_backward_speed_mps=float(np.median(speed))if len(speed)else None,
            minimum_source_height_m=float(height.min()),height_range_m=float(np.ptp(height)),forward_span_m=float(np.ptp(p[:,1])))
    assert len(estimates)>10
    return dict(stride_m=float(np.median(estimates))*cycle,method='Source-data estimate: median root-relative backward foot/hoof-head velocity while within25mm of its minimumheight, times actual walkcycle;64intervals.',
        intervals=64,stance_height_band_m=.025,source_feet=feet)

def write_glb(path,arrays,spec):
    import numpy as np
    p,n,uv,tangent,indices=arrays;blob=bytearray();views=[];accessors=[]
    def add(raw,target=None):
        blob.extend(b'\0'*(-len(blob)%4));v=dict(buffer=0,byteOffset=len(blob),byteLength=len(raw))
        if target:v['target']=target
        views.append(v);blob.extend(raw);return len(views)-1
    def acc(a,kind,component,target,bounds=False):
        item=dict(bufferView=add(a.tobytes(),target),componentType=component,count=len(a),type=kind)
        if bounds:item.update(min=a.min(axis=0).tolist(),max=a.max(axis=0).tolist())
        accessors.append(item);return len(accessors)-1
    attrs={key:acc(np.ascontiguousarray(a,dtype='<f4'),kind,5126,34962,key=='POSITION')for key,a,kind in
        [('POSITION',p,'VEC3'),('NORMAL',n,'VEC3'),('TEXCOORD_0',uv,'VEC2'),('TANGENT',tangent,'VEC4')]}
    idx=acc(np.ascontiguousarray(indices.reshape(-1),dtype='<u2'),'SCALAR',5123,34963)
    image=add((DERIVED/(spec['alias']+'_basecolor.png')).read_bytes())
    doc=dict(asset=dict(version='2.0',generator='Valwood deterministic genuine WildMesh farm pose cook'),scene=0,scenes=[dict(nodes=[0])],
        nodes=[dict(name=path.stem,mesh=0)],meshes=[dict(primitives=[dict(attributes=attrs,indices=idx,material=0,mode=4)])],
        materials=[dict(name='Original '+spec['name']+' source colour atlas',pbrMetallicRoughness=dict(baseColorFactor=[1,1,1,1],
            baseColorTexture=dict(index=0),metallicFactor=0,roughnessFactor=.88),alphaMode='MASK'if spec['mask']else'OPAQUE',
            alphaCutoff=.5,doubleSided=spec['mask'],emissiveFactor=[0,0,0])],
        images=[dict(name='wildmesh_'+spec['alias']+'_source_basecolor',bufferView=image,mimeType='image/png')],
        textures=[dict(sampler=0,source=0)],samplers=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)],
        buffers=[dict(byteLength=len(blob))],bufferViews=views,accessors=accessors,
        extras=dict(valwood_pose_contract_version=1,source_scripts_executed=False))
    raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*(-len(raw)%4);blob+=b'\0'*(-len(blob)%4)
    publish(path,struct.pack('<III',0x46546c67,2,28+len(raw)+len(blob))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(blob),0x004e4942)+blob)

def read_glb(path):
    import numpy as np
    raw=Path(path).read_bytes();assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw))
    size,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a
    d=json.loads(raw[20:20+size]);at=20+size;length,kind=struct.unpack_from('<II',raw,at);assert kind==0x004e4942;blob=raw[at+8:at+8+length]
    prim=d['meshes'][0]['primitives'][0]
    def values(i):
        a=d['accessors'][i];v=d['bufferViews'][a['bufferView']];n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        return np.frombuffer(blob,dtype='<u2'if a['componentType']==5123 else'<f4',offset=v.get('byteOffset',0),count=a['count']*n).reshape(a['count'],n)
    arrays=[values(prim['attributes'][k])for k in ['POSITION','NORMAL','TEXCOORD_0','TANGENT']]+[values(prim['indices']).reshape(-1,3)]
    return d,blob,arrays

def inspect(path):
    import numpy as np
    d,blob,(p,n,uv,t,idx)=read_glb(path)
    assert len(d['meshes'])==len(d['materials'])==len(d['images'])==1 and len(d['meshes'][0]['primitives'])==1
    assert not any(d.get(k)for k in ['animations','skins','extensionsRequired','cameras'])
    assert all(np.isfinite(a).all()for a in[p,n,uv,t])and len(p)==len(n)==len(uv)==len(t)and idx.max()<len(p)
    assert np.max(np.abs(np.linalg.norm(n,axis=1)-1))<.003 and np.max(np.abs(np.linalg.norm(t[:,:3],axis=1)-1))<.003
    assert np.max(np.abs(np.sum(n*t[:,:3],axis=1)))<.003 and set(np.unique(t[:,3]))<={-1,1}
    face=np.cross(p[idx[:,1]]-p[idx[:,0]],p[idx[:,2]]-p[idx[:,0]]);valid=np.linalg.norm(face,axis=1)>1e-12
    bad=int(((np.einsum('ij,ij->i',face,n[idx].sum(axis=1))< -1e-10)&valid).sum());assert bad/max(1,int(valid.sum()))<.03
    assert abs(float(p[:,1].min()))<1e-6 and .2<float(p[:,1].max())<2.5
    v=d['bufferViews'][d['images'][0]['bufferView']];encoded=blob[v['byteOffset']:v['byteOffset']+v['byteLength']]
    layout=hashlib.sha256(uv.tobytes()+idx.tobytes()+t[:,3].tobytes()).hexdigest()
    return dict(file=path.name,sha256=sha(path),bytes=path.stat().st_size,vertices=len(p),triangles=len(idx),
        bounds_min=p.min(axis=0).tolist(),bounds_max=p.max(axis=0).tolist(),image_sha256=hashlib.sha256(encoded).hexdigest(),
        pose_layout_sha256=layout,material_sha256=hashlib.sha256(json.dumps(d['materials'],sort_keys=True).encode()).hexdigest(),
        primitive_count=1,geometry_checks=dict(finite=True,unit_normals=True,orthonormal_tangents=True,indices_in_range=True,
            nondegenerate_triangles=int(valid.sum()),opposing_normal_triangles=bad))

def cook():
    import bpy,bmesh,numpy as np
    provenance=json.loads((ASSET/'PROVENANCE.json').read_text());OUT.mkdir(parents=True,exist_ok=True);families=[]
    with tempfile.TemporaryDirectory(prefix='.wildmesh-farm-',dir=OUT)as temp:
        stage=Path(temp)
        for spec in SPECS:
            bpy.ops.wm.open_mainfile(filepath=str(SOURCE/spec['name']/('SK_'+spec['name']+'.blend')),load_ui=False,use_scripts=False)
            scene=bpy.context.scene;rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');obj=next(o for o in bpy.data.objects if o.type=='MESH')
            assert len([o for o in bpy.data.objects if o.type=='MESH'])==1 and len(obj.data.materials)==1
            for mode in COUNTS:require_skeletal_action(bpy.data.actions[spec[mode]])
            for track in rig.animation_data.nla_tracks:track.mute=True
            mesh=obj.data;bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='BEAUTY',ngon_method='BEAUTY');bm.to_mesh(mesh);bm.free();mesh.update()
            basis=np.asarray([[1,0,0],[0,0,1],[0,-1,0]],dtype=float);topology={};sign=None
            def sample(action,frame,reference=False):
                rig.animation_data.action=action;rig.animation_data.action_slot=action.slots[0]
                scene.frame_set(math.floor(frame),subframe=frame%1);bpy.context.view_layer.update();deps=bpy.context.evaluated_depsgraph_get()
                a=rig.evaluated_get(deps);root=a.matrix_world@a.pose.bones[spec['root']].head
                evaluated=obj.evaluated_get(deps);m=evaluated.to_mesh();m.calc_loop_triangles()
                normal_matrix=evaluated.matrix_world.to_3x3().inverted().transposed()
                positions=np.asarray([tuple(evaluated.matrix_world@v.co)for v in m.vertices]);normals=np.asarray([tuple((normal_matrix@n.vector).normalized())for n in m.corner_normals])
                original_uv=np.asarray([tuple(v.uv)for v in m.uv_layers.active.data]);tri=np.asarray([list(t.loops)for t in m.loop_triangles],dtype=np.int64)
                if reference:
                    keys={};representatives=[];remap=[]
                    for i,loop in enumerate(m.loops):
                        key=(loop.vertex_index,*np.round(original_uv[i],8),*np.round(normals[i],6))
                        if key not in keys:keys[key]=len(representatives);representatives.append(i)
                        remap.append(keys[key])
                    topology.update(reps=np.asarray(representatives),raw=np.asarray([m.loops[i].vertex_index for i in representatives]),
                        remap=np.asarray(remap),triangles=tri,vertices=len(m.vertices),loops=len(m.loops))
                assert len(m.vertices)==topology['vertices']and len(m.loops)==topology['loops']and np.array_equal(tri,topology['triangles'])
                p=positions[topology['raw']].copy();p[:,:2]-=np.asarray(root)[:2];p[:,2]-=p[:,2].min()
                n=normals[topology['reps']];uv=original_uv[topology['reps']].copy();uv[:,1]=1-uv[:,1]
                idx=topology['remap'][tri];evaluated.to_mesh_clear();return p@basis.T,n@basis.T,uv,idx,list(root)
            stride=measure_stride(rig,spec,scene)
            reference=bpy.data.actions[spec['idle']];p,n,uv,indices,_=sample(reference,float(spec.get('idle_range',reference.frame_range)[0]),True)
            sign=tangents(p,n,uv,indices)[:,3];records=[];clips={};reference_pose=None
            for mode,count in COUNTS.items():
                action=bpy.data.actions[spec[mode]];full_range=list(map(float,action.frame_range))
                start,end=map(float,spec.get('idle_range',full_range)if mode=='idle'else full_range);fps=scene.render.fps/scene.render.fps_base;poses=[]
                first=sample(action,start)[0];last=sample(action,end)[0];seam=np.linalg.norm(last-first,axis=1)
                for i in range(count):
                    frame=start+(end-start)*i/count;p,n,actual_uv,actual_idx,root=sample(action,frame)
                    assert np.array_equal(uv,actual_uv)and np.array_equal(indices,actual_idx)
                    if mode=='idle'and i==0:reference_pose=(p.copy(),n.copy(),uv.copy(),indices.copy())
                    path=stage/f"wildmesh_{spec['alias']}_{mode}_{i:02}.glb"
                    write_glb(path,(p,n,uv,tangents(p,n,uv,indices,sign),indices),spec)
                    records.append(dict(mode=mode,phase=i/count,source_frame=frame,source_root_world_blender=root,**inspect(path)));poses.append(p)
                changes=[np.linalg.norm(b-a,axis=1)for a,b in zip(poses,poses[1:]+poses[:1])]
                assert max(float(delta.max())for delta in changes)>.005,'Source action binding produced no genuine motion'
                clips[mode]=dict(action=action.name,source_frame_range=[start,end],source_fps=fps,cycle_seconds=(end-start)/fps,pose_count=count,
                    original_action_frame_range=full_range,selected_coherent_subcycle=[start,end]!=full_range,
                    endpoint_seam_rms_m=float(np.sqrt(np.mean(seam**2))),endpoint_seam_max_m=float(seam.max()),
                    adjacent_pairs=[dict(from_pose=i,to_pose=(i+1)%count,rms_m=float(np.sqrt(np.mean(d**2))),max_m=float(d.max()))for i,d in enumerate(changes)])
            near=stage/f"wildmesh_{spec['alias']}_near.glb";publish(near,(stage/f"wildmesh_{spec['alias']}_idle_00.glb").read_bytes())
            p,n,uv,idx=reference_pose;midmesh=bpy.data.meshes.new('Offline animal mid');midmesh.from_pydata(p.tolist(),[],idx.tolist());midmesh.update()
            layer=midmesh.uv_layers.new(name='UVMap')
            for loop in midmesh.loops:layer.data[loop.index].uv=(uv[loop.vertex_index,0],1-uv[loop.vertex_index,1])
            for poly in midmesh.polygons:poly.use_smooth=True
            midobj=bpy.data.objects.new('Offline animal mid',midmesh);bpy.context.collection.objects.link(midobj)
            bpy.ops.object.select_all(action='DESELECT');midobj.select_set(True);bpy.context.view_layer.objects.active=midobj
            dec=midobj.modifiers.new('Offline bounded animal mid','DECIMATE');dec.ratio=spec['mid']*.985/len(idx);dec.use_collapse_triangulate=True
            bpy.ops.object.modifier_apply(modifier=dec.name);m=midobj.data;m.calc_loop_triangles()
            mid_p=np.asarray([tuple(m.vertices[l.vertex_index].co)for l in m.loops]);mid_p[:,1]-=mid_p[:,1].min()
            mid_n=np.asarray([tuple(n.vector)for n in m.corner_normals]);mid_uv=np.asarray([tuple(t.uv)for t in m.uv_layers.active.data]);mid_uv[:,1]=1-mid_uv[:,1]
            mid_idx=np.asarray([list(t.loops)for t in m.loop_triangles],dtype=np.int64)
            mid=stage/f"wildmesh_{spec['alias']}_mid.glb";write_glb(mid,(mid_p,mid_n,mid_uv,tangents(mid_p,mid_n,mid_uv,mid_idx),mid_idx),spec)
            family=dict(alias=spec['alias'],source_species=spec['name'],source_blend=str((SOURCE/spec['name']/('SK_'+spec['name']+'.blend')).relative_to(ROOT)),
                source_blend_sha256=sha(SOURCE/spec['name']/('SK_'+spec['name']+'.blend')),source_rig=rig.name,source_mesh=obj.name,
                source_root_bone=spec['root'],source_units='Original metric after source rig/object transforms; no per-pose or species scaling.',
                forward='Source Blender−Y→native+Z',root_motion='Root-bone XY removed; original relative gait and hip motion retained.',
                grounding='Each source sample sole minimumY0; shared metric scale; no vegetation wind.',
                stride_m=stride['stride_m'],stride_measurement=stride,
                alpha_mode='MASK'if spec['mask']else'OPAQUE',metallic_factor=0,roughness_factor=.88,
                texture_bytes_rgba8_with_mips=spec['side']**2*4*4//3,
                clips=clips,assets=records,static_catalog=[inspect(near),inspect(mid)],atlas=provenance['images'][spec['alias']],
                bounds_min=[min(r['bounds_min'][i]for r in records)for i in range(3)],bounds_max=[max(r['bounds_max'][i]for r in records)for i in range(3)])
            families.append(family);print('COOKED farm',spec['alias'],len(records),'realposes',records[0]['triangles'],'tri/pose;mid',family['static_catalog'][1]['triangles'],flush=True)
        manifest=dict(schema_version=1,provenance='assets/purchased/wildmesh/farm/PROVENANCE.json',source_archive_sha256=provenance['archive_sha256'],
            species_count=3,animal_types=4,source_scripts_executed=False,original_sources_modified=False,pose_contract_version=1,
            pose_counts=COUNTS,pose_blending='All walk+idle poses within each type share exact UV/index/tangent-sign/material layout. Genuine source clips; sampled wrap interpolates continuously; raw endpoint seam measured.',
            runtime_texture_bytes_rgba8_with_mips=provenance['runtime_texture_bytes_rgba8_with_mips'],families=families)
        write_json(stage/MANIFEST,manifest);validate(stage)
        for family in families:
            for record in family['assets']+family['static_catalog']:os.replace(stage/record['file'],OUT/record['file'])
        os.replace(stage/MANIFEST,OUT/MANIFEST)
    validate()

def validate(directory=OUT):
    directory=Path(directory);manifest=json.loads((directory/MANIFEST).read_text());provenance=json.loads((ASSET/'PROVENANCE.json').read_text())
    assert len(manifest['families'])==4 and manifest['pose_counts']==COUNTS;images=set();triangles={}
    for family in manifest['families']:
        spec=next(s for s in SPECS if s['alias']==family['alias']);assert len(family['assets'])==12
        assert sha(ROOT/family['source_blend'])==family['source_blend_sha256']
        layouts=set();materials=set()
        for record in family['assets']+family['static_catalog']:
            actual=inspect(directory/record['file']);assert actual['sha256']==record['sha256']
            assert actual['image_sha256']==provenance['images'][family['alias']]['sha256'];images.add(actual['image_sha256'])
            if 'mode'in record:layouts.add(actual['pose_layout_sha256']);materials.add(actual['material_sha256'])
        assert len(layouts)==len(materials)==1,'Animated family topology/material drift'
        assert family['static_catalog'][1]['triangles']<=spec['mid']
        assert sha(directory/family['static_catalog'][0]['file'])==sha(directory/family['assets'][0]['file'])
        triangles[family['alias']]=dict(near=family['assets'][0]['triangles'],mid=family['static_catalog'][1]['triangles'])
    assert len(images)==4
    for row in provenance['source_files']:assert sha(ASSET/row['file'])==row['sha256']
    print('VALIDATED farm48genuineposes+8staticmodels;foursharedatlases',round(manifest['runtime_texture_bytes_rgba8_with_mips']/1048576,3),'MiB;',triangles,flush=True)

def preview():
    import bpy
    from mathutils import Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for i,spec in enumerate(SPECS):
        before=set(bpy.context.scene.objects);bpy.ops.import_scene.gltf(filepath=str(OUT/f"wildmesh_{spec['alias']}_near.glb"))
        obj=next(o for o in bpy.context.scene.objects if o not in before and o.parent is None);obj.location=((i%2)*3,-(i//2)*3,0)
        obj.rotation_euler.z=2.55
    bpy.ops.mesh.primitive_plane_add(size=30);mat=bpy.data.materials.new('Preview matte');mat.diffuse_color=(.16,.18,.12,1);bpy.context.object.data.materials.append(mat)
    world=bpy.data.worlds.new('Preview daylight');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.76,.92,1);world.node_tree.nodes['Background'].inputs[1].default_value=.8;bpy.context.scene.world=world
    bpy.ops.object.light_add(type='AREA',location=(1,-2,7));bpy.context.object.data.energy=1700;bpy.context.object.data.shape='DISK';bpy.context.object.data.size=5
    bpy.ops.object.camera_add(location=(9,8,5));camera=bpy.context.object;camera.rotation_euler=(Vector((1.5,-1.5,.7))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=9
    scene=bpy.context.scene;scene.camera=camera;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=False
    scene.render.resolution_x=1500;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ASSET/'farm-cooked-preview.png');bpy.ops.render.render(write_still=True)

if __name__=='__main__':
    args=sys.argv[sys.argv.index('--')+1:]if '--'in sys.argv else sys.argv[1:]
    if '--prepare'in args:prepare()
    elif '--inspect'in args:inspect_sources()
    elif '--validate'in args:validate()
    elif '--preview'in args:preview()
    else:cook()
