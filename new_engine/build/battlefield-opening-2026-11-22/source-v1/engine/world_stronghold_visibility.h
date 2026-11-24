#ifndef VALWOOD_WORLD_STRONGHOLD_VISIBILITY_H
#define VALWOOD_WORLD_STRONGHOLD_VISIBILITY_H
#include <cmath>
#include <limits>

// Camera-relative sphere against the actual color/reflection/light VP. A
// boundary intersection, missing matrix or invalid value keeps the actor.
inline bool world_stronghold_sphere_outside_clip(const float *vp,
        double x,double y,double z,double radius) noexcept {
    if(!vp||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
       !std::isfinite(radius)||radius<0)return false;
    for(unsigned i=0;i<16;i++)if(!std::isfinite(vp[i]))return false;
    constexpr double rounding=64.*std::numeric_limits<float>::epsilon();
    for(unsigned axis=0;axis<3;axis++)for(unsigned side=0;side<2;side++){
        const int sign=side?1:-1;
        const double a=double(vp[3])+sign*double(vp[axis]);
        const double b=double(vp[7])+sign*double(vp[4+axis]);
        const double c=double(vp[11])+sign*double(vp[8+axis]);
        const double d=double(vp[15])+sign*double(vp[12+axis]);
        const double distance=a*x+b*y+c*z+d;
        const double support=radius*std::sqrt(a*a+b*b+c*c);
        const double guard=rounding*(std::abs(a*x)+std::abs(b*y)+
            std::abs(c*z)+std::abs(d)+support+1.);
        if(!std::isfinite(distance)||!std::isfinite(support)||!std::isfinite(guard))return false;
        if(distance+support+guard<0)return true;
    }
    return false;
}
#endif
