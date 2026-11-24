#include "engine/world_demo_audio.h"
#include "engine/world_atlas.h"
#include "engine/world_castle_creek.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <filesystem>
#include <fstream>
#include <vector>
#include <atomic>
#include <bitset>
#include <memory>

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
namespace {
constexpr unsigned thunder_asset_limit=4;
constexpr std::size_t thunder_pcm_budget=64u*1024u*1024u;
struct ThunderPcm {std::vector<float> samples;};
struct ThunderBank {
    std::array<ThunderPcm,thunder_asset_limit> clips;
    std::array<unsigned,2> near{},distant{};unsigned count=0,near_count=0,distant_count=0;
    std::size_t bytes=0;
};
struct ThunderCommand {const ThunderPcm*clip=nullptr;std::uint64_t epoch=0;};
struct ThunderPlayback {
    const ThunderPcm*clip=nullptr;unsigned cursor=0;
    float gain=0,pan=0,coefficient=1,filter1=0,filter2=0;
};
struct ThunderVoice {
    std::array<ThunderCommand,16> commands{};std::atomic<unsigned> head{0},tail{0};
    std::atomic<std::uint64_t> epoch{0},crossfades{0};std::atomic<bool> active{false};
    std::atomic<float> target_gain{0},target_pan{0},target_coefficient{1};
    ThunderPlayback current,retiring;unsigned retire_remaining=0,attack_remaining=0,retire_length=1,edge_frames=1;
    float gain_follow=1,pan_follow=1,filter_follow=1;std::uint64_t observed_epoch=0;Uint16 format=0;
};
float thunder_sample(ThunderPlayback&p,unsigned edge){
    if(!p.clip||p.cursor>=p.clip->samples.size()){p.clip=nullptr;return 0;}
    float sample=p.clip->samples[p.cursor++];
    const auto remaining=p.clip->samples.size()-p.cursor;
    if(remaining<edge)sample*=float(remaining)/edge;
    p.filter1+=p.coefficient*(sample-p.filter1);p.filter2+=p.coefficient*(p.filter1-p.filter2);
    if(p.cursor>=p.clip->samples.size())p.clip=nullptr;
    return p.filter2*p.gain;
}
void SDLCALL thunder_effect(int,void *stream,int length,void *context){
    auto &v=*static_cast<ThunderVoice*>(context);const auto epoch=v.epoch.load(std::memory_order_acquire);
    if(epoch!=v.observed_epoch){v.current={};v.retiring={};v.retire_remaining=v.attack_remaining=0;v.observed_epoch=epoch;}
    unsigned tail=v.tail.load(std::memory_order_relaxed),end=v.head.load(std::memory_order_acquire);
    for(;tail!=end;++tail){const auto &c=v.commands[tail%v.commands.size()];if(c.epoch!=epoch)continue;
        if(v.current.clip&&v.current.gain>.00001f){v.retiring=v.current;v.retire_remaining=v.retire_length;v.crossfades.fetch_add(1,std::memory_order_relaxed);}
        v.attack_remaining=v.retire_remaining;v.current={};v.current.clip=c.clip;v.current.pan=v.target_pan.load(std::memory_order_relaxed);v.current.coefficient=v.target_coefficient.load(std::memory_order_relaxed);
    }
    v.tail.store(tail,std::memory_order_release);
    auto *out=static_cast<Uint8*>(stream);std::memset(out,0,std::size_t(std::max(length,0)));
    const unsigned width=v.format==AUDIO_S16SYS?2:4,frames=length>0?unsigned(length)/(width*2):0;
    const float gain=v.target_gain.load(std::memory_order_relaxed),pan=v.target_pan.load(std::memory_order_relaxed),coefficient=v.target_coefficient.load(std::memory_order_relaxed);
    for(unsigned frame=0;frame<frames;++frame){
        auto &p=v.current;p.gain+=(gain-p.gain)*v.gain_follow;p.pan+=(pan-p.pan)*v.pan_follow;p.coefficient+=(coefficient-p.coefficient)*v.filter_follow;
        const float fade_in=v.attack_remaining?1.f-float(v.attack_remaining--)/v.retire_length:1.f;
        const float main=thunder_sample(p,v.edge_frames)*fade_in;const double angle=(p.pan*.72+1.)*.7853981633974483;
        float left=main*float(std::cos(angle)),right=main*float(std::sin(angle));
        if(v.retire_remaining&&v.retiring.clip){const float fade=float(v.retire_remaining)/v.retire_length;const float old=thunder_sample(v.retiring,v.edge_frames)*fade;const double old_angle=(v.retiring.pan*.72+1.)*.7853981633974483;left+=old*float(std::cos(old_angle));right+=old*float(std::sin(old_angle));--v.retire_remaining;}else v.retire_remaining=0;
        if(v.format==AUDIO_S16SYS){const Sint16 l=Sint16(std::lround(std::clamp(left,-1.f,32767.f/32768.f)*32768.f)),r=Sint16(std::lround(std::clamp(right,-1.f,32767.f/32768.f)*32768.f));std::memcpy(out+frame*4,&l,2);std::memcpy(out+frame*4+2,&r,2);}
        else if(v.format==AUDIO_F32SYS){left=std::clamp(left,-1.f,1.f);right=std::clamp(right,-1.f,1.f);std::memcpy(out+frame*8,&left,4);std::memcpy(out+frame*8+4,&right,4);}
    }
    v.active.store(v.current.clip||v.retire_remaining,std::memory_order_release);
}
void stop_thunder(ThunderVoice&v){v.epoch.fetch_add(1,std::memory_order_acq_rel);}
}
struct WorldDemoAudio {
    float ambience_gain=1;
    std::unique_ptr<ThunderBank> thunder;int thunder_hz=0;
    std::array<world_storm::Thunder,2> thunder_sources{};std::array<bool,2> thunder_sources_valid{};std::array<unsigned,2> last_thunder_choice={~0u,~0u};unsigned thunder_voice=0;std::uint64_t thunder_serial=0;std::bitset<1024> thunder_seen;
    std::array<ThunderVoice,2> thunder_voices;std::array<Uint8,2048> thunder_silence{};Mix_Chunk thunder_carrier{};bool thunder_effects=false;
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

namespace {void thunder_parameters(ThunderVoice&,const world_storm::Thunder&,const WorldDemoAudioFrame&,int);}
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
    a->thunder_carrier.allocated=0;a->thunder_carrier.abuf=a->thunder_silence.data();a->thunder_carrier.alen=unsigned(a->thunder_silence.size());a->thunder_carrier.volume=MIX_MAX_VOLUME;
    int thunder_hz=0,thunder_channels=0;Uint16 thunder_format=0;Mix_QuerySpec(&thunder_hz,&thunder_format,&thunder_channels);
    a->thunder_effects=thunder_channels==2&&(thunder_format==AUDIO_S16SYS||thunder_format==AUDIO_F32SYS);
    for(unsigned i=0;i<2;++i){const int channel=a->first_channel+12+int(i);auto &v=a->thunder_voices[i];v.format=thunder_format;v.retire_length=std::max(1,thunder_hz*35/1000);v.edge_frames=std::max(1,thunder_hz*15/1000);v.gain_follow=float(-std::expm1(-1./(thunder_hz*.003)));v.pan_follow=float(-std::expm1(-1./(thunder_hz*.05)));v.filter_follow=float(-std::expm1(-1./(thunder_hz*.06)));a->thunder_hz=thunder_hz;Mix_Volume(channel,MIX_MAX_VOLUME);
        if(Mix_PlayChannel(channel,&a->thunder_carrier,-1)<0){if(error&&capacity)std::snprintf(error,capacity,"Cannot reserve thunder carrier");world_demo_audio_free(a);return nullptr;}
        if(a->thunder_effects&&!Mix_RegisterEffect(channel,thunder_effect,nullptr,&v))a->thunder_effects=false;
    }
    a->stats.ready=true;
    const char *driver=SDL_GetCurrentAudioDriver();
    std::snprintf(a->stats.driver,sizeof(a->stats.driver),"%s",driver?driver:"unavailable");
    std::printf("[World] Demo audio: 8 crossfading beds, 12 surface footsteps, 14 voices, %.2f MiB PCM, driver=%s\n",a->stats.decoded_bytes/1048576.,a->stats.driver);
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
    if(!f.focused||f.paused||f.loading||f.muted){
        for(unsigned i=0;i<12;i++)Mix_Volume(a->first_channel+int(i),0);
        for(unsigned i=beds;i<12;i++)Mix_HaltChannel(a->first_channel+int(i));
        std::fill_n(s.gains,beds,0.f);std::fill_n(s.targets,beds,0.f);s.active_voices=beds;
        for(auto &v:a->thunder_voices)stop_thunder(v);a->stride_distance=0;s.active_thunder=0;return true;
    }
    for(unsigned i=0;i<2;++i)if(a->thunder_sources_valid[i])thunder_parameters(a->thunder_voices[i],a->thunder_sources[i],f,a->thunder_hz);
    // A quiet home is sealed from the forest: no wind, birds, night insects,
    // creek or rain bed bleeds through. Its own room tone is owned elsewhere.
    const float outdoors=f.interior?(f.quiet_interior?0.f:.055f):f.covered?.45f:1.f;
    const float day=float(smooth(5.4,7.2,f.hour)*(1.-smooth(18.6,20.4,f.hour)));
    const float rain=std::clamp(f.rain,0.f,1.f),wind=std::clamp(f.wind,0.f,1.5f);
    float targets[]={
        outdoors*(.10f+.11f*wind)*(1-.5f*a->village),
        outdoors*a->forest*(.12f+.30f*day)*(1-.8f*rain),
        outdoors*.42f*a->creek,
        .28f*a->fire,
        (.55f*rain)*(f.interior?(f.quiet_interior?0.f:.10f):f.covered?.42f:1.f),
        outdoors*.25f*a->market*day,
        f.interior?(f.quiet_interior?0.f:.46f):float(circle(f.x-a->village_x,f.z-a->village_z,-145,-82,8,20))*.035f,
        outdoors*a->forest*.27f*(1-day)*(1-.7f*rain)
    };
    // Peak-normalized clips are <=.36; sum of bed gains<=1.12 and four quiet
    // step voices leave ample headroom even at every overlapping zone boundary.
    float total=0;for(float gain:targets)total+=gain;
    const float normalization=std::min(1.f,1.12f/std::max(total,.001f));
    const float follow=float(-std::expm1(-double(dt)/.65));
    const float user=std::isfinite(f.sfx_gain)?std::clamp(f.sfx_gain,0.f,1.f):1.f;
    auto step_volume=[&](int v){return int(std::lround(v*user));};
    for(unsigned i=0;i<beds;i++){
        s.targets[i]=targets[i]*normalization*a->ambience_gain;
        s.gains[i]+=(s.targets[i]-s.gains[i])*follow;
        Mix_Volume(a->first_channel+int(i),int(std::lround(s.gains[i]*user*MIX_MAX_VOLUME)));
    }
    stereo(a->first_channel+int(WorldDemoAudioBed::Creek),pan(a->creek_x,a->creek_z,f));
    stereo(a->first_channel+int(WorldDemoAudioBed::Fire),pan(a->fire_x,a->fire_z,f));
    stereo(a->first_channel+int(WorldDemoAudioBed::Market),pan(a->village_x,a->village_z+35,f)*.45);
    s.surface=f.interior?WorldDemoAudioSurface::Wood:a->terrain_surface;
    if(s.surface==WorldDemoAudioSurface::Soil&&rain>.2f&&!f.covered)s.surface=WorldDemoAudioSurface::Wet;
    if(f.contact_steps){
        a->stride_distance=0;
        if(f.grounded&&!cut&&f.dt>0)s.walked_m+=displacement;
        if(f.step_events&&f.grounded&&!cut&&f.dt>0){
            const unsigned surface=unsigned(s.surface),variant=a->step_variant++%WORLD_DEMO_AUDIO_STEP_VARIANTS;
            const int channel=a->first_channel+int(beds+a->step_voice++%4);
            Mix_HaltChannel(channel);
            // Soft socks-and-shoes indoors; ordinary boots outdoors.
            Mix_Volume(channel,step_volume(f.quiet_interior?15:f.running?29:24));
            Mix_PlayChannel(channel,a->clips[beds+surface*3+variant],0);
            s.steps++;s.surface_steps[surface]++;
        }
    }
    else if(!f.grounded||cut||f.dt<=0)a->stride_distance=0;
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
            Mix_Volume(channel,step_volume(f.running?31:26));
            Mix_PlayChannel(channel,a->clips[beds+surface*3+variant],0);
            s.steps++;s.surface_steps[surface]++;
        }
    }
    s.active_thunder=0;for(auto &v:a->thunder_voices)s.active_thunder+=v.active.load(std::memory_order_acquire);
    s.active_voices=s.active_thunder;for(unsigned i=0;i<12;i++)if(Mix_Playing(a->first_channel+int(i)))s.active_voices++;
    return true;
}

namespace {
bool load_thunder_pcm(const char*path,int hz,std::size_t budget,ThunderPcm&out,char*error,std::size_t capacity){
    auto fail=[&](const char*message){if(error&&capacity)std::snprintf(error,capacity,"%s",message);return false;};
    if(!path||!*path)return fail("Missing optional thunder WAV path");
    std::ifstream f(std::filesystem::u8path(path),std::ios::binary|std::ios::ate);if(!f||f.tellg()<12||f.tellg()>16*1024*1024)return fail("Missing or oversized optional thunder WAV");
    const auto size=std::uint64_t(f.tellg());f.seekg(0);Uint8 riff[12];if(!f.read(reinterpret_cast<char*>(riff),12)||std::memcmp(riff,"RIFF",4)||std::memcmp(riff+8,"WAVE",4))return fail("Thunder asset must be PCM WAV");
    auto u16=[](const Uint8*p){return unsigned(p[0])|(unsigned(p[1])<<8);};auto u32=[](const Uint8*p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);};
    const auto end=std::uint64_t(u32(riff+4))+8;if(end>size||end<12)return fail("Malformed thunder RIFF length");
    bool seen=false;unsigned tag=0,channels=0,bits=0,rate=0,alignment=0;std::uint64_t offset=0,bytes=0;
    for(std::uint64_t at=12;at+8<=end;){Uint8 chunk[8];f.seekg(std::streamoff(at));if(!f.read(reinterpret_cast<char*>(chunk),8))return fail("Truncated thunder WAV");const auto n=u32(chunk+4);const auto next=at+8+n+(n&1);if(next>end)return fail("Malformed thunder WAV chunk");
        if(!std::memcmp(chunk,"fmt ",4)){Uint8 fmt[16];if(n<16||n>4096||seen||!f.read(reinterpret_cast<char*>(fmt),16))return fail("Invalid/duplicate thunder PCM format");seen=true;tag=u16(fmt);channels=u16(fmt+2);rate=u32(fmt+4);alignment=u16(fmt+12);bits=u16(fmt+14);}
        if(!std::memcmp(chunk,"data",4)){if(offset)return fail("Duplicate thunder PCM data");offset=at+8;bytes=n;}at=next;
    }
    if(!offset||!bytes||bytes>8*1024*1024||channels<1||channels>2||rate<8000||rate>96000||!((tag==1&&bits==16)||(tag==3&&bits==32))||alignment!=channels*(bits/8)||bytes%alignment)return fail("Thunder supports PCM16/float32 mono/stereo WAV8..96kHz");
    SDL_AudioCVT conversion{};if(SDL_BuildAudioCVT(&conversion,tag==1?AUDIO_S16LSB:AUDIO_F32LSB,Uint8(channels),int(rate),AUDIO_F32SYS,1,hz)<0||conversion.len_mult<1||bytes>budget/std::size_t(conversion.len_mult))return fail("Thunder replacement exceeds shared64MiB PCM limit");
    const auto storage=std::size_t(bytes)*std::size_t(conversion.len_mult);std::vector<float> samples((storage+3)/4);if(samples.capacity()*sizeof(float)>budget)return fail("Thunder allocation exceeds shared64MiB PCM limit");
    f.seekg(std::streamoff(offset));if(!f.read(reinterpret_cast<char*>(samples.data()),std::streamsize(bytes)))return fail("Truncated thunder PCM");conversion.buf=reinterpret_cast<Uint8*>(samples.data());conversion.len=int(bytes);
    if(conversion.needed&&SDL_ConvertAudio(&conversion)<0)return fail("Thunder PCM conversion failed");const auto converted=conversion.needed?std::size_t(conversion.len_cvt):std::size_t(conversion.len);if(!converted||converted%sizeof(float)||converted>storage)return fail("Invalid converted thunder PCM");samples.resize(converted/sizeof(float));
    // SDL's resampling workspace can be much larger than the final mono PCM.
    // Compact during loading, charging BOTH allocations against the same cap.
    if(samples.capacity()>samples.size()){
        const auto working=samples.capacity()*sizeof(float);if(converted>budget-working)return fail("Thunder loading workspace plus resident PCM exceeds64MiB");
        std::vector<float> compact(samples.begin(),samples.end());if(working+compact.capacity()*sizeof(float)>budget)return fail("Thunder compact PCM exceeds64MiB loading budget");samples.swap(compact);
    }
    for(float value:samples)if(!std::isfinite(value)||std::abs(value)>1.001f)return fail("Thunder PCM contains nonfinite/out-of-range samples");out.samples=std::move(samples);return true;
}
void thunder_parameters(ThunderVoice&v,const world_storm::Thunder&event,const WorldDemoAudioFrame&f,int hz){
    const double distance=std::hypot(std::hypot(event.x-f.x,event.z-f.z),event.y-f.y);
    const float attenuation=float(1./std::sqrt(1.+std::pow(distance/1200.,2.)));
    const float roof=f.interior?.42f:f.covered?.78f:1.f;
    const float distance_cutoff=float(20000.-18500.*smooth(150,1800,distance));
    const float cutoff=f.interior?std::min(distance_cutoff,800.f):f.covered?std::min(distance_cutoff,2200.f):distance_cutoff;
    v.target_gain.store(event.volume*.45f*attenuation*roof,std::memory_order_relaxed);
    v.target_pan.store(float(pan(event.x,event.z,f)),std::memory_order_relaxed);
    v.target_coefficient.store(float(-std::expm1(-6.283185307179586*cutoff/hz)),std::memory_order_relaxed);
}
}
bool world_demo_audio_preload_thunder_bank(WorldDemoAudio*a,const WorldDemoThunderBank&files,char*error,std::size_t capacity){
    if(error&&capacity)error[0]=0;auto fail=[&](const char*message){if(error&&capacity)std::snprintf(error,capacity,"%s",message);return false;};
    if(!a||!a->stats.ready||!a->thunder_effects||files.near_count<1||files.near_count>2||files.distant_count<1||files.distant_count>2)return fail("Thunder bank needs1..2 close and1..2 distant WAVs");
    try{
        int hz=0,channels=0;Uint16 format=0;if(!Mix_QuerySpec(&hz,&format,&channels)||hz!=a->thunder_hz||channels!=2||(format!=AUDIO_S16SYS&&format!=AUDIO_F32SYS))return fail("Thunder mixer changed; recreate audio owner");
        auto prepared=std::make_unique<ThunderBank>();prepared->near_count=files.near_count;prepared->distant_count=files.distant_count;
        struct Input{const char*path=nullptr;std::uintmax_t bytes=0;};std::array<Input,4> inputs{};unsigned input_count=0;
        for(unsigned family=0;family<2;++family){const auto count=family?files.distant_count:files.near_count;for(unsigned i=0;i<count;++i){const char*path=family?files.distant_paths[i]:files.near_paths[i];if(!path||!*path)return fail("Thunder bank has an unassigned WAV");bool found=false;for(unsigned j=0;j<input_count;++j)found|=std::strcmp(inputs[j].path,path)==0;if(!found)inputs[input_count++]={path,std::filesystem::file_size(std::filesystem::u8path(path))};}}
        // Largest first minimizes transient resampling workspace at a high-rate
        // borrowed mixer. The role mapping remains the caller's exact order.
        for(unsigned i=0;i<input_count;++i)for(unsigned j=i+1;j<input_count;++j)if(inputs[j].bytes>inputs[i].bytes)std::swap(inputs[i],inputs[j]);
        for(unsigned i=0;i<input_count;++i){if(a->stats.decoded_bytes+prepared->bytes>=thunder_pcm_budget)return fail("Thunder replacement exceeds shared64MiB PCM limit");auto &clip=prepared->clips[i];if(!load_thunder_pcm(inputs[i].path,hz,thunder_pcm_budget-a->stats.decoded_bytes-prepared->bytes,clip,error,capacity))return false;prepared->bytes+=clip.samples.capacity()*sizeof(float);++prepared->count;}
        for(unsigned family=0;family<2;++family){const auto count=family?files.distant_count:files.near_count;for(unsigned i=0;i<count;++i){const char*path=family?files.distant_paths[i]:files.near_paths[i];unsigned index=0;for(;index<input_count;++index)if(std::strcmp(inputs[index].path,path)==0)break;if(index==input_count)return fail("Thunder role mapping changed during loading");(family?prepared->distant:prepared->near)[i]=index;}}
        // Explicit loading only: synchronize these TWO effects while their
        // permanent carriers continue to reserve the channels from channel=-1.
        for(unsigned i=0;i<2;++i)Mix_UnregisterEffect(a->first_channel+12+int(i),thunder_effect);
        for(auto &v:a->thunder_voices){stop_thunder(v);v.current={};v.retiring={};v.retire_remaining=v.attack_remaining=0;v.active=false;v.tail.store(v.head.load());}
        auto previous=std::move(a->thunder);a->thunder=std::move(prepared);bool registered=true;
        for(unsigned i=0;i<2;++i)registered=Mix_RegisterEffect(a->first_channel+12+int(i),thunder_effect,nullptr,&a->thunder_voices[i])!=0&&registered;
        if(!registered){for(unsigned i=0;i<2;++i)Mix_UnregisterEffect(a->first_channel+12+int(i),thunder_effect);a->thunder=std::move(previous);for(unsigned i=0;i<2;++i)Mix_RegisterEffect(a->first_channel+12+int(i),thunder_effect,nullptr,&a->thunder_voices[i]);return fail("Cannot register thunder effects; previous bank retained");}
        a->thunder_sources_valid.fill(false);a->stats.decoded_bytes-=a->stats.thunder_bytes;a->stats.thunder_bytes=a->thunder->bytes;a->stats.decoded_bytes+=a->thunder->bytes;a->stats.thunder_assets=a->thunder->count;a->stats.thunder_ready=true;a->stats.active_thunder=0;a->last_thunder_choice.fill(~0u);return true;
    }catch(const std::exception&){return fail("Optional thunder loading failed; previous bank preserved");}
}
bool world_demo_audio_preload_thunder(WorldDemoAudio*a,const char*path,char*error,std::size_t capacity){WorldDemoThunderBank bank;bank.near_paths[0]=bank.distant_paths[0]=path;bank.near_count=bank.distant_count=1;return world_demo_audio_preload_thunder_bank(a,bank,error,capacity);}
bool world_demo_audio_thunder(WorldDemoAudio *a,const world_storm::Thunder *events,std::size_t count,const WorldDemoAudioFrame &f){
    if(!a||!a->stats.ready||!valid(f)||count>world_storm::pending_limit||(count&&!events))return false;
    for(std::size_t i=0;i<count;++i){const auto&e=events[i];if(!e.serial||!std::isfinite(e.volume)||e.volume<0||e.volume>1)return false;for(double v:{e.x,e.y,e.z})if(!std::isfinite(v)||std::abs(v)>1.e8)return false;}
    const bool allowed=f.focused&&!f.paused&&!f.loading&&!f.muted;
    if(!allowed)for(auto &v:a->thunder_voices)stop_thunder(v);
    for(std::size_t i=0;i<count;++i){const auto&e=events[i];if(e.serial>a->thunder_serial){const auto shift=e.serial-a->thunder_serial;a->thunder_seen<<=std::size_t(std::min<std::uint64_t>(shift,1024));a->thunder_serial=e.serial;}
        const auto age=a->thunder_serial-e.serial;if(age>=1024||a->thunder_seen.test(std::size_t(age))){++a->stats.thunder_ignored;continue;}a->thunder_seen.set(std::size_t(age));
        if(!allowed||!a->thunder||e.volume<=0){++a->stats.thunder_ignored;continue;}
        const unsigned slot=a->thunder_voice++%2;auto &voice=a->thunder_voices[slot];const unsigned head=voice.head.load(std::memory_order_relaxed),tail=voice.tail.load(std::memory_order_acquire);if(head-tail>=voice.commands.size()){++a->stats.thunder_ignored;continue;}
        if(voice.active.load(std::memory_order_acquire)||head!=tail)++a->stats.thunder_replaced;
        const double distance=std::hypot(std::hypot(e.x-f.x,e.z-f.z),e.y-f.y);const double close_probability=1.-smooth(250,1100,distance);const bool near=world_storm::random(e.serial^0x93bc41u)<close_probability;const unsigned choices=near?a->thunder->near_count:a->thunder->distant_count;unsigned choice=unsigned(world_storm::hash(e.serial^0x419a23u)%choices);const unsigned family=near?0:1;if(choices>1&&choice==a->last_thunder_choice[family])choice=(choice+1)%choices;a->last_thunder_choice[family]=choice;
        const unsigned index=(near?a->thunder->near:a->thunder->distant)[choice];a->thunder_sources[slot]=e;a->thunder_sources_valid[slot]=true;thunder_parameters(voice,e,f,a->thunder_hz);
        voice.commands[head%voice.commands.size()]={&a->thunder->clips[index],voice.epoch.load(std::memory_order_acquire)};voice.head.store(head+1,std::memory_order_release);++a->stats.thunder_started;if(near)++a->stats.thunder_near_started;else ++a->stats.thunder_distant_started;
    }
    a->stats.active_thunder=0;for(auto &v:a->thunder_voices)a->stats.active_thunder+=v.active.load(std::memory_order_acquire);return true;
}
WorldDemoAudioStats world_demo_audio_stats(const WorldDemoAudio *a){if(!a)return {};auto result=a->stats;result.active_thunder=0;for(const auto&v:a->thunder_voices){result.active_thunder+=v.active.load(std::memory_order_acquire);result.thunder_crossfades+=v.crossfades.load(std::memory_order_relaxed);}return result;}
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
