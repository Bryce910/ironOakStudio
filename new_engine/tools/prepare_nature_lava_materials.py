#!/usr/bin/env python3
"""Static source lava PBR/emission variants with source heat-paint routing.

Keep original repeated UV0; do not flatten hundred-tile rivers into one atlas.
Three authored temperature layers become three material groups per original
flow material. This preserves original crust textures and most painted heat
regions without porting the proprietary animated UV4/tessellation shader.
"""
from pathlib import Path
import argparse,functools,json
import numpy as np
from PIL import Image
from cook_nature_citadel import ROOT,DATA,yaml_material
from prepare_nature_guardian import srgb_to_linear,linear_to_srgb

LAYERS=['Cold','Medium','Hot']


@functools.lru_cache(maxsize=1)
def inventory():return {e['guid']:e['path']for e in json.loads((DATA/'INVENTORY.json').read_text())['entries']}


@functools.lru_cache(maxsize=3)
def material(path):
    s=yaml_material(DATA/'source'/path)
    return ({k:v for i in s['m_Floats']for k,v in i.items()},
            {k:v for i in s['m_Colors']for k,v in i.items()},
            {k:v for i in s['m_TexEnvs']for k,v in i.items()})


@functools.lru_cache(maxsize=6)
def source_image(path):
    with Image.open(path)as im:return np.asarray(im.convert('RGBA')).copy()


def texture(tex,key):
    guid=tex[key]['m_Texture']['guid'];path=DATA/'source'/inventory()[guid]
    return source_image(str(path)),path


def resize(value,size):
    return np.stack([np.asarray(Image.fromarray(value[:,:,i],'F').resize((size,size),Image.Resampling.LANCZOS))for i in range(value.shape[2])],axis=2)


def variant_key(path,layer):return path+'#'+layer


def prepare_variant(path,layer,output,size=1024,arm_size=512):
    f,c,t=material(path);prefix='_'+layer+'Lava'
    base,basepath=texture(t,prefix+'Albedo_SM');mask,maskpath=texture(t,prefix+'MT_AO_H_EM');normal,normalpath=texture(t,prefix+'Normal')
    base=base.astype(np.float32)/255;mask=mask.astype(np.float32)/255;normal=normal.astype(np.float32)/255
    base[:,:,:3]=srgb_to_linear(base[:,:,:3]);base=resize(base,size);mask=resize(mask,size);normal=resize(normal[:,:,:3],size)
    tint=c.get(prefix+'AlbedoColor',c.get(prefix+'ALbedoColor',{'r':1,'g':1,'b':1}))
    tint=srgb_to_linear(np.array([tint[x]for x in 'rgb'],np.float32))
    rgb=base[:,:,:3]*tint*f.get(prefix+'AlbedoColorMultiply',1)
    n=normal*2-1;n[:,:,:2]*=f.get(prefix+'NormalScale',1);n[:,:,2]=np.sqrt(np.maximum(1-(n[:,:,:2]**2).sum(2),0))
    n/=np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-8);n[:,:,1]*=-1
    metallic=f.get(prefix+'Metallic',f.get(prefix+'Metalic',1))
    arm=np.stack((np.clip(mask[:,:,1],1-f.get(prefix+'AO',1),1),1-np.clip(base[:,:,3]*f.get(prefix+'Smoothness',1),0,1),mask[:,:,0]*metallic),2)
    intensity=f.get(prefix+'EmissionMaskIntensivity',f.get(prefix+'EmissionMaskIntesivity',1))
    emission=np.power(np.abs(mask[:,:,3]*intensity),f.get(prefix+'EmissionMaskTreshold',1))*1.2
    color=np.array([c['_LavaEmissionColor'][x]for x in 'rgb'],np.float32)
    emitted=emission[:,:,None]*color
    source_peak=float(emitted.max());strength=max(1e-4,min(source_peak,10000))
    emissive=linear_to_srgb(np.clip(emitted/strength,0,1))
    name=Path(path).stem+'__'+layer.lower();folder=output/name;folder.mkdir(parents=True,exist_ok=True)
    for role,value in [('base',linear_to_srgb(rgb)),('normal',n*.5+.5),('arm',arm),('emissive',emissive)]:
        image=Image.fromarray(np.uint8(np.clip(value,0,1)*255+.5))
        if role=='arm':image=image.resize((arm_size,arm_size),Image.Resampling.LANCZOS)
        image.save(folder/(role+'.png'))
    tiling=c.get(prefix+'Tiling',{'r':1,'g':1});global_tiling=f.get('_GlobalTiling',1)
    return {'name':name,'status':'candidate','paths':{role:str(folder/(role+'.png'))for role in ['base','normal','arm','emissive']},
            'emissive_factor':[strength,strength,strength],'double_sided':False,'uv_scale':[tiling[x]/global_tiling for x in 'rg'],
            'source_material':path,'temperature_layer':layer,'source_textures':[str(x.relative_to(ROOT))for x in [basepath,maskpath,normalpath]],
            'source_emission_peak':source_peak,'native_emission_strength':strength,
            'limitations':['Static flow: original UV4 velocity/tessellation not exported','Heat paint/slope/height routed per triangle; no continuous cross-layer blend','Emission alpha below .05 routed to cold; continuous per-vertex opacity not ported','Noise frozen at its source maximum1.2; view-dependent rim excluded','Only rare values above native emissive10000limit clip']}


def _height(tex,layer,uv,scale):
    pixels,_=texture(tex,'_'+layer+'LavaMT_AO_H_EM');h,w=pixels.shape[:2]
    xy=(uv*scale)%1;x=(xy[:,0]*w-.5)%w;y=((1-xy[:,1])*h-.5)%h
    xf=np.floor(x);yf=np.floor(y);x0=xf.astype(int)%w;y0=yf.astype(int)%h;fx=x-xf;fy=y-yf
    return ((pixels[y0,x0,2]*(1-fx)+pixels[y0,(x0+1)%w,2]*fx)*(1-fy)+(pixels[(y0+1)%h,x0,2]*(1-fx)+pixels[(y0+1)%h,(x0+1)%w,2]*fx)*fy)/255


def route_triangles(path,vertex14_world,indices_flat):
    """Return [(fullMaterialPath#Temperature, flatInt32Indices), ...].

    Input p/n are original Unity world coordinates, UV0 untouched, paintRGBA at
    columns10:14. The same source world transform used for GI is appropriate.
    Routing uses the exact static source slope/height/paint weight expressions
    at each triangle centroid, then chooses the dominant layer for native PBR.
    """
    f,c,t=material(path);v=np.asarray(vertex14_world,np.float32);i=np.asarray(indices_flat,np.int32).reshape(-1,3)
    uv=v[i,6:8].mean(1);normal=v[i,3:6].mean(1);normal/=np.maximum(np.linalg.norm(normal,axis=1,keepdims=True),1e-8)
    paint=np.clip(v[i,10:14].mean(1),0,1);heights=[]
    for layer in LAYERS:
        tile=c.get('_'+layer+'LavaTiling',{'r':1,'g':1});scale=np.array([tile[x]for x in 'rg'])/f.get('_GlobalTiling',1)
        heights.append(_height(t,layer,uv,scale))
    weights=np.zeros((len(i),3),np.float32);weights[:,0]=1
    def blend(layer,amount):
        a=np.clip(amount,0,1);weights[:]*=1-a[:,None];weights[:,layer]+=a
    for layer,name,height in [(1,'Medium',heights[0]),(2,'Hot',heights[1])]:
        p='_'+name+'Lava';angle=max(f.get(p+'Angle',45)/45,1e-5)
        slope=np.clip(1-np.clip((np.abs(normal[:,1])-(1-angle))/angle,0,1),0,1)**f.get(p+'AngleFalloff',1)
        amount=np.abs(np.power(np.abs(1-height),f.get(p+'HeightBlendTreshold',1))*slope*4+slope*2)**f.get(p+'HeightBlendStrenght',1)
        blend(layer,amount)
    for layer,name,height in [(0,'Cold',heights[0]),(1,'Medium',heights[1]),(2,'Hot',heights[2])]:
        p=paint[:,layer];amount=np.abs((1-height)*p*4+p*2)**f.get('_VC'+name+'LavaHeightBlendStrenght',1)
        blend(layer,amount)
    selected=weights.argmax(1);selected[paint[:,3]<.05]=0
    return [(variant_key(path,name),np.ascontiguousarray(i[selected==layer].reshape(-1),np.int32))for layer,name in enumerate(LAYERS)if np.any(selected==layer)]


def vertex_heat(path,vertex14_world):
    """Continuous original source slope/height/paint temperature, scalar0..1.

    Geometry must export this as _VALWOOD_LAVA_HEAT, never choose a material
    independently for each face. Original UV0 and complete indices stay intact.
    """
    f,c,t=material(path);v=np.asarray(vertex14_world,np.float32)
    uv=v[:,6:8];normal=v[:,3:6].copy();normal/=np.maximum(np.linalg.norm(normal,axis=1,keepdims=True),1e-8)
    paint=np.clip(v[:,10:14],0,1);heights=[]
    for layer in LAYERS:
        tile=c.get('_'+layer+'LavaTiling',{'r':1,'g':1});scale=np.array([tile[x]for x in 'rg'])/f.get('_GlobalTiling',1)
        heights.append(_height(t,layer,uv,scale))
    weights=np.zeros((len(v),3),np.float32);weights[:,0]=1
    def blend(layer,amount):
        a=np.clip(amount,0,1);weights[:]*=1-a[:,None];weights[:,layer]+=a
    for layer,name,height in [(1,'Medium',heights[0]),(2,'Hot',heights[1])]:
        p='_'+name+'Lava';angle=max(f.get(p+'Angle',45)/45,1e-5)
        slope=np.clip(1-np.clip((np.abs(normal[:,1])-(1-angle))/angle,0,1),0,1)**f.get(p+'AngleFalloff',1)
        amount=np.abs(np.power(np.abs(1-height),f.get(p+'HeightBlendTreshold',1))*slope*4+slope*2)**f.get(p+'HeightBlendStrenght',1)
        blend(layer,amount)
    for layer,name,height in [(0,'Cold',heights[0]),(1,'Medium',heights[1]),(2,'Hot',heights[2])]:
        p=paint[:,layer];amount=np.abs((1-height)*p*4+p*2)**f.get('_VC'+name+'LavaHeightBlendStrenght',1)
        blend(layer,amount)
    return np.ascontiguousarray(np.clip((weights@np.array([0.,.5,1.],np.float32))*paint[:,3],0,1),np.float32)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-lava/materials');p.add_argument('--size',type=int,default=1024);args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    results={}
    for name in ['M_Lava_River_3_Trial','M_Lava_River_3_Dynamic_Trial','M_Lava_River_4_Trial']:
        path=str(next((DATA/'source').rglob(name+'.mat')).relative_to(DATA/'source'))
        for layer in LAYERS:
            results[variant_key(path,layer)]=prepare_variant(path,layer,args.output,args.size)
            print('Lava',name,layer,'peak',results[variant_key(path,layer)]['source_emission_peak'],flush=True)
    (args.output.parent/'materials.json').write_text(json.dumps(results,indent=2)+'\n')


if __name__=='__main__':main()
