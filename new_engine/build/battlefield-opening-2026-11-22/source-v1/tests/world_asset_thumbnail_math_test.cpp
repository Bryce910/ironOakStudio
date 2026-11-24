#include "engine/world_asset_thumbnail_math.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
namespace {
unsigned checks=0;
void check(bool yes,const char *why){++checks;if(!yes){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
struct V {double x,y,z;};
V subtract(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V unit(V a){const double n=std::sqrt(dot(a,a));return {a.x/n,a.y/n,a.z/n};}
void project_box(const std::array<float,3> &lo,const std::array<float,3> &hi){
    world_asset_thumbnail::Fit f;check(world_asset_thumbnail::fit(lo.data(),hi.data(),f),"finite source box accepted");
    const V c{f.center[0],f.center[1],f.center[2]},direction=unit({.68,.38,.78}),eye{c.x+direction.x*f.distance,c.y+direction.y*f.distance,c.z+direction.z*f.distance};
    const V forward=unit(subtract(c,eye)),right=unit(cross(forward,{0,1,0})),up=cross(right,forward);
    const double lens=std::tan(16.*.017453292519943295);
    for(unsigned k=0;k<8;++k){
        const V p{double(k&1?hi[0]:lo[0]),double(k&2?hi[1]:lo[1]),double(k&4?hi[2]:lo[2])},relative=subtract(p,eye);
        const double z=dot(relative,forward),x=dot(relative,right)/(z*lens),y=dot(relative,up)/(z*lens);
        check(z>f.near_plane&&z<f.far_plane,"all source corners survive near/far clipping");
        check(std::abs(x)<1&&std::abs(y)<1,"all source corners fit square tile projection");
    }
}
}
int main(){
    using namespace world_asset_thumbnail;
    // Transparent-black source padding must not darken minified source RGB.
    // Alpha remains unchanged here; the independent coverage pass follows it.
    std::vector<std::uint8_t> color_source={255,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0},color_mip={0,0,0,64};
    const auto color_original=color_source;
    check(alpha_weighted_mip(color_source,2,2,color_mip,1,1,true),"alpha-aware source RGB reduction succeeds");
    check(color_mip==std::vector<std::uint8_t>({255,0,0,64}),"transparent black cannot darken source red or alter reduced alpha");
    check(color_source==color_original,"full-resolution photo bytes remain unchanged");
    for(unsigned alpha=0;alpha<256;++alpha){
        std::vector<std::uint8_t> photo={123,45,222,std::uint8_t(alpha),123,45,222,255},mip={0,0,0,37};
        check(alpha_weighted_mip(photo,2,1,mip,1,1,true),"source opacity range has valid RGB reduction");
        check(mip==std::vector<std::uint8_t>({123,45,222,37}),"constant source photo color survives every alpha ratio without changing output alpha");
    }
    color_source.assign(5*3*4,0);
    for(unsigned pixel=0;pixel<15;++pixel){color_source[pixel*4]=255;color_source[pixel*4+1]=17;color_source[pixel*4+2]=83;}
    color_source.back()=255;color_mip={0,0,0,17};
    check(alpha_weighted_mip(color_source,5,3,color_mip,1,1,true),"odd source dimensions reduce successfully");
    check(color_mip==std::vector<std::uint8_t>({255,17,83,17}),"the final odd source row and column contribute their real opaque color");
    color_source.back()=0;
    check(alpha_weighted_mip(color_source,5,3,color_mip,1,1,true),"fully transparent source reduction succeeds");
    check(color_mip==std::vector<std::uint8_t>({0,0,0,17}),"transparent pixels cannot invent source color or change alpha");
    check(!alpha_weighted_mip(color_source,0,3,color_mip,1,1,true),"zero source dimensions rejected");
    check(!alpha_weighted_mip(color_source,5,3,color_mip,6,1,true),"unbounded enlarged RGB mip rejected");
    check(!alpha_weighted_mip(color_source,5,3,color_mip,2,1,true),"RGB target buffer dimensions must match");
    // Fine source needles occupy3/16 of an atlas. Plain area mips average to
    // alpha48 and disappear at the real .38/.5 cutout. The thumbnail operation
    // must retain their area for each material's actual cutoff and alpha factor.
    std::vector<std::uint8_t> source(256*256*4);
    for(unsigned i=0;i<256*256;++i){source[i*4]=std::uint8_t(i%251);source[i*4+1]=77;source[i*4+2]=19;source[i*4+3]=i%16<3?255:0;}
    const auto needle=alpha_reference(source);
    check(std::abs(alpha_coverage(needle,.38f,1)-3./16)<1e-12,"fine source needle coverage measured at its real cutoff");
    const auto original=source;
    for(unsigned side:{256u,128u,64u,32u,16u,8u,4u}){
        std::vector<std::uint8_t> mip(std::size_t(side)*side*4);
        for(std::size_t i=0;i<mip.size();i+=4){mip[i]=37;mip[i+1]=84;mip[i+2]=12;mip[i+3]=48;}
        check(alpha_coverage(alpha_reference(mip),.38f,1)==0,"ordinary minified alpha loses fine-needle hard coverage");
        check(preserve_alpha(mip,needle,987),"masked thumbnail mip coverage rebuilt");
        const auto measured=alpha_reference(mip);
        for(auto material:std::array<std::array<float,2>,5>{{{{.38f,1}},{{.5f,1}},{{.6f,.8f}},{{.8f,.7f}},{{.001f,1}}}})
            check(std::abs(alpha_coverage(measured,material[0],material[1])-alpha_coverage(needle,material[0],material[1]))<=1./(side*side),"coverage retained for the real cutoff and material factor");
        for(std::size_t i=0;i<mip.size();i+=4)check(mip[i]==37&&mip[i+1]==84&&mip[i+2]==12,"coverage preparation preserves every RGB source channel");
        auto deterministic=mip;check(preserve_alpha(deterministic,needle,987)&&deterministic==mip,"stable spatial ordering is deterministic and idempotent");
    }
    check(source==original,"full-resolution source data remains unchanged");
    for(unsigned pixels:{1u,3u,4u,8u,15u}){
        std::vector<std::uint8_t> tiny(pixels*4,0);
        for(unsigned i=0;i<pixels;++i){tiny[i*4]=91;tiny[i*4+3]=48;}
        check(preserve_alpha(tiny,needle,123),"tiny mip keeps fractional area instead of forcing an opaque quad");
        const auto measured=alpha_reference(tiny);
        check(std::abs(double(measured.sum)/measured.pixels-double(needle.sum)/needle.pixels)<=.5/double(pixels)+1e-9,"tiny minification alpha mean conserves source area to one byte quantization");
        check(measured.sum>0&&measured.sum<255*measured.pixels,"subpixel needle coverage remains fractional");
    }
    std::vector<std::uint8_t> soft(4096*4,0);
    for(unsigned i=0;i<4096;++i)soft[i*4+3]=std::uint8_t((i*37)%256);
    const auto soft_ref=alpha_reference(soft);
    std::vector<std::uint8_t> reduced(64*4,96);
    check(preserve_alpha(reduced,soft_ref,3),"soft alpha histogram transferred to a retained mip");
    for(float cutoff:{.1f,.38f,.5f,.7f,.95f})for(float factor:{.3f,.7f,1.f})
        check(std::abs(alpha_coverage(alpha_reference(reduced),cutoff,factor)-alpha_coverage(soft_ref,cutoff,factor))<=1./64,"shared image retains coverage for several independent mesh cutoffs/factors");
    std::vector<std::uint8_t> transparent(256*4,0),target(16*4,255);
    check(preserve_alpha(target,alpha_reference(transparent))&&alpha_reference(target).sum==0,"fully transparent images cannot gain fake foliage");
    check(!preserve_alpha(target,AlphaReference{}),"missing source alpha distribution rejected");
    project_box({-.5f,0,-.5f},{.5f,1,.5f});
    project_box({-50,0,-.01f},{50,.01f,.01f});
    project_box({-.05f,0,-.05f},{.05f,170,.05f});
    project_box({-90,-5,-90},{90,-5,90});
    project_box({0,0,0},{0,0,0});
    std::mt19937 rng(987121);std::uniform_real_distribution<float> extent(.001f,900.f),origin(-2000.f,2000.f);
    for(unsigned i=0;i<1000;++i){std::array<float,3> lo,hi;for(unsigned j=0;j<3;++j){lo[j]=origin(rng);hi[j]=lo[j]+extent(rng);}project_box(lo,hi);}
    Fit f;const float lo[]={0,0,0},bad[]={-1,1,1},nan[]={std::numeric_limits<float>::quiet_NaN(),1,1},inf[]={1,std::numeric_limits<float>::infinity(),1};
    check(!fit(lo,bad,f),"inverted source bounds rejected");check(!fit(lo,nan,f),"NaN source bounds rejected");check(!fit(lo,inf,f),"infinite source bounds rejected");check(!fit(nullptr,lo,f),"missing source bounds rejected");
    for(const auto stem:{"castle_house_01","imports/revision-abcd/asset","grandstone/door-frame.01","Rock-Moss_2"})check(safe_stem(stem),"legal immutable model stem accepted");
    for(const auto stem:{"","../asset","x/../asset","/models/asset","x//asset","x/./asset","x/","C:/asset","x\\asset","asset\n","asset;touch"})check(!safe_stem(stem),"unsafe model source path rejected");
    check(std::size_t(tile_limit)*tile_side*tile_side*4==14155776,"retained thumbnail tile budget is13.5 MiB");
    check(upload_budget<=1024u*1024u&&gpu_job_limit<=64u*1024u*1024u,"per-frame upload and active model budgets bounded");
    std::printf("Asset thumbnail fit/path contracts: %u checks passed\n",checks);
}
