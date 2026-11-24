#include "engine/world_structure_splines.h"
#include "engine/world_studio.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <unistd.h>
using namespace world_structure;
static unsigned checks=0;
static void check(bool value,const char *label){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::abort();}}
static bool near(double a,double b){return std::abs(a-b)<1e-8;}
static bool same(Point a,Point b){return near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z);}
static Definition fixture(Kind kind=Kind::Bridge){Definition d;d.id=1;d.kind=kind;d.width=2;d.points={{0,3,0},{0,5,20},{12,5,32}};return d;}
static std::string bytes(const world_studio::Document &d){std::string text,e;check(world_studio::encode(d,text,e),"encode fixture");return text;}
int main(){
    std::string e;Generated g;
    auto d=fixture();check(build(d,g,e),"sloping angled bridge builds");
    check(g.owner_id==1&&g.kind==Kind::Bridge&&!g.segments.empty(),"generated owner and kind");
    for(unsigned i=0;i<g.segments.size();++i){
        const auto &s=g.segments[i];check(s.owner_id==1&&s.index==i&&s.length<=d.spacing+1e-8,"bounded stable segment keys");
        double y=0;check(deck_height(s,s.a.x,s.a.z,y)&&near(y,s.a.y),"deck starts at exact authored height");
        check(deck_height(s,s.b.x,s.b.z,y)&&near(y,s.b.y),"deck ends at exact authored height");
        // Interior barycentric samples hit precisely the geometry triangle,
        // including the nonplanar corner/slope case, rather than a second plane.
        for(unsigned triangle=0;triangle<2;++triangle){
            Point a=triangle?s.right_a:s.left_a,b=s.left_b,c=triangle?s.right_b:s.right_a;
            Point p={a.x*.2+b.x*.3+c.x*.5,a.y*.2+b.y*.3+c.y*.5,a.z*.2+b.z*.3+c.z*.5};
            check(deck_height(s,p.x,p.z,y)&&near(y,p.y),"floor matches rendered triangle barycentrics");
        }
        if(i){const auto &prev=g.segments[i-1];check(same(prev.b,s.a)&&same(prev.left_b,s.left_a)&&same(prev.right_b,s.right_a),"shared center and edge vertices have no cracks");}
    }
    double y=99;check(!deck_height(g.segments.front(),30,3,y)&&y==99,"outside bridge is not a floor");
    const auto first_count=g.segments.size();for(unsigned i=0;i<50;++i){check(build(d,g,e)&&g.segments.size()==first_count,"regeneration replaces owned output without duplication");}
    d.points.back().x=7;check(build(d,g,e)&&g.owner_id==1&&same(g.segments.back().b,d.points.back()),"editing preserves owner and moves final geometry");
    d.points.pop_back();check(build(d,g,e)&&g.segments.size()<first_count,"removing a control point removes old pieces");

    auto wall=fixture(Kind::Wall);wall.points={{0,0,0},{0,1,10}};wall.alias="";
    check(build(wall,g,e,2.7)&&g.segments.size()==4,"measured module length fills span exactly");
    check(near(distance(g.segments[0].left_a,g.segments[0].right_a),wall.thickness),"wall uses thickness independently of bridge width");
    check(same(g.segments.front().a,wall.points.front())&&same(g.segments.back().b,wall.points.back()),"wall module end caps meet exact endpoints");
    check(g.segments[0].pitch<0&&near(g.segments[0].yaw,0),"ascending +Z transform pitch convention");
    wall.smooth=true;wall.points={{0,0,0},{0,1,20},{20,2,40},{40,2,40}};
    check(build(wall,g,e),"3D Catmull Rom wall builds");
    check(same(g.segments.front().a,wall.points.front())&&same(g.segments.back().b,wall.points.back()),"smooth endpoints exact");
    for(Point p:wall.points){bool found=false;for(const auto &s:g.segments)found=found||same(s.a,p)||same(s.b,p);check(found,"smooth path interpolates each authored control point");}
    bool curved=false;for(const auto &s:g.segments)curved=curved||s.a.x<-.01;check(curved,"smooth mode actually curves");

    auto invalid=fixture();invalid.points[1]=invalid.points[0];invalid.smooth=true;
    check(!build(invalid,g,e)&&g.segments.empty(),"duplicate control points reject atomically");
    invalid=fixture();invalid.points={{0,0,0},{0,10,1}};check(!build(invalid,g,e),"unwalkable bridge grade rejected");
    invalid=fixture();invalid.points={{0,0,0},{0,0,4},{0,0,1}};check(!build(invalid,g,e),"near reversing turn rejects bounded miter");
    invalid=fixture();invalid.points={{0,0,0},{0,0,20000}};check(!build(invalid,g,e),"segment budget rejects oversized path");
    invalid=fixture();invalid.points[0].x=std::numeric_limits<double>::quiet_NaN();check(!build(invalid,g,e),"nonfinite authored point rejected");
    invalid=fixture();invalid.points={{-50400,0,0},{-50400,0,10}};check(!build(invalid,g,e),"deck width cannot spill outside world bounds");
    invalid=fixture();invalid.smooth=true;invalid.points={{50399,0,0},{50400,0,10},{50400,0,20}};check(!build(invalid,g,e),"curve overshoot or deck boundary rejected");
    invalid=fixture();invalid.id=0;check(!build(invalid,g,e),"unowned geometry rejected");
    invalid=fixture();invalid.points.resize(point_limit+1);check(!build(invalid,g,e),"authored point cap enforced");
    invalid=fixture();invalid.spacing=.1;check(!build(invalid,g,e),"tiny spacing rejected");
    invalid=fixture();invalid.support_depth=65;check(!build(invalid,g,e),"support reach bounded");
    invalid=fixture();invalid.width=24;invalid.points={{0,0,0},{0,0,1},{1,0,1}};check(!build(invalid,g,e),"inverted wide deck corner rejected");

    Point p=snap_grid({1.4,3.3,-2.6},1);check(same(p,{1,3.3,-3}),"grid snap preserves elevation by default");
    check(same(snap_grid({1.4,3.3,-2.6},1,true),{1,3,-3}),"3D grid snap available");
    check(same(snap_grid({50400,0,0},7),{50400,0,0}),"out of bounds grid result rejected");
    auto snap=snap_point({.1,3.1,.1},{fixture()},.3);check(snap.snapped&&snap.owner_id==1&&snap.point_index==0&&same(snap.point,{0,3,0}),"3D point snapping exact ownership");
    check(!snap_point({.1,30,.1},{fixture()},.3).snapped,"point snap respects storey elevation");
    check(!snap_point({.1,3.1,.1},{fixture()},.3,1).snapped,"point snap excludes active owner");

    world_studio::Document doc;const auto legacy=bytes(doc);check(legacy.rfind("VALWOOD_STUDIO 6\n",0)==0,"unmodified legacy scene keeps version6 bytes");
    Id issued=0;auto added=fixture();added.smooth=true;added.follow_ground=false;
    check(append(doc.structures,doc.next_structure_id,added,issued,e)&&issued==1&&doc.next_structure_id==2,"valid append issues stable identity");
    auto bad=added;bad.width=-1;check(!append(doc.structures,doc.next_structure_id,bad,issued,e)&&issued==0&&doc.next_structure_id==2&&doc.structures.size()==1,"invalid append has no partial mutation");
    auto configured=fixture(Kind::Wall);configured.follow_ground=true;configured.railings=false;configured.supports=false;configured.alias="reserved_module";configured.height=4.5;configured.thickness=.5;configured.support_depth=7;
    check(append(doc.structures,doc.next_structure_id,configured,issued,e)&&issued==2,"second independently owned configured wall");
    world_studio::Placement prop;prop.alias="fixture";prop.wind_enabled=false;prop.wind_sway=.5f;doc.placements.push_back(prop);
    world_studio::Actor actor;actor.name="Upstairs";actor.points={{{1,2}},{{6,2}}};actor.floor_heights={104,106};doc.actors.push_back(actor);
    world_studio::NavigationArea navigation;navigation.x=2;navigation.z=2;navigation.source_floor=true;navigation.floor_y=104;doc.navigation.push_back(navigation);doc.actors[0].navigation=0;
    const auto encoded=bytes(doc);check(encoded.rfind("VALWOOD_STUDIO 12\n",0)==0&&encoded.find("structure_ids 3\n")!=std::string::npos,"structure serialization uses explicit version12 high water");
    char directory[]="/tmp/valwood-structure-spline-XXXXXX";check(mkdtemp(directory)!=nullptr,"private CPU fixture directory");const std::string path=std::string(directory)+"/world.studio";
    check(world_studio::save(path.c_str(),doc,e),"save structure scene");world_studio::Document loaded;
    check(world_studio::load(path.c_str(),loaded,e)&&bytes(loaded)==encoded,"save reopen preserves exact definitions and generated ownership");
    check(loaded.structures[0].smooth&&!loaded.structures[0].follow_ground&&loaded.placements.size()==1,"new modes persist separately from placements");
    check(loaded.structures[1].follow_ground&&!loaded.structures[1].railings&&!loaded.structures[1].supports&&loaded.structures[1].alias==configured.alias&&near(loaded.structures[1].height,4.5)&&near(loaded.structures[1].support_depth,7),"all configured wall options reopen");
    check(loaded.actors[0].floor_heights==actor.floor_heights&&loaded.navigation[0].source_floor&&!loaded.placements[0].wind_enabled,"version12 retains version10 wind and version11 actor storeys");
    auto rejects=[&](std::string text,const char *label){std::ofstream f(path);f<<text;f.close();const auto before=bytes(loaded);check(!world_studio::load(path.c_str(),loaded,e)&&bytes(loaded)==before,label);};
    auto malformed=encoded;auto at=malformed.find("structure_ids 3\n");malformed.erase(at,std::string("structure_ids 3\n").size());rejects(malformed,"missing identity metadata rejects without replacing live scene");
    malformed=encoded;malformed.replace(malformed.find("structure_ids 3"),15,"structure_ids 1");rejects(malformed,"reused structure high water rejected");
    malformed=encoded;malformed.replace(malformed.find("structure_ids 3"),15,"structure_ids -2");rejects(malformed,"negative unsigned identity rejected");
    malformed=encoded;malformed.replace(malformed.find("VALWOOD_STUDIO 12"),16,"VALWOOD_STUDIO 11");rejects(malformed,"older version cannot silently accept new records");
    auto duplicate=doc;duplicate.structures.push_back(duplicate.structures[0]);check(!world_studio::valid(duplicate,e),"duplicate owners rejected");
    auto cap=doc;while(cap.structures.size()<definition_limit){check(append(cap.structures,cap.next_structure_id,added,issued,e),"bounded owner append");}check(!append(cap.structures,cap.next_structure_id,added,issued,e)&&cap.structures.size()==definition_limit,"17th structure rejected atomically");
    auto exhausted=world_studio::Document{};exhausted.next_structure_id=id_limit+1;check(!append(exhausted.structures,exhausted.next_structure_id,added,issued,e),"exhausted IDs never wrap");

    world_studio::History history;world_studio::Document live;history.checkpoint(live);check(append(live.structures,live.next_structure_id,added,issued,e),"history fixture append");
    check(history.undo(live)&&live.structures.empty()&&live.next_structure_id==2,"Undo restores empty structure set without reissuing owner IDs");
    check(bytes(live).rfind("VALWOOD_STUDIO 12\n",0)==0,"high water alone persists version12");
    check(history.redo(live)&&live.structures.size()==1&&live.structures[0].id==1,"Redo restores same owner");
    check(history.undo(live),"Undo prior to new branch");history.checkpoint(live);check(append(live.structures,live.next_structure_id,added,issued,e)&&issued==2&&history.forward.empty(),"new history branch uses fresh stable ID");
    std::remove(path.c_str());rmdir(directory);
    std::printf("world_structure_splines: %u CPU checks passed\n",checks);
}
