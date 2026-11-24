#!/usr/bin/env python3
"""Stage, but never install, the lossless source RGB8 + baked-GI contract.

The first city preview uses RGB in the existing extra vec4. This candidate
instead packs three canonical source bytes into the existing scalar, leaving
the extra vec4 available for independently validated baked lighting. All
runtime and shader edits are written under a private candidate directory.
"""
from pathlib import Path
import argparse, hashlib, json

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT.parent / '.local-tools/village-source/candidates/rgb8-gi'

HEADER = r'''#ifndef VALWOOD_WORLD_MODEL_SOURCE_PAINT_H
#define VALWOOD_WORLD_MODEL_SOURCE_PAINT_H
#ifndef CGLTF_H_INCLUDED__
#include "cgltf.h"
#endif
#include <cmath>
#include <cstdint>
#include <cstring>
namespace world_model_source_paint {
// RGB8 is a separate, explicitly enabled scalar interpretation. Every uint24
// value is exact in float32; decode before interpolation, never afterwards.
inline float pack(const float rgb[3]) {
    uint32_t value=0;
    for(unsigned i=0;i<3;i++) {
        if(!std::isfinite(rgb[i])||rgb[i]<0||rgb[i]>1)
            throw "Invalid source RGB paint sample";
        const uint32_t byte=uint32_t(std::lround(rgb[i]*255.f));
        if(std::fabs(rgb[i]-float(byte)/255.f)>0.000002f)
            throw "Source paint is not canonical RGB8";
        value|=byte<<(8*i);
    }
    return float(value);
}
inline void unpack(float packed,float rgb[3]) {
    if(!std::isfinite(packed)||packed<0||packed>16777215.f||packed!=std::floor(packed))
        throw "Invalid packed source RGB8";
    const uint32_t value=uint32_t(packed);
    for(unsigned i=0;i<3;i++)rgb[i]=float((value>>(8*i))&255u)/255.f;
}
inline const cgltf_accessor *accessor(const cgltf_primitive &primitive,cgltf_size count) {
    const cgltf_accessor *result=nullptr;bool incompatible=false;
    for(cgltf_size i=0;i<primitive.attributes_count;i++) {
        const auto &a=primitive.attributes[i];if(!a.name)continue;
        if(!std::strcmp(a.name,"_VALWOOD_SOURCE_PAINT")) {
            if(result)throw "Duplicate source RGB paint stream";
            result=a.data;if(!result)throw "Missing source RGB paint accessor";
        }else if(!std::strcmp(a.name,"_VALWOOD_PAINT")||!std::strcmp(a.name,"_VALWOOD_LAVA_HEAT"))incompatible=true;
    }
    // Baked GI has its own validated vec4 stream and may now coexist.
    if(result&&incompatible)throw "Ambiguous source RGB paint/legacy paint/lava streams";
    if(result&&(result->count!=count||result->type!=cgltf_type_vec3||result->component_type!=cgltf_component_type_r_32f||result->normalized))throw "Invalid source RGB paint stream layout";
    return result;
}
inline void read(const cgltf_accessor *source,cgltf_size index,float[4],float &packed) {
    if(!source)return;
    float rgb[3];
    if(!cgltf_accessor_read_float(source,index,rgb,3))throw "Unreadable source RGB paint";
    packed=pack(rgb); // Keep the previously decoded baked-light vec4 exact.
}
}
#endif
'''

TEST = r'''#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "engine/world_model_source_paint.h"
#include "engine/world_model_baked_light.h"
#include "engine/world_model_cpu.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
template<class F>void rejects(F f){bool caught=false;try{f();}catch(...){caught=true;}assert(caught);}
int main(){
    // Exhaustive representability and shader-equivalent byte extraction.
    for(uint32_t n=0;n<16777216u;n++){
        const float f=float(n);assert(uint32_t(f)==n);
        const float r=std::fmod(f,256.f),g=std::fmod(std::floor(f/256.f),256.f),b=std::floor(f/65536.f);
        assert(uint32_t(r)==(n&255u)&&uint32_t(g)==((n>>8)&255u)&&uint32_t(b)==(n>>16));
    }
    float values[3][3]={{0,1,64.f/255},{1,0,192.f/255},{128.f/255,51.f/255,1}};
    cgltf_buffer buffer={};buffer.data=values;buffer.size=sizeof(values);
    cgltf_buffer_view view={};view.buffer=&buffer;view.size=sizeof(values);
    cgltf_accessor a={};a.buffer_view=&view;a.count=3;a.type=cgltf_type_vec3;a.component_type=cgltf_component_type_r_32f;a.stride=sizeof(values[0]);
    cgltf_attribute attrs[2]={};attrs[0].name=const_cast<char*>("_VALWOOD_SOURCE_PAINT");attrs[0].data=&a;
    cgltf_primitive p={};p.attributes=attrs;p.attributes_count=1;
    auto *source=world_model_source_paint::accessor(p,3);assert(source==&a);
    const float original_gi[4]={12.5f,.001f,65504.f,.75f};float rgba[4],packed=0,rgb[3];
    for(unsigned i=0;i<3;i++){
        std::memcpy(rgba,original_gi,sizeof(rgba));world_model_source_paint::read(source,i,rgba,packed);
        assert(!std::memcmp(rgba,original_gi,sizeof(rgba)));world_model_source_paint::unpack(packed,rgb);
        for(unsigned c=0;c<3;c++)assert(rgb[c]==values[i][c]);
    }
    cgltf_buffer gi_buffer={};gi_buffer.data=const_cast<float*>(original_gi);gi_buffer.size=sizeof(original_gi);
    cgltf_buffer_view gi_view={};gi_view.buffer=&gi_buffer;gi_view.size=sizeof(original_gi);
    cgltf_accessor gi_access={};gi_access.buffer_view=&gi_view;gi_access.count=1;gi_access.type=cgltf_type_vec4;gi_access.component_type=cgltf_component_type_r_32f;gi_access.stride=sizeof(original_gi);
    attrs[1].name=const_cast<char*>("_VALWOOD_BAKED_LIGHT");attrs[1].data=&gi_access;p.attributes_count=2;
    assert(world_model_source_paint::accessor(p,3)==&a);
    a.count=1;assert(world_model_baked_light::accessor(p,1)==&gi_access);assert(world_model_source_paint::accessor(p,1)==&a);
    world_model_baked_light::read(&gi_access,0,rgba);world_model_source_paint::read(&a,0,rgba,packed);
    assert(!std::memcmp(rgba,original_gi,sizeof(rgba)));a.count=3;
    for(const char *name:{"_VALWOOD_PAINT","_VALWOOD_LAVA_HEAT"}){attrs[1].name=const_cast<char*>(name);rejects([&](){world_model_source_paint::accessor(p,3);});}
    attrs[1]=attrs[0];rejects([&](){world_model_source_paint::accessor(p,3);});p.attributes_count=1;
    rejects([&](){world_model_source_paint::accessor(p,2);});a.type=cgltf_type_vec4;rejects([&](){world_model_source_paint::accessor(p,3);});a.type=cgltf_type_vec3;
    a.normalized=true;rejects([&](){world_model_source_paint::accessor(p,3);});a.normalized=false;
    for(float bad:{-1.f,1.001f,NAN,INFINITY,.25f}){values[0][0]=bad;rejects([&](){world_model_source_paint::read(source,0,rgba,packed);});}
    for(float bad:{-1.f,16777216.f,.25f,NAN,INFINITY})rejects([&](){world_model_source_paint::unpack(bad,rgb);});
    static_assert(sizeof(world_model_cpu::Vertex)==68,"No vertex ABI or GPU allocation increase");
    std::puts("PASS candidate RGB8+GI: all 16777216 exact scalar values, vertex-before-interpolation decode, canonical byte validation, GI vec4 unchanged, legacy/lava rejection, 68-byte ABI");
}
'''

def replace_once(value, old, new):
    if value.count(old) != 1:
        raise RuntimeError('Candidate source anchor changed: ' + old[:80])
    return value.replace(old, new, 1)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()
    out = args.output.resolve()
    if out == ROOT or out == ROOT/'engine' or out == ROOT/'assets/purchased/medieval_village/city-materials/shaders':
        raise ValueError('Candidate output must be isolated')
    (out/'runtime/engine').mkdir(parents=True, exist_ok=True)
    (out/'shaders').mkdir(exist_ok=True)
    (out/'tests').mkdir(exist_ok=True)
    (out/'runtime/engine/world_model_source_paint.h').write_text(HEADER)
    # Both loaders already call read after GI decode. The candidate header's
    # same signature preserves GI while putting packed paint into the scalar.
    snapshots = {}
    for name in ['engine/world_model_cpu.cpp','engine/world_models.cpp','engine/world_model_source_paint.h']:
        src=ROOT/name;snapshots[name]=hashlib.sha256(src.read_bytes()).hexdigest()
        if not name.endswith('source_paint.h'):
            value=src.read_text()
            if value.count('world_model_source_paint::read(source_paint,vi,v.baked_light,v.facade_paint);') != 1:
                raise RuntimeError('Source loader read order changed: '+name)
            (out/'runtime'/name).write_text(value)
    source=ROOT/'assets/purchased/medieval_village/city-materials/shaders'
    v=(source/'world_foliage.vs.glsl').read_text()
    v=replace_once(v,'#ifndef VALWOOD_CHARACTER_APPEARANCE\nlayout(location=12) in vec4 aSourceExtra;\n#endif\n','')
    v=replace_once(v,'''#ifndef VALWOOD_CHARACTER_APPEARANCE
    if(uSourcePaintEnabled&&!uPoseBlendEnabled&&aSourceExtra.w<-.5)
        vSourcePaint=clamp(aSourceExtra.rgb,vec3(0),vec3(1));
#endif''','''    // Source RGB8 is decoded per vertex; interpolation operates on RGB.
    if(uSourcePaintEnabled&&!uPoseBlendEnabled) {
        float value24=floor(aFacadePaint+.5);
        vSourcePaint=vec3(mod(value24,256.),mod(floor(value24/256.),256.),floor(value24/65536.))/255.;
    }''')
    v=replace_once(v,'    vFacadePaint=aFacadePaint;','    vFacadePaint=uSourcePaintEnabled?vSourcePaint.r:aFacadePaint;')
    (out/'shaders/world_foliage.vs.glsl').write_text(v)
    (out/'shaders/world_foliage.fs.glsl').write_text((source/'world_foliage.fs.glsl').read_text())
    for p in source.glob('*.glsl'):snapshots[str(p.relative_to(ROOT))]=hashlib.sha256(p.read_bytes()).hexdigest()
    (out/'tests/source_rgb8_gi_test.cpp').write_text(TEST)
    audit=dict(candidate_only=True,installed=False,source_snapshots=snapshots,Vertex_bytes=68,
        RGB8_contract='R | (G << 8) | (B << 16), uint24 stored exactly in float32; no normalized attribute or interpolation of packed values',
        validity='uSourcePaintEnabled distinguishes packed source values from ordinary scalar paint/lava; GI keeps independent RGBA validity unchanged',
        canonical_input_tolerance=0.000002,
        future_integration=['Verify decoded city source GI and source sunlight bake contribution first','Install candidate loader/header + private VS together; old union RGB vertices are incompatible with the new opt-in interpretation','Add validated baked-light VS/FS path after removing old source-extra location12 declaration; do not duplicate location12','Rebuild both direct and streamed parser paths and relink city runtime, then run full CPU sweep + native A/B'],
        production_files_modified=False)
    (out/'audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(out)

if __name__=='__main__':main()
