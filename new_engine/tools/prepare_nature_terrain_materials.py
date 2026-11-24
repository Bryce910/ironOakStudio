#!/usr/bin/env python3
"""Prepare source TerrainLayer tiles and the publisher's verified eight weights.

PNG rows are left in their natural order. Material PNG row0 is source texture
top (V=1); alpha PNG row0 is source terrain Zmin (Unity GetAlphamaps[z,x,l]).
The native uploader flips PNG rows and samples alpha in increasing engine Z.
Source normal green is preserved for T=engine+X, B=engine-Z on flat terrain.
"""
from pathlib import Path
import argparse,hashlib,json,re
import numpy as np
from PIL import Image
import yaml
from cook_nature_citadel import ROOT,DATA
from prepare_nature_guardian import srgb_to_linear,linear_to_srgb

# Verified by UnityPy TerrainData serialized m_TerrainLayers, recovered with
# per-byte nibble order correction. This is the actual alpha-channel order.
ORDER=['609eb8683981db64cb2c6e1a5e6a52ad','d23c8f25c52489a4398e062e6a292d21',
       'be1a5f8277a3ced40909e48c698df290','a3ece3bf09e5eb845877ce4f0f327db6',
       '264105d0ebbf05b4b8a218c0e2a0703d','a90872413b002be48a56e729f893680a',
       'de01fb70e40133a4898b46e917f4233f','c605eb25ebb53364394d206074c20304']


def document(path):
    text=re.sub(r'^%.*\n','',path.read_text(),flags=re.M)
    text=re.sub(r'^--- !u!\d+ &-?\d+','---',text,flags=re.M)
    return yaml.safe_load(text)['TerrainLayer']


def resize(value,size):
    return np.stack([np.asarray(Image.fromarray(value[:,:,c],'F').resize((size,size),Image.Resampling.LANCZOS))for c in range(value.shape[2])],axis=2)


def prepare_layer(guid,paths,output,size):
    path=DATA/'source'/paths[guid];layer=document(path);sources={}
    def image(key):
        texture_guid=layer.get(key,{}).get('guid')
        if not texture_guid:return None
        if texture_guid not in paths:raise ValueError('Missing source terrain texture '+texture_guid)
        source=DATA/'source'/paths[texture_guid]
        sources[key]={'path':str(source.relative_to(ROOT)),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()}
        with Image.open(source)as raw:return np.asarray(raw.convert('RGBA'),np.float32)/255
    base=image('m_DiffuseTexture');normal=image('m_NormalMapTexture');mask=image('m_MaskMapTexture')
    if base is None:raise ValueError('Missing source terrain albedo')
    base=srgb_to_linear(base[:,:,:3])
    low=np.array([layer.get('m_DiffuseRemapMin',{}).get(c,0)for c in 'xyz'],np.float32)
    high=np.array([layer.get('m_DiffuseRemapMax',{}).get(c,1)for c in 'xyz'],np.float32)
    base=linear_to_srgb(resize(base*(high-low)+low,size))
    if normal is None:normal=np.broadcast_to(np.array([.5,.5,1],np.float32),(size,size,3)).copy()
    else:normal=resize(normal[:,:,:3],size)
    if mask is None:
        arm=np.broadcast_to(np.array([1,1-layer.get('m_Smoothness',0),layer.get('m_Metallic',0)],np.float32),(size,size,3)).copy()
        height=np.zeros((size,size,1),np.float32)
    else:
        low=np.array([layer.get('m_MaskMapRemapMin',{}).get(c,0)for c in 'xyzw'],np.float32)
        high=np.array([layer.get('m_MaskMapRemapMax',{}).get(c,1)for c in 'xyzw'],np.float32)
        raw=resize(mask,size);value=raw*(high-low)+low
        arm=np.stack((value[:,:,1],1-value[:,:,3],value[:,:,0]),axis=2)
        height=raw[:,:,2:3]
    name=layer['m_Name'];folder=output/name;folder.mkdir(parents=True,exist_ok=True)
    for role,value in [('base',base),('normal',normal),('arm',arm),('height',height)]:
        pixels=np.uint8(np.clip(value,0,1)*255+.5)
        if pixels.shape[2]==1:pixels=pixels[:,:,0]
        Image.fromarray(pixels).save(folder/(role+'.png'))
    return {'guid':guid,'name':name,'source':str(path.relative_to(ROOT)),
            'base':str(folder/'base.png'),'normal':str(folder/'normal.png'),'arm':str(folder/'arm.png'),'height':str(folder/'height.png'),
            'tile_size':[layer['m_TileSize'][c]for c in 'xy'],'tile_offset':[layer['m_TileOffset'][c]for c in 'xy'],
            'normal_scale':layer['m_NormalScale'],'fallback_metallic':layer['m_Metallic'],'fallback_smoothness':layer['m_Smoothness'],
            'mask_remap_min':layer.get('m_MaskMapRemapMin'),'mask_remap_max':layer.get('m_MaskMapRemapMax'),'sources':sources}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--size',type=int,choices=[512,1024,2048],default=1024)
    p.add_argument('--output',type=Path,default=ROOT/'docs/stronghold/2026-11-01/showcase/terrain-materials')
    p.add_argument('--alpha',type=Path,default=ROOT.parent/'.local-tools/unity-source/showcase-native/terrain/00-alpha.f32')
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    paths={e['guid']:e['path']for e in json.loads((DATA/'INVENTORY.json').read_text())['entries']}
    layers=[prepare_layer(guid,paths,args.output,args.size)for guid in ORDER]
    alpha=np.fromfile(args.alpha,'<f4').reshape(1024,1024,8)
    if not np.isfinite(alpha).all()or alpha.min()<-1e-6 or alpha.max()>1+1e-6:raise ValueError('Invalid source terrain alpha')
    total=alpha.sum(2);max_error=float(np.max(np.abs(total-1)))
    if total.min()<=1e-6:raise ValueError('Empty source terrain weights')
    source_sum_range=[float(total.min()),float(total.max())]
    # The original serialized alphamaps include overlapping paint whose sum
    # can exceed one (source maximum1.545). Terrain blending normalizes those
    # weights. Preserve their ratios while providing a stable unit partition.
    alpha=alpha/total[:,:,None]
    # Largest-remainder quantization keeps each eight-channel sum exactly255,
    # avoiding bright/dark seams when blending two separate RGBA weight maps.
    scaled=np.clip(alpha,0,1)*255;weights=np.floor(scaled).astype(np.uint8)
    remainder=255-weights.astype(np.int32).sum(2);rank=np.argsort(-(scaled-weights),axis=2)
    for n in range(8):
        yy,xx=np.nonzero(remainder>n);weights[yy,xx,rank[yy,xx,n]]+=1
    if not np.all(weights.astype(np.int32).sum(2)==255):raise ValueError('Weight quantization sum mismatch')
    for i in range(2):Image.fromarray(weights[:,:,i*4:(i+1)*4],'RGBA').save(args.output/('alpha'+str(i)+'.png'))
    manifest={'version':1,'source':'Purchased NatureManufacture publisher winter castle demo TerrainData','size':args.size,
              'material_png_rows':'standard image top row; source texture V=1','normal_green':'source preserved; shader T=engine+X B=engine-Z on flat terrain',
              'alpha_png_rows':'raw Unity GetAlphamaps[z,x,layer]; row0=sourceZmin; increasing rows=source+Z=engine-Z',
              'upload_flip_png_rows':True,'alpha_shader_axis':'increasing engine Z from engine boundsZmin',
              'alpha0':str(args.output/'alpha0.png'),'alpha1':str(args.output/'alpha1.png'),
              'source_alpha_sha256':hashlib.sha256(args.alpha.read_bytes()).hexdigest(),'alpha_sum_max_error':max_error,
              'source_alpha_sum_range':source_sum_range,'weights_normalized_before_quantization':True,
              'quantized_alpha_sum':255,'layers':layers}
    (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Prepared8terrainlayers; sourcealphasumerror',max_error,'quantizedsum255exact',flush=True)


if __name__=='__main__':main()
