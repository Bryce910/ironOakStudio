#include "engine/world_model_mip_math.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned long long checks=0;
void check(bool value){++checks;if(!value){std::fprintf(stderr,"FAIL mip math check %llu\n",checks);std::exit(1);}}
std::uint32_t random_bits(){static std::uint32_t state=12879123;state^=state<<13;state^=state>>17;state^=state<<5;return state;}
struct Image {unsigned w,h;std::vector<unsigned char> rgba;};
std::vector<Image> mip_chain(Image image,bool srgb,bool optimized){
    std::vector<Image> output;output.push_back(std::move(image));
    while(output.back().w>1||output.back().h>1){
        const auto &from=output.back();Image to{std::max(1u,from.w/2),std::max(1u,from.h/2),{}};to.rgba.resize(std::size_t(to.w)*to.h*4);
        for(unsigned y=0;y<to.h;++y)for(unsigned x=0;x<to.w;++x){
            const unsigned x0=x*from.w/to.w,x1=(x+1)*from.w/to.w,y0=y*from.h/to.h,y1=(y+1)*from.h/to.h;
            for(unsigned c=0;c<4;++c){
                float sum=0;unsigned count=0;
                for(unsigned sy=y0;sy<y1;++sy)for(unsigned sx=x0;sx<x1;++sx){
                    const auto value=from.rgba[(std::size_t(sy)*from.w+sx)*4+c];
                    sum+=(srgb&&c<3)?(optimized?world_model_mip_math::linear(value):world_model_mip_math::reference_linear(value)):value/255.f;++count;
                }
                sum/=count;
                to.rgba[(std::size_t(y)*to.w+x)*4+c]=(srgb&&c<3)?(optimized?world_model_mip_math::encoded(sum):world_model_mip_math::reference_encoded(sum)):static_cast<unsigned char>(std::lround(sum*255));
            }
        }
        output.push_back(std::move(to));
    }
    return output;
}
}
int main(){
    using namespace world_model_mip_math;
    for(unsigned i=0;i<256;++i)check(bits_of(linear(static_cast<unsigned char>(i)))==bits_of(reference_linear(static_cast<unsigned char>(i))));
    const auto &threshold=tables().encoded_threshold;
    for(unsigned i=0;i<threshold.size();++i){
        check(!i||threshold[i]>threshold[i-1]);
        for(int delta=-16;delta<=16;++delta){const auto bits=std::uint32_t(std::int64_t(threshold[i])+delta);const auto value=from_bits(bits);check(encoded(value)==reference_encoded(value));}
        check(encoded(from_bits(threshold[i]-1))==i&&encoded(from_bits(threshold[i]))==i+1);
    }
    for(unsigned i=0;i<tables().encoded_bucket.size();++i){
        for(std::uint32_t bits:{i<<16,std::min(0x3f800000u,(i<<16)+65535u)})check(encoded(from_bits(bits))==reference_encoded(from_bits(bits)));
    }
    for(unsigned i=0;i<1000000;++i){const auto value=from_bits(random_bits()%0x3f800001u);check(encoded(value)==reference_encoded(value));}
    for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){const float value=(reference_linear(a)+reference_linear(b))*.5f;check(encoded(value)==reference_encoded(value));}
    for(float value:{0.f,-0.f,-1.f,1.f,2.f,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})check(encoded(value)==reference_encoded(value));
    for(const auto size:std::array<std::array<unsigned,2>,7>{{{{1,1}},{{1,17}},{{31,1}},{{17,31}},{{64,64}},{{129,257}},{{511,255}}}}){
        for(unsigned pattern=0;pattern<4;++pattern)for(bool srgb:{false,true}){
            Image image{size[0],size[1],{}};image.rgba.resize(std::size_t(image.w)*image.h*4);
            for(std::size_t i=0;i<image.rgba.size();++i)image.rgba[i]=pattern==0?0:pattern==1?255:pattern==2?static_cast<unsigned char>(i*37):static_cast<unsigned char>(random_bits());
            const auto original=mip_chain(image,srgb,false),optimized=mip_chain(image,srgb,true);check(original.size()==optimized.size());
            for(std::size_t i=0;i<original.size();++i){check(original[i].w==optimized[i].w&&original[i].h==optimized[i].h);check(original[i].rgba==optimized[i].rgba);}
        }
    }
    std::printf("Model mip math: %llu checks passed; original float32 transfer/mip bytes preserved\n",checks);
}
