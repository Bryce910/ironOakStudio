#include "engine/world_horizon_grid.h"
#include "engine/world_selection_membership.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>

namespace {bool track=false;std::size_t allocations=0;}
[[gnu::noinline]] void *operator new(std::size_t n){if(track)allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
struct V3 {float x,y,z;};
struct Camera {
    double x=0,y=0,z=0;float yaw=0,pitch=0,aspect=16.f/9;
    V3 forward()const{return {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};}
    V3 right()const{return {std::cos(yaw),0,std::sin(yaw)};}
};
struct Program {unsigned id=1;void i(const char*,int){}void f(const char*,float){}void v3(const char*,V3){}void v2(const char*,float,float){}};
struct WorldModelAsset {std::size_t count=0;};
WorldModelAsset *world_model_load(const char*){return new WorldModelAsset;}
void world_model_free(WorldModelAsset *a){delete a;}
bool world_model_upload_instances(WorldModelAsset *a,const WorldModelInstance*,std::size_t count){a->count=count;return true;}
void world_model_draw(WorldModelAsset*,unsigned){}
#include "horizon_grid_viewer.inc"

using Pairs=std::vector<std::pair<double,std::size_t>>;
using Buckets=std::array<Pairs,8>;
static WorldHorizonGrid::View view(const Camera &cam,double ax,double az,bool mirror){
    WorldHorizonGrid::View v;const auto f=cam.forward(),r=cam.right();
    v.anchor_x=ax;v.anchor_z=az;v.x=cam.x;v.y=cam.y;v.z=cam.z;
    v.forward_x=f.x;v.forward_y=f.y;v.forward_z=f.z;v.right_x=r.x;v.right_z=r.z;
    v.aspect=cam.aspect;v.pool_mirror=mirror;v.pool_y=134;return v;
}
// Independent old point predicate. Do not replace it with grid support math.
static void select(const std::vector<WorldModelInstance> &roots,const WorldHorizonGrid::View &v,Buckets &out,const WorldHorizonGrid *grid){
    for(auto &b:out)b.clear();
    for(std::size_t i=0;i<roots.size();i++) {
        if(grid&&!grid->includes(i))continue;
        const auto &a=roots[i];
        const double dx=v.anchor_x+a.x-v.x,dz=v.anchor_z+a.z-v.z,d2=dx*dx+dz*dz;
        if(d2<1450.*1450.||d2>12000.*12000.)continue;
        const double horizontal=dx*v.forward_x+dz*v.forward_z;
        double depth=std::max(horizontal+(a.y+17*a.scale-v.y)*v.forward_y,
            horizontal-(a.y+17*a.scale+v.y)*v.forward_y);
        if(v.pool_mirror)depth=std::max(depth,horizontal-(a.y+17*a.scale+v.y-2*v.pool_y)*v.forward_y);
        if(depth<-45||std::fabs(dx*v.right_x+dz*v.right_z)>std::max(0.,depth)*v.aspect*std::tan(.54)+55)continue;
        out[(i*73+i/19)%8].push_back({d2,i});
    }
}
static std::uint64_t random_bits(std::uint64_t &state){state^=state>>12;state^=state<<25;state^=state>>27;return state*2685821657736338717ull;}
static double random_unit(std::uint64_t &state){return double(random_bits(state)>>11)*(1./9007199254740992.);}
static void independent_tests(){
    WorldHorizonGrid grid;assert(grid.initialize(100000));
    std::vector<WorldModelInstance> roots;roots.reserve(100000);std::uint64_t state=79314875;
    for(std::size_t i=0;i<100000;i++)roots.push_back({float((random_unit(state)-.5)*25000),float(random_unit(state)*1800-100),float((random_unit(state)-.5)*25000),0,float(.25+random_unit(state)*3)});
    // Exact radial endpoints, both sides of float boundaries, equal-distance
    // ties, cell edges and negative coordinates are deliberate controls.
    for(std::size_t i=0;i<96;i++) {
        float d=i%3==0?1450.f:i%3==1?12000.f:1024.f;
        if(i%4==1)d=std::nextafter(d,0.f);
        if(i%4==2)d=std::nextafter(d,INFINITY);
        roots[i]={i%2?d:-d,float(i%9)*17,0,0,1};
    }
    Buckets brute,fast;for(auto *bs:{&brute,&fast})for(auto &b:*bs)b.reserve(100000);
    const float pitches[]={-1.52f,-.5f,-.08f,0.f,.08f,.5f,1.52f};
    const float aspects[]={.5f,16.f/9.f,3.7f};
    std::size_t views=0,candidates=0,accepted=0,overflows=0;
    track=true;allocations=0;void *control=::operator new(17);track=false;
    assert(allocations==1);::operator delete(control);
    track=true;allocations=0;assert(grid.publish(roots.data(),roots.size()));
    for(unsigned angle=0;angle<24;angle++)for(float pitch:pitches)for(float aspect:aspects)for(bool mirror:{false,true}) {
        Camera cam;cam.x=-27648+(angle%3)*37.;cam.z=9216-(angle%4)*17.;cam.y=angle%2?1450:135.75;
        cam.yaw=angle*.2617993878f;cam.pitch=pitch;cam.aspect=aspect;
        const auto v=view(cam,-27648,9216,mirror);const auto stats=grid.classify(v);assert(stats.accelerated);
        select(roots,v,brute,nullptr);select(roots,v,fast,&grid);assert(brute==fast);
        candidates+=stats.candidate_roots;for(auto &b:fast)accepted+=b.size();
        for(unsigned bucket=0;bucket<8;bucket++) {
            overflows+=brute[bucket].size()>3500;
            HorizonForest::bound_selection(brute[bucket]);HorizonForest::bound_selection(fast[bucket]);
            assert(brute[bucket]==fast[bucket]);
        }
        views++;
    }
    track=false;assert(allocations==0&&overflows>0);
    // Publication replacement and failures must fail open rather than reuse a
    // stale cell mask. Classification failure also restores the full scan.
    auto invalid=view(Camera{},0,0,false);invalid.forward_x=NAN;assert(!grid.classify(invalid).accelerated);
    for(std::size_t i=0;i<roots.size();i++)assert(grid.includes(i));
    const auto saved=roots[0];roots[0].x=NAN;assert(!grid.publish(roots.data(),roots.size()));
    assert(!grid.classify(view(Camera{},0,0,false)).accelerated);
    for(std::size_t i=0;i<roots.size();i++)assert(grid.includes(i));
    roots[0]=saved;assert(grid.publish(roots.data(),roots.size()));
    roots[0].x=-1e6f;assert(!grid.publish(roots.data(),roots.size()));assert(grid.includes(0));
    roots[0]=saved;assert(!grid.publish(roots.data(),100001));assert(grid.includes(99999));
    assert(grid.publish<WorldModelInstance>(nullptr,0));assert(grid.classify(view(Camera{},0,0,false)).accelerated);
    // Same-count valid publication reorders arbitrary source IDs into entirely
    // different cells; no previous publication's keep mask may survive it.
    std::rotate(roots.begin(),roots.begin()+173,roots.end());
    assert(grid.publish(roots.data(),roots.size()));
    Camera replacement;replacement.y=135.75;replacement.pitch=.08f;
    const auto replacement_view=view(replacement,0,0,true);
    assert(grid.classify(replacement_view).accelerated);
    select(roots,replacement_view,brute,nullptr);select(roots,replacement_view,fast,&grid);assert(brute==fast);
    std::printf("PASS independent horizon grid:100000 roots,%zu views, uncapped exact pair/source order and capped nth/tie parity, invalid/replaced/empty publication fallback, zero hot allocations; candidates%.2f%%, selected%zu\n",views,100.*candidates/(views*roots.size()),accepted);
}
static void actual_source_tests(bool benchmark){
    assert(world_atlas_init());
    HorizonForest fast;LegacyHorizonForest brute;assert(fast.init()&&brute.init());
    const auto *pool=world_atlas_rainforest_pool();Camera start;
    start.x=pool->x+7;start.z=pool->z+120;start.y=world_atlas_height(start.x,start.z)+1.75;start.yaw=.2f;start.pitch=.015f;
    // Execute the production root generator and publication once. Its source
    // order is the bounded stable-rank heap, not a synthetic spatial ordering.
    fast.update(start,1000000);assert(!fast.building&&fast.live.size()>50000);
    brute.live=fast.live;brute.live_bucket=fast.live_bucket;brute.live_x=fast.live_x;brute.live_z=fast.live_z;
    brute.target_x=fast.target_x;brute.target_z=fast.target_z;brute.live_publication=fast.live_publication;
    std::vector<Camera> views;views.reserve(64);
    for(unsigned i=0;i<64;i++) {
        Camera cam=start;cam.x+=double(i%11)*1.37;cam.z-=double(i%17)*13.;cam.y=world_atlas_height(cam.x,cam.z)+1.75;
        cam.yaw=float(i)*.0981747704f;cam.pitch=i%3==0?-.08f:i%3==1?.15f:-.45f;cam.aspect=i%5==0?2.8f:16.f/9.f;views.push_back(cam);
    }
    track=true;allocations=0;
    for(const auto &cam:views) {
        fast.has_selection=brute.has_selection=false;fast.update(cam,0);brute.update(cam,0);
        for(unsigned b=0;b<8;b++) {
            assert(fast.buckets[b].selected==brute.buckets[b].selected);
            const auto &a=fast.buckets[b].uploaded,&c=brute.buckets[b].uploaded;
            assert(a.size()==c.size()&&(a.empty()||!std::memcmp(a.data(),c.data(),a.size()*sizeof(WorldModelInstance))));
        }
        assert(fast.drawn==brute.drawn);
    }
    track=false;assert(allocations==0);
    std::printf("PASS actual HorizonForest source: real atlas field%zu roots,%zu camera views; exact capped pair/order/upload bytes and counts, zero selection allocations\n",fast.live.size(),views.size());
    if(benchmark) {
        // Alternating control/accelerated rounds limit temperature/order bias.
        // Includes point testing, nth partitioning and membership checks; GL
        // calls are inert stubs, so this is only aCPU selector microbenchmark.
        double baseline_ms=0,accelerated_ms=0;
        auto time=[&](auto &forest){const auto begin=std::chrono::steady_clock::now();for(const auto &cam:views){forest.has_selection=false;forest.update(cam,0);}return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();};
        for(unsigned round=0;round<10;round++)if(round%2){accelerated_ms+=time(fast);baseline_ms+=time(brute);}else{baseline_ms+=time(brute);accelerated_ms+=time(fast);}
        std::printf("BENCH actual%zu-root selector:%zu views x10, baseline%.6f ms/selection, grid%.6f ms/selection, speedup%.3fx; CPU-only,noGPU/frame-rate claim\n",fast.live.size(),views.size(),baseline_ms/(views.size()*10),accelerated_ms/(views.size()*10),baseline_ms/accelerated_ms);
    }
    fast.release();brute.release();world_atlas_shutdown();
}
int main(int argc,char **argv){independent_tests();actual_source_tests(argc>1&&!std::strcmp(argv[1],"--benchmark"));}
