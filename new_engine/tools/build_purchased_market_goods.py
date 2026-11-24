#!/usr/bin/env python3
"""Cook user-supplied purchased market props for Valwood's native renderer.

  python3 tools/build_purchased_market_goods.py --prepare
  OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --factory-startup \
    --python-exit-code 1 --python tools/build_purchased_market_goods.py
  python3 tools/build_purchased_market_goods.py --validate

Only selected original GLBs are staged. Downloads remain untouched. Image
resizing and offline mesh LODs avoid importing the source's 50k-triangle props
and 4K images directly into the running market. No source scripts are executed.
"""
from pathlib import Path
import copy
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
SOURCE=ROOT/'assets/purchased/market_goods'
OUT=ROOT/'projects/valwood_world/models'
DOWNLOADS=Path('/home/computer/Downloads')
FOOD='medieval-tavern-props-pack-35-assets-fantasy-food-8-props-pack(1).zip'
DRINKS='medieval-tavern-props-pack-35-assets-fantasy-tavern-drinks-tableware-props(1).zip'
GOODS=[dict(alias='bread',archive=FOOD,member_name='Bread Loaf.glb',horizontal_m=.42),
       dict(alias='cheese',archive=FOOD,member_name='cheese wheel.glb',horizontal_m=.30),
       dict(alias='chicken',archive=FOOD,member_name='ROASTED CHICKEN.glb',horizontal_m=.40),
       dict(alias='tankard',archive=DRINKS,member_name='Wooden Beer Tankard.glb',height_m=.24)]
TARGETS={'near':3900,'mid':950}
MANIFEST='purchased_market_goods_manifest.json'


def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()


def glb(path):
    raw=Path(path).read_bytes()
    assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw)),path
    length,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a
    data=json.loads(raw[20:20+length]);at=20+length
    length,kind=struct.unpack_from('<II',raw,at);assert kind==0x004e4942
    return data,raw[at+8:at+8+length]


def write_glb(path,data,binary):
    encoded=json.dumps(data,separators=(',',':')).encode()
    encoded+=b' '*((-len(encoded))%4);binary+=b'\0'*((-len(binary))%4)
    Path(path).write_bytes(struct.pack('<III',0x46546c67,2,28+len(encoded)+len(binary))+
        struct.pack('<II',len(encoded),0x4e4f534a)+encoded+
        struct.pack('<II',len(binary),0x004e4942)+binary)


def encoded_image(data,binary,index):
    image=data['images'][index];assert 'uri' not in image
    view=data['bufferViews'][image['bufferView']];offset=view.get('byteOffset',0)
    return binary[offset:offset+view['byteLength']]


def prepare_image_glb(source,destination):
    """Retain original geometry/UVs and PBR map semantics while capping images."""
    from PIL import Image
    import numpy as np
    data,binary=glb(source)
    normal_images={data['textures'][m['normalTexture']['index']]['source']
        for m in data.get('materials',[]) if 'normalTexture' in m}
    image_views={};image_report=[]
    for i,image_record in enumerate(data.get('images',[])):
        image=Image.open(io.BytesIO(encoded_image(data,binary,i))).convert('RGBA')
        original_size=list(image.size)
        image.thumbnail((1024,1024),Image.Resampling.LANCZOS)
        if i in normal_images:
            # Filtered tangent normals must remain unit vectors, especially
            # after a 4K -> 1K reduction. Preserve any original alpha channel.
            pixels=np.array(image);normal=pixels[:,:,:3].astype(np.float32)/127.5-1
            normal/=np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),1e-6)
            pixels[:,:,:3]=np.uint8(np.clip(np.rint((normal+1)*127.5),0,255))
            image=Image.fromarray(pixels)
        encoded=io.BytesIO();image.save(encoded,format='PNG',compress_level=6)
        image_views[image_record['bufferView']]=encoded.getvalue()
        image_record['mimeType']='image/png'
        image_report.append(dict(image=i,original_size=original_size,size=list(image.size),
            normal_renormalized=i in normal_images,sha256=hashlib.sha256(encoded.getvalue()).hexdigest()))
    output=bytearray()
    for i,view in enumerate(data['bufferViews']):
        assert view.get('buffer',0)==0
        content=image_views.get(i)
        if content is None:
            offset=view.get('byteOffset',0);content=binary[offset:offset+view['byteLength']]
        output+=b'\0'*((-len(output))%4)
        view['byteOffset']=len(output);view['byteLength']=len(content);output+=content
    data['buffers'][0]['byteLength']=len(output)
    write_glb(destination,data,bytes(output))
    return image_report


def prepare():
    (SOURCE/'source_glbs').mkdir(parents=True,exist_ok=True)
    (SOURCE/'prepared_glbs').mkdir(exist_ok=True)
    records=[]
    for good in GOODS:
        with zipfile.ZipFile(DOWNLOADS/good['archive']) as archive:
            members=[m for m in archive.infolist() if Path(m.filename).name==good['member_name']]
            assert len(members)==1,good
            member=members[0];source=SOURCE/'source_glbs'/(good['alias']+'.glb')
            original=archive.read(member)
            if source.exists():assert source.read_bytes()==original,'Staged original differs; original is never overwritten'
            else:source.write_bytes(original)
            record={**good,'source_member':member.filename,'archive_bytes':(DOWNLOADS/good['archive']).stat().st_size,
                'source_bytes':member.file_size,'source_crc32':format(member.CRC,'08x'),
                'source_sha256':hashlib.sha256(original).hexdigest()}
        prepared=SOURCE/'prepared_glbs'/(good['alias']+'.glb')
        record['images']=prepare_image_glb(source,prepared)
        record['prepared_sha256']=sha(prepared);records.append(record)
        print('PREPARED',good['alias'],record['images'],flush=True)
    provenance={'schema_version':1,'provenance':'User-provided purchased asset archives from local Downloads.',
        'license':'Commercial/purchased, as supplied by the user; no embedded license document in these archives.',
        'license_note':'No Creative Commons/free redistribution permission is asserted. Original archives remain unchanged in Downloads.',
        'changes':'Selective GLB staging; derived maps capped at1024px, tangent normal maps renormalized; source PBR/UVs retained.',
        'sources':records}
    (SOURCE/'PROVENANCE.json').write_text(json.dumps(provenance,indent=2)+'\n')


def inspect(path):
    from PIL import Image
    import numpy as np
    data,binary=glb(path)
    assert not data.get('skins') and not data.get('animations') and not data.get('extensionsRequired')
    triangles=0;primitives=[];bounds=[];bad_winding=0;nondegenerate=0
    def accessor_values(index):
        accessor=data['accessors'][index];view=data['bufferViews'][accessor['bufferView']]
        dtype=np.dtype({5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}[accessor['componentType']])
        components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[accessor['type']]
        offset=view.get('byteOffset',0)+accessor.get('byteOffset',0)
        stride=view.get('byteStride',dtype.itemsize*components)
        return np.ndarray((accessor['count'],components),dtype=dtype,buffer=binary,offset=offset,
            strides=(stride,dtype.itemsize))
    for mesh in data['meshes']:
        for primitive in mesh['primitives']:
            assert primitive.get('mode',4)==4 and 'NORMAL' in primitive['attributes'] and 'TEXCOORD_0' in primitive['attributes']
            assert 'COLOR_0' not in primitive['attributes'] and 'material' in primitive
            triangles+=data['accessors'][primitive['indices']]['count']//3;primitives.append(primitive)
            bounds.append(data['accessors'][primitive['attributes']['POSITION']])
            positions=accessor_values(primitive['attributes']['POSITION'])
            normals=accessor_values(primitive['attributes']['NORMAL'])
            uv=accessor_values(primitive['attributes']['TEXCOORD_0'])
            indices=accessor_values(primitive['indices']).reshape(-1,3)
            assert np.isfinite(positions).all() and np.isfinite(normals).all() and np.isfinite(uv).all()
            assert indices.max()<len(positions) and len(normals)==len(positions)==len(uv)
            assert np.max(np.abs(np.linalg.norm(normals,axis=1)-1))<.003
            face=np.cross(positions[indices[:,1]]-positions[indices[:,0]],positions[indices[:,2]]-positions[indices[:,0]])
            valid=np.linalg.norm(face,axis=1)>1e-10
            facing=np.einsum('ij,ij->i',face,normals[indices].sum(axis=1))
            nondegenerate+=int(valid.sum());bad_winding+=int(((facing<0)&valid).sum())
    assert len(primitives)==1,path
    assert bad_winding/max(1,nondegenerate)<.025,(path,bad_winding,nondegenerate)
    low=[min(a['min'][i] for a in bounds) for i in range(3)]
    high=[max(a['max'][i] for a in bounds) for i in range(3)]
    assert all(math.isfinite(x) for x in low+high) and all(high[i]>low[i] for i in range(3))
    # Blender transforms are applied before export: mesh bounds equal renderer
    # bounds and no hidden presentation-scale node can move the tabletop origin.
    for node in data.get('nodes',[]):
        assert 'matrix' not in node and node.get('scale',[1,1,1])==[1,1,1]
        assert node.get('translation',[0,0,0])==[0,0,0]
    maps=[]
    for material in data['materials']:
        pbr=material['pbrMetallicRoughness']
        assert 'baseColorTexture' in pbr and 'normalTexture' in material
        assert material['normalTexture'].get('scale',1)==1
        for semantic,view,srgb in [('basecolor',pbr['baseColorTexture'],True),('normal',material['normalTexture'],False),
                                   ('metallic_roughness',pbr.get('metallicRoughnessTexture'),False)]:
            if view is None:continue
            assert view.get('texCoord',0)==0 and not view.get('extensions')
            texture=data['textures'][view['index']];encoded=encoded_image(data,binary,texture['source'])
            im=Image.open(io.BytesIO(encoded));assert max(im.size)<=1024
            values=np.asarray(im.convert('RGB'))
            maps.append(dict(semantic=semantic,srgb=srgb,size=list(im.size),
                sha256=hashlib.sha256(encoded).hexdigest(),encoded_bytes=len(encoded),
                mean_rgb=[float(x) for x in values.mean(axis=(0,1))],
                min_rgb=[int(x) for x in values.min(axis=(0,1))],max_rgb=[int(x) for x in values.max(axis=(0,1))]))
    return dict(triangles=triangles,primitive_count=len(primitives),bounds_min=low,bounds_max=high,
        dimensions_m=[high[i]-low[i] for i in range(3)],bytes=path.stat().st_size,sha256=sha(path),
        materials=copy.deepcopy(data['materials']),maps=maps,
        geometry_validation={'nondegenerate_triangles':nondegenerate,'opposing_normal_triangles':bad_winding,
            'finite_positions_normals_uvs':True,'indices_in_range':True})


def attach_standard_materials(path,prepared):
    """Attach the source's exact standard PBR graph to new mesh buffers.

    Some Blender versions drop a source normal-texture link on round-trip.
    The LOD operation changes geometry only: copying the validated GLB texture
    graph avoids that conversion, preserves its UV channel, and makes both
    LODs share byte-identical images in the native texture cache.
    """
    data,binary=glb(path);source,source_binary=glb(prepared)
    assert len(source['materials'])==1 and not source.get('extensionsRequired')
    assert all('sparse' not in a for a in data.get('accessors',[]))
    wanted=sorted({a['bufferView'] for a in data['accessors'] if 'bufferView' in a})
    output=bytearray();views=[];remap={}
    def add_view(content,record):
        output.extend(b'\0'*((-len(output))%4))
        record={**record,'buffer':0,'byteOffset':len(output),'byteLength':len(content)}
        views.append(record);output.extend(content)
        return len(views)-1
    for old_index in wanted:
        view=data['bufferViews'][old_index];offset=view.get('byteOffset',0)
        remap[old_index]=add_view(binary[offset:offset+view['byteLength']],view)
    for accessor in data['accessors']:
        if 'bufferView' in accessor:accessor['bufferView']=remap[accessor['bufferView']]
    data['images']=copy.deepcopy(source['images'])
    for i,record in enumerate(data['images']):
        record['bufferView']=add_view(encoded_image(source,source_binary,i),{})
    for key in ['materials','textures','samplers']:
        if key in source:data[key]=copy.deepcopy(source[key])
        else:data.pop(key,None)
    for mesh in data['meshes']:
        for primitive in mesh['primitives']:primitive['material']=0
    for material in data['materials']:
        material.get('extensions',{}).pop('KHR_materials_specular',None)
        if material.get('extensions')=={}:material.pop('extensions')
    extensions=[e for e in source.get('extensionsUsed',[]) if e!='KHR_materials_specular']
    if extensions:data['extensionsUsed']=extensions
    else:data.pop('extensionsUsed',None)
    data['bufferViews']=views;data['buffers']=[{'byteLength':len(output)}]
    write_glb(path,data,bytes(output))


def validate(directory=OUT):
    directory=Path(directory);manifest=json.loads((directory/MANIFEST).read_text())
    assert len(manifest['assets'])==8
    unique={}
    for asset in manifest['assets']:
        actual=inspect(directory/asset['file'])
        assert actual['sha256']==asset['sha256'] and actual['triangles']==asset['triangles']
        assert 0<actual['triangles']<=TARGETS[asset['lod']]
        assert abs(actual['bounds_min'][1])<.004 and actual['dimensions_m'][1]>.05
        source,source_binary=glb(SOURCE/'prepared_glbs'/(asset['alias']+'.glb'))
        expected_materials=copy.deepcopy(source['materials'])
        for material in expected_materials:
            material.get('extensions',{}).pop('KHR_materials_specular',None)
            if material.get('extensions')=={}:material.pop('extensions')
        assert actual['materials']==expected_materials,'Standard source PBR graph must survive geometry cooking exactly'
        expected_images={hashlib.sha256(encoded_image(source,source_binary,i)).hexdigest()
            for i in range(len(source['images']))}
        assert {m['sha256'] for m in actual['maps']}==expected_images
        for image in actual['maps']:
            key=(image['sha256'],image['srgb']);unique[key]=image['size']
    memory=sum(w*h*4*4/3 for w,h in unique.values())/1048576
    assert memory<=60 and len(unique)<=11,(memory,len(unique))
    for alias in [g['alias'] for g in GOODS]:
        pair=[a for a in manifest['assets'] if a['alias']==alias]
        assert len(pair)==2
        assert [(m['sha256'],m['srgb']) for m in pair[0]['maps']]==[(m['sha256'],m['srgb']) for m in pair[1]['maps']],alias
        assert max(abs(pair[0]['dimensions_m'][i]-pair[1]['dimensions_m'][i]) for i in range(3))<.015
    print('VALIDATED market goods:',len(manifest['assets']),'LODs,',len(unique),'shared texture policies,',round(memory,3),'MiB RGBA8+mips',flush=True)
    return dict(unique_texture_policies=len(unique),estimated_rgba_mips_mib=memory,
        max_texture_side=max(max(s) for s in unique.values()),model_count=8,
        max_near_triangles=max(a['triangles'] for a in manifest['assets'] if a['lod']=='near'),
        max_mid_triangles=max(a['triangles'] for a in manifest['assets'] if a['lod']=='mid'))


def cook():
    import bpy
    from mathutils import Matrix,Vector
    provenance=json.loads((SOURCE/'PROVENANCE.json').read_text())
    records=[]
    # Stage on the destination filesystem so publication renames are atomic
    # even when /tmp and the user's Desktop use different mounts.
    OUT.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.purchased-goods-',dir=OUT) as temporary:
        stage=Path(temporary)
        for good in GOODS:
            for lod,budget in TARGETS.items():
                bpy.ops.wm.read_factory_settings(use_empty=True)
                bpy.ops.import_scene.gltf(filepath=str(SOURCE/'prepared_glbs'/(good['alias']+'.glb')))
                meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];assert len(meshes)==1
                obj=meshes[0]
                points=[obj.matrix_world@Vector(c) for c in obj.bound_box]
                low=Vector([min(p[i] for p in points) for i in range(3)])
                high=Vector([max(p[i] for p in points) for i in range(3)])
                scale=good.get('height_m',0)/(high.z-low.z) if 'height_m' in good else good['horizontal_m']/max(high.x-low.x,high.y-low.y)
                origin=Vector(((low.x+high.x)*.5,(low.y+high.y)*.5,low.z))
                obj.matrix_world=Matrix.Scale(scale,4)@Matrix.Translation(-origin)@obj.matrix_world
                bpy.context.view_layer.objects.active=obj
                bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
                bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
                source_triangles=sum(len(p.vertices)-2 for p in obj.data.polygons)
                for attempt in range(3):
                    count=sum(len(p.vertices)-2 for p in obj.data.polygons)
                    if count<=budget:break
                    modifier=obj.modifiers.new('Bounded purchased prop LOD','DECIMATE')
                    modifier.ratio=min(.99,budget*.98/count);modifier.use_collapse_triangulate=True
                    bpy.ops.object.modifier_apply(modifier=modifier.name)
                assert sum(len(p.vertices)-2 for p in obj.data.polygons)<=budget
                obj.name='purchased_goods_'+good['alias']
                for mat in obj.data.materials:
                    for node in mat.node_tree.nodes:
                        if node.type=='TEX_IMAGE' and node.image:node.image.pack()
                name='purchased_goods_'+good['alias']+'_'+lod+'.glb';path=stage/name
                bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
                    export_yup=True,export_animations=False,export_cameras=False,export_lights=False,
                    export_materials='NONE',export_apply=True)
                # The native PBR loader implements glTF's standard dielectric
                # response, not the source's optional two-times specular tint.
                attach_standard_materials(path,SOURCE/'prepared_glbs'/(good['alias']+'.glb'))
                record=dict(alias=good['alias'],lod=lod,file=name,source_triangles=source_triangles,
                    source=next(r for r in provenance['sources'] if r['alias']==good['alias']),
                    **inspect(path))
                records.append(record);print('COOKED',name,record['triangles'],record['dimensions_m'],flush=True)
        manifest={'schema_version':1,'provenance':provenance['provenance'],'license':provenance['license'],
            'origin':'Grounded atY0, X/Z centered; dimensions are metres. Mesh and node transforms are applied.',
            'materials_note':'Standard basecolor/normal/metallic-roughness PBR retained. Native default dielectric specular response; optional source KHR_materials_specular tint omitted. No baked HDRI or scene light.',
            'source_archives_preserved':True,'sources':provenance['sources'],'assets':records}
        (stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n')
        manifest['budget']=validate(stage)
        (stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n')
        OUT.mkdir(parents=True,exist_ok=True)
        for record in records:os.replace(stage/record['file'],OUT/record['file'])
        os.replace(stage/MANIFEST,OUT/MANIFEST)
    validate()


if __name__=='__main__':
    args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else sys.argv[1:]
    if '--prepare' in args:prepare()
    elif '--validate' in args:validate()
    else:cook()
