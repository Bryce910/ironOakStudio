#ifndef VALWOOD_WORLD_PLAYER_MOVEMENT_H
#define VALWOOD_WORLD_PLAYER_MOVEMENT_H

#include <algorithm>
#include <cmath>

// Horizontal walking only. The caller owns collision sweeps, vertical motion,
// dodge displacement and mode/teleport detection. Reset on blocked controls,
// fly/swim changes or externally relocated positions; commit the actual swept
// displacement immediately after each step so blocked velocity cannot build up.
namespace world_player_movement {
struct Delta { double dx=0, dz=0; };
constexpr double acceleration_tau=.035; // 95% of requested speed in 105 ms.
constexpr double stopping_tau=.021;
constexpr double reversal_tau=.024;
constexpr double stop_snap_speed=.01;
constexpr double maximum_dt=.05;
constexpr double safety_speed_limit=1024.;

// Preserve radial analog magnitude; clamp combined keyboard/pad diagonals once.
// forward is positive ahead, side positive right, yaw 0 points along -Z.
inline Delta target(double forward,double side,double yaw,double maximum_speed) {
    if(!std::isfinite(forward)||!std::isfinite(side)||!std::isfinite(yaw)||
       !std::isfinite(maximum_speed)||maximum_speed<=0)return {};
    double length=std::hypot(forward,side);
    if(!std::isfinite(length))return {};
    if(length>1){forward/=length;side/=length;}
    maximum_speed=std::min(maximum_speed,safety_speed_limit);
    const double s=std::sin(yaw),c=std::cos(yaw);
    return {(s*forward+c*side)*maximum_speed,(-c*forward+s*side)*maximum_speed};
}

struct State {
    double vx=0,vz=0;
    void reset(){vx=vz=0;}

    Delta step(double target_vx,double target_vz,double dt,bool enabled=true) {
        if(!enabled){reset();return {};}
        if(!std::isfinite(dt)||dt<0||!std::isfinite(target_vx)||!std::isfinite(target_vz)||
           !std::isfinite(vx)||!std::isfinite(vz)||std::hypot(vx,vz)>safety_speed_limit+1.e-8){
            reset();return {};
        }
        if(dt==0)return {};
        dt=std::min(dt,maximum_dt);
        const double desired_speed=std::hypot(target_vx,target_vz);
        if(!std::isfinite(desired_speed)){reset();return {};}
        if(desired_speed>safety_speed_limit){
            target_vx*=safety_speed_limit/desired_speed;
            target_vz*=safety_speed_limit/desired_speed;
        }
        Delta out;
        auto integrate=[&](double time,double tau){
            // Analytic displacement and velocity for dv/dt=(target-v)/tau.
            // expm1 retains precision at tiny dt; new_velocity*dt would drift
            // with frame rate, especially through acceleration and release.
            const double k=-std::expm1(-time/tau);
            out.dx+=target_vx*time+(vx-target_vx)*tau*k;
            out.dz+=target_vz*time+(vz-target_vz)*tau*k;
            vx+=(target_vx-vx)*k;vz+=(target_vz-vz)*k;
        };
        if(target_vx==0&&target_vz==0){
            const double speed=std::hypot(vx,vz);
            if(speed<=stop_snap_speed){reset();return {};}
            const double snap_time=stopping_tau*std::log(speed/stop_snap_speed);
            integrate(std::min(dt,snap_time),stopping_tau);
            if(dt>=snap_time)reset();
            return out;
        }
        const double speed=std::hypot(target_vx,target_vz);
        const double toward=vx*(target_vx/speed)+vz*(target_vz/speed);
        if(toward<0){
            // Faster braking while travelling against the request. Split at
            // its exact zero crossing, not a per-frame branch: partitions at
            // 30/60/120 Hz then follow the same reversal and displacement.
            const double crossing=reversal_tau*std::log1p(-toward/speed);
            const double braking=std::min(dt,crossing);
            integrate(braking,reversal_tau);
            dt-=braking;
        }
        if(dt>0)integrate(dt,acceleration_tau);
        return out;
    }

    void commit(double requested_dx,double requested_dz,double actual_dx,double actual_dz) {
        if(!std::isfinite(requested_dx)||!std::isfinite(requested_dz)||
           !std::isfinite(actual_dx)||!std::isfinite(actual_dz)){reset();return;}
        auto component=[](double requested,double actual,double &velocity){
            // Existing sweeps slide independently on X/Z. Keep the accepted
            // tangent component, remove only its blocked component, and never
            // acquire velocity from NPC separation, bounds correction or push.
            if(std::abs(requested)<=1.e-12)return;
            if(requested*actual<=0){velocity=0;return;}
            velocity*=std::min(1.,std::abs(actual/requested));
        };
        component(requested_dx,actual_dx,vx);
        component(requested_dz,actual_dz,vz);
    }
};
}
#endif
