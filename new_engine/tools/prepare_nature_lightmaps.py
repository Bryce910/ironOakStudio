"""Decode purchased Unity EXR lightmaps into linear float32 CPU sampling caches.

Run with Blender's Python (CPU file decoding only, no rendering):
blender -b -t 2 --python tools/prepare_nature_lightmaps.py
"""
from pathlib import Path
import hashlib,json,sys
import bpy
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/purchased/nature_castle/source/Assets/NatureManufacture Assets/Castle and Dungeon/Scenes/Demo Castle'
OUT=ROOT.parent/'.local-tools/unity-source/showcase-lightmaps'


def main():
    OUT.mkdir(parents=True,exist_ok=True);records=[]
    for index in range(8):
        path=SOURCE/('Lightmap-'+str(index)+'_comp_light.exr')
        direction=SOURCE/('Lightmap-'+str(index)+'_comp_dir.png')
        image=bpy.data.images.load(str(path),check_existing=False)
        w,h=image.size;channels=image.channels
        if channels<3 or not image.is_float:raise ValueError('Expected floating-point RGB EXR')
        pixels=np.empty(w*h*channels,np.float32);image.pixels.foreach_get(pixels)
        # Blender pixels use UV-bottom-left row order, already matching Unity
        # lightmap UV1. No color conversion, RGBM expansion, or V flip needed.
        rgb=pixels.reshape(h,w,channels)[:,:,:3].copy()
        nonfinite=int((~np.isfinite(rgb)).sum());negative=int((rgb<0).sum());above=int((rgb>65504).sum())
        if nonfinite:raise ValueError('Non-finite source lightmap pixels')
        raw_range=[float(rgb.min()),float(rgb.max())]
        rgb=np.clip(rgb,0,65504).astype(np.float32)
        target=OUT/('lightmap-'+str(index)+'.npy');np.save(target,rgb,allow_pickle=False)
        records.append({'index':index,'source':str(path.relative_to(ROOT)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                        'cache':str(target),'width':w,'height':h,'channels':3,'dtype':'float32','cache_rows':'row0=UnityUVv0 (bottom)',
                        'raw_rgb_range':raw_range,'negative_components_clipped':negative,'above65504_clipped':above,'nonfinite_components':nonfinite,
                        'direction_source':str(direction.relative_to(ROOT))if direction.is_file()else None,
                        'direction_sha256':hashlib.sha256(direction.read_bytes()).hexdigest()if direction.is_file()else None,
                        'rgb_mean':rgb.mean((0,1)).tolist()})
        bpy.data.images.remove(image);del image,pixels,rgb
        print('Decoded linearEXR',index,w,h,'rawrange',raw_range,flush=True)
    manifest={'version':1,'source':'Purchased Demo Castle LightingDataAsset atlas indices0..7 verified by source ordered PPtrs',
              'decode':'Unity FULL_HDR DecodeLightmap returns encodedIlluminance.rgb; source EXR stores linear HDR RGB, not GPU RGBM',
              'decode_multiplier':1,'gamma_decode':False,'cache_rows':'Blender image pixels, UVv0 bottom row',
              'direction_decode':'rgb * (dot(unitNormalSourceUnityWorld, dir.rgb - .5) + .5) / max(1e-4, dir.a); dir vector is intentionally not normalized',
              'shader_evidence':'Editor/Data/Resources/PackageManager/BuiltInPackages/com.unity.render-pipelines.core/ShaderLibrary/EntityLighting.hlsl DecodeLightmap and SampleDirectionalLightmap',
              'atlas':records}
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')


if __name__=='__main__':main()
