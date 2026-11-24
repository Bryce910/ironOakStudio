#!/usr/bin/env python3
"""Preserve tiled source cold/medium/hot texture detail with continuous heat.

Native adaptation: source five-digit emission is compressed to warm HDR0..24,
with dark crust preserved. Three source layers are rows of one image per PBR
role, blended continuously by the private lava shader. No face categories.
"""
from pathlib import Path
import argparse,json
import numpy as np
from PIL import Image
from prepare_nature_lava_materials import ROOT,DATA,LAYERS,prepare_variant,material,texture,resize
from prepare_nature_guardian import linear_to_srgb

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-lava-continuous');args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    results={}
    for name in ['M_Lava_River_3_Trial','M_Lava_River_3_Dynamic_Trial','M_Lava_River_4_Trial']:
        path=str(next((DATA/'source').rglob(name+'.mat')).relative_to(DATA/'source'));f,c,t=material(path);rows={k:[] for k in ['base','normal','arm','emissive']};peaks=[]
        for index,layer in enumerate(LAYERS):
            old=prepare_variant(path,layer,args.output/'source-layers',1024,512)
            for role in ['base','normal','arm']:
                with Image.open(old['paths'][role])as im:rows[role].append(np.asarray(im.convert('RGB')).copy())
            prefix='_'+layer+'Lava';mask,_=texture(t,prefix+'MT_AO_H_EM');alpha=resize(mask.astype(np.float32)/255,1024)[:,:,3]
            intensity=f.get(prefix+'EmissionMaskIntensivity',f.get(prefix+'EmissionMaskIntesivity',1))
            raw=np.power(np.abs(alpha*intensity),f.get(prefix+'EmissionMaskTreshold',1))*1.2
            relative=np.clip(raw/max(float(raw.max()),1e-8),0,1)
            # Expand only the source cracks; retain black/cold surface texels.
            energy=np.sqrt(relative);peak=(5.,14.,24.)[index];red=peak*energy
            green_ratio=.018+(.09,.20,.32)[index]*energy**3
            emitted=np.stack([red,red*green_ratio,red*.0008],axis=2)
            rows['emissive'].append(np.uint8(np.clip(linear_to_srgb(emitted/24.),0,1)*255+.5));peaks.append(float(emitted.max()))
        folder=args.output/name;folder.mkdir(exist_ok=True)
        for role,layers in rows.items():Image.fromarray(np.concatenate(layers,axis=0),'RGB').save(folder/(role+'.png'))
        results[path]={'name':name+'__continuous','status':'candidate','paths':{role:str(folder/(role+'.png')) for role in rows},'emissive_factor':[24,24,24],'uv_scale':[1,1],'double_sided':False,'continuous_heat':True,'atlas_rows_GL_low_v_to_high_v':LAYERS,'source_material':path,'native_emission_peaks':peaks,'limitations':['Static source flow velocity UV4 not exported; native gentle uniform flow substitutes','Original three heat weights reduced to continuous temperature scalar','Source HDR emission compressed to warm native0..24 to retain texture detail']}
        print('CONTINUOUS LAVA',name,peaks,flush=True)
    path=str(next((DATA/'source').rglob('M_Emissive_Red.mat')).relative_to(DATA/'source'));folder=args.output/'bounded-red-fixtures';folder.mkdir(exist_ok=True)
    for role,color in [('base',(3,2,1)),('normal',(128,128,255)),('arm',(255,160,0)),('emissive',(255,255,255))]:Image.new('RGB',(4,4),color).save(folder/(role+'.png'))
    results[path]={'name':'M_Emissive_Red__bounded','status':'candidate','paths':{role:str(folder/(role+'.png')) for role in ['base','normal','arm','emissive']},'emissive_factor':[12.,1.4,.006],'double_sided':False,'continuous_heat':False,'uv_scale':[1,1]}
    (args.output/'materials.json').write_text(json.dumps(results,indent=2)+'\n');print('READY',args.output/'materials.json',flush=True)

if __name__=='__main__':main()
