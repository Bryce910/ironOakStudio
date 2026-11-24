#ifndef VALWOOD_WORLD_COMBAT_STICK_H
#define VALWOOD_WORLD_COMBAT_STICK_H

#include "world_controller.h"
#include "world_training_combat.h"

namespace world_controller {

struct SwordStickResult {
    bool select_direction=false;
    bool attack=false;
    TrainingDirection direction=TrainingDirection::High;
};

// Consume the controller's deadzone-adjusted right stick once per frame.
// A neutral return separates intentional sword gestures from a held stick.
class SwordStick {
    bool armed_=false;
    bool was_guarding_=false;

    static TrainingDirection direction(Stick value,bool guarding){
        // Horizontal wins exact diagonal ties, independent of frame history.
        if(std::abs(value.x)>=std::abs(value.y))
            return value.x<0?TrainingDirection::Left:TrainingDirection::Right;
        // The sword's High animation is a downward overhead cut and Low is
        // an upward rising cut. Guard names describe where the sword is held.
        if(guarding)return value.y<0?TrainingDirection::High:TrainingDirection::Low;
        return value.y<0?TrainingDirection::Low:TrainingDirection::High;
    }
public:
    static constexpr float neutral_radius=.25f;
    static constexpr float gesture_radius=.5f;

    void reset(){armed_=false;was_guarding_=false;}

    SwordStickResult sample(Stick value,bool enabled,bool guarding,bool can_swing=true){
        SwordStickResult result;
        if(!enabled||!std::isfinite(value.x)||!std::isfinite(value.y)){
            reset();
            return result;
        }
        value.x=std::clamp(value.x,-1.f,1.f);
        value.y=std::clamp(value.y,-1.f,1.f);
        const float radius=std::hypot(value.x,value.y);
        const bool neutral=radius<=neutral_radius;
        const bool released_guard=was_guarding_&&!guarding;
        was_guarding_=guarding;

        if(neutral){
            armed_=true;
            if(guarding){
                result.select_direction=true;
                result.direction=TrainingDirection::Thrust;
            }
            return result;
        }
        if(guarding){
            // Guard changes need no neutral return, but can never become an
            // attack when LT is released with the stick still deflected.
            armed_=false;
            if(radius>=gesture_radius){
                result.select_direction=true;
                result.direction=direction(value,true);
            }
            return result;
        }
        if(released_guard||!can_swing){
            // Consume motion during countdowns, recovery gates, and LT release.
            // It must not be replayed as an attack when the gate later opens.
            armed_=false;
            return result;
        }
        if(armed_&&radius>=gesture_radius){
            armed_=false;
            result.select_direction=true;
            result.attack=true;
            result.direction=direction(value,false);
        }
        return result;
    }
};

}
#endif
