#include "engine/world_combat_attack_entry.h"
#include "engine/world_combat_presentation.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

int main(){
    unsigned checks=0;
    auto require=[&](bool okay){++checks;assert(okay);};
    using namespace world_combat_attack_entry;
    for(double duration:{.08,.11,.12,.18,.25,.5,1.2}){
        const double lead=leadin(duration),settle=settle_time(lead);
        require(lead>0&&lead<=.09000001&&lead<=duration*.5000001);
        require(settle>0&&settle<lead&&duration-lead>=duration*.4999999);
        require(weight(0,settle)==0&&weight(settle,settle)==1);
        require(weight(0,lead-settle)==0&&weight(lead-settle,lead-settle)==1);
        require(windup_fraction(lead,duration,lead)==0);
        require(std::abs(windup_fraction(duration,duration,lead)-1)<1e-6);
        float previous=-1;
        for(unsigned k=0;k<=1000;++k){
            const double time=duration*k/1000.;
            const float fraction=windup_fraction(time,duration,lead);
            require(std::isfinite(fraction)&&fraction>=previous&&fraction>=0&&fraction<=1);
            previous=fraction;
            if(time>=lead){
                const float phase=.36f*std::pow(fraction,1.6f);
                require(phase>=0&&phase<=.360001f);
            }
        }
        // Both sides of each presentation-only seam converge to the SAME
        // vertex pose. These use representative source normal/position values,
        // including a9cm sole correction, rather than a gameplay clock edit.
        for(float prior_phase:{0.f,.17f,.5f,.999f,1.f}){
            const float idle_start=prior_phase*(1-weight(0,settle));
            const float idle_end=prior_phase*(1-weight(settle,settle));
            require(idle_start==prior_phase&&idle_end==0);
            const unsigned old_slot=unsigned(prior_phase*3);
            const float old_weight=prior_phase*3-old_slot;
            const float captured=(old_slot+old_weight)/3;
            require(std::abs(captured-prior_phase)<1e-7);
        }
        const std::array<float,3> neutral={.23f,0.f,.575f},first={.23f,.09f,.545f};
        for(unsigned axis=0;axis<3;++axis){
            const auto sample=[&](double elapsed){const float w=weight(elapsed,lead-settle);return neutral[axis]+(first[axis]-neutral[axis])*w;};
            require(sample(0)==neutral[axis]);
            require(std::abs(sample(lead-settle)-first[axis])<1e-7);
            const double epsilon=(lead-settle)*1e-5;
            require(std::abs(sample(epsilon)-neutral[axis])<1e-6);
            require(std::abs(sample(lead-settle-epsilon)-first[axis])<1e-6);
        }
        // Authored contact is reached at the SAME original gameplay windup
        // boundary, including all actual directional bank contact-frame values.
        for(float contact:{.88f,2.4f,3.96f,5.1f,6.38f}){
            const float windup_end=.36f*std::pow(windup_fraction(duration,duration,lead),1.6f);
            const float a=world_combat_presentation::source_phase(windup_end,contact);
            const float b=world_combat_presentation::source_phase(.36f,contact);
            require(std::abs(a-b)<1e-6&&std::abs(a-contact/11.f)<1e-6);
            require(world_combat_presentation::source_phase(1.f,contact)==1.f);
        }
        // Previous guarded/combo normalization remains BIT-identical; only a
        // newly selected idle-entry leading window changes visual sampling.
        for(double old_lead:{0.,.045,.09})for(unsigned k=0;k<=1000;++k){
            const double t=duration*k/1000.;
            const double old_t=std::max(0.,t-old_lead),old_duration=std::max(.001,duration-old_lead);
            const float before=float(std::clamp(old_t/std::max(.001,old_duration),0.,1.));
            require(windup_fraction(t,duration,old_lead)==before);
        }
    }
    require(leadin(0)==0&&leadin(-1)==0);
    require(leadin(std::numeric_limits<double>::infinity())==0);
    require(leadin(std::numeric_limits<double>::quiet_NaN())==0);
    std::printf("ATTACK_ENTRY_CHECKS %u passed; original contact boundary and guarded/combo normalization preserved\n",checks);
}
