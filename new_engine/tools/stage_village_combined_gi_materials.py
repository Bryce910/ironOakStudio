#!/usr/bin/env python3
"""Bundle isolated RGB8/GI coexistence and verified source material inputs."""
from pathlib import Path
import argparse, hashlib, json, shutil, struct, subprocess, sys

ROOT=Path(__file__).resolve().parents[1]
PRIVATE=ROOT.parent/'.local-tools/village-source'

CPU_TEST=r'''#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_model_cpu.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(int argc,char**argv){
    assert(argc==2);world_model_cpu::Options o;o.bc7=true;o.reuse_decoder_scratch=true;o.cpu_bytes=768ull*1024*1024;
    std::string error;auto a=world_model_cpu::prepare(argv[1],o,error);if(!a){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    assert(a->meshes.size()==1);const auto&m=a->meshes[0];assert(m.source_paint&&m.baked_light&&m.vertices.size()==4);
    const float paint[4]={0,16777215,11763840,19712};
    const float gi[4][4]={{.25,.5,1,1},{2,3,4,.75},{.001f,12,65504,.5},{0,0,0,0}};
    for(unsigned i=0;i<4;i++){assert(m.vertices[i].facade_paint==paint[i]);for(unsigned c=0;c<4;c++)assert(m.vertices[i].baked_light[c]==gi[i][c]);}
    static_assert(sizeof(world_model_cpu::Vertex)==68,"No vertex ABI increase");texture_cache_shutdown();
    std::puts("PASS actual native CPU candidate: source RGB8 scalar and all HDR/validity RGBA values preserved independently under 768 MiB budget");
}
'''

def once(value,old,new):
    if value.count(old)!=1:raise RuntimeError('City GI shader anchor changed: '+old[:90])
    return value.replace(old,new,1)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,default=PRIVATE/'candidates/combined-gi-materials')
    ap.add_argument('--include-foliage-gi',action='store_true')
    ap.add_argument('--keep-dynamic-sky',action='store_true',help='Add baked GI independently; retain authored movable-skylight diffuse and native bounce')
    args=ap.parse_args();out=args.output.resolve()
    if args.keep_dynamic_sky and out==(PRIVATE/'candidates/combined-gi-materials').resolve():
        raise ValueError('Use a new --output revision; the original replacement candidate is frozen')
    subprocess.run([sys.executable,str(ROOT/'tools/stage_village_rgb8_gi_candidate.py'),'--output',str(out)],check=True)
    graph=out/'graph-correction'
    subprocess.run([sys.executable,str(ROOT/'tools/stage_village_material_graph_candidate.py'),'--source',str(out/'shaders'),'--output',str(graph)],check=True)
    shutil.copyfile(graph/'world_foliage.fs.glsl',out/'shaders/world_foliage.fs.glsl')
    vpath=out/'shaders/world_foliage.vs.glsl';fpath=out/'shaders/world_foliage.fs.glsl'
    v=vpath.read_text();f=fpath.read_text()
    declaration='''// VALWOOD_CITY_SOURCE_BAKED_LIGHT
#ifndef VALWOOD_CHARACTER_APPEARANCE
layout(location=12) in vec4 aBakedLight;
#endif
uniform bool uBakedLightEnabled=false;
out vec4 vBakedLight;
'''
    condition='uBakedLightEnabled&&!uPoseBlendEnabled'+(''if args.include_foliage_gi else'&&uVegetation==0')
    v=once(v,'uniform bool uPoseBlendEnabled;',declaration+'uniform bool uPoseBlendEnabled;')
    v=once(v,'void main() {','''void main() {
    // Decode a separate validated GI stream. Premultiplication prevents a
    // fallback vertex from darkening a neighbouring valid sample.
    vBakedLight=vec4(0);
#ifndef VALWOOD_CHARACTER_APPEARANCE
    if('''+condition+''')
        vBakedLight=vec4(aBakedLight.rgb*aBakedLight.a,aBakedLight.a);
#endif''')
    f=once(f,'in vec3 vPos,vNormal,vRelative,vTangent,vBitangent;','// VALWOOD_CITY_SOURCE_BAKED_LIGHT\nin vec4 vBakedLight;\nin vec3 vPos,vNormal,vRelative,vTangent,vBitangent;')
    weight='clamp(vBakedLight.a,0.,1.)'if args.include_foliage_gi else'(!leaf&&uVegetation==0&&!uGrass)?clamp(vBakedLight.a,0.,1.):0.'
    block='''
    // Source UE HQ-lightmap output is linear OutDiffuseLighting. No extra
    // gamma, exposure or PI conversion belongs here. The current source Sun
    // and SkyLight are Movable: replace ambient diffuse, retain dynamic sun
    // and the native environment specular. Historic bake mobility is unknown.
    float bakedWeight='''+weight+''';
    if(bakedWeight>0.) {
        vec3 sourceIrradiance=max(vBakedLight.rgb/max(bakedWeight,.0001),vec3(0));
        if(leaf) {
            indirect=mix(indirect,base.rgb*sourceIrradiance*ao,bakedWeight);
        } else {
            float nv=clamp(dot(geometricN,V),0.,1.);
            vec3 f0=mix(vec3(.035+.005*clamp(uWetness,0.,1.)),base.rgb,metal);
            vec3 fresnel=f0+(max(vec3(1.-rough),f0)-f0)*pow(1.-nv,5.);
            vec3 diffuseFactor=base.rgb*(1.-fresnel)*(1.-metal)*ao;
            indirect+=diffuseFactor*(sourceIrradiance-ambient)*bakedWeight;
            indirect=max(indirect,vec3(0));
        }
    }
'''
    anchor='    if(!leaf)indirect=opaque_environment(base.rgb,metal,rough,geometricN,V,skyLight,groundLight,ambient,ao);'
    f=once(f,anchor,anchor+block)
    f=once(f,'indirect+=base.rgb*(1.-metal)*ao*tavern_diffuse_return();','indirect+=base.rgb*(1.-metal)*ao*tavern_diffuse_return()*(1.-bakedWeight);')
    if args.include_foliage_gi:
        # The already-cheap far-card path returns before ordinary PBR. It can
        # still use its validated vertex GI without any extra texture fetch.
        card='        vec3 indirect=base.rgb*mix(groundLight,skyLight,.70)*1.12*tint;'
        f=once(f,card,card+'''
        float cardBakedWeight=clamp(vBakedLight.a,0.,1.);
        if(cardBakedWeight>0.) {
            vec3 sourceIrradiance=max(vBakedLight.rgb/max(cardBakedWeight,.0001),vec3(0));
            indirect=mix(indirect,base.rgb*sourceIrradiance,cardBakedWeight);
        }''')
    if args.keep_dynamic_sky:
        f=once(f,'''    // and SkyLight are Movable: replace ambient diffuse, retain dynamic sun
    // and the native environment specular. Historic bake mobility is unknown.''','''    // and SkyLight are Movable: baked GI adds independently to dynamic sky
    // diffuse, direct sunlight and environment specular. Historic bake
    // mobility is unknown; near-black architecture bakes prove no fill.''')
        f=once(f,'indirect=mix(indirect,base.rgb*sourceIrradiance*ao,bakedWeight);','indirect+=base.rgb*sourceIrradiance*ao*bakedWeight;')
        f=once(f,'indirect+=diffuseFactor*(sourceIrradiance-ambient)*bakedWeight;','indirect+=diffuseFactor*sourceIrradiance*bakedWeight;')
        f=once(f,'indirect+=base.rgb*(1.-metal)*ao*tavern_diffuse_return()*(1.-bakedWeight);','indirect+=base.rgb*(1.-metal)*ao*tavern_diffuse_return();')
        if args.include_foliage_gi:
            f=once(f,'indirect=mix(indirect,base.rgb*sourceIrradiance,cardBakedWeight);','indirect+=base.rgb*sourceIrradiance*cardBakedWeight;')
    vpath.write_text(v);fpath.write_text(f)
    # A real parser fixture exercises simultaneous source paint + HDR GI,
    # including partial validity and a 65504 HDR endpoint. Existing source
    # images are copied into the isolated GLB without changing those images.
    sys.path.insert(0,str(ROOT/'tools'))
    import numpy as np
    from restore_village_source_windows import append
    from build_purchased_village import encode_glb
    fixture=ROOT/'assets/purchased/medieval_village/city-materials/fixtures/MI_Wood_Trims_01.glb'
    data=fixture.read_bytes();size,kind=struct.unpack_from('<II',data,12);assert kind==0x4e4f534a
    doc=json.loads(data[20:20+size]);offset=20+size;binary_size,kind=struct.unpack_from('<II',data,offset);assert kind==0x004e4942
    raw=bytearray(data[offset+8:offset+8+binary_size])
    values=np.array([[.25,.5,1,1],[2,3,4,.75],[.001,12,65504,.5],[0,0,0,0]],'<f4')
    doc['meshes'][0]['primitives'][0]['attributes']['_VALWOOD_BAKED_LIGHT']=append(doc,raw,values,'VEC4')
    doc['buffers'][0]['byteLength']=len(raw)
    (out/'tests/source-rgb8-with-gi.glb').write_bytes(encode_glb(doc,raw))
    (out/'tests/source_rgb8_gi_cpu_test.cpp').write_text(CPU_TEST)
    files={str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest()for folder in ['runtime','shaders','tests']for p in(out/folder).rglob('*')if p.is_file()}
    ready=dict(candidate_only=True,installed=False,files=files,includes_foliage_GI=args.include_foliage_gi,dynamic_sky_diffuse_retained=args.keep_dynamic_sky,
        RGB8_and_GI_coexist=True,Vertex_bytes=68,incremental_vertex_GPU_bytes=0,incremental_texture_GPU_bytes=0,
        source_lightmap_contract='UE GetLightMapColorHQ OutDiffuseLighting=Luma*UVW, linear RGB multiplied directly by native diffuse factor; no extra PI conversion',
        sun_scope='Both current original Sun and SkyLight are Movable; dynamic sun and sky remain independent of baked GI when --keep-dynamic-sky is used. Historic bake settings are not proven.',
        loader_changes='Source header allows GI coexistence and uses scalar uint24; both direct and streamed read calls preserve baked RGBA; cloth mode5 validation fix retained',
        must_install_together=['runtime/engine/world_model_source_paint.h','shaders/world_foliage.vs.glsl','shaders/world_foliage.fs.glsl'],
        verification_pending=['Full real GI catalog CPU loader sweep','Native GLSL link and closeups','Sustained native frame-time/VRAM check'],
        production_files_modified=False)
    (out/'READY.json').write_text(json.dumps(ready,indent=2)+'\n');print('COMBINED CANDIDATE READY',out)

if __name__=='__main__':main()
