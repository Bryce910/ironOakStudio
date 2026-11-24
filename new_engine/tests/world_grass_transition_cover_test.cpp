/* Integration regression: real atlas ecology, real 24m cover publications,
 * and the renderer's transition registry. No GL, synthetic roots, protected
 * radius exemption, or directional subset can hide disappearing grass here. */
#include "engine/world_atlas.h"
#include "engine/world_cover.h"
#include "engine/world_grass_transition.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace {
struct Snapshot {
    WorldCoverField metadata{};
    std::vector<WorldModelInstance> roots;
    WorldCoverField field() const {
        WorldCoverField result=metadata;
        result.instances=roots.data();result.count=roots.size();
        return result;
    }
};

Snapshot publish(WorldCover *cover,double camera_x,double camera_z) {
    assert(world_cover_update(cover,camera_x,camera_z));
    Snapshot result;
    unsigned grass_publications=0,pumps=0;
    for(;;) {
        assert(world_cover_pump(cover,4096)<=4096);
        WorldCoverField field;
        while(world_cover_poll(cover,&field)) {
            if(field.kind!=WORLD_COVER_GRASS)continue;
            result.metadata=field;
            result.roots.assign(field.instances,field.instances+field.count);
            ++grass_publications;
        }
        WorldCoverMetrics metrics;
        world_cover_metrics(cover,&metrics);
        if(!metrics.near_building&&!metrics.tree_building)break;
        assert(++pumps<1000);
    }
    assert(grass_publications==1);
    assert(result.roots.size()>100&&result.roots.size()<=WORLD_COVER_GRASS_CAP);
    return result;
}

// Seeded yaw and scale are independent of the publication anchor or ordering.
// Coordinates are checked separately in absolute world space below.
std::uint64_t identity(const WorldModelInstance &root) {
    std::uint32_t yaw,scale;
    std::memcpy(&yaw,&root.yaw,sizeof(yaw));std::memcpy(&scale,&root.scale,sizeof(scale));
    return (std::uint64_t(yaw)<<32)|scale;
}
using RootMap=std::unordered_map<std::uint64_t,size_t>;
RootMap index(const WorldCoverField &field) {
    RootMap roots;
    roots.reserve(field.count);
    for(size_t i=0;i<field.count;++i)assert(roots.emplace(identity(field.instances[i]),i).second);
    return roots;
}

void same_world_root(const WorldCoverField &actual,size_t ai,const WorldCoverField &expected,size_t ei) {
    const auto &a=actual.instances[ai],&e=expected.instances[ei];
    // Relative uploads are floats but anchors are doubles. A 24m rebase may
    // round one relative coordinate by a few micrometres, never move a blade.
    assert(std::abs(actual.anchor_x+double(a.x)-expected.anchor_x-double(e.x))<.00003);
    assert(std::abs(actual.anchor_z+double(a.z)-expected.anchor_z-double(e.z))<.00003);
    assert(a.y==e.y&&a.yaw==e.yaw&&a.scale==e.scale);
}

void check_region(const char *name,double x,double z) {
    WorldCover *cover=world_cover_init();assert(cover);
    const Snapshot before=publish(cover,x,z);
    const Snapshot after=publish(cover,x+24,z);
    // Both copies must outlive the cover cache's source storage and later
    // swaps; the fixture never accidentally compares a cache buffer to itself.
    world_cover_shutdown(cover);
    const WorldCoverField old=before.field(),desired=after.field();
    assert(desired.anchor_x-old.anchor_x==24&&desired.anchor_z==old.anchor_z);
    assert(desired.generation==old.generation+1);
    assert(std::hypot(desired.anchor_x-old.anchor_x,desired.anchor_z-old.anchor_z)<160);
    const RootMap old_ids=index(old),desired_ids=index(desired);
    size_t common=0,missing=0,added=0;
    for(const auto &root:before.roots) {
        // Every former root lies within the ordinary overlap cache. No near
        // radius, view cone, bisector, or protected-tier filter is permitted.
        assert(std::hypot(old.anchor_x+root.x-desired.anchor_x,
                          old.anchor_z+root.z-desired.anchor_z)<160);
        if(desired_ids.count(identity(root)))++common;else ++missing;
    }
    for(const auto &root:after.roots)if(!old_ids.count(identity(root)))++added;
    assert(common>0&&missing>0&&added>0);
    assert(common+missing==old.count&&common+added==desired.count);
    const size_t union_count=old.count+added;
    assert(union_count<=2*WORLD_COVER_GRASS_CAP);

    WorldGrassTransition registry;
    assert(registry.replace(old));
    assert(registry.field().count==old.count);
    for(float fade:registry.fades())assert(fade==1);
    assert(registry.replace(desired));
    assert(registry.field().count==union_count);
    const auto immediate=index(registry.field());
    assert(immediate.size()==union_count&&registry.fades().size()==union_count);
    double initial_coverage=0;
    for(const auto &entry:old_ids) {
        const auto found=immediate.find(entry.first);assert(found!=immediate.end());
        assert(registry.fades()[found->second]==1);
        same_world_root(registry.field(),found->second,old,entry.second);
    }
    for(const auto &entry:desired_ids) {
        const auto found=immediate.find(entry.first);assert(found!=immediate.end());
        same_world_root(registry.field(),found->second,desired,entry.second);
        assert(registry.fades()[found->second]==(old_ids.count(entry.first)?1.f:0.f));
    }
    for(float fade:registry.fades())initial_coverage+=fade;
    assert(initial_coverage==double(old.count));

    assert(registry.advance(.4));
    assert(registry.field().count==union_count);
    const auto halfway=index(registry.field());
    double half_coverage=0;
    for(const auto &entry:halfway) {
        const bool was_present=old_ids.count(entry.first),is_desired=desired_ids.count(entry.first);
        assert(was_present||is_desired);
        const float expected=was_present&&is_desired?1.f:.5f;
        assert(std::abs(registry.fades()[entry.second]-expected)<.000001f);
        if(was_present)same_world_root(registry.field(),entry.second,old,old_ids.at(entry.first));
        else same_world_root(registry.field(),entry.second,desired,desired_ids.at(entry.first));
        half_coverage+=registry.fades()[entry.second];
    }
    assert(std::abs(half_coverage-(double(old.count)+double(desired.count))*.5)<.000001);

    assert(registry.advance(.4)); // 0.8s total: outgoing roots are compacted.
    assert(registry.field().count==desired.count&&registry.fades().size()==desired.count);
    const auto complete=index(registry.field());
    assert(complete.size()==desired_ids.size());
    for(const auto &entry:desired_ids) {
        const auto found=complete.find(entry.first);assert(found!=complete.end());
        assert(registry.fades()[found->second]==1);
        same_world_root(registry.field(),found->second,desired,entry.second);
    }
    assert(!registry.advance(.4));
    std::printf("Grass real-cover %-15s old %zu desired %zu common %zu missing %zu (%.2f%%) added %zu; all %zu prior roots retained at1, new at0; half mean %.6f; complete %zu exact world roots PASS\n",
        name,old.count,desired.count,common,missing,100.*missing/old.count,added,old.count,
        half_coverage/union_count,desired.count);
}
}

int main() {
    assert(world_atlas_init());
    double x,z;
    world_atlas_uv_to_world(.52,.24,&x,&z);check_region("forest",x,z);
    world_atlas_uv_to_world(.511,.252,&x,&z);check_region("glade",x,z);
    world_atlas_uv_to_world(.235,.670,&x,&z);check_region("rainforest",x,z);
    world_atlas_village_origin(&x,&z);check_region("village-outskirt",x+250,z+100);
    world_atlas_shutdown();
    std::puts("world_grass_transition_cover_test: all roots checked without directional/protected exemptions PASS");
}
