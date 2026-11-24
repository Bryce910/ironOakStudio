#include "engine/world_cover.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <numeric>
#include <random>
#include <vector>
// Expose only these fully included test classes, never standard-library code.
#define private public
#include "engine/world_grass_transition.h"
#include "tests/reference/world_grass_transition_legacy.h"
#undef private

namespace {bool armed=false;std::size_t allocations=0,comparisons=0;}
[[gnu::noinline]] void *operator new(std::size_t n){if(armed)allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
static std::uint64_t identity(const WorldModelInstance &a){std::uint32_t y,s;std::memcpy(&y,&a.yaw,4);std::memcpy(&s,&a.scale,4);return(std::uint64_t(y)<<32)|s;}
static WorldModelInstance root(std::uint32_t id){const std::uint32_t y=0x3f000000u+(id&0x7fffffu),s=0x3f800000u+(id>>23);WorldModelInstance a={float(int(id%401)-200),25.f+float(id%79)*.01f,float(int(id%317)-158),0,0};std::memcpy(&a.yaw,&y,4);std::memcpy(&a.scale,&s,4);return a;}
template<class T> static void bytes(const std::vector<T> &a,const std::vector<T> &b){assert(a.size()==b.size()&&(a.empty()||!std::memcmp(a.data(),b.data(),a.size()*sizeof(T))));}
static std::vector<std::pair<std::uint64_t,std::size_t>> canonical;
static void equal(const WorldGrassTransition &a,const LegacyWorldGrassTransition &b){
    const auto &x=a.field(),&y=b.field();assert(x.kind==y.kind&&x.generation==y.generation&&x.anchor_x==y.anchor_x&&x.anchor_z==y.anchor_z&&x.count==y.count);
    assert(!x.count||!std::memcmp(x.instances,y.instances,x.count*sizeof(WorldModelInstance)));bytes(a.fades(),b.fades());
    assert(a.active_==b.active_&&a.initialized_==b.initialized_&&a.structure_revision()==b.structure_revision()&&a.states_.size()==b.states_.size());
    for(std::size_t i=0;i<a.states_.size();i++)assert(!std::memcmp(&a.states_[i].world_x,&b.states_[i].world_x,sizeof(double))&&!std::memcmp(&a.states_[i].world_z,&b.states_[i].world_z,sizeof(double))&&a.states_[i].desired==b.states_[i].desired);
    canonical.clear();for(std::size_t i=0;i<x.count;i++)canonical.push_back({identity(x.instances[i]),i});std::sort(canonical.begin(),canonical.end());
    assert(canonical.size()==a.lookup_.size());for(std::size_t i=0;i<canonical.size();i++)assert(canonical[i].first==a.lookup_[i].id&&canonical[i].second==a.lookup_[i].index);
    comparisons++;
}
static void replace(WorldGrassTransition &a,LegacyWorldGrassTransition &b,const WorldCoverField &field){armed=true;allocations=0;const bool x=a.replace(field),y=b.replace(field);armed=false;assert(x==y&&allocations==0);equal(a,b);}
static void advance(WorldGrassTransition &a,LegacyWorldGrassTransition &b,double dt){armed=true;allocations=0;const bool x=a.advance(dt),y=b.advance(dt);armed=false;assert(x==y&&allocations==0);equal(a,b);}
static WorldCoverField field(const std::vector<WorldModelInstance> &v,double x,double z,std::uint64_t gen){return{WORLD_COVER_GRASS,gen,x,z,v.size(),v.data()};}
static void differential(){
    canonical.reserve(WorldGrassTransition::max_instances);WorldGrassTransition a;LegacyWorldGrassTransition b;
    std::vector<WorldModelInstance> incoming;incoming.reserve(WORLD_COVER_GRASS_CAP);
    std::vector<std::size_t> order;order.reserve(WorldGrassTransition::max_instances);std::mt19937 random(0x90177u);std::uint32_t next_id=1;
    armed=true;allocations=0;void *control=::operator new(17);armed=false;assert(allocations==1);::operator delete(control);
    equal(a,b);const std::size_t sizes[]={0,1,2,19,1000,8454,8602,46000};double x=-26736,z=9744;
    for(unsigned generation=1;generation<=160;generation++){
        if(generation%29==0){armed=true;allocations=0;a.reset();b.reset();armed=false;assert(allocations==0);equal(a,b);}
        const auto n=sizes[random()%std::size(sizes)];order.resize(a.field().count);std::iota(order.begin(),order.end(),0);std::shuffle(order.begin(),order.end(),random);
        incoming.clear();const std::size_t common=std::min(n*3/5,order.size());
        for(std::size_t i=0;i<common;i++){auto r=a.field().instances[order[i]];r.x+=.0003f;r.y+=.0002f;r.z-=.0002f;incoming.push_back(r);}
        while(incoming.size()<n)incoming.push_back(root(next_id++));
        std::shuffle(incoming.begin(),incoming.end(),random);
        x+=generation%13==0?161:generation%11==0?-160:generation%7==0?0:24;z+=generation%5==0?-24:0;
        auto desired=field(incoming,x,z,generation);replace(a,b,desired);
        advance(a,b,generation%4==0?.4:generation%4==1?1./60.:generation%4==2?.000001:.17);
        // Full content/state equivalence after every rejected input, including
        // scratch-mutating late validation failures and duplicate identities.
        auto invalid=desired;invalid.anchor_x=NAN;replace(a,b,invalid);
        invalid=desired;invalid.count=WORLD_COVER_GRASS_CAP+1;replace(a,b,invalid);
        if(!incoming.empty()){
            invalid=desired;invalid.instances=nullptr;replace(a,b,invalid);
            const auto original=incoming[0];incoming[0].scale=NAN;replace(a,b,desired);incoming[0]=original;
            if(incoming.size()>1){const auto second=incoming[1];incoming[1]=incoming[0];replace(a,b,desired);incoming[1]=second;}
        }
        advance(a,b,0);advance(a,b,-1);advance(a,b,NAN);advance(a,b,INFINITY);
        if(generation%3==0)advance(a,b,.8); // actual arbitrary-position compaction
        if(generation%4==0){std::reverse(incoming.begin(),incoming.end());replace(a,b,desired);advance(a,b,.16);} // same count/generation reorder/reentry
    }
    // Saturated disjoint replacements must retain the exact old opacity/ID
    // overflow ranking and desired-first output order across rapid reentry.
    incoming.resize(WORLD_COVER_GRASS_CAP);
    for(unsigned generation=0;generation<10;generation++){
        for(auto &r:incoming)r=root(next_id++);
        std::shuffle(incoming.begin(),incoming.end(),random);
        replace(a,b,field(incoming,1000+generation*24.,2000,generation+1000));advance(a,b,.03+generation*.001);
        if(generation%2)advance(a,b,.01);
    }
    assert(a.field().count==WorldGrassTransition::max_instances);advance(a,b,.8);assert(a.field().count==WORLD_COVER_GRASS_CAP);
    std::reverse(incoming.begin(),incoming.end());replace(a,b,field(incoming,1240,2000,2000));
    incoming.clear();replace(a,b,field(incoming,1240,2000,2001));advance(a,b,.8);assert(a.field().count==0&&a.lookup_.empty());
    std::printf("PASS transition differential:%zu full field/fade/world-state/revision comparisons against frozen production legacy; sorted-cache invariant after every operation; random reentry/reorder/compaction/failure/teleport/reset/empty/saturated overflow; zero guarded allocations\n",comparisons);
}
struct Snapshot {WorldCoverField metadata={};std::vector<WorldModelInstance> roots;WorldCoverField get()const{auto f=metadata;f.count=roots.size();f.instances=roots.data();return f;}};
static Snapshot publish(WorldCover *cover,double x,double z){
    assert(world_cover_update(cover,x,z));Snapshot out;
    for(unsigned i=0;;i++){assert(i<2000);world_cover_pump(cover,4096);WorldCoverField f;while(world_cover_poll(cover,&f))if(f.kind==WORLD_COVER_GRASS){out.metadata=f;out.roots.assign(f.instances,f.instances+f.count);}WorldCoverMetrics m;world_cover_metrics(cover,&m);if(!m.near_building&&!m.tree_building)break;}
    return out;
}
static void benchmark_case(const char *name,const Snapshot &before,const Snapshot &after){
    WorldGrassTransition a;LegacyWorldGrassTransition b;std::vector<double> old_times,new_times,old_compaction,new_compaction;old_times.reserve(40);new_times.reserve(40);old_compaction.reserve(40);new_compaction.reserve(40);
    auto timed=[](auto &registry,const WorldCoverField &input){const auto begin=std::chrono::steady_clock::now();assert(registry.replace(input));return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();};
    for(unsigned repeat=0;repeat<40;repeat++){
        a.reset();b.reset();assert(a.replace(before.get())&&b.replace(before.get()));
        double old_ms,new_ms;if(repeat%2){new_ms=timed(a,after.get());old_ms=timed(b,after.get());}else{old_ms=timed(b,after.get());new_ms=timed(a,after.get());}
        equal(a,b);
        auto compact=[](auto &registry){const auto begin=std::chrono::steady_clock::now();registry.advance(.8);return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();};
        double old_advance,new_advance;if(repeat%2){new_advance=compact(a);old_advance=compact(b);}else{old_advance=compact(b);new_advance=compact(a);}
        equal(a,b);if(repeat>=4){old_times.push_back(old_ms);new_times.push_back(new_ms);old_compaction.push_back(old_advance);new_compaction.push_back(new_advance);}
    }
    auto mean=[](const auto &v){return std::accumulate(v.begin(),v.end(),0.)/v.size();};
    std::sort(old_times.begin(),old_times.end());std::sort(new_times.begin(),new_times.end());
    std::printf("BENCH actual%s cover%zu->%zu, alternating40rounds/4warmup: legacy mean%.6f/median%.6f ms; linear mean%.6f/median%.6f ms; speedup%.3fx; exact all-byte/state parity each round\n",name,before.roots.size(),after.roots.size(),mean(old_times),old_times[old_times.size()/2],mean(new_times),new_times[new_times.size()/2],mean(old_times)/mean(new_times));
    std::printf("BENCH actual%s compaction: legacy mean%.6f ms; maintained-cache mean%.6f ms (extra%.6f ms); exact all-byte/state parity\n",name,mean(old_compaction),mean(new_compaction),mean(new_compaction)-mean(old_compaction));
}
static void benchmark(){
    assert(world_atlas_init());std::size_t count=0;const auto *p=world_atlas_rainforest_trail_points(&count);assert(count==6);auto *cover=world_cover_init();assert(cover);
    const auto first=publish(cover,p[0].x,p[0].z),second=publish(cover,p[0].x,p[0].z-4),third=publish(cover,p[0].x+4,p[0].z-8);
    benchmark_case(" rainforest generation2",first,second);benchmark_case(" rainforest generation3",second,third);
    double x,z;world_atlas_village_origin(&x,&z);const auto castle=publish(cover,x+80,z-80),castle_next=publish(cover,x+104,z-80);benchmark_case(" castle",castle,castle_next);
    world_cover_shutdown(cover);world_atlas_shutdown();
}
int main(int argc,char **argv){differential();if(argc>1&&!std::strcmp(argv[1],"--benchmark"))benchmark();}
