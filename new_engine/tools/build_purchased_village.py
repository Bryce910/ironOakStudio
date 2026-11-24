#!/usr/bin/env python3
"""Cook the user's purchased medieval architecture for Valwood's native GL renderer.

Python: --prepare tiers retained sources; --validate audits published GLBs.
Blender -b -t 2 --python this_file -- [--families house_a,house_b,...] cooks.
No Unreal runtime is used. Source files and existing Valwood assets are retained.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import os
import struct
import sys

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/purchased/medieval_village'
OUT=ROOT/'projects/valwood_world/models'
ARCHIVE=Path('/home/computer/Downloads/Medieval_Village_ALL.rar')
RECIPE=5
ARCHITECTURE_GPU_BUDGET_MIB=544
TILE_KEYS={'MI_Tileable_Roof1','MI_Tileable_Roof2',
           'MI_Tileable_Wall_Stone_01','MI_Tileable_Wall_Sone_02',
           'MI_Tileable_Thached_Roof_3','mı_Tileable_Thached_Roof_Edge'}
FAMILIES={
    'house_a':('SM_House_One.FBX',20000,5000),
    'house_b':('SM_House_Five.FBX',20000,5000),
    'stall_a':('SM_Shop_04.FBX',10000,2500),
    'stall_b':('SM_Shop_02.FBX',10000,2500),
    'house_c':('SM_House_Two.FBX',18000,4200),
    'house_d':('SM_House_Three.FBX',18000,4200),
    'house_e':('SM_House_Four.FBX',14000,3500),
    'house_f':('SM_House_Six.FBX',12000,3000),
    'stall_c':('SM_Shop_01.FBX',6500,1800),
    'stall_d':('SM_Shop_03.FBX',4500,1400),
    'castle':('SM_Castle.FBX',10000,3000),
    'gate':('SM_Castle_Boundary_Gate_01.FBX',5000,1800),
    'wall':('SM_Castle_Boundary_Wall_01.FBX',2000,1000),
    'barrel':('SM_Barrel_01.FBX',3000,750),
    'crate':('SM_Crate.FBX',3000,750),
}
# New families use bounded maps; nearby original market façades retain 2K.
ATLAS_SIZES={family:(1024,512,256) for family in FAMILIES}
for family in ['house_a','house_b','stall_a','stall_b']:ATLAS_SIZES[family]=(2048,1024,512)
ATLAS_SIZES['castle']=(1536,768,384)
for family in ['barrel','crate']:ATLAS_SIZES[family]=(512,256,128)
PHYSICAL={
    'house_a':dict(front_local_axis='-X',walkable_interior=False),
    'house_b':dict(front_local_axis='+X',walkable_interior=False),
    'stall_a':dict(front_local_axis='+X',counter_y_approx=.625,counter_x_range=[.462,1.105],counter_z_range=[-.956,1.831],source_crates_present=True),
    'stall_b':dict(front_local_axis='either X side',canopy_only=True,counter_present=False),
}
MATERIALS={
    'MI_Tileable_Wood_Trims_01':'T_Tileable_Wood_Trims_01',
    'MI_Tileable_Wall_Plaster':'T_Tileable_Wall_Plaster_01',
    'MI_Tileable_Roof1':'T_Roof_Tile_01',
    'MI_Tileable_Roof2':'T_Roof_Tile_02',
    'MI_Tileable_Wall_Stone_01':'T_Tileable_Wall_Stone_01',
    'MI_Tileable_Wall_Sone_02':'T_Tileable_Wall_Stone_02',
    'MI_Tileable_Glass_Window':'T_Tileable_Glass_Window',
    'MI_Tileable_Metal':'T_Tileable_Metal',
    'MI_Wood_End_01':'T_Wood_End_01',
    'MI_House_Decals':'T_Decals',
    'MI_Tent1':'T_Tent_1','MI_Tent2':'T_Tent_2','MI_Tent3':'T_Tent_3',
    'MI_Tileable_Thached_Roof_3':'T_Roof_Tile_03',
    'mı_Tileable_Thached_Roof_Edge':'T_Roof_Tile_03',
    'MI_Barrel':'T_Barrel','MI_Crate':'T_Crate',
    'MI_Tent_4':'T_Tent_4','MI_Food_Sheet_02':'T_Food_Sheet_02',
    'MI_Sack_Bag_01':'T_Sack_Bag_01','MI_Sack_Bag_02':'T_Sack_Bag_02',
    'MI_Tileable_Rope':'T_Tileable_Rope',
    'MI_Tileable_Castle_Walls_01':'T_Castle_Tileable_Walls_01',
    'MI_Tileable_Castle_Walls_02':'T_Castle_Tileable_Walls_02',
}
# Material-instance names in the original Unreal project differ from the FBX
# export names. This mapping is backed by unreal-material-source-audit.json.
SOURCE_INSTANCES={
    'MI_Tileable_Wood_Trims_01':'MI_Wood_Trims_01',
    'MI_Tileable_Wall_Plaster':'MI_WallPlaster',
    'MI_Tileable_Roof1':'MI_Roof1','MI_Tileable_Roof2':'MI_Roof2',
    'MI_Tileable_Wall_Stone_01':'MI_Wall_Stone_01',
    'MI_Tileable_Wall_Sone_02':'MI_Wall_Sone_02',
    'MI_Tileable_Castle_Walls_01':'MI_Castle_Walls_01',
    'MI_Tileable_Castle_Walls_02':'MI_Castle_Walls_02',
    'MI_Tileable_Thached_Roof_3':'MI_Thached_Roof_3',
    'mı_Tileable_Thached_Roof_Edge':'MI_Thached_Roof_Edge',
    'MI_Tileable_Glass_Window':'MI_GlassWindow','MI_Tileable_Metal':'MI_Metal',
    'MI_Wood_End_01':'MI_WoodEnd_01','MI_House_Decals':'MI_HouseDecals',
    'MI_Tent1':'MI_Tent_1','MI_Tent2':'MI_Tent_2','MI_Tent3':'MI_Tent_3',
    'MI_Tileable_Rope':'MI_Rope','MI_Food_Sheet_02':'MI_FoodSheet_02',
    'MI_Sack_Bag_01':'MI_SackBag_01','MI_Sack_Bag_02':'MI_SackBag_02',
}

def material_source_settings(key):
    path=SOURCE/'unreal-material-source-audit.json'
    assert path.exists(),'Audit the supplied Unreal source project before cooking'
    record=json.loads(path.read_text())['material_instances'].get(SOURCE_INSTANCES.get(key,key),{})
    scalars=record.get('scalars',{})
    tiling=scalars.get('Texture_Tiling',scalars.get('Tex_tiling',1.))
    assert math.isfinite(tiling) and .1<=tiling<=8
    return record,tiling

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def prepare():
    from PIL import Image,ImageOps
    import numpy as np
    dest=SOURCE/'prepared';dest.mkdir(parents=True,exist_ok=True)
    previous={row['source']:row for row in json.loads((SOURCE/'PROVENANCE.json').read_text()).get('source_textures',[])} if (SOURCE/'PROVENANCE.json').exists() else {}
    files=[]
    for path in sorted((SOURCE/'source_textures').glob('*.PNG')):
        kind=path.stem.rsplit('_',1)[-1]
        if kind not in {'BC','N','ARM','MRA','Mask'}:continue
        output=dest/(path.stem+'.png');source_hash=sha(path)
        if path.name in previous and previous[path.name]['sha256']==source_hash and output.exists():
            files.append(previous[path.name]);continue
        image=Image.open(path).convert('RGBA' if kind=='BC' else 'RGB')
        image.thumbnail((2048,2048) if kind=='BC' else ((1024,1024) if kind in {'N','Mask'} else (512,512)),Image.Resampling.LANCZOS)
        if kind=='N':
            values=np.asarray(image).copy();values[:,:,1]=255-values[:,:,1]
            image=Image.fromarray(values)
        elif kind=='MRA':
            # This source explicitly uses M/R/A instead of the ARM triplets.
            values=np.asarray(image).copy();image=Image.fromarray(values[:,:,[2,1,0]])
        image.save(output,optimize=True)
        files.append(dict(source=path.name,sha256=source_hash,prepared=output.name,
            dimensions=list(image.size),normal_convention='DirectX green inverted to OpenGL' if kind=='N' else None))
    records=[dict(path=str(p.relative_to(SOURCE)),bytes=p.stat().st_size,sha256=sha(p))
             for p in sorted((SOURCE/'source_meshes').glob('*.FBX'))]
    provenance=dict(origin='User-provided purchased asset archive',archive=str(ARCHIVE),
        archive_bytes=ARCHIVE.stat().st_size,archive_sha256=sha(ARCHIVE),
        license='User-provided purchased content; no redistribution or open-license claim',
        license_document='No license/readme was present in the supplied exported archive; retain the purchase terms separately.',
        original_downloads_modified=False,source_meshes=records,source_textures=files,
        recovered_unreal_source_materials='unreal-material-source-audit.json',
        adaptations=['Only selected source LODs; UCX collision meshes excluded',
            'Source material PBR channels and explicit UV0 retained during bake',
            'Source material-instance texture tiling and masked vertex-paint layers baked',
            'Red vertex paint is a layer mask, never an RGB albedo multiplier',
            'Per-family baked atlas, near/mid LODs share embedded images',
            '2K base/1K tangent normal/512 ARM atlas; native renderer remains custom'])
    (SOURCE/'PROVENANCE.json').write_text(json.dumps(provenance,indent=2)+'\n')
    print('Prepared',len(files),'bounded source PBR images',flush=True)

def glb(path):
    data=path.read_bytes()
    assert struct.unpack_from('<III',data)==(0x46546c67,2,len(data))
    length,kind=struct.unpack_from('<II',data,12);assert kind==0x4e4f534a
    document=json.loads(data[20:20+length]);offset=20+length
    length,kind=struct.unpack_from('<II',data,offset);assert kind==0x004e4942
    return document,data[offset+8:offset+8+length]

def remove_house_d_duplicate_plaster(document,binary):
    """Keep the outward plaster face where the source repeats it4.8um behind.

    Indexed vertex/UV/normal/tangent and image bytes stay untouched. Only six
    redundant triangles are removed from the index stream in each LOD. Equal
    geometric keys alone are insufficient: require matching outward normals,
    matching winding and positions within10um before choosing the outer face.
    """
    import numpy as np
    output=bytearray(binary);removed=0;proof=[]
    def values(index,width):
        a=document['accessors'][index];v=document['bufferViews'][a['bufferView']]
        dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']];item=np.dtype(dtype).itemsize
        return np.ndarray((a['count'],width),dtype=dtype,buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',item*width),item))
    for mesh in document['meshes']:
        for primitive in mesh['primitives']:
            xyz=values(primitive['attributes']['POSITION'],3);normal=values(primitive['attributes']['NORMAL'],3)
            indices=values(primitive['indices'],1).reshape(-1,3);groups={}
            for i,tri in enumerate(indices):
                ordered=sorted([int(v) for v in tri],key=lambda v:tuple(xyz[v]));points=xyz[ordered]
                key=tuple(tuple(int(round(float(v)*100000)) for v in p) for p in points)
                groups.setdefault(key,[]).append((i,ordered))
            discard=set()
            for rows in groups.values():
                if len(rows)<2:continue
                assert len(rows)==2,'Unexpected source duplicate multiplicity'
                (ia,va),(ib,vb)=rows;pa,pb=xyz[va],xyz[vb];na,nb=normal[va],normal[vb]
                displacement=float(np.max(np.abs(pa-pb)));assert displacement<=.00001
                assert np.max(np.abs(na-nb))<.00001
                n=np.mean(na,axis=0);assert n[2]>.9999 and abs(n[0])+abs(n[1])<.00001
                assert np.mean(pa[:,2])>8.5,'Duplicate outside the known plaster surface'
                # Both triangles must face outwards, not form a legitimate
                # opposite-facing double-sided sheet. Export winding is kept.
                def winding(index):
                    p=xyz[indices[index]];return np.cross(p[1]-p[0],p[2]-p[0])
                assert float(np.dot(winding(ia),winding(ib)))>0
                score_a=float(np.dot(np.mean(pa,axis=0),n));score_b=float(np.dot(np.mean(pb,axis=0),n))
                keep,drop=(ia,ib) if score_a>=score_b else (ib,ia);discard.add(drop)
                p=xyz[indices[keep]];area=float(np.linalg.norm(np.cross(p[1]-p[0],p[2]-p[0]))/2)
                proof.append(dict(retained_triangle=keep,removed_triangle=drop,outward_normal=[float(v) for v in n],maximum_position_difference_m=displacement,area_m2=area,retained_outward_plane_m=max(score_a,score_b)))
            if discard:
                retained=np.asarray([tri for i,tri in enumerate(indices) if i not in discard],dtype=indices.dtype)
                a=document['accessors'][primitive['indices']];view=document['bufferViews'][a['bufferView']]
                assert not view.get('byteStride') and not a.get('byteOffset',0)
                offset=view.get('byteOffset',0);encoded=retained.tobytes();output[offset:offset+len(encoded)]=encoded
                a['count']=int(retained.size);a['min']=[int(retained.min())];a['max']=[int(retained.max())]
                view['byteLength']=len(encoded);removed+=len(discard)
    assert removed in {0,6},('Unexpected HouseD duplicate count',removed)
    return bytes(output),dict(removed_triangles=removed,selection='Matching outward plaster normals; retain the outermost source face and its original UVs',triangles=proof)

def encode_glb(document,binary):
    encoded=json.dumps(document,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4);binary+=b'\0'*((-len(binary))%4)
    return struct.pack('<III',0x46546c67,2,28+len(encoded)+len(binary))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary

def retain_mid_geometry(previous_document,previous_binary,new_document,new_binary):
    """Keep the validated published LOD shape while replacing baked PBR maps.

    Only used when the FBX source hash/budget are unchanged. Re-running Blender
    decimation after UV/material edits can choose a different edge collapse;
    that must not alter distant silhouettes in a material-only conversion.
    """
    import copy
    result=copy.deepcopy(previous_document);binary=bytearray(previous_binary)
    assert len(result['materials'])==len(new_document['materials'])==1
    result['materials']=copy.deepcopy(new_document['materials'])
    result['textures']=copy.deepcopy(new_document['textures'])
    result['samplers']=copy.deepcopy(new_document['samplers'])
    result['images']=copy.deepcopy(new_document['images'])
    for image in result['images']:
        view=new_document['bufferViews'][image['bufferView']];offset=view.get('byteOffset',0)
        binary+=b'\0'*((-len(binary))%4);new_view=dict(view,byteOffset=len(binary))
        binary+=new_binary[offset:offset+view['byteLength']]
        image['bufferView']=len(result['bufferViews']);result['bufferViews'].append(new_view)
    # Compact away unused old image views. Accessor/vertex/index bytes survive
    # exactly; keeping stale encoded atlases in every GLB would waste disk/RAM.
    used={a['bufferView'] for a in result['accessors'] if 'bufferView' in a}
    used.update(image['bufferView'] for image in result['images']);remap={};views=[];packed=bytearray()
    for index in sorted(used):
        view=result['bufferViews'][index];offset=view.get('byteOffset',0)
        packed+=b'\0'*((-len(packed))%4);remap[index]=len(views)
        views.append(dict(view,byteOffset=len(packed)));packed+=binary[offset:offset+view['byteLength']]
    for a in result['accessors']:
        if 'bufferView' in a:a['bufferView']=remap[a['bufferView']]
    for image in result['images']:image['bufferView']=remap[image['bufferView']]
    result['bufferViews']=views;result['buffers'][0]['byteLength']=len(packed)
    return result,bytes(packed)

def repair_published_duplicates():
    import shutil
    audit=SOURCE/'geometry-audit';audit.mkdir(exist_ok=True)
    manifest_path=OUT/'purchased_village_manifest.json';manifest=json.loads(manifest_path.read_text())
    backup_manifest=audit/'purchased_village_manifest_before_duplicate_repair.json'
    if not backup_manifest.exists():shutil.copy2(manifest_path,backup_manifest)
    records=[]
    for row in manifest['assets']:
        if row['family']!='house_d':continue
        path=OUT/(row['asset']+'.glb');backup=audit/(path.stem+'_before_duplicate_repair.glb')
        if not backup.exists():shutil.copy2(path,backup)
        before=inspect_glb(path);doc,binary=glb(path);original_attributes={key:doc['accessors'][i].copy() for p in doc['meshes'][0]['primitives'] for key,i in p['attributes'].items()}
        repaired,proof=remove_house_d_duplicate_plaster(doc,binary)
        if not proof['removed_triangles']:
            print('HouseD already repaired:',row['lod'],flush=True);continue
        assert proof['removed_triangles']==6
        # The repair only writes index views. Prove every source attribute and
        # every embedded image survives exactly, including UV seams/tangents.
        for p in doc['meshes'][0]['primitives']:
            for key,index in p['attributes'].items():
                a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];off=v.get('byteOffset',0)
                assert a==original_attributes[key] and repaired[off:off+v['byteLength']]==binary[off:off+v['byteLength']]
        temporary=audit/(path.stem+'_repaired.glb');temporary.write_bytes(encode_glb(doc,repaired));after=inspect_glb(temporary)
        assert before['images']==after['images'] and before['bounds_y_up']==after['bounds_y_up']
        assert before['triangles']-after['triangles']==6
        row.update(after);row['recipe']=RECIPE;row['geometry_cleanup']=proof
        records.append(dict(asset=row['asset'],backup=str(backup.relative_to(ROOT)),before=before,after=after,geometry_cleanup=proof,attribute_stream_bytes_identical=True,embedded_image_bytes_identical=True))
        os.replace(temporary,path)
    manifest['gpu_rgba8_mip_bytes']=validate_from_rows(manifest)
    manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
    if records:(audit/'duplicate-plaster-repair.json').write_text(json.dumps(dict(schema=1,records=records,source_archives_modified=False),indent=2)+'\n')

def validate_from_rows(manifest):
    # Shared helper for the in-place index-only repair publication.
    unique={}
    for row in manifest['assets']:
        for image in row['images']:unique[image['sha256']]=image
    gpu_bytes=sum(int(im['dimensions'][0])*int(im['dimensions'][1])*4*4/3 for im in unique.values())
    assert gpu_bytes<=ARCHITECTURE_GPU_BUDGET_MIB*1024*1024
    return gpu_bytes

def inspect_glb(path):
    from PIL import Image
    import io
    import numpy as np
    document,binary=glb(path);triangles=0;bounds=[[math.inf,-math.inf] for _ in range(3)]
    def values(index,width):
        a=document['accessors'][index];v=document['bufferViews'][a['bufferView']]
        dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']]
        item=np.dtype(dtype).itemsize
        return np.ndarray((a['count'],width),dtype=dtype,buffer=binary,
            offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',item*width),item))
    for mesh in document['meshes']:
        for primitive in mesh['primitives']:
            assert primitive.get('mode',4)==4
            assert 'COLOR_0' not in primitive['attributes']
            triangles+=document['accessors'][primitive['indices']]['count']//3
            a=document['accessors'][primitive['attributes']['POSITION']]
            for key,width in [('POSITION',3),('NORMAL',3),('TEXCOORD_0',2),('TANGENT',4)]:
                assert key in primitive['attributes'],(path,key)
                assert np.isfinite(values(primitive['attributes'][key],width)).all(),(path,'nonfinite',key)
            assert values(primitive['indices'],1).max()<a['count'],(path,'index outside vertex stream')
            for k in range(3):bounds[k]=[min(bounds[k][0],a['min'][k]),max(bounds[k][1],a['max'][k])]
    images=[]
    for image in document.get('images',[]):
        assert 'uri' not in image
        view=document['bufferViews'][image['bufferView']];offset=view.get('byteOffset',0)
        encoded=binary[offset:offset+view['byteLength']]
        size=Image.open(io.BytesIO(encoded)).size;assert max(size)<=2048
        images.append(dict(sha256=hashlib.sha256(encoded).hexdigest(),dimensions=list(size),bytes=len(encoded)))
    assert not document.get('skins') and not document.get('animations')
    assert len(document['meshes'])==1 and 1<=len(document['materials'])<=8
    primitive_count=sum(len(mesh['primitives']) for mesh in document['meshes'])
    assert primitive_count<=8
    return dict(triangles=triangles,primitive_count=primitive_count,bounds_y_up=bounds,images=images,bytes=path.stat().st_size,sha256=sha(path))

def validate(manifest_path=None,stage=None):
    path=manifest_path or OUT/'purchased_village_manifest.json'
    manifest=json.loads(path.read_text());unique={}
    for row in manifest['assets']:
        model=OUT/(row['asset']+'.glb')
        if stage and (stage/model.name).exists():model=stage/model.name
        actual=inspect_glb(model)
        assert actual['sha256']==row['sha256'] and actual['triangles']<=row['budget']
        for image in actual['images']:unique[image['sha256']]=image
    gpu_bytes=sum(int(image['dimensions'][0])*int(image['dimensions'][1])*4*4/3 for image in unique.values())
    assert gpu_bytes<=ARCHITECTURE_GPU_BUDGET_MIB*1024*1024,(gpu_bytes/1024/1024,'MiB exceeds purchased architecture budget')
    print('VALID purchased architecture:',len(manifest['assets']),'GLBs,',len(unique),'unique images,',round(gpu_bytes/1024/1024,3),'MiB RGBA8+mips',flush=True)
    return gpu_bytes

def publish_staged():
    import shutil
    stage=SOURCE/'cooked'; path=stage/'purchased_village_manifest.json'
    manifest=json.loads(path.read_text()); validate(path,stage)
    old=json.loads((OUT/'purchased_village_manifest.json').read_text())
    old_rows={r['asset']:r for r in old['assets']}; changes=[]
    backup=SOURCE/'material-fidelity-before'; backup.mkdir(exist_ok=True)
    for row in manifest['assets']:
        source=stage/(row['asset']+'.glb'); target=OUT/source.name
        if not source.exists(): continue
        assert row['triangles']<=row['budget']
        previous=old_rows[row['asset']]
        assert max(abs(row['bounds_y_up'][i][j]-previous['bounds_y_up'][i][j]) for i in range(3) for j in range(2))<.0001
        saved=backup/target.name
        if not saved.exists(): shutil.copy2(target,saved)
        changes.append(dict(asset=row['asset'],before_sha256=sha(target),after_sha256=row['sha256'],
                            triangles=row['triangles'],primitives=row['primitive_count'],bounds_unchanged=True))
    facade_change=any(row.get('facade_repack') and (stage/(row['asset']+'.glb')).exists() for row in manifest['assets'])
    if facade_change:
        facade_backup=SOURCE/'facade-repack-before';facade_backup.mkdir(exist_ok=True)
        if not (facade_backup/'purchased_village_manifest.json').exists():shutil.copy2(OUT/'purchased_village_manifest.json',facade_backup/'purchased_village_manifest.json')
        changed_families={row['family'] for row in manifest['assets']if row.get('facade_repack') and (stage/(row['asset']+'.glb')).exists()}
        for row in old['assets']:
            if row['family'] in changed_families:
                name=row['asset']+'.glb'
                if not (facade_backup/name).exists():shutil.copy2(OUT/name,facade_backup/name)
    if not (backup/'purchased_village_manifest.json').exists():shutil.copy2(OUT/'purchased_village_manifest.json',backup/'purchased_village_manifest.json')
    # All stage/bounds checks pass before the first runtime mutation.
    for row in changes:
        name=row['asset']+'.glb';os.replace(stage/name,OUT/name)
    os.replace(path,OUT/path.name)
    (SOURCE/('facade-repack-publication.json' if facade_change else 'material-fidelity-publication.json')).write_text(json.dumps(dict(schema=1,recipe=RECIPE,changes=changes,
        source_originals_modified=False,full_architecture_unique_gpu_mib=manifest['gpu_rgba8_mip_bytes']/1024/1024),indent=2)+'\n')
    print('PUBLISHED',len(changes),'validated architecture LODs; original runtime assets retained in',backup,flush=True)

def repack_facade_uv(obj,original_materials):
    """Pack only source-equivalent non-tile charts, preserving repeated trim.

    Reuse requires the same material, complete source UV polygon modulo that
    material's exact repeat period, and identical corner paint values. No
    positional welding or decal-geometry changes occur. Only representatives
    are baked; duplicates receive their equivalent chart after the bake.
    """
    import bpy
    from fractions import Fraction
    source=obj.data.uv_layers['SourceUV'];target=obj.data.uv_layers['BakedUV']
    paint=obj.data.color_attributes[0] if obj.data.color_attributes else None
    groups={};records=[];surface_area=0.;material_period={}
    for index,material in enumerate(original_materials):
        key=material.name.split('.')[0];settings,tiling=material_source_settings(key)
        rates=[tiling]+[v for n,v in settings.get('scalars',{}).items() if 'tiling' in n.lower() and v>0]
        period=1
        for value in rates:period=math.lcm(period,Fraction(float(value)).limit_denominator(64).denominator)
        material_period[index]=period
    for p in obj.data.polygons:
        key=original_materials[p.material_index].name.split('.')[0]
        if key in TILE_KEYS:continue
        loops=list(p.loop_indices);period=material_period[p.material_index]
        shift=[math.floor((min(source.data[l].uv[k] for l in loops)+1e-7)/period)*period for k in range(2)]
        corners=[]
        for loop in loops:
            uv=source.data[loop].uv
            color=paint.data[loop if paint.domain=='CORNER' else obj.data.loops[loop].vertex_index].color if paint else (0,0,0,0)
            corners.append((round(float(uv[0])-shift[0],6),round(float(uv[1])-shift[1],6),*(float(c).hex() for c in color)))
        rotations=[(tuple(corners[r:]+corners[:r]),r) for r in range(len(corners))]
        canonical,rotation=min(rotations);signature=(key,len(loops),canonical)
        row=dict(polygon=p.index,loops=loops,rotation=rotation,signature=signature,area=float(p.area))
        records.append(row);surface_area+=p.area
        if signature not in groups or row['area']>groups[signature]['area']:groups[signature]=row
    representatives={row['polygon'] for row in groups.values()}
    # Closed, non-walkable houses contain extensive enclosed timber faces.
    # Give their islands a smaller share instead of spending equal texels on
    # surfaces never visible from outside. Five outward hemisphere probes keep
    # bevels/eaves that can be seen obliquely at exterior quality.
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    tree=BVHTree.FromPolygons([v.co.copy() for v in obj.data.vertices],
                             [list(p.vertices) for p in obj.data.polygons])
    max_distance=max(obj.dimensions)*4
    for row in records:
        p=obj.data.polygons[row['polygon']];center=sum((obj.data.vertices[v].co for v in p.vertices),Vector())/len(p.vertices)
        normal=p.normal.normalized();tangent=normal.cross(Vector((0,0,1) if abs(normal.z)<.9 else (0,1,0))).normalized();bitangent=normal.cross(tangent)
        directions=[normal,(normal+tangent*.9).normalized(),(normal-tangent*.9).normalized(),
                    (normal+bitangent*.9).normalized(),(normal-bitangent*.9).normalized()]
        row['exterior']=any(tree.ray_cast(center+direction*.003,direction,max_distance)[0] is None for direction in directions)
    group_exterior={key:False for key in groups}
    for row in records:group_exterior[row['signature']]=group_exterior[row['signature']] or row['exterior']
    # Source tile faces and duplicate faces must not write into the new bake.
    for loop in target.data:loop.uv=(10.,10.)
    # Edit-mode round-trips can discard loose vertices/reorder split data.
    # Project a scratch mesh and carry source loop IDs in a spare UV channel;
    # copy back only UVs so the published render geometry stays byte-stable.
    scratch=obj.copy();scratch.data=obj.data.copy();bpy.context.collection.objects.link(scratch)
    loop_ids=scratch.data.uv_layers.new(name='RepackOriginalLoop')
    for i,loop in enumerate(loop_ids.data):loop.uv=(float(i),0.)
    scratch.data.uv_layers.active=scratch.data.uv_layers['BakedUV']
    for p in scratch.data.polygons:p.select=p.index in representatives
    obj.select_set(False);scratch.select_set(True);bpy.context.view_layer.objects.active=scratch
    bpy.context.tool_settings.mesh_select_mode=(False,False,True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66.),island_margin=.003,area_weight=0.,correct_aspect=True,scale_to_bounds=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    projected=scratch.data.uv_layers['BakedUV'];loop_ids=scratch.data.uv_layers['RepackOriginalLoop']
    loop_polygon={loop:p.index for p in obj.data.polygons for loop in p.loop_indices}
    source_signature={row['polygon']:row['signature'] for row in records}
    selected=[p for p in scratch.data.polygons if p.select]
    parent={p.index:p.index for p in selected}
    def find(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    edges={}
    for p in selected:
        loops=list(p.loop_indices)
        for a,b in zip(loops,loops[1:]+loops[:1]):
            va,vb=scratch.data.loops[a].vertex_index,scratch.data.loops[b].vertex_index
            aa=(va,*(round(float(v),6) for v in projected.data[a].uv));bb=(vb,*(round(float(v),6) for v in projected.data[b].uv))
            key=tuple(sorted([aa,bb]))
            if key in edges:parent[find(p.index)]=find(edges[key])
            else:edges[key]=p.index
    islands={}
    for p in selected:islands.setdefault(find(p.index),[]).append(p)
    hidden_islands=0
    for faces in islands.values():
        exterior=any(group_exterior[source_signature[loop_polygon[int(round(loop_ids.data[p.loop_start].uv[0]))]]] for p in faces)
        if not exterior:
            hidden_islands+=1;loops=[loop for p in faces for loop in p.loop_indices]
            center=sum((projected.data[loop].uv for loop in loops),Vector((0,0)))/len(loops)
            for loop in loops:projected.data[loop].uv=center+(projected.data[loop].uv-center)*.18
    scratch.data.uv_layers.active=projected
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.uv.pack_islands(rotate=True,margin=.003,scale=True)
    bpy.ops.object.mode_set(mode='OBJECT')
    projected=scratch.data.uv_layers['BakedUV'];loop_ids=scratch.data.uv_layers['RepackOriginalLoop']
    for uv,identifier in zip(projected.data,loop_ids.data):target.data[int(round(identifier.uv[0]))].uv=uv.uv
    bpy.data.objects.remove(scratch,do_unlink=True)
    obj.select_set(True);bpy.context.view_layer.objects.active=obj
    final_uv=[loop.uv.copy() for loop in target.data]
    for row in records:
        rep=groups[row['signature']];count=len(row['loops'])
        for j,loop in enumerate(row['loops']):
            r=(j-row['rotation']+rep['rotation'])%count
            final_uv[loop]=target.data[rep['loops'][r]].uv.copy()
    unique_area=sum(row['area'] for row in groups.values())
    exterior_polygons={row['polygon'] for row in records if row['exterior']}
    facade_polygons={row['polygon'] for row in records};obj.data.calc_loop_triangles()
    texel_area=0.;exterior_texel_area=0.;triangle_area=0.;exterior_triangle_area=0.
    for tri in obj.data.loop_triangles:
        if tri.polygon_index not in facade_polygons:continue
        a,b,c=(final_uv[i] for i in tri.loops);u=b-a;v=c-a
        area=abs(u.x*v.y-u.y*v.x)*.5*2048**2
        texel_area+=area;triangle_area+=tri.area
        if tri.polygon_index in exterior_polygons:exterior_texel_area+=area;exterior_triangle_area+=tri.area
    report=dict(policy='Area-scaled source-equivalent facade charts; source UV/paint equality required for reuse',
        surface_area_m2=float(surface_area),representative_area_m2=unique_area,
        non_tile_polygons=len(records),representative_polygons=len(groups),
        reused_polygons=len(records)-len(groups),theoretical_2k_unique_area_density=math.sqrt(2048**2/max(unique_area,1e-9)),
        exterior_surface_area_m2=sum(row['area'] for row in records if row['exterior']),
        enclosed_surface_area_m2=sum(row['area'] for row in records if not row['exterior']),
        uv_islands=len(islands),enclosed_uv_islands=hidden_islands,enclosed_uv_linear_scale=.18,
        achieved_all_surface_texels_m=math.sqrt(texel_area/max(triangle_area,1e-9)),
        achieved_exterior_texels_m=math.sqrt(exterior_texel_area/max(exterior_triangle_area,1e-9)),
        tile_polygons_excluded_from_bake=True,decal_geometry_modified=False,
        source_repeat_periods={original_materials[i].name.split('.')[0]:period for i,period in material_period.items()})
    print('FACADE_REPACK',json.dumps(report),flush=True)
    return final_uv,report

def cook(families,stage_only=False,facade_near_only=False):
    import bpy
    from mathutils import Matrix,Vector
    from PIL import Image
    from array import array
    stage=SOURCE/'cooked';stage.mkdir(parents=True,exist_ok=True)
    baked=SOURCE/'baked';baked.mkdir(exist_ok=True)
    rows=[]
    published_rows={r['asset']:r for r in json.loads((OUT/'purchased_village_manifest.json').read_text())['assets']} if (OUT/'purchased_village_manifest.json').exists() else {}
    existing_manifest=stage/'purchased_village_manifest.json' if stage_only and (stage/'purchased_village_manifest.json').exists() else OUT/'purchased_village_manifest.json'
    if existing_manifest.exists():
        rows=[row for row in json.loads(existing_manifest.read_text())['assets'] if row['family'] not in families or facade_near_only and row['lod']=='mid']
    for family in families:
        source_name,near_budget,mid_budget=FAMILIES[family]
        source=SOURCE/'source_meshes'/source_name
        bpy.ops.wm.read_factory_settings(use_empty=True)
        scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=1
        scene.cycles.use_denoising=False;scene.cycles.use_preview_denoising=False
        scene.render.threads_mode='FIXED';scene.render.threads=2;scene.render.bake.margin=8;scene.render.bake.use_clear=False
        bpy.ops.import_scene.fbx(filepath=str(source),use_anim=False,use_custom_normals=True)
        meshes=[o for o in scene.objects if o.type=='MESH' and not o.name.startswith('UCX_') and ('_LOD0' in o.name or '_LOD' not in o.name)]
        assert len(meshes)==1,(family,[o.name for o in meshes]);obj=meshes[0]
        for other in list(scene.objects):
            if other!=obj:bpy.data.objects.remove(other,do_unlink=True)
        obj.data=obj.data.copy();obj.data.transform(obj.matrix_world);obj.matrix_world=Matrix.Identity(4)
        scene.view_layers[0].objects.active=obj;obj.select_set(True)
        def tris():return sum(len(p.vertices)-2 for p in obj.data.polygons)
        original_tris=tris()
        if original_tris>near_budget:
            mod=obj.modifiers.new('Native near triangle budget','DECIMATE');mod.ratio=near_budget/original_tris*.995
            bpy.ops.object.modifier_apply(modifier=mod.name)
        points=[v.co.copy() for v in obj.data.vertices]
        lo=Vector([min(p[k] for p in points) for k in range(3)]);hi=Vector([max(p[k] for p in points) for k in range(3)])
        obj.data.transform(Matrix.Translation(Vector([-(lo.x+hi.x)/2,-(lo.y+hi.y)/2,-lo.z])))
        obj.data.uv_layers[0].name='SourceUV'
        assert 'LightMapUV' in obj.data.uv_layers
        atlas=obj.data.uv_layers['LightMapUV'];atlas.name='BakedUV'
        assert min(min(v.uv) for v in atlas.data)>-.001 and max(max(v.uv) for v in atlas.data)<1.001
        obj.data.uv_layers.active=atlas;atlas.active_render=True
        original_materials=list(obj.data.materials);material_outputs=[];image_cache={};paint_used=[];mapping=[]
        source_uvs=[loop.uv.copy() for loop in obj.data.uv_layers['SourceUV'].data]
        source_polygon_materials=[p.material_index for p in obj.data.polygons]
        facade_final_uv=None;facade_repack=None
        if facade_near_only:facade_final_uv,facade_repack=repack_facade_uv(obj,original_materials)
        def image(stem,kind):
            key=stem+'_'+kind
            if key not in image_cache:
                path=SOURCE/'prepared'/(key+'.png')
                if not path.exists() and stem=='T_Sack_Bag_02' and kind in {'N','ARM'}:
                    return image('T_Sack_Bag_01',kind)
                if not path.exists():return None
                im=bpy.data.images.load(str(path),check_existing=True)
                # Unreal samples RGB and alpha independently, including opaque
                # thatch that stores coverage data in alpha. Prevent Blender
                # from premultiplying away valid RGB where source alpha is0.
                if kind=='BC':im.alpha_mode='CHANNEL_PACKED'
                # Both supplied RGB-mask textures retain the source Texture2D
                # default sRGB flag and Color sampler; match that decode here.
                im.colorspace_settings.name='sRGB' if kind in {'BC','Mask'} else 'Non-Color';image_cache[key]=im
            return image_cache[key]
        def tex(nodes,links,uv,stem,kind,tiling=1.):
            im=image(stem,kind)
            if not im:return None
            node=nodes.new('ShaderNodeTexImage');node.image=im;node.extension='REPEAT'
            vector=uv.outputs['UV']
            if tiling!=1.:
                scale=nodes.new('ShaderNodeVectorMath');scale.operation='SCALE';scale.inputs['Scale'].default_value=tiling;links.new(vector,scale.inputs[0]);vector=scale.outputs[0]
            links.new(vector,node.inputs['Vector']);return node
        for material in dict.fromkeys(original_materials):
            key=material.name.split('.')[0];stem=MATERIALS.get(key)
            assert stem or key in {'MM_Simple_Black','WorldGridMaterial'},(family,key)
            material.use_nodes=True;nodes=material.node_tree.nodes;nodes.clear();links=material.node_tree.links
            output=nodes.new('ShaderNodeOutputMaterial');bsdf=nodes.new('ShaderNodeBsdfPrincipled');links.new(bsdf.outputs['BSDF'],output.inputs['Surface'])
            uv=nodes.new('ShaderNodeUVMap');uv.uv_map='SourceUV'
            settings,tiling=material_source_settings(key)
            if key=='MI_Tileable_Wall_Plaster':tiling=settings['scalars']['Tex_Tiling_1']
            base=tex(nodes,links,uv,stem,'BC',tiling) if stem else None
            arm=tex(nodes,links,uv,stem,'ARM',tiling) if stem else None
            normal=tex(nodes,links,uv,stem,'N',tiling) if stem else None
            rgb=nodes.new('ShaderNodeRGB');rgb.outputs[0].default_value=(.012,.010,.008,1) if key=='MM_Simple_Black' else (.35,.32,.28,1)
            alpha=nodes.new('ShaderNodeValue');alpha.outputs[0].default_value=1
            base_socket=base.outputs['Color'] if base else rgb.outputs[0]
            # Source alpha can hold subsurface/utility data in opaque maps.
            # Only the source masked master uses it for geometry coverage.
            # In particular thatch bulk and fringe share a BC image but bulk
            # is opaque; globally masking both punched holes in distant roofs.
            alpha_socket=base.outputs['Alpha'] if base and settings.get('parent')=='MM_Master_Alpha' else alpha.outputs[0]
            arm_socket=arm.outputs['Color'] if arm else None
            normal_socket=normal.outputs['Color'] if normal else None
            def mix_color(a,b,weight):
                mix=nodes.new('ShaderNodeMixRGB');links.new(weight,mix.inputs[0]);links.new(a,mix.inputs[1]);links.new(b,mix.inputs[2]);return mix.outputs[0]
            def height_weight(phase,height,contrast):
                # Bounded native reconstruction of UE HeightLerp/CheapContrast.
                # It preserves phase0/1 endpoints and height at phase0.5. The
                # external Engine function asset is absent, so this is declared
                # as an approximation rather than claiming bit-identical UE.
                def math_node(operation,a,b=None):
                    node=nodes.new('ShaderNodeMath');node.operation=operation
                    for i,v in enumerate([a,b] if b is not None else [a]):
                        if hasattr(v,'node'):links.new(v,node.inputs[i])
                        else:node.inputs[i].default_value=v
                    return node
                phase_bias=math_node('SUBTRACT',math_node('MULTIPLY',phase,2.).outputs[0],1.).outputs[0]
                raw=math_node('ADD',height,phase_bias).outputs[0]
                multiplier=math_node('ADD',math_node('MULTIPLY',contrast,2.).outputs[0],1.).outputs[0]
                boosted=math_node('SUBTRACT',math_node('MULTIPLY',raw,multiplier).outputs[0],contrast);boosted.use_clamp=True
                return boosted.outputs[0]
            # The source wood mask modulates red vertex paint with the green
            # mask texture at 2x UV. Raw red previously made large timber areas
            # uniformly alternate/dark and erased the source's broken wear.
            if obj.data.color_attributes and key=='MI_Tileable_Wood_Trims_01':
                alternate=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_02' if 'Plaster' in key else 'T_Tileable_Wood_Trims_01_01','BC',settings.get('scalars',{}).get('Tex_Tiling_2',1.))
                if alternate:
                    attr=nodes.new('ShaderNodeVertexColor');attr.layer_name=obj.data.color_attributes[0].name
                    separate=nodes.new('ShaderNodeSeparateColor');links.new(attr.outputs['Color'],separate.inputs['Color'])
                    weight=separate.outputs['Red']
                    if 'Wood' in key:
                        mask=tex(nodes,links,uv,'T_RGB','Mask',2.);assert mask
                        channels=nodes.new('ShaderNodeSeparateColor');links.new(mask.outputs['Color'],channels.inputs[0])
                        multiply=nodes.new('ShaderNodeMath');multiply.operation='MULTIPLY';links.new(weight,multiply.inputs[0]);links.new(channels.outputs['Green'],multiply.inputs[1]);weight=multiply.outputs[0]
                    mix=nodes.new('ShaderNodeMixRGB');links.new(weight,mix.inputs[0]);links.new(base_socket,mix.inputs[1]);links.new(alternate.outputs['Color'],mix.inputs[2]);base_socket=mix.outputs[0];paint_used.append(key)
            if obj.data.color_attributes and key=='MI_Tileable_Wall_Plaster':
                attr=nodes.new('ShaderNodeVertexColor');attr.layer_name=obj.data.color_attributes[0].name
                paint=nodes.new('ShaderNodeSeparateColor');links.new(attr.outputs['Color'],paint.inputs[0])
                mask_red=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_01_RGB','Mask',settings['scalars']['Red_RGB_Mask_Tiling'])
                mask_blue=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_01_RGB','Mask',settings['scalars']['Blue_RGB_Mask_Tiling']);assert mask_red and mask_blue
                red=nodes.new('ShaderNodeSeparateColor');blue=nodes.new('ShaderNodeSeparateColor');links.new(mask_red.outputs['Color'],red.inputs[0]);links.new(mask_blue.outputs['Color'],blue.inputs[0])
                t1=settings['scalars']['Tex_Tiling_1'];t2=settings['scalars']['Tex_Tiling_2']
                alternate=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_02','BC',t2)
                third=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_01_01','BC')
                # The source color graph uses GREEN, not red vertex paint.
                # The supplied export paints red only: green0 means the clean
                # source plaster remains visible rather than the old red tint.
                color_first=height_weight(paint.outputs['Green'],blue.outputs['Blue'],red.outputs['Red'])
                color_second=height_weight(paint.outputs['Green'],red.outputs['Red'],1.)
                base_socket=mix_color(mix_color(base_socket,alternate.outputs['Color'],color_first),third.outputs['Color'],color_second)
                normal_first=height_weight(paint.outputs['Red'],blue.outputs['Blue'],1.)
                normal_second=color_second
                alternate_normal=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_02','N',t2)
                third_normal=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_02','N')
                normal_socket=mix_color(mix_color(normal_socket,alternate_normal.outputs['Color'],normal_first),third_normal.outputs['Color'],normal_second)
                # ARM1/ARM2 use the opposite texture-coordinate nodes in the
                # source master graph. Preserve those explicit connections.
                first_arm=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_01','ARM',t2)
                second_arm=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_02','ARM',t1)
                third_arm=tex(nodes,links,uv,'T_Tileable_Wall_Plaster_01','ARM')
                arm_socket=mix_color(mix_color(first_arm.outputs['Color'],second_arm.outputs['Color'],normal_first),third_arm.outputs['Color'],normal_second)
                paint_used.append(key)
            links.new(base_socket,bsdf.inputs['Base Color']);links.new(alpha_socket,bsdf.inputs['Alpha'])
            arm_rgb=nodes.new('ShaderNodeRGB');arm_rgb.outputs[0].default_value=(1,.83,0,1)
            arm_socket=arm_socket or arm_rgb.outputs[0]
            sep=nodes.new('ShaderNodeSeparateColor');links.new(arm_socket,sep.inputs[0]);links.new(sep.outputs['Green'],bsdf.inputs['Roughness']);links.new(sep.outputs['Blue'],bsdf.inputs['Metallic'])
            if normal_socket:
                normalmap=nodes.new('ShaderNodeNormalMap');normalmap.uv_map='SourceUV';links.new(normal_socket,normalmap.inputs['Color']);links.new(normalmap.outputs['Normal'],bsdf.inputs['Normal'])
            material_outputs.append((material,output,bsdf,base_socket,arm_socket,alpha_socket))
            mapping.append(dict(material=key,textures=stem or 'constant dark fill',source_instance=SOURCE_INSTANCES.get(key,key),source_texture_tiling=tiling,
                wood_blend='vertex red * green RGB mask at 2x UV' if 'Wood_Trims' in key else None,
                plaster_blend='Source graph color GREEN paint; normal/ARM RED paint; recovered RGB masks. HeightLerp endpoint-preserving native approximation.' if 'Wall_Plaster' in key else None))
        output_images={}
        base_size,normal_size,arm_size=(2048,1024,512) if facade_near_only else ATLAS_SIZES[family]
        for kind,size,color in [('base',base_size,(.02,.02,.02,0)),('normal',normal_size,(.5,.5,1,1)),('arm',arm_size,(1,.83,0,1)),('alpha',base_size,(0,0,0,1))]:
            target=bpy.data.images.new(f'{family}_{kind}',width=size,height=size,alpha=True,is_data=kind!='base')
            target.generated_color=color;target.colorspace_settings.name='sRGB' if kind=='base' else 'Non-Color'
            for material,output,bsdf,base_socket,arm_socket,alpha_socket in material_outputs:
                nodes=material.node_tree.nodes;links=material.node_tree.links
                node=nodes.new('ShaderNodeTexImage');node.image=target;nodes.active=node
                if kind=='normal':links.new(bsdf.outputs['BSDF'],output.inputs['Surface'])
                else:
                    emit=nodes.new('ShaderNodeEmission');links.new(base_socket if kind=='base' else arm_socket if kind=='arm' else alpha_socket,emit.inputs['Color']);links.new(emit.outputs[0],output.inputs['Surface'])
            bpy.ops.object.bake(type='NORMAL' if kind=='normal' else 'EMIT',margin=8,use_clear=False)
            output_path=baked/(family+('_facade_near' if facade_near_only else '')+'_'+kind+'.png');target.filepath_raw=str(output_path);target.file_format='PNG';target.save();output_images[kind]=output_path
            print('BAKED',family,kind,flush=True)
        # Alpha is baked independently so decals and torn cloth keep their
        # coverage while opaque structural surfaces remain fully covered.
        base_image=Image.open(output_images['base']).convert('RGBA');opacity=Image.open(output_images['alpha']).convert('RGB').getchannel('R')
        assert opacity.getextrema()[1]>240 and max(base_image.getextrema()[k][1] for k in range(3))>45,(family,'empty or black material bake')
        base_image.putalpha(opacity);base_image.save(output_images['base'],optimize=True)
        if facade_final_uv:
            for loop,uv in zip(obj.data.uv_layers['BakedUV'].data,facade_final_uv):loop.uv=uv
        for attribute in list(obj.data.color_attributes):obj.data.color_attributes.remove(attribute)
        for layer in list(obj.data.uv_layers):
            if layer.name!='BakedUV':obj.data.uv_layers.remove(layer)
        material=bpy.data.materials.new('Purchased_'+family);material.use_nodes=True
        nodes=material.node_tree.nodes;links=material.node_tree.links;bsdf=nodes.get('Principled BSDF')
        baked_images={}
        for kind in ['base','normal','arm']:
            im=bpy.data.images.load(str(output_images[kind]),check_existing=False);im.colorspace_settings.name='sRGB' if kind=='base' else 'Non-Color';im.pack()
            node=nodes.new('ShaderNodeTexImage');node.image=im;baked_images[kind]=node
        links.new(baked_images['base'].outputs['Color'],bsdf.inputs['Base Color']);links.new(baked_images['base'].outputs['Alpha'],bsdf.inputs['Alpha'])
        if hasattr(material,'surface_render_method'):material.surface_render_method='DITHERED'
        else:material.blend_method='CLIP';material.alpha_threshold=.4
        material.use_backface_culling=False
        normalmap=nodes.new('ShaderNodeNormalMap');links.new(baked_images['normal'].outputs['Color'],normalmap.inputs['Color']);links.new(normalmap.outputs['Normal'],bsdf.inputs['Normal'])
        sep=nodes.new('ShaderNodeSeparateColor');links.new(baked_images['arm'].outputs['Color'],sep.inputs[0]);links.new(sep.outputs['Green'],bsdf.inputs['Roughness']);links.new(sep.outputs['Blue'],bsdf.inputs['Metallic'])
        obj.data.materials.clear();obj.data.materials.append(material)
        for polygon in obj.data.polygons:polygon.material_index=0
        atlas_mesh=obj.data.copy()
        # Nearby roofs and stone retain their authored repeating UVs. Baking
        # a complete multi-storey house into one atlas formerly reduced a 2K
        # tile to a small fraction of the atlas, losing shingles/mortar relief.
        # Painted plaster/timber/decals remain in the bounded façade atlas.
        # Farther LODs keep their single draw and stable prefiltered atlas.
        tile_materials={}
        tile_keys=TILE_KEYS
        for i,old_material in enumerate(original_materials):
            key=old_material.name.split('.')[0]
            if key not in tile_keys:continue
            stem=MATERIALS[key];settings,tiling=material_source_settings(key)
            # Roof03 edge and bulk roof share the same material and imagebytes.
            masked=key=='mı_Tileable_Thached_Roof_Edge'
            signature=(stem,tiling,masked)
            if signature not in tile_materials:
                source_material=bpy.data.materials.new('Purchased_Tile_'+stem+('_Mask' if masked else ''));source_material.use_nodes=True
                ns=source_material.node_tree.nodes;ls=source_material.node_tree.links;shader=ns.get('Principled BSDF')
                texture_nodes={}
                for kind in ['BC','N','ARM']:
                    im=image(stem,kind);assert im,(family,stem,kind)
                    node=ns.new('ShaderNodeTexImage');node.image=im;im.pack();texture_nodes[kind]=node
                ls.new(texture_nodes['BC'].outputs['Color'],shader.inputs['Base Color'])
                if masked:
                    ls.new(texture_nodes['BC'].outputs['Alpha'],shader.inputs['Alpha'])
                    if hasattr(source_material,'surface_render_method'):source_material.surface_render_method='DITHERED'
                    else:source_material.blend_method='CLIP';source_material.alpha_threshold=.3333
                normalmap=ns.new('ShaderNodeNormalMap');ls.new(texture_nodes['N'].outputs['Color'],normalmap.inputs['Color']);ls.new(normalmap.outputs['Normal'],shader.inputs['Normal'])
                sep=ns.new('ShaderNodeSeparateColor');ls.new(texture_nodes['ARM'].outputs['Color'],sep.inputs[0]);ls.new(sep.outputs['Green'],shader.inputs['Roughness']);ls.new(sep.outputs['Blue'],shader.inputs['Metallic'])
                source_material.use_backface_culling=False
                obj.data.materials.append(source_material);tile_materials[signature]=len(obj.data.materials)-1
            slot=tile_materials[signature]
            for p,source_index in zip(obj.data.polygons,source_polygon_materials):
                if source_index!=i:continue
                p.material_index=slot
                for loop in p.loop_indices:obj.data.uv_layers[0].data[loop].uv=source_uvs[loop]*tiling
        for lod,budget in ([('near',near_budget)] if facade_near_only else [('near',near_budget),('mid',mid_budget)]):
            if lod=='mid':obj.data=atlas_mesh.copy()
            if lod=='mid' and tris()>budget:
                mod=obj.modifiers.new('Native middle triangle budget','DECIMATE');mod.ratio=budget/tris()*.995;bpy.ops.object.modifier_apply(modifier=mod.name)
            obj.data.validate(clean_customdata=False);obj.data.update()
            name='purchased_village_'+family+'_'+lod
            path=stage/(name+'.glb')
            bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',export_yup=True,export_animations=False,export_cameras=False,export_lights=False,export_tangents=True,export_colors=False,export_image_format='AUTO')
            doc,binary=glb(path)
            old=published_rows.get(name)
            if lod=='mid' and old and old['source_sha256']==sha(source) and old['budget']==budget:
                old_doc,old_binary=glb(OUT/path.name)
                doc,binary=retain_mid_geometry(old_doc,old_binary,doc,binary)
            geometry_cleanup=None
            if family=='house_d':binary,geometry_cleanup=remove_house_d_duplicate_plaster(doc,binary)
            for exported_material in doc['materials']:
                if exported_material['name']=='Purchased_'+family:
                    exported_material['alphaMode']='MASK';exported_material['alphaCutoff']=.4
                elif exported_material['name'].endswith('_Mask'):
                    exported_material['alphaMode']='MASK';exported_material['alphaCutoff']=.3333
                else:exported_material['alphaMode']='OPAQUE';exported_material.pop('alphaCutoff',None)
                mr=exported_material['pbrMetallicRoughness']['metallicRoughnessTexture']['index']
                exported_material['occlusionTexture']={'index':mr}
            encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4);binary+=b'\0'*((-len(binary))%4)
            path.write_bytes(struct.pack('<III',0x46546c67,2,28+len(encoded)+len(binary))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary)
            entry=inspect_glb(path);assert entry['triangles']<=budget
            expected={sha(output_images[k]) for k in ['base','normal','arm']}
            if lod=='near':
                for stem,_,_ in tile_materials:
                    for kind in ['BC','N','ARM']:expected.add(sha(SOURCE/'prepared'/(stem+'_'+kind+'.png')))
            assert {im['sha256'] for im in entry['images']}==expected,(family,'embedded PBR channel bytes changed')
            entry.update(asset=name,family=family,lod=lod,source=source_name,source_sha256=sha(source),source_objects=[obj.name],source_lod0_triangles=original_tris,budget=budget,material_mapping=mapping,vertex_paint_baked_as_layer_mask=sorted(set(paint_used)),license='User-provided purchased content; no open-license claim',recipe=RECIPE,atlas_sizes=list(ATLAS_SIZES[family]))
            if geometry_cleanup:entry['geometry_cleanup']=geometry_cleanup
            if facade_repack:entry['facade_repack']=facade_repack;entry['atlas_sizes']=[2048,1024,512]
            entry['surface_representation']='Authored repeating roof/stone UV0 + painted façade atlas' if lod=='near' and tile_materials else 'Single bounded façade atlas'
            entry.update(PHYSICAL.get(family,{}))
            rows.append(entry);print('COOKED',name,entry['triangles'],entry['bounds_y_up'],flush=True)
    manifest=dict(schema=1,source_archive=str(ARCHIVE),license='User-provided purchased content',assets=rows)
    staged_manifest=stage/'purchased_village_manifest.json';staged_manifest.write_text(json.dumps(manifest,indent=2)+'\n')
    manifest['gpu_rgba8_mip_bytes']=validate(staged_manifest,stage)
    if stage_only:
        staged_manifest.write_text(json.dumps(manifest,indent=2)+'\n')
        print('STAGED ONLY: runtime GLBs unchanged; validated outputs in',stage,flush=True)
        return
    # Publish validated outputs, then the manifest. Existing project assets
    # retain their names/contents and remain a fallback for layout comparisons.
    for row in rows:
        p=stage/(row['asset']+'.glb')
        if p.exists():os.replace(p,OUT/p.name)
    staged_manifest.write_text(json.dumps(manifest,indent=2)+'\n')
    os.replace(staged_manifest,OUT/'purchased_village_manifest.json')

def preview(families,stage_only=False):
    import bpy
    from mathutils import Vector
    from PIL import Image,ImageDraw
    dest=SOURCE/'previews'/('material-fidelity-stage' if stage_only else 'material-fidelity-before');dest.mkdir(parents=True,exist_ok=True)
    for family in families:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=8
        scene.cycles.use_denoising=False;scene.cycles.use_preview_denoising=False
        scene.render.threads_mode='FIXED';scene.render.threads=2;scene.render.resolution_x=420;scene.render.resolution_y=420;scene.render.resolution_percentage=100
        scene.world=bpy.data.worlds.new('Neutral asset QA daylight');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.42,.50,.62,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.5
        bpy.ops.import_scene.gltf(filepath=str((SOURCE/'cooked' if stage_only else OUT)/('purchased_village_'+family+'_near.glb')))
        meshes=[o for o in scene.objects if o.type=='MESH'];points=[o.matrix_world@Vector(p) for o in meshes for p in o.bound_box]
        lo=Vector([min(p[k] for p in points) for k in range(3)]);hi=Vector([max(p[k] for p in points) for k in range(3)])
        target=(lo+hi)/2;span=max(hi-lo)
        bpy.ops.object.light_add(type='SUN',location=(4,-6,10));sun=bpy.context.object;sun.data.energy=2.;sun.data.angle=.12;sun.rotation_euler=(math.radians(27),math.radians(-25),math.radians(-25))
        bpy.ops.object.camera_add();camera=bpy.context.object;camera.data.type='ORTHO';camera.data.ortho_scale=span*1.5;scene.camera=camera
        panels=[]
        for label,direction in [('front(-Z)',(0,1,.4)),('right(+X)',(1,0,.4)),('back(+Z)',(0,-1,.4)),('left(-X)',(-1,0,.4))]:
            camera.location=target+Vector(direction).normalized()*span*2.;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
            path=dest/(family+'_'+label.split('(')[0]+'.png');scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)
            panel=Image.open(path).convert('RGB');ImageDraw.Draw(panel).text((8,8),family+' '+label,fill='white');panels.append(panel)
        sheet=Image.new('RGB',(840,840));
        for i,panel in enumerate(panels):sheet.paste(panel,((i%2)*420,(i//2)*420))
        sheet.save(dest/(family+'_contact.png'));print('PREVIEW',family,flush=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare',action='store_true');parser.add_argument('--validate',action='store_true');parser.add_argument('--preview',action='store_true')
    parser.add_argument('--repair-duplicates',action='store_true')
    parser.add_argument('--publish-staged',action='store_true')
    parser.add_argument('--stage-only',action='store_true')
    parser.add_argument('--facade-near-only',action='store_true',help='Repack/bake non-tile near facade only; retain all mid files/images')
    parser.add_argument('--families',default=','.join(FAMILIES))
    argv=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else sys.argv[1:]
    args=parser.parse_args(argv)
    if args.publish_staged:publish_staged()
    elif args.repair_duplicates:repair_published_duplicates()
    elif args.prepare:prepare()
    elif args.validate:validate()
    else:
        families=args.families.split(',');assert all(k in FAMILIES for k in families)
        if args.preview:preview(families,args.stage_only)
        else:cook(families,args.stage_only,args.facade_near_only)

if __name__=='__main__':main()
