#include "engine/world_authoring_identity.h"
#include "engine/world_prefab.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace world_authoring;
using namespace world_entity;
static double support(double x,double,const void *){return x*.01;}
int main(){
    world_studio::Document d;Graph g;std::string e;
    std::vector<Source> sources={{"tavern","village.txt",10,20,3,90,1},{"tree","trees.txt",30,40,4,0,1}};
    d.source_signature="1111111111111111";d.overrides.push_back({0,12,22,100,1,false,2});
    d.groups.push_back({"Tavern",{0}});d.placements.push_back({"barrel",12,24});
    assert(reconcile(d,g,sources,d.source_signature,support,nullptr,e));
    assert(g.nodes.size()==2);const Id tavern=g.nodes[0].id,barrel=g.nodes[1].id;const auto initial=g.next_id;
    assert(reparent(g,barrel,tavern,true,e));Transform before;assert(world_transform(g,barrel,before,e));
    std::swap(sources[0],sources[1]);assert(reconcile(d,g,sources,"2222222222222222",support,nullptr,e));
    assert(d.overrides[0].index==1&&d.groups[0].members[0]==1);
    assert(find(g,tavern)->binding.index==1&&find(g,barrel)->parent==tavern&&g.next_id==initial);
    Transform after;assert(world_transform(g,barrel,after,e)&&std::abs(before.x-after.x)<1e-8);
    const auto saved=d;const auto graph=g;sources[1].x++;
    assert(!reconcile(d,g,sources,"3333333333333333",support,nullptr,e));
    assert(d.source_signature==saved.source_signature&&g.nodes[0].source_key==graph.nodes[0].source_key);
    sources[1].x--;sources.push_back(sources[1]);assert(!reconcile(d,g,sources,"3333333333333333",support,nullptr,e));sources.pop_back();
    // A legacy save has no prior canonical keys and cannot silently rebind.
    Graph legacy;assert(!reconcile(d,legacy,sources,"3333333333333333",support,nullptr,e));
    // One numeric parent edit does not drag an unedited child.
    d.overrides[0].x+=3;assert(synchronize(d,g,sources,support,nullptr,e));
    assert(world_transform(g,barrel,after,e)&&std::abs(after.x-before.x)<1e-8);
    // Compaction preserves a later object's durable ID.
    d.placements.push_back({"crate",18,25});assert(synchronize(d,g,sources,support,nullptr,e));
    const Id crate=g.nodes.back().id;assert(erased(g,{Kind::Placement,0},e));d.placements.erase(d.placements.begin());
    assert(synchronize(d,g,sources,support,nullptr,e));assert(find(g,crate)->binding.index==0);
    assert(!find(g,barrel));assert(validate(g,&d,e));
    // Registering after a rotated bundle preserves the original nav square,
    // rather than treating its wider axis-aligned envelope as a new square.
    world_studio::Document nav;world_studio::NavigationArea area;area.name="Training";area.span=16;nav.navigation.push_back(area);
    world_prefab::Selection chosen;chosen.objects.push_back({Kind::Navigation,0});world_prefab::Prefab bundle;
    assert(world_prefab::capture(nav,chosen,{},"Training",bundle,e));world_studio::Document instance;Graph entities;world_prefab::Instance created;
    assert(world_prefab::instantiate(bundle,instance,entities,{0,0,0,45,1},0,created,e));
    assert(synchronize(instance,entities,{},support,nullptr,e));
    assert(world_prefab::transform_instance(instance,entities,created.root,{},e));
    assert(std::abs(instance.navigation[0].span-16)<.0001);
    // Deleting array element zero must retain element one's durable identity
    // for every authored attachment kind, including actor navigation links.
    for(auto kind:{Kind::Placement,Kind::Light,Kind::Actor,Kind::Particle,Kind::Water,Kind::Navigation}){
        world_studio::Document records;
        for(unsigned i=0;i<2;i++){
            records.placements.push_back({"crate",double(i),0});
            records.lights.push_back({});
            world_studio::Actor actor;actor.points.push_back({double(i),0});actor.navigation=int(i);records.actors.push_back(actor);
            records.particles.push_back({});
            world_studio::Water water;water.source=3;water.x=i*20;records.waters.push_back(water);
            world_studio::NavigationArea area;area.name="Area "+std::to_string(i);records.navigation.push_back(area);
        }
        assert(synchronize(records,records.entities,{},support,nullptr,e));
        Id retained=0;for(const auto &node:records.entities.nodes)if(node.binding==Binding{kind,1})retained=node.id;
        assert(retained);const auto high=records.entities.next_id;
        assert(erase_record(records,{kind,0},e));
        assert(find(records.entities,retained)&&find(records.entities,retained)->binding.index==0);
        assert(records.entities.next_id==high&&world_studio::valid(records,e));
        if(kind==Kind::Navigation)assert(records.actors[0].navigation==-1&&records.actors[1].navigation==0);
        auto *locked=find(records.entities,retained);locked->locked=true;
        assert(!erase_record(records,{kind,0},e));
        assert(find(records.entities,retained)&&world_studio::valid(records,e));
    }
    std::cout<<"world authoring source identity: passed\n";
}
