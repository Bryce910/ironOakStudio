#include "engine/world_joust.h"
#include <cstdio>
#include <limits>
#include <vector>
using namespace world_joust;
static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){std::fprintf(stderr,"FAIL joust line %d: %s\n",__LINE__,#v);return 1;}}while(0)
static bool close(float a,float b,float epsilon=1.e-5f){return std::abs(a-b)<=epsilon;}
static bool close(Vec3 a,Vec3 b,float epsilon=1.e-5f){return length(a-b)<=epsilon;}
static Vec3 local_point(const Rider &r,Vec3 world){
    const float yaw=r.unhorsed?r.fall_yaw:r.horse.yaw,pitch=r.unhorsed?r.fall_pitch:r.reaction_pitch,roll=r.unhorsed?r.fall_roll:r.reaction_roll;
    const Vec3 p=world-rider_pivot(r)-r.body_offset;
    return mounted_pelvis+Vec3{dot(p,rider_rotate({1,0,0},yaw,pitch,roll)),dot(p,rider_rotate({0,1,0},yaw,pitch,roll)),dot(p,rider_rotate({0,0,1},yaw,pitch,roll))};
}
static Vec3 target(const Rider &r,Hit hit){return hit==Hit::Shield?r.shield_center:(hit==Hit::Helmet?r.helmet_center:r.torso_center);}
static Vec3 radii(Hit hit){return hit==Hit::Shield?shield_radii:(hit==Hit::Helmet?helmet_radii:torso_radii);}
// Determine input-event times from ordinary horse motion, then split every
// caller frame at those same fixed-tick event boundaries. This compares
// identical control streams at 120Hz, 30Hz, and one-second render hitches.
static std::vector<int> pulse_schedule(Setup setup,Input input,float distance=9){
    Game g;g.configure(setup);g.start();input.thrust=input.couch=false;
    std::vector<int> result(std::size_t(g.setup.passes),-1);int tick=0;
    for(int i=0;i<30000&&g.phase!=Phase::MatchOver;++i){
        if(g.phase==Phase::Charge&&g.distance_to_opponent()<=distance&&result[std::size_t(g.pass_index)]<0)result[std::size_t(g.pass_index)]=tick;
        g.update(fixed_step,input);++tick;
        if(g.phase==Phase::Result){g.next_pass();tick=0;}
    }
    return result;
}
static Game match(Setup setup,Input input,int batch=1,bool attack=true,float distance=9){
    const auto schedule=pulse_schedule(setup,input,distance);Game g;g.configure(setup);g.start();int tick=0;
    for(int i=0;i<30000&&g.phase!=Phase::MatchOver;++i){
        const int pulse=schedule[std::size_t(g.pass_index)];int count=batch;
        if(tick<pulse)count=std::min(count,pulse-tick);
        input.thrust=attack&&tick>=pulse;input.couch=false;
        g.update(double(count)*fixed_step,input);tick+=count;
        if(g.phase==Phase::Result){g.next_pass();tick=0;}
    }
    return g;
}
int main(){
    Game fresh;CHECK(fresh.phase==Phase::Setup);CHECK(fresh.player_score==0&&fresh.consume_reward()==0);
    CHECK(close(fresh.player.horse.position,{-1.7f,0,-40}));CHECK(close(fresh.opponent.horse.position,{1.7f,0,40}));
    CHECK(close(length(fresh.player.lance_tip-fresh.player.lance_start),Game::lance_length()));
    CHECK(close(fresh.player.lance_start,{-2.05f,2.152761f,-39.71f}));CHECK(close(Game::thrust_distance(),.60f));
    CHECK(fresh.set_grip_offset({-.35f,2.15f,.29f}));CHECK(close(fresh.grip_offset(),{-.35f,2.15f,.29f}));
    CHECK(!fresh.set_grip_offset({0,9,0}));CHECK(!fresh.set_grip_offset({std::numeric_limits<float>::infinity(),2,0}));
    fresh.configure({999,8,false});CHECK(fresh.setup.difficulty==2&&fresh.setup.passes==7);
    fresh.configure({-8,-2,true});CHECK(fresh.setup.difficulty==0&&fresh.setup.passes==1&&fresh.setup.practice);
    fresh.start();CHECK(!fresh.set_grip_offset({0,2,0}));CHECK(fresh.phase==Phase::Countdown);
    const float before=fresh.elapsed;fresh.update(-1,{});fresh.update(std::numeric_limits<double>::quiet_NaN(),{});fresh.update(std::numeric_limits<double>::infinity(),{});CHECK(fresh.elapsed==before);
    fresh.update(2,{});fresh.update(1,{});CHECK(fresh.phase==Phase::Charge);CHECK(fresh.countdown()==0);
    CHECK(fresh.player.horse.speed==0&&fresh.opponent.horse.speed==0);
    fresh.reset();CHECK(fresh.phase==Phase::Setup&&fresh.elapsed==0&&fresh.contact_serial==0&&fresh.thrust_serial==0);

    // Exact geometry, including whole-target tunnelling between endpoints.
    float t=0;Vec3 n;
    CHECK(swept_ellipsoid({0,0,-100},{0,0,100},{0,0,0},{0,0,0},{.2f,.3f,.4f},t,n));
    CHECK(close(t,.498f,2.e-5f)&&close(n,{0,0,-1},1.e-4f));
    CHECK(!swept_ellipsoid({.21f,0,-100},{.21f,0,100},{0,0,0},{0,0,0},{.2f,.3f,.4f},t,n));
    CHECK(swept_ellipsoid({0,0,0},{0,0,0},{0,0,1},{0,0,-1},{.25f,.25f,.25f},t,n));CHECK(close(t,.375f));
    CHECK(swept_ellipsoid({0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,1,1},t,n)&&t==0);
    CHECK(!swept_ellipsoid({2,0,0},{2,0,0},{0,0,0},{0,0,0},{1,1,1},t,n));
    CHECK(!swept_ellipsoid({0,0,-1},{0,0,1},{0,0,0},{0,0,0},{0,1,1},t,n));
    for(int i=-32;i<=32;++i){float u;Vec3 m;const Vec3 shift{float(i)*71,float(i)*13,float(i)*57};
        CHECK(swept_ellipsoid(Vec3{0,0,-2}+shift,Vec3{0,0,2}+shift,shift,shift,{.5f,.6f,.7f},u,m));
        CHECK(close(u,.325f,1.e-5f)&&close(m,{0,0,-1},1.e-5f));
    }

    // Ready aiming moves the real lance in all four directions, without any
    // attack. Initial held RT needs neutral; one-shot strokes do not queue.
    Game control;control.start();Input in;in.thrust=true;
    control.update(.5,in);CHECK(control.thrust_serial==0&&!control.player.thrust_armed);
    in.thrust=false;control.update(.5,in);CHECK(control.player.thrust_armed&&control.player.couched);
    const Vec3 center=control.player.lance_tip-control.player.lance_rest_grip;
    for(int axis=0;axis<4;++axis){Game aim;aim.start();Input a;a.aim_x=axis==0?-1.f:(axis==1?1.f:0.f);a.aim_y=axis==2?-1.f:(axis==3?1.f:0.f);aim.update(2,a);
        const Vec3 direction=aim.player.lance_tip-aim.player.lance_rest_grip;
        CHECK(axis==0?direction.x<center.x:(axis==1?direction.x>center.x:(axis==2?direction.y<center.y:direction.y>center.y)));
        CHECK(aim.player.thrust_serial==0&&aim.player.thrust_amount==0&&aim.player.thrust_armed);
    }
    in.thrust=true;float maximum=0,previous=0,minimum_offset=0,maximum_body=0;
    for(int i=0;i<104;++i){control.update(fixed_step,in);const auto &r=control.player;
        maximum=std::max(maximum,r.thrust_amount);minimum_offset=std::min(minimum_offset,r.thrust_offset);maximum_body=std::max(maximum_body,length(r.body_offset));CHECK(r.thrust_amount>=0&&r.thrust_amount<=1);
        CHECK(close(length(r.lance_tip-r.lance_start),4.8f,2.e-5f));
        const Vec3 axis=normalized(r.lance_tip-r.lance_start);
        CHECK(close(r.lance_grip-r.lance_rest_grip,axis*r.thrust_offset,2.e-5f));
        CHECK(close(r.lance_start,r.lance_grip));
        CHECK(length(r.lance_grip-rider_point(r,control.grip_offset()))<=.35002f);
        if(i<29)CHECK(r.thrust_amount>=previous-1.e-6f);
        previous=r.thrust_amount;
        CHECK(r.thrust_active==(r.thrust_phase==ThrustPhase::Extend||r.thrust_phase==ThrustPhase::Hold));
    }
    CHECK(close(minimum_offset,-.10f,.001f)&&close(maximum_body,.25f));
    CHECK(maximum==1&&control.thrust_serial==1&&control.player.thrust_serial==1);
    CHECK(control.player.thrust_phase==ThrustPhase::Ready&&control.player.thrust_amount==0&&!control.player.thrust_armed);
    control.update(.5,in);CHECK(control.thrust_serial==1); // held long after recovery
    in.thrust=false;control.update(fixed_step,in);CHECK(control.player.thrust_armed);
    in.thrust=true;control.update(fixed_step,in);CHECK(control.thrust_serial==2);
    in.thrust=false;control.update(.1,in);in.thrust=true;control.update(.8,in);CHECK(control.thrust_serial==2&&!control.player.thrust_armed);
    control.interrupt_thrust();CHECK(control.player.thrust_phase==ThrustPhase::Ready&&!control.player.thrust_armed&&control.player.thrust_amount==0);
    in.thrust=false;in.thrust_allowed=false;control.update(.1,in);CHECK(!control.player.thrust_armed);
    in.thrust=true;control.update(.1,in);CHECK(control.thrust_serial==2);
    in.thrust_allowed=true;control.update(.1,in);CHECK(control.thrust_serial==2); // resume with held RT
    in.thrust=false;control.update(fixed_step,in);in.thrust=true;control.update(.2,in);CHECK(control.thrust_serial==3&&control.player.thrust_active);
    control.interrupt_thrust();CHECK(control.player.thrust_amount==0&&!control.player.thrust_active&&!control.player.thrust_armed);
    // Compatibility keyboard/legacy couch flag is the exact same one-shot.
    Game alias;alias.start();alias.update(fixed_step,{});Input old;old.couch=true;alias.update(.8,old);CHECK(alias.thrust_serial==1);alias.update(.8,old);CHECK(alias.thrust_serial==1);

    // Complete passes use real mounted motion and fresh timed inputs only.
    Input neutral;auto easy=match({0,1,false},neutral);CHECK(easy.phase==Phase::MatchOver&&easy.player_hit==Hit::Shield&&easy.player_score==1);
    Input high;high.aim_y=.8f;
    auto helmet=match({0,1,false},high);CHECK(helmet.player_hit==Hit::Helmet&&helmet.player_score==3&&helmet.winner()==1);
    CHECK(helmet.player_broken&&helmet.player_resolved&&helmet.contact_serial>=1);
    CHECK(helmet.player_contact.active_thrust&&helmet.player_contact.thrust_serial==1&&helmet.player_contact.speed>=9);
    CHECK(helmet.player_contact.thrust_time>=thrust_load_end&&helmet.player_contact.thrust_time<=thrust_active_end);
    CHECK(helmet.reward_coins==16&&helmet.consume_reward()==16&&helmet.consume_reward()==0);
    helmet.update(100,neutral);CHECK(helmet.consume_reward()==0&&helmet.player_score==3);
    auto practice=match({0,1,true},high);CHECK(practice.winner()==1&&practice.reward_coins==0&&practice.consume_reward()==0);
    auto passive=match({0,1,false},high,1,false);CHECK(passive.player_hit==Hit::Miss&&passive.player_score==0&&!passive.player_broken&&passive.thrust_serial==0);
    auto early=match({0,1,false},high,1,true,80);CHECK(early.player_hit==Hit::Miss&&early.player_score==0&&early.thrust_serial==1);
    auto late=match({0,1,false},high,1,true,1);CHECK(late.player_hit==Hit::Miss&&late.player_score==0&&late.thrust_serial==1);
    Input far;far.aim_x=1;auto missed=match({0,1,false},far);CHECK(missed.player_hit==Hit::Miss&&missed.player_score==0&&!missed.player_broken);
    auto hard=match({2,1,false},high);CHECK(hard.player_hit==Hit::Shield&&hard.opponent_score>=2&&hard.winner()==-1);
    // The hard shield has real edges: a carefully angled helmet shot can go
    // around its side. Guarding still physically intercepts the rival lance.
    Input skilled;skilled.aim_x=.1f;skilled.aim_y=1;skilled.guard=true;
    auto skilled_hard=match({2,1,false},skilled);CHECK(skilled_hard.player_hit==Hit::Helmet&&skilled_hard.opponent_hit==Hit::Shield&&skilled_hard.winner()==1);
    CHECK(skilled_hard.reward_coins==36&&skilled_hard.consume_reward()==36&&skilled_hard.consume_reward()==0);

    // Same timed inputs are independent of render cadence across all three
    // difficulty settings and an aim grid, including misses and shields.
    for(int difficulty=0;difficulty<3;++difficulty)for(int y=-2;y<=5;++y)for(int x=-5;x<=5;++x){
        Input a;a.aim_x=float(x)*.2f;a.aim_y=float(y)*.2f;
        auto one=match({difficulty,1,false},a,1),four=match({difficulty,1,false},a,4);
        CHECK(one.phase==Phase::MatchOver&&four.phase==Phase::MatchOver);
        CHECK(one.player_hit==four.player_hit&&one.opponent_hit==four.opponent_hit&&one.contact_serial==four.contact_serial);
        CHECK(one.player_score==int(one.player_hit)&&one.opponent_score==int(one.opponent_hit));
        CHECK(one.player_score==four.player_score&&one.opponent_score==four.opponent_score);
        CHECK(close(one.player_contact.point,four.player_contact.point)&&close(one.opponent_contact.point,four.opponent_contact.point));
        CHECK(one.contact_serial<=2&&one.player_score<=3&&one.opponent_score<=3&&one.thrust_serial==1);
    }
    // Reconstruct each score from the actual old/new tip and target sweep.
    // The receipt retains the active stroke time even after impact recoil.
    for(int difficulty=0;difficulty<3;++difficulty)for(float aim_y:{0.f,.45f,.8f}){
        Game g;g.configure({difficulty,3,false});g.start();Input a;a.aim_y=aim_y;int p_points=0,o_points=0,passes=0;bool fired=false;
        for(int i=0;i<30000&&g.phase!=Phase::MatchOver;++i){
            if(!fired&&g.phase==Phase::Charge&&g.distance_to_opponent()<=9){a.thrust=true;fired=true;}
            const Rider p0=g.player,o0=g.opponent;const auto serial=g.contact_serial;const auto ph=g.phase;
            const Hit old_p=g.player_hit,old_o=g.opponent_hit;g.update(fixed_step,a);
            if(g.contact_serial!=serial){
                for(int side=0;side<2;++side){const auto &c=side?g.opponent_contact:g.player_contact;const Hit old_hit=side?old_o:old_p;
                    if(old_hit!=Hit::Miss||c.hit==Hit::Miss)continue;
                    const Rider &a0=side?o0:p0,&a1=side?g.opponent:g.player,&b0=side?p0:o0,&b1=side?g.player:g.opponent;
                    float at;Vec3 normal;CHECK(ph==Phase::Charge);
                    CHECK(swept_ellipsoid(a0.lance_tip,a1.lance_tip,target(b0,c.hit),target(b1,c.hit),radii(c.hit),at,normal));
                    CHECK(close(c.point,a0.lance_tip+(a1.lance_tip-a0.lance_tip)*at,2.e-4f));CHECK(close(c.normal,normal,2.e-4f));
                    CHECK(c.active_thrust&&c.thrust_serial==a1.thrust_serial&&c.thrust_time>=thrust_load_end&&c.thrust_time<=thrust_active_end);
                    CHECK(c.thrust_amount>=0&&c.thrust_amount<=1&&(a1.thrust_phase==ThrustPhase::Recover||a1.unhorsed)&&!a1.thrust_active);
                }
            }
            CHECK(std::abs(g.player.horse.position.x)>=1.37f&&std::abs(g.opponent.horse.position.x)>=1.6f);
            CHECK(std::abs(g.player.horse.position.z)<=58.001f&&std::abs(g.opponent.horse.position.z)<=58.001f);
            CHECK(close(length(g.player.lance_tip-g.player.lance_start),Game::lance_length(),2.e-5f));
            CHECK(g.player.horse.speed<=11.401f&&g.opponent.horse.speed<=9.301f);
            for(const Rider *r:{&g.player,&g.opponent}){
                const Vec3 c=local_point(*r,r->shield_center);
                CHECK(c.x>=-.05001f&&c.x<=.35001f&&c.y>=2.04999f&&c.y<=2.65001f);
                CHECK(std::abs(.35f-c.x)<=shield_radii.x&&std::abs(2.35f-c.y)<=shield_radii.y);
                CHECK(close(local_point(*r,r->torso_center),{0,2.35f,0},2.e-5f));
                CHECK(close(local_point(*r,r->helmet_center),{0,2.99f,0},2.e-5f));
            }
            if(g.phase==Phase::Result){
                ++passes;p_points+=int(g.player_hit);o_points+=int(g.opponent_hit);const int previous=g.pass_index;const auto starts=g.player.thrust_serial;CHECK(g.next_pass());
                if(g.phase!=Phase::MatchOver){CHECK(g.pass_index==previous+1&&g.phase==Phase::Countdown&&g.player.horse.speed==0);CHECK(g.player.thrust_serial==starts&&!g.player.thrust_armed&&g.player.thrust_amount==0);}
                fired=false;a.thrust=false;
            }
        }
        CHECK(g.phase==Phase::MatchOver&&passes==3&&g.player_score==p_points&&g.opponent_score==o_points);
        CHECK(g.contact_serial<=6&&g.thrust_serial==3&&g.player.thrust_serial==3&&!g.next_pass());
    }
    // Real incoming/outgoing reactions and falls, without edited positions,
    // forced scores, or synthetic contacts. Includes both travel directions,
    // same-tick mutual helmet hits, pass reset and repeated input after falling.
    for(int scenario=0;scenario<3;++scenario){
        Game g;g.configure({scenario?2:0,scenario?3:1,false});g.start();Input a;
        bool fired=false,simultaneous=false;int fall_counts[2]={0,0},body_reactions=0,shield_reactions=0,completed=0;
        bool seen_fall[2]={false,false};std::uint64_t fall_contacts=0,fall_strokes[2]={0,0};
        float loose_horse_start[2]={0,0},peak_reaction=0,flight_peak[2]={0,0};
        for(int i=0;i<12000&&g.phase!=Phase::MatchOver;++i){
            a.aim_x=scenario==2?.1f:0;a.aim_y=scenario==1&&g.pass_index==1?-.2f:(scenario==2?1.f:.8f);
            if(!fired&&g.phase==Phase::Charge&&g.distance_to_opponent()<=9){a.thrust=scenario!=1||g.pass_index!=2;fired=true;}
            const auto old_serial=g.contact_serial;const int old_p=g.player_score,old_o=g.opponent_score;
            g.update(fixed_step,a);
            if(g.contact_serial==old_serial+2&&g.player_score==old_p+3&&g.opponent_score==old_o+3)simultaneous=true;
            for(int side=0;side<2;++side){const Rider &r=side?g.opponent:g.player;
                CHECK(std::isfinite(rider_eye(r).x)&&std::isfinite(rider_eye(r).y)&&std::isfinite(rider_eye(r).z));
                CHECK(length(r.body_offset)<=.25002f&&r.reaction_amount>=0&&r.reaction_amount<=1.25001f);
                peak_reaction=std::max(peak_reaction,r.reaction_amount);
                if(r.hit_received==Hit::Torso&&r.reaction_amount>.1f)++body_reactions;
                if(r.hit_received==Hit::Shield&&r.reaction_amount>.1f)++shield_reactions;
                if(!r.unhorsed){CHECK(!r.fall_grounded);continue;}
                if(!seen_fall[side]){
                    seen_fall[side]=true;++fall_counts[side];fall_contacts=g.contact_serial;
                    fall_strokes[side]=r.thrust_serial;loose_horse_start[side]=r.horse.position.z;
                    CHECK(r.hit_received==Hit::Helmet&&r.hit_serial>0);
                }
                CHECK(r.thrust_serial==fall_strokes[side]&&!r.thrust_armed&&!r.thrust_active&&r.thrust_phase==ThrustPhase::Ready&&r.thrust_offset==0);
                CHECK(g.contact_serial==fall_contacts); // fallen riders are no longer mounted targets
                CHECK(r.fall_position.y>=fall_floor_height-1.e-5f&&r.fall_position.y<3.f);
                CHECK(std::abs(r.fall_position.x)<=fall_lateral_limit+.00001f&&std::abs(r.fall_position.z)<=58.001f);
                CHECK(std::abs(r.fall_position.x)+1.638f<5.48f);
                if(std::abs(r.fall_position.x)>=fall_lateral_limit-.00001f)CHECK(r.fall_position.x*r.fall_velocity.x<=0);
                CHECK(length(r.fall_velocity)<10.f&&std::abs(r.fall_roll)<=pi*.5f+.02f&&std::abs(r.fall_pitch)<=.3f);
                flight_peak[side]=std::max(flight_peak[side],r.fall_position.y);
                CHECK(close(rider_point(r,mounted_pelvis),r.fall_position+r.body_offset));
                CHECK(close(r.lance_grip,rider_point(r,g.grip_offset())+rider_rotate(r.fall_arm_local,r.fall_yaw,r.fall_pitch,r.fall_roll),2.e-5f));
                if(r.fall_grounded)CHECK(close(r.fall_position.y,fall_floor_height)&&r.fall_velocity.y==0);
                CHECK(r.fall_pose_progress>=0&&r.fall_pose_progress<=1&&r.fall_settle_progress>=0&&r.fall_settle_progress<=1);
                CHECK(r.fall_settled==(r.fall_grounded&&r.fall_ground_time>=.65f));
                if(g.phase==Phase::Result){CHECK(r.fall_grounded&&r.fall_ground_time>=1.35f&&r.fall_settled&&r.fall_pose_progress>.99f&&r.fall_settle_progress>.99f);CHECK(std::abs(r.horse.position.z-loose_horse_start[side])>2);}
            }
            if(g.player.unhorsed||g.opponent.unhorsed)a.thrust=(i%30)<15; // release/press cannot attack after ejection
            if(g.phase==Phase::Result){
                ++completed;const auto p_hits=g.player.hit_serial,o_hits=g.opponent.hit_serial;g.next_pass();
                if(g.phase!=Phase::MatchOver){
                    CHECK(!g.player.unhorsed&&!g.opponent.unhorsed&&!g.player.fall_grounded&&!g.opponent.fall_grounded);
                    CHECK(g.player.hit_received==Hit::Miss&&g.opponent.hit_received==Hit::Miss&&g.player.reaction_amount==0&&g.opponent.reaction_amount==0);
                    CHECK(g.player.hit_serial==p_hits&&g.opponent.hit_serial==o_hits);
                }
                a.thrust=false;fired=false;seen_fall[0]=seen_fall[1]=false;
            }
        }
        CHECK(g.phase==Phase::MatchOver&&completed==(scenario?3:1)&&peak_reaction>.4f);
        if(scenario==0)CHECK(fall_counts[0]==0&&fall_counts[1]==1&&flight_peak[1]>mounted_pelvis.y);
        if(scenario==1)CHECK(fall_counts[0]==1&&fall_counts[1]==0&&body_reactions>0&&shield_reactions>0);
        if(scenario==2)CHECK(simultaneous&&fall_counts[0]>=1&&fall_counts[1]>=2);
        g.reset();CHECK(!g.player.unhorsed&&!g.opponent.unhorsed&&g.player.hit_serial==0&&g.opponent.hit_serial==0&&g.player.reaction_amount==0);
    }
    // Head opportunities are actual incoming attacks at every difficulty,
    // with a visible committed high line and a genuine shield counter.
    // No opponent positions, contacts, scores, or falls are injected.
    for(int difficulty=0;difficulty<3;++difficulty)for(int guard=0;guard<2;++guard){
        Game g;g.configure({difficulty,3,true});g.start();Input a;a.guard=guard!=0;
        int completed=0;bool saw_tell=false,saw_windup=false,saw_helmet=false;float peak_strength=0,landing_speed=0;
        for(int i=0;i<10000&&g.phase!=Phase::MatchOver;++i){
            g.update(fixed_step,a);
            if(g.pass_index==1&&g.phase==Phase::Charge){
                CHECK(g.opponent.attack_target==Hit::Helmet&&g.opponent.attack_commit>=0&&g.opponent.attack_commit<=1);
                if(g.opponent.attack_commit>0&&g.opponent.attack_commit<1)saw_windup=true;
                if(g.opponent.attack_commit>.9f&&g.opponent.aim_y>.65f)saw_tell=true;
                if(g.player.unhorsed){
                    saw_helmet=true;peak_strength=std::max(peak_strength,g.player.fall_impact_strength);landing_speed=std::max(landing_speed,g.player.fall_landing_speed);
                    CHECK(close(length(g.player.fall_impact_direction),1,2.e-5f));
                    CHECK(g.player.fall_impact_strength>=0&&g.player.fall_impact_strength<=1);
                    CHECK(std::abs(g.player.fall_yaw-g.player.fall_yaw_start)<=.12001f);
                    CHECK(std::abs(g.player.fall_position.x)<=fall_lateral_limit+.00001f);
                    CHECK(std::abs(g.player.fall_pitch)<=.30f&&std::abs(g.player.fall_roll)<=pi*.5f+.02f);
                    CHECK(length(g.player.fall_angular_velocity)<.35f);
                }
            }
            if(g.phase==Phase::Result){
                if(g.pass_index==1){
                    CHECK(saw_tell&&saw_windup);
                    if(guard)CHECK(g.opponent_hit==Hit::Shield&&!g.player.unhorsed&&!saw_helmet);
                    else CHECK(g.opponent_hit==Hit::Helmet&&g.player.unhorsed&&saw_helmet&&peak_strength>.3f&&landing_speed>4.f&&g.player.fall_settled);
                }else CHECK(!g.player.unhorsed);
                ++completed;CHECK(g.next_pass());
            }
        }
        CHECK(g.phase==Phase::MatchOver&&completed==3&&g.reward_coins==0);
    }
    Input double_head;double_head.aim_x=.1f;double_head.aim_y=1;
    const Game mutual=match({2,3,false},double_head,1),mutual30=match({2,3,false},double_head,4),mutual_hitch=match({2,3,false},double_head,120);
    CHECK(mutual.player_score==mutual30.player_score&&mutual.player_score==mutual_hitch.player_score&&mutual.opponent_score==mutual30.opponent_score&&mutual.opponent_score==mutual_hitch.opponent_score);
    CHECK(mutual.contact_serial==mutual30.contact_serial&&mutual.contact_serial==mutual_hitch.contact_serial);
    CHECK(close(mutual.player.fall_position,mutual30.player.fall_position)&&close(mutual.opponent.fall_position,mutual30.opponent.fall_position));
    Game bounds;bounds.configure({1,1,false});bounds.start();Input bad;
    bad.steer=900;bad.throttle=900;bad.aim_x=std::numeric_limits<float>::infinity();bad.aim_y=std::numeric_limits<float>::quiet_NaN();
    for(int i=0;i<20&&bounds.phase!=Phase::MatchOver;++i){bounds.update(2,bad);CHECK(std::isfinite(bounds.player.lance_tip.x)&&std::isfinite(bounds.player.horse.yaw));CHECK(bounds.player.horse.position.x>=-2.021f&&bounds.player.horse.position.x<=-1.379f);CHECK(std::abs(bounds.player.horse.position.z)<=58.001f);}
    CHECK(bounds.phase==Phase::MatchOver);
    auto hitch=match({0,1,false},high,120);CHECK(hitch.player_hit==helmet.player_hit&&hitch.opponent_hit==helmet.opponent_hit&&close(hitch.player_contact.point,helmet.player_contact.point));
    std::printf("PASS jousting: %u checks; four-way ready aim, real 0.60m/0.82s with anticipation one-shot thrust, release/rearm and interrupt safety, clipped active-window swept contacts, no passive/early/late hits, guard difficulty, 120/30Hz and 1s hitch invariance, lane/shield limits, helmet unhorsing for both sides, simultaneous falls, body/shield reactions, grounded pass/reset, all-difficulty committed incoming helmet with shield counter, impact-driven bounded angular settling, real 3-pass totals and one-shot rewards\n",checks);
}
