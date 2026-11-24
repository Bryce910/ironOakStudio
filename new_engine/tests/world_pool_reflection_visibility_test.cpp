#include "engine/world_pool_reflection_visibility.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>

namespace {
using Point=std::array<double,4>;
using Quad=std::array<Point,4>;
constexpr unsigned triangle[2][3]={{0,1,3},{0,3,2}};
double plane(const Point &p,unsigned n){return p[3]+((n&1)?-p[n/2]:p[n/2]);}
bool intersects(const Quad &q){
    for(const auto &indices:triangle){
        std::array<Point,16> a={},b={};unsigned count=3;
        for(unsigned k=0;k<3;k++)a[k]=q[indices[k]];
        for(unsigned side=0;side<6&&count;side++){
            unsigned next=0;
            for(unsigned k=0;k<count;k++){
                const Point &p=a[k],&r=a[(k+1)%count];const double dp=plane(p,side),dr=plane(r,side);
                if(dp>=0){assert(next<b.size());b[next++]=p;}
                if((dp<0)!=(dr<0)){
                    const double t=dp/(dp-dr);Point cut={};for(unsigned c=0;c<4;c++)cut[c]=p[c]+(r[c]-p[c])*t;
                    assert(next<b.size());b[next++]=cut;
                }
            }
            a=b;count=next;
        }
        if(count>=3)return true;
    }
    return false;
}
// Reproduce the actual six-vertex shader's float arithmetic, with several
// legal projection multiply/add association/FMA choices. The tested helper
// instead derives a guarded double interval from the supplied uniforms.
Quad gpu_quad(const WorldPoolMainClipInput &in,unsigned mode){
    Quad out={};unsigned k=0;
    for(int zi=-1;zi<=1;zi+=2)for(int xi=-1;xi<=1;xi+=2){
        const float x=(in.pool_bounds[0]-in.origin[0])+float(xi)/std::max(in.pool_bounds[2],.000001f);
        const float z=(in.pool_bounds[1]-in.origin[1])+float(zi)/std::max(in.pool_bounds[3],.000001f);
        const float point[4]={x,in.water_level+in.offset_y,z,1};
        for(unsigned row=0;row<4;row++){
            float value=0;
            if(mode==0){for(unsigned c=0;c<4;c++)value+=in.vp[c*4+row]*point[c];}
            else if(mode==1)value=(in.vp[row]*point[0]+in.vp[4+row]*point[1])+(in.vp[8+row]*point[2]+in.vp[12+row]);
            else if(mode==2){for(unsigned c=0;c<4;c++)value=std::fma(in.vp[c*4+row],point[c],value);}
            else {for(int c=3;c>=0;c--)value=std::fma(in.vp[c*4+row],point[c],value);}
            out[k][row]=value;
        }
        k++;
    }
    return out;
}
bool common_outside(const Quad &q){
    for(unsigned p=0;p<6;p++){bool all=true;for(const auto &v:q)all=all&&plane(v,p)<0;if(all)return true;}
    return false;
}
WorldPoolMainClipInput orthographic(){
    WorldPoolMainClipInput p;p.vp[0]=p.vp[9]=p.vp[6]=p.vp[15]=1; // XZ water plane maps to screenXY.
    p.pool_bounds[2]=p.pool_bounds[3]=4;return p;
}
std::uint32_t rng=0x4d2a1b3;
float random(float low,float high){rng=rng*1664525u+1013904223u;return low+(high-low)*float(rng>>8)/16777216.f;}
void perspective(WorldPoolMainClipInput &p,float yaw,float pitch,float aspect,float jx,float jy){
    const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    const float right[3]={cy,0,sy},up[3]={-sy*sp,cp,cy*sp},forward[3]={sy*cp,sp,-cy*cp};
    const float near=.4f,far=15000.f,f=1/std::tan(.54f),a=(far+near)/(near-far),b=2*far*near/(near-far);
    for(unsigned c=0;c<3;c++){
        p.vp[c*4]=right[c]*f/aspect-forward[c]*jx;
        p.vp[c*4+1]=up[c]*f-forward[c]*jy;
        p.vp[c*4+2]=-a*forward[c];p.vp[c*4+3]=forward[c];
    }
    p.vp[12]=p.vp[13]=p.vp[15]=0;p.vp[14]=b;
}
unsigned checks=0,rejections=0;
void proof(const WorldPoolMainClipInput &p){
    const bool rejected=world_pool_main_quad_outside_clip(p);checks++;
    if(rejected){
        rejections++;
        for(unsigned mode=0;mode<4;mode++){
            const auto q=gpu_quad(p,mode);assert(common_outside(q));assert(!intersects(q));
        }
    }
}
}
int main(){
    auto p=orthographic();assert(!world_pool_main_quad_outside_clip(p));assert(intersects(gpu_quad(p,0)));
    for(unsigned axis=0;axis<3;axis++)for(float sign:{-1.f,1.f}){
        auto outside=p;
        if(axis==0)outside.pool_bounds[0]=sign*2;
        else if(axis==1)outside.water_level=sign*2;
        else outside.pool_bounds[1]=sign*2;
        assert(world_pool_main_quad_outside_clip(outside));proof(outside);
    }
    p.pool_bounds[0]=1.25f;assert(!world_pool_main_quad_outside_clip(p)); // Exact contact.
    p.pool_bounds[0]=std::nextafter(1.25f,INFINITY);assert(!world_pool_main_quad_outside_clip(p)); // Uncertain fewULP.
    p=orthographic();p.pool_bounds[2]=p.pool_bounds[3]=.25f;
    const auto wide=gpu_quad(p,0);assert(intersects(wide)&&!world_pool_main_quad_outside_clip(p));
    // Negative control: every corner can be individually outside while the
    // rectangle covers the entire image. "Any outside corner" is unsound.
    for(const auto &v:wide){bool outside=false;for(unsigned side=0;side<6;side++)outside=outside||plane(v,side)<0;assert(outside);}
    for(unsigned kind=0;kind<8;kind++){
        auto invalid=orthographic();
        if(kind==0)invalid.vp[0]=NAN;
        if(kind==1)invalid.vp[15]=INFINITY;
        if(kind==2)invalid.pool_bounds[0]=NAN;
        if(kind==3)invalid.origin[0]=INFINITY;
        if(kind==4)invalid.pool_bounds[2]=0;
        if(kind==5)invalid.pool_bounds[3]=-1;
        if(kind==6)invalid.offset_y=NAN;
        if(kind==7)invalid.vp[4]=1e8f;
        assert(!world_pool_main_quad_outside_clip(invalid));
    }
    WorldPoolMainClipInput zero;zero.pool_bounds[2]=zero.pool_bounds[3]=1;assert(!world_pool_main_quad_outside_clip(zero));
    // Perspective/yaw/pitch/jitter, near-plane crossings, rebased world values
    // and actual10x16m water envelope. Deterministic; no project/global state.
    for(unsigned k=0;k<12000;k++){
        WorldPoolMainClipInput in;
        in.pool_bounds[0]=random(-50000,50000);in.pool_bounds[1]=random(-50000,50000);
        in.pool_bounds[2]=1.f/(k%7==0?random(.1f,80.f):10.f);
        in.pool_bounds[3]=1.f/(k%7==0?random(.1f,100.f):16.f);
        const float distance=k%5==0?random(0,.8f):random(1,240);
        const float around=random(-3.14159f,3.14159f);
        in.origin[0]=in.pool_bounds[0]+std::sin(around)*distance;
        in.origin[1]=in.pool_bounds[1]+std::cos(around)*distance;
        in.water_level=134;in.offset_y=-random(130,190);
        perspective(in,random(-3.14159f,3.14159f),random(-1.5f,1.5f),random(.55f,2.5f),random(-.002f,.002f),random(-.003f,.003f));
        proof(in);
    }
    // Rebase cancellation and clip-boundary adversaries: exact uniform values
    // are authoritative; close boundaries must remain open rather than infer
    // certainty from separately reconstructed world doubles.
    for(unsigned k=0;k<3000;k++){
        auto in=orthographic();in.pool_bounds[0]=random(-100000,100000);in.pool_bounds[1]=random(-100000,100000);
        in.origin[0]=in.pool_bounds[0]-random(-2,2);in.origin[1]=in.pool_bounds[1]-random(-2,2);
        in.water_level=random(-1000,1000);in.offset_y=-in.water_level+random(-2,2);
        proof(in);
    }
    assert(rejections>5000&&rejections<checks);
    std::printf("PASS pool reflection visibility: %u deterministic cases,%u strict common-plane rejections,4 float/FMA projection references and independent homogeneous triangle clipping; boundary/near-plane/rebase/invalid/covering-quad controls PASS\n",checks,rejections);
}
