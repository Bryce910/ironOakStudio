#ifndef VALWOOD_WORLD_ARENA_COMBAT_H
#define VALWOOD_WORLD_ARENA_COMBAT_H

#include "world_training_combat.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

// Pure arena-local orchestration and physical arrows. Owns the single shared
// player update; the viewer retains rendering and ordinary input ownership.
namespace world_arena_combat {
struct Vec3 {double x=0,y=0,z=0;};
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(Vec3 a,double b){return {a.x*b,a.y*b,a.z*b};}
inline double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}

enum class Weapon {Sword,Bow};
enum class State {Idle,Entering,Fighting,Intermission,Won,Lost};
enum class ChallengerState {Waiting,Entering,Active,Defeated,Unused};
struct Config {unsigned total=8,simultaneous=1;};
inline bool valid(Config c){return c.total>=1&&c.total<=8&&c.simultaneous>=1&&c.simultaneous<=c.total;}
enum class ArrowState {Unused,Flying,HitEnemy,HitGround,HitWall,Expired};
struct Challenger {ChallengerState state=ChallengerState::Waiting;Vec3 position={};double defeat_age=0;float defeat_yaw=0,yaw=0;bool moving=false;unsigned slot=0;};
struct Arrow {
    ArrowState state=ArrowState::Unused;
    Vec3 position={},previous={},velocity={};
    double age=0,damage=0;
    unsigned generation=0;
    unsigned opponent_id=8;
};
struct Bounds {
    double half_extent=10.8,floor_y=0,player_radius=.38;
    bool contains(Vec3 p,double inset=0)const{
        return finite(p)&&std::isfinite(inset)&&half_extent>inset&&
            std::abs(p.x)<=half_extent-inset&&std::abs(p.z)<=half_extent-inset;
    }
    Vec3 constrain_player(Vec3 p)const{
        if(!finite(p))return {0,floor_y,-5};
        const double limit=std::max(.1,half_extent-player_radius);
        p.x=std::clamp(p.x,-limit,limit);p.z=std::clamp(p.z,-limit,limit);
        return p;
    }
};

// First intersection with a vertical body capsule, including an arrow radius.
// Returns +infinity for a miss. The swept test prevents thin, fast arrows from
// tunneling through an enemy between frames.
inline double swept_capsule(Vec3 from,Vec3 to,Vec3 feet,double body_radius=.34,
                            double body_height=1.8,double arrow_radius=.025){
    const double infinity=std::numeric_limits<double>::infinity();
    if(!finite(from)||!finite(to)||!finite(feet)||!std::isfinite(body_radius)||
       !std::isfinite(body_height)||!std::isfinite(arrow_radius)||body_radius<=0||
       arrow_radius<0||body_height<2*body_radius)return infinity;
    const double radius=body_radius+arrow_radius;
    const double bottom=feet.y+body_radius,top=feet.y+body_height-body_radius;
    const Vec3 delta=to-from;
    const double closest_y=std::clamp(from.y,bottom,top);
    if(dot(from-Vec3{feet.x,closest_y,feet.z},from-Vec3{feet.x,closest_y,feet.z})<=radius*radius)return 0;
    double hit=infinity;
    auto root=[&](double a,double b,double c,auto accept){
        if(a<=1e-20)return;
        const double discriminant=b*b-4*a*c;if(discriminant<0)return;
        const double square=std::sqrt(std::max(0.,discriminant));
        for(double t:{(-b-square)/(2*a),(-b+square)/(2*a)})
            if(t>=0&&t<=1&&t<hit&&accept(t))hit=t;
    };
    const double x=from.x-feet.x,z=from.z-feet.z;
    root(delta.x*delta.x+delta.z*delta.z,2*(x*delta.x+z*delta.z),x*x+z*z-radius*radius,
         [&](double t){const double y=from.y+delta.y*t;return y>=bottom&&y<=top;});
    for(double y:{bottom,top}){
        const Vec3 rel=from-Vec3{feet.x,y,feet.z};
        root(dot(delta,delta),2*dot(rel,delta),dot(rel,rel)-radius*radius,[](double){return true;});
    }
    return hit;
}

class Arena {
public:
    static constexpr std::size_t challenger_count=8,arrow_capacity=24;
    static constexpr double gravity=9.81,arrow_lifetime=4.,maximum_draw=1.;
    static constexpr double entering_speed=4.5,chase_speed=3.,intermission_seconds=1.2;
    Bounds bounds={};
    bool keep_defeated=false;
private:
    std::array<Challenger,challenger_count> challengers_={};
    std::array<TrainingOpponent,challenger_count> opponents_={};
    std::array<Arrow,arrow_capacity> arrows_={};
    std::array<double,challenger_count> entry_delay_={};
    TrainingEvents events_={};
    Config config_={};
    State state_=State::Idle;
    Weapon weapon_=Weapon::Sword;
    std::size_t current_=0,next_to_enter_=0;
    unsigned defeated_=0,generation_=0,shots_=0,arrow_hits_=0,ground_hits_=0,wall_hits_=0;
    bool drawing_=false;
    double draw_time_=0;

    Vec3 queue_position(std::size_t index)const{return {-8.4+2.4*double(index),bounds.floor_y,8.6};}
    void formation(){
        for(std::size_t i=0;i<challenger_count;++i){
            challengers_[i]={i<config_.total?ChallengerState::Waiting:ChallengerState::Unused,queue_position(i)};
            opponents_[i]=TrainingOpponent{};opponents_[i].rng^=0x9e3779b9u*unsigned(i);
            opponents_[i].duration=.65+.11*double(i);opponents_[i].guard=TrainingDirection((i+2)%5);
        }
    }
    void clear_arrows(){for(auto &arrow:arrows_)arrow=Arrow{};}
    bool slot_busy(unsigned slot)const{
        for(const auto &a:challengers_)if(a.slot==slot&&(a.state==ChallengerState::Entering||a.state==ChallengerState::Active))return true;
        return false;
    }
    void fill_slots(){
        for(unsigned slot=0;slot<config_.simultaneous&&next_to_enter_<config_.total;++slot){
            if(slot_busy(slot)||entry_delay_[slot]>0)continue;
            auto &a=challengers_[next_to_enter_++];a.state=ChallengerState::Entering;a.slot=slot;
        }
    }
    Vec3 approach_position(unsigned slot,Vec3 player)const{
        const double angle=6.283185307179586*double(slot)/double(config_.simultaneous);
        return bounds.constrain_player(player+Vec3{std::sin(angle)*2.05,0,std::cos(angle)*2.05});
    }
    void movement(double dt,Vec3 player,bool stopped){
        for(auto &a:challengers_){
            a.moving=false;
            if(a.state!=ChallengerState::Entering&&a.state!=ChallengerState::Active)continue;
            const auto id=std::size_t(&a-challengers_.data());const auto &o=opponents_[id];
            if(stopped||(a.state==ChallengerState::Active&&o.phase!=TrainingEnemyPhase::Measure&&o.phase!=TrainingEnemyPhase::Recovery))continue;
            const Vec3 goal=approach_position(a.slot,player),delta=goal-a.position;
            const double distance=std::hypot(delta.x,delta.z),speed=a.state==ChallengerState::Entering?entering_speed:chase_speed;
            if(distance>.08){
                const double travel=std::min(speed*dt,distance);a.position=bounds.constrain_player(a.position+Vec3{delta.x,0,delta.z}*(travel/distance));a.moving=travel>1e-8;
            }
            a.yaw=float(std::atan2(player.x-a.position.x,player.z-a.position.z));
            if(a.state==ChallengerState::Entering&&distance<=speed*dt+.12)a.state=ChallengerState::Active;
        }
        // Resolve moving approach bodies deterministically. Planted telegraphs
        // never slide; movers yield to them. No per-frame heap/physics objects.
        for(unsigned pass=0;pass<3;++pass)for(std::size_t i=0;i<config_.total;++i)for(std::size_t j=i+1;j<config_.total;++j){
            auto &a=challengers_[i];auto &b=challengers_[j];
            if((a.state!=ChallengerState::Active&&a.state!=ChallengerState::Entering)||
               (b.state!=ChallengerState::Active&&b.state!=ChallengerState::Entering)||(!a.moving&&!b.moving))continue;
            Vec3 delta=a.position-b.position;double d=std::hypot(delta.x,delta.z);
            if(d>=.88)continue;
            if(d<1e-8){delta={i%2?1.:-1.,0,0};d=1.;}
            const double overlap=.88-(d<.88?d:0.);
            const double aw=a.moving?(b.moving?.5:1.):0,bw=b.moving?(a.moving?.5:1.):0;
            a.position=bounds.constrain_player(a.position+delta*(overlap*aw/d));b.position=bounds.constrain_player(b.position-delta*(overlap*bw/d));
        }
    }
    void choose_focus(Vec3 player,unsigned requested=8){
        if(requested<config_.total&&opponents_[requested].health>0&&
           (challengers_[requested].state==ChallengerState::Active||challengers_[requested].state==ChallengerState::Entering)){
            current_=requested;return;
        }
        double nearest=std::numeric_limits<double>::infinity();bool found=false;
        for(std::size_t i=0;i<config_.total;++i)if(challengers_[i].state==ChallengerState::Active){
            const Vec3 d=challengers_[i].position-player;const double squared=d.x*d.x+d.z*d.z;
            if(squared<nearest){nearest=squared;current_=i;found=true;}
        }
        if(!found)for(std::size_t i=0;i<config_.total;++i)if(challengers_[i].state==ChallengerState::Entering){current_=i;break;}
    }
    void project_focus(WorldTrainingCombat &fight)const{
        const auto &o=opponents_[current_];fight.enemy_health=o.health;fight.enemy_max_health=o.max_health;
        fight.enemy_direction=o.direction;fight.enemy_guard=o.guard;fight.enemy_phase=o.phase;
        fight.enemy_time=o.time;fight.enemy_duration=o.duration;fight.enemy_resolved=o.resolved;fight.enemy_swoosh=o.swoosh;
    }
    void mark_defeated(Vec3 player){
        for(unsigned i=0;i<config_.total;++i){auto &a=challengers_[i];
            if(a.state!=ChallengerState::Active||opponents_[i].health>0)continue;
            a.state=ChallengerState::Defeated;a.moving=false;a.defeat_age=0;
            a.defeat_yaw=float(std::atan2(player.x-a.position.x,player.z-a.position.z));++defeated_;
            entry_delay_[a.slot]=intermission_seconds;
        }
    }
    void projectiles(double dt,WorldTrainingCombat &fight,const std::array<Vec3,challenger_count> &before){
        double remaining=dt;
        while(remaining>1e-10){
            const double h=std::min(remaining,1./240.);remaining-=h;
            for(auto &arrow:arrows_){
                if(arrow.state==ArrowState::Unused)continue;
                if(arrow.state!=ArrowState::Flying){arrow.age+=h;if(arrow.age>.3)arrow=Arrow{};continue;}
                const Vec3 from=arrow.position,to=from+arrow.velocity*h+Vec3{0,-.5*gravity*h*h,0};
                double collision=std::numeric_limits<double>::infinity();ArrowState impact=ArrowState::Flying;unsigned victim=8;
                if(state_==State::Fighting&&fight.state==TrainingState::Fighting&&arrow.generation==generation_){
                    for(unsigned i=0;i<config_.total;++i)if(challengers_[i].state==ChallengerState::Active&&opponents_[i].health>0){
                        const Vec3 movement=challengers_[i].position-before[i];
                        const double start=dt>0?(dt-remaining-h)/dt:0,end=dt>0?(dt-remaining)/dt:1;
                        const double t=swept_capsule(from-(before[i]+movement*start),to-(before[i]+movement*end),{});
                        if(t<collision){collision=t;impact=ArrowState::HitEnemy;victim=i;}
                    }
                }
                const double floor=bounds.floor_y+.025;
                const double ground=from.y<=floor?0:to.y<=floor?(from.y-floor)/(from.y-to.y):std::numeric_limits<double>::infinity();
                if(ground<=collision){collision=ground;impact=ArrowState::HitGround;victim=8;}
                if(!bounds.contains(to,.025)){
                    const Vec3 delta=to-from;const double limit=bounds.half_extent-.025;double wall=std::numeric_limits<double>::infinity();
                    if(!bounds.contains(from,.025))wall=0;
                    else{
                        if(delta.x>0&&to.x>limit)wall=std::min(wall,(limit-from.x)/delta.x);
                        if(delta.x<0&&to.x<-limit)wall=std::min(wall,(-limit-from.x)/delta.x);
                        if(delta.z>0&&to.z>limit)wall=std::min(wall,(limit-from.z)/delta.z);
                        if(delta.z<0&&to.z<-limit)wall=std::min(wall,(-limit-from.z)/delta.z);
                    }
                    if(wall>=0&&wall<=1&&wall<collision){collision=wall;impact=ArrowState::HitWall;victim=8;}
                }
                arrow.previous=from;
                if(std::isfinite(collision)&&collision>=0&&collision<=1){
                    arrow.position=from+(to-from)*collision;arrow.state=impact;arrow.age=0;arrow.opponent_id=victim;
                    if(impact==ArrowState::HitEnemy){
                        const unsigned previous=events_.count;
                        if(fight.damage_opponent(opponents_[victim],victim,float(arrow.damage),TrainingDirection::Thrust,events_)){
                            ++arrow_hits_;
                            for(unsigned e=previous;e<events_.count;++e)if(events_.values[e].kind==TrainingContactKind::PlayerBody){
                                auto &contact=events_.values[e];contact.projectile=true;
                                contact.point[0]=arrow.position.x;contact.point[1]=arrow.position.y;contact.point[2]=arrow.position.z;
                            }
                        }
                    }else if(impact==ArrowState::HitGround)++ground_hits_;else if(impact==ArrowState::HitWall)++wall_hits_;
                }else{arrow.position=to;arrow.velocity.y-=gravity*h;arrow.age+=h;if(arrow.age>=arrow_lifetime){arrow.state=ArrowState::Expired;arrow.age=0;}}
            }
        }
    }
public:
    Arena(){formation();}
    State state()const{return state_;}
    Weapon weapon()const{return weapon_;}
    const Config &config()const{return config_;}
    const std::array<Challenger,challenger_count>&challengers()const{return challengers_;}
    const std::array<Arrow,arrow_capacity>&arrows()const{return arrows_;}
    const TrainingOpponent *opponent(unsigned id)const{return id<config_.total?&opponents_[id]:nullptr;}
    const TrainingEvents &events()const{return events_;}
    WorldTrainingCombat view(const WorldTrainingCombat &fight,unsigned id)const{
        if(id>=config_.total)return WorldTrainingCombat{};
        auto result=fight.opponent_view(opponents_[id]);
        if(challengers_[id].state==ChallengerState::Waiting||challengers_[id].state==ChallengerState::Entering)result.state=TrainingState::Idle;
        return result;
    }
    unsigned active_count()const{unsigned n=0;for(const auto &a:challengers_)n+=a.state==ChallengerState::Active;return n;}
    unsigned entering_count()const{unsigned n=0;for(const auto &a:challengers_)n+=a.state==ChallengerState::Entering;return n;}
    std::size_t current_index()const{return current_;}
    unsigned defeated()const{return defeated_;}
    unsigned shots()const{return shots_;}
    unsigned arrow_hits()const{return arrow_hits_;}
    unsigned ground_hits()const{return ground_hits_;}
    unsigned wall_hits()const{return wall_hits_;}
    Vec3 active_position()const{return challengers_[current_].position;}
    Vec3 player_spawn()const{return {0,bounds.floor_y,-5};}
    bool drawing()const{return drawing_;}
    double draw_fraction()const{return std::clamp(draw_time_/maximum_draw,0.,1.);}
    void cancel_draw(){drawing_=false;draw_time_=0;}
    void set_weapon(Weapon weapon){if(weapon_!=weapon){weapon_=weapon;cancel_draw();}}
    // Callers must also clear_buffer on weapon handoff (the runtime owns the
    // shared player's input). Starting a run always resets it itself.
    bool start(WorldTrainingCombat &fight,Config config,bool assist=false){
        if(!valid(config))return false;
        fight=WorldTrainingCombat{};if(!fight.start_shared_arena(assist))return false;
        config_=config;formation();clear_arrows();cancel_draw();events_.clear();entry_delay_.fill(0);
        ++generation_;current_=next_to_enter_=defeated_=shots_=arrow_hits_=ground_hits_=wall_hits_=0;
        fill_slots();state_=State::Entering;project_focus(fight);return true;
    }
    bool start(WorldTrainingCombat &fight,bool assist=false){return start(fight,Config{},assist);}
    void cancel(WorldTrainingCombat &fight){
        fight.leave();fight.player_phase=TrainingPlayerPhase::Ready;fight.combo_count=0;fight.guard=false;
        formation();clear_arrows();cancel_draw();events_.clear();entry_delay_.fill(0);state_=State::Idle;defeated_=0;
    }
    // This owns the SINGLE player update for arena mode. The ordinary legacy
    // fight.update must not also advance it (and explicitly refuses shared mode).
    void update(double dt,WorldTrainingCombat &fight,Vec3 player,float player_yaw,bool paused=false,unsigned requested_focus=8){
        if(!std::isfinite(dt)||dt<0||!finite(player)||!std::isfinite(player_yaw))return;
        events_.clear();
        if(paused){fight.guard=false;fight.clear_buffer();cancel_draw();return;}
        dt=std::min(dt,.25);
        if(state_==State::Idle)return;
        const bool stopped=fight.impact_pause>0;
        const bool result=state_==State::Won||state_==State::Lost;
        // The final contact's hit-stop must finish even after the arena stops
        // stepping live AI; otherwise its corpse would remain frozen forever.
        double corpse_dt=stopped?0:dt;
        if(result){const double hold=std::min(dt,fight.impact_pause);fight.impact_pause=std::max(0.,fight.impact_pause-dt);corpse_dt=dt-hold;}
        for(auto &a:challengers_)if(a.state==ChallengerState::Defeated&&corpse_dt>0){a.defeat_age=std::min(a.defeat_age+corpse_dt,3600.);if(!keep_defeated&&a.defeat_age>=intermission_seconds)a.position=queue_position(std::size_t(&a-challengers_.data()));}
        if(result)return;
        for(auto &delay:entry_delay_)delay=std::max(0.,delay-dt);
        fill_slots();std::array<Vec3,challenger_count> before={};for(unsigned i=0;i<challenger_count;++i)before[i]=challengers_[i].position;
        movement(dt,player,stopped);choose_focus(player,requested_focus);
        std::array<TrainingTarget,challenger_count> targets={};std::size_t count=0;
        const double fx=std::sin(player_yaw),fz=std::cos(player_yaw);
        for(unsigned i=0;i<config_.total;++i)if(challengers_[i].state==ChallengerState::Active){
            const Vec3 delta=challengers_[i].position-player;const double distance=std::hypot(delta.x,delta.z);
            const double facing=distance>1e-9?(delta.x*fx+delta.z*fz)/distance:0;
            targets[count++]={&opponents_[i],i,distance,facing>=.35,facing>=.10};
        }
        if(!fight.update_shared(dt,targets.data(),count,events_)){state_=State::Lost;fight.finish_shared_arena(false);return;}
        state_=active_count()?State::Fighting:entering_count()?State::Entering:State::Intermission;
        projectiles(dt,fight,before);mark_defeated(player);
        if(fight.state==TrainingState::Lost||fight.state==TrainingState::Cancelled){state_=State::Lost;cancel_draw();clear_arrows();}
        else if(defeated_==config_.total){state_=State::Won;fight.finish_shared_arena(true);cancel_draw();clear_arrows();}
        else if(!active_count())state_=entering_count()?State::Entering:State::Intermission;
        choose_focus(player,requested_focus);project_focus(fight);
        if(drawing_){if(state_!=State::Fighting||fight.state!=TrainingState::Fighting||weapon_!=Weapon::Bow)cancel_draw();else draw_time_=std::min(maximum_draw,draw_time_+dt);}
    }
    void update(double dt,WorldTrainingCombat &fight,Vec3 player,bool paused=false){
        const auto e=active_position();update(dt,fight,player,float(std::atan2(e.x-player.x,e.z-player.z)),paused);
    }
    bool begin_draw(const WorldTrainingCombat &fight){
        if(weapon_!=Weapon::Bow||state_!=State::Fighting||fight.state!=TrainingState::Fighting||!active_count()||fight.player_phase!=TrainingPlayerPhase::Ready||fight.stamina<8||drawing_)return false;
        drawing_=true;draw_time_=0;return true;
    }
    bool release_draw(WorldTrainingCombat &fight,Vec3 origin,Vec3 direction){
        if(!drawing_)return false;
        const double charge=draw_fraction();cancel_draw();const double length=std::sqrt(dot(direction,direction));
        if(weapon_!=Weapon::Bow||state_!=State::Fighting||fight.state!=TrainingState::Fighting||charge<.12||!bounds.contains(origin,.05)||origin.y<bounds.floor_y+.30||origin.y>bounds.floor_y+3.5||!finite(direction)||!std::isfinite(length)||length<1e-8)return false;
        const float cost=float(8+6*charge);if(fight.stamina<cost)return false;
        for(auto &arrow:arrows_)if(arrow.state==ArrowState::Unused){
            arrow={ArrowState::Flying,origin,origin,direction*((14+14*charge)/length),0,14+22*charge,generation_,8};
            fight.stamina-=cost;fight.regen_delay=.7;fight.set_guard(false);fight.clear_buffer();++shots_;return true;
        }
        return false;
    }
};
}
#endif
