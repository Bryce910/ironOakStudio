#ifndef VALWOOD_WORLD_ARENA_CAMERA_H
#define VALWOOD_WORLD_ARENA_CAMERA_H
#include "world_arena_combat.h"

// An explicit, stable target. Nearest-opponent HUD changes never own the camera.
// Yaw is steered around the player's existing collision-tested follow boom;
// neither the player position nor the rendered eye is shifted toward an enemy.
class WorldArenaTargetLock {
    unsigned target_=8;
    static bool live(const world_arena_combat::Arena &arena,unsigned id){
        if(id>=arena.config().total)return false;
        const auto state=arena.state();
        if(state!=world_arena_combat::State::Entering&&state!=world_arena_combat::State::Fighting)return false;
        const auto &actor=arena.challengers()[id];const auto *opponent=arena.opponent(id);
        return (actor.state==world_arena_combat::ChallengerState::Entering||actor.state==world_arena_combat::ChallengerState::Active)&&
            opponent&&opponent->health>0&&world_arena_combat::finite(actor.position);
    }
public:
    unsigned target()const{return target_;}
    bool locked()const{return target_<8;}
    void clear(){target_=8;}
    bool maintain(const world_arena_combat::Arena &arena,world_arena_combat::Vec3 player){
        if(!live(arena,target_)||!world_arena_combat::finite(player)){clear();return false;}
        const auto delta=arena.challengers()[target_].position-player;
        if(std::hypot(delta.x,delta.z)>28){clear();return false;}
        return true;
    }
    bool toggle(const world_arena_combat::Arena &arena,world_arena_combat::Vec3 player,float yaw){
        if(locked()){clear();return false;}
        if(!world_arena_combat::finite(player)||!std::isfinite(yaw))return false;
        double best=1e30;const double fx=std::sin(yaw),fz=-std::cos(yaw);
        for(unsigned id=0;id<arena.config().total;++id){
            if(!live(arena,id))continue;
            const auto delta=arena.challengers()[id].position-player;
            const double range=std::hypot(delta.x,delta.z);
            if(range<.15||range>24)continue;
            const double facing=(delta.x*fx+delta.z*fz)/range;
            if(facing<.17)continue;
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
#endif
