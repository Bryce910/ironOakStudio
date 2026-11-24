#ifndef VALWOOD_WORLD_FOLLOW_CAMERA_H
#define VALWOOD_WORLD_FOLLOW_CAMERA_H
#include <algorithm>
#include <cmath>
struct WorldFollowCamera {double x,y,z;float distance;double pivot_y;float clear_distance;};
inline constexpr float world_follow_camera_max_distance=4.2f;
inline constexpr unsigned world_follow_camera_sweep_steps=28;
// Keep the near-sphere capsule strictly above the physics API's 0.01 minimum:
// the nearest float representation of 0.01 lies below that double bound.
inline float world_follow_camera_capsule_half_height(float height,float radius){
    return std::max(.02f,height*.5f-radius);
}
// Allocation-free camera sphere sweep. A collision contracts immediately;
// clear space extends with exponential damping. The controller is never moved.
template<class Height,class Clear>
WorldFollowCamera world_follow_camera(double x,double y,double z,float yaw,float pitch,float previous,float dt,bool cut,Height height,Clear clear,
                                    float requested_maximum=world_follow_camera_max_distance,float requested_lift=0,float requested_pivot_drop=0){
    const float maximum=std::isfinite(requested_maximum)?std::clamp(requested_maximum,1.f,world_follow_camera_max_distance):world_follow_camera_max_distance;
    float lift=std::isfinite(requested_lift)?std::clamp(requested_lift,0.f,.5f):0;
    const double fx=std::sin(yaw)*std::cos(pitch),fy=std::sin(pitch),fz=-std::cos(yaw)*std::cos(pitch);
    const double original_base=y+.10;
    float drop=std::isfinite(requested_pivot_drop)?std::clamp(requested_pivot_drop,0.f,.75f):0;
    // Close exploration frames the torso rather than the eye. Validate the
    // entire downward pivot corridor too, so floors/rails cannot be bypassed
    // by lowering the start of the otherwise unchanged boom sweep.
    while(drop>0&&(height(x,z)+.22>original_base-drop||
          !clear(x,z,x,z,float(original_base-drop-.18),drop+.36f,.22f)))
        drop=std::max(0.f,drop-.05f);
    const double base=original_base-drop;
    // Validate the whole lifted pivot corridor as a capsule volume before
    // sweeping the boom. A ceiling retracts this render-only rise immediately;
    // neither the player's physical capsule nor its eye position is changed.
    while(lift>0&&!clear(x,z,x,z,float(base-.18),lift+.36f,.22f))lift=std::max(0.f,lift-.05f);
    auto sweep=[&](float rise){
        const double py=base+rise;double px=x,cy=py,pz=z;float allowed=0;
        for(unsigned i=1;i<=world_follow_camera_sweep_steps;i++){
            const float d=maximum*i/float(world_follow_camera_sweep_steps);
            const double nx=x-fx*d,ny=py-fy*d+.35*d/world_follow_camera_max_distance,nz=z-fz*d;
            if(height(nx,nz)+.22>ny||!clear(px,pz,nx,nz,float(std::min(cy,ny)-.18),float(std::abs(ny-cy)+.36),.22f))break;
            px=nx;cy=ny;pz=nz;allowed=d;
        }
        return allowed;
    };
    float allowed=sweep(lift);
    // Under a roof prefer the original safe height over collapsing an otherwise
    // clear boom into the hero. At most one additional bounded sweep is needed.
    if(lift>0&&allowed<maximum-.00001f){const float unraised=sweep(0);if(unraised>allowed+.00001f){lift=0;allowed=unraised;}}
    const double py=base+lift;
    const float distance=cut?allowed:std::min(allowed,previous+(allowed-previous)*(1-std::exp(-10*std::clamp(dt,0.f,.1f))));
    return {x-fx*distance,py-fy*distance+.35*distance/world_follow_camera_max_distance,z-fz*distance,distance,py,allowed};
}
#endif
