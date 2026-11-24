// Actual public controller + actual skinned boot geometry, no renderer/GL.
#include "engine/world_skeletal_hero.h"
#include "engine/world_knight_appearance.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
using namespace world_animation;
namespace world_animation_render {Renderer*create(const Asset&,unsigned,std::string&,std::uint64_t){std::abort();}void destroy(Renderer*p){if(p)std::abort();}bool warm(Renderer*,unsigned,std::string&){std::abort();}bool upload(Renderer*,const Pose*,const Instance*,std::size_t,const Bindings&,std::string&){std::abort();}bool draw(Renderer*,unsigned,const Bindings&,std::string&){std::abort();}}
static Vec3 P(const Pose&p,unsigned j){const auto&m=p.global[j].m;return{m[12],m[13],m[14]};}
static Vec3 sub(Vec3 a,Vec3 b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
static double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static double length(Vec3 a){return std::sqrt(dot(a,a));}
static double angle(Vec3 a,Vec3 b){return std::acos(std::clamp(dot(a,b)/length(a)/length(b),-1.,1.))*180/3.141592653589793;}
struct Ground{double sx=0,sz=0;};
static bool ground(double x,double,double z,WorldSkeletalHero::Support&s,void*c){auto&g=*static_cast<Ground*>(c);s.height=g.sx*x+g.sz*z;double l=std::sqrt(1+g.sx*g.sx+g.sz*g.sz);s.nx=float(-g.sx/l);s.ny=float(1/l);s.nz=float(-g.sz/l);return true;}
static void require(bool ok,const std::string&e){if(!ok){std::cerr<<e<<'\n';std::exit(2);}}
int main(int argc,char**argv){require(argc==4,"Usage: probe civilian.cfg armored.cfg rig-directory");std::cout<<std::setprecision(10);const char*names[]={"sword","sword_shield","great_axe","staff"};const char*prefix[]={nullptr,"Shield_","Axe_","Staff_"};
 for(unsigned outfit=0;outfit<2;++outfit){world_knight_appearance::Config cfg;Asset body;std::string e;require(world_knight_appearance::load(argv[1+outfit],cfg,body,e),e);
 for(unsigned weapon=0;weapon<4;++weapon){auto owner=std::make_unique<WorldSkeletalHero>();auto&h=*owner;require(h.load_cpu((std::string(argv[3])+"/"+names[weapon]+".glb").c_str(),e,prefix[weapon]),e);require(h.select_appearance(body,cfg.first_person,e),e);const auto&s=h.asset().skeleton;
 auto id=[&](const char*n){for(unsigned j=0;j<s.joints.size();++j)if(s.joints[j].name==n)return j;std::abort();};unsigned idle=0;for(unsigned c=0;c<h.asset().clips.size();++c)if(h.asset().clips[c].name=="Story_UnarmedIdle")idle=c;
 const unsigned hips[]={id("thigh_l"),id("thigh_r")},knees[]={id("calf_l"),id("calf_r")},ankles[]={id("foot_l"),id("foot_r")};std::vector<unsigned>boots[2];
 for(unsigned v=0;v<h.asset().mesh.vertices.size();++v)for(unsigned side=0;side<2;++side){float weight=0;for(unsigned k=0;k<4;++k){auto n=s.joints[s.palette[h.asset().mesh.vertices[v].joints[k]]].name;if(n==(side?"foot_r":"foot_l")||n==(side?"ball_r":"ball_l"))weight+=h.asset().mesh.vertices[v].weights[k];}if(weight>.95f)boots[side].push_back(v);}
 require(boots[0].size()>10&&boots[1].size()>10,"Actual appearance foot geometry missing");Ground g;WorldSkeletalHero::Input in;in.dt=1./60;in.reset=true;in.ground_context=&g;in.ground_query=ground;WorldTrainingCombat fight;fight.weapon=weapon==0?world_weapon_loadout::Weapon::Sword:weapon==1?world_weapon_loadout::Weapon::SwordShield:weapon==2?world_weapon_loadout::Weapon::GreatAxe:world_weapon_loadout::Weapon::Staff;double clock=0;unsigned frame=0;
 auto step=[&](){in.x+=in.dx;in.z+=in.dz;in.y=g.sx*in.x+g.sz*in.z;fight.clock+=in.dt;if(in.reset)clock=0;clock+=in.dt;require(h.update_cpu(in,fight,e),e);in.reset=false;++frame;};
 auto emit=[&](const char*phase,bool palette=false){Pose raw;require(sample(s,h.asset().clips[idle],clock,true,raw,e),e);const auto&p=h.pose();std::cout<<"{\"case\":"<<outfit*4+weapon<<",\"outfit\":"<<outfit<<",\"weapon\":"<<std::quoted(names[weapon])<<",\"phase\":"<<std::quoted(phase)<<",\"frame\":"<<frame<<",\"knees\":[";
 for(unsigned side=0;side<2;++side){if(side)std::cout<<',';std::cout<<angle(sub(P(p,hips[side]),P(p,knees[side])),sub(P(p,ankles[side]),P(p,knees[side])));}std::cout<<"],\"raw_knees\":[";
 for(unsigned side=0;side<2;++side){if(side)std::cout<<',';std::cout<<angle(sub(P(raw,hips[side]),P(raw,knees[side])),sub(P(raw,ankles[side]),P(raw,knees[side])));}std::cout<<"],\"sole_min\":[";
 for(unsigned side=0;side<2;++side){double low=1e9;for(unsigned v:boots[side]){Vec3 q,n;require(deform_vertex(h.asset().mesh.vertices[v],p,q,n,e),e);q=transform_point(p.global[s.mesh_node],q);double c=std::cos(in.yaw),sn=std::sin(in.yaw),x=in.x+c*q.x+sn*q.z,z=in.z-sn*q.x+c*q.z;low=std::min(low,in.y+q.y-g.sx*x-g.sz*z);}if(side)std::cout<<',';std::cout<<low;}
 std::cout<<"],\"joint_points\":[";for(unsigned side=0;side<2;++side)for(unsigned j:{knees[side],ankles[side]}){auto q=P(p,j);if(side||j!=knees[side])std::cout<<',';std::cout<<'['<<q.x<<','<<q.y<<','<<q.z<<']';}std::cout<<"],\"raw_joint_points\":[";for(unsigned side=0;side<2;++side)for(unsigned j:{knees[side],ankles[side]}){auto q=P(raw,j);if(side||j!=knees[side])std::cout<<',';std::cout<<'['<<q.x<<','<<q.y<<','<<q.z<<']';}std::cout<<"],\"pelvis_y\":"<<P(p,id("pelvis")).y<<",\"head_relative_pelvis_z\":"<<P(p,id("head")).z-P(p,id("pelvis")).z<<",\"full_unarmed\":"<<(h.stats().full_unarmed?"true":"false")<<",\"settled\":"<<(h.full_unarmed_settled()?"true":"false")<<",\"weapon_visible\":"<<(h.equipment_weapon_visible()?"true":"false")<<",\"foot_reach_releases\":"<<h.stats().foot_reach_releases<<",\"full_unarmed_cycles\":"<<h.stats().full_unarmed_cycles;
 if(palette){std::cout<<",\"palette\":[";for(unsigned j=0;j<p.nodes;++j)for(unsigned k=0;k<16;++k){if(j||k)std::cout<<',';std::cout<<p.global[j].m[k];}std::cout<<']';}
 std::cout<<"}\n";
 };
 // Separate runs compiled from published and candidate implementations compare these identical armed controls.
 for(unsigned k=0;k<100;++k){step();if(k>=60&&k%5==0)emit("armed-idle",true);}
 for(unsigned direction=0;direction<5;++direction){fight.guard=true;fight.selected=TrainingDirection(direction);for(unsigned k=0;k<35;++k){step();if(k>=25&&k%5==0)emit("armed-guard",true);}double a[3],b[3];require(h.contact_blade(direction,.46,in.x,in.y,in.z,in.yaw,a,b,e),e);std::cout<<"{\"case\":"<<outfit*4+weapon<<",\"phase\":\"contact\",\"direction\":"<<direction<<",\"values\":["<<a[0]<<','<<a[1]<<','<<a[2]<<','<<b[0]<<','<<b[1]<<','<<b[2]<<"]}\n";}fight.guard=false;
 for(unsigned direction=0;direction<5;++direction){fight.attack_direction=TrainingDirection(direction);++fight.attack_start_serial;for(auto phase:{TrainingPlayerPhase::Windup,TrainingPlayerPhase::Strike,TrainingPlayerPhase::Recovery}){fight.player_phase=phase;fight.player_duration=phase==TrainingPlayerPhase::Windup?.18:phase==TrainingPlayerPhase::Strike?.09:.24;for(unsigned k=0;k<=12;++k){fight.player_time=fight.player_duration*k/12.;step();if(k%4==0)emit("armed-attack",true);}}fight.player_phase=TrainingPlayerPhase::Ready;fight.player_time=0;for(unsigned k=0;k<35;++k)step();}
 in.equipment_stowed=true;for(unsigned k=0;k<180;++k)step();for(unsigned k=0;k<180;++k){step();emit("fresh-stow-idle",k==0);}
 // Long enough for old idle anchors to release over multiple full walking cycles.
 in.dz=1.6*in.dt;for(unsigned k=0;k<600;++k)step();for(unsigned k=0;k<120;++k){step();emit("settled-moving",true);}in.dz=0;for(unsigned k=0;k<180;++k)step();for(unsigned k=0;k<180;++k){step();emit("walk-stop-idle");}
 in.equipment_stowed=false;for(unsigned k=0;k<90;++k)step();in.equipment_stowed=true;for(unsigned k=0;k<180;++k)step();for(unsigned k=0;k<180;++k){step();emit("restow-idle");}
 // Actual ground callback and true weighted boots; slope is allowed to change joint angles.
 g.sx=.04;g.sz=.055;in.reset=true;for(unsigned k=0;k<180;++k)step();for(unsigned k=0;k<180;++k){step();emit("slope-idle");}
 }}
}
