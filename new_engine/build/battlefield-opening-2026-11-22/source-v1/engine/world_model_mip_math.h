#ifndef VALWOOD_WORLD_MODEL_MIP_MATH_H
#define VALWOOD_WORLD_MODEL_MIP_MATH_H

// Preserve the existing float32 sRGB mip recipe while avoiding per-texel pow.
// The output table stores the first positive float32 bit pattern producing
// each encoded byte under this process's original recipe/libm. There is no
// interpolation or approximation: every interval has the original byte result.
// Tables retain no images, cache keys, renderer objects or mutable asset data.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace world_model_mip_math {
static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559,
              "Exact mip lookup requires IEEE754 float32");

inline float reference_linear(unsigned char value){
    const float x=value/255.f;
    return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);
}
inline unsigned char reference_encoded(float value){
    const float x=value<=.0031308f?12.92f*value:1.055f*std::pow(value,1.f/2.4f)-.055f;
    return static_cast<unsigned char>(std::lround(std::max(0.f,std::min(1.f,x))*255));
}
inline float from_bits(std::uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
inline std::uint32_t bits_of(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}

struct Tables {
    std::array<float,256> linear;
    std::array<std::uint32_t,255> encoded_threshold;
    // A float-bit bucket supplies a conservative starting byte. Exact
    // threshold comparisons finish the lookup; this is still not interpolation.
    std::array<unsigned char,0x3f81> encoded_bucket;
    Tables(){
        for(unsigned i=0;i<linear.size();++i)linear[i]=reference_linear(static_cast<unsigned char>(i));
        // Nonnegative IEEE754 bit order matches numeric order. Find transitions
        // with the actual old expression, including its float rounding/lround.
        for(unsigned byte=1;byte<=encoded_threshold.size();++byte){
            std::uint32_t low=0,high=0x3f800000u;
            while(low<high){
                const auto middle=low+(high-low)/2;
                if(reference_encoded(from_bits(middle))>=byte)high=middle;
                else low=middle+1;
            }
            encoded_threshold[byte-1]=low;
        }
        unsigned byte=0;
        for(unsigned i=0;i<encoded_bucket.size();++i){
            while(byte<encoded_threshold.size()&&encoded_threshold[byte]<=(i<<16))++byte;
            encoded_bucket[i]=static_cast<unsigned char>(byte);
        }
    }
};
inline const Tables &tables(){static const Tables value;return value;}
inline float linear(unsigned char value){return tables().linear[value];}
inline unsigned char encoded(const Tables &lookup,float value){
    if(value==0)return 0; // Treat negative zero like the original expression.
    if(!(value>0&&value<=1))return reference_encoded(value);
    const auto bits=bits_of(value);
    unsigned byte=lookup.encoded_bucket[bits>>16];
    while(byte<lookup.encoded_threshold.size()&&bits>=lookup.encoded_threshold[byte])++byte;
    return static_cast<unsigned char>(byte);
}
inline unsigned char encoded(float value){return encoded(tables(),value);}
}
#endif
