// Focused CPU proof for live quality changes, nested roots and stable blockers.
#include "engine/world_cover.h"
#include "engine/world_atlas.h"
#include "engine/world_ground_cover_quality.h"
#include "engine/world_grass_transition.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace {
struct Snapshot {WorldCoverField field{};std::vector<WorldModelInstance> roots;};
using Fields=std::array<Snapshot,WORLD_COVER_KIND_COUNT>;
using Key=std::array<uint32_t,sizeof(WorldModelInstance)/sizeof(uint32_t)>;
Key key(const WorldModelInstance &root){Key k;std::memcpy(k.data(),&root,sizeof root);return k;}
bool equal(const Snapshot &a,const Snapshot &b){return a.field.anchor_x==b.field.anchor_x&&
    a.field.anchor_z==b.field.anchor_z&&a.roots.size()==b.roots.size()&&
    (a.roots.empty()||!std::memcmp(a.roots.data(),b.roots.data(),a.roots.size()*sizeof(WorldModelInstance)));}
void subset(const Snapshot &a,const Snapshot &b){
    std::set<Key> roots;for(const auto &r:b.roots)assert(roots.insert(key(r)).second);
    for(const auto &r:a.roots)assert(roots.count(key(r))==1);
}
unsigned finish(WorldCover *cover,Fields &out,size_t budget=4096){
    unsigned fields=0,calls=0;
    for(;;){
        const auto done=world_cover_pump(cover,budget);assert(done<=budget&&done<=4096);
        WorldCoverField field;while(world_cover_poll(cover,&field)){
            out[field.kind].field=field;
            out[field.kind].roots.assign(field.instances,field.instances+field.count);++fields;
        }
        WorldCoverMetrics m;world_cover_metrics(cover,&m);
        assert(m.near_done<=m.near_total&&m.tree_done<=m.tree_total);
        if(!m.near_building&&!m.tree_building)break;
        assert(++calls<100000);
    }
    return fields;
}
void validate(const Fields &s,int tier){
    const auto p=world_ground_cover_quality::profile(tier);
    const size_t caps[]={p.grass,p.trees,p.rocks,p.ferns};
    for(unsigned kind=0;kind<4;++kind){
        assert(s[kind].roots.size()<=caps[kind]);
        for(const auto &r:s[kind].roots){
            const double x=s[kind].field.anchor_x+r.x,z=s[kind].field.anchor_z+r.z;
            assert(std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.z)&&r.scale>0);
            assert(!world_atlas_local_water(x,z,nullptr,nullptr));
            assert(std::abs(world_atlas_height(x,z)-r.y)<.02);
        }
    }
}
void at(double u,double v,bool authored){
    double x,z;world_atlas_uv_to_world(u,v,&x,&z);
    WorldCover *cover=world_cover_init();assert(cover);
    if(authored){
        WorldCoverRegion r;r.x=x;r.z=z;r.radius=160;r.seed=125;r.max_slope=85;
        for(float &d:r.density)d=1;
        assert(world_cover_set_regions(cover,&r,1,true,nullptr,nullptr));
    }
    std::array<Fields,4> tiers;
    WorldGrassTransition transition;
    for(int tier=0;tier<4;++tier){
        assert(world_cover_set_quality(cover,tier));assert(world_cover_update(cover,x,z));
        assert(finish(cover,tiers[tier],tier%2?137:4096)==(tier?3u:4u));
        // Trees are intentionally not rebuilt when only density is changed.
        if(tier)tiers[tier][WORLD_COVER_TREES]=tiers[0][WORLD_COVER_TREES];
        validate(tiers[tier],tier);
        assert(transition.replace(tiers[tier][0].field));transition.advance(1);
        assert(transition.field().count==tiers[tier][0].roots.size());
        if(tier){
            subset(tiers[tier-1][0],tiers[tier][0]);subset(tiers[tier-1][3],tiers[tier][3]);
            assert(equal(tiers[0][1],tiers[tier][1])&&equal(tiers[0][2],tiers[tier][2]));
        }
    }
    // A naturally sparse shaded biome can remain below even the Low grass
    // budget; quality must preserve that ecological exclusion rather than
    // force a full meadow crop. Dense forest/authored fields do fill more.
    assert(tiers[3][0].roots.size()>=tiers[0][0].roots.size());
    if(u==.520)assert(tiers[3][0].roots.size()>tiers[0][0].roots.size());
    assert(tiers[3][3].roots.size()>tiers[0][3].roots.size()*2);
    WorldCoverMetrics before,after;world_cover_metrics(cover,&before);
    assert(world_cover_set_quality(cover,3));world_cover_metrics(cover,&after);
    assert(!after.near_building&&before.cancelled_generations==after.cancelled_generations);
    assert(!world_cover_set_quality(cover,-1)&&!world_cover_set_quality(cover,4)&&!world_cover_set_quality(nullptr,0));
    world_cover_metrics(cover,&after);assert(before.near_generation==after.near_generation);
    const auto *published=tiers[3][0].field.instances;
    const auto saved=tiers[3][0].roots;
    assert(world_cover_set_quality(cover,0));
    assert(!std::memcmp(published,saved.data(),saved.size()*sizeof(WorldModelInstance)));
    assert(world_cover_update(cover,x,z));assert(world_cover_pump(cover,17)==17);
    assert(!std::memcmp(published,saved.data(),saved.size()*sizeof(WorldModelInstance)));
    // A second Apply during staging cancels bounded private work and still
    // preserves the exact previously published field until replacement poll.
    assert(world_cover_set_quality(cover,2));world_cover_metrics(cover,&after);
    assert(after.cancelled_generations==before.cancelled_generations+1);
    assert(world_cover_update(cover,x,z));Fields restored;
    assert(finish(cover,restored,307)==3);assert(equal(restored[0],tiers[2][0])&&equal(restored[3],tiers[2][3]));
    world_cover_metrics(cover,&after);assert(after.tree_generation==before.tree_generation);
    std::printf("%s UV %.3f %.3f: grass %zu/%zu/%zu/%zu; ferns %zu/%zu/%zu/%zu; stable blockers %zu trees/%zu rocks\n",
        authored?"Authored":"Atlas",u,v,
        tiers[0][0].roots.size(),tiers[1][0].roots.size(),tiers[2][0].roots.size(),tiers[3][0].roots.size(),
        tiers[0][3].roots.size(),tiers[1][3].roots.size(),tiers[2][3].roots.size(),tiers[3][3].roots.size(),
        tiers[0][1].roots.size(),tiers[0][2].roots.size());
    world_cover_shutdown(cover);
}
void transition_capacity(){
    WorldGrassTransition bridge;
    std::vector<WorldModelInstance> a,b;
    for(unsigned i=0;i<WORLD_COVER_GRASS_MAX_CAP;++i){
        a.push_back({float(i%240),10,float(i/240),float(i+1)*.03125f,1});
        b.push_back({float(i%240)+1,10,float(i/240),float(i+100001)*.03125f,1});
    }
    WorldCoverField f={WORLD_COVER_GRASS,1,0,0,a.size(),a.data()};assert(bridge.replace(f));
    f={WORLD_COVER_GRASS,2,24,0,b.size(),b.data()};assert(bridge.replace(f));
    assert(bridge.field().count==2*WORLD_COVER_GRASS_MAX_CAP);
    assert(bridge.advance(1)&&bridge.field().count==WORLD_COVER_GRASS_MAX_CAP);
    const auto before=bridge.structure_revision();f.count=WORLD_COVER_GRASS_MAX_CAP+1;
    assert(!bridge.replace(f)&&before==bridge.structure_revision());
}
}
int main(){
    const auto low=world_ground_cover_quality::profile(WorldFoliageDensity::Low);
    const auto ultra=world_ground_cover_quality::profile(WorldFoliageDensity::Ultra);
    assert(low.grass<ultra.grass&&low.ferns<ultra.ferns&&low.trees==ultra.trees&&low.rocks==ultra.rocks);
    assert(ultra.grass==WORLD_COVER_GRASS_MAX_CAP&&ultra.ferns==WORLD_COVER_FERN_MAX_CAP);
    assert(world_atlas_init());
    at(.520,.240,false);at(.235,.670,false);at(.520,.240,true);
    transition_capacity();world_atlas_shutdown();
    std::puts("PASS: all quality roots nested, real density increases, bounded live Apply/cancellation, unchanged physical blockers, Ultra grass transitions.");
}
