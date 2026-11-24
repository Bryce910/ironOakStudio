#include "engine/world_designer.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <unistd.h>
using namespace world_designer;
using Document=world_studio::Document;
using EK=world_entity::Kind;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
static double flat(double,double,const void *){return 10;}
static bool bounds(const std::string &,world_studio::Vec &lo,world_studio::Vec &hi,const void *){lo={-1,0,-1};hi={1,3,1};return true;}
static std::string bytes(const Document &d){std::string result,e;if(!world_studio::encode(d,result,e))return "INVALID "+e;return result;}
static std::string read(const std::string &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void write(const std::string &p,const std::string &s){std::ofstream f(p,std::ios::binary);f<<s;}
static bool same_wind(const world_studio::Placement &a,const world_studio::Placement &b){return a.wind_enabled==b.wind_enabled&&a.wind_sway==b.wind_sway&&a.wind_flutter==b.wind_flutter;}
static Id id_for(const Document &d,EK k,unsigned i){const auto r=from_binding(d,{k,i});return r.kind==Kind::Entity?r.id:0;}
static std::string replace_once(std::string s,const std::string &a,const std::string &b){auto i=s.find(a);if(i==std::string::npos)return "MISSING";s.replace(i,a.size(),b);return s;}
int main(int argc,char **argv){
    std::string error;const std::string folder=argc>1?argv[1]:"/tmp";
    const std::string path=folder+"/wind-authoring-fixture-"+std::to_string(getpid())+".studio";
    const std::string prefab_path=path+".vwprefab";
    std::vector<world_studio::Pose> source(2);std::vector<world_authoring::Source> identities(2);
    for(unsigned i=0;i<2;++i){source[i].alias="oak";source[i].root={double(i)*8,10,0};source[i].minimum={-1,0,-1};source[i].maximum={1,3,1};identities[i]={"oak","trees.txt",double(i)*8,0,10};}
    Context context;context.source_poses=&source;context.source_identities=&identities;context.source_signature="0123456789abcdef";context.ground={flat,nullptr};
    Document d,next,loaded;Selection selection;Result result;Command command;command.operation=Operation::SetWind;
    CHECK(select(d,context,selection,Ref::source_model(0),false,error));
    CHECK(execute(d,selection,context,command,next,result,error)&&!result.changed&&next.entities.nodes.empty());
    command.change_wind_enabled=true;command.wind_enabled=false;command.change_wind_sway=true;command.wind_sway=.25f;command.change_wind_flutter=true;command.wind_flutter=1.75f;
    CHECK(execute(d,selection,context,command,next,result,error)&&result.changed&&result.geometry);
    CHECK(d.entities.nodes.empty()&&d.overrides.empty());d=next;selection=result.selection;
    CHECK(d.overrides.size()==1&&d.overrides[0].index==0&&!d.overrides[0].wind_enabled&&d.overrides[0].wind_sway==.25f&&d.overrides[0].wind_flutter==1.75f);
    CHECK(d.source_signature==context.source_signature&&d.entities.nodes.size()==1&&selection.active.kind==Kind::Entity);
    const Id source_id=selection.active.id;const auto persistent_key=world_entity::find(d.entities,source_id)->source_key;
    CHECK(persistent_key==world_authoring::source_key(identities[0]));
    world_studio::Pose p,q;CHECK(world_studio::effective_pose(d,source,0,p)&&!p.wind_enabled&&p.wind_sway==.25f&&p.wind_flutter==1.75f);
    CHECK(world_studio::effective_pose(d,source,1,q)&&q.wind_enabled&&q.wind_sway==1&&q.wind_flutter==1);
    CHECK(execute(d,selection,context,command,next,result,error)&&!result.changed&&bytes(next)==bytes(d));
    CHECK(world_studio::save(path.c_str(),d,error));const auto encoded=read(path);CHECK(encoded.rfind("VALWOOD_STUDIO 10\n",0)==0);
    CHECK(world_studio::load(path.c_str(),loaded,error)&&bytes(loaded)==encoded&&world_entity::find(loaded.entities,source_id)->source_key==persistent_key);
    // The persisted source key can safely rebind the full wind override after a source reorder.
    auto reversed=identities;std::swap(reversed[0],reversed[1]);auto graph=d.entities;
    CHECK(world_authoring::reconcile(loaded,graph,reversed,"fedcba9876543210",flat,nullptr,error));loaded.entities=graph;
    CHECK(loaded.overrides[0].index==1&&!loaded.overrides[0].wind_enabled&&loaded.overrides[0].wind_sway==.25f&&loaded.overrides[0].wind_flutter==1.75f);
    CHECK(world_entity::find(loaded.entities,source_id)->binding==world_entity::Binding({EK::SourceModel,1}));
    // Copying an imported source detaches it into an independent added placement.
    Clipboard clipboard;CHECK(clipboard.copy(d,selection,context,{bounds,nullptr},error));PasteOptions paste;paste.snap=false;
    CHECK(clipboard.paste(d,context,paste,next,result,error)&&next.placements.size()==1);
    CHECK(!next.placements[0].wind_enabled&&next.placements[0].wind_sway==.25f&&next.placements[0].wind_flutter==1.75f&&next.overrides.size()==1);
    CHECK(world_entity::find(next.entities,result.created)->binding==world_entity::Binding({EK::Placement,0}));
    command={};command.operation=Operation::Duplicate;command.offset={3,0,0};CHECK(execute(d,selection,context,command,next,result,error));
    CHECK(next.placements.size()==1&&!next.placements[0].wind_enabled&&next.placements[0].wind_sway==.25f&&next.placements[0].wind_flutter==1.75f);
    std::vector<unsigned> created;auto legacy=d;CHECK(world_studio::duplicate_selection(legacy,source,{0},{2,0,0},created,error));
    CHECK(created==std::vector<unsigned>{8192}&&same_wind(legacy.placements[0],next.placements[0]));
    // A source transform and hide preserve its independent response.
    command={};command.operation=Operation::Transform;command.transform={1,10,2,20,1};CHECK(execute(d,selection,context,command,next,result,error));
    CHECK(!next.overrides[0].wind_enabled&&next.overrides[0].wind_sway==.25f&&next.overrides[0].wind_flutter==1.75f);
    command={};command.operation=Operation::Erase;CHECK(execute(d,selection,context,command,next,result,error)&&next.overrides[0].hidden&&!next.overrides[0].wind_enabled);
    // Selection expands groups and nested/overlapping refs once; unrelated same-alias neighbors stay untouched.
    d={};d.placements={{"oak",20,0},{"oak",24,0},{"oak",28,0}};world_studio::Light light;light.x=20;light.y=12;d.lights.push_back(light);
    CHECK(world_authoring::synchronize(d,d.entities,identities,flat,nullptr,error));
    const Id first=id_for(d,EK::Placement,0),second=id_for(d,EK::Placement,1),neighbor=id_for(d,EK::Placement,2),light_id=id_for(d,EK::Light,0);Id root=0,nested=0;
    CHECK(world_entity::add(d.entities,0,"Wind group",{}, {0,10,0,0,1},root,error));
    CHECK(world_entity::add(d.entities,root,"Nested",{}, {},nested,error));
    CHECK(world_entity::reparent(d.entities,first,root,true,error));CHECK(world_entity::reparent(d.entities,second,nested,true,error));CHECK(world_entity::reparent(d.entities,light_id,root,true,error));
    selection.clear();CHECK(select(d,context,selection,Ref::entity(root),false,error));CHECK(select(d,context,selection,Ref::entity(second),true,error));selection.active=Ref::entity(root);
    command={};command.operation=Operation::SetWind;command.change_wind_sway=true;command.wind_sway=2;command.change_wind_flutter=true;command.wind_flutter=0;
    CHECK(execute(d,selection,context,command,next,result,error)&&result.geometry&&next.entities.nodes.size()==d.entities.nodes.size());
    CHECK(next.placements[0].wind_sway==2&&next.placements[1].wind_sway==2&&next.placements[2].wind_sway==1&&next.placements[0].wind_flutter==0&&next.placements[1].wind_flutter==0&&next.placements[2].wind_flutter==1);
    CHECK(world_entity::find(next.entities,root)&&world_entity::find(next.entities,nested)&&world_entity::find(next.entities,first)&&world_entity::find(next.entities,second)&&world_entity::find(next.entities,neighbor));
    const auto group_before=bytes(d),group_after=bytes(next);world_studio::History history;history.checkpoint(d);d=next;
    CHECK(history.undo(d)&&bytes(d)==group_before);CHECK(history.redo(d)&&bytes(d)==group_after);
    // One slider gesture produces one undo; cancel and returning to the original produce none.
    EditScope gesture;world_studio::History gesture_history;CHECK(gesture.begin(d,selection));
    for(unsigned i=0;i<25;++i){command.wind_sway=float(i)/24.f;CHECK(gesture.update(context,command,d,selection,result,error));}
    CHECK(gesture.commit(gesture_history)&&gesture_history.back.size()==1&&d.placements[0].wind_sway==1);
    CHECK(gesture_history.undo(d)&&bytes(d)==group_after);CHECK(gesture_history.redo(d)&&d.placements[1].wind_sway==1);
    const auto before_cancel=bytes(d);CHECK(gesture.begin(d,selection));command.wind_sway=.3f;CHECK(gesture.update(context,command,d,selection,result,error));CHECK(gesture.cancel(d,selection)&&bytes(d)==before_cancel);
    CHECK(gesture.begin(d,selection));command.wind_sway=.2f;CHECK(gesture.update(context,command,d,selection,result,error));command.wind_sway=1;CHECK(gesture.update(context,command,d,selection,result,error));CHECK(!gesture.commit(gesture_history)&&gesture_history.back.size()==1);
    // Locking one affected model or ancestor rejects the full command and preserves both outputs.
    auto locked=d;world_entity::find(locked.entities,second)->locked=true;Document untouched;untouched.placements={{"sentinel",99,99}};Result sentinel;sentinel.created=777;const auto sentinel_bytes=bytes(untouched);
    command.wind_sway=.8f;CHECK(!execute(locked,selection,context,command,untouched,sentinel,error)&&bytes(untouched)==sentinel_bytes&&sentinel.created==777);
    locked=d;world_entity::find(locked.entities,root)->locked=true;CHECK(!execute(locked,selection,context,command,untouched,sentinel,error)&&bytes(untouched)==sentinel_bytes);
    // A locked unrelated light does not prevent editing model response.
    locked=d;world_entity::find(locked.entities,light_id)->locked=true;CHECK(execute(locked,selection,context,command,next,result,error));
    Selection no_model;CHECK(select(d,context,no_model,Ref::entity(light_id),false,error));CHECK(!execute(d,no_model,context,command,untouched,sentinel,error));
    for(float invalid:{-.001f,2.001f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){command.wind_sway=invalid;CHECK(!execute(d,selection,context,command,untouched,sentinel,error)&&bytes(untouched)==sentinel_bytes);command.wind_sway=1;command.wind_flutter=invalid;CHECK(!execute(d,selection,context,command,untouched,sentinel,error));command.wind_flutter=1;}
    // Prefab capture, disk wrapper, instantiate and connected duplicate retain item values and issue fresh IDs.
    d.placements[0].wind_enabled=false;d.placements[0].wind_sway=.4f;d.placements[0].wind_flutter=1.6f;d.placements[1].wind_sway=1.8f;d.placements[1].wind_flutter=.2f;
    world_prefab::Selection capture;capture.objects={{EK::Placement,0},{EK::Placement,1}};capture.ground={flat,nullptr};world_prefab::Prefab prefab,portable;
    CHECK(world_prefab::capture(d,capture,{20,10,0,0,1},"Wind trees",prefab,error));CHECK(world_prefab::save(prefab_path.c_str(),prefab,error));CHECK(world_prefab::load(prefab_path.c_str(),portable,error));
    CHECK(same_wind(portable.content.placements[0],d.placements[0])&&same_wind(portable.content.placements[1],d.placements[1]));
    world_prefab::Instance instance;auto graph_empty=world_entity::Graph{};Document dest;CHECK(world_prefab::instantiate(portable,dest,graph_empty,{50,10,0,0,1},0,instance,error,{flat,nullptr}));dest.entities=graph_empty;
    CHECK(same_wind(dest.placements[0],d.placements[0])&&same_wind(dest.placements[1],d.placements[1]));
    command={};command.operation=Operation::Duplicate;CHECK(execute(d,selection,context,command,next,result,error)&&next.placements.size()==5);
    CHECK(same_wind(next.placements[3],d.placements[0])&&same_wind(next.placements[4],d.placements[1])&&result.created!=root);
    // Removing an earlier placement preserves surviving IDs and item values at their remapped indices.
    const auto survivor=d.placements[1];CHECK(world_authoring::erase_record(d,{EK::Placement,0},error));CHECK(same_wind(d.placements[0],survivor)&&id_for(d,EK::Placement,0)==second&&id_for(d,EK::Placement,1)==neighbor);
    // Every v10 item, including defaults, gets an exact three-field tail; particle and water remain v9-compatible.
    Document mixed;mixed.placements={{"oak",0,0},{"oak",4,0}};mixed.placements[0].wind_enabled=false;mixed.placements[0].wind_sway=.25f;mixed.placements[0].wind_flutter=1.75f;mixed.overrides.push_back({1,8,0,0,1,false});mixed.source_signature=context.source_signature;mixed.particles.push_back({});mixed.waters.push_back({});
    CHECK(world_studio::save(path.c_str(),mixed,error));const auto valid10=read(path);CHECK(world_studio::load(path.c_str(),loaded,error)&&bytes(loaded)==valid10);
    CHECK((loaded.particles[0].tint==std::array<float,3>({1,1,1})&&loaded.waters[0].basin_depth==0&&loaded.placements[1].wind_enabled&&loaded.placements[1].wind_sway==1));
    std::istringstream lines(valid10);std::string line;unsigned props=0,overrides=0;
    while(std::getline(lines,line)){std::istringstream fields(line);std::string field;unsigned count=0;while(fields>>field)++count;if(line.rfind("prop ",0)==0){CHECK(count==17);++props;}if(line.rfind("override ",0)==0){CHECK(count==16);++overrides;}}
    CHECK(props==2&&overrides==1);
    // Invalid saves preserve previous bytes; malformed loads preserve the previous document atomically.
    for(float invalid:{-1.f,2.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){auto broken=mixed;broken.placements[0].wind_sway=invalid;CHECK(!world_studio::save(path.c_str(),broken,error)&&read(path)==valid10);broken=mixed;broken.overrides[0].wind_flutter=invalid;CHECK(!world_studio::save(path.c_str(),broken,error)&&read(path)==valid10);}
    const std::string tail=" 0 0.25 1.75\n";for(const std::string replacement:{" 2 0.25 1.75\n"," -1 0.25 1.75\n"," 0 nan 1.75\n"," 0 0.25 inf\n"," 0 0.25 2.01\n"," 0 0.25\n"," 0 0.25 1.75 extra\n"}){const auto broken=replace_once(valid10,tail,replacement);CHECK(broken!="MISSING");write(path,broken);const auto before=bytes(loaded);CHECK(!world_studio::load(path.c_str(),loaded,error)&&bytes(loaded)==before);}
    write(path,replace_once(valid10,"VALWOOD_STUDIO 10\n","VALWOOD_STUDIO 9\n"));CHECK(!world_studio::load(path.c_str(),loaded,error));
    // Legacy versions1–9 are accepted; canonical output remains the historical version6–9 with defaults.
    for(int version=1;version<=9;++version){std::ostringstream old;old<<"VALWOOD_STUDIO "<<version<<"\nlook 0.28 1 1 1 17 0 0.28 0.48 0.85 0.62\nprop oak 0 0 0 1 0 0";if(version>=2)old<<" 0";if(version>=6)old<<" 0 0 1 1 1";old<<'\n';if(version>=4){std::string metadata;world_entity::Graph empty;CHECK(world_entity::encode(empty,metadata,error));std::istringstream entity_lines(metadata);while(std::getline(entity_lines,line))old<<"entities "<<std::quoted(line)<<'\n';}if(version>=5){std::string metadata;world_surface::Data empty;CHECK(world_surface::encode(empty,metadata,error));std::istringstream surface_lines(metadata);while(std::getline(surface_lines,line))old<<"surfaces "<<std::quoted(line)<<'\n';}write(path,old.str());CHECK(world_studio::load(path.c_str(),loaded,error)&&loaded.placements[0].wind_enabled&&loaded.placements[0].wind_sway==1&&loaded.placements[0].wind_flutter==1);CHECK(bytes(loaded).rfind("VALWOOD_STUDIO 6\n",0)==0);}
    // Resetting all item settings restores the old version selection and byte representation.
    for(int version=6;version<=9;++version){Document old;old.placements={{"oak",0,0}};if(version>=7){world_studio::Particle fx;fx.kind=3;old.particles.push_back(fx);}if(version>=8)old.ecology.push_back({});if(version>=9)old.ecology[0].preset=256;const auto original=bytes(old);CHECK(original.rfind("VALWOOD_STUDIO "+std::to_string(version)+"\n",0)==0);auto changed=old;changed.placements[0].wind_enabled=false;CHECK(bytes(changed).rfind("VALWOOD_STUDIO 10\n",0)==0);changed.placements[0].wind_enabled=true;CHECK(bytes(changed)==original);}
    unlink(path.c_str());unlink(prefab_path.c_str());
    std::cout<<"Item wind authoring: "<<checks<<" checks passed (per-item isolation, source identity rebind, groups, locks, Undo/gestures, clipboard/duplicate/prefab, compaction, atomic malformed records and Studio1–10 persistence)\n";
}
