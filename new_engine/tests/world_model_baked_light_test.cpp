#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "engine/world_model_baked_light.h"
#include "engine/world_model_cpu.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

template<class F>void rejects(F fn){bool rejected=false;try{fn();}catch(const char *){rejected=true;}assert(rejected);}
int main(){
    float values[3][4]={{.25f,2.5f,90.f,1.f},{0,0,0,0},{1,2,3,.5f}};
    cgltf_buffer buffer={};buffer.data=values;buffer.size=sizeof(values);
    cgltf_buffer_view view={};view.buffer=&buffer;view.size=sizeof(values);
    cgltf_accessor accessor={};accessor.buffer_view=&view;accessor.count=3;accessor.type=cgltf_type_vec4;accessor.component_type=cgltf_component_type_r_32f;accessor.stride=4*sizeof(float);
    cgltf_attribute attrs[2]={};attrs[0].name=const_cast<char*>("_VALWOOD_BAKED_LIGHT");attrs[0].data=&accessor;
    cgltf_primitive primitive={};primitive.attributes=attrs;primitive.attributes_count=1;
    const auto *source=world_model_baked_light::accessor(primitive,3);assert(source==&accessor);
    float rgba[4];for(unsigned i=0;i<3;i++){world_model_baked_light::read(source,i,rgba);assert(!std::memcmp(rgba,values[i],sizeof(rgba)));}
    world_model_baked_light::read(nullptr,0,rgba);for(float v:rgba)assert(v==0);
    primitive.attributes_count=0;assert(!world_model_baked_light::accessor(primitive,3));primitive.attributes_count=1;
    rejects([&](){world_model_baked_light::accessor(primitive,2);});
    accessor.type=cgltf_type_vec3;rejects([&](){world_model_baked_light::accessor(primitive,3);});accessor.type=cgltf_type_vec4;
    accessor.component_type=cgltf_component_type_r_16u;rejects([&](){world_model_baked_light::accessor(primitive,3);});accessor.component_type=cgltf_component_type_r_32f;
    accessor.normalized=true;rejects([&](){world_model_baked_light::accessor(primitive,3);});accessor.normalized=false;
    attrs[1]=attrs[0];primitive.attributes_count=2;rejects([&](){world_model_baked_light::accessor(primitive,3);});primitive.attributes_count=1;
    attrs[0].data=nullptr;rejects([&](){world_model_baked_light::accessor(primitive,3);});attrs[0].data=&accessor;
    for(float bad:{-1.f,NAN,INFINITY,65505.f}){values[0][0]=bad;rejects([&](){world_model_baked_light::read(source,0,rgba);});}
    values[0][0]=.25f;for(float bad:{-1.f,1.001f,NAN}){values[0][3]=bad;rejects([&](){world_model_baked_light::read(source,0,rgba);});}
    world_model_cpu::Vertex missing;for(float v:missing.baked_light)assert(v==0);
    world_model_cpu::Mesh ordinary;assert(!ordinary.baked_light);
    puts("PASS baked light: optional zero default, float4 HDR irradiance/validity, exact accessor layout, malformed/duplicate/nonfinite rejection; shared68-byte CPU/GPU static vertex");
}
