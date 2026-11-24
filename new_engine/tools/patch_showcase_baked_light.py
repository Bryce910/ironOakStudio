#!/usr/bin/env python3
"""Opt only the private source-showcase scenery shaders into baked diffuse GI."""
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

VERTEX_DECL='''// VALWOOD_SHOWCASE_BAKED_LIGHT
#ifndef VALWOOD_CHARACTER_APPEARANCE
layout(location=12) in vec4 aBakedLight;
#endif
uniform bool uBakedLightEnabled=false;
out vec4 vBakedLight;
'''
VERTEX_MAIN='''    // Baked light belongs to immutable source scenery, never posed people.
    vBakedLight=vec4(0);
#ifndef VALWOOD_CHARACTER_APPEARANCE
    if(uBakedLightEnabled&&!uPoseBlendEnabled&&uVegetation==0)
        vBakedLight=vec4(aBakedLight.rgb*aBakedLight.a,aBakedLight.a);
#endif
'''
FRAGMENT_GI='''    // The purchased LightingDataAsset marks the sun Mixed/IndirectOnly.
    // Replace diffuse ambient with its source irradiance; keep native direct
    // sun, environment specular, moving lights and emission. Validity uses a
    // premultiplied varying so fallback edges cannot darken baked neighbours.
    float bakedWeight=(!leaf&&uVegetation==0&&!uGrass)?clamp(vBakedLight.a,0.,1.):0.;
    if(bakedWeight>0.){
        float nv=clamp(dot(geometricN,V),0.,1.);
        vec3 f0=mix(vec3(mix(.035+.005*clamp(uWetness,0.,1.),.018,uWinterSurface.y)),base.rgb,metal);
        vec3 fresnel=f0+(max(vec3(1.-rough),f0)-f0)*pow(1.-nv,5.);
        vec3 diffuseFactor=base.rgb*(1.-fresnel)*(1.-metal)*ao;
        vec3 sourceIrradiance=max(vBakedLight.rgb/max(bakedWeight,.0001),vec3(0));
        indirect+=diffuseFactor*(sourceIrradiance-ambient)*bakedWeight;
        indirect=max(indirect,vec3(0));
    }
'''

def replace_once(text,old,new):
 if text.count(old)!=1:raise RuntimeError(f'Expected one shader anchor: {old[:70]}')
 return text.replace(old,new,1)

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--project',type=Path,default=ROOT/'projects/valwood_showcase');args=parser.parse_args()
 vertex=args.project/'shaders/world_foliage.vs.glsl';fragment=args.project/'shaders/world_foliage.fs.glsl'
 v=vertex.read_text();f=fragment.read_text()
 v=v.replace('length(winterRoot-vec2(-43344.0,-23987.82415651827))<540.',
             'length(winterRoot-vec2(-43344.0,-23987.82415651827))<900.')
 if 'VALWOOD_SHOWCASE_BAKED_LIGHT' not in v:
  v=replace_once(v,'uniform bool uPoseBlendEnabled;',VERTEX_DECL+'uniform bool uPoseBlendEnabled;')
  v=replace_once(v,'void main() {','void main() {\n'+VERTEX_MAIN)
 if 'VALWOOD_SHOWCASE_BAKED_LIGHT' not in f:
  f=replace_once(f,'in vec3 vPos,vNormal,vRelative,vTangent,vBitangent;','// VALWOOD_SHOWCASE_BAKED_LIGHT\nin vec4 vBakedLight;\nin vec3 vPos,vNormal,vRelative,vTangent,vBitangent;')
  anchor='    if(!leaf)indirect=opaque_environment(base.rgb,metal,rough,geometricN,V,skyLight,groundLight,ambient,ao);'
  f=replace_once(f,anchor,anchor+'\n'+FRAGMENT_GI)
  f=replace_once(f,'indirect+=base.rgb*(1.-metal)*ao*tavern_diffuse_return();','indirect+=base.rgb*(1.-metal)*ao*tavern_diffuse_return()*(1.-bakedWeight);')
 vertex.write_text(v);fragment.write_text(f)
 print(f'Private baked diffuse GI enabled: {vertex}, {fragment}; native direct sun retained')

if __name__=='__main__':main()
