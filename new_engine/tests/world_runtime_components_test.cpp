#include "engine/world_runtime_components.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <thread>
#include <unistd.h>
static std::atomic<bool> tracking{false};static std::atomic<unsigned> allocations{0};
[[gnu::noinline]] void *operator new(std::size_t n){if(tracking)++allocations;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void *operator new(std::size_t n,const std::nothrow_t &)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
[[gnu::noinline]] void *operator new[](std::size_t n,const std::nothrow_t &)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,const std::nothrow_t &)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,const std::nothrow_t &)noexcept{std::free(p);}
using namespace world_runtime;static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
static bool near(double a,double b,double e=.001){return std::abs(a-b)<e;}
static Configuration fixture(){
    Configuration c;c.entities={{1001,{{0,3,0},{}},2,true},{9001,{{20,0,0},{}},1,true},{8001,{{25,0,0},{}},1,true}};
    c.physics_enabled=c.gameplay_enabled=c.living_enabled=true;
    world_physics::Body ground;ground.id=42;ground.size={30,1,30};ground.pose.position={0,-1,0};
    world_physics::Body crate;crate.id=7;crate.motion=world_physics::Motion::Dynamic;crate.mass=1;crate.pose.position={0,3,0};crate.pose.rotation={0,.70710678f,0,.70710678f};crate.restitution=0;
    c.physics.bodies={ground,crate};PhysicsBinding pb;pb.entity=1001;pb.body=7;pb.body_to_entity.position={1,.25,0};c.components.physics={pb};
    world_living::Definition d;d.enabled=true;d.start_minute=540;d.factions={{1,"Village",{}}};world_living::Settlement settlement;settlement.id=1;settlement.faction=1;settlement.name="Town";settlement.stock={500,100,100,100};settlement.reserve={1000,1000,1000,0};d.settlements={settlement};world_living::Actor actor;actor.id=42;actor.faction=actor.home=1;actor.name="Trainer";d.actors={actor};std::string error;c.living.configure(d,1000,error);
    LivingBinding lb;lb.entity=9001;lb.actor=42;lb.home={19,0,0};lb.work={22,0,0};lb.leisure={20,0,0};lb.guard={25,0,0};c.components.living={lb};
    c.gameplay.entities={{1001,"Crate",true,{0,3,0}},{9001,"Trainer",true,{20,0,0}},{8001,"Training yard",true,{25,0,0}}};
    c.gameplay.flags={{10,"Spoken",false}};world_gameplay::Dialogue talk;talk.id=50;talk.speaker=9001;talk.entry=51;talk.name="Training welcome";world_gameplay::Line line;line.id=51;line.text="Ready to train?";world_gameplay::Choice choice;choice.text="Begin";choice.actions={{world_gameplay::ActionKind::SetFlag,10,1},{world_gameplay::ActionKind::ActivateEntity,1001,0}};line.choices={choice};talk.lines={line};c.gameplay.dialogues={talk};
    world_gameplay::Encounter fight;fight.id=70;fight.arena=8001;fight.name="Knight training";fight.training=true;fight.actors={9001};c.gameplay.encounters={fight};world_gameplay::Rule rule;rule.id=80;rule.name="Speak to trainer";rule.event=world_gameplay::EventKind::Interact;rule.entity=9001;rule.actions={{world_gameplay::ActionKind::Speak,50,1},{world_gameplay::ActionKind::StartTraining,70,1}};c.gameplay.rules={rule};return c;
}
int main(){
    std::string error;auto c=fixture();Runtime runtime;CHECK(runtime.configure(c,error));CHECK(runtime.configured());Entity entity;CHECK(runtime.entity(1001,entity));CHECK(near(entity.pose.position.x,0)&&near(entity.pose.position.y,3.25)&&near(entity.pose.position.z,-1)&&near(entity.scale,2));CHECK(runtime.scene_entity_for_body(7)==1001&&runtime.scene_entity_for_body(42)==0);
    Snapshot initial;CHECK(runtime.snapshot(initial,error));Frame frame;CHECK(runtime.tick(1./60.,1000,{20,0,0},frame));CHECK(frame.entity_count==3&&frame.motion_count==1&&frame.motion[0].entity==9001&&frame.motion[0].actor==42&&frame.motion[0].activity==world_living::Activity::Work&&near(frame.motion[0].destination.x,22));CHECK(runtime.entity(1001,entity)&&entity.pose.position.y<3.25);
    Id selected=999;CHECK(runtime.interact_nearest({20,0,0},2,selected)&&selected==9001);CHECK(runtime.tick(1./60.,1000.02,{20,0,0},frame));bool training=false,speaking=false;for(unsigned i=0;i<frame.requests.count;i++){auto &r=frame.requests.values[i];training|=r.kind==world_gameplay::CommandKind::StartTraining&&r.entity==8001&&r.key==70;speaking|=r.kind==world_gameplay::CommandKind::Speak&&std::string(r.text)=="Ready to train?";}CHECK(training&&speaking);CHECK(runtime.current_line()&&runtime.choices().count==1);CHECK(runtime.choose(0));CHECK(runtime.tick(1./60.,1000.04,{20,0,0},frame));CHECK(!runtime.current_line());CHECK(runtime.entity(1001,entity)&&!entity.active);
    // Activation removes the collider as well as its visible scene entity.
    world_physics::Hit hit;Id mapped=777;CHECK(runtime.raycast({0,10,0},{0,-20,0},hit,mapped)&&hit.hit&&hit.body==42&&mapped==0);
    CHECK(runtime.restore(initial,2000,error));CHECK(runtime.entity(1001,entity)&&entity.active&&near(entity.pose.position.y,3.25));CHECK(runtime.raycast({0,10,0},{0,-20,0},hit,mapped)&&hit.hit&&hit.body==7&&mapped==1001);CHECK(runtime.tick(0,2000,{20,0,0},frame)&&frame.living_steps==0);CHECK(runtime.stats().living_pending_steps==0);
    Pose moved;moved.position={30,0,0};CHECK(runtime.set_scene_pose(9001,moved));CHECK(!runtime.set_scene_pose(1001,moved));CHECK(!runtime.interact_nearest({20,0,0},2,selected));CHECK(runtime.interact_nearest({30,0,0},2,selected)&&selected==9001);CHECK(runtime.tick(.016,2000,{30,0,0},frame));CHECK(runtime.close_dialogue());
    auto bad=c;bad.components.physics[0].body=1001;CHECK(!runtime.configure(bad,error));CHECK(runtime.entity(1001,entity)&&entity.active);bad=c;bad.entities.push_back(c.entities.front());CHECK(!runtime.configure(bad,error));bad=c;bad.components.living[0].actor=9001;CHECK(!runtime.configure(bad,error));bad=c;bad.components.living[0].entity=1001;CHECK(!runtime.configure(bad,error));bad=c;bad.entities[0].pose.rotation.w=0;CHECK(!runtime.configure(bad,error));
    bool foreign=true;std::thread worker([&]{foreign=runtime.tick(.01,2000,{20,0,0},frame);});worker.join();CHECK(!foreign);CHECK(!runtime.tick(.3,2000,{20,0,0},frame));CHECK(!runtime.tick(.01,std::numeric_limits<double>::quiet_NaN(),{20,0,0},frame));
    CHECK(!runtime.tick(.01,1e100,{20,0,0},frame));CHECK(runtime.cast_capsule({10,3,0},.3f,.5f,{0,-5,0},hit,mapped)&&hit.hit&&hit.body==42&&mapped==0);
    // A different configure (even identical documents) invalidates old Play snapshots.
    CHECK(runtime.configure(c,error));CHECK(!runtime.restore(initial,2000,error));Snapshot current;CHECK(runtime.snapshot(current,error));Runtime other;CHECK(other.configure(c,error));CHECK(!other.restore(current,2000,error));
    auto expanded=c.entities;Entity new_marker;new_marker.id=999999;new_marker.pose.position={50,1,50};expanded.push_back(new_marker);CHECK(runtime.entity(1001,entity));const auto old_crate=entity.pose;CHECK(runtime.can_rebind_scene(expanded,error));CHECK(runtime.can_restore(current,2000,error));CHECK(runtime.rebind_scene(expanded,error));CHECK(runtime.entity(1001,entity)&&near(entity.pose.position.y,old_crate.position.y));CHECK(runtime.entity(999999,entity)&&near(entity.pose.position.x,50));CHECK(!runtime.can_restore(current,2000,error));CHECK(!runtime.restore(current,2000,error));auto missing=expanded;missing.erase(missing.begin());CHECK(!runtime.can_rebind_scene(missing,error));CHECK(!runtime.rebind_scene(missing,error));CHECK(runtime.entity(1001,entity));auto scaled=expanded;scaled[0].scale=3;CHECK(!runtime.can_rebind_scene(scaled,error));CHECK(!runtime.rebind_scene(scaled,error));
    // Authored visibility and logical activation are independent masks. A
    // hidden live actor can be shown again; a logically defeated one cannot.
    auto masked=fixture();masked.entities[0].active=false;world_gameplay::Rule reactivate;reactivate.id=81;reactivate.name="Activate training crate";reactivate.entity=8001;reactivate.actions={{world_gameplay::ActionKind::ActivateEntity,1001,1}};masked.gameplay.rules.push_back(reactivate);CHECK(runtime.configure(masked,error));CHECK(runtime.entity(1001,entity)&&!entity.active);CHECK(runtime.raycast({0,10,0},{0,-20,0},hit,mapped)&&hit.body==42);CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,8001}));CHECK(runtime.tick(.016,1000,{25,0,0},frame));CHECK(runtime.entity(1001,entity)&&!entity.active);auto visible=masked.entities;visible[0].active=true;CHECK(runtime.rebind_scene(visible,error));CHECK(runtime.entity(1001,entity)&&entity.active);CHECK(runtime.enqueue({world_gameplay::EventKind::Kill,1001}));CHECK(runtime.tick(.016,1000,{25,0,0},frame));CHECK(runtime.entity(1001,entity)&&!entity.active);CHECK(runtime.rebind_scene(masked.entities,error));CHECK(runtime.rebind_scene(visible,error));CHECK(runtime.entity(1001,entity)&&!entity.active);CHECK(runtime.rebind_scene(masked.entities,error));CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,8001}));CHECK(runtime.tick(.016,1000,{25,0,0},frame));CHECK(runtime.entity(1001,entity)&&!entity.active);CHECK(runtime.rebind_scene(visible,error));CHECK(runtime.entity(1001,entity)&&entity.active);CHECK(runtime.raycast({0,10,0},{0,-20,0},hit,mapped)&&hit.body==7&&mapped==1001);
    // Studio's external authoritative world is read once, without independent time advancement.
    auto external=c.living;c.external_living=&external;c.drive_living_clock=false;CHECK(runtime.configure(c,error));Snapshot external_snapshot;CHECK(runtime.snapshot(external_snapshot,error));const auto ticks=external.state().tick;CHECK(runtime.tick(.016,9000,{20,0,0},frame));CHECK(external.state().tick==ticks&&frame.living_steps==0);CHECK(external.observe_clock(1060,error));external.process(1);CHECK(external.state().tick==ticks+1);CHECK(runtime.restore(external_snapshot,9000,error));CHECK(external.state().tick==ticks&&external.state().last_wall_milliseconds==9000000);
    // Resetting the external authoritative definition during Play cannot poison
    // rollback. Restore validates against the saved definition before publishing
    // it back to the external world. A reset before Play is rejected instead.
    auto replacement=external.definition();replacement.actors[0].id=999;
    CHECK(external.configure(replacement,9000,error));Snapshot unchanged=external_snapshot;
    CHECK(!runtime.snapshot(unchanged,error));CHECK(runtime.can_restore(unchanged,9100,error));
    CHECK(!runtime.read_frame(frame));const auto before_failed_tick=runtime.stats().frames;
    CHECK(!runtime.tick(.016,9100,{20,0,0},frame)&&runtime.stats().frames==before_failed_tick);
    CHECK(runtime.restore(external_snapshot,9100,error));
    CHECK(external.definition().actors[0].id==42&&external.state().tick==ticks&&external.state().last_wall_milliseconds==9100000);
    CHECK(runtime.read_frame(frame)&&frame.motion_count==1&&frame.motion[0].actor==42);
    CHECK(runtime.snapshot(external_snapshot,error));
    // Contact processing, pose export, schedule targets and empty gameplay transactions allocate nothing.
    CHECK(runtime.configure(c,error));CHECK(runtime.tick(.016,9000,{20,0,0},frame));allocations=0;tracking=true;bool frames=true;for(unsigned i=0;i<1200;i++)frames=frames&&runtime.tick(1./60.,9000+i/60.,{100,0,0},frame);tracking=false;CHECK(frames&&allocations==0);CHECK(runtime.stats().physics.total_frame_allocations==0&&runtime.stats().physics.query_allocations==0);
    // Full authored memory banks participate in real NPC destinations, not
    // draws; meetings/choices are captured only after accepted transactions.
    auto memory_fixture=fixture();memory_fixture.external_living=nullptr;memory_fixture.drive_living_clock=true;
    world_npc_minds::Binding mind;mind.entity=9001;mind.context={"player"};mind.memory.profile.npc_id="trainer";mind.memory.profile.name="Trainer";mind.memory.profile.actor_id=42;
    for(unsigned i=0;i<256;i++){world_npc_memory::Memory memory;memory.id="memory_"+std::to_string(i);memory.title="A retained experience";memory.tags={"player"};mind.memory.memories.push_back(memory);}memory_fixture.minds={mind};
    CHECK(runtime.configure(memory_fixture,error));CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,9001}));CHECK(runtime.tick(.016,1000,{20,0,0},frame));CHECK(runtime.experiences()->sequence==1);
    CHECK(runtime.close_dialogue());CHECK(runtime.tick(.016,1000,{20,0,0},frame));
    allocations=0;tracking=true;bool memory_frames=true;for(unsigned i=0;i<1200;i++)memory_frames=memory_frames&&runtime.tick(1./60.,1000+i*60.,{100,0,0},frame);tracking=false;CHECK(memory_frames&&allocations==0);
    CHECK(runtime.experiences()->sequence==1); // no invented observation in empty frames
    char folder[]="/tmp/valwood-runtime-components-XXXXXX";CHECK(mkdtemp(folder));const std::filesystem::path root(folder);Document doc=c.components;doc.physics_asset="world.physics";doc.gameplay_asset="world.gameplay";doc.living_asset="world.living";const auto component_file=(root/"world.components").string();CHECK(save(component_file.c_str(),doc,error));Document loaded;CHECK(load(component_file.c_str(),loaded,error));CHECK(loaded.physics.size()==1&&loaded.physics[0].entity==1001&&loaded.physics[0].body==7&&loaded.living[0].actor==42);
    CHECK(world_physics::save((root/"world.physics").c_str(),c.physics,error));CHECK(world_gameplay::save((root/"world.gameplay").c_str(),c.gameplay,error));CHECK(c.living.save((root/"world.living").string(),error));Configuration imported;CHECK(load_project(folder,loaded,c.entities,10000,imported,error));CHECK(imported.physics_enabled&&imported.gameplay_enabled&&imported.living_enabled&&imported.living.state().last_wall_milliseconds==10000000);CHECK(other.configure(imported,error));
    auto malformed=doc;malformed.physics_asset="../outside.physics";CHECK(!validate(malformed,error));malformed=doc;malformed.physics_asset="/tmp/outside.physics";CHECK(!validate(malformed,error));malformed=doc;malformed.physics.push_back(malformed.physics.front());CHECK(!validate(malformed,error));
    CHECK(symlink("world.physics",(root/"linked.physics").c_str())==0);malformed=doc;malformed.physics_asset="linked.physics";CHECK(!load_project(folder,malformed,c.entities,10000,imported,error));CHECK(symlink("world.components",(root/"linked.components").c_str())==0);CHECK(!load((root/"linked.components").c_str(),loaded,error));CHECK(!save((root/"linked.components").c_str(),doc,error));
    std::ofstream broken(root/"bad.components");broken<<"VALWOOD_COMPONENTS 1\nassets \"\" \"\" \"\"\nphysics -1 7 0 0 0 0 0 0 1\n";broken.close();CHECK(!load((root/"bad.components").c_str(),loaded,error));CHECK(loaded.physics[0].entity==1001);std::filesystem::remove_all(root);
    std::cout<<"world_runtime_components: "<<checks<<" scene/Jolt activation/dialogue/training/living/Play/asset contracts; 1200 frames with zero C++ or Jolt heap fallback\n";return 0;
}
