#ifndef WORLD_COMBAT_FEEDBACK_H
#define WORLD_COMBAT_FEEDBACK_H
#include "world_training_combat.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Presentation only. Damage, reach, guard success and hit-stop stay owned by
// WorldTrainingCombat. In particular, a whiff cannot request impact effects.
namespace world_combat_feedback {
enum class Kind {None,Miss,Body,Incoming,Block,Blocked,Parry};
struct Preferences {bool vibration=true,reduced_motion=false;};
struct Cue {
    Kind kind=Kind::None;
    std::uint64_t serial=0;
    float damage=0,duration=0,remaining=0;
    unsigned opponent=8;
    std::array<unsigned,3> color={};
    const char *caption="",*hint="";
    bool impact=false,metal=false;
    int sound=-1,volume=0;
    unsigned sound_fade_ms=0;
    float low=0,high=0,camera=0,yaw_sign=1;
    float rumble_tail_low=0,rumble_tail_high=0;
    unsigned rumble_ms=0,rumble_attack_ms=0,priority=0;
};
struct Stats {
    std::array<std::uint64_t,7> outcomes={};
    std::uint64_t swings=0,duplicates=0,paused_suppressed=0,resets=0;
    std::uint64_t haptic_commands=0,haptic_stops=0,haptic_saturated=0;
};
inline bool local_air(TrainingContactKind kind){return kind==TrainingContactKind::PlayerSwing||kind==TrainingContactKind::PlayerMiss;}
inline void merge_rumble(Cue &merged,const Cue &cue){
    merged.low=std::max(merged.low,cue.low);merged.high=std::max(merged.high,cue.high);
    merged.rumble_ms=std::max(merged.rumble_ms,cue.rumble_ms);merged.priority=std::max(merged.priority,cue.priority);
}
inline Cue classify(const TrainingContactEvent &event,Preferences preferences={}) {
    Cue cue;cue.serial=event.serial;cue.damage=std::isfinite(event.damage)?std::max(0.f,event.damage):0;
    cue.opponent=event.opponent_id;cue.yaw_sign=event.direction==TrainingDirection::Right?-1.f:1.f;
    switch(event.kind){
    case TrainingContactKind::PlayerSwing:cue.sound=0;cue.volume=56;return cue;
    case TrainingContactKind::EnemySwing:cue.sound=0;cue.volume=36;return cue;
    case TrainingContactKind::PlayerMiss:
        cue.kind=Kind::Miss;cue.caption="MISSED";cue.hint="Adjust range or aim";cue.color={181,191,197};cue.duration=.30f;cue.priority=1;break;
    case TrainingContactKind::PlayerBody:
        cue.kind=Kind::Body;cue.caption="HIT";cue.color={255,114,94};cue.duration=.42f;cue.priority=4;
        cue.sound=1;cue.volume=88;cue.low=.30f;cue.high=.48f;cue.rumble_ms=75;cue.camera=.006f;cue.impact=true;break;
    case TrainingContactKind::EnemyBody:
        cue.kind=Kind::Incoming;cue.caption="HIT TAKEN";cue.color={255,101,92};cue.duration=.48f;cue.priority=6;
        cue.sound=1;cue.volume=94;cue.low=.70f;cue.high=.36f;cue.rumble_ms=140;cue.camera=.013f;cue.impact=true;break;
    case TrainingContactKind::EnemyBlock:
        cue.kind=Kind::Blocked;cue.caption="BLOCKED";cue.hint="Enemy weapon stopped your swing";cue.color={255,209,112};cue.duration=.46f;cue.priority=3;
        cue.sound=2;cue.volume=94;cue.sound_fade_ms=180;cue.low=.46f;cue.high=.68f;cue.rumble_ms=110;cue.rumble_attack_ms=24;cue.rumble_tail_low=.20f;cue.rumble_tail_high=.16f;cue.camera=.010f;cue.impact=cue.metal=true;break;
    case TrainingContactKind::PlayerBlock:
        cue.kind=Kind::Block;cue.caption="BLOCK";cue.hint="Your guard held";cue.color={142,220,245};cue.duration=.40f;cue.priority=3;
        cue.sound=2;cue.volume=90;cue.sound_fade_ms=140;cue.low=.45f;cue.high=.34f;cue.rumble_ms=120;cue.rumble_attack_ms=24;cue.rumble_tail_low=.22f;cue.rumble_tail_high=.10f;cue.camera=.007f;cue.impact=cue.metal=true;
        if(event.guard_broken){cue.caption="GUARD BROKEN";cue.hint="Recover your guard";cue.color={255,169,112};cue.duration=.46f;cue.priority=4;cue.damage=0;}
        break;
    case TrainingContactKind::PlayerParry:
        cue.kind=Kind::Parry;cue.caption="PARRY";cue.hint="Strike back";cue.color={255,242,193};cue.duration=.50f;cue.priority=5;
        cue.sound=2;cue.volume=108;cue.sound_fade_ms=300;cue.low=.26f;cue.high=.90f;cue.rumble_ms=100;cue.rumble_attack_ms=18;cue.rumble_tail_low=.12f;cue.rumble_tail_high=.24f;cue.camera=.013f;cue.impact=cue.metal=true;break;
    default:return cue;
    }
    cue.remaining=cue.duration;
    if(!preferences.vibration){cue.low=cue.high=cue.rumble_tail_low=cue.rumble_tail_high=0;cue.rumble_ms=cue.rumble_attack_ms=0;}
    if(preferences.reduced_motion)cue.camera=0;
    return cue;
}
class System {
    struct HapticPulse {Cue cue;float age=0;bool active=false;};
    Cue attack_={},defense_={};Stats stats_={};
    std::array<HapticPulse,8> haptics_={};
    std::uint64_t haptic_revision_=0,haptic_published_revision_=0;
    float haptic_published_low_=0,haptic_published_high_=0;
    std::uint64_t serial_=0;
    double clock_=-1;
    float camera_age_=1,camera_strength_=0,camera_yaw_=1;
    unsigned frame_priority_=0,attack_priority_=0,defense_priority_=0;
    bool paused_=false;
public:
    void reset(double clock=-1){attack_=defense_={};haptics_={};++haptic_revision_;serial_=0;clock_=clock;camera_age_=1;camera_strength_=0;frame_priority_=attack_priority_=defense_priority_=0;paused_=false;++stats_.resets;}
    void advance(double dt,double clock,bool paused){
        if(clock_>=0&&std::isfinite(clock)&&clock+1e-6<clock_)reset(clock);
        if(std::isfinite(clock))clock_=clock;
        paused_=paused;frame_priority_=attack_priority_=defense_priority_=0;
        if(paused){attack_=defense_={};haptics_={};camera_strength_=0;return;}
        const float step=std::isfinite(dt)?float(std::clamp(dt,0.,.25)):0;
        for(auto *cue:{&attack_,&defense_}){cue->remaining=std::max(0.f,cue->remaining-step);if(cue->remaining<=0)*cue={};}camera_age_+=step;
        for(auto &pulse:haptics_)if(pulse.active){pulse.age+=step;if(pulse.age>=pulse.cue.rumble_ms*.001f)pulse.active=false;}
    }
    bool observe(const TrainingContactEvent &event,Preferences preferences,Cue &pulse){
        pulse={};
        if(!event.serial)return false;
        if(event.serial<=serial_){++stats_.duplicates;return false;}
        serial_=event.serial;
        if(paused_){++stats_.paused_suppressed;return false;}
        pulse=classify(event,preferences);
        if(pulse.kind==Kind::None){if(pulse.sound>=0){++stats_.swings;return true;}return false;}
        ++stats_.outcomes[unsigned(pulse.kind)];
        if(pulse.impact&&pulse.rumble_ms){
            auto at=std::find_if(haptics_.begin(),haptics_.end(),[](const HapticPulse &p){return !p.active;});
            if(at==haptics_.end()){
                // Keep bounded storage and the longest remaining real impact.
                // A crowd cannot allocate voices or erase a stronger pulse.
                at=std::min_element(haptics_.begin(),haptics_.end(),[](const HapticPulse &a,const HapticPulse &b){return a.cue.rumble_ms*.001f-a.age<b.cue.rumble_ms*.001f-b.age;});
                const bool attack=!at->cue.rumble_attack_ms||at->age<at->cue.rumble_attack_ms*.001f;
                const float low=attack?at->cue.low:at->cue.rumble_tail_low,high=attack?at->cue.high:at->cue.rumble_tail_high;
                pulse.low=std::max(pulse.low,low);pulse.high=std::max(pulse.high,high);
                pulse.rumble_tail_low=std::max(pulse.rumble_tail_low,low);pulse.rumble_tail_high=std::max(pulse.rumble_tail_high,high);
                pulse.rumble_ms=std::max(pulse.rumble_ms,unsigned(std::ceil(at->cue.rumble_ms-at->age*1000)));
                ++stats_.haptic_saturated;
            }
            *at={pulse,0,true};++haptic_revision_;
        }
        const bool outgoing=pulse.kind==Kind::Miss||pulse.kind==Kind::Body||pulse.kind==Kind::Blocked;
        auto &notice=outgoing?attack_:defense_;auto &priority=outgoing?attack_priority_:defense_priority_;
        if(pulse.priority>=priority){notice=pulse;priority=pulse.priority;}
        if(pulse.impact&&pulse.priority>=frame_priority_){frame_priority_=pulse.priority;camera_age_=0;camera_strength_=pulse.camera;camera_yaw_=pulse.yaw_sign;}
        return true;
    }
    // Call once after this frame's real events. Send only new/changed motor
    // levels, so a later weak event cannot replace a stronger active impact.
    // Zero duration is an explicit stop, including setting changes and reset.
    bool take_haptic_command(Preferences preferences,Cue &command){
        command={};
        if(paused_||!preferences.vibration)haptics_={};
        float low=0,high=0,remaining=0;
        for(const auto &pulse:haptics_)if(pulse.active){
            const auto &cue=pulse.cue;const bool attack=!cue.rumble_attack_ms||pulse.age<cue.rumble_attack_ms*.001f;
            low=std::max(low,attack?cue.low:cue.rumble_tail_low);high=std::max(high,attack?cue.high:cue.rumble_tail_high);
            remaining=std::max(remaining,cue.rumble_ms*.001f-pulse.age);
        }
        const bool changed=low!=haptic_published_low_||high!=haptic_published_high_||haptic_revision_!=haptic_published_revision_;
        if(!changed)return false;
        haptic_published_low_=low;haptic_published_high_=high;haptic_published_revision_=haptic_revision_;
        command.low=low;command.high=high;
        if(low>0||high>0)command.rumble_ms=unsigned(std::max(1.f,std::ceil(remaining*1000-.0001f)));
        ++stats_.haptic_commands;if(!command.rumble_ms)++stats_.haptic_stops;
        return true;
    }
    const Cue &attack_notice()const{return attack_;}
    const Cue &defense_notice()const{return defense_;}
    const Stats &stats()const{return stats_;}
    float camera_impulse()const{return paused_?0:camera_strength_*std::exp(-camera_age_*30)*std::cos(camera_age_*70);}
    float camera_yaw()const{return camera_impulse()*.24f*camera_yaw_;}
};
}
#endif
