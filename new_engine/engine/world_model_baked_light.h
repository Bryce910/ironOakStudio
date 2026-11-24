#ifndef VALWOOD_WORLD_MODEL_BAKED_LIGHT_H
#define VALWOOD_WORLD_MODEL_BAKED_LIGHT_H
#ifndef CGLTF_H_INCLUDED__
#include "cgltf.h"
#endif
#include <cmath>
#include <cstring>

namespace world_model_baked_light {
/* Optional linear diffuse irradiance/PI plus validity. No albedo, exposure or
 * gamma transform belongs here. The source showcase bakes indirect sunlight
 * and direct static fixtures; its native direct sun remains separate. */
inline const cgltf_accessor *accessor(const cgltf_primitive &primitive,cgltf_size count){
    const cgltf_accessor *result=nullptr;
    for(cgltf_size i=0;i<primitive.attributes_count;i++){
        const auto &a=primitive.attributes[i];
        if(a.name&&!std::strcmp(a.name,"_VALWOOD_BAKED_LIGHT")){
            if(result)throw "Duplicate baked light stream";
            result=a.data;
            if(!result)throw "Missing baked light accessor";
        }
    }
    if(result&&(result->count!=count||result->type!=cgltf_type_vec4||
            result->component_type!=cgltf_component_type_r_32f||result->normalized))
        throw "Invalid baked light stream layout";
    return result;
}
inline void read(const cgltf_accessor *source,cgltf_size index,float rgba[4]){
    rgba[0]=rgba[1]=rgba[2]=rgba[3]=0;
    if(!source)return;
    if(!cgltf_accessor_read_float(source,index,rgba,4))throw "Unreadable baked light sample";
    for(unsigned i=0;i<4;i++)if(!std::isfinite(rgba[i])||rgba[i]<0||rgba[i]>(i==3?1.f:65504.f))
        throw "Invalid baked light sample";
}
}
#endif
