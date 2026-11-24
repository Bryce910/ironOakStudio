#ifndef VALWOOD_WORLD_ARENA_CAMERA_H
#define VALWOOD_WORLD_ARENA_CAMERA_H
#include "world_arena_combat.h"
#include "world_combat_capacity.h"

// An explicit, stable target. Nearest-opponent HUD changes never own the camera.
// Yaw is steered around the player's existing collision-tested follow boom;
// neither the player position nor the rendered eye is shifted toward an enemy.
class WorldArenaTargetLock {
    unsigned target_=world_combat_capacity::opponents;
    static bool live(const world_arena_combat::Arena &arena,unsigned id){
        if(id>=world_combat_capacity::opponents||id>=arena.config().total)return false;
        const auto state=arena.state();
        if(state!=world_arena_combat::State::Entering&&state!=world_arena_combat::State::Fighting)return false;
        const auto &actor=arena.challengers()[id];const auto *opponent=arena.opponent(id);
        return (actor.state==world_arena_combat::ChallengerState::Entering||actor.state==world_arena_combat::ChallengerState::Active)&&
            opponent&&opponent->health>0&&world_arena_combat::finite(actor.position);
    }
    static bool eligible(const world_arena_combat::Arena &arena,unsigned id,world_arena_combat::Vec3 player){
        if(!world_arena_combat::finite(player)||!live(arena,id))return false;
        const auto delta=arena.challengers()[id].position-player;
        return std::hypot(delta.x,delta.z)<=28;
    }
    bool next(const world_arena_combat::Arena &arena,world_arena_combat::Vec3 player,unsigned after){
        // Stable identity order visits every engaged opponent even while the
        // camera turns. Sorting by the changing camera yaw would starve targets
        // behind the hero when several enemies surround the player.
        for(unsigned offset=1;offset<=world_combat_capacity::opponents;++offset){
            const unsigned id=(after+offset)%world_combat_capacity::opponents;
            if(eligible(arena,id,player)){target_=id;return true;}
        }
        clear();return false;
    }
public:
    unsigned target()const{return target_;}
    bool locked()const{return target_<world_combat_capacity::opponents;}
    void clear(){target_=world_combat_capacity::opponents;}
    bool maintain(const world_arena_combat::Arena &arena,world_arena_combat::Vec3 player){
        if(!locked())return false; // Free camera never acquires a target itself.
        if(eligible(arena,target_,player))return true;
        // A defeated, inactive or distant target passes focus to the next
        // combatant; an empty/finished fight returns to the normal camera.
        return next(arena,player,target_);
    }
    bool cycle(const world_arena_combat::Arena &arena,world_arena_combat::Vec3 player,float yaw){
        if(!world_arena_combat::finite(player)||!std::isfinite(yaw)){clear();return false;}
        if(locked())return next(arena,player,target_);
        double best=1e30;const double fx=std::sin(yaw),fz=-std::cos(yaw);
        for(unsigned id=0;id<arena.config().total;++id){
            if(!eligible(arena,id,player))continue;
            const auto delta=arena.challengers()[id].position-player;
            const double range=std::hypot(delta.x,delta.z);
            const double facing=range>.001?(delta.x*fx+delta.z*fz)/range:1.;
            const double score=(1-facing)*4+range*.035;
            if(score<best-1e-9){best=score;target_=id;}
        }
        return locked();
    }
    float steer(const world_arena_combat::Arena &arena,world_arena_combat::Vec3 player,float yaw,float dt){
        if(!std::isfinite(yaw)||!std::isfinite(dt)||dt<=0||!maintain(arena,player))return yaw;
        const auto delta=arena.challengers()[target_].position-player;
        if(std::hypot(delta.x,delta.z)<.15)return yaw;
        const double step=std::clamp(double(dt),0.,.05);
        const double angle=std::remainder(std::atan2(delta.x,-delta.z)-double(yaw),6.283185307179586);
        return float(yaw+std::clamp(angle*(1-std::exp(-12*step)),-4.5*step,4.5*step));
    }
};

// Shared by RB and middle mouse. A fresh press cycles once immediately; holding
// the same button releases focus once after .45s. Repeats and held buttons do
// not continually cycle, attack, or reacquire after release.
class WorldArenaFocusGesture {
    bool held_=false,released_=false;
    double elapsed_=0;
public:
    static constexpr double hold_seconds=.45;
    bool press(){if(held_)return false;held_=true;released_=false;elapsed_=0;return true;}
    void reset(){held_=released_=false;elapsed_=0;}
    bool update(float dt){
        if(!held_||released_||!std::isfinite(dt)||dt<=0)return false;
        elapsed_+=std::min(double(dt),.1);
        if(elapsed_+1e-7<hold_seconds)return false;
        released_=true;return true;
    }
};
#endif
