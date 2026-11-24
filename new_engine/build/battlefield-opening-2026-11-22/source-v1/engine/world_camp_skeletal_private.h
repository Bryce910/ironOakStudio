#ifndef VALWOOD_WORLD_CAMP_SKELETAL_PRIVATE_H
#define VALWOOD_WORLD_CAMP_SKELETAL_PRIVATE_H
#include "engine/world_animation.h"
#include "engine/world_animation_render.h"
#include "engine/world_camp_skeletal_timing.h"
#include <memory>
#include <cstdlib>
#include <filesystem>
// Authored source-rig skeletal acting. Separate from camp logic, collision and
// dialogue credit. One renderer/appearance keeps temporal identity unambiguous.
struct WorldCampSkeletal {
    struct Actor {
        world_animation::Asset asset;
        world_animation_render::Renderer *gpu=nullptr;
        world_animation::Pose pose;
        std::array<unsigned,4> clips={};
        std::array<float,4> weights={1,0,0,0};
        std::uint64_t motion_uploads=0;
        unsigned long long warm_epoch=0;
        std::array<unsigned,4> warmed={};
        bool uploaded=false;
        ~Actor(){world_animation_render::destroy(gpu);}
    };
    std::array<std::unique_ptr<Actor>,3> actors;
    bool enabled=false;
    double time=-1;
    std::uint64_t uploads=0,draws=0,motion_draws=0,failures=0,cpu_bytes=0,gpu_bytes=0;
    static std::unique_ptr<WorldCampSkeletal> load(){
        const char *directory=std::getenv("VALWOOD_CAMP_SKELETAL_DIRECTORY");
        if(!directory||!*directory)return {};
        auto result=std::make_unique<WorldCampSkeletal>();
        const char *roles[]={"rowan","bram","ellis"};
        const char *names[]={"Ambient","Listen","Talk","WarmHands"};
        for(unsigned i=0;i<3;++i){
            auto a=std::make_unique<Actor>();std::string error;
            const auto path=std::filesystem::path(directory)/(std::string(roles[i])+".glb");
            if(!world_animation::load_glb(path.string().c_str(),a->asset,error)){
                std::fprintf(stderr,"[Camp skeletal] Retained pose fallback: %s: %s\n",roles[i],error.c_str());return {};
            }
            for(unsigned k=0;k<4;++k){unsigned matches=0;
                for(unsigned n=0;n<a->asset.clips.size();++n)if(a->asset.clips[n].name==names[k]){a->clips[k]=n;++matches;}
                if(matches!=1){std::fprintf(stderr,"[Camp skeletal] Retained pose fallback: missing/ambiguous %s in %s\n",names[k],roles[i]);return {};}
            }
            if(!(a->gpu=world_animation_render::create(a->asset,1,error,96ull*1024*1024))){
                std::fprintf(stderr,"[Camp skeletal] Retained pose fallback: %s renderer: %s\n",roles[i],error.c_str());return {};
            }
            const auto gpu=world_animation_render::stats(a->gpu);
            result->gpu_bytes+=gpu.geometry_bytes+gpu.palette_bytes+gpu.morph_bytes+gpu.instance_bytes;
            result->cpu_bytes+=sizeof(Actor);
            for(const auto&clip:a->asset.clips)for(const auto&channel:clip.channels)result->cpu_bytes+=channel.times.size()*sizeof(float)+channel.values.size()*sizeof(std::array<float,4>);
            // Source image and geometry arrays are needed only during GPU
            // admission; playback retains the skeleton and real joint clips.
            a->asset.images.clear();a->asset.images.shrink_to_fit();
            a->asset.mesh={};a->asset.materials.clear();a->asset.materials.shrink_to_fit();
            result->actors[i]=std::move(a);
        }
        result->enabled=true;
        std::fprintf(stderr,"[Camp skeletal] Three retained purchased appearances, four continuous authored joint clips each; no source mocap claimed\n");
        return result;
    }
    bool warm(unsigned program){
        if(!enabled||!program)return false;
        for(auto &p:actors){auto&a=*p;
            if(a.warm_epoch!=world_program_epoch){a.warmed={};a.warm_epoch=world_program_epoch;}
            if(std::find(a.warmed.begin(),a.warmed.end(),program)!=a.warmed.end())continue;
            std::string error;if(!world_animation_render::warm(a.gpu,program,error)){
                std::fprintf(stderr,"[Camp skeletal] Retained pose fallback: program %u: %s\n",program,error.c_str());enabled=false;++failures;return false;
            }
            auto slot=std::find(a.warmed.begin(),a.warmed.end(),0u);if(slot!=a.warmed.end())*slot=program;
        }return true;
    }
    bool update(const std::array<WorldTrailCamper,4>&placements,const world_camp_presentation::State &presentation,double now){
        if(!enabled)return false;
        if(time==now&&actors[0]->uploaded)return true;
        const double dt=time<0?.0:std::clamp(now-time,0.,.25);time=now;
        for(unsigned i=0;i<3;++i){auto&a=*actors[i];const auto&acting=presentation.actors[i];
            const bool warm_hands=i==1&&!acting.nearby;
            const auto target=world_camp_skeletal_timing::target(acting.speaking,acting.listening||acting.attention>.35,warm_hands);
            world_camp_skeletal_timing::approach(a.weights,target,dt);
            std::array<world_animation::Blend,4> blends;
            for(unsigned k=0;k<4;++k)blends[k]={a.clips[k],world_camp_skeletal_timing::seconds(k,i,now),a.weights[k]};
            std::string error;
            if(!world_animation::blend(a.asset,blends.data(),4,true,a.pose,error)){std::fprintf(stderr,"[Camp skeletal] Retained pose fallback: sample: %s\n",error.c_str());enabled=false;++failures;return false;}
            const auto&p=placements[i];world_animation_render::Instance instance{0,p.y,0,1,p.yaw};
            world_animation_render::Bindings bindings;bindings.cull_face=false;
            if(!world_animation_render::upload(a.gpu,&a.pose,&instance,1,bindings,error)){std::fprintf(stderr,"[Camp skeletal] Retained pose fallback: upload: %s\n",error.c_str());enabled=false;++failures;return false;}
            a.uploaded=true;++uploads;
        }return true;
    }
};
#endif
