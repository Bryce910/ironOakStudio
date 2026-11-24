#include "engine/world_studio_history_rebuild.h"
#include "engine/world_surface_field.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <limits>

using world_studio::Document;
using world_studio_history::requires_scenery_rebuild;
static unsigned checks=0;
static void check(bool result){assert(result);++checks;}
static void conservative(const Document &before,const Document &after){
    check(requires_scenery_rebuild(before,after));
    check(requires_scenery_rebuild(after,before));
}
int main(){
    Document empty,plants;plants.ecology.push_back({});
    check(!requires_scenery_rebuild(empty,empty));
    check(!requires_scenery_rebuild(empty,plants));
    check(!requires_scenery_rebuild(plants,empty));
    auto next=plants;next.ecology[0].density[1]=.15f;
    check(!requires_scenery_rebuild(plants,next));
    check(requires_scenery_rebuild(plants,next,true));

    // Mixed edits always retain the old scene rebuild, regardless of ecology.
    next=plants;next.terrain.push_back({});next.terrain[0].delta[32*65+32]=1;
    conservative(plants,next);
    next=plants;next.placements.push_back({"fixture"});conservative(plants,next);
    next=plants;next.overrides.push_back({});conservative(plants,next);
    next=plants;next.stamps.push_back({});conservative(plants,next);
    next=plants;next.materials.push_back({"fixture"});conservative(plants,next);
    next=plants;next.waters.push_back({});conservative(plants,next);
    next=plants;next.look.hour=12;conservative(plants,next);
    next=plants;next.lights.push_back({});conservative(plants,next);
    next=plants;next.decals.push_back({});conservative(plants,next);
    next=plants;world_studio::Spline spline;spline.alias="fixture";next.splines.push_back(spline);conservative(plants,next);
    next=plants;next.assets.push_back({"fixture","imports/fixture/asset"});conservative(plants,next);
    next=plants;next.navigation.push_back({});conservative(plants,next);
    next=plants;next.particles.push_back({});conservative(plants,next);
    next=plants;world_studio::Actor actor;actor.points.push_back({0,0});next.actors.push_back(actor);conservative(plants,next);
    next=plants;next.source_signature="0123456789abcdef";conservative(plants,next);

    // Metadata, shader/surface data and dormant values must not fall through a
    // hand-maintained geometry-only whitelist as their authoring grows.
    next=plants;world_entity::Id id=0;std::string error;
    check(world_entity::add(next.entities,0,"Root",{}, {},id,error));conservative(plants,next);
    auto grouped=next;grouped.groups.push_back({"fixture",{0}});conservative(plants,grouped);
    next=plants;next.surfaces.winds.push_back({});conservative(plants,next);
    next=plants;next.surfaces.layers.push_back({});conservative(plants,next);
    next=plants;next.environment.rotation=15;conservative(plants,next);
    next=plants;next.environment.intensity=2;conservative(plants,next);
    next=plants;next.navigation.push_back({});auto dormant=next;dormant.navigation[0].floor_y=123;
    conservative(next,dormant);
    next=plants;next.surface_field=std::make_shared<world_surface::TerrainField>();conservative(plants,next);
    check(!requires_scenery_rebuild(next,next));

    // New owned structure geometry needs collision/render rebuilding; an ID-only
    // high-water difference must retain the ecology optimization after Undo.
    auto structural=plants;world_structure::Definition wall;wall.id=1;wall.points={{0,0,0},{8,0,0}};
    structural.structures.push_back(wall);structural.next_structure_id=2;conservative(plants,structural);
    auto issued=plants;issued.next_structure_id=42;check(!requires_scenery_rebuild(plants,issued));check(!requires_scenery_rebuild(issued,plants));

    // Invalid/unknown document state never authorizes the optimized path.
    next=plants;next.ecology[0].radius=std::numeric_limits<float>::quiet_NaN();conservative(plants,next);
    next=plants;next.look.sun=std::numeric_limits<float>::quiet_NaN();conservative(plants,next);

    // Inspecting a real history destination does not mutate either branch.
    // Restoring ecology preserves the monotonically issued entity IDs.
    world_studio::History history;Document live=empty;history.checkpoint(live);
    live=plants;live.entities.next_id=91;
    check(!requires_scenery_rebuild(live,history.back.back()));
    check(history.back.size()==1&&history.forward.empty()&&live.entities.next_id==91);
    check(history.undo(live));check(live.entities.next_id==91&&live.ecology.empty());
    check(!requires_scenery_rebuild(live,history.forward.back()));
    check(history.redo(live));check(live.entities.next_id==91&&live.ecology.size()==1);
    check(requires_scenery_rebuild(live,history.back.back(),true));
    world_studio::History no_history;const auto before=live.entities.next_id;
    check(!no_history.undo(live)&&!no_history.redo(live)&&live.entities.next_id==before);

    // Exercise retention, alternating restoration and a new history branch.
    history={};live=empty;
    for(unsigned i=0;i<96;++i){history.checkpoint(live);live=plants;live.ecology[0].seed=i+1;check(history.back.size()<=32);}
    for(unsigned cycle=0;cycle<128;++cycle){
        const auto count=history.back.size()+history.forward.size();
        check(!requires_scenery_rebuild(live,history.back.back()));
        check(history.undo(live));check(history.back.size()+history.forward.size()==count);
        check(!requires_scenery_rebuild(live,history.forward.back()));
        check(history.redo(live));check(history.back.size()+history.forward.size()==count);
    }
    check(history.undo(live));history.checkpoint(live);
    check(history.forward.empty()&&history.back.size()<=32);

    // Maximum editable terrain stays eligible when it is byte-identical.
    // Report explicit command cost; this never runs per editor frame.
    Document full=plants;
    for(unsigned i=0;i<world_studio::tile_limit;++i){world_studio::TerrainTile tile;tile.x=int(i*2);tile.delta[32*65+32]=float(i)*.125f;full.terrain.push_back(tile);}
    auto full_next=full;full_next.ecology[0].seed++;
    const auto started=std::chrono::steady_clock::now();
    for(unsigned i=0;i<8;++i)check(!requires_scenery_rebuild(full,full_next));
    const double maximum_tiles_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()/8;
    full_next.terrain.back().delta[33*65+33]=.125f;conservative(full,full_next);
    std::printf("world_studio_history_rebuild: %u checks passed; 64-tile classification mean %.3f ms\n",checks,maximum_tiles_ms);
}
