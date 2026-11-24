#define CGLTF_IMPLEMENTATION
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
