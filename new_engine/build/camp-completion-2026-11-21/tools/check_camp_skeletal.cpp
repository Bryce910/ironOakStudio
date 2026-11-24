#include "engine/world_animation.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cassert>
using namespace world_animation;
static Vec3 point(const Asset&a,const Pose&p,unsigned index){Vec3 v,n;std::string e;assert(deform_vertex(a.mesh.vertices[index],p,v,n,e));v=transform_point(p.global[a.skeleton.mesh_node],v);assert(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));return v;}
static double distance(Vec3 a,Vec3 b){return std::sqrt(std::pow(a.x-b.x,2)+std::pow(a.y-b.y,2)+std::pow(a.z-b.z,2));}
int main(int argc,char**argv){assert(argc==4);for(int file=1;file<argc;++file){Asset a;std::string e;if(!load_glb(argv[file],a,e)){std::fprintf(stderr,"%s: %s\n",argv[file],e.c_str());return 1;}assert(a.clips.size()==4&&a.skeleton.palette.size()==54);assert(std::count_if(a.materials.begin(),a.materials.end(),[](const Material&m){return m.organic_body;})==1);Pose neutral;assert(sample(a.skeleton,a.clips[0],0,false,neutral,e));std::vector<unsigned> soles;double low=1e9,high=-1e9;for(unsigned i=0;i<a.mesh.vertices.size();++i){auto p=point(a,neutral,i);low=std::min(low,double(p.y));high=std::max(high,double(p.y));if(p.y<.035)soles.push_back(i);}assert(!soles.empty());assert(std::abs(low)<.025&&high>1.75&&high<1.90);double worst_drift=0,worst_jump=0,worst_seam=0,max_hand=0;unsigned hand=0;for(unsigned i=0;i<a.skeleton.joints.size();++i)if(a.skeleton.joints[i].name=="hand_r")hand=i;
for(const auto&clip:a.clips){Pose first,last;assert(sample(a.skeleton,clip,0,false,first,e));assert(sample(a.skeleton,clip,clip.duration,false,last,e));for(unsigned i=0;i<a.mesh.vertices.size();i+=73)worst_seam=std::max(worst_seam,distance(point(a,first,i),point(a,last,i)));Pose prior=first;for(unsigned frame=0;frame<=unsigned(clip.duration*60);++frame){Pose p;assert(sample(a.skeleton,clip,double(frame)/60,false,p,e));for(unsigned i:soles)worst_drift=std::max(worst_drift,distance(point(a,neutral,i),point(a,p,i)));for(unsigned i=0;i<a.mesh.vertices.size();i+=113)worst_jump=std::max(worst_jump,distance(point(a,prior,i),point(a,p,i)));max_hand=std::max(max_hand,distance(transform_point(p.global[hand],{}),transform_point(first.global[hand],{})));prior=p;}}
std::printf("%s: nodes=%zu joints=%zu vertices=%zu clips=%zu height=%.6f sole_drift=%.6f max60Hz_step=%.6f loop_seam=%.6f hand_motion=%.6f\n",argv[file],a.skeleton.joints.size(),a.skeleton.palette.size(),a.mesh.vertices.size(),a.clips.size(),high-low,worst_drift,worst_jump,worst_seam,max_hand);assert(worst_drift<.025);assert(worst_jump<.04);assert(worst_seam<.002);assert(max_hand>.10);}
}
