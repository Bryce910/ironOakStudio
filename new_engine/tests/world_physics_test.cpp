#include "engine/world_physics.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <thread>
#include <unistd.h>

static std::atomic<bool> tracking{false};static std::atomic<unsigned> allocations{0};
[[gnu::noinline]] void *operator new(std::size_t bytes){if(tracking)allocations++;if(void *p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
using namespace world_physics;
static unsigned checks=0;
#define CHECK(x) do{checks++;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
static bool near(double a,double b,double e=1e-4){return std::abs(a-b)<e;}
static Body body(Id id,Shape shape,Motion motion,Vec3 position){Body b;b.id=id;b.shape=shape;b.motion=motion;b.pose.position=position;return b;}
static Document fixture(){
    Document d;auto ground=body(1,Shape::Box,Motion::Static,{100000,-1,100000});ground.size={20,1,20};
    auto sphere=body(2,Shape::Sphere,Motion::Dynamic,{100000.125,3,100000});sphere.mass=1;sphere.restitution=0;
    auto cube=body(3,Shape::Box,Motion::Dynamic,{100003,4,100000});cube.mass=2;cube.restitution=0;
    auto capsule=body(4,Shape::Capsule,Motion::Dynamic,{100005,3,100000});capsule.size={.3,.5,.3};capsule.restitution=0;
    auto trigger=body(5,Shape::Box,Motion::Static,{100000,1.5,100000});trigger.sensor=true;trigger.size={2,3,2};
    auto door=body(6,Shape::Box,Motion::Kinematic,{100009.5,1,100004});door.size={1,1,.15};
    d.bodies={ground,sphere,cube,capsule,trigger,door};Door hinge;hinge.body=6;hinge.pivot={100008.5,1,100004};hinge.speed=2;d.doors={hinge};return d;
}
static std::string bytes(const std::string &path){std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}
int main(){
    std::string error;{World first;CHECK(first.configure(Document{},error));CHECK(first.step(fixed_step));CHECK(first.stop());}
    Document d=fixture();CHECK(validate(d,error));World world;CHECK(world.configure(d,error));BodyState state;CHECK(world.body_state(2,state));CHECK(near(state.pose.position.x,100000.125,1e-10)&&near(state.pose.position.y,3,1e-10));
    Event events[event_limit];unsigned enter=0,exit=0,contact=0;
    for(unsigned i=0;i<300;i++){
        CHECK(world.step(fixed_step));unsigned count=world.drain_events(events,event_limit);for(unsigned j=0;j<count;j++){
            const auto &e=events[j];enter+=e.kind==EventKind::TriggerEnter&&e.a==5&&e.b==2;exit+=e.kind==EventKind::TriggerExit&&e.a==5&&e.b==2;
            contact+=e.kind==EventKind::ContactBegin&&((e.a==1&&e.b==2)||(e.a==2&&e.b==1));
        }
    }
    CHECK(world.body_state(2,state));CHECK(near(state.pose.position.y,.5,.015)&&near(state.pose.position.x,100000.125,.001));CHECK(enter==1&&exit==0&&contact>=1);CHECK(world.stats().update_errors==0);CHECK(world.body_state(3,state)&&near(state.pose.position.y,.5,.02));CHECK(world.body_state(4,state)&&state.pose.position.y>.28&&state.pose.position.y<.85);
    Pose away;away.position={100015,.5,100000};CHECK(world.teleport(2,away));CHECK(world.step(fixed_step));unsigned count=world.drain_events(events,event_limit);exit=0;for(unsigned j=0;j<count;j++)exit+=events[j].kind==EventKind::TriggerExit&&events[j].a==5&&events[j].b==2;CHECK(exit==1);
    CHECK(world.impulse(2,{1,0,0}));CHECK(world.body_state(2,state)&&near(state.linear_velocity.x,1,.002));CHECK(!world.impulse(1,{1,0,0}));
    // True Jolt queries use double world origins and exact stable entity IDs.
    Hit hit;CHECK(world.raycast({100012.3,5,100000},{0,-10,0},hit));CHECK(hit.hit&&hit.body==1&&near(hit.fraction,.5,.001)&&hit.normal.y>.99);CHECK(near(hit.position.x,100012.3,1e-8));
    CHECK(world.cast_capsule({100012.3,5,100000},.35f,.55f,{0,-10,0},hit));CHECK(hit.hit&&hit.body==1&&near(hit.fraction,.41,.01)&&hit.normal.y>.9);
    away.position={100015,4,100000};CHECK(world.teleport(2,away));CHECK(world.sweep_body(2,{0,-10,0},hit));CHECK(hit.hit&&hit.body==1&&near(hit.fraction,.35,.01));
    CHECK(world.raycast({100030,3,100000},{0,1,0},hit)&&!hit.hit);CHECK(!world.raycast({100030,3,100000},{0,0,0},hit));
    CHECK(world.door_target(6,1.570796327f));for(unsigned i=0;i<90;i++)CHECK(world.step(fixed_step));CHECK(world.body_state(6,state));CHECK(near(state.pose.position.x,100008.5,.002)&&near(state.pose.position.z,100003,.002));CHECK(!world.teleport(6,away));
    Snapshot snapshot;CHECK(world.snapshot(snapshot));BodyState saved;CHECK(world.body_state(2,saved));CHECK(world.impulse(2,{0,3,0}));for(unsigned i=0;i<45;i++)CHECK(world.step(fixed_step));CHECK(world.restore(snapshot,error));CHECK(world.body_state(2,state));CHECK(near(state.pose.position.x,saved.pose.position.x,1e-8)&&near(state.pose.position.y,saved.pose.position.y,1e-8)&&near(state.linear_velocity.y,saved.linear_velocity.y,1e-6));
    Snapshot invalid=snapshot;invalid.bodies[0].id=999;CHECK(!world.restore(invalid,error));CHECK(world.body_state(2,state)&&near(state.pose.position.y,saved.pose.position.y,1e-8));
    auto bad=d;bad.bodies[0].size.x=-1;CHECK(!world.configure(bad,error));CHECK(world.body_state(2,state)&&near(state.pose.position.y,saved.pose.position.y,1e-8));bad=d;bad.bodies[1].id=1;CHECK(!validate(bad,error));bad=d;bad.bodies[1].pose.rotation={0,0,0,0};CHECK(!validate(bad,error));
    // Independent worlds keep Jolt's shared factory alive across destruction.
    {World other;CHECK(other.configure(d,error));CHECK(other.step(fixed_step));CHECK(other.stop());}CHECK(world.step(fixed_step));
    bool wrong_thread=true;std::thread foreign([&]{wrong_thread=world.step(fixed_step);});foreign.join();CHECK(!wrong_thread);

    // Real dynamic collisions reverse two equal masses; layer masks disable it.
    Document collision;collision.gravity={0,0,0};auto left=body(20,Shape::Sphere,Motion::Dynamic,{-1,1,0});left.mass=1;left.friction=0;left.restitution=1;left.linear_velocity={2,0,0};auto right=left;right.id=21;right.pose.position={1,1,0};right.linear_velocity={-2,0,0};collision.bodies={left,right};CHECK(world.configure(collision,error));
    for(unsigned i=0;i<60;i++){CHECK(world.step(fixed_step));}
    CHECK(world.body_state(20,state)&&state.linear_velocity.x< -1.5);CHECK(world.body_state(21,state)&&state.linear_velocity.x>1.5);
    collision.bodies[1].layer=1;collision.layer_masks[0]=1;collision.layer_masks[1]=2;CHECK(world.configure(collision,error));for(unsigned i=0;i<60;i++)CHECK(world.step(fixed_step));CHECK(world.body_state(20,state)&&state.linear_velocity.x>1.5&&state.pose.position.x>.5);
    // Static CCW mesh is real collision geometry, not a bounding-box surrogate.
    Document mesh;auto floor=body(30,Shape::Mesh,Motion::Static,{100000,0,100000});floor.vertices={{-20,0,-20},{20,0,-20},{20,0,20},{-20,0,20}};floor.triangles={{{0,2,1}},{{0,3,2}}};auto ball=body(31,Shape::Sphere,Motion::Dynamic,{100000.0625,2,100000});ball.restitution=0;mesh.bodies={floor,ball};CHECK(world.configure(mesh,error));for(unsigned i=0;i<240;i++)CHECK(world.step(fixed_step));CHECK(world.body_state(31,state)&&near(state.pose.position.y,.5,.02)&&near(state.pose.position.x,100000.0625,.001));CHECK(world.raycast({100010,5,100000},{0,-10,0},hit)&&hit.hit&&hit.body==30);
    bad=mesh;bad.bodies[0].triangles[0][2]=99;CHECK(!world.configure(bad,error));CHECK(world.body_state(31,state));bad=mesh;bad.bodies[0].motion=Motion::Dynamic;CHECK(!validate(bad,error));

    CHECK(world.configure(fixture(),error));CHECK(world.step(1));CHECK(world.stats().substeps==4&&world.stats().dropped_seconds>.9&&world.stats().accumulator<fixed_step);CHECK(!world.step(-1)&&!world.step(std::numeric_limits<double>::quiet_NaN()));
    // Disabled bodies keep their ID/pose/velocity while leaving broadphase.
    CHECK(world.configure(fixture(),error));CHECK(world.impulse(2,{2,0,0}));CHECK(world.body_state(2,saved));CHECK(world.set_enabled(2,false));CHECK(world.body_state(2,state)&&!state.enabled&&!state.active&&near(state.linear_velocity.x,saved.linear_velocity.x));CHECK(!world.impulse(2,{1,0,0}));CHECK(world.raycast({100000.125,6,100000},{0,-10,0},hit)&&hit.hit&&hit.body==1);CHECK(world.snapshot(snapshot));for(unsigned i=0;i<10;i++)CHECK(world.step(fixed_step));CHECK(world.body_state(2,state)&&near(state.pose.position.y,saved.pose.position.y));CHECK(world.set_enabled(2,true));CHECK(world.body_state(2,state)&&state.enabled&&near(state.linear_velocity.x,saved.linear_velocity.x));CHECK(world.step(fixed_step));CHECK(world.body_state(2,state)&&state.pose.position.x>saved.pose.position.x);CHECK(world.restore(snapshot,error));CHECK(world.body_state(2,state)&&!state.enabled&&near(state.linear_velocity.x,saved.linear_velocity.x));CHECK(world.set_enabled(2,true));
    CHECK(world.set_enabled(5,false));CHECK(world.step(fixed_step));count=world.drain_events(events,event_limit);enter=0;for(unsigned i=0;i<count;i++)enter+=events[i].kind==EventKind::TriggerEnter&&events[i].a==5;CHECK(enter==0);CHECK(world.set_enabled(5,true));CHECK(world.step(fixed_step));count=world.drain_events(events,event_limit);enter=0;for(unsigned i=0;i<count;i++)enter+=events[i].kind==EventKind::TriggerEnter&&events[i].a==5;CHECK(enter>=1);CHECK(world.set_enabled(5,false));count=world.drain_events(events,event_limit);exit=0;for(unsigned i=0;i<count;i++)exit+=events[i].kind==EventKind::TriggerExit&&events[i].a==5;CHECK(exit>=1);
    CHECK(world.configure(fixture(),error));allocations=0;tracking=true;bool toggles=true;for(unsigned i=0;i<180;i++){toggles=toggles&&world.set_enabled(2,(i%2)!=0)&&world.step(fixed_step);world.drain_events(events,event_limit);}tracking=false;CHECK(toggles&&allocations==0);CHECK(world.stats().query_allocations==0&&world.stats().total_frame_allocations==0);
    // Exercise changing contacts/queries/doors after startup without heap growth.
    CHECK(world.configure(fixture(),error));world.drain_events(events,event_limit);CHECK(world.door_target(6,1));allocations=0;tracking=true;bool frames=true;
    for(unsigned i=0;i<900;i++){frames=frames&&world.step(fixed_step)&&world.cast_capsule({100012,3,100000},.35f,.55f,{0,-5,0},hit);world.drain_events(events,event_limit);}
    tracking=false;CHECK(frames);CHECK(allocations==0);
    if(world.stats().total_frame_allocations||world.stats().query_allocations||world.stats().event_overflows)std::cerr<<"allocation evidence: update="<<world.stats().total_frame_allocations<<" query="<<world.stats().query_allocations<<" events="<<world.stats().event_overflows<<'\n';
    CHECK(world.stats().total_frame_allocations==0&&world.stats().query_allocations==0&&world.stats().event_overflows==0);CHECK(world.stats().pooled_frame_allocations>0);

    // Maximum body/sensor capacities, sleeping disabled, symmetric collision
    // masks and bounded event overflow. The 32 sensor queries generate 15,360
    // first-frame overlaps without creating 480x480 physical contact pairs.
    Document full;full.gravity={0,0,0};full.layer_masks[0]=2;full.layer_masks[1]=1;
    for(unsigned i=0;i<sensor_limit;i++){auto b=body(1000+i,Shape::Box,Motion::Static,{0,2,0});b.sensor=true;b.size={100,10,100};full.bodies.push_back(b);}
    for(unsigned i=sensor_limit;i<body_limit;i++){auto b=body(1000+i,Shape::Sphere,Motion::Dynamic,{double((i-sensor_limit)%24)*2-24,2,double((i-sensor_limit)/24)*2-20});b.layer=1;b.allow_sleep=false;full.bodies.push_back(b);}
    CHECK(world.configure(full,error));CHECK(world.stats().bodies==body_limit&&world.stats().sensors==sensor_limit);allocations=0;tracking=true;frames=true;
    for(unsigned i=0;i<120;i++){frames=frames&&world.step(fixed_step);world.drain_events(events,event_limit);}tracking=false;
    CHECK(frames&&allocations==0&&world.stats().total_frame_allocations==0);CHECK(world.stats().event_overflows==sensor_limit*(body_limit-sensor_limit)-event_limit);CHECK(world.stats().pair_overflows==0&&world.stats().update_errors==0);bad=full;bad.bodies.push_back(body(9999,Shape::Box,Motion::Static,{0,0,0}));CHECK(!world.configure(bad,error));CHECK(world.stats().bodies==body_limit);

    char folder[]="/tmp/valwood-physics-test-XXXXXX";CHECK(mkdtemp(folder));std::string a=std::string(folder)+"/world.physics",b=std::string(folder)+"/copy.physics",broken=std::string(folder)+"/bad.physics";d=fixture();d.bodies.push_back(mesh.bodies[0]);d.bodies.back().id=50;CHECK(save(a.c_str(),d,error));Document loaded;CHECK(load(a.c_str(),loaded,error));CHECK(save(b.c_str(),loaded,error));CHECK(bytes(a)==bytes(b));auto original=bytes(a);bad=d;bad.bodies[0].pose.position.x=std::numeric_limits<double>::infinity();CHECK(!save(a.c_str(),bad,error)&&bytes(a)==original);
    for(const std::string &text:{std::string("VALWOOD_PHYSICS 99\n"),std::string("VALWOOD_PHYSICS 1\nlayer 0 999\n"),std::string("VALWOOD_PHYSICS 1\nvertex 999 0 0 0\n"),std::string("VALWOOD_PHYSICS 1\ndoor -1 0 0 0 0 1 0 0 1 1\n")}){std::ofstream out(broken);out<<text;out.close();CHECK(!load(broken.c_str(),loaded,error));CHECK(loaded.bodies.size()==d.bodies.size());}
    // A partial configuration must not silently restore default gravity or
    // broaden collision masks, even if all body records remain parseable.
    for(const std::string &missing:{std::string("gravity "),std::string("layer 7 ")}){std::string partial=original;auto start=partial.find(missing);CHECK(start!=std::string::npos);auto end=partial.find('\n',start);CHECK(end!=std::string::npos);partial.erase(start,end-start+1);std::ofstream out(broken);out<<partial;out.close();CHECK(!load(broken.c_str(),loaded,error));CHECK(loaded.bodies.size()==d.bodies.size());}
    std::string link=std::string(folder)+"/link.physics";CHECK(symlink(a.c_str(),link.c_str())==0);CHECK(!save(link.c_str(),d,error)&&!load(link.c_str(),loaded,error));unlink(link.c_str());unlink(a.c_str());unlink(b.c_str());unlink(broken.c_str());CHECK(rmdir(folder)==0);CHECK(world.stop());
    std::cout<<"world_physics: "<<checks<<" actual Jolt gravity/collision/sensor/door/query/snapshot/codec contracts; 900 frames+queries and 120 max-capacity frames with zero heap fallback\n";return 0;
}
