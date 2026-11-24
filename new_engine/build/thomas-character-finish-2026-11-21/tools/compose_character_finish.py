"""Compose reviewed authored facial geometry and the Thomas brunette hair recipe."""
from pathlib import Path
import argparse
import copy
import hashlib
import io
import json
import struct
from PIL import Image

N=Path(__file__).resolve().parents[1]
B=N.with_name('thomas-polish-2026-11-21')
TIERS=('Low','Medium','High','Ultra')

def need(ok, message):
    if not ok: raise RuntimeError(message)

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def digest(data): return hashlib.sha256(data).hexdigest()
def read(p): return json.loads(Path(p).read_text())
def save(p,d):
    p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
    t=p.with_suffix(p.suffix+'.tmp');t.write_text(json.dumps(d,indent=2)+'\n');t.replace(p)

def glb(p):
    data=Path(p).read_bytes()
    magic,version,total=struct.unpack_from('<4sII',data)
    length,kind=struct.unpack_from('<II',data,12)
    need(magic==b'glTF' and version==2 and total==len(data) and kind==0x4e4f534a,'Invalid GLB '+str(p))
    offset=20+length; size,kind=struct.unpack_from('<II',data,offset)
    need(kind==0x004e4942 and offset+8+size==len(data),'Expected embedded BIN '+str(p))
    return json.loads(data[20:20+length]),data[offset+8:]

def write_glb(p,doc,data):
    doc['buffers'][0]['byteLength']=len(data)
    head=json.dumps(doc,separators=(',',':')).encode();head+=b' '*((-len(head))%4)
    data+=b'\0'*((-len(data))%4)
    payload=struct.pack('<4sII',b'glTF',2,28+len(head)+len(data))+struct.pack('<II',len(head),0x4e4f534a)+head+struct.pack('<II',len(data),0x004e4942)+data
    p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(payload)
    return payload

def view_bytes(doc,data,index):
    view=doc['bufferViews'][index];start=view.get('byteOffset',0)
    return data[start:start+view['byteLength']]

def hair(doc,data,recipe,tier):
    doc=copy.deepcopy(doc);before=copy.deepcopy(doc)
    indices=[i for i,m in enumerate(doc['materials']) if m.get('name')=='Thomas_Tom_Hair_M']
    need(len(indices)==1,'Expected one explicit legacy Thomas hair material')
    material_index=indices[0];m=doc['materials'][material_index]
    roles={'base':m['pbrMetallicRoughness']['baseColorTexture']['index'],'normal':m['normalTexture']['index']}
    images={role:doc['textures'][index]['source'] for role,index in roles.items()}
    replacements={}
    for role,index in images.items():
        rec=recipe['tiers'][tier][role];path=Path(rec['file'])
        need(sha(path)==rec['sha256'],'Hair bake changed: '+str(path))
        image=doc['images'][index];view=image['bufferView'];payload=path.read_bytes()
        need(view not in replacements,'Hair images unexpectedly share one buffer view')
        replacements[view]=payload;image['mimeType']=rec['mimeType']
        image['name']='Thomas_HDRP_'+role
    # None of the replaced images may be used by another material.
    def textures(value):
        found=[]
        if isinstance(value,dict):
            for key,v in value.items():
                if key.endswith('Texture') and isinstance(v,dict) and 'index' in v: found.append(v['index'])
                else: found.extend(textures(v))
        elif isinstance(value,list):
            for v in value: found.extend(textures(v))
        return found
    for i,other in enumerate(doc['materials']):
        if i!=material_index:
            need(not set(images.values()).intersection(doc['textures'][t]['source'] for t in textures(other)),'Hair image shared with another material')
    desired=recipe['material'];m['name']=desired['name']
    for key in ('baseColorFactor','metallicFactor','roughnessFactor'):m['pbrMetallicRoughness'][key]=desired[key]
    m['normalTexture']['scale']=desired['normalScale']
    for key in ('alphaMode','alphaCutoff','doubleSided'):m[key]=desired[key]
    output=bytearray();preserved={}
    for index,view in enumerate(doc['bufferViews']):
        old=view_bytes(before,data,index);payload=replacements.get(index,old)
        output+=b'\0'*((-len(output))%4);view['byteOffset']=len(output);view['byteLength']=len(payload);output+=payload
        if index not in replacements: preserved[str(index)]=digest(old)
    for index in range(len(doc['bufferViews'])):
        if index not in replacements:need(digest(view_bytes(doc,output,index))==preserved[str(index)],'A facial/geometry buffer changed during hair merge')
    for index,old in enumerate(before['materials']):
        if index!=material_index:need(doc['materials'][index]==old,'Nonhair material changed')
    for key in ('meshes','nodes','skins','accessors','animations'):
        need(before.get(key)==doc.get(key),'Geometry/morph metadata changed during hair merge')
    return doc,bytes(output),dict(material_index=material_index,image_indices=images,preserved_buffer_view_sha256=preserved,
        geometry_morph_skin_uv_metadata_identical=True,other_materials_identical=True)

def mip_bytes(w,h,channels):
    total=0
    while True:
        total+=w*h*channels
        if w==h==1:return total
        w=max(1,w//2);h=max(1,h//2)

def measure(doc,data):
    textures=[];encoded=decoded=gpu=0
    for image in doc['images']:
        payload=view_bytes(doc,data,image['bufferView']);encoded+=len(payload)
        with Image.open(io.BytesIO(payload)) as im:
            w,h=im.size;c=len(im.getbands());decoded+=w*h*c;gpu+=mip_bytes(w,h,4)
        textures.append(dict(width=w,height=h,channels=c,encoding=image['mimeType'],encoded_bytes=len(payload),sha256=digest(payload)))
    primitives=[p for mesh in doc['meshes'] for p in mesh['primitives']]
    counts=dict(vertices=sum(doc['accessors'][p['attributes']['POSITION']]['count'] for p in primitives),
                indices=sum(doc['accessors'][p['indices']]['count'] for p in primitives),
                triangles=sum(doc['accessors'][p['indices']]['count']//3 for p in primitives),
                encoded_image_bytes=encoded,decoded_texture_bytes=decoded,conservative_gpu_rgba8_with_mips_bytes=gpu)
    return counts,textures

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--hair-preview',action='store_true');parser.add_argument('--compose',action='store_true');a=parser.parse_args()
    need(a.hair_preview!=a.compose,'Choose one composition mode')
    recipe_path=N/'review/hair-assets/HAIR_RECIPE.json';recipe=read(recipe_path);need(recipe['passed'],'Hair recipe failed')
    for path,h in recipe['source_inputs_sha256'].items():need(sha(path)==h,'Hair recipe source changed: '+path)
    if a.hair_preview:
        original=B/'assets/character-quality/High/story_hero/civilian/body.glb';doc,data=glb(original)
        doc,data,proof=hair(doc,data,recipe,'High');target=N/'review/hair-assets/preview/High/story_hero/civilian/body.glb';write_glb(target,doc,data)
        save(target.with_name('HAIR_PREVIEW.json'),dict(passed=True,original=str(original),original_sha256=sha(original),file=str(target),sha256=sha(target),recipe_sha256=sha(recipe_path),**proof))
        print(target);return
    face_path=N/'review/facial-assets/FACIAL_BANK.json';face=read(face_path);need(face['passed'] and len(face['rows'])==8,'Facial bank incomplete')
    qb=read(B/'assets/character-quality/QUALITY_BUILD.json');qv=read(B/'assets/character-quality/QUALITY_VALIDATION.json');rows={};staged=[]
    for row in face['rows']:
        tier,outfit=row['tier'],row['outfit'];source=Path(row['path']);need(sha(source)==row['sha256'],'Facial body changed')
        target=N/'assets/character-quality'/tier/'story_hero'/outfit/'body.glb'
        need(sha(target)==row['baseline_sha256'],'Final bank is no longer its reviewed baseline')
        doc,data=glb(source);doc,data,proof=hair(doc,data,recipe,tier)
        stage=N/'review/composed-assets'/tier/outfit/'body.glb'
        payload=write_glb(stage,doc,data);staged.append((stage,target))
        counts,images=measure(doc,data);file_bytes=len(payload);gpu=counts['conservative_gpu_rgba8_with_mips_bytes']
        need(file_bytes<=64*1024*1024 and counts['encoded_image_bytes']<=32*1024*1024 and counts['vertices']<=200000 and counts['indices']<=600000,'Native model safety budget exceeded')
        old=copy.deepcopy(qb['tiers'][tier][outfit]);record=qb['tiers'][tier][outfit]
        record.update(file=str(target),sha256=sha(stage),file_bytes=file_bytes,**{k:v for k,v in counts.items() if k!='conservative_gpu_rgba8_with_mips_bytes'})
        record['geometry']=[dict(primitive=i,material=doc['materials'][p['material']]['name'],vertices=doc['accessors'][p['attributes']['POSITION']]['count'],triangles=doc['accessors'][p['indices']]['count']//3,requested_ratio=1 if p.get('targets') else old['geometry'][i].get('requested_ratio',1)) for i,p in enumerate(doc['meshes'][0]['primitives'])]
        for item in record.get('textures',[]):
            if item['material']=='Thomas_Tom_Hair_M' and item['role'] in ('base','normal'):
                image=images[proof['image_indices'][item['role']]];item.update(image,material=recipe['material_identity'],source_paths=list(recipe['source_inputs_sha256']))
        record['facial_targets']=face['keys'];record['facial_bank_sha256']=sha(face_path);record['hdrp_hair_recipe_sha256']=sha(recipe_path)
        valid=qv['bodies'][tier+'/'+outfit];prior=copy.deepcopy(valid)
        valid.update(vertices=counts['vertices'],triangles=counts['triangles'],file_bytes=file_bytes,encoded_image_bytes=counts['encoded_image_bytes'],decoded_texture_bytes=counts['decoded_texture_bytes'],conservative_gpu_rgba8_with_mips_bytes=gpu,sha256=sha(stage))
        valid['normal_compression_error']=[v for v in valid.get('normal_compression_error',[]) if v['material']!='Thomas_Tom_Hair_M']
        for weapon,composed in valid['composed_weapon_budgets'].items():
            composed['encoded_image_bytes_upper_bound']=counts['encoded_image_bytes']+prior['composed_weapon_budgets'][weapon]['encoded_image_bytes_upper_bound']-prior['encoded_image_bytes']
            composed['vertices_upper_bound']=counts['vertices']+prior['composed_weapon_budgets'][weapon]['vertices_upper_bound']-prior['vertices']
            composed['indices']=counts['indices']+prior['composed_weapon_budgets'][weapon]['indices']-old['indices']
            composed['gpu_rgba8_with_mips_bytes_upper_bound']=gpu+prior['composed_weapon_budgets'][weapon]['gpu_rgba8_with_mips_bytes_upper_bound']-prior['conservative_gpu_rgba8_with_mips_bytes']
            need(composed['encoded_image_bytes_upper_bound']<=32*1024*1024 and composed['vertices_upper_bound']<=200000 and composed['indices']<=600000 and composed['gpu_rgba8_with_mips_bytes_upper_bound']<=valid['native_profile_texture_budget_bytes'],'Composed weapon budget exceeded')
        rows[tier+'/'+outfit]=dict(passed=True,file=str(target),sha256=sha(stage),facial_source=str(source),facial_source_sha256=sha(source),file_bytes=file_bytes,**counts,**proof)
    for outfit in ('civilian','armored'):
        t=[rows[tier+'/'+outfit]['triangles'] for tier in TIERS];need(t[0]<t[1]<t[2]<=t[3],'Quality geometry ordering changed')
    for path in [face_path,recipe_path,Path(__file__)]:qb['source_inputs'][str(path)]=sha(path)
    qb['source_inputs'].update(recipe['source_inputs_sha256'])
    for tier in recipe['tiers'].values():
        for role in ('base','normal'):qb['source_inputs'][tier[role]['file']]=tier[role]['sha256']
    revision=dict(facial_source_sha256=sha(face_path),hair_recipe_sha256=sha(recipe_path),policy='Restore authored facial geometry only where required on Low/Medium, retain garment LODs, add sparse authored facial targets; replace only explicit Thomas hair material/maps with the reviewed source brunette/HDRP response hybrid. Preserve all other material/texture payloads and all geometry/morph data during hair merge.')
    qb['finish_revision']=revision;qv['finish_revision']=revision
    qv['material_preservation_exceptions'].append(dict(material=recipe['material_identity'],reason='Reviewed brunette source hybrid with HDRP fiber response',recipe_sha256=sha(recipe_path)))
    # Every body and composed weapon budget passed before changing the final bank.
    for stage,target in staged:
        temporary=target.with_suffix('.glb.tmp');temporary.write_bytes(stage.read_bytes());temporary.replace(target)
    save(N/'assets/character-quality/QUALITY_BUILD.json',qb);save(N/'assets/character-quality/QUALITY_VALIDATION.json',qv)
    save(N/'review/CHARACTER_ASSET_COMPOSITION.json',dict(schema='valwood.character-finish.asset-composition.v1',passed=True,facial_bank_sha256=sha(face_path),hair_recipe_sha256=sha(recipe_path),rows=rows))
    print(json.dumps({key:{k:value[k] for k in ('vertices','triangles','file_bytes','encoded_image_bytes')} for key,value in rows.items()},indent=2))

if __name__=='__main__':main()
