#include "engine/world_cover.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

struct Snapshot { WorldCoverField field{}; std::vector<WorldModelInstance> roots; };
static void build(WorldCover *cover,Snapshot *out,unsigned expected) {
    unsigned published=0;
    for(unsigned calls=0;;++calls) {
        assert(calls<100000);
        assert(world_cover_pump(cover,4096)<=4096);
        WorldCoverField f{};
        while(world_cover_poll(cover,&f)) {
            out[f.kind].field=f;
            out[f.kind].roots.assign(f.instances,f.instances+f.count);
            ++published;
        }
        WorldCoverMetrics m{};world_cover_metrics(cover,&m);
        assert(m.near_done<=m.near_total&&m.tree_done<=m.tree_total);
        if(!m.near_building&&!m.tree_building)break;
    }
    assert(published==expected);
}
static unsigned long long fingerprint(const Snapshot &s) {
    unsigned long long h=1469598103934665603ull;
    for(const auto &r:s.roots)for(float f:{r.x,r.y,r.z,r.scale,r.yaw}) {
        unsigned bits=0;std::memcpy(&bits,&f,sizeof(bits));
        for(unsigned i=0;i<4;++i){h^=(bits>>(8*i))&255;h*=1099511628211ull;}
    }
    return h;
}
int main() {
    assert(world_atlas_init());
    double x,z;world_atlas_uv_to_world(.52,.24,&x,&z);
    WorldCover *cover=world_cover_init();assert(cover);
    assert(world_cover_update(cover,x,z));Snapshot original[WORLD_COVER_KIND_COUNT];build(cover,original,4);
    for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;++k)
        std::printf("DEFAULT %u %zu %016llx\n",k,original[k].roots.size(),fingerprint(original[k]));
#ifndef VALWOOD_PARENT_COVER_CONTROL
    const auto physical=original[WORLD_COVER_ROCKS].roots;
    assert(!physical.empty());
    const auto tree_generation=original[WORLD_COVER_TREES].field.generation;
    for(float scale:{2.f,.65f,1.5f,1.f}) {
        assert(world_cover_set_detail_scale(cover,scale));
        assert(world_cover_update(cover,x,z));Snapshot resized[WORLD_COVER_KIND_COUNT];build(cover,resized,3);
        assert(resized[WORLD_COVER_GRASS].roots.size()<=WORLD_COVER_GRASS_CAP);
        assert(resized[WORLD_COVER_FERNS].roots.size()<=WORLD_COVER_FERN_CAP);
        double furthest=0;
        for(const auto &r:resized[WORLD_COVER_GRASS].roots) {
            const double d=std::hypot(r.x,r.z);assert(d<=90.*scale+21.01);furthest=std::max(furthest,d);
        }
        if(scale==2.f)assert(furthest>120.);
        assert(resized[WORLD_COVER_ROCKS].roots.size()==physical.size());
        for(size_t i=0;i<physical.size();++i) {
            const auto &a=physical[i],&b=resized[WORLD_COVER_ROCKS].roots[i];
            assert(a.x==b.x&&a.y==b.y&&a.z==b.z&&a.scale==b.scale&&a.yaw==b.yaw);
        }
        WorldCoverMetrics m{};world_cover_metrics(cover,&m);assert(m.tree_generation==tree_generation);
        assert(!world_cover_set_detail_scale(cover,NAN));
        assert(!world_cover_set_detail_scale(cover,.6f)&&!world_cover_set_detail_scale(cover,2.1f));
        assert(world_cover_set_detail_scale(cover,scale)&&world_cover_pump(cover,4096)==0);
        std::printf("DISTANCE %.2f grass %zu max %.2f; physical roots unchanged\n",scale,resized[0].roots.size(),furthest);
    }
    std::puts("PASS foliage generation: four levels, bounded work/caps, real range extension, unchanged collisions, no redundant rebuild");
#endif
    world_cover_shutdown(cover);world_atlas_shutdown();
}
