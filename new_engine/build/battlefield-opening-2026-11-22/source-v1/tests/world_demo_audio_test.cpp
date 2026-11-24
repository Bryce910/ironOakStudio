#include "engine/world_demo_audio.h"
#include "engine/world_atlas.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

namespace {bool track=false;unsigned allocations=0;}
void *operator new(std::size_t n){if(track)allocations++;if(auto *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete(void *p,std::size_t) noexcept{std::free(p);}
void *operator new(std::size_t n,const std::nothrow_t&) noexcept{if(track)allocations++;return std::malloc(n?n:1);}
void operator delete(void *p,const std::nothrow_t&) noexcept{std::free(p);}

namespace {
unsigned bed(WorldDemoAudioBed value){return unsigned(value);}
void gain_budget(const WorldDemoAudioStats &s){
    float target=0,gain=0;for(unsigned i=0;i<WORLD_DEMO_AUDIO_BEDS;i++){
        assert(std::isfinite(s.targets[i])&&s.targets[i]>=0&&s.targets[i]<=1.12f);
        assert(std::isfinite(s.gains[i])&&s.gains[i]>=0&&s.gains[i]<=1.12f);
        target+=s.targets[i];gain+=s.gains[i];
    }
    assert(target<=1.12001f&&gain<=1.12001f&&s.active_voices<=WORLD_DEMO_AUDIO_VOICES);
}
WorldDemoAudio *create(){char error[256]={};auto *a=world_demo_audio_create("projects/valwood_world/audio/demo",error,sizeof(error));
    if(!a)std::fprintf(stderr,"Audio creation failed: %s\n",error);
    assert(a);
    const auto s=world_demo_audio_stats(a);assert(s.ready&&s.loaded_beds==8&&s.loaded_steps==12&&s.voice_capacity==WORLD_DEMO_AUDIO_VOICES);
    assert(s.decoded_bytes>10*1024*1024&&s.decoded_bytes<32*1024*1024);return a;
}
void settle(WorldDemoAudio *a,WorldDemoAudioFrame &f){for(unsigned i=0;i<600;i++){assert(world_demo_audio_update(a,f));gain_budget(world_demo_audio_stats(a));}}
std::uint64_t distance_run(unsigned fps){
    auto *a=create();WorldDemoAudioFrame f;world_atlas_village_origin(&f.x,&f.z);f.z+=35;f.y=25.75;f.grounded=true;f.dt=1.f/fps;
    assert(world_demo_audio_update(a,f));
    track=true;for(unsigned i=0;i<fps*12;i++){f.x+=3.5/fps;assert(world_demo_audio_update(a,f));}track=false;
    auto s=world_demo_audio_stats(a);assert(s.steps==35&&std::abs(s.walked_m-42)<1e-6);
    assert(s.surface_steps[unsigned(WorldDemoAudioSurface::Stone)]>10&&allocations==0);gain_budget(s);
    const auto before=s.steps;
    for(unsigned i=0;i<fps*3;i++){f.yaw+=.01;assert(world_demo_audio_update(a,f));}
    assert(world_demo_audio_stats(a).steps==before); // Look and idle do not step.
    f.grounded=false;for(unsigned i=0;i<fps;i++){f.x+=2./fps;assert(world_demo_audio_update(a,f));}
    assert(world_demo_audio_stats(a).steps==before); // Flight/air never steps.
    f.grounded=true;f.x+=1000;assert(world_demo_audio_update(a,f));
    assert(world_demo_audio_stats(a).rejected_jumps==1&&world_demo_audio_stats(a).steps==before);
    world_demo_audio_free(a);return s.steps;
}
}
int main(){
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);assert(world_atlas_init());
    char error[256]={};assert(!world_demo_audio_create("/nonexistent/demo-audio",error,sizeof(error))&&error[0]);
    assert(!Mix_QuerySpec(nullptr,nullptr,nullptr)); // Partial failure closed device.
    const auto n30=distance_run(30),n60=distance_run(60),n144=distance_run(144);
    assert(n30==n60&&n60==n144);
    auto *a=create();WorldDemoAudioFrame f;world_atlas_village_origin(&f.x,&f.z);f.y=25.75;f.dt=1.f/60;
    const double ox=f.x,oz=f.z;f.z+=35;settle(a,f);
    auto s=world_demo_audio_stats(a);assert(s.targets[bed(WorldDemoAudioBed::Market)]>.2f&&s.surface==WorldDemoAudioSurface::Stone);
    assert(s.targets[bed(WorldDemoAudioBed::Creek)]<.01f); // No river sound in courtyard.
    f.x=ox-145;f.z=oz-82;f.interior=true;f.covered=true;settle(a,f);
    s=world_demo_audio_stats(a);assert(s.surface==WorldDemoAudioSurface::Wood&&s.targets[bed(WorldDemoAudioBed::Tavern)]>.4f);
    // A quiet home uses the same roof acoustics and wood steps as an interior,
    // while explicitly omitting the unrelated tavern crowd. The existing
    // interior default must still restore the original bed when requested.
    const auto tavern=s;f.quiet_interior=true;settle(a,f);s=world_demo_audio_stats(a);
    assert(s.targets[bed(WorldDemoAudioBed::Tavern)]==0&&s.gains[bed(WorldDemoAudioBed::Tavern)]<.000001f);
    for(unsigned i=0;i<WORLD_DEMO_AUDIO_BEDS;i++)if(i!=bed(WorldDemoAudioBed::Tavern))assert(std::abs(s.targets[i]-tavern.targets[i])<.000001f);
    assert(s.surface==WorldDemoAudioSurface::Wood);
    const auto home_steps=s.steps,wood_steps=s.surface_steps[unsigned(WorldDemoAudioSurface::Wood)];
    f.grounded=true;track=true;
    for(unsigned i=0;i<48;i++){f.x+=.05;assert(world_demo_audio_update(a,f));}
    track=false;s=world_demo_audio_stats(a);
    assert(s.steps==home_steps+2&&s.surface_steps[unsigned(WorldDemoAudioSurface::Wood)]==wood_steps+2&&allocations==0);
    assert(s.targets[bed(WorldDemoAudioBed::Tavern)]==0);
    f.grounded=false;f.interior=false;settle(a,f);s=world_demo_audio_stats(a);
    assert(s.targets[bed(WorldDemoAudioBed::Tavern)]>.03f); // Quiet flag is scoped to interiors.
    f.quiet_interior=false;settle(a,f);assert(std::abs(world_demo_audio_stats(a).targets[bed(WorldDemoAudioBed::Tavern)]-s.targets[bed(WorldDemoAudioBed::Tavern)])<.000001f);
    f.interior=true;settle(a,f);assert(world_demo_audio_stats(a).targets[bed(WorldDemoAudioBed::Tavern)]>.4f);
    f.rain=.8f;settle(a,f);const float inside_rain=world_demo_audio_stats(a).targets[bed(WorldDemoAudioBed::Rain)];
    f.x=ox;f.z=oz+219;f.interior=false;f.covered=false;settle(a,f);
    s=world_demo_audio_stats(a);assert(s.surface==WorldDemoAudioSurface::Wood&&s.targets[bed(WorldDemoAudioBed::Creek)]>.3f);
    assert(s.targets[bed(WorldDemoAudioBed::Rain)]>inside_rain*5);
    const WorldDemoAudioFire fire={f.x+2,f.y-1,f.z,12,1};assert(world_demo_audio_set_fires(a,&fire,1));
    WorldDemoAudioFire invalid=fire;invalid.radius=-1;assert(!world_demo_audio_set_fires(a,&invalid,1));
    settle(a,f);assert(world_demo_audio_stats(a).targets[bed(WorldDemoAudioBed::Fire)]>.20f);
    // The source stronghold has165 real fixtures. A nearby last socket must
    // contribute beyond the former40-origin bound without allocating voices.
    std::array<WorldDemoAudioFire,165> source_fires={};
    for(auto &v:source_fires)v={f.x+1000,f.y,f.z,12,1};
    source_fires.back()=fire;
    assert(world_demo_audio_set_fires(a,source_fires.data(),source_fires.size()));
    settle(a,f);assert(world_demo_audio_stats(a).targets[bed(WorldDemoAudioBed::Fire)]>.20f);
    assert(!world_demo_audio_set_fires(a,source_fires.data(),WORLD_DEMO_AUDIO_FIRE_CAPACITY+1));
    const auto *pool=world_atlas_rainforest_pool();f.x=pool->x+pool->radius_x+3;f.z=pool->z;f.y=float(pool->surface_y+1.75);f.rain=0;settle(a,f);
    s=world_demo_audio_stats(a);assert(s.targets[bed(WorldDemoAudioBed::Forest)]>.35f&&s.targets[bed(WorldDemoAudioBed::Creek)]>.3f);
    f.hour=23;settle(a,f);s=world_demo_audio_stats(a);assert(s.targets[bed(WorldDemoAudioBed::Night)]>.20f&&s.targets[bed(WorldDemoAudioBed::Forest)]<.15f);
    f.focused=false;assert(world_demo_audio_update(a,f));s=world_demo_audio_stats(a);for(float value:s.gains)assert(value==0);
    f.focused=true;assert(world_demo_audio_update(a,f));s=world_demo_audio_stats(a);assert(s.gains[bed(WorldDemoAudioBed::Night)]>0&&s.gains[bed(WorldDemoAudioBed::Night)]<.05f);
    const auto updates=s.updates;f.x=std::numeric_limits<double>::quiet_NaN();assert(!world_demo_audio_update(a,f));assert(world_demo_audio_stats(a).updates==updates);
    world_demo_audio_free(a);world_demo_audio_free(nullptr);assert(!Mix_QuerySpec(nullptr,nullptr,nullptr));
    // Borrowing an existing audio device must preserve it and its channel count.
    assert(SDL_InitSubSystem(SDL_INIT_AUDIO)==0&&Mix_OpenAudio(44100,AUDIO_S16SYS,2,1024)==0);Mix_AllocateChannels(33);
    a=create();assert(!world_demo_audio_stats(a).owns_device&&Mix_AllocateChannels(-1)==33+int(WORLD_DEMO_AUDIO_VOICES));world_demo_audio_free(a);
    assert(Mix_QuerySpec(nullptr,nullptr,nullptr)&&Mix_AllocateChannels(-1)==33);Mix_CloseAudio();SDL_QuitSubSystem(SDL_INIT_AUDIO);
    world_atlas_shutdown();SDL_Quit();
    std::printf("PASS demo audio: 20 PCM clips; 14 bounded voices; surfaces/zones/roof/focus; quiet homes retain roof attenuation and wood steps without tavern crowd; distance steps30/60/144=%llu/%llu/%llu; no C++ allocations during movement; borrowed device and partial failure clean\n",(unsigned long long)n30,(unsigned long long)n60,(unsigned long long)n144);
}
