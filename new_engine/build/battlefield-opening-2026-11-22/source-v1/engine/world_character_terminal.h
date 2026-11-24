#ifndef VALWOOD_CANDIDATE_CHARACTER_TERMINAL_H
#define VALWOOD_CANDIDATE_CHARACTER_TERMINAL_H
#include "engine/world_training_combat.h"
#include <algorithm>
#include <cmath>

// Presentation only. A terminal body hit can make gameplay Won/Ready before
// this frame's renderer sees the contact. Keep that exact captured contact,
// then finish the committed swing without ticking combat or applying damage.
namespace world_character_terminal {
struct Contact {
    std::uint64_t attack_serial=0;
    TrainingDirection direction=TrainingDirection::High;
    double windup_seconds=0;
    double strike_seconds=WorldTrainingCombat::player_strike_seconds;
    double recovery_seconds=WorldTrainingCombat::player_recovery_seconds;
    double combo_link_seconds=WorldTrainingCombat::combo_link_seconds;
    double canonical_phase=.36;
    float yaw=0;
    bool eligible=false;
    world_weapon_loadout::Weapon weapon=world_weapon_loadout::Weapon::Sword;
};
class Continuation {
public:
    // Call before any gameplay combat tick/callback for the frame. Active or
    // genuinely paused continuation survives; a query from an old frame does not.
    void begin_frame(){pending_valid_=false;}
    void reset(){active_=finished_=pending_valid_=deferred_valid_=false;time_=previous_impact_=0;phase_=TrainingPlayerPhase::Ready;}
    bool record(const Contact &c){
        // Current anatomical body queries sweep [.26,.36] before the contact
        // key. Keeping the first presentation in Windup also supplies the
        // recorded physical windup to a not-yet-rendered attack's adapter latch.
        if(!c.eligible||!melee(c.weapon)||!c.attack_serial||unsigned(c.direction)>4||!std::isfinite(c.yaw)||
           !duration(c.windup_seconds)||!duration(c.strike_seconds)||!duration(c.recovery_seconds)||!duration(c.combo_link_seconds)||
           !std::isfinite(c.canonical_phase)||c.canonical_phase<0||c.canonical_phase>.36+1e-9)return false;
        pending_=c;pending_.canonical_phase=std::clamp(c.canonical_phase,0.,.36);pending_valid_=true;return true;
    }
    bool record(const TrainingAnatomicalQuery &q,const TrainingAnatomicalHit &hit,float yaw,bool eligible){
        if(!melee(q.weapon)||!hit.valid||hit.weapon_contact||hit.part==TrainingBodyPart::None)return false;
        Contact c;c.attack_serial=q.attack_serial;c.direction=q.direction;c.windup_seconds=q.windup_seconds;
        c.strike_seconds=q.strike_seconds;c.recovery_seconds=q.recovery_seconds;c.combo_link_seconds=q.combo_link_seconds;
        c.canonical_phase=q.phase_end;c.yaw=yaw;c.eligible=eligible;c.weapon=q.weapon;return record(c);
    }
    // enabled is the shared loaded/view/weapon/support/context eligibility.
    // body_hit_increased must compare authoritative hits across THIS frame's
    // gameplay ticks, not a previous cached query or an enemy-defeat counter.
    // A genuine pause wins even over a newly published terminal contact: retain
    // it deferred, and commit its exact contact only after play resumes.
    bool update(const WorldTrainingCombat &game,double dt,bool paused,bool enabled,bool body_hit_increased){
        if(!std::isfinite(dt)||dt<0||!std::isfinite(game.impact_pause)||game.impact_pause<0||!enabled||
           !melee(game.weapon)||game.health<=0||
           game.state!=TrainingState::Won||game.engaged()) {reset();return false;}
        if((active_&&(contact_.attack_serial!=game.attack_start_serial||contact_.weapon!=game.weapon))||
           (deferred_valid_&&(deferred_.attack_serial!=game.attack_start_serial||deferred_.weapon!=game.weapon))){reset();return false;}
        const bool fresh=pending_valid_&&body_hit_increased&&pending_.attack_serial==game.attack_start_serial&&pending_.weapon==game.weapon;
        if(!active_&&fresh){
            if(paused){deferred_=pending_;deferred_valid_=true;}
            else {latch(pending_,game.impact_pause);return true;}
        }
        if(paused)return active_;
        if(!active_&&deferred_valid_){const Contact c=deferred_;deferred_valid_=false;latch(c,game.impact_pause);return true;}
        if(!active_)return false;
        // Display the exact recovery endpoint once before releasing to the
        // adapter's normal entry/exit bridge. Paused endpoint frames still hold.
        if(finished_){active_=false;finished_=false;return false;}
        const double bounded=std::min(dt,.1);
        double step=std::max(0.,bounded-std::min(bounded,previous_impact_));
        if(game.impact_pause>0)step=0;
        previous_impact_=game.impact_pause;
        if(step>0)advance(step);
        return active_;
    }
    // Modify an existing VISUAL COPY only. All state, body/weapon counters,
    // impact pause, difficulty, damage, stamina and gameplay clock stay copied
    // from the authoritative game; only committed player presentation changes.
    bool apply(WorldTrainingCombat &visual)const{
        if(!active_||visual.weapon!=contact_.weapon)return false;
        visual.attack_start_serial=contact_.attack_serial;visual.attack_direction=contact_.direction;
        visual.player_phase=phase_;visual.player_time=time_;visual.player_duration=phase_duration();
        visual.player_attack_timing={contact_.windup_seconds,contact_.strike_seconds,contact_.recovery_seconds,contact_.combo_link_seconds};
        visual.guard=false;visual.pending_attack=false;return true;
    }
    bool active()const{return active_;}
    bool deferred()const{return deferred_valid_;}
    float yaw()const{return contact_.yaw;}
    std::uint64_t attack_serial()const{return active_?contact_.attack_serial:0;}
    double canonical_phase()const{
        if(!active_)return 0;
        const double f=std::clamp(time_/phase_duration(),0.,1.);
        return phase_==TrainingPlayerPhase::Windup?.36*f:phase_==TrainingPlayerPhase::Strike?.36+.24*f:.60+.40*f;
    }
private:
    static bool melee(world_weapon_loadout::Weapon weapon){
        using W=world_weapon_loadout::Weapon;
        return weapon==W::Sword||weapon==W::SwordShield||weapon==W::GreatAxe||weapon==W::Staff;
    }
    static bool duration(double value){return std::isfinite(value)&&value>0&&value<=5;}
    void latch(const Contact &c,double impact){contact_=c;active_=true;finished_=false;deferred_valid_=false;
        phase_=TrainingPlayerPhase::Windup;time_=c.windup_seconds*c.canonical_phase/.36;previous_impact_=impact;}
    double phase_duration()const{return phase_==TrainingPlayerPhase::Windup?contact_.windup_seconds:phase_==TrainingPlayerPhase::Strike?contact_.strike_seconds:contact_.recovery_seconds;}
    void advance(double step){
        for(unsigned n=0;n<3&&step>0;++n){const double left=std::max(0.,phase_duration()-time_);
            if(step<left-1e-12){time_+=step;return;}
            step=std::max(0.,step-left);time_=phase_duration();
            if(phase_==TrainingPlayerPhase::Recovery){finished_=true;return;}
            phase_=phase_==TrainingPlayerPhase::Windup?TrainingPlayerPhase::Strike:TrainingPlayerPhase::Recovery;time_=0;
        }
    }
    Contact pending_,contact_,deferred_;TrainingPlayerPhase phase_=TrainingPlayerPhase::Ready;
    double time_=0,previous_impact_=0;
    bool pending_valid_=false,active_=false,finished_=false,deferred_valid_=false;
};
}
#endif
