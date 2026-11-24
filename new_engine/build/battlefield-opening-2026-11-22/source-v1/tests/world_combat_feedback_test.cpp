#include "engine/world_combat_feedback.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace world_combat_feedback;
struct Query {unsigned mode=0,calls=0;};
static bool query(const TrainingAnatomicalQuery&,TrainingAnatomicalHit &hit,void *context){
    auto &value=*static_cast<Query*>(context);++value.calls;
    if(!value.mode)return false;
    hit.valid=true;hit.weapon_contact=value.mode==1;hit.part=hit.weapon_contact?TrainingBodyPart::None:TrainingBodyPart::Torso;
    hit.point[0]=.2;hit.point[1]=1.2;hit.point[2]=1.1;return true;
}
static void ready(WorldTrainingCombat &fight){
    assert(fight.start_shared_arena());TrainingEvents events;
    for(unsigned n=0;n<1000&&fight.state!=TrainingState::Fighting;++n)assert(fight.update_shared(1./240.,nullptr,0,events));
    assert(fight.state==TrainingState::Fighting);
}
static TrainingContactEvent resolve(unsigned mode,unsigned weapon,double &lead,TrainingDirection direction=TrainingDirection::Left){
    WorldTrainingCombat fight;fight.weapon=world_weapon_loadout::Weapon(weapon);ready(fight);
    Query queries{mode};fight.contact_query=query;fight.contact_query_context=&queries;
    TrainingOpponent opponent;opponent.health=opponent.max_health=100;opponent.guard=direction;opponent.duration=100;
    if(mode==2)opponent.phase=TrainingEnemyPhase::Recovery;
    TrainingTarget target{&opponent,0,1.25};target.spatial=true;target.local_z=1.25;
    fight.choose(direction);assert(fight.attack());TrainingContactEvent result;bool found=false;double swoosh=-1;
    for(unsigned n=0;n<1000&&!found;++n){TrainingEvents events;assert(fight.update_shared(1./240.,&target,1,events));
        for(unsigned i=0;i<events.count;++i){const auto &event=events.values[i];if(event.kind==TrainingContactKind::PlayerSwing)swoosh=event.time;
            if(event.kind==TrainingContactKind::PlayerMiss||event.kind==TrainingContactKind::EnemyBlock||event.kind==TrainingContactKind::PlayerBody){result=event;found=true;}}
    }
    assert(found&&queries.calls==1&&swoosh>=0);lead=result.time-swoosh;
    const auto expected=mode==0?TrainingAttackOutcome::Miss:mode==1?TrainingAttackOutcome::EnemyBlock:TrainingAttackOutcome::Body;
    assert(fight.resolved_attack_serial==fight.attack_start_serial&&fight.resolved_attack_outcome==expected);
    if(mode<2)assert(opponent.health==100&&fight.hits==0&&fight.misses==1);
    else assert(opponent.health<100&&fight.hits==1&&result.damage==fight.resolved_attack_damage);
    if(!mode)assert(result.opponent_id==8&&!result.anatomical&&!result.damage&&fight.impact_pause==0);
    return result;
}
static TrainingContactEvent defend(bool parry){
    WorldTrainingCombat fight;ready(fight);fight.choose(TrainingDirection::High);fight.set_guard(true);
    if(!parry)fight.guard_age=.5;
    TrainingOpponent opponent;opponent.health=opponent.max_health=100;opponent.phase=TrainingEnemyPhase::Windup;opponent.direction=TrainingDirection::High;opponent.duration=.5;opponent.time=.499;
    TrainingTarget target{&opponent,0,1.25};TrainingEvents events;assert(fight.update_shared(.01,&target,1,events));
    for(unsigned i=0;i<events.count;++i)if(events.values[i].kind==(parry?TrainingContactKind::PlayerParry:TrainingContactKind::PlayerBlock))return events.values[i];
    assert(false);return {};
}
int main(){
    static_assert(unsigned(TrainingContactKind::PlayerBody)==0&&unsigned(TrainingContactKind::EnemySwing)==7&&unsigned(TrainingContactKind::PlayerMiss)==8);
    unsigned checks=0;std::vector<double> leads;double min_lead=1,max_lead=0;
    for(unsigned weapon:{0u,2u,3u,4u})for(unsigned mode=0;mode<3;++mode){double lead=0;auto event=resolve(mode,weapon,lead);if(mode==0)leads.push_back(lead);
        System feedback;feedback.advance(1./60.,event.time,false);Cue pulse;assert(feedback.observe(event,{},pulse));
        assert(pulse.kind==(mode==0?Kind::Miss:mode==1?Kind::Blocked:Kind::Body));
        if(!mode)assert(!pulse.impact&&!pulse.metal&&pulse.sound<0&&pulse.rumble_ms==0&&pulse.camera==0);
        else assert(pulse.impact&&pulse.sound>=0&&pulse.rumble_ms>0&&feedback.camera_impulse()>0);
        assert(!feedback.observe(event,{},pulse));++checks;
    }
    for(bool parry:{false,true}){auto event=defend(parry);System feedback;feedback.advance(.01,event.time,false);Cue pulse;assert(feedback.observe(event,{},pulse));assert(pulse.kind==(parry?Kind::Parry:Kind::Block)&&pulse.metal&&pulse.rumble_ms>0);++checks;}
    for(unsigned weapon:{0u,2u,3u,4u})for(unsigned direction=0;direction<5;++direction){double lead=0;resolve(0,weapon,lead,TrainingDirection(direction));
        assert(std::abs(lead-.15)<=1./240.+1e-8);min_lead=std::min(min_lead,lead);max_lead=std::max(max_lead,lead);++checks;}
    TrainingContactEvent blocked;blocked.serial=1;blocked.kind=TrainingContactKind::EnemyBlock;
    TrainingContactEvent incoming;incoming.serial=2;incoming.kind=TrainingContactKind::EnemyBody;incoming.damage=18;
    System combined;combined.advance(.01,1,false);Cue first,second;assert(combined.observe(blocked,{},first)&&combined.observe(incoming,{},second));
    assert(combined.attack_notice().kind==Kind::Blocked&&combined.defense_notice().kind==Kind::Incoming);
    Cue rumble;merge_rumble(rumble,first);merge_rumble(rumble,second);assert(rumble.low==second.low&&rumble.high==first.high&&rumble.rumble_ms==second.rumble_ms);
    merge_rumble(rumble,classify(TrainingContactEvent{},{}));assert(rumble.low==second.low&&rumble.high==first.high);++checks;
    auto disabled=classify(blocked,{false,true});assert(disabled.sound==2&&disabled.low==0&&disabled.high==0&&disabled.rumble_ms==0&&disabled.camera==0);++checks;
    System paused;paused.advance(.01,1,true);Cue pulse;assert(!paused.observe(blocked,{},pulse));paused.advance(.01,1.1,false);assert(!paused.observe(blocked,{},pulse));assert(paused.attack_notice().kind==Kind::None);++checks;
    paused.reset();paused.advance(.01,0,false);assert(paused.observe(blocked,{},pulse));paused.advance(.01,-1,false);assert(paused.observe(blocked,{},pulse));++checks;
    // The historical sentinel8 may also be an actor index in a25-person UI.
    // The explicit kind must keep local whoosh and whiff centered/unattenuated.
    assert(local_air(TrainingContactKind::PlayerSwing)&&local_air(TrainingContactKind::PlayerMiss)&&!local_air(TrainingContactKind::EnemySwing));
    TrainingContactEvent swing;swing.kind=TrainingContactKind::PlayerSwing;swing.opponent_id=8;auto own=classify(swing);assert(own.sound==0&&own.volume==56&&!own.impact&&!own.rumble_ms);++checks;
    std::printf("{\"passed\":true,\"real_training_outcome_cases\":12,\"production_defense_event_cases\":2,\"policy_checks\":%u,\"whoosh_timing_cases\":20,\"whoosh_lead_min_seconds\":%.9f,\"whoosh_lead_max_seconds\":%.9f,\"timing_step_seconds\":%.9f,\"four_weapon_swoosh_to_resolution_seconds\":[%.9f,%.9f,%.9f,%.9f],\"controlled_geometry_queries\":true,\"actual_owned_geometry_acceptance\":false}\n",checks,min_lead,max_lead,1./240.,leads[0],leads[1],leads[2],leads[3]);
}
