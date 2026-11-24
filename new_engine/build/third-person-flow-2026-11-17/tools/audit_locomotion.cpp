#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_skeletal_hero.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace world_animation;
namespace {
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 mul(Vec3 a,float f){return {a.x*f,a.y*f,a.z*f};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float norm(Vec3 a){return std::sqrt(dot(a,a));}
unsigned node(const Asset&a,const std::string&name){for(unsigned n=0;n<a.skeleton.joints.size();++n)if(a.skeleton.joints[n].name==name)return n;throw std::runtime_error("Missing "+name);}
Vec3 origin(const Pose&p,unsigned n){const auto&m=p.global[n].m;return {m[12],m[13],m[14]};}
Vec3 vertex(const Asset&a,const Pose&p,unsigned n){Vec3 v,normal;std::string e;if(!deform_vertex(a.mesh.vertices[n],p,v,normal,e))throw std::runtime_error(e);return transform_point(p.global[a.skeleton.mesh_node],v);}
struct Foot {unsigned hip,knee,ankle,ball,heel,toe;std::vector<unsigned> vertices;};
Foot make_foot(const Asset&a,const Pose&rest,const std::string&side){
 Foot f{node(a,"thigh_"+side),node(a,"calf_"+side),node(a,"foot_"+side),node(a,"ball_"+side),0,0,{}};
 float bottom=100;
 for(unsigned n=0;n<a.mesh.vertices.size();++n){const auto&v=a.mesh.vertices[n];float total=0;for(unsigned k=0;k<4;++k){const auto j=a.skeleton.palette[v.joints[k]];if(j==f.ankle||j==f.ball)total+=v.weights[k];}if(total>.95f){f.vertices.push_back(n);bottom=std::min(bottom,vertex(a,rest,n).y);}}
 if(f.vertices.empty())throw std::runtime_error("No foot-weighted boot geometry");
 Vec3 forward=sub(origin(rest,f.ball),origin(rest,f.ankle));forward.y=0;forward=mul(forward,1/norm(forward));
 float back=100,front=-100;std::vector<unsigned> sole;
 for(unsigned n:f.vertices){const auto p=vertex(a,rest,n);if(p.y<bottom+.015f){sole.push_back(n);const float d=dot(p,forward);back=std::min(back,d);front=std::max(front,d);}}
 if(sole.empty())throw std::runtime_error("No sole band");
 auto representative=[&](bool toe){Vec3 center;unsigned count=0;std::vector<unsigned> region;for(unsigned n:sole){auto p=vertex(a,rest,n);float fwd=dot(p,forward);if(toe?fwd>front-(front-back)*.25f:fwd<back+(front-back)*.25f){center=add(center,p);++count;region.push_back(n);}}if(!count)throw std::runtime_error("Empty sole region");center=mul(center,1.f/count);unsigned best=region[0];float distance=100;for(unsigned n:region){const float d=norm(sub(vertex(a,rest,n),center));if(d<distance){distance=d;best=n;}}return best;};
 f.heel=representative(false);f.toe=representative(true);return f;
}
double ease(double t){t=std::clamp(t,0.,1.);return t*t*(3-2*t);}
struct Motion {double vx=0,vz=0,yaw=0;const char*phase="idle";};
Motion script(const std::string&s,double t){Motion m;
 if(s=="walk_smooth"||s=="run_smooth"){const bool run=s=="run_smooth";const double speed=run?5:1.65,start=run?.5:.3,stop=run?.35:.3;if(t<1){}else if(t<1+start){m.vz=speed*ease((t-1)/start);m.phase="accelerate";}else if(t<4){m.vz=speed;m.phase="steady";}else if(t<4+stop){m.vz=speed*(1-ease((t-4)/stop));m.phase="decelerate";}else m.phase="settle";
 }else if(s=="walk_abrupt"||s=="run_abrupt"){if(t>=1&&t<4){m.vz=s=="run_abrupt"?5:1.65;m.phase="steady";}else if(t>=4)m.phase="blocked_stop";
 }else if(s=="walk_reverse"){if(t>=1&&t<3){m.vz=1.65;m.phase="forward";}else if(t>=3&&t<5){m.vz=-1.65;m.phase="reverse";}else if(t>=5)m.phase="settle";
 }else if(s=="walk_strafe"){if(t>=1&&t<3){m.vz=1.65;m.phase="forward";}else if(t>=3&&t<5){m.vx=-1.65;m.phase="left";}else if(t>=5&&t<7){m.vx=1.65;m.phase="right";}else if(t>=7)m.phase="settle";
 }else if(s=="walk_steering"){m.yaw=std::clamp((t-3)/1.2,0.,1.)*1.5707963267948966;if(t>=1&&t<6){m.vx=std::sin(m.yaw)*1.65;m.vz=std::cos(m.yaw)*1.65;m.phase=t<3?"forward":t<4.2?"steer90":"turned";}else if(t>=6)m.phase="settle";
 }else if(s=="idle_turn"){m.yaw=std::clamp((t-1)/.6,0.,1.)*1.5707963267948966;m.phase=t<1?"idle":t<1.6?"turn90":"settle";
 }else if(s=="walk_run_change"){if(t>=1&&t<3){m.vz=1.65;m.phase="walk";}else if(t>=3&&t<5){m.vz=5;m.phase="run";}else if(t>=5&&t<7){m.vz=1.65;m.phase="walk_after_run";}else if(t>=7)m.phase="settle";
 }else throw std::runtime_error("Unknown scenario");return m;
}
void emit_point(std::ostream&o,Vec3 p,const WorldSkeletalHero::Input&i){o<<','<<i.x+std::cos(i.yaw)*p.x+std::sin(i.yaw)*p.z<<','<<i.y+p.y<<','<<i.z-std::sin(i.yaw)*p.x+std::cos(i.yaw)*p.z;}
void emit_pose(std::ostream&o,const Asset&a,const Pose&p,const std::array<Foot,2>&feet,const WorldSkeletalHero::Input&i){
 for(const auto&f:feet){for(unsigned j:{f.hip,f.knee,f.ankle,f.ball})emit_point(o,origin(p,j),i);emit_point(o,vertex(a,p,f.heel),i);emit_point(o,vertex(a,p,f.toe),i);float bottom=100;for(unsigned v:f.vertices)bottom=std::min(bottom,vertex(a,p,v).y);o<<','<<i.y+bottom;const auto h=origin(p,f.hip),k=origin(p,f.knee),ank=origin(p,f.ankle);o<<','<<norm(sub(k,h))<<','<<norm(sub(ank,k));}
}
void header(std::ostream&o){o<<"scenario,frame,time,phase,x,z,yaw,speed,gait_cycles,locomotion,run";for(const char*s:{"l","r"}){for(const char*p:{"hip","knee","ankle","ball","heel","toe"})for(const char*c:{"x","y","z"})o<<','<<p<<'_'<<s<<'_'<<c;o<<",min_boot_"<<s<<"_y,upper_"<<s<<"_length,lower_"<<s<<"_length";}o<<'\n';}
}
int main(int argc,char**argv){try{
 if(argc<3||argc>5)throw std::runtime_error("Usage: audit_support_feet frozenV3.glb output_dir");
 const unsigned fps=argc==5?unsigned(std::stoul(argv[4])):60;if(fps!=30&&fps!=60&&fps!=120)throw std::runtime_error("Unsupported QA rate");
 WorldSkeletalHero reference;std::string e;if(!reference.load_cpu(argv[1],e,argc>=4?argv[3]:nullptr))throw std::runtime_error(e);const auto&a=reference.asset();Pose rest;if(!rest_pose(a.skeleton,rest,e))throw std::runtime_error(e);std::array<Foot,2>feet={make_foot(a,rest,"l"),make_foot(a,rest,"r")};
 std::filesystem::create_directories(argv[2]);std::ofstream meta(std::filesystem::path(argv[2])/"sole-selection.json");meta<<std::setprecision(10)<<"{\"method\":\"Actual boot vertices with>95% foot/ball weights; neutral rest bottom15mm band; representative heel/toe from end25% bands\",\"feet\":[";
 for(unsigned s=0;s<2;++s){const auto&f=feet[s];if(s)meta<<',';meta<<"{\"side\":\""<<(s?"r":"l")<<"\",\"vertices\":"<<f.vertices.size()<<",\"heel_vertex\":"<<f.heel<<",\"toe_vertex\":"<<f.toe<<",\"upper_length\":"<<norm(sub(origin(rest,f.hip),origin(rest,f.knee)))<<",\"lower_length\":"<<norm(sub(origin(rest,f.knee),origin(rest,f.ankle)))<<",\"all_chain_positive_uniform_scales\":";bool uniform=true;for(int n=int(f.ankle);n>=0;n=a.skeleton.joints[n].parent){auto scale=rest.local[n].scale;uniform&=scale.x>0&&std::abs(scale.x-scale.y)<.0001&&std::abs(scale.x-scale.z)<.0001;}meta<<(uniform?"true":"false")<<'}';}meta<<"]}\n";
 std::ofstream out(std::filesystem::path(argv[2])/"controller-feet.csv");out<<std::setprecision(10);header(out);
 for(const std::string scenario:{"walk_smooth","run_smooth","walk_abrupt","run_abrupt","walk_reverse","walk_strafe","walk_steering","idle_turn","walk_run_change"}){
  WorldSkeletalHero hero;if(!hero.load_cpu(argv[1],e,argc>=4?argv[3]:nullptr))throw std::runtime_error(e);WorldTrainingCombat combat;WorldSkeletalHero::Input input;input.dt=1./fps;
  const unsigned frames=(scenario=="walk_strafe"||scenario=="walk_run_change"?9:scenario=="walk_steering"?8:6)*fps;
  for(unsigned n=0;n<=frames;++n){const double t=double(n)/fps;const auto m=script(scenario,t);input.reset=n==0;input.dx=m.vx*input.dt;input.dz=m.vz*input.dt;input.x+=input.dx;input.z+=input.dz;input.yaw=float(m.yaw);if(!hero.update_cpu(input,combat,e))throw std::runtime_error(e);const auto&stats=hero.stats();out<<scenario<<','<<n<<','<<t<<','<<m.phase<<','<<input.x<<','<<input.z<<','<<input.yaw<<','<<std::hypot(m.vx,m.vz)<<','<<stats.gait_cycles<<','<<stats.locomotion<<','<<stats.run;emit_pose(out,hero.asset(),hero.pose(),feet,input);out<<'\n';}
 }
 std::ofstream source(std::filesystem::path(argv[2])/"source-feet.csv");source<<std::setprecision(10);header(source);
 for(const auto&clip:a.clips)if(clip.name=="Idle"||clip.name=="Run"||clip.name.rfind("Walk_",0)==0){const unsigned frames=unsigned(std::ceil(clip.duration*240));for(unsigned n=0;n<=frames;++n){const double t=std::min(clip.duration,n/240.);Pose p;if(!sample(a.skeleton,clip,t,false,p,e))throw std::runtime_error(e);WorldSkeletalHero::Input i;source<<clip.name<<','<<n<<','<<t<<",source,0,0,0,0,"<<t/clip.duration<<",1,"<<(clip.name=="Run"?1:0);emit_pose(source,a,p,feet,i);source<<'\n';}}
 std::cout<<"Audit traces written; accepted asset/source unchanged.\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
