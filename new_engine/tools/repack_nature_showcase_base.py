#!/usr/bin/env python3
"""Replace isolated showcase GI base maps, preserving all geometry and lighting.

Compacts embedded bufferViews so superseded images do not consume GLB budget.
The original first-preview showcase catalog is never selected by default.
"""
from pathlib import Path
import argparse,hashlib,json,copy
from bake_nature_material_fidelity import glb_document
from align_nature_showcase_lods import write_glb

def repack(path,by_name,native_winter=False):
    doc,old=glb_document(path);replacement={};changed=[]
    for material in doc['materials']:
        info=by_name.get(material.get('name'))
        if not info or info.get('status')!='candidate':continue
        texture=material.get('pbrMetallicRoughness',{}).get('baseColorTexture')
        if not texture:continue
        image=doc['textures'][texture['index']]['source'];replacement[image]=Path(info['paths']['base']).read_bytes();changed.append(material['name'])
        if info.get('alphaMode'):material.update(alphaMode=info['alphaMode'],alphaCutoff=info['alphaCutoff'])
        lower=material.get('name','').lower()
        if native_winter and 'snow'in lower and 'nosnow'not in lower and 'snow_mountain'not in lower:
            material.setdefault('extensions',{})['VALWOOD_materials_winter']={'parameters':[1,.86,0,0]}
            if 'VALWOOD_materials_winter'not in doc.setdefault('extensionsUsed',[]):doc['extensionsUsed'].append('VALWOOD_materials_winter')
    # Preserve accessor storage bit for bit, including baked-light attributes,
    # invisible shared-floor datums, and their interleaving/stride definitions.
    binary=bytearray();views=[];mapping={};geometry_hash=hashlib.sha256()
    for accessor in doc['accessors']:
        if accessor.get('sparse'):raise ValueError('Unexpected sparse source accessor')
        old_index=accessor['bufferView']
        if old_index not in mapping:
            view=doc['bufferViews'][old_index];payload=old[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']];geometry_hash.update(payload)
            new_view=copy.deepcopy(view);new_view.update(buffer=0,byteOffset=len(binary));mapping[old_index]=len(views);views.append(new_view);binary.extend(payload);binary.extend(b'\0'*(-len(binary)%4))
        accessor['bufferView']=mapping[old_index]
    cache={}
    for index,image in enumerate(doc['images']):
        view=doc['bufferViews'][image['bufferView']];payload=replacement.get(index)
        if payload is None:payload=old[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
        digest=hashlib.sha256(payload).digest()
        if digest not in cache:
            cache[digest]=len(views);views.append({'buffer':0,'byteOffset':len(binary),'byteLength':len(payload)});binary.extend(payload);binary.extend(b'\0'*(-len(binary)%4))
        image['bufferView']=cache[digest]
    doc['bufferViews']=views;doc.setdefault('asset',{}).setdefault('extras',{})['valwood_shaderlab_color_srgb_corrected']=True
    write_glb(path,doc,binary)
    if path.stat().st_size>128*1024**2:raise ValueError('Corrected base maps exceed native GLB cap: '+str(path))
    # Verify exact accessor storage survived repacking.
    after,raw=glb_document(path);verified=hashlib.sha256();seen=set()
    for accessor in after['accessors']:
        index=accessor['bufferView']
        if index in seen:continue
        seen.add(index);view=after['bufferViews'][index];verified.update(raw[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']])
    if verified.digest()!=geometry_hash.digest():raise ValueError('Accessor data changed during image-only repack')
    return {'file':str(path),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'materials':changed,'geometry_and_baked_light_preserved':True}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--catalog',type=Path,required=True);parser.add_argument('--materials',type=Path,required=True);parser.add_argument('--native-winter',action='store_true');args=parser.parse_args()
    if args.catalog.parent.name=='showcase':raise ValueError('Initial native preview is frozen; use isolated showcase_gi catalog')
    results=json.loads(args.materials.read_text());by_name={r['name']:r for r in results.values()if r.get('status')=='candidate'};catalog=json.loads(args.catalog.read_text());files=[]
    for record in catalog:
        record['base_color_gamma_corrected']=True;record['native_snow_cover_adaptation']=args.native_winter;record['material_cache']=str(args.materials);record['file_sha256']=[]
        for file in record['files']:
            report=repack(Path(file),by_name,args.native_winter);files.append(report);record['file_sha256'].append(report['sha256'])
        print('Corrected source base maps:',record['name'],flush=True)
    args.catalog.write_text(json.dumps(catalog,indent=2)+'\n');(args.catalog.parent/'base-repack.json').write_text(json.dumps({'materials':str(args.materials),'files':files,'geometry_and_baked_light_preserved':True},indent=2)+'\n');print('Source base repack complete:',len(files),'GLBs')
if __name__=='__main__':main()
