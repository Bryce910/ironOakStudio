#!/usr/bin/env python3
"""Prepare a static dry-stone PBR atlas from the purchased Unity material."""
from pathlib import Path
import json
import re
import numpy as np
from PIL import Image
import yaml

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'assets/purchased/nature_castle'
SOURCE=DATA/'source'
OUT=DATA/'textures/guardian'


def srgb_to_linear(x):
    return np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)


def linear_to_srgb(x):
    return np.where(x<=.0031308,x*12.92,1.055*np.maximum(x,0)**(1/2.4)-.055)


def main():
    inventory=json.loads((DATA/'INVENTORY.json').read_text())
    guids={e['guid']:e['path'] for e in inventory['entries']}
    material=next(SOURCE.rglob('M_stone_guard_01.mat'))
    text=re.sub(r'^%.*\n','',material.read_text(),flags=re.M)
    text=re.sub(r'^--- !u!\d+ &-?\d+','---',text,flags=re.M)
    mat=next(d['Material'] for d in yaml.safe_load_all(text) if d and 'Material' in d)
    saved=mat['m_SavedProperties']
    floats={k:v for item in saved['m_Floats'] for k,v in item.items()}
    colors={k:v for item in saved['m_Colors'] for k,v in item.items()}
    tex={k:v for item in saved['m_TexEnvs'] for k,v in item.items()}
    sources={}
    size=2048

    def image(key,repeat=1):
        path=SOURCE/guids[tex[key]['m_Texture']['guid']]
        sources[key]=str(path.relative_to(ROOT))
        with Image.open(path) as raw:
            if repeat!=1:
                tile=raw.convert('RGBA').resize((size//repeat,size//repeat),Image.Resampling.LANCZOS)
                im=Image.new('RGBA',(size,size))
                for y in range(repeat):
                    for x in range(repeat):im.paste(tile,(x*tile.width,y*tile.height))
            else:im=raw.convert('RGBA').resize((size,size),Image.Resampling.LANCZOS)
            return np.asarray(im,dtype=np.float32)/255

    base=image('_BaseColorMap')
    mask=image('_BaseMaskMap')
    shape=image('_ShapeCurvAOHLeaksMask')
    leaks=image('_LeaksR',2)[:,:,0:1]
    tint=np.array([colors['_BaseColor'][c] for c in 'rgb'],dtype=np.float32)
    stain=np.array([colors['_BaseLeaksColorMultiply'][c] for c in 'rgb'],dtype=np.float32)
    stain_weight=np.clip(colors['_BaseLeaksColorMultiply']['a']*(1-shape[:,:,3:4]),0,1)
    rgb=srgb_to_linear(base[:,:,:3])*tint
    stain_color=(1-leaks)**2*stain+leaks
    rgb*=1-stain_weight+stain_weight*stain_color
    curvature=np.clip(rgb+2*shape[:,:,0:1]-1,0,1)
    reduction=floats.get('_Shape_AO_Curvature_Reduction',0)
    curvature*=1-reduction+reduction*shape[:,:,1:2]
    amount=floats.get('_CurvatureBlend',0)
    rgb=rgb*(1-amount)+curvature*amount

    normals=[]
    for key,strength in [('_BaseNormalMap',floats.get('_BaseNormalScale',1)),
                         ('_ShapeNormal',floats.get('_ShapeNormalStrenght1',1))]:
        n=image(key)[:,:,:3]*2-1
        n[:,:,:2]*=strength
        n[:,:,2]=np.sqrt(np.maximum(1-(n[:,:,:2]**2).sum(axis=2),0))
        normals.append(n)
    normal=np.dstack((normals[0][:,:,0]+normals[1][:,:,0],
                      normals[0][:,:,1]+normals[1][:,:,1],
                      normals[0][:,:,2]*normals[1][:,:,2]))
    normal/=np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),1e-8)

    smooth=floats.get('_BaseSmoothnessRemapMin',0)+mask[:,:,3:4]*(floats.get('_BaseSmoothnessRemapMax',1)-floats.get('_BaseSmoothnessRemapMin',0))
    smooth*=1-stain_weight+stain_weight*(floats.get('_LeaksSmoothnessMultiply',1)*(1-leaks)+leaks)
    ao=mask[:,:,1:2]*shape[:,:,1:2]
    metal=mask[:,:,0:1]*floats.get('_BaseMetallic',1)
    arm=np.concatenate((ao,1-np.clip(smooth,0,1),metal),axis=2)
    OUT.mkdir(parents=True,exist_ok=True)
    for role,value in [('base',linear_to_srgb(rgb)),('normal',normal*.5+.5),('arm',arm)]:
        Image.fromarray(np.uint8(np.clip(value,0,1)*255+.5)).save(OUT/(role+'.png'))
    report={'material':str(material.relative_to(ROOT)),'texture_size':size,'sources':sources,
            'variant':'Static dry-stone guardian: source color, curvature, stains, normals and packed surface properties',
            'dynamic_cover_wetness_and_heat':'Handled separately in the game; excluded from this static variant',
            'required_uv_relationship':'Body UV0 and UV2 match; shield base-grain variant uses UV0 because its source has no UV2',
            'textures':{role:str((OUT/(role+'.png')).relative_to(ROOT)) for role in ['base','normal','arm']}}
    (OUT/'material.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
