#!/usr/bin/env python3
"""Convert verified owned Unity material maps without altering model geometry."""
from pathlib import Path
import argparse,json,hashlib,io
import numpy as np
import yaml
from PIL import Image

STAGE=Path(__file__).resolve().parent
INTAKE=STAGE/'intake'
OUT=STAGE/'arena_orcs/textures/shared'

def sha(path):
    with Path(path).open('rb')as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def load_yaml(path):
    text=Path(path).read_text()
    clean='\n'.join('---'if line.startswith('--- !u!')else line for line in text.splitlines()if not line.startswith('%'))
    return yaml.safe_load(clean)
def linear(rgb):return np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4)
def srgb(rgb):return np.where(rgb<=.0031308,rgb*12.92,1.055*np.maximum(rgb,0)**(1/2.4)-.055)
def resize(a,size):return np.asarray(Image.fromarray(a.astype(np.float32),'F').resize((size,size),Image.Resampling.LANCZOS),dtype=np.float32)
def quantize(a):return np.rint(np.clip(a,0,1)*255).astype(np.uint8)
def write_png(values,role):
    image=Image.fromarray(values,'RGBA'if values.shape[2]==4 else'RGB')
    data=io.BytesIO();image.save(data,format='PNG',compress_level=6)
    raw=data.getvalue();digest=hashlib.sha256(raw).hexdigest();path=OUT/(digest+'.png')
    if path.exists():assert path.read_bytes()==raw
    else:path.write_bytes(raw)
    return {'path':str(path),'sha256':digest,'size':list(image.size),'mode':image.mode,'bytes':len(raw),'role':role,'uri':'../textures/shared/'+path.name}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--correspondence',type=Path,required=True)
    parser.add_argument('--profile',type=Path,required=True)
    parser.add_argument('--normal-green',choices=['preserve','flip'],required=True)
    parser.add_argument('--normal-basis-proof',type=Path,required=True)
    parser.add_argument('--armor-size',type=int,choices=[1024,2048],default=2048)
    args=parser.parse_args()
    correspondence=json.loads(args.correspondence.read_text());basis=json.loads(args.normal_basis_proof.read_text());profile=json.loads(args.profile.read_text())
    assert correspondence.get('passed')is True,'Actual model/UV correspondence must be established before assigning maps'
    assert basis.get('passed')is True,'Normal basis correspondence must be measured before converting'
    inventory=json.loads((INTAKE/'UNITY_GUID_INVENTORY.json').read_text())['entries']
    extraction=json.loads((INTAKE/profile['extraction_receipt']).read_text())['files']
    inputs={};images=[];textures=[];materials=[];details=[];converted={};image_indices={};image_sizes={}
    OUT.mkdir(parents=True,exist_ok=True)
    for i,spec in enumerate(profile['materials']):
        guid=spec['guid'];is_mask=spec.get('alpha_mode')=='MASK'
        record=extraction[guid];mat_path=Path(record['asset']['path']);mat=load_yaml(mat_path)['Material']
        saved=mat['m_SavedProperties'];floats={k:v for row in saved['m_Floats']for k,v in row.items()}
        colors={k:v for row in saved['m_Colors']for k,v in row.items()};tex={k:v for row in saved['m_TexEnvs']for k,v in row.items()}
        assert colors['_Color']=={'r':1,'g':1,'b':1,'a':1}
        assert floats['_GlossMapScale']==floats['_BumpScale']==floats['_OcclusionStrength']==1
        assert floats['_SmoothnessTextureChannel']==0
        assert '_METALLICGLOSSMAP'in mat['m_ShaderKeywords']and '_NORMALMAP'in mat['m_ShaderKeywords']
        roles={}
        for role,key in [('albedo','_MainTex'),('normal','_BumpMap'),('metallic_smoothness','_MetallicGlossMap'),('occlusion','_OcclusionMap')]:
            slot=tex[key];assert slot['m_Scale']=={'x':1,'y':1}and slot['m_Offset']=={'x':0,'y':0}
            tg=slot['m_Texture']['guid'];entry=extraction[tg];path=Path(entry['asset']['path']);meta_path=Path(entry['asset.meta']['path'])
            importer=load_yaml(meta_path)['TextureImporter'];is_srgb=bool(importer['mipmaps']['sRGBTexture'])
            with Image.open(path)as image:
                image.load();width,height=image.size;assert width==height==4096
                stats=np.asarray(image.resize((64,64),Image.Resampling.BOX).convert('RGB'),dtype=np.float32)
                source_mode=image.mode
            roles[role]={'guid':tg,'path':str(path),'sha256':sha(path),'source_size':[width,height],'source_mode':source_mode,'mean_rgb':stats.mean(axis=(0,1)).tolist(),'pixel_std':float(stats.std()),'meta_path':str(meta_path),'meta_sha256':sha(meta_path),'original_asset_path':inventory[tg]['path'],'import_flags':{'srgb':is_srgb,'texture_type':importer['textureType'],'flip_green_channel':bool(importer.get('flipGreenChannel',0)),'convert_to_normal_map':bool(importer['bumpmap']['convertToNormalMap'])}}
            if role=='normal':assert importer['textureType']==1 and not is_srgb and not importer['bumpmap']['convertToNormalMap']and not importer.get('flipGreenChannel',0)
            for value in entry.values():inputs[value['path']]=value['sha256']
        for value in record.values():inputs[value['path']]=value['sha256']
        target=spec.get('target_size',2048)
        # Resample base color in linear light, preserving alpha separately.
        with Image.open(roles['albedo']['path'])as image:
            rgba=image.convert('RGBA');alpha=np.asarray(rgba.getchannel('A'),dtype=np.float32)/255
            a=resize(alpha,target);channels=[]
            for channel in 'RGB':
                source=linear(np.asarray(rgba.getchannel(channel),dtype=np.float32)/255)
                if is_mask:
                    source=resize(source*alpha,target);source=np.divide(source,a,out=np.zeros_like(source),where=a>1e-6)
                else:source=resize(source,target)
                channels.append(quantize(srgb(source)))
            albedo=write_png(np.dstack([*channels,quantize(a)]),'albedo_srgb')
        # Raw authored RGB normals, not Unity's platform DXT5nm/BC5 cache.
        with Image.open(roles['normal']['path'])as image:
            rgb=image.convert('RGB');components=[resize(np.asarray(rgb.getchannel(c),dtype=np.float32)/127.5-1,target)for c in'RGB']
            normal=np.dstack(components)
            if args.normal_green=='flip':normal[:,:,1]*=-1
            lengths=np.linalg.norm(normal,axis=2);bad=lengths<.1;fallback_count=int(bad.sum())
            if fallback_count:
                # Opposing authored directions at sharp atlas boundaries can
                # cancel during filtering. Keep an actual nearest source
                # sample there instead of inventing a flat/zero direction.
                nearest=np.asarray(rgb.resize((target,target),Image.Resampling.NEAREST),dtype=np.float32)/127.5-1
                if args.normal_green=='flip':nearest[:,:,1]*=-1
                normal[bad]=nearest[bad];lengths=np.linalg.norm(normal,axis=2)
            assert float(lengths.min())>.1
            normal/=lengths[:,:,None];nrm=write_png(quantize(normal*.5+.5),'normal_linear');nrm['near_zero_filtered_pixels_replaced_with_nearest_original_normal']=fallback_count
        # Match actual Standard shader/importer channels. Imported sRGB RGB
        # must be decoded; source alpha/smoothness is always linear.
        with Image.open(roles['metallic_smoothness']['path'])as image:
            assert image.mode=='RGBA','Original metallic/smoothness alpha required'
            metallic=np.asarray(image.getchannel('R'),dtype=np.float32)/255
            if roles['metallic_smoothness']['import_flags']['srgb']:metallic=linear(metallic)
            smoothness=np.asarray(image.getchannel('A'),dtype=np.float32)/255
            metal=resize(metallic,1024);rough=1-resize(smoothness,1024)
        with Image.open(roles['occlusion']['path'])as image:
            occlusion=np.asarray(image.convert('RGB').getchannel('G'),dtype=np.float32)/255
            if roles['occlusion']['import_flags']['srgb']:occlusion=linear(occlusion)
            ao=resize(occlusion,1024)
        arm=write_png(quantize(np.dstack([ao,rough,metal])),'arm_linear')
        conversions={'albedo':albedo,'normal':nrm,'arm':arm}
        indices={}
        for role,result in conversions.items():
            if result['path']not in image_indices:
                index=len(images);image_indices[result['path']]=index;images.append({'uri':result['uri']});textures.append({'source':index,'sampler':0});image_sizes[result['path']]=result['size']
            indices[role]=image_indices[result['path']];converted[result['path']]=result['sha256']
        material={'name':spec['name'],'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'baseColorTexture':{'index':indices['albedo']},'metallicFactor':1,'roughnessFactor':1,'metallicRoughnessTexture':{'index':indices['arm']}},'normalTexture':{'index':indices['normal'],'scale':1},'occlusionTexture':{'index':indices['arm'],'strength':1},'alphaMode':spec.get('alpha_mode','OPAQUE'),'doubleSided':spec.get('double_sided',False),'extras':{'original_unity_material_guid':guid,'original_unity_material':record['asset']['original_asset_path'],'original_textures':True}}
        if is_mask:material['alphaCutoff']=spec.get('alpha_cutoff',.5)
        materials.append(material)
        details.append({'material_index':i,'source_material_guid':guid,'source_mat':str(mat_path),'source_mat_sha256':sha(mat_path),'source_meta':record['asset.meta']['path'],'unity_name':mat['m_Name'],'unity_shader':mat['m_Shader'],'unity_keywords':mat['m_ShaderKeywords'],'unity_floats':floats,'source_images':roles,'conversions':conversions,'normal_green_policy':args.normal_green,'formulas':{'albedo':'Linear-light Lanczos resize of original sRGB RGB; same white tint; alpha retained. Armor alpha-weighted RGB avoids dark cutout fringes.','normal':'Raw linear RGB decoded to unit tangent normal; measured cooked UV/TBN green policy applied; resized vectors renormalized.','arm_R':'lerp(1,imported original AO.G,1); source RGB sRGB decoded exactly when importer flag is set.','arm_G':'1 - original MetallicGlossMap.A * GlossMapScale(1); alpha is linear.','arm_B':'Imported original MetallicGlossMap.R; sRGB RGB decoded exactly when importer flag is set.','factors':'roughnessFactor=metallicFactor=1, no duplicate channel multiplier.'}})
    # Pin source evidence beyond images/materials, including the original shader,
    # prefab and exact mesh correspondence's separately hashed inputs.
    for guid,record in extraction.items():
        for value in record.values():inputs[value['path']]=value['sha256']
    inputs[str(args.correspondence.resolve())]=sha(args.correspondence)
    inputs[str(args.normal_basis_proof.resolve())]=sha(args.normal_basis_proof)
    inputs[str(args.profile.resolve())]=sha(args.profile)
    for name,digest in correspondence.get('inputs',{}).items():
        assert sha(name)==digest
        inputs[name]=digest
    for name,digest in basis.get('inputs',{}).items():
        assert sha(name)==digest
        inputs[name]=digest
    inputs[str(INTAKE/'UNITY_GUID_INVENTORY.json')]=sha(INTAKE/'UNITY_GUID_INVENTORY.json')
    inputs[str(INTAKE/'UNITY_PACKAGE_EXTRACTION.json')]=sha(INTAKE/'UNITY_PACKAGE_EXTRACTION.json')
    inputs[str(Path(__file__).resolve())]=sha(__file__)
    primitives=[dict(row,correspondence_receipt=str(args.correspondence.resolve()))for row in profile['primitive_mapping']]
    receipt={'schema':'valwood.power-orc.original-material-mapping.v1','source_model':profile['source_model'],'passed':True,'original_textures_present':True,'materials_provisional':False,'inputs':inputs,'materials':details,'primitive_mapping':primitives,'correspondence_receipt':{'path':str(args.correspondence.resolve()),'sha256':sha(args.correspondence)},'normal_basis_receipt':{'path':str(args.normal_basis_proof.resolve()),'sha256':sha(args.normal_basis_proof)},'converted_textures':converted,'image_count':len(images),'rgba8_with_mips_upper_bound_bytes':int(sum(size[0]*size[1]*4*4/3 for size in image_sizes.values())),'geometry_edited':False,'source_images_unchanged':True,'visual_acceptance':False}
    output=STAGE/'arena_orcs/ORC_TEXTURE_MAPPING.json';output.write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    mapping={'schema':'valwood.power-orc.gltf-material-patch.v1','source_model':profile['source_model'],'passed':True,'gltf':{'materials':materials,'images':images,'textures':textures,'samplers':[{'magFilter':9729,'minFilter':9987,'wrapS':10497,'wrapT':10497}]},'primitive_materials':[row['material_index']for row in primitives],'source_files':inputs,'converted_textures':converted,'per_primitive':primitives,'mesh_correspondence':{'passed':True,'path':str(args.correspondence.resolve()),'sha256':sha(args.correspondence)},'texture_mapping_receipt':{'path':str(output),'sha256':sha(output)},'original_textures_present':True,'materials_provisional':False,'visual_ready':False}
    (STAGE/'ORC_MATERIAL_MAPPING.json').write_text(json.dumps(mapping,indent=2,sort_keys=True)+'\n')
    print(json.dumps({'passed':True,'images':len(images),'materials':len(materials),'mapping':str(STAGE/'ORC_MATERIAL_MAPPING.json'),'mapping_sha256':sha(STAGE/'ORC_MATERIAL_MAPPING.json'),'texture_receipt':str(output),'rgba8_with_mips_upper_bound_bytes':receipt['rgba8_with_mips_upper_bound_bytes']}))

if __name__=='__main__':main()
