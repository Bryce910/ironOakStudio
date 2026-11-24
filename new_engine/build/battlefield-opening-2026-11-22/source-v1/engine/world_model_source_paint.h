#ifndef VALWOOD_WORLD_MODEL_SOURCE_PAINT_H
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
