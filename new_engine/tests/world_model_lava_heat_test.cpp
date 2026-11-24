#define CGLTF_IMPLEMENTATION
#include "engine/world_model_lava_heat.h"
#include <cassert>
#include <limits>
#include <cstdio>
#include <initializer_list>

int main(){
    float samples[]={0,.25f,1};cgltf_buffer buffer={};buffer.data=samples;buffer.size=sizeof samples;
    cgltf_buffer_view view={};view.buffer=&buffer;view.size=sizeof samples;
    cgltf_accessor data={};data.buffer_view=&view;data.component_type=cgltf_component_type_r_32f;data.type=cgltf_type_scalar;data.count=3;data.stride=sizeof(float);
    cgltf_attribute attributes[2]={};attributes[0].name=const_cast<char*>("_VALWOOD_LAVA_HEAT");attributes[0].data=&data;
    cgltf_primitive primitive={};primitive.attributes=attributes;primitive.attributes_count=1;
    unsigned checks=0;
    assert(world_model_lava_heat::accessor(primitive,3)==&data);++checks;
    for(unsigned i=0;i<3;i++){float tagged=0;world_model_lava_heat::read(&data,i,tagged);assert(tagged==-1.f-samples[i]);++checks;}
    float ordinary=.6f;world_model_lava_heat::read(nullptr,0,ordinary);assert(ordinary==.6f);++checks;
    auto rejects=[&](){bool rejected=false;try{world_model_lava_heat::accessor(primitive,3);}catch(const char*){rejected=true;}assert(rejected);++checks;};
    data.count=2;rejects();data.count=3;
    data.type=cgltf_type_vec2;rejects();data.type=cgltf_type_scalar;
    data.component_type=cgltf_component_type_r_16u;rejects();data.component_type=cgltf_component_type_r_32f;
    data.normalized=true;rejects();data.normalized=false;
    attributes[1]=attributes[0];primitive.attributes_count=2;rejects();
    attributes[1].name=const_cast<char*>("_VALWOOD_PAINT");rejects();primitive.attributes_count=1;
    for(float invalid:{-.01f,1.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        samples[0]=invalid;float tagged=0;bool rejected=false;try{world_model_lava_heat::read(&data,0,tagged);}catch(const char*){rejected=true;}assert(rejected&&tagged==0);++checks;
    }
    std::printf("PASS: %u CPU lava stream layout/range/tag checks\n",checks);
}
