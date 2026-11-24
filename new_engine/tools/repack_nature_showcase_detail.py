#!/usr/bin/env python3
"""Stage three selected 2K material replacements without touching imports.

All accessor storage, source anchors, GI, alpha and winter/lava metadata remain
unchanged. Embedded image payloads are compacted and content-deduplicated.
"""
from pathlib import Path
import argparse,copy,hashlib,json
from bake_nature_material_fidelity import glb_document,ROOT
from align_nature_showcase_lods import write_glb

def sha(value):return hashlib.sha256(value).hexdigest()
def accessor_hash(doc,binary):
    result=hashlib.sha256();seen=set()
    for accessor in doc['accessors']:
        if accessor.get('sparse'):raise ValueError('Sparse source accessor is unsupported')
        index=accessor['bufferView']
        if index in seen:continue
        seen.add(index);view=doc['bufferViews'][index];offset=view.get('byteOffset',0)
        result.update(binary[offset:offset+view['byteLength']])
    return result.hexdigest()

def candidate(source,output,materials,keep_arm=False):
    doc,old=glb_document(source);replacement={};changed=[]
    for material in doc['materials']:
        info=materials.get(material.get('name'))
        if not info:continue
        pbr=material['pbrMetallicRoughness']
        roles=[('base',pbr['baseColorTexture']),('normal',material['normalTexture'])]
        if not keep_arm:roles.append(('arm',pbr['metallicRoughnessTexture']))
        for role,texture in roles:
            image=doc['textures'][texture['index']]['source'];path=Path(info['paths'][role]);payload=path.read_bytes()
            mime='image/jpeg'if path.suffix.lower()in('.jpg','.jpeg')else'image/png'
            if image in replacement and replacement[image]!=(payload,mime):raise ValueError('Conflicting shared image replacement')
            replacement[image]=(payload,mime)
        changed.append(info['name'])
    if not changed:return None
    before=accessor_hash(doc,old)
    for index,(_,mime)in replacement.items():doc['images'][index]['mimeType']=mime
    original_doc=copy.deepcopy(doc)
    binary=bytearray();views=[];mapping={}
    for accessor in doc['accessors']:
        index=accessor['bufferView']
        if index not in mapping:
            view=doc['bufferViews'][index];offset=view.get('byteOffset',0);payload=old[offset:offset+view['byteLength']]
            new=copy.deepcopy(view);new.update(buffer=0,byteOffset=len(binary));mapping[index]=len(views);views.append(new)
            binary.extend(payload);binary.extend(b'\0'*(-len(binary)%4))
        accessor['bufferView']=mapping[index]
    cache={};old_images={};new_images={}
    for index,image in enumerate(doc['images']):
        view=doc['bufferViews'][image['bufferView']];offset=view.get('byteOffset',0);original=old[offset:offset+view['byteLength']]
        payload=replacement[index][0]if index in replacement else original;digest=sha(payload)
        if index in replacement:old_images[sha(original)]=len(original);new_images[digest]=len(payload)
        if digest not in cache:
            cache[digest]=len(views);views.append({'buffer':0,'byteOffset':len(binary),'byteLength':len(payload)})
            binary.extend(payload);binary.extend(b'\0'*(-len(binary)%4))
        image['bufferView']=cache[digest]
    doc['bufferViews']=views
    output.parent.mkdir(parents=True,exist_ok=True);write_glb(output,doc,binary)
    if output.stat().st_size>128*1024**2:
        size=output.stat().st_size;output.unlink();raise ValueError('Selected 2K textures exceed native 128 MiB cap: '+str(source)+' -> '+str(round(size/1024**2,2))+' MiB')
    after,raw=glb_document(output)
    if accessor_hash(after,raw)!=before:raise ValueError('Geometry/GI accessor storage changed')
    # Only buffer storage offsets may change; all rendering metadata is exact.
    def semantic(value):
        value=copy.deepcopy(value);value.pop('buffers',None);value.pop('bufferViews',None)
        for a in value['accessors']:a.pop('bufferView',None)
        for im in value['images']:im.pop('bufferView',None)
        return value
    if semantic(original_doc)!=semantic(after):raise ValueError('Material/source/LOD metadata changed')
    return {'source':str(source),'candidate':str(output),'source_sha256':sha(source.read_bytes()),'sha256':sha(output.read_bytes()),'source_bytes':source.stat().st_size,'bytes':output.stat().st_size,'materials':changed,'all_accessor_bytes_unchanged':True,'all_render_metadata_unchanged':True,'unique_replaced_encoded_bytes_before':sum(old_images.values()),'unique_replaced_encoded_bytes_after':sum(new_images.values())}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--catalog',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-buttress-gi/catalog.json')
    ap.add_argument('--materials',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-closeup-materials/materials.json')
    ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-closeup-2k')
    ap.add_argument('--material',action='append',choices=['M_Pillar_stone_01','M_Pillar_stone_02','M_Wall_03_Main'],default=[])
    ap.add_argument('--keep-arm',action='store_true',help='Retain exact existing 512px source ARM maps to bound embedded payloads')
    args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    materials={r['name']:r for r in json.loads(args.materials.read_text()).values()if r.get('status')=='candidate'}
    expected={'M_Pillar_stone_01','M_Pillar_stone_02','M_Wall_03_Main'}
    if set(materials)!=expected:raise ValueError('Expected exactly the three verified closeup materials')
    if args.material:materials={name:info for name,info in materials.items()if name in args.material}
    catalog=copy.deepcopy(json.loads(args.catalog.read_text()));files=[]
    for record in catalog:
        changed=False
        for lod,path in enumerate(record['files']):
            report=candidate(Path(path),args.output/'chunks'/Path(path).name,materials,args.keep_arm)
            if not report:continue
            record['files'][lod]=report['candidate'];files.append(report);changed=True
            print('DETAIL2K GLB',Path(path).name,round(report['bytes']/1024**2,2),'MiB',flush=True)
        if changed:
            record['file_sha256']=[sha(Path(path).read_bytes())for path in record['files']]
            record['selected_2k_material_cache']=str(args.materials.resolve())
            record['selected_2k_roles']=['base','normal']if args.keep_arm else['base','normal','arm']
    (args.output/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n')
    report={'source_catalog':str(args.catalog.resolve()),'materials':str(args.materials.resolve()),'selected_material_names':sorted(materials),'selected_roles':['base','normal']if args.keep_arm else['base','normal','arm'],'files':files,'changed_aliases':sum(any(str(args.output.resolve())in str(Path(f).resolve())for f in r['files'])for r in catalog),'max_candidate_bytes':max((f['bytes']for f in files),default=0),'geometry_GI_bounds_and_render_metadata_preserved':True,'production_imports_and_cache_untouched':True}
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('READY',len(files),'files',report['changed_aliases'],'aliases',flush=True)

if __name__=='__main__':main()
