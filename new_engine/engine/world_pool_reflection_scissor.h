#ifndef VALWOOD_WORLD_POOL_REFLECTION_SCISSOR_H
#define VALWOOD_WORLD_POOL_REFLECTION_SCISSOR_H
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

// Pass the exact float uniforms used by world_water.vs/fs.glsl, not separately
// reconstructed double positions. This bounds every texel the complete pool
// rectangle can sample, including distortion, the five-texel blur and bilinear
// filtering. The visible ellipse is a subset of that rectangle.
struct WorldPoolReflectionScissorInput {
    float vp[16]={}; // Column-major uReflectionVP.
    float pool_bounds[4]={}; // uPoolBounds: centerXZ, inverse radiiXZ.
    float origin[2]={}; // uOrigin.
    float offset_y=0,water_level=0,reflection_shift_y=0;
    int width=0,height=0; // Existing reflection texture size; never resized here.
};
struct WorldPoolReflectionScissor {
    int x=0,y=0,width=0,height=0;
    bool restricted=false; // False means use the entire existing viewport.
};

// Pure CPU/no allocation/no GL. Apply only AFTER the full reflection sky, to
// opaque/masked geometry. The caller must restore scissor state before returning
// to the main view. Near-plane intersections and all uncertainty fail open.
inline WorldPoolReflectionScissor world_pool_reflection_scissor(const WorldPoolReflectionScissorInput &in) noexcept {
    const WorldPoolReflectionScissor full={0,0,std::max(in.width,0),std::max(in.height,0),false};
    if(in.width<1||in.height<1||in.width>8192||in.height>8192)return full;
    for(float value:in.vp)if(!std::isfinite(value)||std::abs(value)>1e7f)return full;
    for(float value:in.pool_bounds)if(!std::isfinite(value)||std::abs(value)>1e7f)return full;
    for(float value:in.origin)if(!std::isfinite(value)||std::abs(value)>1e7f)return full;
    for(float value:{in.offset_y,in.water_level,in.reflection_shift_y})
        if(!std::isfinite(value)||std::abs(value)>1e7f)return full;
    if(in.pool_bounds[2]<=0||in.pool_bounds[3]<=0)return full;
    // Carry a conservative float-operation/interpolation envelope through the
    // homogeneous projection. This covers reciprocal/FMA differences, rather
    // than assuming a fixed pixel margin bounds an arbitrarily small w.
    constexpr double rounding=32.*std::numeric_limits<float>::epsilon();
    const double center_x=double(in.pool_bounds[0])-in.origin[0];
    const double center_z=double(in.pool_bounds[1])-in.origin[1];
    const double radius_x=1./std::max(in.pool_bounds[2],.000001f);
    const double radius_z=1./std::max(in.pool_bounds[3],.000001f);
    const float gpu_center_x=in.pool_bounds[0]-in.origin[0],gpu_center_z=in.pool_bounds[1]-in.origin[1];
    const float gpu_radius_x=1.f/std::max(in.pool_bounds[2],.000001f);
    const float gpu_radius_z=1.f/std::max(in.pool_bounds[3],.000001f);
    const float gpu_y=(in.water_level+in.offset_y)+in.reflection_shift_y;
    const double error_y=rounding*(std::abs(double(in.water_level))+std::abs(double(in.offset_y))+
        std::abs(double(in.reflection_shift_y))+std::abs(double(gpu_y))+1.);
    double lo_x=std::numeric_limits<double>::infinity(),lo_y=lo_x,hi_x=-lo_x,hi_y=-lo_x;
    for(int z_sign:{-1,1})for(int x_sign:{-1,1}) {
        const float x=gpu_center_x+float(x_sign)*gpu_radius_x,z=gpu_center_z+float(z_sign)*gpu_radius_z;
        const double point[4]={x,gpu_y,z,1.};
        const double error[4]={rounding*(std::abs(center_x)+radius_x+std::abs(double(x))+1.),error_y,
            rounding*(std::abs(center_z)+radius_z+std::abs(double(z))+1.),0.};
        double clip[4]={},clip_error[4]={};
        for(unsigned row=0;row<4;row++) {
            double magnitude=0;
            for(unsigned column=0;column<4;column++) {
                const double matrix=in.vp[column*4+row];
                clip[row]+=matrix*point[column];magnitude+=std::abs(matrix*point[column]);
                clip_error[row]+=std::abs(matrix)*error[column];
            }
            clip_error[row]+=rounding*(magnitude+1.);
            if(!std::isfinite(clip[row])||!std::isfinite(clip_error[row]))return full;
        }
        const double min_w=clip[3]-clip_error[3],max_w=clip[3]+clip_error[3];
        if(min_w<=.001||clip[2]+clip[3]-clip_error[2]-clip_error[3]<=0.)return full;
        for(unsigned axis=0;axis<2;axis++) {
            double low=std::numeric_limits<double>::infinity(),high=-low;
            for(int sign:{-1,1})for(double divisor:{min_w,max_w}) {
                const double projected=(clip[axis]+sign*clip_error[axis])/divisor*.5+.5;
                if(!std::isfinite(projected))return full;
                low=std::min(low,projected);high=std::max(high,projected);
            }
            if(!axis){lo_x=std::min(lo_x,low);hi_x=std::max(hi_x,high);}
            else{lo_y=std::min(lo_y,low);hi_y=std::max(hi_y,high);}
        }
    }
    // Shader distortion clamps each axis to .009 UV. The existing filter is
    // <=5 texels; three more cover bilinear footprint, raster rounding and a
    // conservative extra texel. Neither kernel nor reflection quality changes.
    const double pad_x=std::ceil(.009*in.width)+8.,pad_y=std::ceil(.009*in.height)+8.;
    const int x0=int(std::floor(std::clamp(lo_x*in.width-pad_x,0.,double(in.width))));
    const int y0=int(std::floor(std::clamp(lo_y*in.height-pad_y,0.,double(in.height))));
    const int x1=int(std::ceil(std::clamp(hi_x*in.width+pad_x,0.,double(in.width))));
    const int y1=int(std::ceil(std::clamp(hi_y*in.height+pad_y,0.,double(in.height))));
    if(x1<=x0||y1<=y0||(x0==0&&y0==0&&x1==in.width&&y1==in.height))return full;
    return {x0,y0,x1-x0,y1-y0,true};
}
#endif
