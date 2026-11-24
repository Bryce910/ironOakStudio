#!/usr/bin/env python3
"""Build four private Thomas quality tiers from purchased, unchanged sources.

High/Ultra retain the complete canonical purchased mesh. Lower tiers use real
Blender topology collapse with corner UV/custom normal and skin interpolation.
All outputs retain the exact runtime skeleton and palette; no source is edited.
"""
from pathlib import Path
import argparse, copy, hashlib, importlib.util, io, json, re, subprocess, sys
import numpy as np
from PIL import Image, ImageOps, ImageFile
ImageFile.MAXBLOCK=16*1024*1024

GEN = Path(__file__).resolve().parents[1]
ENGINE = GEN.parents[1]
PREVIOUS = ENGINE/'build/thomas-kubold-2026-11-20'
INTAKE = ENGINE/'assets/characters/incoming/thomas_kubold/inspection'
OUT = GEN/'assets/character-quality'
WORK = GEN/'tools/quality_asset_work'
sys.path.insert(0, str(ENGINE/'tools'))
import finalize_modular_knight_glb as glb
spec = importlib.util.spec_from_file_location('compact_body', ENGINE/'build/plain-father-2026-11-18/tools/build_story_hero.py')
old = importlib.util.module_from_spec(spec); spec.loader.exec_module(old)
MATERIALS = json.loads((INTAKE/'materials_converted.json').read_text())['materials']
TIERS = {
    'Low': {'base':512, 'normal':256, 'orm':256},
    'Medium': {'base':1024, 'normal':512, 'orm':512},
    'High': {'base':2048, 'normal':1024, 'orm':512},
    'Ultra': {'base':2048, 'normal':2048, 'orm':1024},
}
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def acc(d,b,a,kind,comp=5126):
    a = np.ascontiguousarray(a, dtype={5126:'<f4',5123:'<u2',5125:'<u4'}[comp])
    if a.ndim==1:a=a[:,None]
    i = glb.append_accessor(d,b,a,{'componentType':comp,'type':kind})
    if kind=='VEC3': d['accessors'][i].update(min=a.min(0).tolist(),max=a.max(0).tolist())
    return i
def resized(im,limit):
    im=im.copy(); im.thumbnail((limit,limit),Image.Resampling.LANCZOS);return im
def source_image(path,limit,mode='RGB'):
    with Image.open(path) as image: return resized(image.convert(mode),limit)
def texture_data(material_name,role,tier,embedded):
    """Rebuild source Unity texture semantics at genuine source resolutions."""
    name=material_name.removeprefix('Thomas_') if material_name.startswith('Thomas_') else None
    row=MATERIALS.get(name)
    limit=TIERS[tier][role]
    # These tiny eye/mouth surfaces occupy little screen area even in a close
    # camera. Keep their source resolution budget for the visible face/outfit.
    if name in ['EyeBall','EyeLashes','Tongue_M']:limit=min(limit,1024)
    # Hair cards encode separate strands in alpha. Their full topology and a
    # 1K minimum opacity map are identity critical even on a low-end computer.
    if name=='Tom_Hair_M' and role=='base':limit=max(limit,1024)
    if tier=='Ultra' and name=='Thomas_Head' and role=='base':limit=4096
    sources=[]
    if row:
        tex=row['source_textures']
        if role=='base' and name=='Tom_Hair_M':
            root=Path(row['unity_material']).parents[4]
            # Material path is Thomas/Materials/Built-in_Materials/Hairstyles/Thomas_Hair/...
            root=INTAKE.parent/'thomas/Assets/David Grette/Adventure_Pack/Thomas'
            albedo=root/'Texture/Hair/Thomas/T_Thomas_Hair_albedo.PNG'
            alpha=root/'Texture/Hair/Thomas/T_Thomas_Hair_alpha.PNG'
            im=source_image(albedo,limit,'RGBA');im.putalpha(source_image(alpha,limit,'RGB').getchannel('R'));sources=[str(albedo),str(alpha)]
        elif role=='base' and name=='EyeLashes':
            sources=[tex['_MainTex']];alpha=source_image(sources[0],limit,'L');im=Image.new('RGBA',alpha.size,(255,255,255,255));im.putalpha(alpha)
        elif role=='base':sources=[tex['_MainTex']];im=source_image(sources[0],limit)
        elif role=='normal':sources=[tex['_BumpMap']];im=source_image(sources[0],limit)
        else:
            # Standard shader source channels: R metallic, G occlusion, A smoothness.
            packed_path=tex.get('_MetallicGlossMap');ao_path=tex.get('_OcclusionMap')
            if packed_path:
                packed=source_image(packed_path,limit,'RGBA');rough=ImageOps.invert(packed.getchannel('A'));metal=packed.getchannel('R');sources.append(packed_path)
            else:
                size=source_image(ao_path,limit).size if ao_path else (1,1)
                authored=Path(row['unity_material']).read_text()
                scalars={k:float(v) for k,v in re.findall(r'^    - (_\w+): ([0-9.eE+-]+)$',authored,re.M)}
                rough=Image.new('L',size,round((1-scalars.get('_Glossiness',.5))*255));metal=Image.new('L',size,round(scalars.get('_Metallic',0)*255))
            if ao_path:
                ao=source_image(ao_path,limit,'RGBA').getchannel('G');sources.append(ao_path)
                if ao.size!=rough.size:ao=ao.resize(rough.size,Image.Resampling.LANCZOS)
            else:ao=Image.new('L',rough.size,255)
            im=Image.merge('RGB',(ao,rough,metal))
    else:
        with Image.open(io.BytesIO(embedded)) as image:im=resized(image,limit)
    # RGB maps use high-quality no-chroma-subsampling JPEG. This bounds native
    # encoded-image memory while preserving normal tangent channels in linear RGB.
    # MASK hair/lashes retain lossless alpha. No normal green channel is flipped.
    payload=io.BytesIO()
    if im.mode=='RGBA':im.save(payload,format='PNG',optimize=True);mime='image/png'
    else:
        quality=92 if tier=='Ultra' and role=='normal' and name!='Thomas_Head' else 96 if role=='normal' else 94
        if tier=='Ultra' and role=='normal' and name in ['Tom_Pants_M','Tom_Boots_M']:quality=90
        im.convert('RGB').save(payload,format='JPEG',quality=quality,subsampling=0,optimize=True);mime='image/jpeg'
    return payload.getvalue(),mime,{'material':material_name,'role':role,'width':im.width,'height':im.height,'source_paths':sources,'source_original_dimensions':[list(Image.open(x).size) for x in sources],'encoding':mime}
def ratio(tier,name,index,variant):
    if tier not in ['Low','Medium']:return 1
    low=tier=='Low'
    if 'Thomas_Head' in name:return .72 if low else 1
    if any(k in name for k in ['EyeBall','Curuncle','EyeLashes']):return 1
    if 'Tongue' in name:return .10 if low else .25
    if 'Hair' in name:return 1
    if name=='Thomas_Thomas_Torso':return .65 if low else .85
    if variant=='civilian' and index==10:return .5 if low else .8
    if 'Gloves' in name or 'Mittens' in name:return .6 if low else .85
    return .18 if low else .45
def arrays(d,b,primitive):
    result={k:glb.array(d,b,v) for k,v in primitive['attributes'].items()}
    result['triangles']=glb.array(d,b,primitive['indices']).reshape(-1,3);return result
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tier',choices=list(TIERS))
    parser.add_argument('--variant',choices=['civilian','armored'])
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True);WORK.mkdir(parents=True,exist_ok=True)
    sources={v:PREVIOUS/f'assets/story_hero/{v}/body.glb' for v in ['civilian','armored']}
    frozen={str(p):sha(p) for p in sources.values()}
    docs={v:glb.load(path)[:2] for v,path in sources.items()}
    # A converter change must never silently reuse stale flat/invalid normals.
    cache_contract=hashlib.sha256((sha(GEN/'tools/quality_asset_decimate.py')+json.dumps(frozen,sort_keys=True)).encode()).hexdigest()
    cache_receipt=WORK/'worker-contract.sha256'
    if not cache_receipt.exists() or cache_receipt.read_text().strip()!=cache_contract:
        for cached in WORK.glob('*.npz'):
            if cached.name.startswith(('Low-','Medium-')):cached.unlink()
    jobs=[]
    for variant,(d,b) in docs.items():
        for index,primitive in enumerate(d['meshes'][0]['primitives']):
            name=d['materials'][primitive['material']]['name'];input=f'{variant}-{index}.npz'
            np.savez_compressed(WORK/input,**arrays(d,b,primitive))
            for tier in ['Low','Medium']:
                r=ratio(tier,name,index,variant)
                if r<1:jobs.append(dict(key=f'{tier}-{variant}-{index}',input=input,output=f'{tier}-{variant}-{index}.npz',ratio=r))
    (WORK/'jobs.json').write_text(json.dumps(jobs,indent=2)+'\n')
    missing=[job for job in jobs if not (WORK/job['output']).exists()]
    if missing:
        (WORK/'jobs.json').write_text(json.dumps(missing,indent=2)+'\n')
        subprocess.run(['blender','--background','--factory-startup','--python',str(GEN/'tools/quality_asset_decimate.py'),'--',str(WORK)],check=True)
    cache_receipt.write_text(cache_contract+'\n')
    report={'schema':'valwood.character-quality.v1','source_inputs':frozen,'tiers':{},'skeleton_contract':'Exact canonical 95-joint nodes, palette and inverse binds; no animation/gameplay changes.','method':'High/Ultra retain exact canonical geometry. Low/Medium use offline Blender Decimate collapse per material primitive, with corner UV/normal data and normalized interpolated skin weights. Eyes remain full detail; head/hands are protected relative to clothes.','texture_conversion':'Actual purchased Unity maps only; never upscaled. JPEG RGB uses 4:4:4 quality 94, normal quality 96 (Ultra nonface normal quality 92 for the native encoded image budget). Linear normal channels preserved without green inversion; alpha cards use PNG. Standard smoothness A becomes roughness 1-A; AO G becomes R; metallic R becomes B. Small eye/mouth surfaces cap at 1K.','limitations':['Neutral expression only; runtime does not consume source facial blendshapes.','Cloth helper bones remain conformed to canonical anatomical rig; dynamic cloth is not simulated.']}
    if (args.tier or args.variant) and (OUT/'QUALITY_BUILD.json').exists():report['tiers']=json.loads((OUT/'QUALITY_BUILD.json').read_text())['tiers']
    report['texture_conversion']+=' Ultra pants/boots normals use quality 90 to reserve the known equipped-weapon encoded-image budget.'
    report['method']+=' Hair cards retain exact full canonical topology on every tier, with minimum 1K alpha; reducing cards changed the silhouette. Lower body meshes use smooth polygons to preserve continuous surface shading after collapse.'
    for tier in ([args.tier] if args.tier else TIERS):
        report['tiers'].setdefault(tier,{})
        for variant in ([args.variant] if args.variant else docs):
            source_d,source_b=docs[variant]
            d=copy.deepcopy(source_d);b=bytearray(source_b);texture_rows=[];cache={};geometry=[]
            for index,primitive in enumerate(d['meshes'][0]['primitives']):
                r=ratio(tier,d['materials'][primitive['material']]['name'],index,variant)
                if r<1:
                    a=np.load(WORK/f'{tier}-{variant}-{index}.npz')
                    assert all(np.isfinite(a[k]).all() for k in a.files),(tier,variant,index,'non-finite LOD')
                    assert np.min(np.linalg.norm(a['NORMAL'],axis=1))>1e-5,(tier,variant,index,'zero normal')
                    primitive['attributes']={k:acc(d,b,a[k],kind,comp) for k,kind,comp in [('POSITION','VEC3',5126),('NORMAL','VEC3',5126),('TEXCOORD_0','VEC2',5126),('JOINTS_0','VEC4',5123),('WEIGHTS_0','VEC4',5126)]}
                    primitive['indices']=acc(d,b,a['triangles'].reshape(-1),'SCALAR',5125)
                geometry.append({'primitive':index,'material':d['materials'][primitive['material']]['name'],'vertices':d['accessors'][primitive['attributes']['POSITION']]['count'],'triangles':d['accessors'][primitive['indices']]['count']//3,'requested_ratio':r})
            for material in d['materials']:
                pbr=material.setdefault('pbrMetallicRoughness',{})
                for role,container,key in [('base',pbr,'baseColorTexture'),('normal',material,'normalTexture'),('orm',pbr,'metallicRoughnessTexture')]:
                    if key not in container:continue
                    tx=container[key]['index'];im=d['images'][d['textures'][tx]['source']];view=d['bufferViews'][im['bufferView']];start=view.get('byteOffset',0);embedded=bytes(b[start:start+view['byteLength']])
                    data,mime,row=texture_data(material['name'],role,tier,embedded);digest=hashlib.sha256(data).hexdigest()
                    if digest not in cache:
                        image_index=len(d['images']);d['images'].append({'name':f'{tier}_{material["name"]}_{role}','bufferView':glb.append_view(d,b,data),'mimeType':mime})
                        cache[digest]=len(d['textures']);d['textures'].append({'source':image_index,'sampler':0})
                    container[key]['index']=cache[digest]
                    if role=='orm' and 'occlusionTexture' in material:material['occlusionTexture']['index']=cache[digest]
                    row.update(encoded_bytes=len(data),sha256=digest);texture_rows.append(row)
            d,b=old.compact(d,b)
            folder=OUT/tier/'story_hero'/variant;folder.mkdir(parents=True,exist_ok=True)
            body=folder/'body.glb';body.write_bytes(glb.encode(d,b));(folder/'appearance.cfg').write_text((PREVIOUS/f'assets/story_hero/{variant}/appearance.cfg').read_text())
            vertices=sum(x['vertices'] for x in geometry);triangles=sum(x['triangles'] for x in geometry);image_bytes=sum(d['bufferViews'][im['bufferView']]['byteLength'] for im in d['images'])
            assert vertices<=200000,(tier,variant,vertices)
            assert triangles*3<=600000,(tier,variant,triangles)
            assert body.stat().st_size<=64*1024**2,(tier,variant,body.stat().st_size)
            assert image_bytes<=32*1024**2,(tier,variant,image_bytes)
            report['tiers'][tier][variant]={'file':str(body.relative_to(GEN)),'sha256':sha(body),'file_bytes':body.stat().st_size,'encoded_image_bytes':image_bytes,'decoded_texture_bytes':sum(x['width']*x['height']*(4 if x['encoding']=='image/png' else 3) for x in texture_rows),'vertices':vertices,'triangles':triangles,'indices':triangles*3,'geometry':geometry,'textures':texture_rows}
            print(tier,variant,'vertices',vertices,'triangles',triangles,'images',image_bytes,'file',body.stat().st_size,flush=True)
    assert all(sha(p)==v for p,v in frozen.items()),'Previous generation changed'
    (OUT/'QUALITY_BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
    print('QUALITY_BUILD',OUT/'QUALITY_BUILD.json',flush=True)
if __name__=='__main__':main()
