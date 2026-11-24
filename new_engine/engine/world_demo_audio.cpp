#include "engine/world_demo_audio.h"
#include "engine/world_atlas.h"
#include "engine/world_castle_creek.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <new>

namespace {
constexpr unsigned beds=WORLD_DEMO_AUDIO_BEDS,steps=unsigned(WorldDemoAudioSurface::Count)*WORLD_DEMO_AUDIO_STEP_VARIANTS;
constexpr const char *bed_names[]={"wind","forest","creek","fire","rain","market","tavern","night"};
constexpr const char *surface_names[]={"soil","stone","wood","wet"};
double smooth(double lo,double hi,double value){double t=std::clamp((value-lo)/(hi-lo),0.,1.);return t*t*(3.-2.*t);}
double circle(double x,double z,double cx,double cz,double inner,double outer){return 1.-smooth(inner,outer,std::hypot(x-cx,z-cz));}
bool valid(const WorldDemoAudioFrame &f){
    for(double v:{f.x,f.y,f.z,f.yaw})if(!std::isfinite(v)||std::abs(v)>1e8)return false;
    return std::isfinite(f.dt)&&f.dt>=0&&f.dt<=1&&std::isfinite(f.rain)&&f.rain>=0&&f.rain<=1.5f&&
        std::isfinite(f.wind)&&f.wind>=0&&f.wind<=3&&std::isfinite(f.hour)&&f.hour>=0&&f.hour<=24;
}
double pan(double sx,double sz,const WorldDemoAudioFrame &f){
    const double dx=sx-f.x,dz=sz-f.z,d=std::hypot(dx,dz);
    // Viewer forward=(sin(yaw),0,-cos(yaw)); its right=(cos(yaw),0,sin(yaw)).
    return d<.01?0.:std::clamp((dx*std::cos(f.yaw)+dz*std::sin(f.yaw))/d,-1.,1.);
}
void stereo(int channel,double value){
    // Equal-power stereo is deliberately mild: fully lateral sources retain
    // the far ear, preventing binary left/right jumps at close distances.
    const double angle=(value*.72+1.)*.7853981633974483;
    Mix_SetPanning(channel,Uint8(std::lround(std::cos(angle)*255)),Uint8(std::lround(std::sin(angle)*255)));
}
}
struct WorldDemoAudio {
    float ambience_gain=1;
    std::array<Mix_Chunk*,beds+steps> clips={};
    std::array<WorldDemoAudioFire,WORLD_DEMO_AUDIO_FIRE_CAPACITY> fires={};
    std::size_t fire_count=0;
    WorldDemoAudioStats stats;
    int first_channel=0;
    bool owns_audio_subsystem=false,owns_channels=false,have_position=false;
    double previous_x=0,previous_z=0,stride_distance=0,zone_timer=0;
    unsigned step_variant=0,step_voice=0;
    double village_x=0,village_z=0;
    double creek_x=0,creek_z=0;
    double fire_x=0,fire_z=0;
    float village=0,forest=0,creek=0,market=0,fire=0;
    WorldDemoAudioSurface terrain_surface=WorldDemoAudioSurface::Soil;
};

bool world_demo_audio_set_ambience_gain(WorldDemoAudio *a,float gain){if(!a||!std::isfinite(gain)||gain<0||gain>1)return false;a->ambience_gain=gain;return true;}

WorldDemoAudio *world_demo_audio_create(const char *directory,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    if(!directory||!directory[0])return nullptr;
    auto *a=new(std::nothrow)WorldDemoAudio;if(!a)return nullptr;
    a->owns_audio_subsystem=(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO)==0;
    if(a->owns_audio_subsystem&&SDL_InitSubSystem(SDL_INIT_AUDIO)<0){
        if(error&&capacity)std::snprintf(error,capacity,"SDL audio unavailable: %s",SDL_GetError());
        delete a;return nullptr;
    }
    int frequency=0,channels=0;Uint16 format=0;
    if(!Mix_QuerySpec(&frequency,&format,&channels)){
        if(Mix_OpenAudio(44100,AUDIO_S16SYS,2,1024)<0){
            if(error&&capacity)std::snprintf(error,capacity,"SDL_mixer unavailable: %s",Mix_GetError());
            world_demo_audio_free(a);return nullptr;
        }
        a->stats.owns_device=true;
    }
    // Tail channels do not disturb the legacy mixer's0..32 ownership. The
    // atlas viewer is the sole producer here; it never calls legacy audio_update.
    a->first_channel=Mix_AllocateChannels(-1);
    if(a->first_channel<0||a->first_channel>128||Mix_AllocateChannels(a->first_channel+WORLD_DEMO_AUDIO_VOICES)!=a->first_channel+int(WORLD_DEMO_AUDIO_VOICES)){
        if(error&&capacity)std::snprintf(error,capacity,"Cannot reserve demo audio channels");
        world_demo_audio_free(a);return nullptr;
    }
    a->owns_channels=true;
    for(unsigned i=0;i<a->clips.size();i++){
        char path[1024];int length=0;
        if(i<beds)length=std::snprintf(path,sizeof(path),"%s/%s.wav",directory,bed_names[i]);
        else {const unsigned n=i-beds;length=std::snprintf(path,sizeof(path),"%s/step_%s_%u.wav",directory,surface_names[n/3],n%3);}
        if(length<=0||std::size_t(length)>=sizeof(path)||!(a->clips[i]=Mix_LoadWAV(path))){
            if(error&&capacity)std::snprintf(error,capacity,"Demo PCM clip unavailable: %s",path);
            world_demo_audio_free(a);return nullptr;
        }
        a->stats.decoded_bytes+=a->clips[i]->alen;
        if(i<beds)a->stats.loaded_beds++;else a->stats.loaded_steps++;
    }
    world_atlas_village_origin(&a->village_x,&a->village_z);
    for(unsigned i=0;i<beds;i++){
        Mix_Volume(a->first_channel+int(i),0);
        stereo(a->first_channel+int(i),0);
        if(Mix_PlayChannel(a->first_channel+int(i),a->clips[i],-1)<0){
            if(error&&capacity)std::snprintf(error,capacity,"Cannot start demo ambience voice");
            world_demo_audio_free(a);return nullptr;
        }
    }
    a->stats.ready=true;
    const char *driver=SDL_GetCurrentAudioDriver();
    std::snprintf(a->stats.driver,sizeof(a->stats.driver),"%s",driver?driver:"unavailable");
    std::printf("[World] Demo audio: 8 crossfading beds, 12 surface footsteps, 12 voices, %.2f MiB PCM, driver=%s\n",a->stats.decoded_bytes/1048576.,a->stats.driver);
    return a;
}
bool world_demo_audio_set_fires(WorldDemoAudio *a,const WorldDemoAudioFire *fires,std::size_t count){
    if(!a||count>a->fires.size()||(count&&!fires))return false;
    for(std::size_t i=0;i<count;i++){
        for(double v:{fires[i].x,fires[i].y,fires[i].z})if(!std::isfinite(v)||std::abs(v)>1e8)return false;
        if(!std::isfinite(fires[i].radius)||fires[i].radius<=0||fires[i].radius>80||
           !std::isfinite(fires[i].strength)||fires[i].strength<0||fires[i].strength>1)return false;
    }
    if(count)std::copy_n(fires,count,a->fires.begin());
    a->fire_count=count;a->zone_timer=0;return true;
}
namespace {
void sample_zones(WorldDemoAudio &a,const WorldDemoAudioFrame &f){
    const auto sample=world_atlas_sample(f.x,f.z);
    const auto village_sample=world_atlas_village_sample(f.x,f.z);
    const double x=f.x-a.village_x,z=f.z-a.village_z;
    a.village=world_atlas_village_weight(f.x,f.z);
    a.forest=std::clamp(std::max(sample.forest_density*.5f,world_atlas_rainforest_weight(f.x,f.z)),0.f,1.f);
    a.market=float(circle(x,z,0,35,35,100))*a.village;
    double nearest=1e12;
    if(std::abs(x)<300&&std::abs(z)<300){
        const auto &points=world_castle_creek_points;
        for(std::size_t i=1;i<std::size(points);i++){
            const double dx=points[i].x-points[i-1].x,dz=points[i].z-points[i-1].z;
            const double t=std::clamp(((x-points[i-1].x)*dx+(z-points[i-1].z)*dz)/(dx*dx+dz*dz),0.,1.);
            const double px=points[i-1].x+t*dx,pz=points[i-1].z+t*dz,d=std::hypot(x-px,z-pz);
            if(d<nearest){nearest=d;a.creek_x=a.village_x+px;a.creek_z=a.village_z+pz;}
        }
    }
    // Gentle pool shore, rather than a waterfall over the still-water pool.
    const auto *pool=world_atlas_rainforest_pool();
    if(pool){const double dx=f.x-pool->x,dz=f.z-pool->z;
        const double angle=std::atan2(dz/pool->radius_z,dx/pool->radius_x);
        const double px=pool->x+std::cos(angle)*pool->radius_x,pz=pool->z+std::sin(angle)*pool->radius_z;
        const double d=std::hypot(f.x-px,f.z-pz);
        if(d<nearest){nearest=d;a.creek_x=px;a.creek_z=pz;}
    }
    a.creek=float((1.-smooth(2,32,nearest))*.8);
    a.fire=0;
    for(std::size_t i=0;i<a.fire_count;i++){
        const auto &fire=a.fires[i];const double d=std::hypot(std::hypot(f.x-fire.x,f.z-fire.z),f.y-fire.y);
        const float gain=float((1.-smooth(.6,fire.radius,d))*fire.strength);
        if(gain>a.fire){a.fire=gain;a.fire_x=fire.x;a.fire_z=fire.z;}
    }
    a.terrain_surface=village_sample.paving>.45?WorldDemoAudioSurface::Stone:WorldDemoAudioSurface::Soil;
    if(world_castle_creek_bridge_local(x,z))a.terrain_surface=WorldDemoAudioSurface::Wood;
    a.stats.zone_samples++;
}
}
bool world_demo_audio_update(WorldDemoAudio *a,const WorldDemoAudioFrame &f){
    if(!a||!a->stats.ready||!valid(f))return false;
    auto &s=a->stats;s.updates++;s.focused=f.focused;
    const float dt=std::min(f.dt,.25f);
    const double displacement=a->have_position?std::hypot(f.x-a->previous_x,f.z-a->previous_z):0.;
    const bool cut=displacement>std::max(2.,double(f.dt)*14.);
    if(cut)s.rejected_jumps++;
    a->zone_timer-=dt;
    if(a->zone_timer<=0||cut||!a->have_position){sample_zones(*a,f);a->zone_timer=.15;}
    a->previous_x=f.x;a->previous_z=f.z;a->have_position=true;
    if(!f.focused){
        for(unsigned i=0;i<WORLD_DEMO_AUDIO_VOICES;i++)Mix_Volume(a->first_channel+int(i),0);
        for(unsigned i=beds;i<WORLD_DEMO_AUDIO_VOICES;i++)Mix_HaltChannel(a->first_channel+int(i));
        std::fill_n(s.gains,beds,0.f);std::fill_n(s.targets,beds,0.f);s.active_voices=beds;
        a->stride_distance=0;return true;
    }
    const float outdoors=f.interior?.055f:f.covered?.45f:1.f;
    const float day=float(smooth(5.4,7.2,f.hour)*(1.-smooth(18.6,20.4,f.hour)));
    const float rain=std::clamp(f.rain,0.f,1.f),wind=std::clamp(f.wind,0.f,1.5f);
    float targets[]={
        outdoors*(.10f+.11f*wind)*(1-.5f*a->village),
        outdoors*a->forest*(.12f+.30f*day)*(1-.8f*rain),
        outdoors*.42f*a->creek,
        .28f*a->fire,
        (.55f*rain)*(f.interior?.10f:f.covered?.42f:1.f),
        outdoors*.25f*a->market*day,
        f.interior?(f.quiet_interior?0.f:.46f):float(circle(f.x-a->village_x,f.z-a->village_z,-145,-82,8,20))*.035f,
        outdoors*a->forest*.27f*(1-day)*(1-.7f*rain)
    };
    // Peak-normalized clips are <=.36; sum of bed gains<=1.12 and four quiet
    // step voices leave ample headroom even at every overlapping zone boundary.
    float total=0;for(float gain:targets)total+=gain;
    const float normalization=std::min(1.f,1.12f/std::max(total,.001f));
    const float follow=float(-std::expm1(-double(dt)/.65));
    for(unsigned i=0;i<beds;i++){
        s.targets[i]=targets[i]*normalization*a->ambience_gain;
        s.gains[i]+=(s.targets[i]-s.gains[i])*follow;
        Mix_Volume(a->first_channel+int(i),int(std::lround(s.gains[i]*MIX_MAX_VOLUME)));
    }
    stereo(a->first_channel+int(WorldDemoAudioBed::Creek),pan(a->creek_x,a->creek_z,f));
    stereo(a->first_channel+int(WorldDemoAudioBed::Fire),pan(a->fire_x,a->fire_z,f));
    stereo(a->first_channel+int(WorldDemoAudioBed::Market),pan(a->village_x,a->village_z+35,f)*.45);
    s.surface=f.interior?WorldDemoAudioSurface::Wood:a->terrain_surface;
    if(s.surface==WorldDemoAudioSurface::Soil&&rain>.2f&&!f.covered)s.surface=WorldDemoAudioSurface::Wet;
    if(!f.grounded||cut||f.dt<=0)a->stride_distance=0;
    else if(displacement>.00001){
        s.walked_m+=displacement;a->stride_distance+=displacement;
        const double stride=f.running?1.85:1.20;
        // A normal frame schedules at most one step. Dropped frames do not
        // replay missed transient sounds in a burst after a stall.
        if(a->stride_distance+1e-7>=stride){
            a->stride_distance=std::fmod(std::max(0.,a->stride_distance-stride),stride);
            const unsigned surface=unsigned(s.surface),variant=a->step_variant++%WORLD_DEMO_AUDIO_STEP_VARIANTS;
            const int channel=a->first_channel+int(beds+a->step_voice++%4);
            // Transient voices stay centered. Registering a panning effect on
            // a short finished SDL_mixer channel would allocate on each step.
            Mix_HaltChannel(channel);
            // Even four simultaneous transient peaks plus maximum overlapping
            // bed gains stay below0.85 full scale before device mixing.
            Mix_Volume(channel,f.running?31:26);
            Mix_PlayChannel(channel,a->clips[beds+surface*3+variant],0);
            s.steps++;s.surface_steps[surface]++;
        }
    }
    s.active_voices=0;for(unsigned i=0;i<WORLD_DEMO_AUDIO_VOICES;i++)if(Mix_Playing(a->first_channel+int(i)))s.active_voices++;
    return true;
}
WorldDemoAudioStats world_demo_audio_stats(const WorldDemoAudio *a){return a?a->stats:WorldDemoAudioStats{};}
void world_demo_audio_free(WorldDemoAudio *a){
    if(!a)return;
    if(a->owns_channels&&Mix_QuerySpec(nullptr,nullptr,nullptr))
        for(unsigned i=0;i<WORLD_DEMO_AUDIO_VOICES;i++)Mix_HaltChannel(a->first_channel+int(i));
    for(auto *clip:a->clips)if(clip)Mix_FreeChunk(clip);
    if(a->stats.owns_device)Mix_CloseAudio();
    else if(a->owns_channels&&Mix_AllocateChannels(-1)==a->first_channel+int(WORLD_DEMO_AUDIO_VOICES))
        Mix_AllocateChannels(a->first_channel);
    if(a->owns_audio_subsystem)SDL_QuitSubSystem(SDL_INIT_AUDIO);
    delete a;
}
