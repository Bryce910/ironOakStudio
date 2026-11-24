#include "engine/world_water_reflection_demand.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks=0;
static void check(bool value,const char *message) {
    ++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int main() {
    world_studio::Document document;
    check(world_water_source_style(document,0)==nullptr,"Missing override keeps legacy ocean");
    check(world_water_style_enabled(document,nullptr),"Missing style stays visible");
    world_studio::Water pool;pool.source=1;pool.enabled=false;document.waters.push_back(pool);
    check(world_water_source_style(document,0)==nullptr,"A disabled pool cannot disable the ocean");
    world_studio::Water ocean;ocean.source=0;ocean.enabled=false;document.waters.push_back(ocean);
    check(!world_water_style_enabled(document,world_water_source_style(document,0)),"Explicit inland override disables ocean consumer");
    ocean.enabled=true;document.waters.push_back(ocean);
    check(world_water_source_style(document,0)==&document.waters[1],"First matching override preserves draw precedence even on an unvalidated duplicate");
    check(!world_water_style_enabled(document,world_water_source_style(document,0)),"Later enabled duplicate cannot re-enable first disabled style");
    document.waters.resize(2);document.waters[1].enabled=true;
    world_entity::Node parent;parent.id=1;parent.visible=false;
    world_entity::Node child;child.id=2;child.parent=1;child.binding={world_entity::Kind::Water,1};
    document.entities.nodes={parent,child};
    check(!world_water_style_enabled(document,world_water_source_style(document,0)),"Hidden parent disables the same ocean draw and demand");
    document.entities.nodes[0].visible=true;
    check(world_water_style_enabled(document,world_water_source_style(document,0)),"Showing parent restores ocean immediately");
    document.entities.nodes[1].visible=false;
    check(!world_water_style_enabled(document,world_water_source_style(document,0)),"Hidden ocean node disables consumer");
    check(world_water_source_style(document,3,&document.waters[0])==&document.waters[0],"Authored custom style retains explicit owner");

    for(unsigned mask=0;mask<32;++mask) {
        const bool graphics=mask&1,pool_visible=mask&2,creek_visible=mask&4,pond_visible=mask&8,shore_candidate=mask&16;
        const bool local=pool_visible||creek_visible||pond_visible;
        const bool legacy=graphics&&(local||shore_candidate);
        auto normal=world_water_reflection_demand(graphics,local,shore_candidate,true);
        check(normal.render==legacy&&!normal.skipped_unused_ocean,"Default/coastal policy exactly preserves every legacy combination");
        auto inland=world_water_reflection_demand(graphics,local,shore_candidate,false);
        check(!graphics||!local||inland.render,"Every individual/mixed local mirror consumer survives inland override");
        check(graphics||!inland.render,"Graphics reflections off cannot allocate a mirror");
        if(graphics&&!local&&shore_candidate)
            check(!inland.render&&inland.skipped_unused_ocean,"Only sole unused disabled-ocean demand is removed");
        else check(inland.render==legacy&&!inland.skipped_unused_ocean,"All other inland combinations retain their old result");
        auto forced=world_water_reflection_demand(graphics,local,shore_candidate,false,true);
        check(forced.render==legacy&&!forced.skipped_unused_ocean,"QA force restores only prior reflection work");
    }
    std::printf("{\"passed\":true,\"checks\":%u,\"gpu_used\":false}\n",checks);
}
