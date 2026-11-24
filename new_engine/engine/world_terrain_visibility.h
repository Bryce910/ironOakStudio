#ifndef VALWOOD_WORLD_TERRAIN_VISIBILITY_H
#define VALWOOD_WORLD_TERRAIN_VISIBILITY_H
#include <cmath>
#include <initializer_list>
// Exact six camera-relative clip half-spaces; includes all terrain vertices
// and skirts via the tile's generated min/max. A4m guard covers float upload
// rounding and projection jitter. Invalid bounds fail open.
inline bool world_terrain_box_visible(const float *vp,double x,double y,double z,double hx,double hy,double hz) {
    if(!vp||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
       !std::isfinite(hx)||!std::isfinite(hy)||!std::isfinite(hz)||hx<0||hy<0||hz<0)return true;
    for(int i=0;i<16;i++)if(!std::isfinite(vp[i]))return true;
    hx+=4.;hy+=4.;hz+=4.;
    for(int axis=0;axis<3;axis++)for(int sign:{-1,1}) {
        const double a=double(vp[3])+sign*double(vp[axis]);
        const double b=double(vp[7])+sign*double(vp[4+axis]);
        const double c=double(vp[11])+sign*double(vp[8+axis]);
        const double d=double(vp[15])+sign*double(vp[12+axis]);
        if(a*x+b*y+c*z+d+std::abs(a)*hx+std::abs(b)*hy+std::abs(c)*hz<0)return false;
    }
    return true;
}
#endif
