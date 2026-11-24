#ifndef VALWOOD_WORLD_JOUST_IMPACTS_H
#define VALWOOD_WORLD_JOUST_IMPACTS_H
#include "engine/world_joust.h"
#include "engine/world_models.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// Fixed render-thread pool for an owned immutable wood shard. Only load() does
// file IO/allocation. Positions remain arena-local; the host owns shader light,
// view and uOffset state. No shader modifications, blending or new targets.
class WorldJoustImpacts {
public:
    static constexpr unsigned capacity=48;
    struct Counters {
        std::uint64_t contacts=0,emitted=0,updates=0,uploads=0,draws=0,failures=0;
        unsigned alive=0,peak_alive=0;
    };
    bool ready=false;
private:
    struct Particle {
        world_joust::Vec3 p,v;
        float age=0,lifetime=.8f,scale=1,yaw=0,pitch=0,roll=0,spin=0;
        bool active=false;
    };
    std::array<Particle,capacity> particles_={};
    std::array<WorldModelInstance,capacity> instances_={};
    WorldModelAsset *chip_=nullptr;
    WorldModelInstanceBatch *batch_=nullptr;
    Counters counters_;
    unsigned cursor_=0;
    std::uint32_t serial_=0;
    bool dirty_=false;
    static bool finite(world_joust::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    static std::uint32_t bits(float v){std::uint32_t value=0;std::memcpy(&value,&v,sizeof value);return value;}
    static float random(std::uint32_t &state){state^=state<<13;state^=state>>17;state^=state<<5;return float(state&0x00ffffffu)/float(0x01000000u);}
    void upload(){
        unsigned count=0;
        for(const auto &p:particles_)if(p.active){
            const float fade=std::clamp((p.lifetime-p.age)/.22f,0.f,1.f);
            // Bounded final shrink avoids alpha overdraw and sorting costs.
            instances_[count++]={p.p.x,p.p.y,p.p.z,p.yaw,p.scale*std::max(.03f,fade),p.pitch,p.roll};
        }
        counters_.alive=count;counters_.peak_alive=std::max(counters_.peak_alive,count);
        if(!world_model_instance_batch_upload(batch_,instances_.data(),count)){++counters_.failures;return;}
        ++counters_.uploads;dirty_=false;
    }
public:
    WorldJoustImpacts()=default;
    WorldJoustImpacts(const WorldJoustImpacts&)=delete;
    WorldJoustImpacts &operator=(const WorldJoustImpacts&)=delete;
    ~WorldJoustImpacts(){release();}
    bool load(const char *path){
        release();counters_={};cursor_=0;serial_=0;
        if(!path||!*path){++counters_.failures;return false;}
        chip_=world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION);
        if(chip_)batch_=world_model_instance_batch_create(chip_,capacity);
        if(!chip_||!batch_){release();++counters_.failures;return false;}
        ready=true;dirty_=true;upload();
        if(counters_.failures){release();return false;}return true;
    }
    void emit(world_joust::Contact contact){
        if(!ready||contact.hit==world_joust::Hit::Miss)return;
        if(!finite(contact.point)||!finite(contact.normal)||!std::isfinite(contact.speed)||contact.point.y<-.5f||contact.point.y>12.f){++counters_.failures;return;}
        std::uint32_t state=0x9e3779b9u^(++serial_*0x85ebca6bu)^bits(contact.point.x)^bits(contact.point.z);
        if(!state)state=1;
        const auto outward=world_joust::normalized(contact.normal);
        const float power=std::clamp(contact.speed/18.f,.65f,1.35f);
        const unsigned number=14u+(serial_%5u);
        for(unsigned n=0;n<number;++n){
            auto &p=particles_[cursor_];cursor_=(cursor_+1)%capacity;p=Particle{};p.active=true;
            p.p=contact.point+outward*(.025f+.07f*random(state));
            p.p.y=std::max(p.p.y,.04f);
            p.v={(outward.x*1.4f+(random(state)-.5f)*4.7f)*power,
                 (1.35f+random(state)*2.2f+std::max(outward.y,0.f))*power,
                 (outward.z*1.4f+(random(state)-.5f)*4.7f)*power};
            p.lifetime=.62f+random(state)*.34f;p.scale=.8f+random(state)*.65f;
            p.yaw=random(state)*world_joust::pi*2;p.pitch=random(state)*world_joust::pi*2;p.roll=random(state)*world_joust::pi*2;
            p.spin=(random(state)-.5f)*19.f;++counters_.emitted;
        }
        ++counters_.contacts;dirty_=true;
    }
    void update(float dt,bool paused){
        if(!ready)return;
        if(!std::isfinite(dt)||dt<0){++counters_.failures;return;}
        if(!paused&&dt>0){
            // Preserve ordinary bounded slow frames with small deterministic
            // steps; a debugger stop never produces unbounded catch-up work.
            float left=std::min(dt,.25f);
            while(left>1.e-7f){
                const float step=std::min(left,1.f/120.f);left-=step;
                for(auto &p:particles_)if(p.active){
                    p.age+=step;if(p.age>=p.lifetime){p.active=false;continue;}
                    p.yaw+=p.spin*step;p.pitch+=p.spin*.71f*step;p.roll+=p.spin*.43f*step;
                    p.v.y-=9.81f*step;p.p=p.p+p.v*step;
                    // Exact vertical support extent of the centred shard's
                    // Y*X*Z instance rotation; rotated tips cannot cross y=0.
                    const float cp=std::cos(p.pitch),sp=std::sin(p.pitch);
                    const float floor=(.015f*std::abs(cp*std::sin(p.roll))+.015f*std::abs(cp*std::cos(p.roll))+.11f*std::abs(sp))*p.scale;
                    if(p.p.y<floor){p.p.y=floor;if(p.v.y<0)p.v.y=-p.v.y*.23f;p.v.x*=.72f;p.v.z*=.72f;p.spin*=.68f;}
                }
            }
            dirty_=true;++counters_.updates;
        }
        if(dirty_)upload();
    }
    void draw(unsigned program){
        if(!ready||!program||!counters_.alive)return;
        world_model_instance_batch_draw(batch_,program);++counters_.draws;
    }
    void release(){
        ready=false;world_model_instance_batch_free(batch_);batch_=nullptr;world_model_free(chip_);chip_=nullptr;
        for(auto &p:particles_)p.active=false;
        dirty_=false;counters_.alive=0;
    }
    Counters counters()const{return counters_;}
};
#endif
