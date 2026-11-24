#ifndef VALWOOD_WORLD_SCENERY_SHADOW_VISIBILITY_H
#define VALWOOD_WORLD_SCENERY_SHADOW_VISIBILITY_H
#include "engine/world_models.h"
#include "engine/world_transform_math.h"
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>

// CPU-only conservative light-clip test. The matrix and offsets must describe
// the same camera-relative coordinate system as the shadow draw. An enclosing
// world AABB includes the complete yaw/pitch/roll/axis-scaled source bounds;
// padding is in world metres and covers wind plus GPU float rounding. No main
// camera visibility, depth heuristic or root-distance shortcut is involved.
namespace world_scenery_shadow_visibility {
struct Bounds {std::array<double,3> center={},half={};};
inline bool transform(const float *minimum,const float *maximum,const WorldModelInstance &a,
                      double offset_x,double offset_y,double offset_z,double padding,Bounds &out){
    if(!minimum||!maximum||!std::isfinite(offset_x)||!std::isfinite(offset_y)||!std::isfinite(offset_z)||
       !std::isfinite(padding)||padding<0||!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.z)||
       !std::isfinite(a.yaw)||!std::isfinite(a.pitch)||!std::isfinite(a.roll)||!std::isfinite(a.scale)||a.scale<=0||
       !std::isfinite(a.axis_x)||!std::isfinite(a.axis_y)||!std::isfinite(a.axis_z)||a.axis_x<=-1||a.axis_y<=-1||a.axis_z<=-1)return false;
    std::array<double,3> center={},half={};
    for(unsigned k=0;k<3;++k){if(!std::isfinite(minimum[k])||!std::isfinite(maximum[k])||minimum[k]>maximum[k])return false;center[k]=(double(minimum[k])+maximum[k])*.5;half[k]=(double(maximum[k])-minimum[k])*.5;}
    constexpr double degrees=57.29577951308232;
    const auto m=vw_transform::matrix({a.x+offset_x,a.y+offset_y,a.z+offset_z,a.yaw*degrees,a.scale,a.pitch*degrees,a.roll*degrees,1.+a.axis_x,1.+a.axis_y,1.+a.axis_z});
    Bounds next;next.center=vw_transform::point(m,center);
    double magnitude=1.+std::max({std::abs(offset_x),std::abs(offset_y),std::abs(offset_z),std::abs(double(a.x)),std::abs(double(a.y)),std::abs(double(a.z))});
    for(unsigned row=0;row<3;++row){next.half[row]=padding;for(unsigned col=0;col<3;++col)next.half[row]+=std::abs(m.a[row][col])*half[col];if(!std::isfinite(next.center[row])||!std::isfinite(next.half[row]))return false;magnitude=std::max(magnitude,std::abs(next.center[row])+next.half[row]);}
    // Renderer additions and trig use float precision. This guard grows with
    // the actual relative coordinates instead of assuming a small world origin.
    const double rounding=32.*std::numeric_limits<float>::epsilon()*magnitude;
    for(auto &h:next.half)h+=rounding;
    out=next;return true;
}
inline bool visible(const float *vp,const Bounds &box){
    if(!vp)return true;
    for(unsigned k=0;k<16;++k)if(!std::isfinite(vp[k]))return true;
    for(unsigned k=0;k<3;++k)if(!std::isfinite(box.center[k])||!std::isfinite(box.half[k])||box.half[k]<0)return true;
    for(unsigned axis=0;axis<3;++axis)for(int sign:{-1,1}){
        const double x=double(vp[3])+sign*double(vp[axis]),y=double(vp[7])+sign*double(vp[4+axis]),z=double(vp[11])+sign*double(vp[8+axis]),d=double(vp[15])+sign*double(vp[12+axis]);
        const double distance=x*box.center[0]+y*box.center[1]+z*box.center[2]+d;
        const double radius=std::abs(x)*box.half[0]+std::abs(y)*box.half[1]+std::abs(z)*box.half[2];
        // Strict exclusion with a relative numerical margin. Degenerate or
        // otherwise uncertain planes always keep the original submission.
        if(!std::isfinite(distance)||!std::isfinite(radius))return true;
        if(distance+radius < -1.e-7*(1.+std::abs(distance)+radius+std::abs(d)))return false;
    }
    return true;
}
inline bool instance_visible(const float *vp,const float *minimum,const float *maximum,
                             const WorldModelInstance &a,double ox,double oy,double oz,double padding){
    Bounds box;return !transform(minimum,maximum,a,ox,oy,oz,padding,box)||visible(vp,box);
}
}
#endif
