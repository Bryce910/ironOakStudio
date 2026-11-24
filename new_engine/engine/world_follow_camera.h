#ifndef VALWOOD_WORLD_FOLLOW_CAMERA_H
#define VALWOOD_WORLD_FOLLOW_CAMERA_H
#include <algorithm>
#include <cmath>
struct WorldFollowCamera {double x,y,z;float distance;};
// Keep the near-sphere capsule strictly above the physics API's 0.01 minimum:
// the nearest float representation of 0.01 lies below that double bound.
inline float world_follow_camera_capsule_half_height(float height,float radius){
    return std::max(.02f,height*.5f-radius);
}
// Allocation-free camera sphere sweep. A collision contracts immediately;
// clear space extends with exponential damping. The controller is never moved.
template<class Height,class Clear>
WorldFollowCamera world_follow_camera(double x,double y,double z,float yaw,float pitch,float previous,float dt,bool cut,Height height,Clear clear){
    const double fx=std::sin(yaw)*std::cos(pitch),fy=std::sin(pitch),fz=-std::cos(yaw)*std::cos(pitch);
    const double py=y+.10;double px=x,cy=py,pz=z;float allowed=0;
    for(unsigned i=1;i<=24;i++){
        const float d=3.6f*i/24.f;
        const double nx=x-fx*d,ny=py-fy*d+.35*d/3.6,nz=z-fz*d;
        if(height(nx,nz)+.22>ny||!clear(px,pz,nx,nz,float(std::min(cy,ny)-.18),float(std::abs(ny-cy)+.36),.22f))break;
        px=nx;cy=ny;pz=nz;allowed=d;
    }
    const float distance=cut?allowed:std::min(allowed,previous+(allowed-previous)*(1-std::exp(-10*std::clamp(dt,0.f,.1f))));
    return {x-fx*distance,py-fy*distance+.35*distance/3.6,z-fz*distance,distance};
}
#endif
