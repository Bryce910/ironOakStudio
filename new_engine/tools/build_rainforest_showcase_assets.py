#!/usr/bin/env python3
"""Download and cook bounded CC0 Poly Haven rainforest showcase models.

Python --download --prepare; Blender --background --threads 2 --python this_file
to cook; Python --validate verifies real GLB geometry, alpha and texture sharing.
Powered by Poly Haven. Source bytes/provenance stay alongside the downloaded kit.
"""
from pathlib import Path
import copy
import hashlib
import io
import json
import math
import struct
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'assets/polyhaven/rainforest_showcase'
OUT = ROOT/'projects/valwood_world/models'
sys.path.insert(0, str(ROOT/'tools'))
from build_cinematic_assets import fetch, sha, texture_infos
from build_starting_area_assets import glb

KIT = [
    dict(name='showcase_roots', source='root_cluster_01', near=10000, mid=2400, base=1024, normal=1024, arm=512),
    dict(name='showcase_stump', source='tree_stump_02', near=9000, mid=2000, base=2048, normal=1024, arm=512),
    dict(name='showcase_shrub', source='shrub_02', objects=['shrub_02_a'], plant=True, near=8000, mid=1800),
    dict(name='showcase_shrub_b', source='shrub_02', objects=['shrub_02_c'], plant=True, near=9500, mid=2200),
    dict(name='showcase_sorrel', source='shrub_sorrel_01',
         objects=['shrub_sorrel_01_a','shrub_sorrel_01_b','shrub_sorrel_01_d','shrub_sorrel_01_e'],
         plant=True, clump=True, near=2500, mid=650),
]


def families():
    return sorted({config['source'] for config in KIT})


def download():
    SOURCE.mkdir(parents=True, exist_ok=True)
    tasks=[]
    for asset in families():
        folder=SOURCE/asset;folder.mkdir(exist_ok=True)
        fetch('https://api.polyhaven.com/files/'+asset,folder/'metadata.json')
        metadata=json.loads((folder/'metadata.json').read_text());tier='2k' if asset=='tree_stump_02' else '1k'
        record=metadata['gltf'][tier]['gltf'];fetch(record['url'],folder/(asset+'.gltf'),record)
        tasks.extend((r['url'],folder/name,r) for name,r in record['include'].items())
        for key,value in metadata.items():
            if 'alpha' in key.lower() and tier in value and 'png' in value[tier]:
                r=value[tier]['png'];tasks.append((r['url'],folder/('source_'+key+'.png'),r))
    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(lambda task:fetch(*task),tasks))
    provenance()


def prepare():
    from PIL import Image
    for asset in families():
        folder=SOURCE/asset;gltf=json.loads((folder/(asset+'.gltf')).read_text())
        configs=[c for c in KIT if c['source']==asset];plant=any(c.get('plant') for c in configs)
        source_images=copy.deepcopy(gltf['images']);source_textures=copy.deepcopy(gltf['textures'])
        images=[];textures=[];mapping={}
        for material in gltf['materials']:
            pbr=material['pbrMetallicRoughness']
            for kind,view in [('base',pbr.get('baseColorTexture')),('normal',material.get('normalTexture')),('arm',pbr.get('metallicRoughnessTexture'))]:
                if not view:continue
                uri=source_images[source_textures[view['index']]['source']]['uri']
                limit=max(c.get(kind,{'base':1024,'normal':512,'arm':256}[kind]) for c in configs)
                key=(uri,kind,limit)
                if key not in mapping:
                    image=Image.open(folder/uri).convert('RGBA' if plant and kind=='base' else 'RGB')
                    if plant and kind=='base':
                        masks=list(folder.glob('source_*[Aa]lpha*.png'));assert len(masks)==1,(asset,masks)
                        mask=Image.open(masks[0]).convert('L')
                        if mask.size!=image.size:mask=mask.resize(image.size,Image.Resampling.LANCZOS)
                        image.putalpha(mask)
                    image.thumbnail((limit,limit),Image.Resampling.LANCZOS)
                    name='runtime_'+str(len(images))+'_'+kind+'.png';image.save(folder/name,optimize=True)
                    mapping[key]=len(textures);images.append(dict(uri=name,mimeType='image/png'));textures.append(dict(source=len(images)-1,sampler=0))
                view['index']=mapping[key]
            material.pop('occlusionTexture',None)
            if plant:material.update(alphaMode='MASK',alphaCutoff=.35,doubleSided=True)
        gltf['images']=images;gltf['textures']=textures
        gltf['samplers']=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)]
        (folder/'runtime_prepared.gltf').write_text(json.dumps(gltf,indent=2)+'\n')
        print('PREPARED',asset,flush=True)
    provenance()


def provenance():
    records=[]
    for asset in families():
        folder=SOURCE/asset
        if not folder.exists():continue
        for path in sorted(p for p in folder.rglob('*') if p.is_file()):
            records.append(dict(path=str(path.relative_to(SOURCE)),bytes=path.stat().st_size,sha256=sha(path),
                                source='https://polyhaven.com/a/'+asset,license='API reference' if path.name=='metadata.json' else 'CC0-1.0',
                                reference_only=path.name=='metadata.json',derived=path.name.startswith('runtime_')))
    (SOURCE/'provenance.json').write_text(json.dumps(dict(asset_license='https://polyhaven.com/license',
        api_terms='https://github.com/Poly-Haven/Public-API',api_credit='Powered by Poly Haven',
        request_user_agent='Valwood-local-asset-cook/1.0 (CC0 game assets)',files=records),indent=2)+'\n')


def cook():
    import bpy
    import bmesh
    from mathutils import Matrix,Vector
    from bake_gltf_uv import bake_uv_transforms
    OUT.mkdir(parents=True,exist_ok=True);entries=[]
    def triangles(obj):return sum(len(p.vertices)-2 for p in obj.data.polygons)
    for config in KIT:
        for lod in ['near','mid']:
            bpy.ops.wm.read_factory_settings(use_empty=True)
            bpy.context.scene.render.threads_mode='FIXED';bpy.context.scene.render.threads=2
            bpy.ops.import_scene.gltf(filepath=str(SOURCE/config['source']/'runtime_prepared.gltf'))
            chosen=config.get('objects');meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and (chosen is None or o.name in chosen)]
            assert meshes and (chosen is None or len(meshes)==len(chosen)),(config,[o.name for o in bpy.context.scene.objects])
            for obj in list(bpy.context.scene.objects):
                if obj not in meshes:bpy.data.objects.remove(obj,do_unlink=True)
            for i,obj in enumerate(sorted(meshes,key=lambda o:o.name)):
                obj.data=obj.data.copy();obj.data.transform(obj.matrix_world);obj.matrix_world=Matrix.Identity(4)
                if config.get('clump'):
                    # Rearrange four original sorrel plants into an irregular
                    # compact clump rather than retaining the source gallery row.
                    points=[v.co for v in obj.data.vertices];lo=Vector([min(p[k] for p in points) for k in range(3)]);hi=Vector([max(p[k] for p in points) for k in range(3)])
                    obj.data.transform(Matrix.Translation(Vector([-(lo.x+hi.x)*.5,-(lo.y+hi.y)*.5,-lo.z])))
                    obj.data.transform(Matrix.Rotation(i*1.71,4,'Z'))
                    offsets=[(-.085,.025),(.04,-.065),(.095,.055),(-.025,.11)]
                    obj.data.transform(Matrix.Translation(Vector([*offsets[i],0.])))
            total=sum(triangles(o) for o in meshes);budget=config[lod]
            for obj in meshes:
                if total>budget:
                    bpy.context.view_layer.objects.active=obj;mod=obj.modifiers.new('Bounded source geometry LOD','DECIMATE');mod.ratio=budget/total;mod.use_collapse_triangulate=True
                    bpy.ops.object.modifier_apply(modifier=mod.name)
                bm=bmesh.new();bm.from_mesh(obj.data)
                bad=[f for f in bm.faces if f.calc_area()<1.e-12]
                if bad:bmesh.ops.delete(bm,geom=bad,context='FACES')
                bm.to_mesh(obj.data);bm.free();obj.data.update()
            points=[v.co for obj in meshes for v in obj.data.vertices];lo=Vector([min(p[k] for p in points) for k in range(3)]);hi=Vector([max(p[k] for p in points) for k in range(3)])
            shift=Matrix.Translation(Vector([-(lo.x+hi.x)*.5,-(lo.y+hi.y)*.5,-lo.z]))
            for obj in meshes:obj.data.transform(shift);obj.data.update();obj.select_set(True)
            for material in bpy.data.materials:
                material.use_backface_culling=False
                if material.node_tree:
                    for node in material.node_tree.nodes:
                        if node.type=='TEX_IMAGE' and node.image:node.image.pack()
            name=config['name']+('_mid' if lod=='mid' else '');path=OUT/(name+'.glb')
            bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',export_yup=True,export_animations=False,export_cameras=False,export_lights=False,export_image_format='AUTO')
            bake_uv_transforms(path)
            entry=dict(asset=name,source=config['source'],source_url='https://polyhaven.com/a/'+config['source'],license='CC0-1.0',
                       source_objects=chosen or [o.name for o in meshes],triangles=sum(triangles(o) for o in meshes),bytes=path.stat().st_size,sha256=sha(path),
                       vegetation=3 if config.get('plant') else 0,grounding='root' if config.get('plant') else 'support',
                       adaptations=['selected original source geometry','bounded near/mid LOD','photographic PBR/alpha retained','shared tiered embedded images','UV0 transforms baked','grounded horizontal center']+(['four original sorrel plants arranged as an irregular clump'] if config.get('clump') else []))
            entries.append(entry);(OUT/'showcase_manifest.json').write_text(json.dumps(entries,indent=2)+'\n')
            print('COOKED',name,entry['triangles'],flush=True)


def validate():
    import numpy as np
    from PIL import Image
    from build_starting_area import glb_bounds
    provenance_record=json.loads((SOURCE/'provenance.json').read_text())
    for source in provenance_record['files']:
        path=SOURCE/source['path']
        assert path.is_file() and path.stat().st_size==source['bytes'] and sha(path)==source['sha256'],source['path']
    entries=json.loads((OUT/'showcase_manifest.json').read_text());assert len(entries)==2*len(KIT)
    unique={};by_asset={}
    for entry in entries:
        path=OUT/(entry['asset']+'.glb');data,binary=glb(path);assert sha(path)==entry['sha256']
        assert not data.get('animations') and not data.get('skins');count=0
        def accessor(index):
            ac=data['accessors'][index];view=data['bufferViews'][ac['bufferView']]
            dimensions={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[ac['type']]
            dtype=np.dtype({5121:'u1',5123:'<u2',5125:'<u4',5126:'<f4'}[ac['componentType']])
            return np.ndarray((ac['count'],dimensions),dtype,binary,offset=view.get('byteOffset',0)+ac.get('byteOffset',0),strides=(view.get('byteStride',dimensions*dtype.itemsize),dtype.itemsize))
        for mesh in data['meshes']:
            for primitive in mesh['primitives']:
                assert primitive.get('mode',4)==4 and 'TEXCOORD_0' in primitive['attributes'];count+=data['accessors'][primitive['indices']]['count']//3
                for index in primitive['attributes'].values():
                    ac=data['accessors'][index];view=data['bufferViews'][ac['bufferView']]
                    dimensions={'VEC2':2,'VEC3':3,'VEC4':4}[ac['type']]
                    assert ac['componentType']==5126
                    values=np.ndarray((ac['count'],dimensions),'<f4',binary,offset=view.get('byteOffset',0)+ac.get('byteOffset',0),strides=(view.get('byteStride',dimensions*4),4))
                    assert np.isfinite(values).all()
                positions=accessor(primitive['attributes']['POSITION']);normals=accessor(primitive['attributes']['NORMAL'])
                indices=accessor(primitive['indices']).reshape(-1,3);assert indices.max()<len(positions)
                assert np.max(np.abs(np.linalg.norm(normals,axis=1)-1))<.002
                triangles=positions[indices];areas=np.sum(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])**2,axis=1)
                assert np.all(areas>1.e-22),entry['asset']
        assert count==entry['triangles'] and 0<count<=10000
        for material in data['materials']:
            assert material.get('alphaMode','OPAQUE')!='BLEND'
            for view in texture_infos(material):assert view.get('texCoord',0)==0 and not view.get('extensions',{}).get('KHR_texture_transform')
        hashes=set()
        for image in data['images']:
            view=data['bufferViews'][image['bufferView']];encoded=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
            digest=hashlib.sha256(encoded).hexdigest();im=Image.open(io.BytesIO(encoded));w,h=im.size;assert max(w,h)<=2048
            memory=0
            while True:
                memory+=w*h*4
                if w==h==1:break
                w,h=max(1,w//2),max(1,h//2)
            unique[digest]=memory;hashes.add(digest)
            if im.mode=='RGBA':
                alpha=np.asarray(im.getchannel('A'));assert alpha.min()<32 and alpha.max()>224
        by_asset[entry['asset']]=hashes;lo,hi=glb_bounds(path);entry['bounds_y_up']=[lo,hi]
        assert lo[1]==0 and hi[1]>0
        print('VALIDATED',entry['asset'],count,'bounds',lo,hi,flush=True)
    for config in KIT:assert by_asset[config['name']]==by_asset[config['name']+'_mid']
    assert by_asset['showcase_shrub']==by_asset['showcase_shrub_b']
    memory=sum(unique.values());assert memory<=80*1048576
    (OUT/'showcase_manifest.json').write_text(json.dumps(entries,indent=2)+'\n')
    report=dict(encoded_glb_bytes=sum((OUT/(entry['asset']+'.glb')).stat().st_size for entry in entries),assets=len(entries),source_families=len(families()),unique_embedded_images=len(unique),rgba8_mip_bytes=memory,rgba8_mip_mib=memory/1048576,
                near_mid_shared=True,shrub_variants_shared=True,api_credit='Powered by Poly Haven')
    (SOURCE/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    (OUT/'showcase_manifest.summary.json').write_text(json.dumps(dict(report,asset_manifest='showcase_manifest.json',provenance='assets/polyhaven/rainforest_showcase/provenance.json'),indent=2)+'\n')
    print('PASS showcase validation',json.dumps(report),flush=True)


if __name__=='__main__':
    if '--help' in sys.argv or '-h' in sys.argv:
        print(__doc__+'\nOptions: --download --prepare --validate; cooking runs only under Blender.\nAll outputs are fixed repository paths; source files and generated manifests are retained.')
        raise SystemExit(0)
    if len(sys.argv)==1:
        print(__doc__)
        raise SystemExit(2)
    if '--download' in sys.argv:download()
    if '--prepare' in sys.argv:prepare()
    if '--validate' in sys.argv:validate()
    if not any(flag in sys.argv for flag in ['--download','--prepare','--validate']):cook()
