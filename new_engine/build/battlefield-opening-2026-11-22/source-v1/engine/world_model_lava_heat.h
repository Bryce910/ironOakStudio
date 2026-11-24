#ifndef VALWOOD_WORLD_MODEL_LAVA_HEAT_H
#define VALWOOD_WORLD_MODEL_LAVA_HEAT_H
#ifndef CGLTF_H_INCLUDED__
#include "cgltf.h"
#endif
#include <cmath>
#include <cstring>

namespace world_model_lava_heat {
// Lava and architectural paint are mutually exclusive uses of the existing
// scalar stream. A negative tag keeps the static vertex ABI and GPU size fixed.
inline const cgltf_accessor *accessor(const cgltf_primitive &primitive,cgltf_size count){
    const cgltf_accessor *result=nullptr;bool paint=false;
    for(cgltf_size i=0;i<primitive.attributes_count;i++){
        const auto &a=primitive.attributes[i];
        if(a.name&&!std::strcmp(a.name,"_VALWOOD_PAINT"))paint=true;
        if(a.name&&!std::strcmp(a.name,"_VALWOOD_LAVA_HEAT")){
            if(result)throw "Duplicate lava heat stream";
            result=a.data;if(!result)throw "Missing lava heat accessor";
        }
    }
    if(result&&(paint||result->count!=count||result->type!=cgltf_type_scalar||
            result->component_type!=cgltf_component_type_r_32f||result->normalized))
        throw "Invalid or ambiguous lava heat stream layout";
    return result;
}
inline void read(const cgltf_accessor *source,cgltf_size index,float &paint_or_heat){
    if(!source)return;
    float heat=0;
    if(!cgltf_accessor_read_float(source,index,&heat,1)||!std::isfinite(heat)||heat<0||heat>1)
        throw "Invalid lava heat sample";
    paint_or_heat=-1.f-heat;
}
}
#endif
