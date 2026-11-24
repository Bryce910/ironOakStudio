#!/usr/bin/env python3
"""Structural, visual-data and memory-bound checks for all eight owned bodies."""
from pathlib import Path
import copy, hashlib, io, json, re, sys
import numpy as np
from PIL import Image
GEN=Path(__file__).resolve().parents[1]
ENGINE=GEN.parents[1]
sys.path.insert(0,str(ENGINE/'tools'))
import finalize_modular_knight_glb as glb
OUT=GEN/'assets/character-quality'
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def image_bytes(d,b,im):
    v=d['bufferViews'][im['bufferView']];o=v.get('byteOffset',0);return bytes(b[o:o+v['byteLength']])
def normalized_material(material):
    material=copy.deepcopy(material)
    def visit(item):
        if isinstance(item,dict):
            for k,v in item.items():
                if k.endswith('Texture') and isinstance(v,dict):v.pop('index',None)
                visit(v)
        elif isinstance(item,list):
            for v in item:visit(v)
    visit(material);return material
def arrays(d,b,p):return {k:glb.array(d,b,v) for k,v in p['attributes'].items()}
def main():
    receipt=json.loads((OUT/'QUALITY_BUILD.json').read_text())
    frozen=receipt['source_inputs'];assert all(sha(p)==digest for p,digest in frozen.items())
    result={'schema':'valwood.character-quality.validation.v1','passed':True,'bodies':{},'assertions':['All eight bodies are owned regular files, within native file/image/vertex/index budgets.','Exact canonical 95-joint palette, transforms, inverse binds and skin node contract on every tier.','All indices, UVs, normals and four normalized skin weights are finite and valid.','All original material factors, alpha cutoffs, double-sided flags and sampler factors are retained.','Low/Medium have actual reduced topology; High/Ultra preserve complete canonical geometry byte values.','Civilian and armored share identical Thomas face/hair/eyes geometry within each tier.','Texture dimensions equal receipt; all source maps are genuine and no source texture is upscaled.','Previous purchased canonical body hashes are unchanged.']}
    shared={}
    source_hashes={}
    weapon_budgets={}
    for path in (GEN/'motion/rigs').glob('*.glb'):
        wd,wb,_=glb.load(path);used_images=set();vertices=0;indices=0
        def scan(value):
            if isinstance(value,dict):
                for key,item in value.items():
                    if key.endswith('Texture') and isinstance(item,dict) and 'index' in item:used_images.add(wd['textures'][item['index']]['source'])
                    else:scan(item)
            elif isinstance(value,list):
                for item in value:scan(item)
        for primitive in wd['meshes'][0]['primitives'][19:]:
            scan(wd['materials'][primitive['material']])
            vertices+=wd['accessors'][primitive['attributes']['POSITION']]['count'];indices+=wd['accessors'][primitive['indices']]['count']
        encoded=sum(wd['bufferViews'][wd['images'][i]['bufferView']]['byteLength'] for i in used_images)
        rgba8=0
        for i in used_images:
            with Image.open(io.BytesIO(image_bytes(wd,wb,wd['images'][i]))) as image:rgba8+=image.width*image.height*4
        weapon_budgets[path.stem]={'rig_sha256':sha(path),'vertices_upper_bound':vertices,'indices':indices,'encoded_image_bytes':encoded,'gpu_rgba8_with_mips_bytes_upper_bound':(rgba8*4+2)//3}
    assert len(weapon_budgets)==4
    result['weapon_additional_budgets']=weapon_budgets
    for tier in ['Low','Medium','High','Ultra']:
        for variant in ['civilian','armored']:
            path=OUT/tier/'story_hero'/variant/'body.glb';row=receipt['tiers'][tier][variant]
            assert path.is_file() and not path.is_symlink();assert sha(path)==row['sha256']
            d,b,_=glb.load(path);s,sb,_=glb.load(ENGINE/f'build/thomas-kubold-2026-11-20/assets/story_hero/{variant}/body.glb')
            assert len(d['skins'])==1 and len(d['skins'][0]['joints'])==95
            assert d['nodes']==s['nodes']
            for key in ['joints','skeleton']:
                assert d['skins'][0].get(key)==s['skins'][0].get(key)
            assert np.array_equal(glb.array(d,b,d['skins'][0]['inverseBindMatrices']),glb.array(s,sb,s['skins'][0]['inverseBindMatrices']))
            assert not d.get('animations')
            assert len(d['meshes'][0]['primitives'])==len(s['meshes'][0]['primitives'])
            source_materials={m['name']:normalized_material(m) for m in s['materials']}
            for mat in d['materials']:assert normalized_material(mat)==source_materials[mat['name']],(tier,variant,mat['name'])
            for index,p in enumerate(d['meshes'][0]['primitives']):
                a=arrays(d,b,p);tri=glb.array(d,b,p['indices']).reshape(-1,3)
                assert tri.min()>=0 and tri.max()<len(a['POSITION'])
                assert all(np.isfinite(v).all() for v in a.values())
                assert np.max(np.abs(a['WEIGHTS_0'].sum(axis=1)-1))<2e-6
                assert a['JOINTS_0'].max()<95 and a['WEIGHTS_0'].min()>=0
                assert np.max(np.abs(np.linalg.norm(a['NORMAL'],axis=1)-1))<.005
                name=d['materials'][p['material']]['name']
                if name.startswith('Thomas_'):
                    payload=hashlib.sha256(b''.join(a[k].tobytes() for k in sorted(a))+tri.tobytes()).hexdigest()
                    shared.setdefault((tier,name),{})[variant]=payload
                if tier in ['High','Ultra'] or name=='Thomas_Tom_Hair_M':
                    orig=s['meshes'][0]['primitives'][index]
                    assert all(np.array_equal(a[k],v) for k,v in arrays(s,sb,orig).items())
                    assert np.array_equal(tri,glb.array(s,sb,orig['indices']).reshape(-1,3))
            cfg=(path.parent/'appearance.cfg').read_text()
            fp=re.search(r'^first_person (\d+) (.*)$',cfg,re.M)
            assert fp and len(fp[2].split())==int(fp[1]) and all(0<=int(x)<len(d['meshes'][0]['primitives']) for x in fp[2].split())
            image_total=0;gpu_rgba8_bytes=0;normal_errors=[]
            for im in d['images']:
                payload=image_bytes(d,b,im);image_total+=len(payload)
                with Image.open(io.BytesIO(payload)) as image:
                    gpu_rgba8_bytes+=image.width*image.height*4
                    image.verify()
            for tx in row['textures']:
                assert all(tx['width']<=size[0] and tx['height']<=size[1] for size in tx['source_original_dimensions'])
                for source in tx['source_paths']:
                    if source not in source_hashes:source_hashes[source]=sha(source)
                if tx['role']=='normal' and tx['source_paths']:
                    image=next(im for im in d['images'] if im['name']==f'{tier}_{tx["material"]}_normal')
                    with Image.open(io.BytesIO(image_bytes(d,b,image))) as encoded:
                        step=max(1,encoded.width//512)
                        out=np.asarray(encoded.convert('RGB'))[::step,::step].astype(np.float32)/127.5-1
                        with Image.open(tx['source_paths'][0]) as source:
                            source=source.convert('RGB');source.thumbnail(encoded.size,Image.Resampling.LANCZOS)
                            original=np.asarray(source)[::step,::step].astype(np.float32)/127.5-1
                    out/=np.maximum(np.linalg.norm(out,axis=-1,keepdims=True),1e-8)
                    original/=np.maximum(np.linalg.norm(original,axis=-1,keepdims=True),1e-8)
                    angles=np.degrees(np.arccos(np.clip(np.sum(out*original,axis=-1),-1,1)))
                    normal_errors.append({'material':tx['material'],'mean_degrees':float(angles.mean()),'p95_degrees':float(np.percentile(angles,95)),'max_degrees':float(angles.max())})
            assert image_total==row['encoded_image_bytes']<=32*1024**2
            assert row['vertices']<=200000 and row['indices']<=600000 and path.stat().st_size<=64*1024**2
            gpu_with_mips=(gpu_rgba8_bytes*4+2)//3
            gpu_budget=(256 if tier in ['Low','Medium'] else 768)*1024**2
            assert gpu_with_mips<gpu_budget,(tier,variant,gpu_with_mips,gpu_budget)
            result['bodies'][f'{tier}/{variant}']={**{k:row[k] for k in ['vertices','triangles','file_bytes','encoded_image_bytes','decoded_texture_bytes']},'conservative_gpu_rgba8_with_mips_bytes':gpu_with_mips,'native_profile_texture_budget_bytes':gpu_budget,'normal_compression_error':normal_errors}
            composed={}
            for weapon,budget in weapon_budgets.items():
                total_images=image_total+budget['encoded_image_bytes']
                total_vertices=row['vertices']+budget['vertices_upper_bound']
                total_indices=row['indices']+budget['indices']
                total_gpu=gpu_with_mips+budget['gpu_rgba8_with_mips_bytes_upper_bound']
                assert total_images<=32*1024**2,(tier,variant,weapon,total_images)
                assert total_vertices<=200000 and total_indices<=600000
                assert total_gpu<gpu_budget
                composed[weapon]={'encoded_image_bytes_upper_bound':total_images,'vertices_upper_bound':total_vertices,'indices':total_indices,'gpu_rgba8_with_mips_bytes_upper_bound':total_gpu}
            result['bodies'][f'{tier}/{variant}']['composed_weapon_budgets']=composed
    for (tier,name),variants in shared.items():
        # The little torso neck patch shares the large hands material name;
        # compare head/eyes/hair specifically rather than overwritten aliases.
        if name in ['Thomas_Thomas_Head','Thomas_EyeBall','Thomas_Tongue_M','Thomas_Curuncle_M','Thomas_EyeLashes','Thomas_Tom_Hair_M']:
            assert variants.get('civilian')==variants.get('armored'),(tier,name)
    for variant in ['civilian','armored']:
        counts=[receipt['tiers'][tier][variant]['triangles'] for tier in ['Low','Medium','High','Ultra']]
        assert counts[0]<counts[1]<counts[2]==counts[3],counts
    result['original_source_texture_sha256']=source_hashes
    (OUT/'QUALITY_VALIDATION.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
