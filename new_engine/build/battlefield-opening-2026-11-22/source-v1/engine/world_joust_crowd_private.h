#ifndef VALWOOD_WORLD_JOUST_CROWD_PRIVATE_H
#define VALWOOD_WORLD_JOUST_CROWD_PRIVATE_H
#include "engine/world_models.h"
#include "engine/world_model_pose_transform.h"
#include "engine/world_joust.h"
#include <array>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
// Five actual owned head/hair/clothing families share lean original-rig poses.
// All A/B VAOs and bulk transforms are configured once at loading, never while
// cheering; a frame uploads only20 fixed-capacity placement/weight snapshots.
class WorldJoustCrowd {
    struct Family {unsigned source=0;bool hair=false,seated=false;float seat_y=0;std::string style;};
    struct Group {std::array<WorldModelPoseBlend*,8> blends={};std::vector<WorldModelInstance> base,instances;std::vector<float> weights;unsigned variant=0,active=0;};
    std::array<std::array<WorldModelAsset*,8>,5> poses_={};std::array<Family,5> families_={};std::array<Group,20> groups_={};
    float clock_=0,last_clock_=0,swell_=0,cycle_=5.6f;std::uint64_t serial_=0;
    unsigned spectators_=0,seated_=0,family_count_=0,hair_families_=0,styles_=0,pose_pairs_=0;
    unsigned long long draws_=0,uploads_=0,failures_=0;
    static bool safe_path(const std::string &path){const std::filesystem::path p(path);if(p.empty()||p.is_absolute())return false;for(const auto &part:p)if(part=="..")return false;return true;}
public:
    bool load(const std::filesystem::path &directory,std::string &error){
        release();std::ifstream in(directory/"crowd.cfg");std::string tag;unsigned version=0,variants=0,poses=0;
        if(!(in>>tag>>version)||tag!="VALWOOD_JOUST_CROWD"||version!=2){error="Missing diverse spectator manifest";return false;}
        while(in>>tag){
            if(tag=="variants")in>>variants;
            else if(tag=="poses")in>>poses;
            else if(tag=="cycle")in>>cycle_;
            else if(tag=="family"){
                unsigned index=0,source=0,hair=0,seated=0;float support=0;std::string style;
                if(!(in>>index>>source>>hair>>seated>>support>>std::quoted(style))||index>=5||source!=index+1||families_[index].source||hair>1||seated>1||!std::isfinite(support)||support<0||support>.7f||style.empty()){error="Invalid source spectator family";release();return false;}
                families_[index]={source,hair!=0,seated!=0,support,style};
            }else if(tag=="pose"){
                unsigned v=0,p=0;std::string path;
                if(!(in>>v>>p>>std::quoted(path))||v>=5||p>=8||poses_[v][p]||!safe_path(path)){error="Invalid spectator source pose";release();return false;}
                poses_[v][p]=world_model_load_pose_origin((directory/path).c_str(),WORLD_MODEL_POSE_BLEND_VERSION);
                if(!poses_[v][p]){error="Spectator source pose unavailable";release();return false;}
            }else if(tag=="end")break;
            else {error="Unknown spectator field";release();return false;}
            if(!in){error="Truncated spectator manifest";release();return false;}
        }
        if(variants!=5||poses!=8||!std::isfinite(cycle_)||cycle_<4||cycle_>9){error="Diverse spectator bank dimensions differ";release();return false;}
        for(unsigned i=0;i<5;++i){if(!families_[i].source){error="Incomplete spectator family provenance";release();return false;}for(auto *pose:poses_[i])if(!pose){error="Incomplete spectator pose bank";release();return false;}}
        family_count_=5;for(const auto &family:families_)hair_families_+=family.hair;styles_=5;
        unsigned seat=0;
        for(int side:{-1,1})for(float zc:{-33.f,-16.f,16.f,33.f})for(unsigned tier=0;tier<3;++tier)for(float bench:{-5.5f,-2.75f,0.f,2.75f,5.5f})for(int pair:{-1,1}){
            // A coprime permutation interleaves actual families along every row.
            auto &group=groups_[(seat*7u)%groups_.size()];group.variant=unsigned((seat*7u)%groups_.size())/4;
            const auto &family=families_[group.variant];const float height=family.seated?.98f+.02f*float((seat*37u)%101u)/100.f:.92f+.16f*float((seat*37u)%101u)/100.f;
            const float bench_top=.79f+tier*.48f,platform=.40f+tier*.48f;
            const float y=family.seated?std::max(platform+.008f,bench_top-family.seat_y*height):bench_top;
            WorldModelInstance actor{side*(8.2f+tier*1.25f),y,zc+bench+pair*.55f,side>0?-world_joust::pi*.5f:world_joust::pi*.5f,height};
            actor.axis_x=.10f*(float((seat*53u)%101u)/50.f-1.f);actor.axis_z=.06f*(float((seat*29u)%101u)/50.f-1.f);
            group.base.push_back(actor);seated_+=family.seated;++seat;
        }
        spectators_=seat;
        for(auto &group:groups_){
            group.instances=group.base;group.weights.resize(group.base.size());
            for(unsigned pair=0;pair<8;++pair){auto *&blend=group.blends[pair];blend=world_model_pose_blend_create(group.base.size(),WORLD_MODEL_POSE_BLEND_VERSION);
                if(!blend||!world_model_pose_blend_set_targets(blend,poses_[group.variant][pair],poses_[group.variant][(pair+1)%8])||!world_model_pose_blend_enable_transform(blend)){error="Spectator fixed pose/bulk configuration failed";release();return false;}++pose_pairs_;
            }
        }
        return update(0,0);
    }
    bool update(float clock,std::uint64_t contacts){
        if(!spectators_)return false;
        const float dt=std::clamp(clock-last_clock_,0.f,.1f);last_clock_=clock;
        if(contacts!=serial_){serial_=contacts;swell_=1;}else swell_*=std::exp(-dt*1.35f);
        clock_+=dt*(1.f+.55f*swell_);
        for(unsigned index=0;index<groups_.size();++index){
            auto &group=groups_[index];const auto &family=families_[group.variant];const float speed=1.f+float(group.variant)*.045f;
            float phase=clock_*speed/cycle_+float(index%4)/4.f+.073f*group.variant;phase-=std::floor(phase);const float frame=phase*8;group.active=std::min(7u,unsigned(frame));const float weight=frame-group.active;
            for(unsigned i=0;i<group.base.size();++i){group.instances[i]=group.base[i];if(!family.seated){const float hop=std::max(0.f,std::sin(clock_*4.2f+index*.71f+i*.43f));group.instances[i].y+=swell_*.035f*hop*hop;}group.weights[i]=weight;}
            if(!world_model_pose_blend_upload(group.blends[group.active],group.instances.data(),group.weights.data(),group.instances.size())){++failures_;return false;}++uploads_;
        }
        return true;
    }
    void draw(unsigned program){for(auto &group:groups_)if(group.blends[group.active]){world_model_pose_blend_draw(group.blends[group.active],program);++draws_;}}
    void release(){for(auto &group:groups_){for(auto *blend:group.blends)world_model_pose_blend_free(blend);group={};}for(auto &bank:poses_)for(auto *&pose:bank){world_model_free(pose);pose=nullptr;}families_={};spectators_=seated_=family_count_=hair_families_=styles_=pose_pairs_=0;}
    unsigned spectators()const{return spectators_;}unsigned long long draws()const{return draws_;}unsigned long long uploads()const{return uploads_;}unsigned long long failures()const{return failures_;}float swell()const{return swell_;}
    unsigned family_count()const{return family_count_;}unsigned hair_family_count()const{return hair_families_;}unsigned style_count()const{return styles_;}unsigned seated_count()const{return seated_;}unsigned animated_spectators()const{return spectators_;}unsigned fixed_pose_pairs()const{return pose_pairs_;}
};
#endif
