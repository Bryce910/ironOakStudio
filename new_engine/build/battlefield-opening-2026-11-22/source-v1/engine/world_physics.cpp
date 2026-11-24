#include "engine/world_platform.h"
#include "engine/world_physics.h"
#if !defined(JPH_DOUBLE_PRECISION) || !defined(NDEBUG)
#error "Valwood Jolt adapter must match the pinned JPH_DOUBLE_PRECISION/NDEBUG library ABI"
#endif
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <mutex>
#include <new>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <unordered_set>
#include <unistd.h>

namespace world_physics {
namespace {
constexpr unsigned pair_limit=8192;
bool range(double value,double lo,double hi){return std::isfinite(value)&&value>=lo&&value<=hi;}
bool vec(Vec3 value,double limit=1e8){return range(value.x,-limit,limit)&&range(value.y,-limit,limit)&&range(value.z,-limit,limit);}
double length(Vec3 a){return std::hypot(std::hypot(a.x,a.y),a.z);}
bool rotation(Quaternion q){return range(q.x,-1,1)&&range(q.y,-1,1)&&range(q.z,-1,1)&&range(q.w,-1,1)&&std::abs(std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w)-1)<.01;}
bool pose_ok(const Pose &p){return vec(p.position)&&rotation(p.rotation);}
bool name(const std::string &s){return !s.empty()&&s.size()<=95&&s.find_first_of("\r\n\t")==std::string::npos&&s.find('\0')==std::string::npos;}
JPH::Vec3 v3(Vec3 v){return {float(v.x),float(v.y),float(v.z)};}
JPH::RVec3 r3(Vec3 v){return {v.x,v.y,v.z};}
JPH::Quat q4(Quaternion q){return JPH::Quat(q.x,q.y,q.z,q.w).Normalized();}
Vec3 vec_out(JPH::Vec3Arg v){return {v.GetX(),v.GetY(),v.GetZ()};}
Vec3 pos_out(JPH::RVec3Arg v){return {v.GetX(),v.GetY(),v.GetZ()};}
Quaternion rot_out(JPH::QuatArg q){return {q.GetX(),q.GetY(),q.GetZ(),q.GetW()};}
int body_index(const Document &d,Id id){for(unsigned i=0;i<d.bodies.size();i++)if(d.bodies[i].id==id)return int(i);return -1;}
bool read_id(std::istream &in,Id &id){std::string s;if(!(in>>s)||s.empty()||s.size()>20||!std::all_of(s.begin(),s.end(),[](char c){return c>='0'&&c<='9';}))return false;try{id=std::stoull(s);return id!=0;}catch(...){return false;}}

// Jolt's broadphase allocates a pair of temporary trees during UpdatePrepare.
// They remain live until the NEXT FrameSync, so a reset-at-end frame arena is
// unsafe. These fixed slots carry allocation headers and are freed individually
// by Jolt, including delayed frees at the next frame or scene destruction.
struct FramePool;
struct alignas(16) AllocationHeader {void *raw;FramePool *pool;size_t size;unsigned slot;};
constexpr unsigned scratch_slots=32;
constexpr size_t scratch_block_bytes=64*1024;
struct FramePool {
    struct alignas(16) Slot {std::array<unsigned char,scratch_block_bytes+sizeof(AllocationHeader)+256> data;bool used=false;};
    unsigned count=scratch_slots;std::unique_ptr<Slot[]> slots;
    explicit FramePool(unsigned capacity=scratch_slots):count(capacity),slots(new Slot[capacity]){}
    void *allocate(size_t n,size_t alignment){
        if(n>scratch_block_bytes||alignment>256)return nullptr;
        for(unsigned i=0;i<count;i++)if(!slots[i].used){
            auto start=reinterpret_cast<std::uintptr_t>(slots[i].data.data())+sizeof(AllocationHeader);
            auto location=(start+alignment-1)&~std::uintptr_t(alignment-1);
            auto *header=reinterpret_cast<AllocationHeader *>(location)-1;
            ::new(header) AllocationHeader{nullptr,this,n,i};slots[i].used=true;return reinterpret_cast<void *>(location);
        }return nullptr;
    }
};
thread_local std::uint64_t *allocation_counter=nullptr,*pooled_counter=nullptr;
thread_local FramePool *frame_pool=nullptr;
std::mutex global_mutex;unsigned global_users=0;bool allocator_installed=false;
JPH::AllocateFunction base_allocate=nullptr;JPH::FreeFunction base_free=nullptr;
void *count_aligned(size_t n,size_t alignment){
    alignment=std::max(alignment,alignof(AllocationHeader));
    if(!alignment||(alignment&(alignment-1))||n>SIZE_MAX-sizeof(AllocationHeader)-alignment)return nullptr;
    if(frame_pool)if(void *p=frame_pool->allocate(n,alignment)){if(pooled_counter)(*pooled_counter)++;return p;}
    if(allocation_counter)(*allocation_counter)++;
    void *raw=base_allocate(n+sizeof(AllocationHeader)+alignment);if(!raw)return nullptr;
    auto start=reinterpret_cast<std::uintptr_t>(raw)+sizeof(AllocationHeader);
    auto location=(start+alignment-1)&~std::uintptr_t(alignment-1);
    auto *header=reinterpret_cast<AllocationHeader *>(location)-1;
    ::new(header) AllocationHeader{raw,nullptr,n,0};return reinterpret_cast<void *>(location);
}
void *count_allocate(size_t n){return count_aligned(n,16);}
void count_free(void *p){if(!p)return;auto *header=static_cast<AllocationHeader *>(p)-1;if(header->pool)header->pool->slots[header->slot].used=false;else base_free(header->raw);}
void *count_reallocate(void *p,size_t old,size_t n){
    if(!n){count_free(p);return nullptr;}void *next=count_allocate(n);if(!next)return nullptr;
    if(p){auto *header=static_cast<AllocationHeader *>(p)-1;std::memcpy(next,p,std::min({n,old,header->size}));count_free(p);}return next;
}
void count_aligned_free(void *p){count_free(p);}
struct AllocationGuard {
    std::uint64_t *previous,*previous_pooled;FramePool *previous_pool;
    explicit AllocationGuard(std::uint64_t &counter,FramePool &pool,std::uint64_t &pooled):previous(allocation_counter),previous_pooled(pooled_counter),previous_pool(frame_pool){allocation_counter=&counter;frame_pool=&pool;pooled_counter=&pooled;}
    ~AllocationGuard(){allocation_counter=previous;pooled_counter=previous_pooled;frame_pool=previous_pool;}
};
bool acquire(std::string &error){
    std::lock_guard<std::mutex> lock(global_mutex);
    if(!global_users){
        if(JPH::Factory::sInstance){error="Jolt factory is already owned outside this adapter";return false;}
        if(!JPH::VerifyJoltVersionID()){error="Pinned Jolt library ABI differs from adapter flags";return false;}
        if(!allocator_installed){
            JPH::RegisterDefaultAllocator();base_allocate=JPH::Allocate;base_free=JPH::Free;
            JPH::Allocate=count_allocate;JPH::Reallocate=count_reallocate;JPH::Free=count_free;JPH::AlignedAllocate=count_aligned;JPH::AlignedFree=count_aligned_free;allocator_installed=true;
        }else if(JPH::Allocate!=count_allocate||JPH::Reallocate!=count_reallocate||JPH::Free!=count_free||JPH::AlignedAllocate!=count_aligned||JPH::AlignedFree!=count_aligned_free){error="Jolt allocator was replaced outside this adapter";return false;}
        JPH::Factory::sInstance=new JPH::Factory;JPH::RegisterTypes();
    }global_users++;return true;
}
// Hooks intentionally live for the executable lifetime: a Jolt static cache may
// release a wrapped allocation after the final World. There is no DLL unload or
// shared ownership with an external Jolt factory in this component.
void release(){std::lock_guard<std::mutex> lock(global_mutex);if(global_users&&!--global_users){JPH::UnregisterTypes();delete JPH::Factory::sInstance;JPH::Factory::sInstance=nullptr;}}

unsigned group(JPH::ObjectLayer layer){return unsigned(layer)%4;}
unsigned channel(JPH::ObjectLayer layer){return unsigned(layer)/4;}
struct BroadLayers final:JPH::BroadPhaseLayerInterface {
    JPH::uint GetNumBroadPhaseLayers()const override{return 2;}
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer)const override{return JPH::BroadPhaseLayer(group(layer)?1:0);}
};
struct BroadFilter final:JPH::ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer a,JPH::BroadPhaseLayer b)const override{return group(a)!=3&&(group(a)!=0||b.GetValue()==1);}
};
struct PairFilter final:JPH::ObjectLayerPairFilter {
    const Document &document;explicit PairFilter(const Document &d):document(d){}
    bool ShouldCollide(JPH::ObjectLayer a,JPH::ObjectLayer b)const override{
        if(group(a)==3||group(b)==3||(!group(a)&&!group(b)))return false;
        return (document.layer_masks[channel(a)]&(1u<<channel(b)))&&(document.layer_masks[channel(b)]&(1u<<channel(a)));
    }
};
struct Scene;
struct Contacts final:JPH::ContactListener {
    Scene *scene;explicit Contacts(Scene *s):scene(s){}
    void OnContactAdded(const JPH::Body &,const JPH::Body &,const JPH::ContactManifold &,JPH::ContactSettings &)override;
    void OnContactRemoved(const JPH::SubShapeIDPair &)override;
};
struct Scene {
    FramePool scratch; // Must outlive PhysicsSystem and its deferred tree frees.
    Document document;BroadLayers broad;BroadFilter broad_filter;PairFilter pairs;Contacts contacts;
    JPH::PhysicsSystem physics;JPH::TempAllocatorImpl temporary;JPH::JobSystemSingleThreaded jobs{1024};
    struct Record {JPH::BodyID body;JPH::RefConst<JPH::Shape> shape;Vec3 disabled_linear,disabled_angular;};
    std::array<Record,body_limit> records;std::array<int,body_limit> indices;
    struct Pair {JPH::BodyID a,b;Id first=0,second=0;unsigned count=0;Vec3 point,normal;};
    std::array<Pair,pair_limit> live_pairs={};
    std::array<std::array<std::uint64_t,body_limit/64>,body_limit> trigger_bits={};
    std::array<Event,event_limit> events={};unsigned event_head=0,event_count=0;
    std::array<DoorState,door_limit> doors={};Stats stats;double accumulator=0;
    unsigned created=0;std::uint64_t signature=0;Profile profile=Profile::Simulation;
    explicit Scene(const Document &d,Profile p=Profile::Simulation):scratch(p==Profile::StaticQuery?8:scratch_slots),document(d),pairs(document),contacts(this),temporary(p==Profile::StaticQuery?1024*1024:32*1024*1024),profile(p){indices.fill(-1);}
    ~Scene(){physics.SetContactListener(nullptr);auto &bi=physics.GetBodyInterface();for(unsigned i=0;i<created;i++){if(bi.IsAdded(records[i].body))bi.RemoveBody(records[i].body);bi.DestroyBody(records[i].body);}}
    int index(JPH::BodyID id)const{unsigned n=id.GetIndex();return n<body_limit?indices[n]:-1;}
    void push(Event e){e.tick=stats.ticks;if(event_count==event_limit){stats.event_overflows++;return;}events[(event_head+event_count)%event_limit]=e;event_count++;}
    bool build(std::string &error);
    void triggers();
};
void Contacts::OnContactAdded(const JPH::Body &a,const JPH::Body &b,const JPH::ContactManifold &m,JPH::ContactSettings &){
    if(a.IsSensor()||b.IsSensor())return;
    Scene::Pair *pair=nullptr,*empty=nullptr;
    for(auto &p:scene->live_pairs){if(p.count&&p.a==a.GetID()&&p.b==b.GetID()){pair=&p;break;}if(!p.count&&!empty)empty=&p;}
    if(!pair){pair=empty;if(!pair){scene->stats.pair_overflows++;return;}pair->a=a.GetID();pair->b=b.GetID();pair->first=a.GetUserData();pair->second=b.GetUserData();}
    pair->point=m.mRelativeContactPointsOn1.empty()?pos_out(m.mBaseOffset):pos_out(m.GetWorldSpaceContactPointOn1(0));pair->normal=vec_out(m.mWorldSpaceNormal);
    if(pair->count++==0)scene->push({EventKind::ContactBegin,pair->first,pair->second,pair->point,pair->normal,0});
}
void Contacts::OnContactRemoved(const JPH::SubShapeIDPair &key){for(auto &p:scene->live_pairs)if(p.count&&p.a==key.GetBody1ID()&&p.b==key.GetBody2ID()){if(!--p.count)scene->push({EventKind::ContactEnd,p.first,p.second,p.point,p.normal,0});return;}}
struct QueryFilter final:JPH::BodyFilter {
    const Scene &scene;Id ignore;unsigned layer;bool moving_only;
    QueryFilter(const Scene &s,Id id,unsigned l,bool moving=false):scene(s),ignore(id),layer(l),moving_only(moving){}
    bool ShouldCollide(const JPH::BodyID &id)const override{int n=scene.index(id);if(n<0)return false;const auto &b=scene.document.bodies[unsigned(n)];return b.id!=ignore&&!b.sensor&&(!moving_only||b.motion!=Motion::Static)&&(scene.document.layer_masks[layer]&(1u<<b.layer))&&(scene.document.layer_masks[b.layer]&(1u<<layer));}
};
struct Overlap final:JPH::CollideShapeCollector {
    const Scene &scene;std::array<std::uint64_t,body_limit/64> bits={};explicit Overlap(const Scene &s):scene(s){}
    void AddHit(const JPH::CollideShapeResult &hit)override{int i=scene.index(hit.mBodyID2);if(i>=0)bits[unsigned(i)/64]|=1ull<<(unsigned(i)%64);}
};
void Scene::triggers(){
    const auto &bi=physics.GetBodyInterface();for(unsigned i=0;i<document.bodies.size();i++)if(document.bodies[i].sensor){
        Overlap hits(*this);if(bi.IsAdded(records[i].body)){QueryFilter filter(*this,document.bodies[i].id,document.bodies[i].layer,true);JPH::CollideShapeSettings settings;settings.mMaxSeparationDistance=0;
        auto transform=bi.GetCenterOfMassTransform(records[i].body);physics.GetNarrowPhaseQuery().CollideShape(records[i].shape,JPH::Vec3::sReplicate(1),transform,settings,transform.GetTranslation(),hits,{},{},filter);}
        for(unsigned j=0;j<document.bodies.size();j++){bool before=(trigger_bits[i][j/64]>>(j%64))&1,now=(hits.bits[j/64]>>(j%64))&1;if(before!=now)push({now?EventKind::TriggerEnter:EventKind::TriggerExit,document.bodies[i].id,document.bodies[j].id,pos_out(bi.GetPosition(records[j].body)),{},0});}trigger_bits[i]=hits.bits;
    }
}
std::uint64_t fingerprint(const Document &d){
    // Incremental canonical scalar encoding avoids constructing a giant text
    // copy of every vertex when a structural scene contains millions of faces.
    // Snapshot signatures are runtime identities, never an on-disk file format.
    std::uint64_t hash=14695981039346656037ull;
    auto integer=[&](std::uint64_t value,unsigned bytes=8){for(unsigned i=0;i<bytes;++i){hash^=static_cast<unsigned char>(value&255);hash*=1099511628211ull;value>>=8;}};
    auto real=[&](double value){std::uint64_t bits=0;std::memcpy(&bits,&value,8);integer(bits);};
    auto vector=[&](Vec3 v){real(v.x);real(v.y);real(v.z);};
    auto text=[&](const std::string &v){integer(v.size());for(unsigned char c:v)integer(c,1);};
    integer(2);vector(d.gravity);for(unsigned mask:d.layer_masks)integer(mask,4);integer(d.bodies.size());
    for(const auto &b:d.bodies){integer(b.id);text(b.name);integer(unsigned(b.shape),4);integer(unsigned(b.motion),4);integer(b.sensor,1);integer(b.layer,4);vector(b.pose.position);real(b.pose.rotation.x);real(b.pose.rotation.y);real(b.pose.rotation.z);real(b.pose.rotation.w);vector(b.size);real(b.friction);real(b.restitution);real(b.density);real(b.mass);real(b.gravity_factor);vector(b.linear_velocity);vector(b.angular_velocity);integer(b.allow_sleep,1);integer(b.vertices.size());integer(b.triangles.size());for(auto v:b.vertices)vector(v);for(auto t:b.triangles)for(auto ix:t)integer(ix,4);}
    integer(d.doors.size());for(const auto &door:d.doors){integer(door.body);vector(door.pivot);vector(door.axis);real(door.minimum);real(door.maximum);real(door.speed);}return hash;
}
bool Scene::build(std::string &error){
    physics.Init(body_limit,0,8192,4096,broad,broad_filter,pairs);
    auto solver=physics.GetPhysicsSettings();solver.mPenetrationSlop=.005f;physics.SetPhysicsSettings(solver);
    physics.SetGravity(v3(document.gravity));physics.SetContactListener(&contacts);
    auto &bi=physics.GetBodyInterface();
    for(unsigned i=0;i<document.bodies.size();i++){
        const auto &b=document.bodies[i];JPH::ShapeSettings::ShapeResult result;
        if(b.shape==Shape::Box){JPH::BoxShapeSettings setting(v3(b.size),std::min(.025f,float(std::min({b.size.x,b.size.y,b.size.z})*.5)));setting.mDensity=b.density;result=setting.Create();}
        else if(b.shape==Shape::Sphere){JPH::SphereShapeSettings setting(float(b.size.x));setting.mDensity=b.density;result=setting.Create();}
        else if(b.shape==Shape::Capsule){JPH::CapsuleShapeSettings setting(float(b.size.y),float(b.size.x));setting.mDensity=b.density;result=setting.Create();}
        else {JPH::VertexList vertices;vertices.reserve(b.vertices.size());for(auto p:b.vertices)vertices.emplace_back(float(p.x),float(p.y),float(p.z));JPH::IndexedTriangleList triangles;triangles.reserve(b.triangles.size());for(auto t:b.triangles)triangles.emplace_back(t[0],t[1],t[2],0);JPH::MeshShapeSettings setting(std::move(vertices),std::move(triangles));result=setting.Create();}
        if(result.HasError()){error="Jolt shape rejected: "+std::string(result.GetError().c_str());return false;}records[i].shape=result.Get();
        auto motion=b.motion==Motion::Static?JPH::EMotionType::Static:b.motion==Motion::Dynamic?JPH::EMotionType::Dynamic:JPH::EMotionType::Kinematic;
        unsigned g=b.sensor?3:unsigned(b.motion);JPH::BodyCreationSettings settings(records[i].shape,r3(b.pose.position),q4(b.pose.rotation),motion,JPH::ObjectLayer(b.layer*4+g));
        settings.mUserData=b.id;settings.mIsSensor=b.sensor;settings.mCollideKinematicVsNonDynamic=b.motion==Motion::Kinematic;settings.mAllowSleeping=b.allow_sleep;settings.mLinearVelocity=v3(b.linear_velocity);settings.mAngularVelocity=v3(b.angular_velocity);settings.mFriction=b.friction;settings.mRestitution=b.restitution;settings.mGravityFactor=b.gravity_factor;
        settings.mMotionQuality=b.motion==Motion::Dynamic?JPH::EMotionQuality::LinearCast:JPH::EMotionQuality::Discrete;settings.mEnhancedInternalEdgeRemoval=true;
        if(b.mass>0){settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;settings.mMassPropertiesOverride.mMass=b.mass;}
        auto *body=bi.CreateBody(settings);if(!body){error="Jolt body pool exhausted";return false;}records[i].body=body->GetID();indices[body->GetID().GetIndex()]=int(i);created++;bi.AddBody(body->GetID(),b.motion==Motion::Static?JPH::EActivation::DontActivate:JPH::EActivation::Activate);
    }
    physics.OptimizeBroadPhase();
    // Materialize single-thread job page and persistent update caches while
    // loading, then restore authored transforms/velocities before publication.
    auto update=physics.Update(float(fixed_step),1,&temporary,&jobs);if(update!=JPH::EPhysicsUpdateError::None){error="Jolt warmup exceeded pair/contact capacity";return false;}
    for(unsigned i=0;i<document.bodies.size();i++){const auto &b=document.bodies[i];bi.SetPositionAndRotation(records[i].body,r3(b.pose.position),q4(b.pose.rotation),b.motion==Motion::Static?JPH::EActivation::DontActivate:JPH::EActivation::Activate);if(b.motion!=Motion::Static){bi.SetLinearVelocity(records[i].body,v3(b.linear_velocity));bi.SetAngularVelocity(records[i].body,v3(b.angular_velocity));}bi.InvalidateContactCache(records[i].body);}
    live_pairs={};event_count=event_head=0;stats={};stats.temporary_capacity=profile==Profile::StaticQuery?1024*1024:32*1024*1024;stats.scratch_capacity=scratch.count*scratch_block_bytes;stats.bodies=unsigned(document.bodies.size());for(const auto &b:document.bodies)stats.sensors+=b.sensor;for(unsigned i=0;i<document.doors.size();i++)doors[i].body=document.doors[i].body;signature=fingerprint(document);return true;
}
}

bool validate(const Document &d,std::string &error){
    error.clear();auto fail=[&](const char *s){error=s;return false;};if(d.bodies.size()>body_limit||d.doors.size()>door_limit||!vec(d.gravity,1000))return fail("Physics body/door/gravity budget exceeded");
    for(unsigned mask:d.layer_masks)if(mask>255)return fail("Invalid collision layer mask");
    std::unordered_set<Id> ids,doors;unsigned vertices=0,triangles=0,sensors=0;
    for(const auto &b:d.bodies){
        if(!b.id||!ids.insert(b.id).second||!name(b.name)||unsigned(b.shape)>3||unsigned(b.motion)>2||b.layer>=layer_count||!pose_ok(b.pose)||!vec(b.size,10000)||!vec(b.linear_velocity,500)||!vec(b.angular_velocity,100)||!range(b.friction,0,2)||!range(b.restitution,0,1)||!range(b.density,.1,100000)||!range(b.mass,0,1e8)||!range(b.gravity_factor,0,4))return fail("Invalid body ID/transform/material/motion");
        if(b.sensor&&++sensors>sensor_limit)return fail("Sensor budget exceeded");
        if(b.shape!=Shape::Mesh){if(!range(b.size.x,.01,1000)||!range(b.size.y,.01,1000)||!range(b.size.z,.01,1000)||!b.vertices.empty()||!b.triangles.empty())return fail("Invalid primitive dimensions/mesh data");}
        else {
            if(b.motion!=Motion::Static||b.sensor||b.vertices.size()<3||b.triangles.empty()||b.vertices.size()>vertex_limit-vertices||b.triangles.size()>triangle_limit-triangles)return fail("Static mesh geometry budget/motion invalid");
            vertices+=unsigned(b.vertices.size());triangles+=unsigned(b.triangles.size());
            for(auto v:b.vertices)if(!vec(v,10000))return fail("Invalid local mesh vertex");
            for(auto t:b.triangles){if(t[0]>=b.vertices.size()||t[1]>=b.vertices.size()||t[2]>=b.vertices.size())return fail("Mesh triangle index outside vertices");auto a=b.vertices[t[0]],bb=b.vertices[t[1]],c=b.vertices[t[2]];Vec3 u={bb.x-a.x,bb.y-a.y,bb.z-a.z},v={c.x-a.x,c.y-a.y,c.z-a.z};if(length({u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x})<1e-8)return fail("Degenerate collision triangle");}
        }
    }
    for(const auto &door:d.doors){int index=body_index(d,door.body);if(index<0||!doors.insert(door.body).second||d.bodies[unsigned(index)].motion!=Motion::Kinematic||d.bodies[unsigned(index)].sensor||d.bodies[unsigned(index)].shape==Shape::Mesh||!vec(door.pivot)||!vec(door.axis,1)||std::abs(length(door.axis)-1)>.001||!range(door.minimum,-6.283185307,0)||!range(door.maximum,0,6.283185307)||door.minimum>=door.maximum||!range(door.speed,.01,10))return fail("Invalid kinematic hinge/pivot/body");}
    return true;
}
struct World::Impl {std::thread::id owner=std::this_thread::get_id();bool acquired=false;std::unique_ptr<Scene> scene;bool owner_thread()const{return owner==std::this_thread::get_id();}~Impl(){scene.reset();if(acquired)release();}};
World::World():impl_(new Impl){}
World::~World()=default;
bool World::configure(const Document &d,std::string &error,Profile profile){
    auto &p=*impl_;if(!p.owner_thread()){error="Physics configure requires owner thread";return false;}if(!validate(d,error))return false;
    if(profile!=Profile::Simulation&&profile!=Profile::StaticQuery){error="Invalid physics memory profile";return false;}
    if(profile==Profile::StaticQuery&&(!d.doors.empty()||std::any_of(d.bodies.begin(),d.bodies.end(),[](const Body &b){return b.motion!=Motion::Static||b.sensor;}))){error="Static query profile requires nonsensor static geometry";return false;}
    if(!p.acquired){if(!acquire(error))return false;p.acquired=true;}
    auto next=std::make_unique<Scene>(d,profile);if(!next->build(error))return false;p.scene=std::move(next);error.clear();return true;
}
bool World::handoff_static_query_owner(std::thread::id target,std::string &error){
    auto &p=*impl_;if(!p.owner_thread()){error="Static collision handoff requires its current owner thread";return false;}
    if(target==std::thread::id{}||!p.scene||p.scene->profile!=Profile::StaticQuery){error="Only configured static query collision may transfer ownership";return false;}
    p.owner=target;error.clear();return true;
}
bool World::step(double dt){
    auto &p=*impl_;if(!p.owner_thread()||!p.scene||!range(dt,0,86400))return false;auto &s=*p.scene;AllocationGuard guard(s.stats.frame_allocations,s.scratch,s.stats.pooled_frame_allocations);s.stats.frame_allocations=0;s.stats.substeps=0;s.stats.last_update_error=0;s.accumulator+=dt;
    const unsigned count=unsigned(std::min(4.,std::floor((s.accumulator+1e-12)/fixed_step)));
    for(unsigned n=0;n<count;n++){
        auto &bi=s.physics.GetBodyInterface();for(unsigned i=0;i<s.document.doors.size();i++){auto &state=s.doors[i];const auto &door=s.document.doors[i];const auto index=unsigned(body_index(s.document,door.body));if(!bi.IsAdded(s.records[index].body))continue;const auto &body=s.document.bodies[index];const double difference=state.target-state.angle;state.angle+=float(std::clamp(difference,-door.speed*fixed_step,door.speed*fixed_step));auto rotation=JPH::Quat::sRotation(v3(door.axis).Normalized(),state.angle);JPH::RVec3 location=r3(door.pivot)+rotation*static_cast<JPH::Vec3>(r3(body.pose.position)-r3(door.pivot));bi.MoveKinematic(s.records[index].body,location,rotation*q4(body.pose.rotation),float(fixed_step));}
        auto result=s.physics.Update(float(fixed_step),1,&s.temporary,&s.jobs);s.stats.ticks++;s.stats.substeps++;s.accumulator=std::max(0.,s.accumulator-fixed_step);if(result!=JPH::EPhysicsUpdateError::None){s.stats.last_update_error|=unsigned(result);s.stats.update_errors++;}s.triggers();
    }
    s.stats.unprocessed_debt=s.accumulator;if(s.accumulator>=fixed_step){double dropped=std::floor(s.accumulator/fixed_step)*fixed_step;s.stats.dropped_seconds+=dropped;s.accumulator-=dropped;}
    s.stats.accumulator=s.accumulator;s.stats.pending_events=s.event_count;s.stats.active_bodies=s.physics.GetNumActiveBodies(JPH::EBodyType::RigidBody);s.stats.total_frame_allocations+=s.stats.frame_allocations;return s.stats.last_update_error==0;
}
bool World::body_state(Id id,BodyState &out)const{const auto &p=*impl_;if(!p.owner_thread()||!p.scene)return false;const auto &s=*p.scene;int i=body_index(s.document,id);if(i<0)return false;const auto &bi=s.physics.GetBodyInterface();const auto &record=s.records[unsigned(i)];auto body=record.body;BodyState next;next.id=id;next.pose={pos_out(bi.GetPosition(body)),rot_out(bi.GetRotation(body))};next.enabled=bi.IsAdded(body);next.active=next.enabled&&bi.IsActive(body);if(s.document.bodies[unsigned(i)].motion!=Motion::Static){next.linear_velocity=next.enabled?vec_out(bi.GetLinearVelocity(body)):record.disabled_linear;next.angular_velocity=next.enabled?vec_out(bi.GetAngularVelocity(body)):record.disabled_angular;}out=next;return true;}
bool World::set_enabled(Id id,bool enabled){auto &p=*impl_;if(!p.owner_thread()||!p.scene)return false;auto &s=*p.scene;int i=body_index(s.document,id);if(i<0)return false;auto &record=s.records[unsigned(i)];auto &bi=s.physics.GetBodyInterface();if(bi.IsAdded(record.body)==enabled)return true;AllocationGuard guard(s.stats.query_allocations,s.scratch,s.stats.pooled_query_allocations);const bool moving=s.document.bodies[unsigned(i)].motion!=Motion::Static;if(enabled){bi.AddBody(record.body,moving?JPH::EActivation::Activate:JPH::EActivation::DontActivate);if(moving){bi.SetLinearVelocity(record.body,v3(record.disabled_linear));bi.SetAngularVelocity(record.body,v3(record.disabled_angular));}bi.InvalidateContactCache(record.body);}else{if(moving){record.disabled_linear=vec_out(bi.GetLinearVelocity(record.body));record.disabled_angular=vec_out(bi.GetAngularVelocity(record.body));}bi.RemoveBody(record.body);for(unsigned a=0;a<s.document.bodies.size();++a)if(s.document.bodies[a].sensor)for(unsigned b=0;b<s.document.bodies.size();++b)if((a==unsigned(i)||b==unsigned(i))&&((s.trigger_bits[a][b/64]>>(b%64))&1)){s.trigger_bits[a][b/64]&=~(1ull<<(b%64));s.push({EventKind::TriggerExit,s.document.bodies[a].id,s.document.bodies[b].id,pos_out(bi.GetPosition(s.records[b].body)),{},0});}}s.stats.pending_events=s.event_count;return true;}
bool World::impulse(Id id,Vec3 value){auto &p=*impl_;if(!p.owner_thread()||!p.scene||!vec(value,1e8))return false;int i=body_index(p.scene->document,id);if(i<0||p.scene->document.bodies[unsigned(i)].motion!=Motion::Dynamic)return false;auto &bi=p.scene->physics.GetBodyInterface();auto body=p.scene->records[unsigned(i)].body;if(!bi.IsAdded(body))return false;bi.AddImpulse(body,v3(value));return true;}
bool World::teleport(Id id,const Pose &pose,bool activate){auto &p=*impl_;if(!p.owner_thread()||!p.scene||!pose_ok(pose))return false;auto &s=*p.scene;int i=body_index(s.document,id);if(i<0)return false;for(const auto &d:s.document.doors)if(d.body==id)return false;auto &bi=s.physics.GetBodyInterface();auto body=s.records[unsigned(i)].body;bi.SetPositionAndRotation(body,r3(pose.position),q4(pose.rotation),activate&&bi.IsAdded(body)?JPH::EActivation::Activate:JPH::EActivation::DontActivate);return true;}
bool World::door_target(Id id,float radians){auto &p=*impl_;if(!p.owner_thread()||!p.scene||!std::isfinite(radians))return false;auto &s=*p.scene;for(unsigned i=0;i<s.document.doors.size();i++)if(s.document.doors[i].body==id){s.doors[i].target=std::clamp(radians,s.document.doors[i].minimum,s.document.doors[i].maximum);return true;}return false;}
bool World::raycast(Vec3 origin,Vec3 delta,Hit &out,Id ignore,unsigned layer)const{
    const auto &p=*impl_;if(!p.owner_thread()||!p.scene||!vec(origin)||!vec(delta,100000)||length(delta)<1e-8||layer>=layer_count)return false;auto &s=*p.scene;AllocationGuard guard(s.stats.query_allocations,s.scratch,s.stats.pooled_query_allocations);QueryFilter filter(s,ignore,layer);JPH::RRayCast ray(r3(origin),v3(delta));JPH::RayCastResult result;Hit next;
    if(s.physics.GetNarrowPhaseQuery().CastRay(ray,result,{},{},filter)){int i=s.index(result.mBodyID);if(i<0)return false;next.hit=true;next.body=s.document.bodies[unsigned(i)].id;next.fraction=result.mFraction;auto point=ray.GetPointOnRay(result.mFraction);next.position=pos_out(point);JPH::BodyLockRead lock(s.physics.GetBodyLockInterface(),result.mBodyID);if(lock.Succeeded())next.normal=vec_out(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2,point));}out=next;return true;
}
namespace {
struct MovementCastCollector final:JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> {
    JPH::Vec3 displacement;float tolerance;
    explicit MovementCastCollector(Vec3 delta):displacement(v3(delta)),tolerance(1e-6f*std::max(1.f,displacement.Length())){}
    void AddHit(const JPH::ShapeCastResult &hit)override{
        // A floor seam or an already touched jamb must not consume the closest
        // hit and hide a forward wall. Keep every inward or future contact.
        if(hit.mPenetrationAxis.LengthSq()>1e-16f){
            const auto outward=-hit.mPenetrationAxis.Normalized();
            if(outward.Dot(displacement)>=-tolerance)return;
        }
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>::AddHit(hit);
    }
};
template<class Collector>bool cast_with_collector(Scene &s,const JPH::Shape *shape,JPH::RMat44Arg transform,Vec3 delta,Hit &out,Id ignore,unsigned layer,Collector &collector,bool movement=false){
    // Jolt's active-edge correction needs the motion hint to retain a less
    // obstructive EPA normal when escaping or sliding along a mesh face.
    QueryFilter filter(s,ignore,layer);JPH::ShapeCastSettings settings;settings.mReturnDeepestPoint=true;settings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);if(movement)settings.mActiveEdgeMovementDirection=v3(delta);auto shape_cast=JPH::RShapeCast::sFromWorldTransform(shape,JPH::Vec3::sReplicate(1),transform,v3(delta));const auto base=transform.GetTranslation();s.physics.GetNarrowPhaseQuery().CastShape(shape_cast,settings,base,collector,{},{},filter);Hit next;
    if(collector.HadHit()){const auto &hit=collector.mHit;int i=s.index(hit.mBodyID2);if(i<0)return false;next.hit=true;next.body=s.document.bodies[unsigned(i)].id;next.fraction=std::max(hit.mFraction,0.f);next.position=pos_out(base+hit.mContactPointOn2);next.normal=vec_out(-hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sAxisY()));}out=next;return true;
}
bool cast(Scene &s,const JPH::Shape *shape,JPH::RMat44Arg transform,Vec3 delta,Hit &out,Id ignore,unsigned layer){JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;return cast_with_collector(s,shape,transform,delta,out,ignore,layer,collector);}
}
bool World::cast_capsule(Vec3 center,float radius,float half_height,Vec3 delta,Hit &out,Id ignore,unsigned layer)const{const auto &p=*impl_;if(!p.owner_thread()||!p.scene||!vec(center)||!vec(delta,100000)||!range(radius,.01,100)||!range(half_height,.01,100)||layer>=layer_count)return false;auto &s=*p.scene;AllocationGuard guard(s.stats.query_allocations,s.scratch,s.stats.pooled_query_allocations);JPH::CapsuleShape shape(half_height,radius);return cast(s,&shape,JPH::RMat44::sRotationTranslation(JPH::Quat::sIdentity(),r3(center)),delta,out,ignore,layer);}
bool World::cast_capsule_movement(Vec3 center,float radius,float half_height,Vec3 delta,Hit &out,Id ignore,unsigned layer)const{const auto &p=*impl_;if(!p.owner_thread()||!p.scene||!vec(center)||!vec(delta,100000)||!range(radius,.01,100)||!range(half_height,.01,100)||layer>=layer_count)return false;auto &s=*p.scene;AllocationGuard guard(s.stats.query_allocations,s.scratch,s.stats.pooled_query_allocations);JPH::CapsuleShape shape(half_height,radius);MovementCastCollector collector(delta);return cast_with_collector(s,&shape,JPH::RMat44::sRotationTranslation(JPH::Quat::sIdentity(),r3(center)),delta,out,ignore,layer,collector,true);}
bool World::sweep_body(Id id,Vec3 delta,Hit &out,unsigned layer)const{const auto &p=*impl_;if(!p.owner_thread()||!p.scene||!vec(delta,100000)||layer>=layer_count)return false;auto &s=*p.scene;int i=body_index(s.document,id);if(i<0||s.document.bodies[unsigned(i)].shape==Shape::Mesh||!s.physics.GetBodyInterface().IsAdded(s.records[unsigned(i)].body))return false;AllocationGuard guard(s.stats.query_allocations,s.scratch,s.stats.pooled_query_allocations);return cast(s,s.records[unsigned(i)].shape,s.physics.GetBodyInterface().GetWorldTransform(s.records[unsigned(i)].body),delta,out,id,layer);}
unsigned World::drain_events(Event *out,unsigned capacity){auto &p=*impl_;if(!p.owner_thread()||!p.scene||(!out&&capacity))return 0;auto &s=*p.scene;unsigned count=std::min(capacity,s.event_count);for(unsigned i=0;i<count;i++)out[i]=s.events[(s.event_head+i)%event_limit];s.event_head=(s.event_head+count)%event_limit;s.event_count-=count;s.stats.pending_events=s.event_count;return count;}
bool World::snapshot(Snapshot &out)const{const auto &p=*impl_;if(!p.owner_thread()||!p.scene)return false;const auto &s=*p.scene;Snapshot next;next.signature=s.signature;next.count=unsigned(s.document.bodies.size());next.door_count=unsigned(s.document.doors.size());next.accumulator=s.accumulator;for(unsigned i=0;i<next.count;i++)if(!body_state(s.document.bodies[i].id,next.bodies[i]))return false;next.doors=s.doors;out=next;return true;}
bool World::restore(const Snapshot &snapshot,std::string &error){
    auto &p=*impl_;if(!p.owner_thread()||!p.scene){error="Physics restore requires an active owner scene";return false;}const auto &old=*p.scene;
    if(snapshot.signature!=old.signature||snapshot.count!=old.document.bodies.size()||snapshot.door_count!=old.document.doors.size()||!range(snapshot.accumulator,0,fixed_step)){error="Snapshot does not match physics scene";return false;}
    for(unsigned i=0;i<snapshot.count;i++){const auto &b=snapshot.bodies[i];if(b.id!=old.document.bodies[i].id||(b.active&&!b.enabled)||!pose_ok(b.pose)||!vec(b.linear_velocity,500)||!vec(b.angular_velocity,100)){error="Invalid physics snapshot body";return false;}}
    for(unsigned i=0;i<snapshot.door_count;i++){const auto &state=snapshot.doors[i];const auto &d=old.document.doors[i];if(state.body!=d.body||!range(state.angle,d.minimum,d.maximum)||!range(state.target,d.minimum,d.maximum)){error="Invalid physics snapshot door";return false;}}
    auto next=std::make_unique<Scene>(old.document,old.profile);if(!next->build(error))return false;auto &bi=next->physics.GetBodyInterface();for(unsigned i=0;i<snapshot.count;i++){const auto &b=snapshot.bodies[i];auto &record=next->records[i];auto id=record.body;bi.SetPositionAndRotation(id,r3(b.pose.position),q4(b.pose.rotation),b.active&&b.enabled?JPH::EActivation::Activate:JPH::EActivation::DontActivate);if(next->document.bodies[i].motion!=Motion::Static){bi.SetLinearVelocity(id,v3(b.linear_velocity));bi.SetAngularVelocity(id,v3(b.angular_velocity));if(!b.active&&b.enabled)bi.DeactivateBody(id);}bi.InvalidateContactCache(id);if(!b.enabled){record.disabled_linear=b.linear_velocity;record.disabled_angular=b.angular_velocity;bi.RemoveBody(id);}}next->doors=snapshot.doors;next->accumulator=snapshot.accumulator;next->stats.accumulator=snapshot.accumulator;p.scene=std::move(next);error.clear();return true;
}
Stats World::stats()const{const auto &p=*impl_;return p.owner_thread()&&p.scene?p.scene->stats:Stats{};}
bool World::stop(){if(!impl_->owner_thread())return false;impl_->scene.reset();return true;}

bool save(const char *path,const Document &d,std::string &error){
    if(!path||!*path){error="Choose a physics file";return false;}if(!validate(d,error))return false;
    struct stat old={};if(lstat(path,&old)==0&&!S_ISREG(old.st_mode)){error="Physics destination must be a regular file";return false;}
    std::ostringstream out;out<<std::setprecision(17)<<"VALWOOD_PHYSICS 1\ngravity "<<d.gravity.x<<' '<<d.gravity.y<<' '<<d.gravity.z<<'\n';
    for(unsigned i=0;i<layer_count;i++)out<<"layer "<<i<<' '<<d.layer_masks[i]<<'\n';
    for(const auto &b:d.bodies){const auto &p=b.pose;const auto &q=p.rotation;
        out<<"body "<<b.id<<' '<<std::quoted(b.name)<<' '<<unsigned(b.shape)<<' '<<unsigned(b.motion)<<' '<<b.sensor<<' '<<b.layer<<' '<<p.position.x<<' '<<p.position.y<<' '<<p.position.z<<' '<<q.x<<' '<<q.y<<' '<<q.z<<' '<<q.w<<' '<<b.size.x<<' '<<b.size.y<<' '<<b.size.z<<' '<<b.friction<<' '<<b.restitution<<' '<<b.density<<' '<<b.mass<<' '<<b.gravity_factor<<' '<<b.linear_velocity.x<<' '<<b.linear_velocity.y<<' '<<b.linear_velocity.z<<' '<<b.angular_velocity.x<<' '<<b.angular_velocity.y<<' '<<b.angular_velocity.z<<' '<<b.allow_sleep<<'\n';
        for(auto v:b.vertices)out<<"vertex "<<b.id<<' '<<v.x<<' '<<v.y<<' '<<v.z<<'\n';
        for(auto t:b.triangles)out<<"triangle "<<b.id<<' '<<t[0]<<' '<<t[1]<<' '<<t[2]<<'\n';
    }
    for(const auto &door:d.doors)out<<"door "<<door.body<<' '<<door.pivot.x<<' '<<door.pivot.y<<' '<<door.pivot.z<<' '<<door.axis.x<<' '<<door.axis.y<<' '<<door.axis.z<<' '<<door.minimum<<' '<<door.maximum<<' '<<door.speed<<'\n';
    const auto data=out.str();if(data.size()>document_byte_limit){error="Physics document exceeds file budget";return false;}
    std::string pattern=std::string(path)+".tmp.XXXXXX";std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);int fd=mkstemp(temporary.data());if(fd<0){error="Cannot create physics save transaction";return false;}
    size_t at=0;bool ok=true;while(at<data.size()){ssize_t n=write(fd,data.data()+at,data.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=size_t(n);}if(ok)ok=fsync(fd)==0;int closed=close(fd);ok=ok&&closed==0;if(ok)ok=vw_replace(temporary.data(),path)==0;if(!ok){unlink(temporary.data());error="Physics save failed; previous file preserved";return false;}
    const char *slash=std::strrchr(path,'/');std::string directory=slash?std::string(path,size_t(slash-path)):".";if(directory.empty())directory="/";fd=vw_open(directory.c_str(),O_RDONLY|O_DIRECTORY);if(fd>=0){(void)fsync(fd);close(fd);}error.clear();return true;
}
bool load(const char *path,Document &out,std::string &error){
    error.clear();if(!path||!*path){error="Choose a physics file";return false;}int fd=vw_open(path,O_RDONLY|O_NOFOLLOW);if(fd<0){error="Cannot open physics document";return false;}
    struct stat info={};if(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_size<0||std::uint64_t(info.st_size)>document_byte_limit){close(fd);error="Invalid/oversized physics document";return false;}
    std::string data(size_t(info.st_size),'\0');size_t at=0;bool ok=true;while(at<data.size()){ssize_t n=read(fd,data.data()+at,data.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=size_t(n);}close(fd);if(!ok||data.find('\0')!=std::string::npos){error="Incomplete/binary physics document";return false;}
    std::istringstream file(data);std::string row;std::getline(file,row);if(row!="VALWOOD_PHYSICS 1"){error="Unsupported physics document";return false;}Document next;std::array<bool,layer_count> layers={};bool gravity=false;unsigned line=1,vertices=0,triangles=0;
    while(std::getline(file,row)){
        line++;std::istringstream in(row);std::string kind;in>>kind;if(kind.empty()||kind[0]=='#')continue;bool parsed=false;
        if(kind=="gravity"){parsed=!gravity&&bool(in>>next.gravity.x>>next.gravity.y>>next.gravity.z);gravity=true;}
        else if(kind=="layer"){unsigned index=0,mask=0;parsed=bool(in>>index>>mask)&&index<layer_count&&mask<=255;if(parsed){parsed=!layers[index];layers[index]=true;next.layer_masks[index]=mask;}}
        else if(kind=="body"&&next.bodies.size()<body_limit){Body b;unsigned shape=0,motion=0;int sensor=0,sleep=0;auto &p=b.pose;auto &q=p.rotation;
            parsed=read_id(in,b.id)&&bool(in>>std::quoted(b.name)>>shape>>motion>>sensor>>b.layer>>p.position.x>>p.position.y>>p.position.z>>q.x>>q.y>>q.z>>q.w>>b.size.x>>b.size.y>>b.size.z>>b.friction>>b.restitution>>b.density>>b.mass>>b.gravity_factor>>b.linear_velocity.x>>b.linear_velocity.y>>b.linear_velocity.z>>b.angular_velocity.x>>b.angular_velocity.y>>b.angular_velocity.z>>sleep)&&sensor>=0&&sensor<=1&&sleep>=0&&sleep<=1;b.shape=Shape(shape);b.motion=Motion(motion);b.sensor=sensor==1;b.allow_sleep=sleep==1;next.bodies.push_back(std::move(b));
        }else if(kind=="vertex"&&vertices<vertex_limit){Id id=0;Vec3 v;parsed=read_id(in,id)&&bool(in>>v.x>>v.y>>v.z);int index=body_index(next,id);if(index<0)parsed=false;else{next.bodies[unsigned(index)].vertices.push_back(v);vertices++;}}
        else if(kind=="triangle"&&triangles<triangle_limit){Id id=0;std::array<std::uint32_t,3> t={};parsed=read_id(in,id)&&bool(in>>t[0]>>t[1]>>t[2]);int index=body_index(next,id);if(index<0)parsed=false;else{next.bodies[unsigned(index)].triangles.push_back(t);triangles++;}}
        else if(kind=="door"&&next.doors.size()<door_limit){Door door;parsed=read_id(in,door.body)&&bool(in>>door.pivot.x>>door.pivot.y>>door.pivot.z>>door.axis.x>>door.axis.y>>door.axis.z>>door.minimum>>door.maximum>>door.speed);next.doors.push_back(door);}
        std::string extra;if(!parsed||(in>>extra)){error="Invalid physics record at line "+std::to_string(line);return false;}
    }
    // Every v1 save carries these records. Quietly filling a truncated layer
    // table from defaults can enable collisions that the author disabled.
    if(!gravity||!std::all_of(layers.begin(),layers.end(),[](bool present){return present;})){error="Incomplete physics gravity/collision layer table";return false;}
    if(!validate(next,error))return false;
    out=std::move(next);return true;
}
}
