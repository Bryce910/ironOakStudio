#ifndef VALWOOD_WORLD_JOUST_H
#define VALWOOD_WORLD_JOUST_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

// Standalone mounted practice rules. All coordinates are arena-local metres;
// rendering and persistence belong to the host, never to this simulation.
namespace world_joust {
constexpr float pi=3.14159265358979323846f;
constexpr double fixed_step=1.0/120.0;
constexpr float temporary_lance_length=4.8f;
constexpr float thrust_travel=.60f,thrust_anticipation=.10f,thrust_load_end=.12f,thrust_extend_end=.30f,thrust_active_end=.46f,thrust_duration=.82f;
inline float smooth_unit(float t){t=std::max(0.f,std::min(1.f,t));return t*t*(3.f-2.f*t);}
struct Vec3 { float x=0,y=0,z=0; };
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
inline float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float length(Vec3 a){return std::sqrt(dot(a,a));}
inline Vec3 normalized(Vec3 a){const float n=length(a);return n>1.e-6f?a*(1.f/n):Vec3{0,1,0};}
inline float finite_clamp(float v,float lo,float hi,float fallback=0){return std::isfinite(v)?std::max(lo,std::min(hi,v)):fallback;}
inline float approach(float a,float b,float maximum){return a+std::max(-maximum,std::min(maximum,b-a));}
inline float smooth_approach(float a,float b,float rate,float dt){return a+(b-a)*(1.f-std::exp(-rate*dt));}

struct Setup { int difficulty=1,passes=3; bool practice=false; };
struct Input { float throttle=0,steer=0,aim_x=0,aim_y=0; bool couch=false,guard=false,thrust=false,thrust_allowed=true; };
enum class Phase { Setup,Countdown,Charge,Result,MatchOver };
enum class Hit { Miss=0,Shield=1,Torso=2,Helmet=3 };
enum class ThrustPhase { Ready,Load,Extend,Hold,Recover };
inline const char *hit_name(Hit h){switch(h){case Hit::Shield:return "SHIELD";case Hit::Torso:return "BODY";case Hit::Helmet:return "HELMET";default:return "MISS";}}
inline const char *phase_name(Phase p){switch(p){case Phase::Countdown:return "READY";case Phase::Charge:return "CHARGE";case Phase::Result:return "PASS RESULT";case Phase::MatchOver:return "MATCH COMPLETE";default:return "JOUST SETUP";}}
struct Horse { Vec3 position; float yaw=0,speed=0,gait=0; };
struct Rider {
    Horse horse;
    Vec3 lance_rest_grip,lance_grip,lance_start,lance_tip,shield_center,torso_center,helmet_center;
    float aim_x=0,aim_y=0,couch_amount=0,guard_amount=0;
    bool couched=false;
    ThrustPhase thrust_phase=ThrustPhase::Ready;
    float thrust_amount=0,thrust_offset=0,thrust_time=0,thrust_recovery_from=1;
    bool thrust_active=false,thrust_armed=false;
    std::uint64_t thrust_serial=0;
    // Body lunge is separate from the authored palm: .25m torso motion plus
    // .35m arm reach produces the .60m stroke without stretching the arm.
    Vec3 body_offset;
    Hit hit_received=Hit::Miss;
    std::uint64_t hit_serial=0;
    float reaction_age=10,reaction_duration=0,reaction_strength=0,reaction_amount=0,reaction_pitch=0,reaction_roll=0;
    Vec3 reaction_direction,reaction_offset;
    Hit attack_target=Hit::Torso;float attack_commit=0;
    Vec3 fall_impact_direction,fall_angular_velocity;float fall_impact_strength=0,fall_landing_speed=0,fall_yaw_start=0,fall_yaw_target=0;
    bool unhorsed=false,fall_grounded=false,fall_settled=false;
    float fall_pose_progress=0,fall_settle_progress=0;
    Vec3 fall_position,fall_velocity,fall_body_local,fall_arm_local,fall_lance_local;
    float fall_age=0,fall_ground_time=0,fall_yaw=0,fall_pitch=0,fall_roll=0,fall_roll_target=0,fall_pitch_target=0,fall_initial_roll=0,fall_initial_pitch=0;
};
// The source rig's pelvis is the rotation pivot; the renderer retains its
// per-pose anatomy and may lift the final mesh to its exact floor support.
constexpr Vec3 mounted_pelvis{0,1.7864436f,-.1312653f};
constexpr float fall_floor_height=.30f;
// Nearest arena post face is |X|=5.48m. The authored rider plus
// worst impact lunge fits a 1.638m sphere, leaving >9cm at this bound.
constexpr float fall_lateral_limit=3.75f;
inline Vec3 rider_rotate(Vec3 p,float yaw,float pitch=0,float roll=0){
    const float cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll),cy=std::cos(yaw),sy=std::sin(yaw);
    const Vec3 a{p.x,p.y*cp-p.z*sp,p.y*sp+p.z*cp};
    const Vec3 b{a.x*cr-a.y*sr,a.x*sr+a.y*cr,a.z};
    return {b.x*cy+b.z*sy,b.y,b.z*cy-b.x*sy};
}
inline Vec3 rider_unrotate(Vec3 p,float yaw,float pitch=0,float roll=0){
    return {dot(p,rider_rotate({1,0,0},yaw,pitch,roll)),dot(p,rider_rotate({0,1,0},yaw,pitch,roll)),dot(p,rider_rotate({0,0,1},yaw,pitch,roll))};
}
inline Vec3 rider_pivot(const Rider &r){return r.unhorsed?r.fall_position:r.horse.position+rider_rotate(mounted_pelvis,r.horse.yaw)+r.reaction_offset;}
inline Vec3 rider_point(const Rider &r,Vec3 authored_local){
    return rider_pivot(r)+rider_rotate(authored_local-mounted_pelvis,r.unhorsed?r.fall_yaw:r.horse.yaw,
        r.unhorsed?r.fall_pitch:r.reaction_pitch,r.unhorsed?r.fall_roll:r.reaction_roll)+r.body_offset;
}
inline Vec3 rider_eye(const Rider &r){return rider_point(r,{0,2.88f,.04f});}
struct Contact {
    Hit hit=Hit::Miss;Vec3 point,normal;float speed=0;
    std::uint64_t thrust_serial=0;float thrust_time=0,thrust_amount=0,thrust_offset=0;bool active_thrust=false;
};
constexpr Vec3 shield_radii{.43f,.48f,.15f};
constexpr Vec3 torso_radii{.30f,.48f,.25f};
constexpr Vec3 helmet_radii{.23f,.26f,.23f};

// First contact of a translating point with a translating axis-aligned
// ellipsoid. Relative motion is exact within each fixed simulation step.
// The lance tip, not an aim cone or random roll, must touch the target.
inline bool swept_ellipsoid(Vec3 tip0,Vec3 tip1,Vec3 center0,Vec3 center1,
                            Vec3 radii,float &when,Vec3 &normal){
    if(!(radii.x>0&&radii.y>0&&radii.z>0))return false;
    const Vec3 a=tip0-center0,b=(tip1-tip0)-(center1-center0);
    const Vec3 q{a.x/radii.x,a.y/radii.y,a.z/radii.z};
    const Vec3 d{b.x/radii.x,b.y/radii.y,b.z/radii.z};
    const float aa=dot(d,d),bb=2.f*dot(q,d),cc=dot(q,q)-1.f;
    float t=0;
    if(cc>0){
        if(aa<1.e-12f)return false;
        const float discr=bb*bb-4.f*aa*cc;
        if(discr<0)return false;
        t=(-bb-std::sqrt(discr))/(2.f*aa);
        if(t<0||t>1)return false;
    }
    when=t;
    const Vec3 local=a+b*t;
    normal=normalized({local.x/(radii.x*radii.x),local.y/(radii.y*radii.y),local.z/(radii.z*radii.z)});
    return true;
}

class Game {
public:
    Setup setup;
    Phase phase=Phase::Setup;
    Rider player,opponent;
    Hit player_hit=Hit::Miss,opponent_hit=Hit::Miss;
    Contact player_contact,opponent_contact;
    int player_score=0,opponent_score=0,pass_index=0,reward_coins=0;
    float phase_time=0,elapsed=0;
    bool player_broken=false,opponent_broken=false,player_resolved=false,opponent_resolved=false;
    std::uint64_t contact_serial=0,thrust_serial=0;

    Game(){reset();}
    bool set_grip_offset(Vec3 local){
        if(phase!=Phase::Setup||!std::isfinite(local.x)||!std::isfinite(local.y)||!std::isfinite(local.z)||
           local.x<-.8f||local.x>.8f||local.y<1.6f||local.y>2.8f||local.z<-.2f||local.z>.8f)return false;
        grip_offset_=local;geometry(player,direction_,true);geometry(opponent,-direction_,false);return true;
    }
    Vec3 grip_offset()const{return grip_offset_;}
    static constexpr float lance_length(){return temporary_lance_length;}
    static constexpr float thrust_distance(){return thrust_travel;}
    void interrupt_thrust(){
        cancel_stroke(player);player.thrust_armed=false;
        geometry(player,direction_,true);
    }
    void configure(Setup requested){
        setup.difficulty=std::max(0,std::min(2,requested.difficulty));
        setup.passes=std::max(1,std::min(9,requested.passes));
        if((setup.passes&1)==0)--setup.passes;
        setup.practice=requested.practice;
        reset();
    }
    void reset(){
        phase=Phase::Setup;phase_time=elapsed=0;player_score=opponent_score=pass_index=reward_coins=0;
        reward_pending_=false;accumulator_=0;contact_serial=thrust_serial=0;player.thrust_serial=opponent.thrust_serial=player.hit_serial=opponent.hit_serial=0;prepare_pass();
    }
    void start(){
        player_score=opponent_score=pass_index=reward_coins=0;elapsed=phase_time=0;
        reward_pending_=false;accumulator_=0;contact_serial=thrust_serial=0;player.thrust_serial=opponent.thrust_serial=player.hit_serial=opponent.hit_serial=0;prepare_pass();phase=Phase::Countdown;
    }
    bool next_pass(){
        if(phase!=Phase::Result)return false;
        if(pass_index+1>=setup.passes){finish();return true;}
        ++pass_index;prepare_pass();phase=Phase::Countdown;phase_time=0;accumulator_=0;return true;
    }
    int winner()const{return player_score>opponent_score?1:(player_score<opponent_score?-1:0);}
    int consume_reward(){if(phase!=Phase::MatchOver||!reward_pending_)return 0;reward_pending_=false;return reward_coins;}
    float distance_to_opponent()const{return std::abs(player.horse.position.z-opponent.horse.position.z);}
    float countdown()const{return phase==Phase::Countdown?std::max(0.f,3.f-phase_time):0.f;}
    // Invalid time never advances simulation. A two-second bound prevents a
    // paused/debugger frame becoming a catch-up burst; ordinary slow frames
    // through 2s preserve every 120Hz contact and simultaneous decision.
    void update(double dt,Input input){
        if(!std::isfinite(dt)||dt<=0||phase==Phase::Setup||phase==Phase::MatchOver)return;
        input.throttle=finite_clamp(input.throttle,-1,1);input.steer=finite_clamp(input.steer,-1,1);
        input.aim_x=finite_clamp(input.aim_x,-1,1);input.aim_y=finite_clamp(input.aim_y,-1,1);
        accumulator_+=std::min(dt,2.0);
        while(accumulator_+1.e-10>=fixed_step){accumulator_-=fixed_step;tick(float(fixed_step),input);}
    }

private:
    Vec3 grip_offset_{-.35f,2.152761f,.29f};
    double accumulator_=0;
    bool reward_pending_=false;
    float direction_=1,opponent_guard_x_=0,opponent_guard_y_=0;

    void prepare_pass(){
        direction_=(pass_index&1)?-1.f:1.f;
        const auto p_serial=player.thrust_serial,o_serial=opponent.thrust_serial,p_hit=player.hit_serial,o_hit=opponent.hit_serial;
        player=Rider{};opponent=Rider{};player.thrust_serial=p_serial;opponent.thrust_serial=o_serial;player.hit_serial=p_hit;opponent.hit_serial=o_hit;
        player.horse.position={-1.7f*direction_,0,-40.f*direction_};
        opponent.horse.position={1.7f*direction_,0,40.f*direction_};
        player.horse.yaw=direction_>0?0:pi;opponent.horse.yaw=direction_>0?pi:0;
        player_hit=opponent_hit=Hit::Miss;player_contact=opponent_contact=Contact{};
        player_broken=opponent_broken=player_resolved=opponent_resolved=false;
        opponent_guard_x_=opponent_guard_y_=0;
        geometry(player,direction_,true);geometry(opponent,-direction_,false);
    }
    void finish(){
        phase=Phase::MatchOver;phase_time=0;
        reward_coins=(!setup.practice&&winner()>0)?10*setup.passes*(setup.difficulty+1)+2*player_score:0;
        reward_pending_=reward_coins>0;
    }
    void geometry(Rider &r,float forward,bool is_player){
        const float inward=is_player?direction_:-direction_;
        // Rider height is independent of presentation bob so collision never
        // flickers with the horse animation. All target centres are exposed.

        float guard_x=0,guard_y=0;
        if(is_player){guard_x=opponent.aim_x*.35f*r.guard_amount;guard_y=opponent.aim_y*.55f*r.guard_amount;}
        else {guard_x=opponent_guard_x_;guard_y=opponent_guard_y_;}
        const float skill=is_player?r.guard_amount:float(setup.difficulty)*.5f;
        const Vec3 shield_local{finite_clamp(.16f+guard_x,-.05f,.35f),finite_clamp(2.05f+.30f*skill+guard_y,2.05f,2.65f),.30f};
        const float full_reach=temporary_lance_length+thrust_travel;
        const float neutral_pitch=std::asin((2.35f-grip_offset_.y)/full_reach);
        const float neutral_yaw=std::asin(std::min(.98f,(3.4f-grip_offset_.x)/(full_reach*std::cos(neutral_pitch))));
        const float yaw=neutral_yaw+.22f*r.aim_x;
        const float upward_range=std::asin((3.08f-grip_offset_.y)/full_reach)-neutral_pitch;
        const float pitch=neutral_pitch+(r.aim_y>=0?upward_range:.17f)*r.aim_y;
        const Vec3 attack{inward*std::sin(yaw)*std::cos(pitch),std::sin(pitch),forward*std::cos(yaw)*std::cos(pitch)};
        const Vec3 raised{inward*.15f,.975f,forward*.16f};
        Vec3 lance_direction=normalized(raised*(1.f-r.couch_amount)+attack*r.couch_amount);
        if(r.unhorsed)lance_direction=rider_rotate(r.fall_lance_local,r.fall_yaw,r.fall_pitch,r.fall_roll);
        if(!r.unhorsed)r.body_offset=lance_direction*(r.thrust_offset<0?r.thrust_offset:r.thrust_offset*(.25f/thrust_travel));
        r.torso_center=rider_point(r,{0,2.35f,0});r.helmet_center=rider_point(r,{0,2.99f,0});
        r.shield_center=rider_point(r,shield_local);
        r.lance_rest_grip=rider_point(r,grip_offset_)-(r.unhorsed?Vec3{}:r.body_offset);
        r.lance_grip=r.lance_rest_grip+(r.unhorsed?rider_rotate(r.fall_arm_local,r.fall_yaw,r.fall_pitch,r.fall_roll):lance_direction*r.thrust_offset);
        r.lance_start=r.lance_grip;
        r.lance_tip=r.lance_start+lance_direction*temporary_lance_length;
        r.couched=r.couch_amount>=.88f;
    }
    static Contact strike(const Rider &old_attack,const Rider &attack,const Rider &old_target,const Rider &target,float speed){
        Contact result;float first=std::numeric_limits<float>::infinity();
        const float time0=old_attack.thrust_serial==attack.thrust_serial?old_attack.thrust_time:0.f,time1=attack.thrust_time;
        if(attack.thrust_serial==0||time1<=time0||time1<thrust_load_end||time0>thrust_active_end)return result;
        const float begin=std::max(0.f,(thrust_load_end-time0)/(time1-time0));
        const float end=std::min(1.f,(thrust_active_end-time0)/(time1-time0));
        if(begin>end)return result;
        const Vec3 tip0=old_attack.lance_tip+(attack.lance_tip-old_attack.lance_tip)*begin;
        const Vec3 tip1=old_attack.lance_tip+(attack.lance_tip-old_attack.lance_tip)*end;
        auto test=[&](Hit hit,Vec3 c0,Vec3 c1,Vec3 radii){
            float at=0;Vec3 normal;
            const Vec3 target0=c0+(c1-c0)*begin,target1=c0+(c1-c0)*end;
            if(swept_ellipsoid(tip0,tip1,target0,target1,radii,at,normal)){
                const float frame_at=begin+(end-begin)*at;
                if(frame_at>=first)return;
                first=frame_at;result={hit,old_attack.lance_tip+(attack.lance_tip-old_attack.lance_tip)*frame_at,normal,speed};
                result.thrust_serial=attack.thrust_serial;result.thrust_time=time0+(time1-time0)*frame_at;
                result.thrust_amount=old_attack.thrust_amount+(attack.thrust_amount-old_attack.thrust_amount)*frame_at;
                result.thrust_offset=old_attack.thrust_offset+(attack.thrust_offset-old_attack.thrust_offset)*frame_at;result.active_thrust=true;
            }
        };
        test(Hit::Shield,old_target.shield_center,target.shield_center,shield_radii);
        test(Hit::Torso,old_target.torso_center,target.torso_center,torso_radii);
        test(Hit::Helmet,old_target.helmet_center,target.helmet_center,helmet_radii);
        return result;
    }
    void record(Contact contact,bool is_player){
        if(contact.hit==Hit::Miss)return;
        ++contact_serial;
        Rider &r=is_player?player:opponent;
        if(!r.unhorsed){r.thrust_recovery_from=r.thrust_amount;r.thrust_time=thrust_active_end;r.thrust_phase=ThrustPhase::Recover;r.thrust_active=false;}
        else {cancel_stroke(r);r.thrust_armed=false;}
        receive(is_player?opponent:player,contact,is_player?-direction_:direction_);
        if(is_player){player_contact=contact;player_hit=contact.hit;player_resolved=true;player_broken=true;player_score+=int(contact.hit);}
        else {opponent_contact=contact;opponent_hit=contact.hit;opponent_resolved=true;opponent_broken=true;opponent_score+=int(contact.hit);}
    }
    static void cancel_stroke(Rider &r){
        r.thrust_phase=ThrustPhase::Ready;r.thrust_amount=r.thrust_offset=r.thrust_time=0;r.thrust_active=false;r.thrust_recovery_from=1;
    }
    void stroke(Rider &r,float dt,bool held,bool allowed,bool is_player){
        if(!allowed){cancel_stroke(r);r.thrust_armed=false;return;}
        if(r.thrust_phase==ThrustPhase::Ready){
            if(!held)r.thrust_armed=true;
            if(!held||!r.thrust_armed)return;
            r.thrust_armed=false;r.thrust_phase=ThrustPhase::Load;r.thrust_time=0;r.thrust_recovery_from=1;
            ++r.thrust_serial;if(is_player)++thrust_serial;
        }
        r.thrust_armed=false;r.thrust_time+=dt;
        const float age=r.thrust_time;
        if(age>=thrust_duration){cancel_stroke(r);return;}
        if(age<thrust_load_end){r.thrust_phase=ThrustPhase::Load;r.thrust_offset=-thrust_anticipation*smooth_unit(age/thrust_load_end);}
        else if(age<thrust_extend_end){r.thrust_phase=ThrustPhase::Extend;r.thrust_offset=-thrust_anticipation+(thrust_travel+thrust_anticipation)*smooth_unit((age-thrust_load_end)/(thrust_extend_end-thrust_load_end));}
        else if(age<thrust_active_end){r.thrust_phase=ThrustPhase::Hold;r.thrust_offset=thrust_travel;}
        else {r.thrust_phase=ThrustPhase::Recover;r.thrust_offset=thrust_travel*r.thrust_recovery_from*(1.f-smooth_unit((age-thrust_active_end)/(thrust_duration-thrust_active_end)));}
        r.thrust_amount=std::max(0.f,r.thrust_offset/thrust_travel);
        r.thrust_active=age>=thrust_load_end&&age<=thrust_active_end;
    }
    void receive(Rider &r,const Contact &contact,float forward){
        r.hit_received=contact.hit;++r.hit_serial;r.reaction_age=0;
        r.reaction_duration=contact.hit==Hit::Shield?.50f:.72f;
        r.reaction_strength=contact.hit==Hit::Shield?.58f:(contact.hit==Hit::Torso?1.f:1.25f);
        r.reaction_direction=normalized(contact.normal*-1.f);
        if(contact.hit!=Hit::Helmet||r.unhorsed)return;
        r.fall_position=rider_pivot(r);r.fall_yaw_start=r.fall_yaw=r.horse.yaw;
        r.fall_impact_direction=r.reaction_direction;
        r.fall_impact_strength=finite_clamp((contact.speed-9.f)/14.f,0,1);
        const Vec3 impulse_local=rider_unrotate(r.fall_impact_direction,r.horse.yaw);
        r.fall_yaw_target=r.horse.yaw+finite_clamp(impulse_local.x*.10f+impulse_local.z*.04f,-.12f,.12f);
        const float outward=forward>0?-1.f:1.f;
        r.fall_velocity={outward*(1.85f+.9f*r.fall_impact_strength),2.2f+1.05f*r.fall_impact_strength,forward*r.horse.speed*(.48f+.10f*r.fall_impact_strength)};
        r.fall_body_local=rider_unrotate(r.body_offset,r.fall_yaw,r.reaction_pitch,r.reaction_roll);
        r.fall_arm_local=rider_unrotate(r.lance_grip-rider_point(r,grip_offset_),r.fall_yaw,r.reaction_pitch,r.reaction_roll);
        r.fall_lance_local=rider_unrotate(normalized(r.lance_tip-r.lance_grip),r.fall_yaw,r.reaction_pitch,r.reaction_roll);
        r.fall_initial_pitch=r.fall_pitch=r.reaction_pitch;r.fall_initial_roll=r.fall_roll=r.reaction_roll;
        r.unhorsed=true;r.fall_grounded=false;r.fall_age=r.fall_ground_time=0;
        r.fall_roll_target=outward*forward>0?-pi*.5f:pi*.5f;
        r.fall_pitch_target=-.15f+finite_clamp(impulse_local.z*.05f,-.05f,.05f);
        cancel_stroke(r);r.thrust_armed=false;
    }
    static void reaction_step(Rider &r,float dt){
        r.reaction_age+=dt;
        const float age=r.reaction_age;
        const float envelope=age<.10f?smooth_unit(age/.10f):1.f-smooth_unit((age-.10f)/std::max(.01f,r.reaction_duration-.10f));
        r.reaction_amount=r.reaction_strength*envelope;
        const Vec3 local=rider_rotate(r.reaction_direction,-r.horse.yaw);
        r.reaction_pitch=local.z*.25f*r.reaction_amount;r.reaction_roll=-local.x*.22f*r.reaction_amount;
        r.reaction_offset=r.reaction_direction*(.12f*r.reaction_amount);
        if(!r.unhorsed)return;
        r.fall_age+=dt;
        // Brake the physical outward trajectory before the actual source
        // silhouette can reach the spectator rail; camera and dust use this
        // same pelvis position. The final clamp is only a numerical guard.
        if(r.fall_position.x*r.fall_velocity.x>0){
            const float distance=std::max(0.f,fall_lateral_limit-std::abs(r.fall_position.x));
            const float allowed=std::sqrt(2.f*4.f*distance);
            r.fall_velocity.x=std::copysign(std::min(std::abs(r.fall_velocity.x),allowed),r.fall_velocity.x);
        }
        r.fall_pose_progress=smooth_unit(r.fall_age/.95f);
        if(!r.fall_grounded){
            r.fall_velocity.y-=9.81f*dt;r.fall_position=r.fall_position+r.fall_velocity*dt;
            const float t=smooth_unit(r.fall_age/.90f);
            r.fall_roll=r.fall_initial_roll+(r.fall_roll_target-r.fall_initial_roll)*t+.08f*r.fall_impact_direction.y*std::sin(pi*t);
            r.fall_pitch=r.fall_initial_pitch+(r.fall_pitch_target-r.fall_initial_pitch)*t;
            r.fall_yaw=r.fall_yaw_start+(r.fall_yaw_target-r.fall_yaw_start)*smooth_unit(r.fall_age/.85f);
            if(r.fall_position.y<=fall_floor_height){
                r.fall_position.y=fall_floor_height;r.fall_landing_speed=std::abs(r.fall_velocity.y);r.fall_velocity.y=0;r.fall_grounded=true;
                r.fall_angular_velocity={finite_clamp(r.fall_impact_direction.z*.30f*r.fall_impact_strength,-.20f,.20f),0,finite_clamp(r.fall_impact_direction.x*.20f*r.fall_impact_strength,-.12f,.12f)};
            }
        }else{
            r.fall_ground_time+=dt;
            r.fall_velocity.x=approach(r.fall_velocity.x,0,5.f*dt);r.fall_velocity.z=approach(r.fall_velocity.z,0,6.f*dt);
            r.fall_position=r.fall_position+r.fall_velocity*dt;
            auto settle_angle=[&](float &angle,float target,float &velocity){velocity+=(-35.f*(angle-target)-12.f*velocity)*dt;angle+=velocity*dt;};
            settle_angle(r.fall_pitch,r.fall_pitch_target,r.fall_angular_velocity.x);
            settle_angle(r.fall_roll,r.fall_roll_target,r.fall_angular_velocity.z);
            settle_angle(r.fall_yaw,r.fall_yaw_target,r.fall_angular_velocity.y);
        }
        r.fall_settle_progress=r.fall_grounded?smooth_unit(r.fall_ground_time/.65f):0;
        r.fall_settled=r.fall_grounded&&r.fall_ground_time>=.65f;
        if(std::abs(r.fall_position.x)>=fall_lateral_limit){
            r.fall_position.x=finite_clamp(r.fall_position.x,-fall_lateral_limit,fall_lateral_limit);
            if(r.fall_position.x*r.fall_velocity.x>0)r.fall_velocity.x=0;
        }
        r.fall_position.z=finite_clamp(r.fall_position.z,-58.f,58.f);
        r.body_offset=rider_rotate(r.fall_body_local,r.fall_yaw,r.fall_pitch,r.fall_roll)*(1.f-smooth_unit(r.fall_age/.28f));
    }
    void tick(float dt,Input input){
        if(phase==Phase::Setup||phase==Phase::MatchOver)return;
        phase_time+=dt;elapsed+=dt;
        reaction_step(player,dt);reaction_step(opponent,dt);
        if(phase==Phase::Result){
            player.horse.speed=approach(player.horse.speed,0,7.f*dt);opponent.horse.speed=approach(opponent.horse.speed,0,7.f*dt);
            player.horse.position.z=finite_clamp(player.horse.position.z+direction_*player.horse.speed*dt,-58.f,58.f);
            opponent.horse.position.z=finite_clamp(opponent.horse.position.z-direction_*opponent.horse.speed*dt,-58.f,58.f);
            player.horse.gait+=player.horse.speed*dt*.85f;opponent.horse.gait+=opponent.horse.speed*dt*.85f;
            cancel_stroke(player);cancel_stroke(opponent);player.thrust_armed=opponent.thrust_armed=false;
            player.couch_amount=approach(player.couch_amount,0,2.f*dt);opponent.couch_amount=approach(opponent.couch_amount,0,2.f*dt);
            geometry(player,direction_,true);geometry(opponent,-direction_,false);
            if(pass_index+1>=setup.passes&&phase_time>=2.8f)finish();
            return;
        }
        const Rider old_player=player,old_opponent=opponent;
        player.aim_x=smooth_approach(player.aim_x,input.aim_x,10.f,dt);player.aim_y=smooth_approach(player.aim_y,input.aim_y,10.f,dt);
        player.guard_amount=approach(player.guard_amount,input.guard?1.f:0.f,3.f*dt);
        player.couch_amount=approach(player.couch_amount,!player_broken?1.f:0.f,3.8f*dt);
        const float pattern=float(pass_index+1);
        const float skill=float(setup.difficulty);
        opponent.aim_x=.19f*std::sin(pattern*1.71f)/(1.f+skill);
        // Every three-pass cycle offers a readable high-line attack on all
        // difficulties. Easy raises slowly and launches later; a raised shield
        // can physically intercept it, with no invisible hit/defence roll.
        const bool head_pass=pass_index%3==1;
        opponent.attack_target=head_pass?Hit::Helmet:Hit::Torso;
        const float signed_gap=(opponent.horse.position.z-player.horse.position.z)*direction_;
        const float commit_distance=setup.difficulty==0?16.f:(setup.difficulty==1?19.f:22.f);
        const bool commit_head=head_pass&&phase==Phase::Charge&&signed_gap<=commit_distance;
        opponent.attack_commit=commit_head?finite_clamp((commit_distance-signed_gap)/(commit_distance-7.f),0,1):0;
        const float target_y=commit_head?.85f:-.30f+.25f*skill+.13f*std::sin(pattern*2.37f);
        opponent.aim_y=smooth_approach(opponent.aim_y,target_y,setup.difficulty==0?4.f:(setup.difficulty==1?6.f:8.f),dt);
        if(head_pass)opponent.aim_x=0;
        opponent.couch_amount=approach(opponent.couch_amount,!opponent_broken?1.f:0.f,2.8f*dt);
        opponent.guard_amount=1;
        const float guard_strength=setup.difficulty==0?0.f:(setup.difficulty==1?.45f:.92f);
        // Difficulty changes a physical shield's delayed tracking, never an
        // invisible hit rejection. Late aim changes can go around the shield.
        opponent_guard_x_=smooth_approach(opponent_guard_x_,player.aim_x*.38f*guard_strength,setup.difficulty==2?4.6f:2.3f,dt);
        opponent_guard_y_=smooth_approach(opponent_guard_y_,player.aim_y*.58f*guard_strength,setup.difficulty==2?4.6f:2.3f,dt);
        const bool request=input.thrust||input.couch;
        // A broken lance cannot start another stroke, but its committed
        // recoil completes smoothly instead of snapping the hand back.
        if(!player.unhorsed&&(!player_broken||player.thrust_phase!=ThrustPhase::Ready))
            stroke(player,dt,request&&!player_broken,input.thrust_allowed,true);
        else player.thrust_armed=false;
        const float signed_separation=(opponent.horse.position.z-player.horse.position.z)*direction_;
        const float predicted_trigger=3.65f+(player.horse.speed+opponent.horse.speed)*.28f;
        const bool ai_request=phase==Phase::Charge&&signed_separation>0&&signed_separation<=predicted_trigger;
        if(!opponent.unhorsed&&(!opponent_broken||opponent.thrust_phase!=ThrustPhase::Ready))
            stroke(opponent,dt,ai_request&&!opponent_broken,true,false);
        else opponent.thrust_armed=false;
        if(phase==Phase::Countdown){
            geometry(player,direction_,true);geometry(opponent,-direction_,false);
            if(phase_time+1.e-5f>=3.f){phase=Phase::Charge;phase_time=0;}
            return;
        }
        const bool player_passed=player.horse.position.z*direction_>=48.f;
        const bool opponent_passed=opponent.horse.position.z*-direction_>=48.f;
        if(player.unhorsed)input.throttle=input.steer=0; // the loose horse runs on without rider control
        const float pace=input.throttle>=0?8.6f+input.throttle*2.8f:8.6f+input.throttle*6.f;
        player.horse.speed=approach(player.horse.speed,player_passed?0:pace,3.2f*dt);
        opponent.horse.speed=approach(opponent.horse.speed,opponent_passed?0:8.2f+.55f*skill,3.1f*dt);
        player.horse.position.z+=direction_*player.horse.speed*dt;
        opponent.horse.position.z-=direction_*opponent.horse.speed*dt;
        const float lane_center=-1.7f*direction_;
        player.horse.position.x=finite_clamp(player.horse.position.x+direction_*input.steer*.55f*dt,lane_center-.32f,lane_center+.32f,lane_center);
        opponent.horse.position.x=1.7f*direction_+.07f*std::sin(phase_time*.75f+pattern)*direction_;
        player.horse.position.z=finite_clamp(player.horse.position.z,-58.f,58.f);
        opponent.horse.position.z=finite_clamp(opponent.horse.position.z,-58.f,58.f);
        // Only a small visible steering angle; horses never turn through the
        // central barrier or leave the runout under a long or invalid frame.
        const float player_yaw=(direction_>0?0:pi)+input.steer*.065f;
        player.horse.yaw=smooth_approach(player.horse.yaw,player_yaw,8.f,dt);
        opponent.horse.yaw=direction_>0?pi:0;
        player.horse.gait+=player.horse.speed*dt*.85f;opponent.horse.gait+=opponent.horse.speed*dt*.85f;
        geometry(player,direction_,true);geometry(opponent,-direction_,false);
        const float closing_speed=player.horse.speed+opponent.horse.speed;
        // Read both contacts from the same pre-recoil geometry. One rider's
        // valid hit cannot erase the other's simultaneous lance contact.
        Contact a,b;
        if(!player.unhorsed&&!opponent.unhorsed&&!player_resolved&&player.couched&&player.horse.speed>=4.2f&&closing_speed>=9.f&&player.thrust_serial>0)
            a=strike(old_player,player,old_opponent,opponent,closing_speed);
        if(!opponent.unhorsed&&!player.unhorsed&&!opponent_resolved&&opponent.couched&&opponent.horse.speed>=4.2f&&closing_speed>=9.f&&opponent.thrust_serial>0)
            b=strike(old_opponent,opponent,old_player,player,closing_speed);
        record(a,true);record(b,false);
        if(player.horse.position.z*direction_>opponent.horse.position.z*direction_+6.f){
            player_resolved=opponent_resolved=true;
        }
        const bool fall_complete=(player.unhorsed||opponent.unhorsed)&&(!player.unhorsed||(player.fall_grounded&&player.fall_ground_time>=1.35f))&&(!opponent.unhorsed||(opponent.fall_grounded&&opponent.fall_ground_time>=1.35f));
        if(fall_complete||(player_passed&&opponent_passed)||phase_time>=36.f){
            player_resolved=opponent_resolved=true;phase=Phase::Result;phase_time=0;
        }
    }
};
} // namespace world_joust
#endif
