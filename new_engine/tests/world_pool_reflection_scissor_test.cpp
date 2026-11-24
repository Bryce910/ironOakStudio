#include "engine/world_pool_reflection_scissor.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <array>

namespace {
WorldPoolReflectionScissorInput example(int width=680,int height=383) {
    WorldPoolReflectionScissorInput in;
    in.width=width;in.height=height;in.pool_bounds[0]=-26715;in.pool_bounds[1]=9615.5f;
    in.pool_bounds[2]=1.f/11;in.pool_bounds[3]=1.f/16;
    in.origin[0]=in.pool_bounds[0]-8;in.origin[1]=in.pool_bounds[1]-35;
    in.water_level=134;in.offset_y=-136;in.reflection_shift_y=4;
    in.vp[0]=.024f;in.vp[9]=.015f;in.vp[6]=.005f;in.vp[11]=.004f;in.vp[15]=1;
    return in;
}
std::uint64_t samples=0;
void reachable(const WorldPoolReflectionScissorInput &in) {
    const auto box=world_pool_reflection_scissor(in);
    assert(box.x>=0&&box.y>=0&&box.width>0&&box.height>0);
    assert(box.x+box.width<=in.width&&box.y+box.height<=in.height);
    if(!box.restricted)return;
    for(unsigned iz=0;iz<=32;iz++)for(unsigned ix=0;ix<=32;ix++) {
        const float qx=float(ix)/16-1,qz=float(iz)/16-1;
        const float x=(in.pool_bounds[0]-in.origin[0])+qx/std::max(in.pool_bounds[2],.000001f);
        const float z=(in.pool_bounds[1]-in.origin[1])+qz/std::max(in.pool_bounds[3],.000001f);
        const float y=(in.water_level+in.offset_y)+in.reflection_shift_y;
        const float p[4]={x,y,z,1};float clip[4]={};
        for(unsigned row=0;row<4;row++)for(unsigned col=0;col<4;col++)clip[row]+=in.vp[col*4+row]*p[col];
        assert(clip[3]>.001f);
        const float uv[2]={clip[0]/clip[3]*.5f+.5f,clip[1]/clip[3]*.5f+.5f};
        if(std::min(std::min(uv[0],uv[1]),std::min(1-uv[0],1-uv[1]))<=0)continue;
        for(int sx:{-1,1})for(int sy:{-1,1})for(int bx:{-1,1})for(int by:{-1,1}) {
            const float texel_x=1.f/in.width,texel_y=1.f/in.height;
            const float shifted_x=std::clamp(uv[0]+sx*.009f,texel_x,1-texel_x);
            const float shifted_y=std::clamp(uv[1]+sy*.009f,texel_y,1-texel_y);
            const float sample_x=std::clamp(shifted_x+bx*5*texel_x,texel_x,1-texel_x)*in.width-.5f;
            const float sample_y=std::clamp(shifted_y+by*5*texel_y,texel_y,1-texel_y)*in.height-.5f;
            const int x0=int(std::floor(sample_x)),x1=x0+1,y0=int(std::floor(sample_y)),y1=y0+1;
            assert(x0>=box.x&&x1<box.x+box.width&&y0>=box.y&&y1<box.y+box.height);
            samples++;
        }
    }
}
}
int main() {
    auto in=example();const auto first=world_pool_reflection_scissor(in);
    assert(first.restricted&&first.width*first.height<in.width*in.height/2);
    for(const auto size:{std::array<int,2>{680,383},{256,192},{1,1},{8192,8192}}) {
        in=example(size[0],size[1]);reachable(in);
    }
    std::uint32_t state=0x3198ad7u;
    auto random=[&](){state=state*1664525u+1013904223u;return float(state>>8)*(1.f/16777216.f);};
    for(unsigned n=0;n<96;n++) {
        in=example(128+int(random()*1024),128+int(random()*768));
        in.vp[0]=.008f+random()*.025f;in.vp[1]=(random()-.5f)*.008f;
        in.vp[8]=(random()-.5f)*.008f;in.vp[9]=.008f+random()*.01f;
        in.vp[3]=(random()-.5f)*.008f;in.vp[11]=(random()-.5f)*.008f;
        in.vp[12]=(random()-.5f)*1.5f;in.vp[13]=(random()-.5f)*1.5f;
        reachable(in);
    }
    in=example();in.vp[15]=0;assert(!world_pool_reflection_scissor(in).restricted);
    in=example();in.vp[15]=.001f;assert(!world_pool_reflection_scissor(in).restricted);
    in=example();in.vp[14]=-2;assert(!world_pool_reflection_scissor(in).restricted); // Near-plane intersection.
    in=example();in.vp[0]=in.vp[9]=2;assert(!world_pool_reflection_scissor(in).restricted); // Full rectangle.
    in=example();in.vp[12]=10;assert(!world_pool_reflection_scissor(in).restricted); // Offscreen: fail open.
    for(int index=0;index<16;index++) {
        in=example();in.vp[index]=std::numeric_limits<float>::quiet_NaN();assert(!world_pool_reflection_scissor(in).restricted);
    }
    in=example();in.pool_bounds[2]=0;assert(!world_pool_reflection_scissor(in).restricted);
    in=example();in.pool_bounds[3]=-1;assert(!world_pool_reflection_scissor(in).restricted);
    in=example();in.origin[0]=INFINITY;assert(!world_pool_reflection_scissor(in).restricted);
    in=example();in.offset_y=NAN;assert(!world_pool_reflection_scissor(in).restricted);
    in=example();in.reflection_shift_y=INFINITY;assert(!world_pool_reflection_scissor(in).restricted);
    in=example();in.water_level=NAN;assert(!world_pool_reflection_scissor(in).restricted);
    in=example();in.width=0;assert(!world_pool_reflection_scissor(in).restricted);
    in=example();in.height=8193;assert(!world_pool_reflection_scissor(in).restricted);
    std::printf("PASS pool reflection scissor CPU: %llu sampled distortion/max-blur/bilinear footprints across100 projections; exact uniform rebasing; positive-w/near-plane/numeric/input failures retain full viewport\n",
        static_cast<unsigned long long>(samples));
}
