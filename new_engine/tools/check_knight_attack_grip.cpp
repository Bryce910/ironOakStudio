// Post-export source-palm audit using the actual Valwood GLB importer/sampler.
#include "engine/world_animation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace wa=world_animation;
struct Stats {
    unsigned samples=0,played=0;
    double palm=0,played_palm=0,length=0,protected_world=0,hand_degrees=0,reach=0;
};
wa::Vec3 sub(wa::Vec3 a,wa::Vec3 b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
double length(wa::Vec3 v){return std::hypot(double(v.x),double(v.y),double(v.z));}
wa::Vec3 point(const wa::Pose&p,unsigned i){return wa::transform_point(p.global[i],{});}
unsigned node(const wa::Asset&a,const std::string&name){
    for(unsigned i=0;i<a.skeleton.joints.size();++i)if(a.skeleton.joints[i].name==name)return i;
    throw std::runtime_error("Missing node: "+name);
}
const wa::Clip&clip(const wa::Asset&a,const std::string&name){
    for(const auto&c:a.clips)if(c.name==name)return c;
    throw std::runtime_error("Missing clip: "+name);
}
bool under(const wa::Asset&a,unsigned child,unsigned parent){
    for(int i=int(child);i>=0;i=a.skeleton.joints[unsigned(i)].parent)if(unsigned(i)==parent)return true;
    return false;
}
double rotation_error(const wa::Mat4&a,const wa::Mat4&b){
    double trace=0;
    for(unsigned c=0;c<3;++c){
        double dot=0,la=0,lb=0;
        for(unsigned r=0;r<3;++r){double x=a.m[c*4+r],y=b.m[c*4+r];dot+=x*y;la+=x*x;lb+=y*y;}
        trace+=dot/std::sqrt(la*lb);
    }
    return std::acos(std::clamp((trace-1.)*.5,-1.,1.))*180./3.14159265358979323846;
}
void json(const Stats&s){
    std::cout<<"{\"samples\":"<<s.samples<<",\"played_window_samples\":"<<s.played
      <<",\"max_source_palm_error_m\":"<<s.palm<<",\"max_played_source_palm_error_m\":"<<s.played_palm
      <<",\"max_segment_length_error_m\":"<<s.length<<",\"max_protected_global_matrix_error\":"<<s.protected_world
      <<",\"max_preserved_hand_rotation_error_degrees\":"<<s.hand_degrees<<",\"max_wrist_reach_fraction\":"<<s.reach<<"}";
}
int main(int argc,char**argv){try{
    if(argc!=4&&argc!=5)throw std::runtime_error("Usage: check_knight_attack_grip original-v2.glb v3.glb source-reference.txt [reference-hz]");
    const unsigned hz=argc==5?unsigned(std::stoi(argv[4])):120;
    wa::Asset old,newer;std::string error;
    if(!wa::load_glb(argv[1],old,error)||!wa::validate(old,error))throw std::runtime_error("v2 import: "+error);
    if(!wa::load_glb(argv[2],newer,error)||!wa::validate(newer,error))throw std::runtime_error("v3 import: "+error);
    if(old.skeleton.joints.size()!=newer.skeleton.joints.size())throw std::runtime_error("Skeleton changed");
    const unsigned clav=node(newer,"clavicle_l"),upper=node(newer,"upperarm_l"),lower=node(newer,"lowerarm_l"),hand=node(newer,"hand_l"),
      index=node(newer,"index_01_l"),ring=node(newer,"ring_01_l"),weapon=node(newer,"valwood_weapon_r");
    std::ifstream input(argv[3]);if(!input)throw std::runtime_error("Missing source reference");
    std::map<std::string,Stats> results;
    std::string name;double time,x,y,z;int played;
    while(input>>name>>time>>x>>y>>z>>played){
        wa::Pose a,b;
        if(!wa::sample(old.skeleton,clip(old,name),time,false,a,error)||!wa::sample(newer.skeleton,clip(newer,name),time,false,b,error))
            throw std::runtime_error("Sample: "+error);
        auto&s=results[name];++s.samples;s.played+=played;
        wa::Vec3 i=point(b,index),r=point(b,ring),palm{(i.x+r.x)*.5f,(i.y+r.y)*.5f,(i.z+r.z)*.5f};
        wa::Vec3 target=wa::transform_point(b.global[weapon],{float(x),float(y),float(z)});
        const double residual=length(sub(palm,target));s.palm=std::max(s.palm,residual);if(played)s.played_palm=std::max(s.played_palm,residual);
        for(const auto chain:{std::pair<unsigned,unsigned>{clav,upper},{upper,lower},{lower,hand}}){
            double before=length(sub(point(a,chain.second),point(a,chain.first))),after=length(sub(point(b,chain.second),point(b,chain.first)));
            s.length=std::max(s.length,std::abs(before-after));
        }
        const double arm=length(sub(point(b,lower),point(b,upper)))+length(sub(point(b,hand),point(b,lower)));
        s.reach=std::max(s.reach,length(sub(point(b,hand),point(b,upper)))/arm);
        for(unsigned n=0;n<b.nodes;++n)if(!under(newer,n,clav))for(unsigned k=0;k<16;++k)
            s.protected_world=std::max(s.protected_world,double(std::abs(a.global[n].m[k]-b.global[n].m[k])));
        s.hand_degrees=std::max(s.hand_degrees,rotation_error(a.global[hand],b.global[hand]));
    }
    bool passed=results.size()==5;
    for(const auto&[name,s]:results)passed&=s.samples>100&&s.palm<.004&&s.played_palm<.0035&&s.length<.000002&&s.protected_world<.00001&&s.hand_degrees<1.&&s.reach<.9951;
    std::cout<<std::setprecision(10)<<"{\"schema\":\"valwood.knight-source-grip-runtime.v3\",\"passed\":"<<(passed?"true":"false")
      <<",\"sample_hz\":"<<hz<<",\"engine_importer_and_sampler\":true,\"midpoints_between_baked_keys_included\":true,\"clips\":{";
    bool first=true;for(const auto&[name,s]:results){if(!first)std::cout<<',';first=false;std::cout<<'"'<<name<<"\":";json(s);}std::cout<<"}}\n";
    return passed?0:1;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
