#!/usr/bin/env python3
"""Shared bounded source-UV material inputs for the exact purchased village.

Original UE material inheritance, LinearColor tint, image color space, cutouts
and source texture coordinates are retained. No whole-object atlas is baked.
Modes3/4 use six repeating source images: three color pages and packed linear
normalXY/roughness/AO pages. Their native shader contract is explicit; packed
normal pages are never admitted to the ordinary normal/ARM BC7 caches.
"""
from pathlib import Path
import argparse,hashlib,io,json,sys,struct,math,zlib
import numpy as np
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parent))
from village_source_registry import ROOT,SOURCE,SourcePackage
from build_purchased_village import SOURCE_INSTANCES

OUT=ROOT/'assets/purchased/medieval_village/city-materials'
REGISTRY=ROOT.parent/'.local-tools/village-source/material-registry.json'

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def linear(v):return np.where(v<=.04045,v/12.92,((v+.055)/1.055)**2.4)
def srgb(v):return np.where(v<=.0031308,v*12.92,1.055*np.maximum(v,0)**(1/2.4)-.055)
def resize_float(a,size):
    return np.stack([np.asarray(Image.fromarray(a[:,:,c].astype(np.float32),mode='F').resize(size,Image.Resampling.LANCZOS))for c in range(a.shape[2])],axis=2)
def source_pixels(package,prop):
    source=prop['Source'];assert source.get('bPNGCompressed')and source['NumMips']==source['NumSlices']==1
    pos=max(e['offset']+e['size']for e in package.exports);magic,chunk,comp,size=struct.unpack_from('<QQQQ',package.data,pos);assert magic==0x9e2a83c1 and size<512*1024**2
    count=math.ceil(size/chunk);sizes=[struct.unpack_from('<QQ',package.data,pos+32+i*16)for i in range(count)];pos+=32+count*16;pieces=[]
    for packed,raw in sizes:
        data=zlib.decompress(package.data[pos:pos+packed]);assert len(data)==raw;pieces.append(data);pos+=packed
    png=b''.join(pieces);assert len(png)==size
    image=Image.open(io.BytesIO(png)).convert('RGBA');assert image.size==(source['SizeX'],source['SizeY'])
    pixels=np.asarray(image).copy()
    if source['Format']=='TSF_BGRA8':pixels=pixels[:,:,[2,1,0,3]]
    else:assert source['Format']in('TSF_G8','TSF_RGBA8')
    return pixels.astype(np.float32)/255,list(image.size)

class VillageMaterialCooker:
    def __init__(self,output=OUT,registry=REGISTRY,source=SOURCE):
        self.output=Path(output);self.output.mkdir(parents=True,exist_ok=True);(self.output/'textures').mkdir(exist_ok=True)
        self.source=Path(source);self.registry=json.loads(Path(registry).read_text());self.cache={};audit=self.output/'textures-audit.json';self.texture_audit=json.loads(audit.read_text())if audit.exists()else{};self.results={}
    def resolve(self,name,seen=None):
        seen=set()if seen is None else seen
        if name in seen:raise ValueError('Cyclic material inheritance: '+name)
        seen.add(name)
        if name in self.registry['material_instances']:
            r=self.registry['material_instances'][name];parent=self.resolve(r['parent'],seen)
            for k in ['scalars','textures','vectors','base_property_overrides']:parent[k].update(r.get(k,{}))
            parent['name']=name;return parent
        if name not in self.registry['master_materials']:raise ValueError('Unknown source material '+name)
        master=self.registry['master_materials'][name];r=dict(name=name,master=name,scalars={},textures={},vectors={},base_property_overrides=master['properties'].copy())
        for node in master['nodes']:
            p=node['properties'];param=p.get('ParameterName')
            if not param:continue
            if node['type']=='MaterialExpressionScalarParameter':r['scalars'][param]=p.get('DefaultValue',0.)
            elif node['type']=='MaterialExpressionVectorParameter':r['vectors'][param]=p.get('DefaultValue',dict(r=0,g=0,b=0,a=0))
            elif node['type']in('MaterialExpressionTextureSampleParameter2D','MaterialExpressionTextureObjectParameter'):r['textures'][param]=p.get('Texture')
        return r
    def texture(self,name,role):
        if name is None:return None
        key=(name,role)
        if key in self.cache:return self.cache[key]
        path=self.source/'Textures'/(name+'.uasset')
        if not path.exists():raise ValueError('Missing original texture '+str(path))
        dest=self.output/'textures'/(name+'__'+role+'.png');previous=self.texture_audit.get(str(dest.resolve()))
        if previous and previous.get('processing_version')==2 and previous.get('source_sha256')==sha(path)and dest.exists()and previous['prepared_sha256']==sha(dest):self.cache[key]=str(dest.resolve());return self.cache[key]
        p=SourcePackage(path);entry=next(e for e in p.exports if e['name']==name);prop=p.export_properties(entry)
        pixels,size=source_pixels(p,prop)
        source_srgb=prop.get('SRGB',True);limit=2048 if role=='base' else (1024 if role in('normal','mask','sss','height') else 512)
        width,height=size;ratio=min(1.,limit/max(width,height));target=(max(1,round(width*ratio)),max(1,round(height*ratio)))
        if role=='normal':
            # UE/Fbx source normal maps use DirectX +Y; native prepared maps
            # follow the already verified OpenGL convention, exactly once.
            n=pixels[:,:,:3]*2-1;n[:,:,1]*=-1
            if target!=(width,height):n=resize_float(n,target)
            n/=np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-8);pixels=np.concatenate((n*.5+.5,np.ones((*n.shape[:2],1),np.float32)),axis=2)
        else:
            # Follow actual UE graph channels, never filename-based MRA guesses.
            # Several source ARM samplers deliberately use Texture2D's default
            # sRGB=true/Color sampling; reproduce that decode once into native
            # linear ARM data. The cloth ARM assets explicitly disable sRGB.
            if role=='arm'and source_srgb:pixels[:,:,:3]=linear(pixels[:,:,:3])
            if target!=(width,height):
                v=pixels.copy()
                if role=='base'and source_srgb:v[:,:,:3]=linear(v[:,:,:3])
                v=resize_float(v,target)
                if role=='base'and source_srgb:v[:,:,:3]=srgb(v[:,:,:3])
                pixels=v
            if role in('base','mask','sss')and not source_srgb:pixels[:,:,:3]=srgb(pixels[:,:,:3])
        pixels=np.uint8(np.rint(np.clip(pixels,0,1)*255));Image.fromarray(pixels).save(dest,compress_level=3)
        self.cache[key]=str(dest.resolve());self.texture_audit[str(dest.resolve())]=dict(source=str(path.resolve()),source_sha256=sha(path),prepared_sha256=sha(dest),source_size=size,prepared_size=list(target),role=role,
            source_srgb=source_srgb,source_format=prop['Source']['Format'],normal_green_inverted_once=role=='normal',source_BGRA_normalized_once=prop['Source']['Format']=='TSF_BGRA8',ARM_swizzle='none; source graph defines RGB channels',source_sRGB_decode_baked=role=='arm'and source_srgb,processing_version=2)
        return self.cache[key]
    def packed_nra(self,normal,arm):
        n=self.texture(normal,'normal');a=self.texture(arm,'arm')
        name=normal+'__'+arm+'__NXY_ROUGH_AO.png';out=self.output/'textures'/name
        previous=self.texture_audit.get(str(out.resolve()))
        if previous and out.exists()and previous.get('normal_source_sha256')==sha(n)and previous.get('ARM_source_sha256')==sha(a)and previous.get('prepared_sha256')==sha(out):return str(out.resolve())
        nv=np.asarray(Image.open(n).convert('RGB'));av=Image.open(a).convert('RGB').resize((nv.shape[1],nv.shape[0]),Image.Resampling.BILINEAR);av=np.asarray(av)
        if np.max(av[:,:,2])!=0:raise ValueError('Packed source layer requires verified dielectric metallic0: '+arm)
        pixels=np.stack((nv[:,:,0],nv[:,:,1],av[:,:,1],av[:,:,0]),axis=2);Image.fromarray(pixels).save(out,compress_level=3)
        self.texture_audit[str(out.resolve())]=dict(role='packed_normalXY_roughness_AO',srgb=False,normal=n,ARM=a,normal_source_sha256=sha(n),ARM_source_sha256=sha(a),prepared_sha256=sha(out),prepared_size=list(Image.open(out).size),metallic_verified_zero=True,BC7_allowed=False)
        return str(out.resolve())
    def get(self,name):
        name=name.rsplit('/',1)[-1].split('.')[-1]
        name=SOURCE_INSTANCES.get(name,name)
        if name in self.results:return self.results[name]
        r=self.resolve(name);s=r['scalars'];t=r['textures'];master=r['master'];over=r['base_property_overrides']
        row=dict(name=name,source_master=master,base=None,normal=None,arm=None,emissive=None,emissive_factor=[0.,0.,0.],factor=[1.,1.,1.,1.],roughness=1.,metallic=1.,alphaMode='MASK'if over.get('BlendMode')=='BLEND_Masked'else'OPAQUE',alphaCutoff=over.get('OpacityMaskClipValue',.3333),doubleSided=bool(over.get('TwoSided',False)),uv_scale=[1.,1.],facade_mode=0,aux=[None,None,None],facade_scale=[1.,1.,1.],source_paint=False,limitations=[])
        if master in('MM_Sky_Dome','MM_Fog_Card'):
            row.update(exclude=True,reason='Native environment/fog must bind the exact authored source settings, not draw an opaque proxy');self.results[name]=row;return row
        if master=='MM_Simple_Black':
            row.update(factor=[0.,0.,0.,1.],metallic=0.);self.results[name]=row;return row
        if master=='MM_Master_2_Blend':
            row.update(base=self.texture(t['Diffuse_A'],'base'),normal=self.texture(t['Normal_A'],'normal'),arm=self.texture(t['ARM_A'],'arm'),facade_mode=1,source_paint=True,
                aux=[self.texture(t['Diffuse_B'],'base'),self.texture(t.get('Rgb_Mask','T_RGB_Mask'),'mask'),None],facade_scale=[s.get('Diffuse_A_tiling',1),s.get('Diffuse_B_tiling',1),s.get('Rgb_Tiling',2)])
        elif master=='MM_Master_3_Blend':
            row.update(base=self.texture(t['Albedo 1'],'base'),normal=self.packed_nra(t['Normal_1'],t['ARM_1']),arm=self.texture(t['Albedo_2'],'base'),facade_mode=3,source_paint=True,
                aux=[self.packed_nra(t['Normal_2'],t['ARM_2']),self.texture(t['Albedo_3'],'base'),self.texture(t['RGB_Mask_Blue_Channel'],'mask')],facade_scale=[s.get('Tex_Tiling_1',1),s.get('Tex_Tiling_2',1),s.get('Tex_Tiling_3',1)])
            row['limitations']=['External UE HeightLerp contrast is reconstructed with endpoint-preserving transitions; no POM']
        elif master=='MM_POM_3_Blend_Puddle':
            row.update(base=self.texture(t['Albedo 1'],'base'),normal=self.packed_nra(t['Normal 1'],t['ARM Mask 1']),arm=self.texture(t['Albedo_2'],'base'),facade_mode=4,source_paint=True,
                aux=[self.packed_nra(t['Normal 2'],t['ARM Mask 2']),self.texture(t['Albedo 3'],'base'),self.packed_nra(t['Normal 3'],t['ARM Mask 3'])],facade_scale=[s.get('Tex_Tiling_1',1),s.get('Tex_Tiling_2',1),s.get('Puddle_Roughness',.065238997)])
            row['emissive']=self.texture(t['Height 1'],'mask')
            row['source_ground_parameters']=dict(blue_contrast=s.get('Blue_vertex_contrast',.5),contrast=[s.get('Contrast_Tex_'+str(i),0)for i in(1,2,3)],puddle_color=r['vectors'].get('Puddle Color'))
            row['limitations']=['Three source PBR layers and blue puddle channel retained; external HeightLerp reconstructed; source POM depth not yet executed']
        else:
            row['base']=self.texture(next((t[k]for k in('Diffuse','Diffuse_A','Albedo')if k in t),None),'base')
            row['normal']=self.texture(t.get('Normal'),'normal');row['arm']=self.texture(t.get('ARM'),'arm')
            scale=s.get('Texture_Tiling',s.get('Tex_tiling',1.));row['uv_scale']=[scale,scale]
            if master=='MM_Rope':
                row['uv_scale']=[s.get('tiling_amount',1),s.get('tiling_amount',1)];row['limitations'].append('Source rope custom coordinate function approximated by original UV0 and authored tiling')
            if master=='MM_Ribbon_Flag':
                # The original opaque, two-sided ribbon graph multiplies its
                # texture in linear space before lighting; alpha is not used.
                darken=s.get('Diffuse_Darken',.5);row['factor']=[darken,darken,darken,1.]
                row['limitations'].append('Original static ribbon PBR preserved; source FlipbookWind vertex animation is not yet executed')
            if master=='MM_Simple_Tint':
                c=r['vectors'].get('Tint_Color',dict(r=1,g=1,b=1,a=1));row['factor']=[c[k]for k in'rgba']
            if master=='MM_POM_Master_Simple':row['limitations'].append('Original source height map retained in metadata; source POM depth not yet executed');row['height']=self.texture(t.get('Height'),'height')
            if master=='MM_Cloth':
                row.update(facade_mode=5,aux=[self.texture(t.get('SSS_Mask'),'sss'),None,None],facade_scale=[1.,s.get('SSS_Value',.4),max(1e-5,s.get('SSS_Mask_Brigthness',0))],source_paint=True)
                row['limitations'].append('Native thin transmission approximates UE subsurface profile; original albedo/normal/ARM/SSS preserved')
        if row['base']is None:raise ValueError('No source base texture for '+name+' / '+master)
        row['facade_version']=2 if row['facade_mode']>=3 else 1
        row['source_instance']=r;self.results[name]=row;return row
    def save(self):
        for name,value in [('materials.json',self.results),('textures-audit.json',self.texture_audit)]:
            path=self.output/name;temporary=path.with_suffix('.json.tmp');temporary.write_text(json.dumps(value,indent=2)+'\n');temporary.replace(path)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=OUT);ap.add_argument('--materials',nargs='*');args=ap.parse_args();c=VillageMaterialCooker(args.output)
    referenced={s['MaterialInterface']for m in c.registry['meshes'].values()for s in m['slots']if s.get('MaterialInterface')}
    known=set(c.registry['material_instances'])|set(c.registry['master_materials'])
    priority=['MI_POM_Ground','MI_Wood_Trims_01','MI_WallPlaster','MI_GlassWindow','MI_Roof1','MI_Roof2','MI_Tent_1','MI_Tent_2','MI_Tent_3','MI_Castle_Walls_01','MI_Castle_Walls_02']
    names=args.materials or priority+sorted((set(c.registry['material_instances'])|{n for n in referenced if n in known})-set(priority))
    for i,name in enumerate(names):
        c.get(name);c.save();print('MATERIAL',i+1,len(names),name,flush=True)
    print('READY',len(c.results),'source material definitions;',len(c.texture_audit),'bounded images',flush=True)

if __name__=='__main__':main()
