#!/usr/bin/env python3
"""Retain source UV/paint on near facades; leave published mid LODs untouched.

Blender: --prepare-source exports decimated UV/paint/tangent metadata only.
Python: --stage matches exact current triangles and embeds shared PBR sources.
Python: --validate checks stage; --publish publishes only validated near files.
Purchased Downloads are read-only data. No external project scripts execute.
"""
from pathlib import Path
import argparse,copy,hashlib,io,json,math,os,shutil,struct,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import numpy as np
from PIL import Image
from build_purchased_village import ROOT,SOURCE,OUT,FAMILIES,MATERIALS,sha,glb,encode_glb,inspect_glb,validate
from validate_purchased_village_fidelity import array,image_data,key,sample

HOUSES=['house_a','house_b','house_c','house_d','house_e','house_f']
WOOD='MI_Tileable_Wood_Trims_01';PLASTER='MI_Tileable_Wall_Plaster'
DEST=SOURCE/'source-facades';STAGE=DEST/'staged';BACKUP=DEST/'before'
EXT='VALWOOD_materials_facade'

def prepare_source():
    import bpy
    from mathutils import Matrix,Vector
    DEST.mkdir(parents=True,exist_ok=True)
    for family in HOUSES:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        source_name,budget,_=FAMILIES[family];source=SOURCE/'source_meshes'/source_name
        bpy.ops.import_scene.fbx(filepath=str(source),use_anim=False,use_custom_normals=True)
        obj=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_') and ('_LOD0' in o.name or '_LOD' not in o.name)][0]
        for other in list(bpy.context.scene.objects):
            if other!=obj:bpy.data.objects.remove(other,do_unlink=True)
        obj.data=obj.data.copy();obj.data.transform(obj.matrix_world);obj.matrix_world=Matrix.Identity(4)
        bpy.context.view_layer.objects.active=obj;obj.select_set(True)
        tris=sum(len(p.vertices)-2 for p in obj.data.polygons)
        if tris>budget:
            mod=obj.modifiers.new('Native near triangle budget','DECIMATE');mod.ratio=budget/tris*.995
            bpy.ops.object.modifier_apply(modifier=mod.name)
        points=[v.co.copy() for v in obj.data.vertices]
        lo=Vector([min(p[k] for p in points) for k in range(3)]);hi=Vector([max(p[k] for p in points) for k in range(3)])
        obj.data.transform(Matrix.Translation(Vector([-(lo.x+hi.x)/2,-(lo.y+hi.y)/2,-lo.z])))
        original=obj.data.uv_layers[0]
        for layer in list(obj.data.uv_layers):
            if layer!=original:obj.data.uv_layers.remove(layer)
        obj.data.uv_layers.active=original;original.active_render=True
        # Metadata GLB has no source images; it records original material
        # slots and exact authored UV/corner paint after the same decimation.
        for material in obj.data.materials:
            material.use_nodes=True;material.node_tree.nodes.clear()
            ns=material.node_tree.nodes;bsdf=ns.new('ShaderNodeBsdfPrincipled');out=ns.new('ShaderNodeOutputMaterial')
            material.node_tree.links.new(bsdf.outputs[0],out.inputs['Surface'])
        path=DEST/(family+'_source.glb')
        bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',export_yup=True,export_animations=False,export_cameras=False,export_lights=False,export_tangents=True,export_colors=True)
        print('SOURCE_FACADE_METADATA',family,path.stat().st_size,flush=True)

def accessor(doc,binary,index,width):
    values=array(doc,binary,index,width).copy();a=doc['accessors'][index]
    if a.get('normalized'):
        values=values.astype(np.float32)/{5121:255.,5123:65535.}[a['componentType']]
    return values

def source_triangles(family):
    doc,binary=glb(DEST/(family+'_source.glb'));records={}
    for primitive in doc['meshes'][0]['primitives']:
        name=doc['materials'][primitive['material']]['name'].split('.')[0]
        if name not in {WOOD,PLASTER}:continue
        attrs=primitive['attributes'];xyz=accessor(doc,binary,attrs['POSITION'],3)
        uv=accessor(doc,binary,attrs['TEXCOORD_0'],2);tangent=accessor(doc,binary,attrs['TANGENT'],4)
        normal=accessor(doc,binary,attrs['NORMAL'],3)
        colors=accessor(doc,binary,attrs['COLOR_0'],4) if 'COLOR_0' in attrs else np.zeros((len(xyz),4),np.float32)
        assert np.max(np.abs(colors[:,1:3]))<1e-6,'Source green/blue paint requires a new documented layer contract'
        for tri in accessor(doc,binary,primitive['indices'],1).reshape(-1,3):
            p=xyz[tri];records.setdefault(key(p),[]).append(dict(material=name,xyz=p,normal=normal[tri],uv=uv[tri],tangent=tangent[tri],paint=colors[tri,0]))
    return records

def compact(doc,binary):
    used={a['bufferView'] for a in doc['accessors'] if 'bufferView' in a}
    used.update(im['bufferView'] for im in doc['images']);mapping={};views=[];packed=bytearray()
    # Remove unused accessors first: old atlas streams must not keep dead views.
    attributes={i for m in doc['meshes'] for p in m['primitives'] for i in p['attributes'].values()}
    attributes.update(p['indices'] for m in doc['meshes'] for p in m['primitives'])
    amap={i:j for j,i in enumerate(sorted(attributes))};doc['accessors']=[doc['accessors'][i] for i in sorted(attributes)]
    for m in doc['meshes']:
        for p in m['primitives']:
            p['attributes']={k:amap[i] for k,i in p['attributes'].items()};p['indices']=amap[p['indices']]
    used={a['bufferView'] for a in doc['accessors'] if 'bufferView' in a};used.update(im['bufferView'] for im in doc['images'])
    for i in sorted(used):
        v=doc['bufferViews'][i];off=v.get('byteOffset',0);packed+=b'\0'*((-len(packed))%4)
        mapping[i]=len(views);views.append(dict(v,byteOffset=len(packed)));packed+=binary[off:off+v['byteLength']]
    for a in doc['accessors']:a['bufferView']=mapping[a['bufferView']]
    for im in doc['images']:im['bufferView']=mapping[im['bufferView']]
    doc['bufferViews']=views;doc['buffers'][0]['byteLength']=len(packed)
    return doc,bytes(packed)

def stage():
    STAGE.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((OUT/'purchased_village_manifest.json').read_text());reports=[]
    for family in HOUSES:
        name='purchased_village_'+family+'_near';path=(BACKUP if (BACKUP/(name+'.glb')).exists() else OUT)/(name+'.glb')
        doc,before=glb(path);doc=copy.deepcopy(doc);binary=bytearray(before)
        sources=source_triangles(family);groups={WOOD:[],PLASTER:[]};retained=[];moved=0
        for primitive in doc['meshes'][0]['primitives']:
            mat=doc['materials'][primitive['material']]
            if mat['name'].startswith('Purchased_Tile_'):retained.append(primitive);continue
            attrs=primitive['attributes'];xyz=accessor(doc,before,attrs['POSITION'],3);uv=accessor(doc,before,attrs['TEXCOORD_0'],2)
            normal=accessor(doc,before,attrs['NORMAL'],3);tangent=accessor(doc,before,attrs['TANGENT'],4)
            remaining=[]
            for tri in accessor(doc,before,primitive['indices'],1).reshape(-1,3):
                points=xyz[tri];candidates=sources.get(key(points),[])
                if not candidates:remaining.extend(int(i) for i in tri);continue
                def candidate_score(r):
                    distances=np.sum((r['xyz'][:,None,:]-points[None,:,:])**2,axis=2)
                    return float(np.sum(np.min(distances,axis=0))),float(np.sum(np.abs(np.mean(r['normal'],axis=0)-np.mean(normal[tri],axis=0))))
                source=min(candidates,key=candidate_score)
                order=[int(np.argmin(np.sum((source['xyz']-p)**2,axis=1))) for p in points]
                assert len(set(order))==3 and np.max(np.abs(source['xyz'][order]-points))<2e-5
                assert np.max(np.abs(source['normal'][order]-normal[tri]))<.002
                for j,old_index in enumerate(tri):
                    groups[source['material']].append(np.concatenate([points[j],normal[old_index],source['uv'][order[j]],source['tangent'][order[j]],[source['paint'][order[j]]]]).astype('<f4'))
                moved+=1
            if remaining:
                indices=np.asarray(remaining,dtype='<u4');binary+=b'\0'*((-len(binary))%4)
                view=len(doc['bufferViews']);doc['bufferViews'].append(dict(buffer=0,byteOffset=len(binary),byteLength=indices.nbytes,target=34963));binary+=indices.tobytes()
                a=len(doc['accessors']);doc['accessors'].append(dict(bufferView=view,componentType=5125,count=len(indices),type='SCALAR',min=[int(indices.min())],max=[int(indices.max())]))
                retained.append(dict(primitive,indices=a))
        assert moved>100,(family,'No source facade triangle matches')
        # Only residual signs/windows/metal/decals retain the previous atlas.
        # Its old full-building UVs stay exact; primary facade detail is shared
        # tile data, so a smaller residual atlas does not dilute wood/plaster.
        def append_image(encoded,label):
            digest=hashlib.sha256(encoded).hexdigest()
            for i,im in enumerate(doc['images']):
                v=doc['bufferViews'][im['bufferView']];off=v.get('byteOffset',0)
                if hashlib.sha256(binary[off:off+v['byteLength']]).hexdigest()==digest:
                    return i
            binary.extend(b'\0'*((-len(binary))%4));v=len(doc['bufferViews'])
            doc['bufferViews'].append(dict(buffer=0,byteOffset=len(binary),byteLength=len(encoded)));binary.extend(encoded)
            i=len(doc['images']);doc['images'].append(dict(name=label,mimeType='image/png',bufferView=v));return i
        def texture_from_bytes(encoded,label):
            image=append_image(encoded,label)
            for i,t in enumerate(doc['textures']):
                if t['source']==image:return i
            doc['textures'].append(dict(source=image,sampler=0));return len(doc['textures'])-1
        def source_texture(stem,kind):
            p=SOURCE/'prepared'/(stem+'_'+kind+'.png');assert p.exists(),p
            return texture_from_bytes(p.read_bytes(),p.stem)
        for mat in doc['materials']:
            if mat['name']!='Purchased_'+family:continue
            pbr=mat['pbrMetallicRoughness']
            for kind,size,slot in [('base',1024,pbr['baseColorTexture']),('normal',512,mat['normalTexture']),('arm',256,pbr['metallicRoughnessTexture'])]:
                pixels,encoded=image_data(doc,before,slot['index']);image=Image.open(io.BytesIO(encoded)).convert('RGBA')
                image=image.resize((size,size),Image.Resampling.LANCZOS);data=io.BytesIO();image.save(data,format='PNG',optimize=True)
                slot['index']=texture_from_bytes(data.getvalue(),family+'_residual_'+kind)
            mat['occlusionTexture']=dict(index=pbr['metallicRoughnessTexture']['index'])
        for material,vertices in groups.items():
            if not vertices:continue
            wood=material==WOOD;mode=1 if wood else 2
            stem='T_Tileable_Wood_Trims_01' if wood else 'T_Tileable_Wall_Plaster_01'
            bc=source_texture(stem,'BC');n=source_texture(stem,'N');arm=source_texture(stem,'ARM')
            aux=[source_texture('T_Tileable_Wood_Trims_01_01','BC'),source_texture('T_RGB','Mask'),-1] if wood else [source_texture('T_Tileable_Wall_Plaster_02','N'),source_texture('T_Tileable_Wall_Plaster_02','ARM'),source_texture('T_Tileable_Wall_Plaster_01_RGB','Mask')]
            scales=[1.,1.,2.] if wood else [1.2000000476837158,1.5,1.2000000476837158]
            mat_index=len(doc['materials']);doc['materials'].append(dict(name='Purchased_Source_'+('Wood' if wood else 'Plaster'),alphaMode='OPAQUE',doubleSided=True,
                pbrMetallicRoughness=dict(baseColorTexture=dict(index=bc),metallicRoughnessTexture=dict(index=arm),metallicFactor=1,roughnessFactor=1),normalTexture=dict(index=n),occlusionTexture=dict(index=arm),
                extensions={EXT:dict(parameters=[1,mode,*aux,*scales])}))
            unique={};packed=[];indices=[]
            for vertex in vertices:
                signature=vertex.tobytes()
                if signature not in unique:unique[signature]=len(packed);packed.append(vertex)
                indices.append(unique[signature])
            data=np.asarray(packed,dtype='<f4');binary+=b'\0'*((-len(binary))%4);view=len(doc['bufferViews'])
            doc['bufferViews'].append(dict(buffer=0,byteOffset=len(binary),byteLength=data.nbytes,byteStride=52,target=34962));binary+=data.tobytes();attrs={}
            for label,width,offset,typ in [('POSITION',3,0,'VEC3'),('NORMAL',3,12,'VEC3'),('TEXCOORD_0',2,24,'VEC2'),('TANGENT',4,32,'VEC4'),('_VALWOOD_PAINT',1,48,'SCALAR')]:
                a=dict(bufferView=view,byteOffset=offset,componentType=5126,count=len(data),type=typ)
                if label=='POSITION':a.update(min=data[:,:3].min(axis=0).tolist(),max=data[:,:3].max(axis=0).tolist())
                attrs[label]=len(doc['accessors']);doc['accessors'].append(a)
            data=np.asarray(indices,dtype='<u4');binary+=b'\0'*((-len(binary))%4);view=len(doc['bufferViews'])
            doc['bufferViews'].append(dict(buffer=0,byteOffset=len(binary),byteLength=data.nbytes,target=34963));binary+=data.tobytes()
            index=len(doc['accessors']);doc['accessors'].append(dict(bufferView=view,componentType=5125,count=len(data),type='SCALAR',min=[int(data.min())],max=[int(data.max())]))
            retained.append(dict(attributes=attrs,indices=index,material=mat_index,mode=4))
        doc['meshes'][0]['primitives']=retained;doc.setdefault('extensionsUsed',[]).append(EXT)
        # Drop unused old image/material entries before compacting their views.
        mats=sorted({p['material'] for p in retained});mmap={i:j for j,i in enumerate(mats)};doc['materials']=[doc['materials'][i] for i in mats]
        for p in retained:p['material']=mmap[p['material']]
        textures=set()
        for m in doc['materials']:
            textures.update([m['pbrMetallicRoughness']['baseColorTexture']['index'],m['pbrMetallicRoughness']['metallicRoughnessTexture']['index'],m['normalTexture']['index']])
            textures.update(i for i in m.get('extensions',{}).get(EXT,{}).get('parameters',[0,0,-1,-1,-1])[2:5] if i>=0)
        tmap={i:j for j,i in enumerate(sorted(textures))};doc['textures']=[doc['textures'][i] for i in sorted(textures)]
        for m in doc['materials']:
            for slot in [m['pbrMetallicRoughness']['baseColorTexture'],m['pbrMetallicRoughness']['metallicRoughnessTexture'],m['normalTexture'],m['occlusionTexture']]:slot['index']=tmap[slot['index']]
            if EXT in m.get('extensions',{}):
                params=m['extensions'][EXT]['parameters'];params[2:5]=[tmap[i] if i>=0 else -1 for i in params[2:5]]
        images=sorted({t['source'] for t in doc['textures']});imap={i:j for j,i in enumerate(images)};doc['images']=[doc['images'][i] for i in images]
        for t in doc['textures']:t['source']=imap[t['source']]
        doc,binary=compact(doc,binary);target=STAGE/path.name;target.write_bytes(encode_glb(doc,binary))
        entry=inspect_glb(target);old=next(r for r in manifest['assets'] if r['asset']==name)
        assert entry['triangles']==old['triangles'] and entry['bounds_y_up']==old['bounds_y_up']
        old.update(entry);old.update(recipe=6,atlas_sizes=[1024,512,256],surface_representation='Shared original source-UV wood/plaster PBR with preserved red paint; source roof/stone; bounded residual atlas',source_facade=dict(schema=1,metadata=sha(DEST/(family+'_source.glb')),source_uv_and_paint=True,green_blue_paint_zero=True,source_triangles=moved,residual_atlas_sizes=[1024,512,256]))
        reports.append(dict(family=family,source_triangles=moved,triangles=entry['triangles'],primitives=entry['primitive_count']))
        print('STAGED_SOURCE_FACADE',family,moved,'source triangles',entry['primitive_count'],'primitives',flush=True)
    manifest['gpu_rgba8_mip_bytes']=sum(im['dimensions'][0]*im['dimensions'][1]*4*4/3 for im in {im['sha256']:im for r in manifest['assets'] for im in r['images']}.values())
    (STAGE/'purchased_village_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('STAGE_BUDGET_MIB',manifest['gpu_rgba8_mip_bytes']/1024/1024,flush=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for flag in ['prepare-source','stage','validate','publish']:parser.add_argument('--'+flag,action='store_true')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else None)
    if args.prepare_source:prepare_source()
    if args.stage:stage()
    if args.validate or args.publish:
        from validate_purchased_village_source_facades import validate_stage
        validate_stage()
    if args.publish:
        BACKUP.mkdir(parents=True,exist_ok=True)
        if not (BACKUP/'purchased_village_manifest.json').exists():shutil.copy2(OUT/'purchased_village_manifest.json',BACKUP/'purchased_village_manifest.json')
        for family in HOUSES:
            name='purchased_village_'+family+'_near.glb'
            if not (BACKUP/name).exists():shutil.copy2(OUT/name,BACKUP/name)
        for family in HOUSES:
            name='purchased_village_'+family+'_near.glb';os.replace(STAGE/name,OUT/name)
        os.replace(STAGE/'purchased_village_manifest.json',OUT/'purchased_village_manifest.json')
        print('PUBLISHED_SOURCE_FACADES source Downloads and all mid files unchanged',flush=True)

if __name__=='__main__':main()
