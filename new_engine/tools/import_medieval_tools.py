#!/usr/bin/env python3
"""Preserve and cook the user's medieval hand-tool collection for Valwood.

    python3 tools/import_medieval_tools.py --prepare
    env OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 \
      --disable-autoexec --factory-startup --python-exit-code 1 \
      --python tools/import_medieval_tools.py
    python3 tools/import_medieval_tools.py --validate

Add -- --preview to the Blender command for an optional CPU-only QA image.
Embedded source text, links and material labels are data, never instructions.
The source scene is opened with script execution disabled. No source stage,
camera, lights, scene scripts, or unselected modern tools enter runtime GLBs.
"""
from pathlib import Path, PurePosixPath
import hashlib
import io
import json
import math
import os
import struct
import sys
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[1]
ASSET=ROOT/'assets/purchased/medieval_tools'
SOURCE=ASSET/'source'
DERIVED=ASSET/'derived'
OUT=ROOT/'projects/valwood_world/models'
ARCHIVE=Path('/home/computer/Downloads/medieval-tools-132c6b8e-c469-453d-8e38-78db4090a57c.zip')
BLEND='Publishing_Tools_v.0.2(1).blend'
MANIFEST='village_toolkit_manifest.json'
SIDE=1024
SCALE=.25
SPECS=[
    ('axe','Axe001',280),('chisel','Chisel001',150),
    ('claw_hammer','Claw Hammer001',400),('crowbar','Crowbar001',380),
    ('firepoker','Firepoker001',190),('hatchet','Hatchet001',410),
    ('hoe','Hoe001',310),('machete','Machete001',350),
    ('pickaxe','Pickaxe001',210),('pitchfork','Pitchfork001',410),
    ('scythe','Scythe001',240),('shovel','Shovel001',560),
]
MAPS=['basecolor','normal_opengl','orm']
FLAT={'axe','chisel','claw_hammer','hatchet','machete'}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def immutable_write(path,data):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists():assert path.read_bytes()==data, 'Preserved source changed: '+str(path)
    else:path.write_bytes(data)


def prepare():
    SOURCE.mkdir(parents=True,exist_ok=True)
    if ARCHIVE.exists():
        with zipfile.ZipFile(ARCHIVE)as archive:
            total=0
            for entry in archive.infolist():
                name=PurePosixPath(entry.filename.replace('\\','/'))
                assert not name.is_absolute()and '..'not in name.parts and ':'not in entry.filename
                assert (entry.external_attr>>16)&0o170000!=0o120000
                assert not entry.is_dir()and entry.file_size<100*1024*1024
                total+=entry.file_size
            assert total<128*1024*1024
            assert archive.namelist()==[BLEND], 'Unexpected archive contents; inspect before extending importer.'
            immutable_write(SOURCE/BLEND,archive.read(BLEND))
    assert (SOURCE/BLEND).is_file()
    print('PRESERVED source blend:',sha(SOURCE/BLEND),flush=True)


def open_source():
    import bpy
    prepare()
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/BLEND),load_ui=False,use_scripts=False)


def source_inventory():
    import bpy
    from mathutils import Vector
    records=[]
    for obj in sorted(bpy.data.objects,key=lambda o:o.name):
        record=dict(name=obj.name,type=obj.type,hidden=obj.hide_render,
            modifiers=[dict(name=m.name,type=m.type)for m in obj.modifiers])
        if obj.type=='MESH':
            obj.data.calc_loop_triangles()
            p=[obj.matrix_world@v.co for v in obj.data.vertices]
            lo=[min(v[i]for v in p)for i in range(3)];hi=[max(v[i]for v in p)for i in range(3)]
            record.update(vertices=len(p),triangles=len(obj.data.loop_triangles),
                bounds_min=lo,bounds_max=hi,dimensions=[hi[i]-lo[i]for i in range(3)],
                materials=[m.name if m else None for m in obj.data.materials],uv_layers=[u.name for u in obj.data.uv_layers])
        records.append(record)
    materials=[]
    for material in bpy.data.materials:
        materials.append(dict(name=material.name,nodes=[dict(name=n.name,type=n.type,
            image=n.image.name if n.type=='TEX_IMAGE'and n.image else None)for n in material.node_tree.nodes],
            links=[dict(source=l.from_node.name+'.'+l.from_socket.name,target=l.to_node.name+'.'+l.to_socket.name)
                for l in material.node_tree.links]))
    result=dict(schema_version=1,units=dict(system=bpy.context.scene.unit_settings.system,
        scale_length=bpy.context.scene.unit_settings.scale_length),objects=records,materials=materials,
        images=[dict(name=i.name,size=list(i.size),packed=bool(i.packed_file),colorspace=i.colorspace_settings.name)
            for i in bpy.data.images],embedded_texts=[dict(name=t.name,lines=len(t.lines))for t in bpy.data.texts])
    (ASSET/'INSPECTION.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def prepare_images():
    import bpy
    import numpy as np
    from PIL import Image
    material=bpy.data.materials['Tools'];nodes=material.node_tree.nodes
    linked={(l.from_node.name,l.from_socket.name,l.to_node.name,l.to_socket.name)for l in material.node_tree.links}
    assert ('Separate Color','Green','Principled BSDF','Roughness')in linked
    assert ('Separate Color','Blue','Principled BSDF','Metallic')in linked
    mapping=next(n for n in nodes if n.type=='MAPPING')
    assert list(mapping.inputs['Location'].default_value)==[0,0,0]
    assert list(mapping.inputs['Rotation'].default_value)==[0,0,0]
    assert list(mapping.inputs['Scale'].default_value)==[1,1,1]
    for name in ['Tools_albedo.jpg','Tools_normal.jpg','Tools_mixmap.jpg']:
        image=bpy.data.images[name];assert image.packed_file and list(image.size)==[4096,4096]
        immutable_write(SOURCE/'textures'/name,bytes(image.packed_file.data))
    DERIVED.mkdir(parents=True,exist_ok=True)
    def resized(name):
        return Image.open(SOURCE/'textures'/name).convert('RGB').resize((SIDE,SIDE),Image.Resampling.LANCZOS)
    base=resized('Tools_albedo.jpg');normal=np.asarray(resized('Tools_normal.jpg'),dtype=np.float32)/127.5-1
    normal/=np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),1e-6)
    normal=Image.fromarray(np.uint8(np.clip(np.rint((normal+1)*127.5),0,255)))
    # Source node graph proves G roughness/B metallic. R is not connected;
    # avoid assuming it is AO: preserve the raw mixmap and use neutral AO1.
    mix=resized('Tools_mixmap.jpg');_,rough,metal=mix.split()
    orm=Image.merge('RGB',(Image.new('L',(SIDE,SIDE),255),rough,metal))
    for name,image in zip(MAPS,[base,normal,orm]):image.save(DERIVED/(name+'.png'),compress_level=6)
    provenance=dict(schema_version=1,source='User-supplied medieval-tools ZIP; original authored source belongs to its respective creator.',
        source_archive=str(ARCHIVE),archive_sha256=sha(ARCHIVE)if ARCHIVE.exists()else None,
        license='User-supplied asset. Archive contains no separate license document; no public redistribution right or free license is asserted.',
        preservation='Original ZIP unchanged. Extracted blend and original packed JPG bytes preserved; source script execution disabled.',
        source_files=[dict(file=str(p.relative_to(ASSET)),bytes=p.stat().st_size,sha256=sha(p))for p in sorted(SOURCE.rglob('*'))if p.is_file()],
        derived_maps={name:dict(file=str((DERIVED/(name+'.png')).relative_to(ROOT)),size=[SIDE,SIDE],sha256=sha(DERIVED/(name+'.png')),
            bytes=(DERIVED/(name+'.png')).stat().st_size)for name in MAPS},
        material_semantics='Basecolor sRGB; OpenGL tangent normal linear, renormalized after resize; ORM linear, R255 neutralAO, Gsource roughness/Bsource metallic as connected in original graph. No emission/alpha/baked illumination.',
        modifications='Twelve source tool meshes isolated, original UVs/near smoothing retained; uniform0.25 metric scale; five tools baked flat, seven lean10degrees toward+Z; both LODs minimumY0/XZcentred. Shared4096px packed maps reduced to1024px; offline mid geometry decimation with compatible normals rebuilt.',
        omitted=['Screwdriver001','Tire Iron001','Iceaxe001','SM_stage_)1','source lights/camera/stage/world'])
    (ASSET/'PROVENANCE.json').write_text(json.dumps(provenance,indent=2)+'\n')


def read_glb(path):
    raw=Path(path).read_bytes();assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw))
    length,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a
    data=json.loads(raw[20:20+length]);at=20+length;length,kind=struct.unpack_from('<II',raw,at);assert kind==0x004e4942
    return data,raw[at+8:at+8+length]


def write_glb(path,data,binary):
    text=json.dumps(data,separators=(',',':')).encode();text+=b' '*(-len(text)%4);binary+=b'\0'*(-len(binary)%4)
    Path(path).write_bytes(struct.pack('<III',0x46546c67,2,28+len(text)+len(binary))+
        struct.pack('<II',len(text),0x4e4f534a)+text+struct.pack('<II',len(binary),0x004e4942)+binary)


def attach_material(path):
    data,binary=read_glb(path);output=bytearray(binary)
    data['images']=[];data['textures']=[]
    data['samplers']=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)]
    for i,name in enumerate(MAPS):
        output+=b'\0'*(-len(output)%4);encoded=(DERIVED/(name+'.png')).read_bytes();view=len(data['bufferViews'])
        data['bufferViews'].append(dict(buffer=0,byteOffset=len(output),byteLength=len(encoded)));output+=encoded
        data['images'].append(dict(name='village_toolkit_'+name,bufferView=view,mimeType='image/png'))
        data['textures'].append(dict(sampler=0,source=i))
    data['materials']=[dict(name='Original Tools shared PBR atlas',pbrMetallicRoughness=dict(
        baseColorFactor=[1,1,1,1],baseColorTexture=dict(index=0),metallicFactor=1,roughnessFactor=1,
        metallicRoughnessTexture=dict(index=2)),normalTexture=dict(index=1),occlusionTexture=dict(index=2),
        emissiveFactor=[0,0,0],alphaMode='OPAQUE',doubleSided=False)]
    for mesh in data['meshes']:
        for primitive in mesh['primitives']:primitive['material']=0
    data['buffers']=[dict(byteLength=len(output))];data.pop('extensionsUsed',None);data.pop('extensionsRequired',None)
    write_glb(path,data,bytes(output))


def inspect(path):
    import numpy as np
    from PIL import Image
    data,binary=read_glb(path)
    assert len(data['meshes'])==len(data['materials'])==1 and len(data['images'])==3
    assert not any(data.get(k)for k in ['skins','animations','cameras','extensionsRequired'])
    assert len(data['meshes'][0]['primitives'])==1
    primitive=data['meshes'][0]['primitives'][0];assert primitive.get('mode',4)==4
    def accessor(index):
        a=data['accessors'][index];v=data['bufferViews'][a['bufferView']]
        dtype=np.dtype({5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']]);n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        return np.ndarray((a['count'],n),dtype=dtype,buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),
            strides=(v.get('byteStride',n*dtype.itemsize),dtype.itemsize))
    p=accessor(primitive['attributes']['POSITION']);n=accessor(primitive['attributes']['NORMAL']);uv=accessor(primitive['attributes']['TEXCOORD_0'])
    indices=accessor(primitive['indices']).reshape(-1,3)
    assert len(p)==len(n)==len(uv)and all(np.isfinite(a).all()for a in [p,n,uv])and indices.max()<len(p)
    assert np.max(np.abs(np.linalg.norm(n,axis=1)-1))<.003
    faces=np.cross(p[indices[:,1]]-p[indices[:,0]],p[indices[:,2]]-p[indices[:,0]])
    valid=np.linalg.norm(faces,axis=1)>1e-11;assert valid.all(), 'Degenerate tool triangles'
    opposing=int((np.einsum('ij,ij->i',faces,n[indices].sum(axis=1))< -1e-9).sum())
    assert opposing/max(1,len(indices))<.025,(path,opposing,len(indices))
    for node in data['nodes']:
        assert not node.get('matrix')and node.get('translation',[0,0,0])==[0,0,0]and node.get('scale',[1,1,1])==[1,1,1]
    images=[]
    for record in data['images']:
        v=data['bufferViews'][record['bufferView']];encoded=binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
        image=Image.open(io.BytesIO(encoded));assert image.size==(SIDE,SIDE)
        images.append(dict(name=record['name'],size=list(image.size),sha256=hashlib.sha256(encoded).hexdigest(),bytes=len(encoded)))
    lo=p.min(axis=0);hi=p.max(axis=0)
    return dict(triangles=len(indices),vertices=len(p),primitive_count=1,material_count=1,
        bounds_min=lo.tolist(),bounds_max=hi.tolist(),dimensions_m=(hi-lo).tolist(),images=images,
        materials=data['materials'],bytes=path.stat().st_size,sha256=sha(path),geometry_checks=dict(
            finite=True,unit_normals=True,indices_in_range=True,nondegenerate_triangles=len(indices),opposing_normal_triangles=opposing))


def validate(directory=OUT):
    directory=Path(directory);manifest=json.loads((directory/MANIFEST).read_text());provenance=json.loads((ASSET/'PROVENANCE.json').read_text())
    assert len(manifest['assets'])==len(SPECS)*2
    expected=[provenance['derived_maps'][n]['sha256']for n in MAPS];images=set()
    for record in manifest['assets']:
        actual=inspect(directory/record['file']);assert actual['sha256']==record['sha256']
        assert actual['triangles']==record['triangles']and 0<actual['triangles']<=record['triangle_budget']
        lo,hi=actual['bounds_min'],actual['bounds_max']
        assert abs(lo[1])<.0001 and .01<hi[1]<2.1 and max(actual['dimensions_m'])>.2,(record['file'],lo,hi)
        assert abs(lo[0]+hi[0])<.025 and abs(lo[2]+hi[2])<.025
        assert [i['sha256']for i in actual['images']]==expected
        material=actual['materials'][0]
        assert material['pbrMetallicRoughness']['metallicRoughnessTexture']['index']==2 and material['normalTexture']['index']==1
        assert material['emissiveFactor']==[0,0,0]and material['alphaMode']=='OPAQUE'
        images.update(i['sha256']for i in actual['images'])
    for alias,_,_ in SPECS:
        near,mid=[a for a in manifest['assets']if a['alias']==alias]
        assert max(abs(near[k][i]-mid[k][i])for k in ['bounds_min','bounds_max']for i in range(3))<.025
        assert near['source_to_canonical_blender']==mid['source_to_canonical_blender']
    for original in provenance['source_files']:assert sha(ASSET/original['file'])==original['sha256']
    for image in provenance['derived_maps'].values():assert sha(ROOT/image['file'])==image['sha256']
    if ARCHIVE.exists():assert sha(ARCHIVE)==provenance['archive_sha256']
    assert len(images)==3
    budget=dict(tools=len(SPECS),models=len(SPECS)*2,unique_images=3,max_image_side=SIDE,
        estimated_rgba8_mips_mib=3*SIDE*SIDE*4*4/3/1048576,
        near_triangles=sum(r['triangles']for r in manifest['assets']if r['lod']=='near'),
        mid_triangles=sum(r['triangles']for r in manifest['assets']if r['lod']=='mid'),
        model_bytes=sum(r['bytes']for r in manifest['assets']))
    print('VALIDATED medieval tools:',json.dumps(budget),flush=True);return budget


def cook():
    import bpy
    from mathutils import Matrix,Vector
    open_source();inventory=source_inventory();prepare_images()
    originals={alias:bpy.data.objects[name].copy()for alias,name,_ in SPECS}
    for alias,name,_ in SPECS:
        originals[alias].data=bpy.data.objects[name].data.copy()
        assert not originals[alias].modifiers and originals[alias].data.uv_layers
    records=[];OUT.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.village-toolkit-',dir=OUT)as temporary:
        stage=Path(temporary)
        for alias,name,mid_budget in SPECS:
            source=originals[alias];points=[source.matrix_world@v.co for v in source.data.vertices]
            lo=Vector([min(p[i]for p in points)for i in range(3)]);hi=Vector([max(p[i]for p in points)for i in range(3)])
            origin=Vector(((lo.x+hi.x)*.5,(lo.y+hi.y)*.5,lo.z))
            angle=math.pi/2 if alias in FLAT else math.radians(10)
            pose=Matrix.Rotation(angle,4,'X')@Matrix.Scale(SCALE,4)@Matrix.Translation(-origin)
            rest=[pose@p for p in points]
            rest_lo=Vector([min(p[i]for p in rest)for i in range(3)]);rest_hi=Vector([max(p[i]for p in rest)for i in range(3)])
            rest_origin=Vector(((rest_lo.x+rest_hi.x)*.5,(rest_lo.y+rest_hi.y)*.5,rest_lo.z))
            canonical=Matrix.Translation(-rest_origin)@pose
            source.data.calc_loop_triangles();source_count=len(source.data.loop_triangles)
            for lod in ['near','mid']:
                bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
                obj=source.copy();obj.data=source.data.copy();bpy.context.collection.objects.link(obj)
                obj.parent=None;obj.matrix_world=canonical@source.matrix_world
                obj.data.materials.clear()
                for polygon in obj.data.polygons:polygon.material_index=0
                bpy.context.view_layer.objects.active=obj;obj.select_set(True)
                bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
                budget=mid_budget if lod=='mid'else source_count
                if lod=='mid':
                    modifier=obj.modifiers.new('Offline toolkit silhouette LOD','DECIMATE')
                    modifier.ratio=min(.99,mid_budget*.985/source_count);modifier.use_collapse_triangulate=True
                    bpy.ops.object.modifier_apply(modifier=modifier.name)
                    if obj.data.has_custom_normals:bpy.ops.mesh.customdata_custom_splitnormals_clear()
                # Decimation may shift extremal vertices by a fraction of a
                # millimetre. Re-ground and centre each cooked mesh so both
                # LODs sit exactly on a world floor or measured tabletop.
                p=[v.co for v in obj.data.vertices]
                low=Vector([min(v[i]for v in p)for i in range(3)]);high=Vector([max(v[i]for v in p)for i in range(3)])
                lod_shift=Vector((-(low.x+high.x)*.5,-(low.y+high.y)*.5,-low.z))
                obj.data.transform(Matrix.Translation(lod_shift))
                obj.name='village_toolkit_'+alias;path=stage/(obj.name+'_'+lod+'.glb')
                bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,
                    export_animations=False,export_cameras=False,export_lights=False,export_materials='NONE',export_apply=True)
                attach_material(path)
                record=dict(alias=alias,runtime_alias='village_toolkit_'+alias,lod=lod,file=path.name,source_object=name,
                    source_triangles=source_count,triangle_budget=budget,source_scale=SCALE,
                    source_bounds_min=list(lo),source_bounds_max=list(hi),source_origin_blender=list(origin),
                    source_to_canonical_blender=[list(row)for row in canonical],lod_origin_correction_blender_m=list(lod_shift),
                    rest_kind='flat_on_support'if alias in FLAT else 'lean_on_ground',
                    rest_rotation_blender_degrees=[90 if alias in FLAT else 10,0,0],
                    lean_toward=None if alias in FLAT else '+Z',**inspect(path))
                record['support_footprint_xz_m']=[record['bounds_min'][0],record['bounds_min'][2],record['bounds_max'][0],record['bounds_max'][2]]
                records.append(record);print('COOKED',path.name,record['triangles'],record['dimensions_m'],flush=True)
        manifest=dict(schema_version=1,provenance='assets/purchased/medieval_tools/PROVENANCE.json',
            inspection='assets/purchased/medieval_tools/INSPECTION.json',
            origin='Metres, Y up. Applied source transforms and uniform0.25 scale; XZbounds centre at0, minimumY0. Flat tools have source broad face upward; long tools lean10degrees towardlocal+Z. No world scene/stage.',
            material_note='Original shared Tools UV atlas. Three1024px embedded images byte-identical across24GLBs; base sRGB, normal/ORM linear; no emissive/alpha/baked light.',
            lod_note='Near retains source geometry. Offline mid decimation; same canonical transform, metric scale and grounding, maximum25mm bounds difference.',assets=records)
        (stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n');manifest['budget']=validate(stage)
        (stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n')
        for record in records:os.replace(stage/record['file'],OUT/record['file'])
        os.replace(stage/MANIFEST,OUT/MANIFEST)
    validate()


def preview():
    import bpy
    from mathutils import Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for i,(alias,_,_)in enumerate(SPECS):
        before=set(bpy.context.scene.objects)
        bpy.ops.import_scene.gltf(filepath=str(OUT/('village_toolkit_'+alias+'_near.glb')))
        roots=[o for o in bpy.context.scene.objects if o not in before and o.parent is None];assert len(roots)==1
        roots[0].location=((i%6)*1.15,-(i//6)*2.3,0)
    bpy.ops.mesh.primitive_plane_add(size=30,location=(3,-1,-.005));mat=bpy.data.materials.new('Preview stone');mat.diffuse_color=(.22,.23,.25,1)
    bpy.context.object.data.materials.append(mat)
    world=bpy.data.worlds.new('Tool QA daylight');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.76,.92,1)
    world.node_tree.nodes['Background'].inputs[1].default_value=.8;bpy.context.scene.world=world
    bpy.ops.object.light_add(type='AREA',location=(2,-3,7));light=bpy.context.object;light.data.energy=1500;light.data.shape='DISK';light.data.size=5
    bpy.ops.object.camera_add(location=(9,9,5.5));camera=bpy.context.object
    camera.rotation_euler=(Vector((2.9,-1.2,.8))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=9
    scene=bpy.context.scene;scene.camera=camera;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=False
    scene.render.resolution_x=1500;scene.render.resolution_y=900;scene.render.resolution_percentage=100
    scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ASSET/'cooked-toolkit-preview.png')
    bpy.ops.render.render(write_still=True);print('CPU preview actual toolkit near GLBs:',scene.render.filepath,flush=True)


if __name__=='__main__':
    args=sys.argv[sys.argv.index('--')+1:]if '--'in sys.argv else sys.argv[1:]
    if '--prepare'in args:prepare()
    elif '--validate'in args:validate()
    elif '--inspect'in args:open_source();source_inventory()
    elif '--preview'in args:preview()
    else:cook()
