#include "engine/world_presentation.h"
#include "engine/world_runtime_demo.h"
#include "engine/world_training_combat.h"
#include "engine/world_runtime_animation.h"
#include <filesystem>
#include <iostream>
#include <limits>
#include <unistd.h>
using namespace world_runtime_demo;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
int main(){
    std::string error;world_living::World living;Sites sites;sites.castle_x=1000;sites.castle_z=2000;sites.jungle_x=-26688;sites.jungle_z=9670.496705929329;sites.jungle_floor=138.235;Seed s;
    CHECK(build(sites,{},living,true,1000,s,error));CHECK(presentation(sites,s,136.5,error));
    CHECK(s.configuration.gameplay.quests.front().objectives.size()==4);
    world_runtime::Runtime r;CHECK(r.configure(s.configuration,error));world_runtime::Frame frame;
    auto tick=[&](){return r.tick(.016,1000,{},frame);};
    CHECK(r.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(r.choose(0));CHECK(tick());
    CHECK(r.enqueue({world_gameplay::EventKind::EncounterFinished,0,training_encounter_id,0,1}));CHECK(tick());CHECK(r.counter(mastery_counter)==1);
    CHECK(r.enqueue({world_gameplay::EventKind::Interact,jungle_actor_first}));CHECK(tick());CHECK(r.choices().count==1);CHECK(r.choose(1));CHECK(tick());CHECK(r.counter(camp_counter)==0);CHECK(r.close_dialogue());CHECK(tick());
    CHECK(r.enqueue({world_gameplay::EventKind::Interact,id_base+0x40}));CHECK(tick());
    bool request=false;for(unsigned i=0;i<frame.requests.count;i++)request|=frame.requests.values[i].kind==world_gameplay::CommandKind::StartEncounter&&frame.requests.values[i].key==world_presentation::threat_encounter;CHECK(request);
    CHECK(r.enqueue({world_gameplay::EventKind::EncounterFinished,0,world_presentation::threat_encounter,0,0}));CHECK(tick());CHECK(r.counter(world_presentation::threat_counter)==0);
    CHECK(r.enqueue({world_gameplay::EventKind::Interact,id_base+0x40}));CHECK(tick());
    CHECK(r.enqueue({world_gameplay::EventKind::EncounterFinished,0,world_presentation::threat_encounter,0,1}));CHECK(tick());CHECK(r.counter(world_presentation::threat_counter)==1);
    CHECK(r.enqueue({world_gameplay::EventKind::Interact,jungle_actor_first}));CHECK(tick());CHECK(r.choices().count==2);CHECK(r.choose(1));CHECK(tick());CHECK(r.counter(camp_counter)==1);
    CHECK(r.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(r.recollection().recognition);CHECK(r.choose(2));CHECK(tick());
    CHECK(r.quest(journey_id)==world_gameplay::Progress::Complete&&r.counter(coin_counter)==25&&r.counter(renown_counter)==1);
    CHECK(r.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(r.choices().count==2);CHECK(r.choose(2));CHECK(tick());CHECK(r.counter(coin_counter)==25);CHECK(r.close_dialogue());CHECK(tick());
    char dir[]="/tmp/valwood-presentation-test-XXXXXX";CHECK(mkdtemp(dir));const auto path=std::filesystem::path(dir)/"preview.progress";
    world_runtime::Runtime::Player player;player.feet={1000,24,2000};CHECK(r.save_progress(path.c_str(),player,error));world_runtime::Runtime fresh;CHECK(fresh.configure(s.configuration,error));CHECK(fresh.load_progress(path.c_str(),player,1010,error));CHECK(fresh.counter(world_presentation::threat_counter)==1&&fresh.counter(coin_counter)==25);std::filesystem::remove_all(dir);
    // A same-frame retry must deliver the pending defeat before reopening:
    // otherwise StartTraining sees an already-active encounter and does nothing.
    world_runtime::Runtime immediate;world_runtime::Frame retry_frame;CHECK(immediate.configure(s.configuration,error));
    CHECK(immediate.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(immediate.tick(.016,1000,{},retry_frame));CHECK(immediate.choose(0));CHECK(immediate.tick(.016,1000,{},retry_frame));
    std::uint64_t pending=training_encounter_id;CHECK(world_presentation::queue_retry(immediate,pending,instructor_id));CHECK(pending==0);CHECK(immediate.tick(.016,1000,{},retry_frame));CHECK(immediate.counter(mastery_counter)==0);CHECK(immediate.current_line());CHECK(immediate.choose(0));CHECK(immediate.tick(.016,1000,{},retry_frame));
    bool restarted=false;for(unsigned i=0;i<retry_frame.requests.count;++i)restarted|=retry_frame.requests.values[i].kind==world_gameplay::CommandKind::StartTraining;CHECK(restarted);
    // Exercise the actual directional rules, including a genuine defeat and a retry.
    WorldTrainingCombat fight;CHECK(fight.start_threat(true));CHECK(!fight.start_threat(true));
    fight.update(3,fight.start_x,fight.start_z);CHECK(fight.engaged());
    const double paused=fight.clock;fight.update(.1,fight.start_x,fight.start_z,true);CHECK(fight.clock==paused);
    for(unsigned i=0;i<15000&&fight.engaged();i++){
        if(fight.state==TrainingState::Fighting){
            if(fight.enemy_phase==TrainingEnemyPhase::Windup){fight.choose(fight.enemy_direction);fight.set_guard(fight.enemy_duration-fight.enemy_time<.17);}
            else{fight.set_guard(false);if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=23){fight.choose(TrainingDirection((unsigned(fight.enemy_guard)+1)%5));fight.attack();}}
        }fight.update(1./60.,fight.enemy_x-2.2,fight.enemy_z);
    }CHECK(fight.state==TrainingState::Won&&fight.hits>0&&fight.parries>0);CHECK(fight.start_threat());
    for(unsigned i=0;i<15000&&fight.engaged();i++)fight.update(1./60.,fight.enemy_x-2.2,fight.enemy_z);
    CHECK(fight.state==TrainingState::Lost);CHECK(fight.start_threat(true,true));CHECK(fight.watch_reward);
    CHECK(world_presentation::stick(.1f)==0&&world_presentation::stick(1.f)==1&&world_presentation::stick(-1.f)==-1);
    CHECK(world_presentation::chapter(true,1,1,1,1)==world_presentation::Chapter::Complete);
    // Real purchased rig: mapped head motion, fixed root, valid bounds, snapshot
    // corruption rejection, and an unmapped-joint failure at configuration time.
    auto config=world_runtime_animation::knight_config(true);config.attention_head="Head";config.attention_neck="neck";config.gesture_arm="RightForeArm";
    const char *asset="projects/valwood_world/models/animations/white_knight_brown_armor_demo.glb";
    auto animation=world_runtime_animation::Manager::load(asset,config,2,error);CHECK(animation);
    world_runtime_animation::ActorDesc desc;desc.placement.position={0,24,0};auto handle=animation->spawn(desc,error);CHECK(handle.slot<2);
    world_runtime_animation::Context context;CHECK(animation->update(0,context,error));const auto first=animation->frame().poses[0].local[18].rotation;
    CHECK(animation->set_attention(handle,.5f,.2f,true,true,error));for(unsigned i=0;i<120;i++)CHECK(animation->update(1./60.,context,error));
    const auto head=animation->frame().poses[0].local[18].rotation;CHECK(std::abs(first.y-head.y)>.02f);
    world_runtime_animation::Bounds bounds;CHECK(animation->bounds(handle,bounds,error));CHECK(std::isfinite(bounds.minimum.x)&&std::isfinite(bounds.maximum.y));
    CHECK(!animation->set_attention(handle,1.5f,0,true,true,error));
    world_runtime_animation::Snapshot state;animation->checkpoint(state);state.actors[handle.slot].attention_yaw=std::numeric_limits<float>::quiet_NaN();CHECK(!animation->restore(state,context,error));
    config.attention_head="missing_head";CHECK(!world_runtime_animation::Manager::load(asset,config,2,error));
    std::cout<<"world_presentation: "<<checks<<" progression, threat-host, reward, save/resume, actual combat, pause, controller deadzone and exact-rig attention checks passed\n";
}
