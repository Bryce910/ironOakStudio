#ifndef VALWOOD_WORLD_POOL_REFLECTION_VISIBILITY_H
#define VALWOOD_WORLD_POOL_REFLECTION_VISIBILITY_H
#include <algorithm>
#include <cmath>
#include <limits>

// Exact float uniforms passed to the MAIN world_water.vs.glsl draw. In
// particular, vp already contains the current temporal projection jitter.
struct WorldPoolMainClipInput {
    float vp[16]={}; // Column-major uVP; default OpenGL -w..w clip volume.
    float pool_bounds[4]={}; // uPoolBounds: world centerXZ, inverse radiiXZ.
    float origin[2]={}; // uOrigin.
    float offset_y=0,water_level=0; // uOffset.y and uWaterLevel.
};

// Pure bounded CPU/no allocation/no GL. True means the complete conservative
// pool rectangle is strictly outside ONE common homogeneous clip plane, so
// neither triangle can produce a main-view water fragment. No perspective
// divide or approximate frustum sphere is used. False includes every uncertain,
// invalid or intersecting case; it does not assert that the pool is visible.
inline bool world_pool_main_quad_outside_clip(const WorldPoolMainClipInput &in) noexcept {
    for(float v:in.vp)if(!std::isfinite(v)||std::abs(v)>1e7f)return false;
    for(float v:in.pool_bounds)if(!std::isfinite(v)||std::abs(v)>1e7f)return false;
    for(float v:in.origin)if(!std::isfinite(v)||std::abs(v)>1e7f)return false;
    if(!std::isfinite(in.offset_y)||!std::isfinite(in.water_level)||
       std::abs(in.offset_y)>1e7f||std::abs(in.water_level)>1e7f||
       in.pool_bounds[2]<=0||in.pool_bounds[3]<=0)return false;
    // Cover uniform subtraction/division, vector additions, projection
    // multiply/add reassociation/FMA and the final clip-plane sum. The +1
    // terms keep a nonzero absolute guard near cancellation/underflow.
    constexpr double rounding=32.*std::numeric_limits<float>::epsilon();
    const double cx=double(in.pool_bounds[0])-double(in.origin[0]);
    const double cz=double(in.pool_bounds[1])-double(in.origin[1]);
    const double rx=1./double(std::max(in.pool_bounds[2],.000001f));
    const double rz=1./double(std::max(in.pool_bounds[3],.000001f));
    const double y=double(in.water_level)+double(in.offset_y);
    double upper[6]={};
    for(unsigned p=0;p<6;p++)upper[p]=-std::numeric_limits<double>::infinity();
    for(int zi=-1;zi<=1;zi+=2)for(int xi=-1;xi<=1;xi+=2) {
        const double x=cx+xi*rx,z=cz+zi*rz;
        const double point[4]={x,y,z,1.};
        const double error[4]={
            rounding*(std::abs(double(in.pool_bounds[0]))+std::abs(double(in.origin[0]))+rx+std::abs(x)+1.),
            rounding*(std::abs(double(in.water_level))+std::abs(double(in.offset_y))+std::abs(y)+1.),
            rounding*(std::abs(double(in.pool_bounds[1]))+std::abs(double(in.origin[1]))+rz+std::abs(z)+1.),0.};
        double clip[4]={},clip_error[4]={};
        for(unsigned row=0;row<4;row++) {
            double magnitude=0;
            for(unsigned c=0;c<4;c++) {
                const double a=in.vp[c*4+row];
                clip[row]+=a*point[c];magnitude+=std::abs(a*point[c]);
                clip_error[row]+=std::abs(a)*error[c];
            }
            clip_error[row]+=rounding*(magnitude+1.);
            if(!std::isfinite(clip[row])||!std::isfinite(clip_error[row]))return false;
        }
        for(unsigned axis=0;axis<3;axis++)for(unsigned side=0;side<2;side++) {
            const double sign=side?-1.:1.;
            const double guard=clip_error[3]+clip_error[axis]+
                rounding*(std::abs(clip[3])+std::abs(clip[axis])+1.);
            const double bound=clip[3]+sign*clip[axis]+guard;
            if(!std::isfinite(bound))return false;
            upper[axis*2+side]=std::max(upper[axis*2+side],bound);
        }
    }
    for(double bound:upper)if(bound<0.)return true;
    return false;
}
#endif
