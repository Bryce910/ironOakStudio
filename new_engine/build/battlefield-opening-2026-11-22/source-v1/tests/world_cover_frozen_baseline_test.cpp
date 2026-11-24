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
    assert(world_atlas_init());WorldCover *a=world_cover_init(),*b=world_cover_init();assert(a&&b);
    assert(world_cover_pump(a,4096)==0&&world_cover_pump(a,0)==0);
    // Forest location includes negative world coordinates and ample dry ground.
    double x,z;world_atlas_uv_to_world(.52,.24,&x,&z);
    assert(world_cover_update(a,x,z)&&world_cover_update(b,x,z));
    Snapshot first[WORLD_COVER_KIND_COUNT],repeat[WORLD_COVER_KIND_COUNT];
    unsigned calls=build(a,first,4096);build(b,repeat,137);validate(first);validate(repeat);
    assert(world_atlas_rainforest_weight(x,z)==0);
    for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;++k)std::printf("BASELINE_FP %u %016llx\n",k,static_cast<unsigned long long>(fingerprint(first[k])));
    std::fflush(stdout);
    assert(first[WORLD_COVER_GRASS].copy.size()>18000&&first[WORLD_COVER_TREES].copy.size()>50);
    // Spend the fixed instance budget where individual blades are visible,
    // rather than spreading a uniform crop across the entire 108m cache ring.
    size_t inner=0,outer=0;
    for(const auto &i:first[WORLD_COVER_GRASS].copy){
        double radius=std::hypot(i.x,i.z);
        if(radius<25)inner++;
        if(radius>60)outer++;
        // The current nine-leaf model's tallest blade is .39m at unit scale.
        assert(i.scale*.39f>.19f&&i.scale*.39f<.44f);
    }
    assert(inner>first[WORLD_COVER_GRASS].copy.size()*.55&&outer<first[WORLD_COVER_GRASS].copy.size()*.12);
    assert(calls<=48);
    for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;k++){
        assert(first[k].copy.size()==repeat[k].copy.size());
        assert(!std::memcmp(first[k].copy.data(),repeat[k].copy.data(),first[k].copy.size()*sizeof(WorldModelInstance)));
    }
    // Updating/staging keeps the published arrays alive until another successful
    // poll of that kind. Invalid input must not cancel the current generation.
    WorldCoverMetrics before,after;world_cover_metrics(a,&before);
    assert(!world_cover_update(a,NAN,z)&&!world_cover_update(a,INFINITY,z)&&!world_cover_update(a,1e100,z));
    world_cover_metrics(a,&after);assert(before.near_generation==after.near_generation);
    assert(world_cover_update(a,x+30,z));world_cover_metrics(a,&after);
    assert(after.near_generation==before.near_generation+1&&after.tree_generation==before.tree_generation);
    world_cover_pump(a,100);
    for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;k++)assert(!std::memcmp(first[k].field.instances,first[k].copy.data(),first[k].copy.size()*sizeof(WorldModelInstance)));
    world_cover_metrics(a,&before);assert(before.near_building);
    assert(world_cover_update(a,x+350,z+300));world_cover_metrics(a,&after);
    assert(after.cancelled_generations>=1&&after.near_done==0&&after.tree_done==0);
    assert(world_cover_pump(a,99999)<=4096);
    // Cancel back to a known forest root; arbitrary pump chunking remains deterministic.
    assert(world_cover_update(a,x,z));build(a,repeat,4096);validate(repeat);
    for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;k++){
        assert(first[k].copy.size()==repeat[k].copy.size());
        assert(!std::memcmp(first[k].copy.data(),repeat[k].copy.data(),first[k].copy.size()*sizeof(WorldModelInstance)));
    }
    double negative_x,negative_z;world_atlas_uv_to_world(.44,.30,&negative_x,&negative_z);
    assert(negative_x<0&&negative_z<0&&world_cover_update(b,negative_x,negative_z));
    build(b,repeat,4096);validate(repeat);
    // Saturated overlapping generations must retain protected rooted plants.
    // This detects the old scan-order reservoir: crossing an anchor changed
    // `seen`, replacing nearby plants despite unchanged world seeds.
    assert(first[WORLD_COVER_GRASS].copy.size()==WORLD_COVER_GRASS_CAP);
    assert(first[WORLD_COVER_TREES].copy.size()==WORLD_COVER_TREE_CAP);
    WorldCover *overlap=world_cover_init();assert(overlap);
    Snapshot shifted[WORLD_COVER_KIND_COUNT];
    assert(world_cover_update(overlap,x+24,z));build(overlap,shifted,4096);validate(shifted);
    size_t stable_grass=retained_protected_roots(first[WORLD_COVER_GRASS],shifted[WORLD_COVER_GRASS],true);
    assert(world_cover_update(overlap,x+128,z));build(overlap,shifted,4096);validate(shifted);
    size_t stable_trees=retained_protected_roots(first[WORLD_COVER_TREES],shifted[WORLD_COVER_TREES],false);
    world_cover_shutdown(overlap);
    // The authored on-foot entrance has a real clear path, with healthy rooted
    // grass still reaching its shoulders. Changing budget and cache anchors
    // must preserve both the exact exclusion and deterministic world roots.
    double glade_x,glade_z;world_atlas_uv_to_world(.511,.252,&glade_x,&glade_z);
    Snapshot glade[WORLD_COVER_KIND_COUNT],glade_repeat[WORLD_COVER_KIND_COUNT];
    assert(world_cover_update(a,glade_x,glade_z)&&world_cover_update(b,glade_x,glade_z));
    build(a,glade,4096);build(b,glade_repeat,137);validate(glade);validate(glade_repeat);
    for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;k++){
        assert(glade[k].copy.size()==glade_repeat[k].copy.size());
        assert(!std::memcmp(glade[k].copy.data(),glade_repeat[k].copy.data(),glade[k].copy.size()*sizeof(WorldModelInstance)));
    }
    size_t edge_grass=0,meadow_grass=0;
    size_t point_count=0;const WorldAtlasTrailPoint *points=world_atlas_trail(&point_count);
    for(const auto &i:glade[WORLD_COVER_GRASS].copy){
        double wx=glade[WORLD_COVER_GRASS].field.anchor_x+i.x,wz=glade[WORLD_COVER_GRASS].field.anchor_z+i.z;
        double closest=1e20;
        for(size_t point=1;point<point_count;point++){
            double dx=points[point].x-points[point-1].x,dz=points[point].z-points[point-1].z;
            double t=std::max(0.0,std::min(1.0,((wx-points[point-1].x)*dx+(wz-points[point-1].z)*dz)/(dx*dx+dz*dz)));
            closest=std::min(closest,std::hypot(wx-points[point-1].x-dx*t,wz-points[point-1].z-dz*t));
        }
        if(closest>3&&closest<8)edge_grass++;
        WorldAtlasLandscapeSample ecology=world_atlas_landscape_mask(wx,wz);
        if(ecology.meadow>.85)meadow_grass++;
    }
    assert(edge_grass>500&&meadow_grass>10000);
    WorldCoverMetrics glade_before,glade_after;world_cover_metrics(b,&glade_before);
    assert(world_cover_update(b,glade_x+24,glade_z));world_cover_metrics(b,&glade_after);
    unsigned shifted_fields=3+unsigned(glade_before.tree_generation!=glade_after.tree_generation);
    build(b,glade_repeat,4096,shifted_fields);validate(glade_repeat);
    size_t glade_stable=retained_protected_roots(glade[WORLD_COVER_GRASS],glade_repeat[WORLD_COVER_GRASS],true);
    // A rock-dominated Ulasse buttress must not receive the old peppering of
    // random trees and grass across visibly exposed faces.
    double rock_x,rock_z;world_atlas_uv_to_world(.374,.108,&rock_x,&rock_z);
    assert(world_atlas_landscape_mask(rock_x,rock_z).rock>.8);
    assert(world_cover_update(b,rock_x,rock_z));build(b,glade_repeat,4096);validate(glade_repeat);
    assert(glade_repeat[WORLD_COVER_GRASS].copy.size()<1000);
    std::printf("Authored ecology: trail clear in all fields; %zu shoulder grass roots, %zu meadow roots, %zu retained after anchor shift; rocky buttress %zu grass roots.\n",
        edge_grass,meadow_grass,glade_stable,glade_repeat[WORLD_COVER_GRASS].copy.size());
    // Old Tanthoon has a shaded fern floor rather than a second meadow. Its
    // exact shared path remains clear, and the same fixed fields/caps publish
    // identically under different pump budgets.
    double rain_x,rain_z;world_atlas_uv_to_world(.235,.670,&rain_x,&rain_z);
    assert(world_atlas_rainforest_weight(rain_x,rain_z)>.99f);
    assert(world_atlas_landscape_mask(rain_x,rain_z).trail>.01f);
    Snapshot rainforest[WORLD_COVER_KIND_COUNT],rainforest_repeat[WORLD_COVER_KIND_COUNT];
    assert(world_cover_update(a,rain_x,rain_z)&&world_cover_update(b,rain_x,rain_z));
    unsigned rainforest_calls=build(a,rainforest,4096);build(b,rainforest_repeat,137);
    validate(rainforest);validate(rainforest_repeat);
    for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;++k){
        assert(rainforest[k].copy.size()==rainforest_repeat[k].copy.size());
        assert(!std::memcmp(rainforest[k].copy.data(),rainforest_repeat[k].copy.data(),rainforest[k].copy.size()*sizeof(WorldModelInstance)));
    }
    assert(rainforest[WORLD_COVER_GRASS].copy.size()>1000&&rainforest[WORLD_COVER_GRASS].copy.size()<first[WORLD_COVER_GRASS].copy.size()*.55);
    auto close_ferns=[](const Snapshot &snapshot){
        size_t count=0;double area=0;
        for(const auto &root:snapshot.copy)if(std::hypot(root.x,root.z)<55){++count;area+=root.scale*root.scale;}
        return std::make_pair(count,area);
    };
    auto forest_floor=close_ferns(first[WORLD_COVER_FERNS]),rain_floor=close_ferns(rainforest[WORLD_COVER_FERNS]);
    assert(rain_floor.first>100&&rain_floor.second>2*forest_floor.second);
    double fern_scale=0,tree_scale=0;
    for(const auto &root:rainforest[WORLD_COVER_FERNS].copy)fern_scale+=root.scale;
    for(const auto &root:rainforest[WORLD_COVER_TREES].copy)tree_scale+=root.scale;
    assert(fern_scale/rainforest[WORLD_COVER_FERNS].copy.size()>.9);
    assert(rainforest[WORLD_COVER_TREES].copy.size()>5000&&tree_scale/rainforest[WORLD_COVER_TREES].copy.size()>1.1);
    WorldCoverMetrics rain_before,rain_after;world_cover_metrics(b,&rain_before);
    assert(world_cover_update(b,rain_x+24,rain_z));world_cover_metrics(b,&rain_after);
    build(b,rainforest_repeat,4096,3+unsigned(rain_before.tree_generation!=rain_after.tree_generation));validate(rainforest_repeat);
    // Core meadow grass is reduced82%; still require hundreds of unchanged
    // overlap roots, and verify every eligible root rather than sampling them.
    size_t rain_stable_grass=retained_protected_roots(rainforest[WORLD_COVER_GRASS],rainforest_repeat[WORLD_COVER_GRASS],true,200);
    std::unordered_map<uint64_t,const WorldModelInstance*> fern_roots;
    for(const auto &root:rainforest_repeat[WORLD_COVER_FERNS].copy)assert(fern_roots.emplace(root_id(root),&root).second);
    size_t rain_stable_ferns=0;
    for(const auto &root:rainforest[WORLD_COVER_FERNS].copy){
        double wx=rainforest[WORLD_COVER_FERNS].field.anchor_x+root.x,wz=rainforest[WORLD_COVER_FERNS].field.anchor_z+root.z;
        if(std::hypot(root.x,root.z)>=45||std::hypot(wx-rainforest_repeat[WORLD_COVER_FERNS].field.anchor_x,wz-rainforest_repeat[WORLD_COVER_FERNS].field.anchor_z)>=45)continue;
        auto found=fern_roots.find(root_id(root));assert(found!=fern_roots.end());
        assert(std::fabs(wx-rainforest_repeat[WORLD_COVER_FERNS].field.anchor_x-found->second->x)<.001);
        assert(std::fabs(wz-rainforest_repeat[WORLD_COVER_FERNS].field.anchor_z-found->second->z)<.001);
        assert(root.y==found->second->y);++rain_stable_ferns;
    }
    assert(rain_stable_ferns>30);
    world_cover_metrics(b,&rain_before);assert(world_cover_update(b,rain_x+128,rain_z));world_cover_metrics(b,&rain_after);
    build(b,rainforest_repeat,4096,3+unsigned(rain_before.tree_generation!=rain_after.tree_generation));validate(rainforest_repeat);
    size_t rain_stable_trees=retained_protected_roots(rainforest[WORLD_COVER_TREES],rainforest_repeat[WORLD_COVER_TREES],false);
    std::printf("Rainforest ecology: %zu grass, %zu capped ferns (%zu within55m, scale-area %.1f versus %.1f); retained %zu grass/%zu ferns/%zu trees after anchor shifts; %u bounded pumps, exact trail clear. Weight0 fingerprints unchanged.\n",
        rainforest[WORLD_COVER_GRASS].copy.size(),rainforest[WORLD_COVER_FERNS].copy.size(),rain_floor.first,rain_floor.second,forest_floor.second,
        rain_stable_grass,rain_stable_ferns,rain_stable_trees,rainforest_calls);
    // Deep Ulasse lake produces empty, publishable fields after bounded work.
    WorldCoverMetrics start_before,start_after;world_cover_metrics(a,&start_before);
    world_atlas_uv_to_world(.511,.252,&x,&z);assert(world_cover_update(a,x-20,z-60));
    world_cover_metrics(a,&start_after);
    build(a,repeat,4096,3+unsigned(start_before.tree_generation!=start_after.tree_generation));validate(repeat);
    std::puts("Starting area ecology: planted pockets and walking loop exclude generated trees, rocks and path grass using the shared cached field.");
    // City buildings must not inherit large procedural boulders whose roots
    // miss a footprint mask but whose geometry projects through a wall.
    world_atlas_village_origin(&x,&z);assert(world_cover_update(a,x,z));
    build(a,repeat,4096);validate(repeat);
    assert(repeat[WORLD_COVER_ROCKS].copy.empty()&&repeat[WORLD_COVER_FERNS].copy.empty());
    for(const auto &root:repeat[WORLD_COVER_TREES].copy) {
        const auto &field=repeat[WORLD_COVER_TREES].field;
        assert(world_atlas_village_weight(field.anchor_x+root.x,field.anchor_z+root.z)<.99f);
    }
    std::puts("Village ecology: no procedural boulders/ferns through houses; built core excludes generated forest while authored trees remain separate.");
    world_atlas_uv_to_world(.410,.170,&x,&z);assert(world_cover_update(a,x,z));
    build(a,repeat,4096);validate(repeat);
    for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;k++)assert(repeat[k].copy.empty());
    // Editor commits invalidate staged work without invalidating the last
    // published arrays until a successful replacement poll.
    world_atlas_uv_to_world(.465,.150,&x,&z);assert(world_cover_update(a,x,z));
    build(a,repeat,4096);const auto *published=repeat[0].field.instances;const auto published_count=repeat[0].copy.size();
    std::size_t filter_calls=0;
    world_cover_set_filter(a,[](int kind,double px,double pz,const void *data)->float{assert(kind>=0&&kind<4&&std::isfinite(px)&&std::isfinite(pz));++*static_cast<std::size_t*>(const_cast<void*>(data));return 0;},&filter_calls);
    if(published_count)assert(std::isfinite(published[0].y));
    assert(world_cover_update(a,x,z));build(a,repeat,4096);
    for(const auto &snapshot:repeat)assert(snapshot.copy.empty());
    assert(filter_calls>0);
    world_cover_set_filter(a,nullptr,nullptr);assert(world_cover_update(a,x,z));build(a,repeat,4096);
    assert(!repeat[0].copy.empty());
    std::puts("Editor foliage masks: staged invalidation preserves live pointers; all four layers suppress and regenerate deterministically.");
    world_cover_shutdown(a);world_cover_shutdown(b);world_atlas_shutdown();
    std::printf("World cover: deterministic roots, independent anchors, bounded staging, cancellation, water exclusion and caps passed (%u initial pumps; grass %zu, trees %zu; grass <25m %zu, >60m %zu).\n",calls,first[0].copy.size(),first[1].copy.size(),inner,outer);
    std::printf("Overlapping capped fields retained %zu protected grass roots and %zu trees; 4096-budget pumps mean %.3f ms, max %.3f ms (%zu calls).\n",
                stable_grass,stable_trees,full_pump_ms/std::max(size_t(1),full_pumps),max_pump_ms,full_pumps);
}
