#include "engine/world_cover.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

struct Snapshot{WorldCoverField field{};std::vector<WorldModelInstance> copy;};
static double full_pump_ms=0,max_pump_ms=0;
static size_t full_pumps=0;
static unsigned build(WorldCover *cover,Snapshot snapshots[WORLD_COVER_KIND_COUNT],size_t budget,unsigned expected_fields=WORLD_COVER_KIND_COUNT){
    unsigned calls=0,fields=0;
    for(;;){
        auto start=std::chrono::steady_clock::now();
        size_t processed=world_cover_pump(cover,budget);assert(processed<=budget&&processed<=4096);calls++;
        if(processed&&budget==4096){
            double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            full_pump_ms+=elapsed;max_pump_ms=std::max(max_pump_ms,elapsed);full_pumps++;
        }
        WorldCoverField field;
        while(world_cover_poll(cover,&field)){
            assert(field.kind>=0&&field.kind<WORLD_COVER_KIND_COUNT);
            snapshots[field.kind].field=field;
            snapshots[field.kind].copy.assign(field.instances,field.instances+field.count);fields++;
        }
        WorldCoverMetrics metrics;world_cover_metrics(cover,&metrics);
        assert(metrics.near_done<=metrics.near_total&&metrics.tree_done<=metrics.tree_total);
        if(!metrics.near_building&&!metrics.tree_building)break;
        assert(calls<100000);
    }
    assert(fields==expected_fields);return calls;
}
static uint64_t root_id(const WorldModelInstance &instance){
    // Independent world-seeded yaw/scale identify the same generated root
    // without relying on array order or anchor-relative float coordinates.
    uint32_t yaw,scale;std::memcpy(&yaw,&instance.yaw,sizeof(yaw));std::memcpy(&scale,&instance.scale,sizeof(scale));
    return uint64_t(yaw)<<32|scale;
}
static uint64_t fingerprint(const Snapshot &snapshot){
    // Captured before the rainforest change from a biome-weight-zero forest.
    // Ordering and every uploaded float are part of the retained contract.
    uint64_t result=14695981039346656037ull;
    const auto *bytes=reinterpret_cast<const unsigned char*>(snapshot.copy.data());
    for(size_t i=0;i<snapshot.copy.size()*sizeof(WorldModelInstance);++i){result^=bytes[i];result*=1099511628211ull;}
    return result;
}
static size_t retained_protected_roots(const Snapshot &before,const Snapshot &after,bool grass,size_t minimum=0){
    std::unordered_map<uint64_t,const WorldModelInstance*> roots;
    for(const auto &instance:after.copy)assert(roots.emplace(root_id(instance),&instance).second);
    double radius=grass?WORLD_COVER_GRASS_PROTECTED_RADIUS-2:WORLD_COVER_TREE_PROTECTED_RADIUS-2;
    double midpoint=(before.field.anchor_x+after.field.anchor_x)*.5;
    size_t checked=0;
    for(const auto &instance:before.copy){
        double x=before.field.anchor_x+instance.x,z=before.field.anchor_z+instance.z;
        if(std::hypot(x-before.field.anchor_x,z-before.field.anchor_z)>=radius||
           std::hypot(x-after.field.anchor_x,z-after.field.anchor_z)>=radius)continue;
        // A grass cell moving nearer to the new anchor keeps its prefix of
        // roots. The 3m margin keeps the complete tuft cell on that side of
        // the bisector, allowing rooted spread around the cell centre.
        if(grass&&x<=midpoint+3)continue;
        auto found=roots.find(root_id(instance));
        assert(found!=roots.end());
        assert(std::fabs(x-(after.field.anchor_x+found->second->x))<.001);
        assert(std::fabs(z-(after.field.anchor_z+found->second->z))<.001);
        assert(instance.y==found->second->y);
        checked++;
    }
    assert(checked>(minimum?minimum:(grass?1000u:100u)));
    return checked;
}
static void validate(const Snapshot snapshots[WORLD_COVER_KIND_COUNT]){
    const size_t caps[]={WORLD_COVER_GRASS_CAP,WORLD_COVER_TREE_CAP,WORLD_COVER_ROCK_CAP,WORLD_COVER_FERN_CAP};
    const double radii[]={108,1750,230,150};
    for(unsigned kind=0;kind<WORLD_COVER_KIND_COUNT;kind++){
        const Snapshot &snapshot=snapshots[kind];assert(snapshot.copy.size()<=caps[kind]);
        for(const WorldModelInstance &i:snapshot.copy){
            assert(std::isfinite(i.x)&&std::isfinite(i.y)&&std::isfinite(i.z)&&std::isfinite(i.yaw));
            assert(i.scale>0&&i.scale<2&&std::hypot(i.x,i.z)<=radii[kind]+.01);
            WorldAtlasSample a=world_atlas_sample(snapshot.field.anchor_x+i.x,snapshot.field.anchor_z+i.z);
            assert(a.height>2&&a.shore_distance>7.99&&a.land_weight>.75);
            assert(std::fabs(a.height-i.y)<.01);
            WorldAtlasLandscapeSample ecology=world_atlas_landscape_mask(snapshot.field.anchor_x+i.x,snapshot.field.anchor_z+i.z);
            assert(ecology.height==0&&ecology.trail<=.011f);
            auto start=world_atlas_starting_sample(snapshot.field.anchor_x+i.x,snapshot.field.anchor_z+i.z);
            assert(start.path<=.011);
            if(kind==WORLD_COVER_GRASS)assert(start.paving<=.121&&start.grass_exclusion<=.011);
            else if(kind==WORLD_COVER_TREES)assert(start.clearing<=.011);
            else assert(start.clearing<=.301);
            if(kind==WORLD_COVER_GRASS)assert(ecology.rock<.7801f);
            if(kind==WORLD_COVER_TREES)assert(ecology.rock<.7501f&&ecology.meadow<.8501f);
        }
    }
}
int main(){
 assert(world_atlas_init());auto*a=world_cover_init();auto*b=world_cover_init();assert(a&&b);
 double x,z;world_atlas_uv_to_world(.52,.24,&x,&z);
 unsigned compared=0;
 for(double shift:{0.,24.,350.}){
  assert(world_cover_update(a,x+shift,z)&&world_cover_update(b,x+shift,z));
  Snapshot first[WORLD_COVER_KIND_COUNT],repeat[WORLD_COVER_KIND_COUNT];
  WorldCoverMetrics ma{},mb{};world_cover_metrics(a,&ma);world_cover_metrics(b,&mb);
  const unsigned fields=3+unsigned(ma.tree_building);
  build(a,first,1024,fields);build(b,repeat,137,fields);
  for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;++k){
   assert(first[k].copy.size()==repeat[k].copy.size());
   assert(!std::memcmp(first[k].copy.data(),repeat[k].copy.data(),first[k].copy.size()*sizeof(WorldModelInstance)));
   compared+=first[k].copy.size();
  }
 }
 world_cover_shutdown(a);world_cover_shutdown(b);world_atlas_shutdown();
 std::printf("PASS: %u roots exactly equal across 1024/137 work budgets and three cache anchors.\n",compared);
}
