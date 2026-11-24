#include "engine/world_ambience.h"
#include "engine/world_atlas.h"
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>

namespace {
constexpr int voices=6; // 0 fire, 1 creek, 2 wind, 3 one-shot A, 4 one-shot B, 5 footsteps
constexpr double tau=6.283185307179586;
float smooth01(float a,float b,float x){float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
}
struct WorldAmbience {
    int first=-1;Mix_Chunk *fire=nullptr,*creek=nullptr,*wind=nullptr,*bird=nullptr,*crow=nullptr,*cricket=nullptr,*owl=nullptr,*step=nullptr;
    bool playing[3]={};float level[3]={};
    float water_timer=0,water_distance=1e9;double water_dir[2]={0,0};
    float shot_timer=4.f;std::uint64_t last_plants=0;bool plants_seen=false;float step_cooldown=0;
    std::mt19937 rng{20261121u};
    unsigned steps=0,shots=0;float max_fire=0,max_creek=0,min_water=1e9f;
};
static Mix_Chunk *load(const std::string &path){Mix_Chunk *c=Mix_LoadWAV(path.c_str());if(!c)std::fprintf(stderr,"[Ambience] missing %s\n",path.c_str());return c;}

WorldAmbience *world_ambience_create(const char *audio_dir,const char *sfx_dir){
    if(!audio_dir||!sfx_dir)return nullptr;
    int frequency=0;Uint16 format=0;int channels=0;
    if(!Mix_QuerySpec(&frequency,&format,&channels)){std::fprintf(stderr,"[Ambience] audio not open\n");return nullptr;}
    auto *a=new WorldAmbience;
    a->first=Mix_AllocateChannels(-1);
    if(a->first<0||a->first>128||Mix_AllocateChannels(a->first+voices)!=a->first+voices){delete a;return nullptr;}
    const std::string A(audio_dir),S(sfx_dir);
    a->fire=load(A+"/camp_fire_loop.wav");a->step=load(A+"/step_single.wav");
    a->creek=load(S+"/amb_river.wav");a->wind=load(S+"/amb_wind.wav");
    a->bird=load(S+"/amb_bird.wav");a->crow=load(S+"/amb_crow.wav");a->cricket=load(S+"/amb_cricket.wav");a->owl=load(S+"/amb_owl.wav");
    for(int i=0;i<voices;++i){Mix_Volume(a->first+i,0);}
    std::printf("[Ambience] ready: fire=%d creek=%d wind=%d birds=%d steps=%d (voices %d..%d)\n",a->fire!=nullptr,a->creek!=nullptr,a->wind!=nullptr,a->bird!=nullptr,a->step!=nullptr,a->first,a->first+voices-1);
    return a;
}

static void pan_for(const WorldAmbienceFrame &f,double dx,double dz,Uint8 &left,Uint8 &right){
    const double len=std::hypot(dx,dz);
    if(len<.5){left=right=255;return;}
    const double rx=std::cos(f.yaw),rz=-std::sin(f.yaw);                 // camera right
    const double p=std::clamp((dx*rx+dz*rz)/len,-1.,1.)*.75;             // keep some bleed
    auto gain=[](double g){return Uint8(255.*std::min(1.,std::sqrt(std::clamp(g,0.,1.))*1.41421356));};
    left=gain(.5-.5*p);right=gain(.5+.5*p);
}
static void loop(WorldAmbience *a,int slot,Mix_Chunk *chunk,float target,float dt,float rate,Uint8 l=255,Uint8 r=255){
    if(!chunk)return;
    a->level[slot]+=(target-a->level[slot])*std::min(1.f,dt*rate);
    const int ch=a->first+slot,vol=int(std::clamp(a->level[slot],0.f,1.f)*MIX_MAX_VOLUME);
    if(!a->playing[slot]&&a->level[slot]>.01f){Mix_Volume(ch,vol);Mix_PlayChannel(ch,chunk,-1);a->playing[slot]=true;}
    else if(a->playing[slot]&&a->level[slot]<.004f&&target<.004f){Mix_HaltChannel(ch);a->playing[slot]=false;}
    if(a->playing[slot]){Mix_Volume(ch,vol);Mix_SetPanning(ch,l,r);}
    if(slot==0)a->max_fire=std::max(a->max_fire,a->level[0]);if(slot==1)a->max_creek=std::max(a->max_creek,a->level[1]);
}

void world_ambience_update(WorldAmbience *a,const WorldAmbienceFrame &f){
    if(!a||!std::isfinite(f.dt)||f.dt<0)return;
    const float dt=std::min(f.dt,.1f);
    // Outdoor ambience owns no sound inside the apartment or while the game is
    // paused, unfocused or loading. Loops fade quickly (rate 6 = ~0.4 s) and
    // the transient wildlife voices stop at once, so nothing carries indoors.
    const bool silent=f.indoor||f.paused||!f.focused||f.loading;
    const float master=silent?0.f:std::clamp(f.sfx_volume,0.f,1.f);
    if(silent){
        for(int k=3;k<5;++k)Mix_HaltChannel(a->first+k);
        if(f.paused||!f.focused||f.loading)Mix_HaltChannel(a->first+5);
        a->shot_timer=std::max(a->shot_timer,3.f);
        for(int k=0;k<3;++k)loop(a,k,k==0?a->fire:k==1?a->creek:a->wind,0.f,dt,6.f);
        a->plants_seen=false;              // re-arm footsteps without a backlog
        return;
    }
    // Campfire: full within 2.5 m, gone by 28 m (inverse-square-ish rolloff).
    {
        float target=0;Uint8 l=255,r=255;
        if(f.hearth){
            const double dx=f.hearth_pos[0]-f.listener[0],dz=f.hearth_pos[2]-f.listener[2],d=std::hypot(dx,dz);
            target=float(.85*std::clamp(2.5/std::max(d,2.5),0.,1.))*(1.f-smooth01(18.f,28.f,float(d)));
            pan_for(f,dx,dz,l,r);
        }
        loop(a,0,a->fire,target*master,dt,3.f,l,r);
    }
    // Creek: sample a ring around the listener every 0.4 s for the nearest water.
    a->water_timer-=dt;
    if(a->water_timer<=0){
        a->water_timer=.4f;a->water_distance=1e9;
        for(int ring=1;ring<=5;++ring)for(int k=0;k<16;++k){
            const double radius=ring*7.,ang=tau*k/16.+ring*.2;
            const double x=f.listener[0]+std::sin(ang)*radius,z=f.listener[2]+std::cos(ang)*radius;
            if(world_atlas_local_water(x,z,nullptr,nullptr)&&radius<a->water_distance){a->water_distance=float(radius);a->water_dir[0]=x-f.listener[0];a->water_dir[1]=z-f.listener[2];}
        }
        if(world_atlas_local_water(f.listener[0],f.listener[2],nullptr,nullptr)){a->water_distance=0;a->water_dir[0]=a->water_dir[1]=0;}
        a->min_water=std::min(a->min_water,a->water_distance);
    }
    {
        Uint8 l=255,r=255;pan_for(f,a->water_dir[0],a->water_dir[1],l,r);
        const float target=a->water_distance<1e8f?.55f*(1.f-smooth01(6.f,36.f,a->water_distance)):0.f;
        loop(a,1,a->creek,target*master,dt,1.5f,l,r);
    }
    // Soft wind/leaf bed, a little stronger in the open evening.
    loop(a,2,a->wind,.16f*master,dt,.5f);
    // One-shots: birds by day, crickets/owl after dusk, occasional crow.
    a->shot_timer-=dt;
    if(a->shot_timer<=0){
        std::uniform_real_distribution<float> u(0.f,1.f);
        const bool dusk=f.hour>19.2f||f.hour<5.5f,evening=f.hour>18.f&&f.hour<=19.2f;
        Mix_Chunk *chunk=nullptr;float vol=.35f;
        const float roll=u(a->rng);
        if(dusk)chunk=roll<.85f?a->cricket:a->owl,vol=.30f;
        else if(evening)chunk=roll<.5f?a->bird:a->cricket,vol=.28f;
        else chunk=roll<.9f?a->bird:a->crow,vol=roll<.9f?.32f:.18f;
        if(chunk){
            const int ch=a->first+3+int(u(a->rng)*2.f);
            if(!Mix_Playing(ch)){
                const float ang=u(a->rng)*float(tau);Uint8 l=255,r=255;pan_for(f,std::sin(ang)*20.,std::cos(ang)*20.,l,r);
                Mix_Volume(ch,int(vol*master*(.6f+.4f*u(a->rng))*MIX_MAX_VOLUME));Mix_SetPanning(ch,l,r);Mix_PlayChannel(ch,chunk,0);++a->shots;
            }
        }
        a->shot_timer=dusk?2.5f+u(a->rng)*4.f:4.f+u(a->rng)*9.f;
    }
    // Footsteps on each skeletal foot plant.
    a->step_cooldown-=dt;
    if(!a->plants_seen||!f.hero_steps){a->last_plants=f.foot_plants;a->plants_seen=true;}
    if(f.foot_plants!=a->last_plants){
        if(f.foot_plants>a->last_plants&&f.hero_grounded&&a->step&&a->step_cooldown<=0){
            const int ch=a->first+5;std::uniform_real_distribution<float> u(.75f,1.f);
            const double dx=f.hero_pos[0]-f.listener[0],dz=f.hero_pos[2]-f.listener[2];Uint8 l=255,r=255;pan_for(f,dx,dz,l,r);
            const float near=float(std::clamp(3./std::max(std::hypot(dx,dz),3.),.2,1.));
            Mix_Volume(ch,int(.55f*master*near*u(a->rng)*MIX_MAX_VOLUME));Mix_SetPanning(ch,l,r);Mix_PlayChannel(ch,a->step,0);
            a->step_cooldown=.18f;++a->steps;
        }
        a->last_plants=f.foot_plants;
    }
}
void world_ambience_free(WorldAmbience *a){
    if(!a)return;
    std::printf("[Ambience] steps=%u calls=%u fire_peak=%.2f creek_peak=%.2f nearest_water=%.0fm\n",a->steps,a->shots,a->max_fire,a->max_creek,a->min_water);
    if(a->first>=0){for(int i=0;i<voices;++i)Mix_HaltChannel(a->first+i);if(Mix_AllocateChannels(-1)==a->first+voices)Mix_AllocateChannels(a->first);}
    for(Mix_Chunk *c:{a->fire,a->creek,a->wind,a->bird,a->crow,a->cricket,a->owl,a->step})if(c)Mix_FreeChunk(c);
    delete a;
}
