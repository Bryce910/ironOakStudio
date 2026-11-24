#ifndef VALWOOD_WORLD_ARENA_COMBAT_H
#define VALWOOD_WORLD_ARENA_COMBAT_H

#include "world_training_combat.h"
#include "world_arena_player_movement.h"
#include "world_arena_opponents.h"
#include "world_arena_team_contact.h"
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

using Weapon=world_weapon_loadout::Weapon;
enum class State {Idle,Entering,Fighting,Intermission,Won,Lost};
enum class ChallengerState {Waiting,Entering,Active,Defeated,Unused};
struct Config {unsigned total=8,simultaneous=1;world_arena_opponents::Kind opponents=world_arena_opponents::Kind::Human;unsigned teammates=0;world_combat_difficulty::Difficulty difficulty=world_combat_difficulty::Difficulty::Normal;};
inline bool valid(Config c){return c.total>=1&&c.total<=world_combat_capacity::opponents&&c.teammates<=world_combat_capacity::teammates&&world_combat_difficulty::valid(c.difficulty)&&c.simultaneous>=1&&c.simultaneous<=c.total&&world_arena_opponents::valid(c.opponents);}
enum class ArrowState {Unused,Flying,HitEnemy,HitGround,HitWall,Expired};
struct Challenger {ChallengerState state=ChallengerState::Waiting;Vec3 position={};double defeat_age=0;float defeat_yaw=0,yaw=0;bool moving=false;unsigned slot=0;double approach_speed=0;bool orbit_approach=false;};
struct Arrow {
    ArrowState state=ArrowState::Unused;
    Vec3 position={},previous={},velocity={};
    double age=0,damage=0;
    unsigned generation=0;
    unsigned opponent_id=world_combat_capacity::npcs;
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
    static constexpr std::size_t opponent_capacity=world_combat_capacity::opponents,teammate_first=opponent_capacity,challenger_count=world_combat_capacity::npcs,arrow_capacity=24;
    static constexpr unsigned player_target=world_combat_capacity::npcs,no_target=player_target+1;
    static constexpr double gravity=9.81,arrow_lifetime=4.,maximum_draw=1.;
    static constexpr double entering_speed=4.5,chase_speed=3.,intermission_seconds=1.2;
    Bounds bounds={};
    bool keep_defeated=false;
private:
    std::array<Challenger,challenger_count> challengers_={};
    std::array<TrainingOpponent,challenger_count> opponents_={};
    std::array<Arrow,arrow_capacity> arrows_={};
    std::array<double,challenger_count> entry_delay_={};
    std::array<bool,challenger_count> fixture_pose_hold_={};
    TrainingEvents events_={};
    TeamContactEvents team_events_={};
    TeamContactQueryFn team_query_=nullptr;void *team_context_=nullptr;
    std::array<unsigned,challenger_count> targets_={};
    std::array<std::uint64_t,challenger_count> team_seen_={};
    std::array<double,challenger_count> team_guard_at_={};
    std::uint64_t team_serial_=0;unsigned teammates_defeated_=0;
    Config config_={};
    State state_=State::Idle;
    Weapon weapon_=Weapon::Sword;
    std::size_t current_=0,next_to_enter_=0;
    unsigned defeated_=0,generation_=0,shots_=0,arrow_hits_=0,ground_hits_=0,wall_hits_=0;
    bool drawing_=false;
    double draw_time_=0;
    // A rig may opt into its measured physical weapon spacing. The default
    // preserves the original actor and every ordinary arena configuration.
    double approach_radius_=2.05;
    static constexpr double team_approach_radius=1.55,team_commit_reach=1.65;
    // Profile of the committed roster, in metres. The original human capsule
    // remains the default; imported opponents supply their measured height.
    double opponent_body_radius_=.34,opponent_body_height_=1.8;
    std::array<double,challenger_count> body_radius_={},body_height_={};

    Vec3 queue_position(std::size_t index)const{if(index>=teammate_first){const unsigned n=unsigned(index-teammate_first);return {(n%2?1.:-1.)*(3.+2.*double(n/2)),bounds.floor_y,-6.5};}return {-8.4+2.4*double(index%8),bounds.floor_y,8.6-1.5*double(index/8)};}
    bool npc_enabled(unsigned id)const{return id<config_.total||(id>=teammate_first&&id<teammate_first+config_.teammates);}
    bool npc_live(unsigned id)const{return id<challenger_count&&npc_enabled(id)&&opponents_[id].health>0&&(challengers_[id].state==ChallengerState::Active||challengers_[id].state==ChallengerState::Entering);}
    bool opposite(unsigned a,unsigned b)const{return a<challenger_count&&b<challenger_count&&((a<teammate_first)!=(b<teammate_first));}
    void formation(){
        fixture_pose_hold_.fill(false);
        for(std::size_t i=0;i<challenger_count;++i){
            challengers_[i]={i<config_.total?ChallengerState::Waiting:i>=teammate_first&&i<teammate_first+config_.teammates?ChallengerState::Active:ChallengerState::Unused,queue_position(i)};
            targets_[i]=i<teammate_first?player_target:no_target;team_seen_[i]=0;team_guard_at_[i]=0;
            opponents_[i]=TrainingOpponent{};opponents_[i].rng^=0x9e3779b9u*unsigned(i);
            opponents_[i].duration=.65+.11*double(i);opponents_[i].guard=TrainingDirection((i+2)%5);
        }
    }
    void clear_arrows(){for(auto &arrow:arrows_)arrow=Arrow{};}
    bool slot_busy(unsigned slot)const{
        for(unsigned i=0;i<config_.total;++i){const auto&a=challengers_[i];if(a.slot==slot&&(a.state==ChallengerState::Entering||a.state==ChallengerState::Active))return true;}
        return false;
    }
    void fill_slots(){
        for(unsigned slot=0;slot<config_.simultaneous&&next_to_enter_<config_.total;++slot){
            if(slot_busy(slot)||entry_delay_[slot]>0)continue;
            auto &a=challengers_[next_to_enter_++];a.state=ChallengerState::Entering;a.slot=slot;
        }
    }
    Vec3 approach_position(unsigned slot,Vec3 player,double radius)const{
        const double angle=6.283185307179586*double(slot)/double(config_.simultaneous);
        return bounds.constrain_player(player+Vec3{std::sin(angle)*radius,0,std::cos(angle)*radius});
    }
    void movement(double dt,Vec3 player,bool stopped,const WorldTrainingCombat &fight){
        namespace separation=world_arena_player_movement;
        const double clearance=std::min(1.35,std::max(1.20,approach_radius_-.05));
        const separation::Circle player_body{player.x,player.z,clearance};
        std::array<Vec3,challenger_count> roots={};for(unsigned i=0;i<challenger_count;++i)roots[i]=challengers_[i].position;
        auto safe_move=[&](Vec3 from,Vec3 requested,std::size_t self){
            std::array<separation::Circle,challenger_count+1> bodies={};unsigned count=1;bodies[0]=player_body;
            for(unsigned i=0;i<challenger_count;++i)if(i!=self&&npc_enabled(i)&&(challengers_[i].state==ChallengerState::Active||challengers_[i].state==ChallengerState::Entering))bodies[count++]={challengers_[i].position.x,challengers_[i].position.z,opponent_pair_clearance(unsigned(self),i)};
            const auto move=separation::move({from.x,from.z},{requested.x,requested.z},bodies.data(),count,std::max(.1,bounds.half_extent-bounds.player_radius));
            return move.valid?Vec3{move.position.x,bounds.floor_y,move.position.z}:from;
        };
        for(auto &a:challengers_){
            a.moving=false;
            if(a.state!=ChallengerState::Entering&&a.state!=ChallengerState::Active)continue;
            const auto id=std::size_t(&a-challengers_.data());const auto &o=opponents_[id];
            if(fixture_pose_hold_[id]){a.approach_speed=0;continue;}
            if(!world_combat_anatomy::can_move(o.severed_parts)||!world_combat_anatomy::can_fight(o.severed_parts)){a.approach_speed=0;continue;}
            // Every authored attack/reaction is in-place. Chasing during the
            // recovery capture used to slide the whole actor over a metre.
            if(stopped||(a.state==ChallengerState::Active&&o.phase!=TrainingEnemyPhase::Measure)){a.approach_speed=0;continue;}
            // Plant before raising a read guard. Walking suppressed the actual
            // guard pose even when the simulation had correctly read the cut.
            // Attack/recovery roots remain planted through the existing path.
            const bool defending_player=targets_[id]==player_target&&
                (a.state==ChallengerState::Active||a.state==ChallengerState::Entering)&&
                world_combat_difficulty::arena_defensive_hold_s(config_.difficulty)>0&&
                !fight.player_resolved&&(fight.player_phase==TrainingPlayerPhase::Windup||fight.player_phase==TrainingPlayerPhase::Strike)&&
                std::hypot(a.position.x-player.x,a.position.z-player.z)<=world_weapon_loadout::profile(fight.weapon).reach+.8;
            if(defending_player){a.approach_speed=0;a.yaw=float(std::atan2(player.x-a.position.x,player.z-a.position.z));continue;}
            const unsigned target=targets_[id];const bool npc_target=target<challenger_count&&npc_live(target);const Vec3 center=npc_target?challengers_[target].position:player;
            // Actual Knight/Orc source edges miss at the player formation's
            // 2.05m spacing. Only NPC duels close to the measured 1.55m reach.
            const double approach=npc_target?team_approach_radius:approach_radius_;
            Vec3 goal=approach_position(a.slot,center,approach);if(id>=teammate_first){const double angle=6.283185307179586*double(id-teammate_first)/std::max(1u,config_.teammates);goal=bounds.constrain_player(center+Vec3{std::sin(angle)*approach,0,std::cos(angle)*approach});}Vec3 delta=goal-a.position;
            const double formation_distance=std::hypot(delta.x,delta.z),radial_distance=std::hypot(a.position.x-center.x,a.position.z-center.z);
            // Formation slots can lie across the player. Steer toward a
            // deterministic side waypoint rather than trying to walk through
            // the idle controller or remaining blocked head-on forever.
            bool detour=false;
            // Detect a blocked direct route without a per-frame path allocation.
            if(std::hypot(delta.x,delta.z)>.08){
                Vec3 obstacle={};double obstacle_radius=0,nearest_along=2.;
                auto consider=[&](Vec3 center,double radius){
                    const Vec3 radial=a.position-center;const double length2=delta.x*delta.x+delta.z*delta.z;
                    const double closest=std::clamp(-(radial.x*delta.x+radial.z*delta.z)/length2,0.,1.);
                    if(closest<=0||closest>=nearest_along)return;
                    if(std::hypot(radial.x+delta.x*closest,radial.z+delta.z*closest)<radius+.025){nearest_along=closest;obstacle=center;obstacle_radius=radius;}
                };
                consider(player,clearance);
                for(unsigned i=0;i<challenger_count;++i)if(i!=id&&npc_enabled(i)&&(challengers_[i].state==ChallengerState::Active||challengers_[i].state==ChallengerState::Entering))consider(challengers_[i].position,opponent_pair_clearance(unsigned(id),i));
                if(obstacle_radius)a.orbit_approach=true;
            }
            if(a.orbit_approach){
                const double angle=std::atan2(a.position.x-center.x,a.position.z-center.z),target_angle=std::atan2(goal.x-center.x,goal.z-center.z);
                const double difference=std::remainder(target_angle-angle,6.283185307179586);
                if(std::abs(difference)<.12)a.orbit_approach=false;
                else{
                    // Route outside the melee formation, then enter an open
                    // slot radially. This avoids another actor's planted boots.
                    const double next=angle+std::clamp(difference,-.40,.40),ring=std::max(clearance+.95,approach+.95);
                    delta=bounds.constrain_player(center+Vec3{std::sin(next)*ring,0,std::cos(next)*ring})-a.position;detour=true;
                }
            }
            const double distance=std::hypot(delta.x,delta.z);
            const double speed=targets_[id]==player_target&&radial_distance<4.2?1.8:(a.state==ChallengerState::Entering?entering_speed:chase_speed);
            if(distance>.08){
                // Ease actual approach displacement into the preloaded gait start.
                // Brake toward the destination rather than snapping from full
                // chase speed on its final frame. Attack clocks are untouched.
                const double acceleration=speed/.18;
                const double desired_speed=std::min(speed,std::sqrt(2*acceleration*std::max(0.,distance-.08)));
                a.approach_speed=std::min(desired_speed,a.approach_speed+acceleration*dt);
                const double travel=std::min(a.approach_speed*dt,distance);const Vec3 before=a.position;
                a.position=safe_move(a.position,a.position+Vec3{delta.x,0,delta.z}*(travel/distance),id);a.moving=std::hypot(a.position.x-before.x,a.position.z-before.z)>1e-8;
            }else a.approach_speed=0;
            a.yaw=float(std::atan2(center.x-a.position.x,center.z-a.position.z));
            if(a.state==ChallengerState::Entering&&(formation_distance<=speed*dt+.12||
                (detour&&radial_distance>=clearance-1.e-5&&radial_distance<=approach+.12)))a.state=ChallengerState::Active;
        }
        // Resolve moving approach bodies deterministically. Planted telegraphs
        // never slide; movers yield to them. No per-frame heap/physics objects.
        for(unsigned pass=0;pass<3;++pass)for(std::size_t i=0;i<challenger_count;++i)for(std::size_t j=i+1;j<challenger_count;++j){
            auto &a=challengers_[i];auto &b=challengers_[j];
            if((a.state!=ChallengerState::Active&&a.state!=ChallengerState::Entering)||
               (b.state!=ChallengerState::Active&&b.state!=ChallengerState::Entering)||(!a.moving&&!b.moving))continue;
            Vec3 delta=a.position-b.position;double d=std::hypot(delta.x,delta.z);
            if(d>=opponent_pair_clearance(i,j))continue;
            if(d<1e-8){delta={i%2?1.:-1.,0,0};d=1.;}
            const double clearance=opponent_pair_clearance(i,j);
            const double overlap=clearance-(d<clearance?d:0.);
            const double aw=a.moving?(b.moving?.5:1.):0,bw=b.moving?(a.moving?.5:1.):0;
            a.position=safe_move(a.position,a.position+delta*(overlap*aw/d),i);b.position=safe_move(b.position,b.position-delta*(overlap*bw/d),j);
        }
        // A blocked request must not play a walking cycle. Pair separation is
        // included in the actual displacement; planted actors were never moved.
        for(unsigned i=0;i<challenger_count;++i){auto &a=challengers_[i];a.position=safe_move(roots[i],a.position,i);a.moving=std::hypot(a.position.x-roots[i].x,a.position.z-roots[i].z)>1.e-8;}
    }
    void choose_focus(Vec3 player,unsigned requested=opponent_capacity){
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
        fight.enemy_decapitated=o.decapitated;fight.enemy_severed_parts=o.severed_parts;fight.enemy_weapon_hand=o.weapon_hand;
    }
    void mark_defeated(Vec3 player){
        for(unsigned i=0;i<challenger_count;++i){auto &a=challengers_[i];
            if(!npc_enabled(i)||(a.state!=ChallengerState::Active&&a.state!=ChallengerState::Entering)||opponents_[i].health>0)continue;
            a.state=ChallengerState::Defeated;a.moving=false;a.defeat_age=0;
            a.defeat_yaw=a.yaw;if(i<teammate_first){++defeated_;entry_delay_[a.slot]=intermission_seconds;}else ++teammates_defeated_;
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
                double collision=std::numeric_limits<double>::infinity();ArrowState impact=ArrowState::Flying;unsigned victim=player_target;
                if(state_==State::Fighting&&fight.state==TrainingState::Fighting&&arrow.generation==generation_){
                    for(unsigned i=0;i<config_.total;++i)if(challengers_[i].state==ChallengerState::Active&&opponents_[i].health>0){
                        const Vec3 movement=challengers_[i].position-before[i];
                        const double start=dt>0?(dt-remaining-h)/dt:0,end=dt>0?(dt-remaining)/dt:1;
                        const double t=swept_capsule(from-(before[i]+movement*start),to-(before[i]+movement*end),{},opponent_body_radius(i),opponent_body_height(i));
                        if(t<collision){collision=t;impact=ArrowState::HitEnemy;victim=i;}
                    }
                }
                const double floor=bounds.floor_y+.025;
                const double ground=from.y<=floor?0:to.y<=floor?(from.y-floor)/(from.y-to.y):std::numeric_limits<double>::infinity();
                if(ground<=collision){collision=ground;impact=ArrowState::HitGround;victim=player_target;}
                if(!bounds.contains(to,.025)){
                    const Vec3 delta=to-from;const double limit=bounds.half_extent-.025;double wall=std::numeric_limits<double>::infinity();
                    if(!bounds.contains(from,.025))wall=0;
                    else{
                        if(delta.x>0&&to.x>limit)wall=std::min(wall,(limit-from.x)/delta.x);
                        if(delta.x<0&&to.x<-limit)wall=std::min(wall,(-limit-from.x)/delta.x);
                        if(delta.z>0&&to.z>limit)wall=std::min(wall,(limit-from.z)/delta.z);
                        if(delta.z<0&&to.z<-limit)wall=std::min(wall,(-limit-from.z)/delta.z);
                    }
                    if(wall>=0&&wall<=1&&wall<collision){collision=wall;impact=ArrowState::HitWall;victim=player_target;}
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
    #include "world_arena_team_ai.inc"
public:
    Arena(){set_opponent_body_profile(.34,1.8);formation();}
    double opponent_body_radius(unsigned actor=challenger_count)const{return actor<challenger_count?body_radius_[actor]:opponent_body_radius_;}
    double opponent_body_height(unsigned actor=challenger_count)const{return actor<challenger_count?body_height_[actor]:opponent_body_height_;}
    double opponent_movement_radius(unsigned actor=challenger_count)const{return opponent_body_radius(actor)+.04;}
    double opponent_pair_clearance(unsigned a=challenger_count,unsigned b=challenger_count)const{return std::max(.88,opponent_movement_radius(a)+opponent_movement_radius(b));}
    static bool valid_opponent_body_profile(double radius,double height){
        return std::isfinite(radius)&&std::isfinite(height)&&radius>=.15&&radius<=.75&&height>=1.&&height<=3.5&&height>=2*radius;
    }
    bool set_opponent_body_profile(double radius,double height){
        if(!valid_opponent_body_profile(radius,height))return false;
        opponent_body_radius_=radius;opponent_body_height_=height;body_radius_.fill(radius);body_height_.fill(height);for(unsigned i=teammate_first;i<challenger_count;++i){body_radius_[i]=.34;body_height_[i]=1.8;}return true;
    }
    bool set_opponent_body_profile(unsigned actor,double radius,double height){
        if(actor>=challenger_count||!valid_opponent_body_profile(radius,height))return false;
        body_radius_[actor]=radius;body_height_[actor]=height;return true;
    }
    double approach_radius()const{return approach_radius_;}
    bool set_approach_radius(double metres){
        if(!std::isfinite(metres)||metres<1.||metres>3.)return false;
        approach_radius_=metres;return true;
    }
    State state()const{return state_;}
    Weapon weapon()const{return weapon_;}
    const Config &config()const{return config_;}
    const std::array<Challenger,challenger_count>&challengers()const{return challengers_;}
    const std::array<Arrow,arrow_capacity>&arrows()const{return arrows_;}
    const TrainingOpponent *opponent(unsigned id)const{return npc_enabled(id)?&opponents_[id]:nullptr;}
    const TrainingEvents &events()const{return events_;}
    const TeamContactEvents &team_events()const{return team_events_;}
    void set_team_contact_query(TeamContactQueryFn query,void*context=nullptr){team_query_=query;team_context_=context;}
    unsigned target(unsigned id)const{return id<challenger_count?targets_[id]:no_target;}
    bool teammate(unsigned id)const{return id>=teammate_first&&id<teammate_first+config_.teammates;}
    unsigned teammate_count()const{return config_.teammates;}
    unsigned teammate_alive()const{unsigned n=0;for(unsigned i=teammate_first;i<teammate_first+config_.teammates;++i)n+=npc_live(i);return n;}
    unsigned teammates_defeated()const{return teammates_defeated_;}
    // Explicitly marked geometric QA fixture only. The viewer verifies its
    // private marker before calling this. No health, sever, defeat, reward or
    // weapon-hand field can be injected here. Release before ordinary AI proof.
    bool fixture_geometry_pose(unsigned id,Vec3 position,float yaw,TrainingDirection guard,std::uint64_t observed_serial,bool recovery=false){
        if(id>=config_.total||challengers_[id].state!=ChallengerState::Active||!bounds.contains(position,bounds.player_radius)||
           !std::isfinite(yaw)||unsigned(guard)>4||std::abs(position.y-bounds.floor_y)>.01)return false;
        auto &a=challengers_[id];auto &o=opponents_[id];if(o.health<=0)return false;
        a.position=position;a.yaw=yaw;a.moving=false;fixture_pose_hold_[id]=true;
        o.phase=recovery?TrainingEnemyPhase::Recovery:TrainingEnemyPhase::Measure;o.time=0;o.duration=10;
        o.guard=o.direction=guard;o.resolved=o.swoosh=false;o.seen_attack_serial=observed_serial;
        o.guard_reaction_pending=false;o.guard_reaction_at=o.guard_tracking_until=0;
        return true;
    }
    bool fixture_attack_pose(unsigned id,Vec3 position,float yaw,TrainingDirection direction){
        if(!fixture_geometry_pose(id,position,yaw,direction,0))return false;
        auto &o=opponents_[id];o.phase=TrainingEnemyPhase::Windup;o.time=0;o.duration=.30;o.attack_windup_seconds=.30;
        o.direction=direction;o.resolved=o.swoosh=false;++o.attack_start_serial;return true;
    }
    void fixture_release_geometry(unsigned id=challenger_count){if(id<challenger_count)fixture_pose_hold_[id]=false;else fixture_pose_hold_.fill(false);}
    WorldTrainingCombat view(const WorldTrainingCombat &fight,unsigned id)const{
        if(!npc_enabled(id))return WorldTrainingCombat{};
        auto result=fight.opponent_view(opponents_[id]);
        if(challengers_[id].state==ChallengerState::Waiting)result.state=TrainingState::Idle;
        return result;
    }
    unsigned active_count()const{unsigned n=0;for(unsigned i=0;i<config_.total;++i)n+=challengers_[i].state==ChallengerState::Active;return n;}
    unsigned entering_count()const{unsigned n=0;for(unsigned i=0;i<config_.total;++i)n+=challengers_[i].state==ChallengerState::Entering;return n;}
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
    void set_weapon(Weapon weapon){if(!world_weapon_loadout::valid(weapon))return;if(weapon_!=weapon){weapon_=weapon;cancel_draw();}}
    // Callers must also clear_buffer on weapon handoff (the runtime owns the
    // shared player's input). Starting a run always resets it itself.
    bool start(WorldTrainingCombat &fight,Config config,bool assist=false){
        if(!valid(config))return false;
        fight.reset_session();if(!fight.start_shared_arena(assist))return false;
        fight.weapon=weapon_;fight.difficulty=config.difficulty;config_=config;formation();clear_arrows();cancel_draw();events_.clear();entry_delay_.fill(0);
        ++generation_;current_=next_to_enter_=defeated_=shots_=arrow_hits_=ground_hits_=wall_hits_=0;
        teammates_defeated_=0;team_serial_=0;team_events_.clear();fill_slots();state_=State::Entering;project_focus(fight);return true;
    }
    bool start(WorldTrainingCombat &fight,bool assist=false){return start(fight,Config{},assist);}
    void cancel(WorldTrainingCombat &fight){
        fight.leave();fight.player_phase=TrainingPlayerPhase::Ready;fight.combo_count=0;fight.guard=false;
        formation();clear_arrows();cancel_draw();events_.clear();team_events_.clear();entry_delay_.fill(0);state_=State::Idle;defeated_=teammates_defeated_=0;
    }
    // This owns the SINGLE player update for arena mode. The ordinary legacy
    // fight.update must not also advance it (and explicitly refuses shared mode).
    void update(double dt,WorldTrainingCombat &fight,Vec3 player,float player_yaw,bool paused=false,unsigned requested_focus=opponent_capacity){
        if(!std::isfinite(dt)||dt<0||!finite(player)||!std::isfinite(player_yaw))return;
        events_.clear();team_events_.clear();fight.weapon=weapon_;
        if(paused){fight.guard=false;fight.clear_buffer();cancel_draw();return;}
        dt=std::min(dt,.25);double remaining=dt;
        std::array<Vec3,challenger_count> frame_roots={};for(unsigned i=0;i<challenger_count;++i)frame_roots[i]=challengers_[i].position;
        while(remaining>1e-10){const double step=std::min(remaining,1./240.);remaining-=step;update_step(step,fight,player,player_yaw,requested_focus);}
        // Float frame deltas often leave a sub-nanosecond fifth step. Publish
        // movement over the full frame, not that final almost-zero substep.
        for(unsigned i=0;i<challenger_count;++i){auto&a=challengers_[i];a.moving=(a.state==ChallengerState::Entering||a.state==ChallengerState::Active)&&std::hypot(a.position.x-frame_roots[i].x,a.position.z-frame_roots[i].z)>1.e-8;}
    }
private:
    void update_step(double dt,WorldTrainingCombat &fight,Vec3 player,float player_yaw,unsigned requested_focus){
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
        choose_team_targets(player);movement(dt,player,stopped,fight);choose_focus(player,requested_focus);
        std::array<TrainingTarget,challenger_count> targets={};std::size_t count=0;
        const double fx=std::sin(player_yaw),fz=std::cos(player_yaw);
        for(unsigned i=0;i<config_.total;++i)if(challengers_[i].state==ChallengerState::Active||challengers_[i].state==ChallengerState::Entering){
            const Vec3 delta=challengers_[i].position-player;const double distance=std::hypot(delta.x,delta.z);
            const double facing=distance>1e-9?(delta.x*fx+delta.z*fz)/distance:0;
            targets[count++]={&opponents_[i],i,distance,facing>=.35,facing>=.10,
                delta.x*std::cos(player_yaw)-delta.z*std::sin(player_yaw),delta.x*fx+delta.z*fz,true,targets_[i]==player_target,challengers_[i].state==ChallengerState::Active};
        }
        if(!fight.update_shared(dt,targets.data(),count,events_)){state_=State::Lost;fight.finish_shared_arena(false);return;}
        if(!stopped&&fight.state==TrainingState::Fighting)step_teams(dt,fight);
        state_=active_count()?State::Fighting:entering_count()?State::Entering:State::Intermission;
        projectiles(dt,fight,before);mark_defeated(player);
        if(fight.state==TrainingState::Lost||fight.state==TrainingState::Cancelled){state_=State::Lost;cancel_draw();clear_arrows();}
        else if(defeated_==config_.total){state_=State::Won;fight.finish_shared_arena(true);cancel_draw();clear_arrows();}
        else if(!active_count())state_=entering_count()?State::Entering:State::Intermission;
        choose_focus(player,requested_focus);project_focus(fight);
        if(drawing_){if(state_!=State::Fighting||fight.state!=TrainingState::Fighting||weapon_!=Weapon::Bow)cancel_draw();else draw_time_=std::min(maximum_draw,draw_time_+dt);}
    }
public:
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
            arrow={ArrowState::Flying,origin,origin,direction*((14+14*charge)/length),0,14+22*charge,generation_,player_target};
            fight.stamina-=cost;fight.regen_delay=.7;fight.set_guard(false);fight.clear_buffer();++shots_;return true;
        }
        return false;
    }
};
}
#endif
