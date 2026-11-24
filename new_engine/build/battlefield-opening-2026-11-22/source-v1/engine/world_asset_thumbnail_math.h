#ifndef VALWOOD_WORLD_ASSET_THUMBNAIL_MATH_H
#define VALWOOD_WORLD_ASSET_THUMBNAIL_MATH_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string_view>
#include <vector>
namespace world_asset_thumbnail {
constexpr unsigned tile_side=192,tile_limit=96,record_limit=128;
constexpr unsigned texture_side=256;
constexpr std::size_t gpu_job_limit=64u*1024u*1024u,upload_budget=1024u*1024u;
struct AlphaReference {
    std::array<std::uint64_t,256> histogram{};
    std::uint64_t pixels=0,sum=0;
};
inline AlphaReference alpha_reference(const std::vector<std::uint8_t> &rgba){
    AlphaReference ref;
    if(rgba.empty()||rgba.size()%4)return ref;
    ref.pixels=rgba.size()/4;
    for(std::size_t i=3;i<rgba.size();i+=4){++ref.histogram[rgba[i]];ref.sum+=rgba[i];}
    return ref;
}
inline double alpha_coverage(const AlphaReference &ref,float cutoff,float factor){
    if(!ref.pixels||!std::isfinite(cutoff)||!std::isfinite(factor))return 0;
    std::uint64_t covered=0;
    for(unsigned i=0;i<256;++i)if(float(i)/255.f*factor>=cutoff)covered+=ref.histogram[i];
    return double(covered)/double(ref.pixels);
}
inline std::uint32_t alpha_rank(std::uint32_t value){
    value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;return value;
}
// Restore the original source alpha distribution in thumbnail-only mips.
// Matching its CDF preserves every real material cutoff*factor, including a
// texture shared by differently masked meshes, within one representable texel.
// Tiny levels cannot represent that binary area. Preserve fractional mean
// coverage there for the shader's stable spatial minification sampling.
inline bool preserve_alpha(std::vector<std::uint8_t> &rgba,const AlphaReference &ref,std::uint32_t salt=0){
    if(!ref.pixels||rgba.empty()||rgba.size()%4||rgba.size()/4>texture_side*texture_side)return false;
    const std::size_t count=rgba.size()/4;
    if(count<16){
        const auto wanted=std::uint64_t(std::llround(double(ref.sum)*double(count)/double(ref.pixels)));
        std::uint64_t before=0;for(std::size_t i=3;i<rgba.size();i+=4)before+=rgba[i];
        std::uint64_t after=0;
        for(std::size_t i=3;i<rgba.size();i+=4){rgba[i]=std::uint8_t(std::clamp<long>(std::lround(before?double(rgba[i])*wanted/before:double(wanted)/count),0,255));after+=rgba[i];}
        // Quantization residual is bounded by the tiny pixel count. Never
        // change RGB, texture dimensions, material factors or authored data.
        while(after!=wanted)for(std::size_t i=3;i<rgba.size()&&after!=wanted;i+=4){
            if(after<wanted&&rgba[i]<255){++rgba[i];++after;}
            else if(after>wanted&&rgba[i]>0){--rgba[i];--after;}
        }
        return true;
    }
    std::vector<std::uint32_t> order(count);std::iota(order.begin(),order.end(),0u);
    std::sort(order.begin(),order.end(),[&](auto a,auto b){
        const auto x=rgba[std::size_t(a)*4+3],y=rgba[std::size_t(b)*4+3];
        if(x!=y)return x>y;
        const auto ra=alpha_rank(a^salt),rb=alpha_rank(b^salt);return ra!=rb?ra<rb:a<b;
    });
    unsigned value=255;std::uint64_t cumulative=ref.histogram[value];
    for(std::size_t rank=0;rank<count;++rank){
        const std::uint64_t source_rank=((std::uint64_t(rank)*2+1)*ref.pixels)/(count*2);
        while(value&&source_rank>=cumulative)cumulative+=ref.histogram[--value];
        rgba[std::size_t(order[rank])*4+3]=std::uint8_t(value);
    }
    return true;
}

// Preview-only alpha-aware color reduction. Keep original alpha unchanged
// until coverage correction and leave original full-resolution RGB untouched.
inline bool alpha_weighted_mip(const std::vector<std::uint8_t> &from,unsigned sw,unsigned sh,
                               std::vector<std::uint8_t> &to,unsigned dw,unsigned dh,bool srgb){
    if(!sw||!sh||!dw||!dh||dw>sw||dh>sh||from.size()!=std::size_t(sw)*sh*4||to.size()!=std::size_t(dw)*dh*4)return false;
    static const auto linear=[](){std::array<double,256> a{};for(unsigned i=0;i<256;++i){double x=double(i)/255.;a[i]=x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);}return a;}();
    for(unsigned y=0;y<dh;++y)for(unsigned x=0;x<dw;++x){
        const unsigned x0=x*sw/dw,x1=(x+1)*sw/dw,y0=y*sh/dh,y1=(y+1)*sh/dh;
        double alpha=0,color[3]={};
        for(unsigned sy=y0;sy<y1;++sy)for(unsigned sx=x0;sx<x1;++sx){
            const auto at=(std::size_t(sy)*sw+sx)*4;const double a=from[at+3];alpha+=a;
            for(unsigned c=0;c<3;++c)color[c]+=(srgb?linear[from[at+c]]:double(from[at+c])/255.)*a;
        }
        const auto at=(std::size_t(y)*dw+x)*4;
        for(unsigned c=0;c<3;++c){double v=alpha>0?color[c]/alpha:0;
            if(srgb)v=v<=.0031308?v*12.92:1.055*std::pow(v,1./2.4)-.055;
            to[at+c]=std::uint8_t(std::clamp<long>(std::lround(v*255),0,255));}
    }
    return true;
}

struct Fit {std::array<float,3> center{};float radius=1,distance=4,near_plane=.01f,far_plane=10;};
inline bool fit(const float *minimum,const float *maximum,Fit &out){
    if(!minimum||!maximum)return false;
    double diagonal=0;
    for(unsigned i=0;i<3;++i){
        if(!std::isfinite(minimum[i])||!std::isfinite(maximum[i])||maximum[i]<minimum[i])return false;
        const double extent=double(maximum[i])-minimum[i];
        diagonal+=extent*extent;out.center[i]=float((double(minimum[i])+maximum[i])*.5);
    }
    if(!std::isfinite(diagonal)||diagonal>1e14)return false;
    out.radius=float(std::max(.01,std::sqrt(diagonal)*.5));
    // A sphere fits every orientation, including flat objects and tall trees.
    // 32-degree square lens with a 12% border leaves every corner visible.
    out.distance=out.radius/std::sin(16.f*.017453292519943295f)*1.12f;
    out.near_plane=std::max(.001f,out.distance-out.radius*1.5f);
    out.far_plane=out.distance+out.radius*1.5f;return true;
}
inline bool safe_stem(std::string_view s){
    if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;
    std::size_t part=0;
    for(std::size_t i=0;i<=s.size();++i){
        if(i==s.size()||s[i]=='/'){
            if(i==part||s.substr(part,i-part)=="."||s.substr(part,i-part)=="..")return false;
            part=i+1;continue;
        }
        const unsigned char c=s[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;
    }return true;
}
}
#endif
