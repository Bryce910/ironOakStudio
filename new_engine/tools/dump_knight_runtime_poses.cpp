// Export actual engine interpolation as an offline source for left-arm fitting.
#include "engine/world_animation.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
namespace wa=world_animation;
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("Usage: dump_knight_runtime_poses accepted-v2.glb");
    wa::Asset asset;std::string error;
    if(!wa::load_glb(argv[1],asset,error)||!wa::validate(asset,error))throw std::runtime_error(error);
    std::cout<<std::setprecision(10)<<"{\"schema\":\"valwood.actual-engine-pose-bake-reference.v3\",\"hz\":120,\"nodes\":[";
    bool first=true;for(const auto&joint:asset.skeleton.joints){if(!first)std::cout<<',';first=false;std::cout<<'"'<<joint.name<<'"';}
    std::cout<<"],\"clips\":{";first=true;
    for(const auto&clip:asset.clips)if(clip.name.rfind("Attack_",0)==0){
        if(!first)std::cout<<',';first=false;std::cout<<'"'<<clip.name<<"\":[";
        unsigned count=unsigned(std::llround(clip.duration*120));
        for(unsigned frame=0;frame<=count;++frame){
            if(frame)std::cout<<',';wa::Pose pose;
            if(!wa::sample(asset.skeleton,clip,double(frame)/120,false,pose,error))throw std::runtime_error(error);
            std::cout<<'[';
            for(unsigned node=0;node<pose.nodes;++node){if(node)std::cout<<',';const auto&t=pose.local[node];
                std::cout<<'['<<t.translation.x<<','<<t.translation.y<<','<<t.translation.z<<','
                  <<t.rotation.x<<','<<t.rotation.y<<','<<t.rotation.z<<','<<t.rotation.w<<','
                  <<t.scale.x<<','<<t.scale.y<<','<<t.scale.z<<']';}
            std::cout<<']';}
        std::cout<<']';}
    std::cout<<"}}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
