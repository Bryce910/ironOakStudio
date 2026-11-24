#include "engine/world_structure_runtime.h"
#include "engine/world_playtest_walk.h"
#include "engine/world_npc_source_navigation.h"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
void check(bool ok,const char *label){++checks;if(!ok)throw std::runtime_error(label);}
double ground(double x,double z,const void *){return 1.+x*.005+z*.002;}
double flat(double,double,const void *){return 0.;}
double invalid(double,double,const void *){return NAN;}
world_structure::Definition bridge(){world_structure::Definition d;d.id=1;d.kind=world_structure::Kind::Bridge;d.points={{0,8,0},{0,10,30}};d.width=4;d.spacing=3;d.support_depth=16;return d;}
bool configure(const world_structure_runtime::Prepared &p,world_playtest_walk::Support &support,std::string &error){
    std::vector<world_physics::Document> docs;for(const auto &m:p.meshes)docs.push_back(m.collision);
    const auto &b=p.bounds;return support.configure(std::move(docs),{b[0]-1,b[1]+1,b[4]-1,b[5]+1,b[2]-1,b[3]+1},{},error,false);
}
}
int main(){try{
    using namespace world_structure_runtime;std::string error;Prepared p;auto d=bridge();
    check(prepare({d},flat,nullptr,p,error),"sloping timber bridge prepares");
    check(p.bridges==1&&p.walls==0&&p.meshes[0].supports>0&&p.meshes[0].posts>2,"real deck, rails, posts and supports generated");
    check(p.vertices==p.triangles*3,"one render vertex per physical triangle corner");
    const auto &m=p.meshes[0];const auto &body=m.collision.bodies[0];
    check(world_physics::validate(m.collision,error),"actual Jolt codec validates generated closed mesh");
    for(unsigned i=0;i<m.vertices.size();i++){const auto &v=m.vertices[i];const auto &q=body.vertices[i];check(q.x==v.x&&q.y==v.y&&q.z==v.z,"GL and collision positions byte-identical after quantization");}
    Prepared again;check(prepare({d},flat,nullptr,again,error)&&again.fingerprint==p.fingerprint,"deterministic regeneration does not accumulate geometry");
    auto saved=p.fingerprint;auto bad=d;bad.support_depth=1;check(!prepare({bad},flat,nullptr,p,error)&&p.fingerprint==saved,"unsafe support reach fails atomically");
    check(!prepare({d},invalid,nullptr,p,error)&&p.fingerprint==saved,"invalid terrain fails atomically");
    check(!prepare({d,d},flat,nullptr,p,error),"duplicate durable owner rejected");
    world_playtest_walk::Support support;check(configure(p,support,error),"actual Jolt static-query scene configures");
    for(unsigned i=1;i<120;++i){const double z=i*.25,y=8+z/15.;float actual=0;double rendered=0;check(floor_near(p,0,z,y,rendered)&&support.sample(0,z,y,actual)&&std::abs(actual-rendered)<.002&&std::abs(actual-y)<.002,"sloping deck Jolt floor matches rendered surface");}
    float floor=0;check(!support.sample(0,15,0,floor),"bridge overhead is not an invisible floor below");
    double probe=0;check(!floor_near(p,0,15,0,probe),"CPU triangle storey filter rejects overhead deck");
    for(double step:{.10,.36,.72}){double z=.4,feet=8+z/15.;while(z+step<29.5){float next=0;check(support.can_move(0,z,0,z+step,feet,next),"walk/run movement traverses sloping deck");check(std::abs(next-(8+(z+step)/15.))<.005,"walk/run feet follow exact deck grade");z+=step;feet=next;}}
    check(!support.can_move(0,15,3,15,9,floor),"physical railing blocks walking off deck");
    world_navigation::Point npc{0,9,15},below{0,0,15};world_navigation::Query query;
    check(world_npc_source_navigation::endpoint(support,npc),"production NPC floor callback resolves bridge deck");
    check(!world_npc_source_navigation::endpoint(support,below),"production NPC floor callback rejects overhead bridge");
    check(world_npc_source_navigation::line(support,query,{0,8.1,1.5},{0,9.9,28.5},.37f),"production NPC route crosses sloping bridge");
    check(!world_npc_source_navigation::line(support,query,{0,9,15},{3,9,15},.37f),"production NPC route respects bridge rail collision");
    world_physics::Hit hit;check(support.cast_capsule_movement({0,7.1+.9,15},.37f,.50f,{0,2,0},hit)&&hit.hit&&hit.normal.y<-.5,"solid underside blocks jump from below");
    auto bare=d;bare.id=2;bare.railings=bare.supports=false;Prepared barep;check(prepare({bare},nullptr,nullptr,barep,error),"unsupported no-rail span explicitly allowed");check(barep.meshes[0].posts==0&&barep.meshes[0].supports==0,"disabled geometry really absent");
    check(prepare({d},flat,nullptr,p,error),"restore original exact bridge after rejected edits");
    auto curved=d;curved.points={{10,6,0},{10,7,12},{20,8,24}};curved.smooth=true;Prepared cp;check(prepare({curved},flat,nullptr,cp,error),"smooth 3D corner bridge prepares");world_playtest_walk::Support cs;check(configure(cp,cs,error),"smooth bridge actual collision configures");world_structure::Generated curve;check(world_structure::build(curved,curve,error),"curve independently generated");for(const auto &s:curve.segments){const auto center=world_structure::mix(s.a,s.b,.5);float y=0;double expected=0;check(world_structure::deck_height(s,center.x,center.z,expected),"curved top triangle contains centre");const bool got=cs.sample(center.x,center.z,expected,y);if(!got||std::abs(y-expected)>=.003)std::cerr<<"curve sample "<<s.index<<" got="<<got<<" y="<<y<<" expected="<<expected<<" at="<<center.x<<","<<center.z<<"\n";check(got&&std::abs(y-expected)<.003,"shared curved deck has no floor cracks");}
    world_structure::Point previous=curve.segments.front().a;float walking_y=float(previous.y);for(const auto &s:curve.segments){for(unsigned k=1;k<=8;++k){auto point=world_structure::mix(s.a,s.b,double(k)/8);double expected=0;check(world_structure::deck_height(s,point.x,point.z,expected),"curved walking target has exact render floor");float next=0;check(cs.can_move(previous.x,previous.z,point.x,point.z,walking_y,next)&&std::abs(next-expected)<.008,"actual walking crosses curved segment and miter seams");previous=point;walking_y=next;}}
    auto moved=d;for(auto &v:moved.points){v.x-=18000;v.z-=12000;v.y+=2;}Prepared far;check(prepare({moved},flat,nullptr,far,error),"arbitrary world bridge position prepares");world_playtest_walk::Support fs;check(configure(far,fs,error)&&fs.sample(-18000,-11985,11,floor)&&std::abs(floor-11)<.003,"walkable deck follows translated elevated bridge");check(!fs.sample(0,15,9,floor),"moving bridge leaves no invisible floor at old location");
    world_structure::Definition wall;wall.id=3;wall.kind=world_structure::Kind::Wall;wall.follow_ground=true;wall.height=3;wall.thickness=.5;wall.points={{100,50,0},{100,50,24}};Prepared wp;check(prepare({wall},ground,nullptr,wp,error),"stone wall follows actual terrain instead of stale control-point height");world_playtest_walk::Support ws;check(configure(wp,ws,error),"stone wall Jolt scene configures");const double bed=ground(100,12,nullptr);check(!ws.can_move(98,12,102,12,bed,floor),"wall is a physical movement blocker");check(!ws.sample(98,12,bed,floor),"wall collision does not add invisible terrain around wall");
    std::cout<<"world_structure_runtime: PASS "<<checks<<" checks; exact render/Jolt triangles, walk/run slopes, rails, underside, curved and moved bridges, ground-following walls, atomic failures\n";return 0;
}catch(const std::exception &e){std::cerr<<"world_structure_runtime: FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
