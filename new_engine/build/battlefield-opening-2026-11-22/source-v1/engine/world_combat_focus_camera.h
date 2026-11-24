#ifndef VALWOOD_WORLD_COMBAT_FOCUS_CAMERA_H
#define VALWOOD_WORLD_COMBAT_FOCUS_CAMERA_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Render orientation only. Position/collision, logical aim and simulation clocks
// are deliberately absent from this interface. A changed focus leaves a short
// angular residual on the live goal; it never delays continuous target tracking.
struct WorldCombatFocusFrame {
    double yaw=0,pitch=0,elapsed_seconds=.12,maximum_error=0,visibility_limit=0;
    bool transitioning=false,visibility_clamped=false,reset=false,valid=true;
};
struct WorldCombatFocusStats {
    std::uint64_t transitions=0,transition_frames=0,resets=0,visibility_clamps=0,player_framing_clamps=0;
    double maximum_transition_error=0,maximum_completion_seconds=0,maximum_transition_frame_seconds=0;
};
// Bearings from the already collision-swept render camera to the player's
// boots and head. These are visibility constraints, never simulation inputs.
struct WorldCombatFocusFraming {
    struct Bearing {double yaw=0,pitch=0;};
    std::array<Bearing,2> player;
};
inline bool world_combat_player_in_frame(double yaw,double pitch,double fov,double aspect,
        const WorldCombatFocusFraming &framing,double inset=.96){
    const double sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    const double half=std::tan(fov*.5)*inset;
    for(const auto &b:framing.player){
        if(!std::isfinite(b.yaw)||!std::isfinite(b.pitch))return false;
        const double x=std::sin(b.yaw)*std::cos(b.pitch),y=std::sin(b.pitch),z=-std::cos(b.yaw)*std::cos(b.pitch);
        const double depth=x*sy*cp+y*sp-z*cy*cp;
        const double right=x*cy+z*sy,up=-x*sy*sp+y*cp+z*cy*sp;
        if(depth<=0||std::abs(right)>depth*half*aspect||std::abs(up)>depth*half)return false;
    }
    return true;
}
class WorldCombatFocusCamera {
public:
    static constexpr double duration_seconds=.12;
    static constexpr double pi=3.14159265358979323846;
    WorldCombatFocusFrame update(double goal_yaw,double goal_pitch,double dt,
                                bool focused,std::uint64_t target,unsigned mode,
                                bool cut,double fov,double aspect,
                                const WorldCombatFocusFraming *framing=nullptr){
        WorldCombatFocusFrame out;out.yaw=goal_yaw;out.pitch=goal_pitch;
        const bool finite=std::isfinite(goal_yaw)&&std::isfinite(goal_pitch)&&
            std::isfinite(dt)&&dt>=0&&std::isfinite(fov)&&fov>0&&fov<pi&&
            std::isfinite(aspect)&&aspect>0;
        if(!finite){
            out.yaw=initialized_?yaw_:0;out.pitch=initialized_?pitch_:0;
            out.valid=false;out.reset=true;initialized_=false;++stats_.resets;return out;
        }
        // A cone inscribed in the rectangular frustum guarantees the selected
        // point stays visible even during a very wide focus switch. Its 20%
        // angular inset leaves room around the point instead of grazing an edge.
        out.visibility_limit=.8*std::min(fov*.5,std::atan(std::tan(fov*.5)*aspect));
        const bool reset=!initialized_||cut||mode!=mode_||dt>.25;
        const bool changed=initialized_&&(focused!=focused_||(focused&&target!=target_));
        if(reset){elapsed_=duration_seconds;offset_yaw_=offset_pitch_=0;out.reset=true;++stats_.resets;}
        else if(changed){
            offset_yaw_=std::remainder(yaw_-goal_yaw,2*pi);offset_pitch_=pitch_-goal_pitch;
            const double magnitude=std::abs(offset_yaw_)+std::abs(offset_pitch_);
            if(focused&&magnitude>out.visibility_limit){
                const double scale=out.visibility_limit/magnitude;offset_yaw_*=scale;offset_pitch_*=scale;
                out.visibility_clamped=true;++stats_.visibility_clamps;
            }
            elapsed_=0;++stats_.transitions;
        }
        if(elapsed_<duration_seconds){
            stats_.maximum_transition_frame_seconds=std::max(stats_.maximum_transition_frame_seconds,dt);
            const double observed=elapsed_+dt;elapsed_=std::min(duration_seconds,observed);
            if(elapsed_>=duration_seconds)stats_.maximum_completion_seconds=std::max(stats_.maximum_completion_seconds,observed);
            const double t=elapsed_/duration_seconds,weight=1-t*t*(3-2*t);
            double dy=offset_yaw_*weight,dp=offset_pitch_*weight;
            const double magnitude=std::abs(dy)+std::abs(dp);
            if(focused&&magnitude>out.visibility_limit){
                const double scale=out.visibility_limit/magnitude;dy*=scale;dp*=scale;
                out.visibility_clamped=true;++stats_.visibility_clamps;
            }
            // Keeping the selected opponent inside the frustum alone does not
            // keep the player visible during a large retarget. Shorten only
            // the angular residual when it would push the boots/head out.
            // Exact settled framing and the swept camera position are retained.
            if(framing&&world_combat_player_in_frame(goal_yaw,goal_pitch,fov,aspect,*framing)&&
               !world_combat_player_in_frame(goal_yaw+dy,goal_pitch+dp,fov,aspect,*framing)){
                double safe=0,unsafe=1;
                for(unsigned i=0;i<20;++i){const double t=(safe+unsafe)*.5;
                    if(world_combat_player_in_frame(goal_yaw+dy*t,goal_pitch+dp*t,fov,aspect,*framing))safe=t;else unsafe=t;
                }
                dy*=safe;dp*=safe;++stats_.player_framing_clamps;
            }
            out.maximum_error=std::min(magnitude,focused?out.visibility_limit:magnitude);
            out.yaw=goal_yaw+dy;out.pitch=goal_pitch+dp;
            out.transitioning=elapsed_<duration_seconds;
            if(out.transitioning)++stats_.transition_frames;
            stats_.maximum_transition_error=std::max(stats_.maximum_transition_error,out.maximum_error);
        }
        out.elapsed_seconds=elapsed_;
        yaw_=out.yaw;pitch_=out.pitch;focused_=focused;target_=target;mode_=mode;initialized_=true;
        return out;
    }
    const WorldCombatFocusStats &stats()const{return stats_;}
private:
    bool initialized_=false,focused_=false;
    std::uint64_t target_=0;unsigned mode_=0;
    double yaw_=0,pitch_=0,offset_yaw_=0,offset_pitch_=0,elapsed_=duration_seconds;
    WorldCombatFocusStats stats_;
};
// Validate the rendered angles against independently reconstructed live goals.
// An expired transition receives no relaxed budget: settled aim stays exact.
inline bool world_combat_focus_frame_valid(const WorldCombatFocusFrame &frame,
        double yaw,double pitch,double goal_yaw,double goal_pitch,bool focused){
    if(!frame.valid||!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(goal_yaw)||!std::isfinite(goal_pitch))return false;
    const double dy=std::abs(std::remainder(yaw-goal_yaw,2*WorldCombatFocusCamera::pi)),dp=std::abs(pitch-goal_pitch);
    if(!frame.transitioning)return std::max(dy,dp)<.0001;
    return std::isfinite(frame.elapsed_seconds)&&frame.elapsed_seconds>=0&&
        frame.elapsed_seconds<WorldCombatFocusCamera::duration_seconds&&
        std::isfinite(frame.maximum_error)&&frame.maximum_error>=0&&dy+dp<=frame.maximum_error+.0001&&
        (!focused||(std::isfinite(frame.visibility_limit)&&frame.visibility_limit>0&&dy+dp<=frame.visibility_limit+.0001));
}
#endif
