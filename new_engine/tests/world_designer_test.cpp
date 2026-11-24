#include "engine/world_designer.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <unistd.h>
using namespace world_designer;
using EntityKind=world_entity::Kind;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
static double flat(double,double,const void *){return 10;}
static double slope(double x,double,const void *){return 10+x*.2;}
static bool near(double a,double b){return std::abs(a-b)<1e-6;}
static std::string bytes(const world_studio::Document &d){
    char path[]="/tmp/valwood-designer-bytes-XXXXXX";const int fd=mkstemp(path);if(fd<0)return {};close(fd);std::string error;
    if(!world_studio::save(path,d,error)){unlink(path);return "invalid: "+error;}
    std::ifstream in(path,std::ios::binary);std::string result{std::istreambuf_iterator<char>(in),{}};unlink(path);return result;
}
static Id id_for(const world_studio::Document &d,EntityKind kind,unsigned index){const auto r=from_binding(d,{kind,index});return r.kind==Kind::Entity?r.id:0;}
static bool reject_candidate(const world_studio::Document &,const void *,std::string &error){error="Host rejected candidate";return false;}
// Compile and exercise the real adapter without any native renderer or window.
struct Studio {
    world_studio::Document draft;world_studio::History history;
    std::vector<world_studio::Pose> source_poses;std::vector<world_authoring::Source> identity_sources;
    std::string signature="0123456789abcdef",message;bool components_ready=false,dirty=false,rebuild=false,reject=false;
    struct Reference {Id entity=0;};struct {std::vector<Reference> physics,living;}components_active_document;
    struct {struct Entity {Id id=0;};std::vector<Entity> entities;}gameplay_document;
    std::vector<unsigned> selection_keys;Id entity_selected=0,production_id_highwater=1;std::uint64_t entity_world_revision=0,revision=0;
    int selected=-1,selected_added=-1,light=-1,actor_selection=-1,area_selection=-1,particle_selection=-1,water_selection=-1,tab=2;
    static double production_ground(double,double,const void *){return 10;}
    bool production_freeze()const{return false;}
    bool components_scene_prevalidate(const world_studio::Document &,std::string &error)const{if(reject){error="Host rejected candidate";return false;}return true;}
    void changed(bool geometry=false){dirty=true;rebuild|=geometry;revision++;}
    unsigned previews=0;
    void designer_preview_object_edit(){++previews;}
    #include "engine/world_studio_designer_selection.inc"
};
int main(){
    std::string error;world_studio::Document d,next;Result result;Selection selection;
    std::vector<world_studio::Pose> poses(7777);std::vector<world_authoring::Source> identities(7777);
    for(unsigned i=0;i<poses.size();i++){poses[i].alias="starting_crate";poses[i].root={double(i)*3,10,0};poses[i].minimum={-.5,0,-.5};poses[i].maximum={.5,1,.5};identities[i]={"starting_crate","village.txt",double(i)*3,0,10,0,1};}
    Context context;context.source_poses=&poses;context.source_identities=&identities;context.ground={flat,nullptr};context.source_signature="0123456789abcdef";
    CHECK(Ref::entity(0).empty());CHECK(!Ref::source_model(0).empty());CHECK(select(d,context,selection,Ref::source_model(0),false,error));
    CHECK(d.entities.nodes.empty()&&selection.active.kind==Kind::Source&&selection.active.source==0&&!selection.active.source_key.empty());
    auto legacy=project(d,context,selection);CHECK(legacy.selected==0&&legacy.entity==0&&legacy.keys==std::vector<unsigned>{0});
    auto swapped_poses=poses;auto swapped_ids=identities;std::swap(swapped_poses[0],swapped_poses[1]);std::swap(swapped_ids[0],swapped_ids[1]);Context reordered=context;reordered.source_poses=&swapped_poses;reordered.source_identities=&swapped_ids;
    auto reordered_selection=selection;reconcile(d,reordered,reordered_selection);CHECK(reordered_selection.active.source==1);swapped_ids[2]=swapped_ids[1];reconcile(d,reordered,reordered_selection);CHECK(reordered_selection.active.empty());
    // An indexed source catalogue owns its immutable identity snapshot. A
    // foreign or stale caller-provided key vector cannot trick the fast path.
    SourceCatalog catalogue(identities);Context indexed=context;indexed.source_catalog=&catalogue;indexed.source_identities=&catalogue.identities();indexed.source_keys=&catalogue.keys();
    auto indexed_selection=selection;reconcile(d,indexed,indexed_selection);CHECK(indexed_selection.active.source==0);CHECK(project(d,indexed,indexed_selection).selected==0);
    unsigned indexed_source=999;CHECK(catalogue.unique_index(selection.active.source_key,indexed_source)&&indexed_source==0);
    auto source_reorder=identities;std::swap(source_reorder[0],source_reorder[1]);SourceCatalog reordered_catalogue(source_reorder);indexed.source_catalog=&reordered_catalogue;indexed.source_identities=&reordered_catalogue.identities();indexed.source_keys=&reordered_catalogue.keys();
    reconcile(d,indexed,indexed_selection);CHECK(indexed_selection.active.source==1);
    auto stale_keys=catalogue.keys();Context foreign=context;foreign.source_identities=&source_reorder;foreign.source_catalog=&catalogue;foreign.source_keys=&stale_keys;
    auto foreign_selection=selection;reconcile(d,foreign,foreign_selection);CHECK(foreign_selection.active.source==1);
    source_reorder[2]=source_reorder[1];SourceCatalog ambiguous_catalogue(source_reorder);indexed.source_catalog=&ambiguous_catalogue;indexed.source_identities=&ambiguous_catalogue.identities();indexed.source_keys=&ambiguous_catalogue.keys();
    CHECK(!ambiguous_catalogue.unique_index(selection.active.source_key,indexed_source));reconcile(d,indexed,indexed_selection);CHECK(indexed_selection.items.empty());
    foreign_selection=selection;reconcile(d,foreign,foreign_selection);CHECK(foreign_selection.items.empty());
    Command command;command.operation=Operation::Rename;command.name="starting_crate";CHECK(execute(d,selection,context,command,next,result,error)&&!result.changed&&next.entities.nodes.empty());
    command.operation=Operation::Reparent;CHECK(execute(d,selection,context,command,next,result,error)&&!result.changed&&next.entities.nodes.empty());
    command.operation=Operation::Rename;command.name="Front gate supply";CHECK(execute(d,selection,context,command,next,result,error)&&result.changed&&next.entities.nodes.size()==1);
    CHECK(result.selection.active.kind==Kind::Entity&&world_entity::find(next.entities,result.selection.active.id)->binding==world_entity::Binding({EntityKind::SourceModel,0}));
    d={};selection.clear();d.placements={{"tavern",100,200},{"barrel",103,201}};
    world_studio::Light light;light.x=102;light.y=12;light.z=201;d.lights={light};world_studio::NavigationArea nav;nav.name="Court";nav.x=102;nav.z=202;nav.span=20;d.navigation={nav};
    world_studio::Actor actor;actor.name="Innkeeper";actor.navigation=0;actor.points={{{101,201}},{{104,201}},{{104,204}}};d.actors={actor};
    world_studio::Particle fire;fire.kind=2;fire.x=103;fire.y=11;fire.z=202;fire.ceiling=25;d.particles={fire};world_studio::Water pond;pond.source=3;pond.x=105;pond.z=206;pond.level=9;pond.radius_x=3;pond.radius_z=5;d.waters={pond};
    CHECK(world_authoring::synchronize(d,d.entities,identities,flat,nullptr,error));
    command={};command.operation=Operation::CreateGroup;command.name="Tavern courtyard";command.transform={100,10,200,0,1};CHECK(execute(d,selection,context,command,next,result,error));d=next;selection=result.selection;const Id root=result.created;CHECK(root!=0);
    const auto children=d.entities.nodes;
    for(const auto &child:children)if(child.id!=root){command={};command.operation=Operation::Reparent;command.subject=Ref::entity(child.id);command.parent=root;world_entity::Transform before,after;CHECK(world_entity::world_transform(d.entities,child.id,before,error));CHECK(execute(d,selection,context,command,next,result,error));d=next;CHECK(world_entity::world_transform(d.entities,child.id,after,error)&&near(before.x,after.x)&&near(before.y,after.y)&&near(before.z,after.z));}
    CHECK(select(d,context,selection,Ref::entity(root),false,error));const auto before_copy=bytes(d);std::set<Id> original_ids;for(const auto &node:d.entities.nodes)original_ids.insert(node.id);
    command={};command.operation=Operation::Duplicate;command.offset={20,0,0};CHECK(execute(d,selection,context,command,next,result,error)&&result.changed&&result.created!=root);const Id copy=result.created;
    CHECK(next.placements.size()==4&&next.lights.size()==2&&next.actors.size()==2&&next.particles.size()==2&&next.waters.size()==2&&next.navigation.size()==2);
    CHECK(next.actors[1].navigation==1&&near(next.actors[1].points[0][0],121)&&near(next.lights[1].x,122)&&near(next.waters[1].x,125));
    CHECK(bytes(d)==before_copy);for(const auto &node:next.entities.nodes)if(world_entity::find(d.entities,node.id)==nullptr)CHECK(original_ids.count(node.id)==0);
    auto duplicated=next;Selection copied=result.selection;command={};command.operation=Operation::Transform;CHECK(world_pose(duplicated,context,Ref::entity(copy),command.transform,error));command.transform.x+=10;
    CHECK(execute(duplicated,copied,context,command,next,result,error));CHECK(near(next.lights[1].x,132)&&near(next.actors[1].points[0][0],131)&&near(next.lights[0].x,102));
    command.transform.yaw=45;world_studio::Document untouched=duplicated;Result untouched_result;untouched_result.created=777;const auto untouched_bytes=bytes(untouched);CHECK(!execute(duplicated,copied,context,command,untouched,untouched_result,error));CHECK(bytes(untouched)==untouched_bytes&&untouched_result.created==777);
    // Protected live references and locked descendants reject the entire edit.
    Context protected_context=context;const Id actor_id=id_for(d,EntityKind::Actor,0);protected_context.protected_entities={actor_id};command={};command.operation=Operation::Erase;
    CHECK(!execute(d,selection,protected_context,command,untouched,untouched_result,error)&&bytes(d)==before_copy&&untouched_result.created==777);
    auto locked=d;world_entity::find(locked.entities,actor_id)->locked=true;CHECK(!execute(locked,selection,context,command,untouched,untouched_result,error));
    command={};command.operation=Operation::Duplicate;CHECK(!execute(locked,selection,context,command,untouched,untouched_result,error));
    // Preserve-children removes only the group; protected child IDs/poses survive.
    command={};command.operation=Operation::Erase;command.children=world_entity::ChildPolicy::PreserveWorld;CHECK(execute(d,selection,protected_context,command,next,result,error));CHECK(!world_entity::find(next.entities,root)&&world_entity::find(next.entities,actor_id)&&next.actors.size()==1);
    world_entity::Transform retained;CHECK(world_entity::world_transform(next.entities,actor_id,retained,error)&&near(retained.x,101)&&near(retained.z,201));
    auto hidden=d;world_entity::find(hidden.entities,root)->visible=false;CHECK(execute(hidden,selection,context,command,next,result,error));CHECK(!world_entity::visible(next.entities,{EntityKind::Actor,0}));
    command={};command.operation=Operation::Duplicate;CHECK(execute(hidden,selection,context,command,next,result,error));CHECK(!world_entity::visible(next.entities,{EntityKind::Placement,2})&&!world_entity::visible(next.entities,{EntityKind::Actor,1}));CHECK(world_entity::find(next.entities,id_for(next,EntityKind::Actor,1))->name=="Innkeeper");
    world_entity::find(next.entities,result.created)->visible=true;CHECK(world_entity::visible(next.entities,{EntityKind::Placement,2})&&world_entity::visible(next.entities,{EntityKind::Actor,1}));
    // Connected groups can contain existing base models without rewriting or
    // cloning their source files. Their real override and graph pose agree.
    command={};command.operation=Operation::Reparent;command.subject=Ref::source_model(0);command.parent=root;CHECK(execute(d,selection,context,command,next,result,error));auto with_source=next;
    command={};command.operation=Operation::Transform;command.transform={110,12,200,0,1};CHECK(execute(with_source,selection,context,command,next,result,error));CHECK(next.overrides.size()==1&&next.overrides[0].index==0&&near(next.overrides[0].x,10)&&near(next.overrides[0].offset_y,2));
    world_entity::Transform source_world;CHECK(world_pose(next,context,from_binding(next,{EntityKind::SourceModel,0}),source_world,error)&&near(source_world.x,10)&&near(source_world.y,12));
    // Multiple existing base sources use one validated bulk pose pass. Source
    // bindings, identities and distinct ground offsets survive the group move.
    auto multi_sources=with_source;command={};command.operation=Operation::Reparent;command.subject=Ref::source_model(1);command.parent=root;
    CHECK(execute(multi_sources,selection,context,command,next,result,error));multi_sources=next;command={};command.operation=Operation::Transform;command.transform={110,12,200,0,1};
    CHECK(execute(multi_sources,selection,context,command,next,result,error)&&next.overrides.size()==2);
    for(unsigned index=0;index<2;index++){const auto source_ref=from_binding(next,{EntityKind::SourceModel,index});CHECK(source_ref.kind==Kind::Entity&&world_pose(next,context,source_ref,source_world,error)&&near(source_world.x,10+double(index)*3)&&near(source_world.y,12));CHECK(world_entity::find(next.entities,source_ref.id)->source_key==world_authoring::source_key(identities[index]));}
    // Individual base and added models use the same transactional absolute
    // transform API as group roots; typed attachments follow the bound root.
    world_studio::Document lone_source;Selection source_selection;CHECK(select(lone_source,context,source_selection,Ref::source_model(0),false,error));
    command={};command.operation=Operation::Transform;command.transform={0,10,0,0,1};CHECK(execute(lone_source,source_selection,context,command,next,result,error)&&!result.changed&&next.entities.nodes.empty());
    Context sloped=context;sloped.ground={slope,nullptr};command.transform={5,15,2,90,2};CHECK(execute(lone_source,source_selection,sloped,command,next,result,error));
    const Id transformed_source=result.selection.active.id;CHECK(transformed_source!=0&&next.entities.nodes.size()==1&&next.overrides.size()==1&&near(next.overrides[0].offset_y,4));
    CHECK(world_pose(next,sloped,Ref::entity(transformed_source),source_world,error)&&near(source_world.x,5)&&near(source_world.y,15)&&near(source_world.yaw,90)&&near(source_world.scale,2));
    CHECK(world_entity::find(next.entities,transformed_source)->binding==world_entity::Binding({EntityKind::SourceModel,0}));
    // Leaf placement drags preserve ground support and burial/elevation scale
    // below a transformed parent, without moving that parent or its siblings.
    world_studio::Document leaf_model;leaf_model.placements.push_back({"crate",100,200,0,1,.4f,.7f,2});
    CHECK(world_authoring::synchronize(leaf_model,leaf_model.entities,identities,flat,nullptr,error));const Id leaf_id=id_for(leaf_model,EntityKind::Placement,0);Id leaf_parent=0;
    CHECK(world_entity::add(leaf_model.entities,0,"Parent",{}, {10,5,20,30,1.4},leaf_parent,error));CHECK(world_entity::reparent(leaf_model.entities,leaf_id,leaf_parent,true,error));
    Selection leaf_selection;CHECK(select(leaf_model,context,leaf_selection,Ref::entity(leaf_id),false,error));command={};command.operation=Operation::Transform;command.transform={110,14,220,40,1.25};
    CHECK(execute(leaf_model,leaf_selection,context,command,next,result,error)&&next.entities.nodes.size()==2&&world_entity::find(next.entities,leaf_id)->parent==leaf_parent);
    CHECK(near(next.placements[0].burial,.5)&&near(next.placements[0].elevation,.875)&&near(next.placements[0].offset_y,3.625));
    world_entity::Transform leaf_pose;CHECK(world_entity::world_transform(next.entities,leaf_id,leaf_pose,error)&&near(leaf_pose.x,110)&&near(leaf_pose.y,14)&&near(leaf_pose.z,220)&&near(leaf_pose.scale,1.25));
    CHECK(world_entity::world_transform(next.entities,leaf_parent,leaf_pose,error)&&near(leaf_pose.x,10)&&near(leaf_pose.y,5)&&near(leaf_pose.scale,1.4));
    auto bound_building=d;bound_building.particles[0].height=2;const Id building_id=id_for(d,EntityKind::Placement,0);
    for(auto binding:{world_entity::Binding{EntityKind::Light,0},{EntityKind::Actor,0},{EntityKind::Particle,0},{EntityKind::Navigation,0}})CHECK(world_entity::reparent(bound_building.entities,id_for(d,binding.kind,binding.index),building_id,true,error));
    Selection building_selection;CHECK(select(bound_building,context,building_selection,Ref::entity(building_id),false,error));
    command={};command.operation=Operation::Transform;command.transform={110,12,200,90,1.5};CHECK(execute(bound_building,building_selection,context,command,next,result,error));
    CHECK(world_entity::find(next.entities,building_id)->binding==world_entity::Binding({EntityKind::Placement,0})&&world_entity::find(next.entities,building_id)->parent==root&&next.entities.nodes.size()==bound_building.entities.nodes.size());
    CHECK(near(next.placements[0].x,110)&&near(next.placements[0].offset_y,2)&&near(next.placements[0].scale,1.5)&&near(next.lights[0].x,111.5)&&near(next.lights[0].y,15)&&near(next.lights[0].radius,12));
    CHECK(near(next.actors[0].points[0][0],111.5)&&near(next.actors[0].points[0][1],198.5)&&near(next.navigation[0].span,30));
    const auto bound_before=bytes(bound_building);command.transform.scale=10000;CHECK(!execute(bound_building,building_selection,context,command,untouched,untouched_result,error)&&bytes(bound_building)==bound_before&&untouched_result.created==777);
    auto bound_lock=bound_building;world_entity::find(bound_lock.entities,actor_id)->locked=true;command.transform.scale=1.5;CHECK(!execute(bound_lock,building_selection,context,command,untouched,untouched_result,error));
    EditScope numeric;world_studio::History numeric_history;auto numeric_doc=lone_source;auto numeric_selection=source_selection;CHECK(numeric.begin(numeric_doc,numeric_selection));
    for(unsigned i=0;i<25;i++){command={};command.operation=Operation::Transform;command.transform={double(i)*.2,12,0,0,1};CHECK(numeric.update(context,command,numeric_doc,numeric_selection,result,error));}
    CHECK(numeric.commit(numeric_history)&&numeric_history.back.size()==1&&numeric_doc.entities.nodes.size()==1);
    const auto accepted_numeric=bytes(numeric_doc);CHECK(numeric.begin(numeric_doc,numeric_selection));command.transform.x=40;CHECK(numeric.update(context,command,numeric_doc,numeric_selection,result,error));CHECK(numeric.cancel(numeric_doc,numeric_selection)&&numeric_history.back.size()==1);
    CHECK(near(numeric_doc.overrides[0].x,4.8)&&near(numeric_doc.overrides[0].offset_y,2));(void)accepted_numeric;
    CHECK(numeric.begin(numeric_doc,numeric_selection));CHECK(world_pose(numeric_doc,context,numeric_selection.active,command.transform,error));CHECK(numeric.update(context,command,numeric_doc,numeric_selection,result,error)&&!numeric.commit(numeric_history)&&numeric_history.back.size()==1);
    // A lazy-source preview can issue a persistent ID, then return exactly to
    // its baseline. The final no-op adds no history, but that exposed ID must
    // not be recycled by the next independently accepted editor command.
    EditScope returned_scope;world_studio::History returned_history;auto returned_doc=lone_source;auto returned_selection=source_selection;
    CHECK(returned_scope.begin(returned_doc,returned_selection));command={};command.operation=Operation::Transform;command.transform={1,10,0,0,1};
    CHECK(returned_scope.update(context,command,returned_doc,returned_selection,result,error));const Id preview_id=returned_selection.active.id;const Id preview_next=returned_doc.entities.next_id;CHECK(preview_id!=0&&preview_next>preview_id);
    command.transform.x=2;CHECK(returned_scope.update(context,command,returned_doc,returned_selection,result,error)&&returned_selection.active.id==preview_id);
    command.transform.x=0;CHECK(returned_scope.update(context,command,returned_doc,returned_selection,result,error)&&!result.changed&&returned_doc.entities.nodes.empty()&&returned_doc.entities.next_id>=preview_next);
    CHECK(!returned_scope.commit(returned_history)&&returned_history.back.empty());command={};command.operation=Operation::CreateGroup;command.name="After returned preview";
    CHECK(execute(returned_doc,returned_selection,context,command,next,result,error)&&result.created>preview_id&&next.entities.next_id>result.created);
    // Generic queries observe same-size mutable graph/record edits directly;
    // malformed external bindings fail before indexing any authored vector.
    auto mutable_model=numeric_doc;auto model_ref=numeric_selection.active;world_entity::Transform mutable_pose;CHECK(world_pose(mutable_model,context,model_ref,mutable_pose,error));
    mutable_model.overrides[0].x+=3;CHECK(world_pose(mutable_model,context,model_ref,mutable_pose,error)&&near(mutable_pose.x,7.8));
    world_entity::find(mutable_model.entities,model_ref.id)->binding={EntityKind::Placement,999};CHECK(!world_pose(mutable_model,context,model_ref,mutable_pose,error));
    world_entity::find(mutable_model.entities,model_ref.id)->binding={EntityKind::Actor,0};world_studio::Actor invalid_actor;invalid_actor.points.clear();mutable_model.actors={invalid_actor};CHECK(!world_pose(mutable_model,context,model_ref,mutable_pose,error));
    // Invalid snapshots remain rejected after repeated updates. Altering the
    // external document cannot invalidate or replace the owned baseline.
    auto invalid_baseline=numeric_doc;invalid_baseline.look.sun=-1;EditScope invalid_scope;Selection invalid_selection=numeric_selection;CHECK(invalid_scope.begin(invalid_baseline,invalid_selection));
    const auto valid_output_before=bytes(numeric_doc);CHECK(!invalid_scope.update(context,command,numeric_doc,numeric_selection,result,error));CHECK(bytes(numeric_doc)==valid_output_before);
    CHECK(!invalid_scope.update(context,command,numeric_doc,numeric_selection,result,error));CHECK(bytes(numeric_doc)==valid_output_before);CHECK(!invalid_scope.commit(numeric_history)&&numeric_history.back.size()==1);
    command={};command.operation=Operation::Transform;command.subject=Ref::entity(actor_id);CHECK(world_pose(d,context,command.subject,command.transform,error));command.transform.y+=3;CHECK(!execute(d,selection,context,command,untouched,untouched_result,error)&&error.find("terrain")!=std::string::npos);
    command={};command.operation=Operation::Erase;
    command.children=world_entity::ChildPolicy::EraseSubtree;CHECK(execute(d,selection,context,command,next,result,error));CHECK(next.entities.nodes.empty()&&next.placements.empty()&&next.lights.empty()&&next.actors.empty()&&next.navigation.empty()&&next.particles.empty()&&next.waters.empty());
    // ID selection survives compaction and never aliases a replacement index.
    Selection barrel;const Id barrel_id=id_for(d,EntityKind::Placement,1);CHECK(select(d,context,barrel,Ref::entity(barrel_id),false,error));auto compact=d;CHECK(world_authoring::erase_record(compact,{EntityKind::Placement,0},error));reconcile(compact,context,barrel);legacy=project(compact,context,barrel);CHECK(legacy.selected_added==0&&barrel.active.id==barrel_id);
    CHECK(world_authoring::erase_record(compact,{EntityKind::Placement,0},error));compact.placements.push_back({"new_crate",0,0});CHECK(world_authoring::synchronize(compact,compact.entities,identities,flat,nullptr,error));reconcile(compact,context,barrel);CHECK(barrel.items.empty());
    command={};command.operation=Operation::Reparent;command.subject=Ref::entity(root);command.parent=actor_id;CHECK(!execute(d,selection,context,command,untouched,untouched_result,error)&&bytes(d)==before_copy);
    command={};command.operation=Operation::Rename;command.name="Court renamed";auto host_reject=context;host_reject.validate_candidate=reject_candidate;CHECK(!execute(d,selection,host_reject,command,untouched,untouched_result,error)&&bytes(untouched)==untouched_bytes);
    // One hundred intermediate accepted values create exactly one undo step.
    EditScope scope;world_studio::History history;auto editing=d;auto editing_selection=selection;CHECK(scope.begin(editing,editing_selection));
    for(unsigned i=0;i<100;i++){command={};command.operation=Operation::Rename;command.name="Court "+std::to_string(i);CHECK(scope.update(context,command,editing,editing_selection,result,error));}
    CHECK(history.back.empty());CHECK(scope.commit(history)&&history.back.size()==1);CHECK(history.undo(editing)&&world_entity::find(editing.entities,root)->name=="Tavern courtyard");CHECK(history.redo(editing)&&world_entity::find(editing.entities,root)->name=="Court 99");
    CHECK(scope.begin(editing,editing_selection));command.name="Cancelled";CHECK(scope.update(context,command,editing,editing_selection,result,error));CHECK(scope.cancel(editing,editing_selection)&&world_entity::find(editing.entities,root)->name=="Court 99"&&history.back.size()==1);
    CHECK(scope.begin(editing,editing_selection));command.name="Court 99";CHECK(scope.update(context,command,editing,editing_selection,result,error));CHECK(!scope.commit(history)&&history.back.size()==1);
    CHECK(scope.begin(editing,editing_selection));command.name="Intermediate";CHECK(scope.update(context,command,editing,editing_selection,result,error));command.name="Court 99";CHECK(scope.update(context,command,editing,editing_selection,result,error));CHECK(!scope.commit(history)&&history.back.size()==1);
    // Hiding lazy source zero remains possible with a full persistent graph.
    world_studio::Document full;for(unsigned i=0;i<world_entity::node_limit;i++)full.entities.nodes.push_back({Id(i+1),0,"Marker",{}, {}});full.entities.next_id=world_entity::node_limit+1;
    Selection lazy;CHECK(select(full,context,lazy,Ref::source_model(0),false,error));command={};command.operation=Operation::Erase;CHECK(execute(full,lazy,context,command,next,result,error)&&next.overrides.size()==1&&next.overrides[0].index==0&&next.overrides[0].hidden&&next.entities.nodes.size()==world_entity::node_limit);
    command={};command.operation=Operation::SetFlags;command.change_locked=true;command.locked=true;CHECK(!execute(full,lazy,context,command,untouched,untouched_result,error));
    world_studio::Document full_models;for(unsigned i=0;i<world_studio::placement_limit;i++)full_models.placements.push_back({"starting_crate",double(i),0});command={};command.operation=Operation::Duplicate;CHECK(!execute(full_models,lazy,context,command,untouched,untouched_result,error)&&error.find("capacity")!=std::string::npos);
    auto full_isolated=full;full_isolated.placements.push_back({"starting_crate",0,0});full_isolated.entities.nodes[0].binding={EntityKind::Placement,0};full_isolated.entities.nodes[0].local={0,10,0,0,1};Selection isolated;CHECK(select(full_isolated,context,isolated,Ref::entity(1),false,error));
    command={};command.operation=Operation::Transform;command.transform={5,13,0,30,1.2};CHECK(execute(full_isolated,isolated,context,command,next,result,error)&&next.entities.nodes.size()==world_entity::node_limit&&next.entities.next_id==full_isolated.entities.next_id&&near(next.placements[0].offset_y,3));
    Studio studio;studio.draft=d;studio.source_poses=poses;studio.identity_sources=identities;
    const auto graph_count=studio.draft.entities.nodes.size();CHECK(studio.designer_select(Ref::source_model(0)));CHECK(studio.selected==0&&studio.entity_selected==0&&studio.draft.entities.nodes.size()==graph_count);
    command={};command.operation=Operation::Rename;command.name="starting_crate";CHECK(studio.designer_run(command)&&studio.history.back.empty());command.name="Supply source";CHECK(studio.designer_run(command)&&studio.history.back.size()==1&&studio.entity_selected!=0&&studio.selected==0);
    CHECK(studio.designer_select(Ref::entity(actor_id)));CHECK(studio.actor_selection==0&&studio.selected==-1&&studio.selection_keys.empty());
    studio.tab=3;studio.light=0;studio.designer_capture_legacy();CHECK(studio.designer_selection.active.id==id_for(studio.draft,EntityKind::Light,0)&&studio.light==0&&studio.actor_selection==-1);
    studio.selection_keys={8193};studio.designer_capture_legacy();CHECK(studio.designer_selection.active.id==barrel_id&&studio.selected_added==1);
    CHECK(world_authoring::erase_record(studio.draft,{EntityKind::Placement,0},error));studio.designer_reconcile_selection();CHECK(studio.selected_added==0&&studio.designer_selection.active.id==barrel_id);
    studio.components_ready=true;studio.components_active_document.living={{actor_id}};CHECK(studio.designer_select(Ref::entity(actor_id)));command={};command.operation=Operation::Erase;const auto adapter_before=bytes(studio.draft);CHECK(!studio.designer_run(command)&&studio.history.back.size()==1&&bytes(studio.draft)==adapter_before);
    CHECK(studio.designer_begin_edit());command={};command.operation=Operation::Rename;command.name="Accepted name";CHECK(studio.designer_update_edit(command));studio.reject=true;command.name="Rejected name";CHECK(!studio.designer_update_edit(command));CHECK(studio.draft.actors[0].name=="Accepted name");studio.reject=false;CHECK(studio.designer_finish_edit()&&studio.history.back.size()==2);
    CHECK(studio.designer_begin_edit());command.name="Cancelled name";CHECK(studio.designer_update_edit(command));CHECK(studio.designer_cancel_edit()&&studio.draft.actors[0].name=="Accepted name"&&studio.history.back.size()==2);
    // A same-size source reload explicitly invalidates Studio's catalogue even
    // if its legacy signature is unchanged (for example an importer rebuild).
    Studio source_reload;source_reload.source_poses=poses;source_reload.identity_sources=identities;CHECK(source_reload.designer_select(Ref::source_model(0)));
    const auto source_generation=source_reload.designer_source_generation;
    std::swap(source_reload.identity_sources[0],source_reload.identity_sources[1]);std::swap(source_reload.source_poses[0],source_reload.source_poses[1]);source_reload.designer_invalidate_sources();source_reload.designer_reconcile_selection();CHECK(source_reload.selected==1&&source_reload.designer_source_generation!=source_generation);
    Studio first_tool;first_tool.draft=d;first_tool.source_poses=poses;first_tool.identity_sources=identities;first_tool.tab=3;first_tool.light=0;first_tool.designer_capture_legacy();CHECK(first_tool.designer_selection.active.id==id_for(d,EntityKind::Light,0));
    Studio new_record;new_record.source_poses=poses;new_record.identity_sources=identities;new_record.draft.placements.push_back({"new_crate",0,0});const auto before_binding_revision=new_record.revision;
    const auto new_binding=new_record.designer_binding_ref({EntityKind::Placement,0});CHECK(new_binding.kind==Kind::Entity&&new_record.revision>before_binding_revision);
    const auto existing_binding_revision=new_record.revision;CHECK(new_record.designer_binding_ref({EntityKind::Placement,0})==new_binding&&new_record.revision==existing_binding_revision);
    // Deleting a lazily registered imported model creates a hidden override.
    // Restoring visibility must retain source tilt and independent axis scale.
    {
        auto shaped=poses;shaped[0].pitch=.31;shaped[0].roll=-.22;shaped[0].sx=1.4;shaped[0].sy=.7;shaped[0].sz=1.8;
        Context shaped_context=context;shaped_context.source_poses=&shaped;world_studio::Document hidden,restored;Selection source_selection;
        CHECK(select(hidden,shaped_context,source_selection,Ref::source_model(0),false,error));
        Command erase;erase.operation=Operation::Erase;CHECK(execute(hidden,source_selection,shaped_context,erase,restored,result,error));
        world_studio::Pose after;CHECK(world_studio::effective_pose(restored,shaped,0,after)&&after.hidden);
        CHECK(near(after.pitch,shaped[0].pitch)&&near(after.roll,shaped[0].roll)&&near(after.sx,shaped[0].sx)&&near(after.sy,shaped[0].sy)&&near(after.sz,shaped[0].sz));
        Command show;show.operation=Operation::SetFlags;show.subject=Ref::source_model(0);show.change_visible=true;show.visible=true;
        CHECK(execute(restored,source_selection,shaped_context,show,hidden,result,error));
        CHECK(world_studio::effective_pose(hidden,shaped,0,after)&&!after.hidden&&near(after.pitch,shaped[0].pitch)&&near(after.sz,shaped[0].sz));
    }
    CHECK(d.entities.nodes.size()<32);std::cout<<"world_designer: "<<checks<<" lazy-source/persistent-selection/mixed-prefab/locks/live-reference/atomic-command/gesture contracts passed\n";
}
