#include "engine/world_skeletal_hero.h"
#include "engine/world_knight_appearance.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
using namespace world_animation;
namespace world_animation_render {Renderer*create(const Asset&,unsigned,std::string&,std::uint64_t){std::abort();}void destroy(Renderer*p){if(p)std::abort();}bool warm(Renderer*,unsigned,std::string&){std::abort();}bool upload(Renderer*,const Pose*,const Instance*,std::size_t,const Bindings&,std::string&){std::abort();}bool draw(Renderer*,unsigned,const Bindings&,std::string&){std::abort();}}
static void require(bool ok,const std::string&e){if(!ok){std::cerr<<e<<'\n';std::exit(2);}}
static Vec3 point(const Pose&p,unsigned j){auto&m=p.global[j].m;return{m[12],m[13],m[14]};}
static bool ground(double,double,double,WorldSkeletalHero::Support&s,void*){s.height=0;return true;}
int main(int argc,char**argv){require(argc==5,"civilian.cfg armored.cfg rigdir output-palette.bin");std::ofstream binary(argv[4],std::ios::binary);require(bool(binary),"binary output");std::cout<<std::setprecision(10);const char*names[]={"sword","sword_shield","great_axe","staff"};const char*prefix[]={nullptr,"Shield_","Axe_","Staff_"};
 for(unsigned outfit=0;outfit<2;++outfit){world_knight_appearance::Config cfg;Asset body;std::string e;require(world_knight_appearance::load(argv[outfit+1],cfg,body,e),e);
 for(unsigned weapon=0;weapon<4;++weapon){auto owner=std::make_unique<WorldSkeletalHero>();auto&h=*owner;require(h.load_cpu((std::string(argv[3])+"/"+names[weapon]+".glb").c_str(),e,prefix[weapon]),e);require(h.select_appearance(body,cfg.first_person,e),e);const auto&s=h.asset().skeleton;auto id=[&](const char*n){for(unsigned j=0;j<s.joints.size();++j)if(s.joints[j].name==n)return j;std::abort();};unsigned neck=id("neck_01"),head=id("head");WorldSkeletalHero::Input in;in.dt=1./60;in.reset=true;in.ground_query=ground;in.equipment_stowed=true;WorldTrainingCombat fight;unsigned frame=0;double clock=0;unsigned armed_idle=0,unarmed_idle=0;for(unsigned c=0;c<h.asset().clips.size();++c){if(h.asset().clips[c].name==std::string(prefix[weapon]?prefix[weapon]:"")+"Idle")armed_idle=c;if(h.asset().clips[c].name=="Story_UnarmedIdle")unarmed_idle=c;}
 auto step=[&](){double dt=in.paused||fight.impact_pause>0?0:in.dt;in.x+=in.dx;in.z+=in.dz;fight.clock+=dt;if(in.reset)clock=0;clock+=dt;require(h.update_cpu(in,fight,e),e);in.reset=false;++frame;};
 auto emit=[&](const char*phase){const auto&p=h.pose();auto offset=binary.tellp();for(unsigned j=0;j<p.nodes;++j)binary.write(reinterpret_cast<const char*>(p.global[j].m.data()),16*sizeof(float));std::vector<world_combat_anatomy::Capsule> capsules;require(h.body_capsules(in.x,in.y,in.z,in.yaw,capsules,e),e);double error=1e9;const auto hp=point(p,head);for(const auto&c:capsules)if(c.part==TrainingBodyPart::Head){const double x=in.x+std::cos(in.yaw)*hp.x+std::sin(in.yaw)*hp.z,y=in.y+hp.y,z=in.z-std::sin(in.yaw)*hp.x+std::cos(in.yaw)*hp.z;error=std::sqrt(std::pow(c.a.x-x,2)+std::pow(c.a.y-y,2)+std::pow(c.a.z-z,2));}
 Pose target;require(sample(s,h.asset().clips[in.equipment_stowed?unarmed_idle:armed_idle],clock,true,target,e),e);std::cout<<"{\"target_head_global\":[";for(unsigned k=0;k<16;++k){if(k)std::cout<<',';std::cout<<target.global[head].m[k];}std::cout<<"],\"equipment_progress\":"<<h.stats().equipment_progress<<",\"equipment_transition\":"<<(h.stats().equipment_transition?"true":"false")<<",\"case\":"<<outfit*4+weapon<<",\"phase\":"<<std::quoted(phase)<<",\"frame\":"<<frame<<",\"nodes\":"<<p.nodes<<",\"offset\":"<<static_cast<long long>(offset)<<",\"neck\":"<<neck<<",\"head\":"<<head<<",\"capsule_head_error_m\":"<<error<<",\"local_translation_scale\":[";for(unsigned j:{neck,head}){if(j==head)std::cout<<',';auto&t=p.local[j];std::cout<<'['<<t.translation.x<<','<<t.translation.y<<','<<t.translation.z<<','<<t.scale.x<<','<<t.scale.y<<','<<t.scale.z<<']';}std::cout<<"]}\n";};
 for(unsigned k=0;k<180;++k)step();for(unsigned k=0;k<180;++k){step();emit("idle");}
 in.paused=true;for(unsigned k=0;k<20;++k){step();emit("pause");}in.paused=false;
 fight.impact_pause=.05;for(unsigned k=0;k<20;++k){step();emit("hitstop");}fight.impact_pause=0;
 in.equipment_stowed=false;for(unsigned k=0;k<36;++k){step();emit("draw-bridge");}for(unsigned k=0;k<90;++k)step();for(unsigned k=0;k<30;++k){step();emit("armed");}
 in.equipment_stowed=true;for(unsigned k=0;k<36;++k){step();emit("stow-bridge");}for(unsigned k=0;k<90;++k)step();
 in.dz=1.6*in.dt;for(unsigned k=0;k<600;++k)step();for(unsigned k=0;k<60;++k){step();emit("moving");}in.dz=0;for(unsigned k=0;k<180;++k)step();
 in.swimming=true;for(unsigned k=0;k<60;++k)step();for(unsigned k=0;k<30;++k){step();emit("swim");}in.submerged=true;for(unsigned k=0;k<60;++k)step();for(unsigned k=0;k<30;++k){step();emit("dive");}
 in.swimming=in.submerged=false;in.dead=true;for(unsigned k=0;k<240;++k)step();for(unsigned k=0;k<30;++k){step();emit("death");}
 in.dead=false;in.reset=true;step();for(unsigned k=0;k<180;++k)step();for(unsigned k=0;k<30;++k){step();emit("retry-idle");}
 }}require(bool(binary),"binary flush");}
