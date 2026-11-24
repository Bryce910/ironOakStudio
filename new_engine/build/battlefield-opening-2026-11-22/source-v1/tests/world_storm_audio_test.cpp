#include "engine/world_demo_audio.h"
#include "engine/world_cinematics_audio.h"
#include "engine/world_atlas.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <new>
#include <string>
#include <unistd.h>
namespace {std::atomic<bool> tracking{false};unsigned allocations=0,checks=0;std::atomic<unsigned> sdl_allocations{0};
SDL_malloc_func original_malloc;SDL_calloc_func original_calloc;SDL_realloc_func original_realloc;SDL_free_func original_free;
void* traced_malloc(std::size_t n){if(tracking.load())++sdl_allocations;return original_malloc(n);}void* traced_calloc(std::size_t n,std::size_t width){if(tracking.load())++sdl_allocations;return original_calloc(n,width);}void* traced_realloc(void*p,std::size_t n){if(tracking.load())++sdl_allocations;return original_realloc(p,n);}void traced_free(void*p){original_free(p);}
}
void *operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);std::abort();}}while(false)
namespace {
std::atomic<unsigned> hooks{0};void SDLCALL existing(void*,Uint8*,int){++hooks;}
struct Capture {std::array<float,65536> samples{};std::atomic<unsigned> count{0};std::atomic<bool> on{false};};
void SDLCALL capture(int,void*p,int len,void*data){auto&c=*static_cast<Capture*>(data);if(!c.on.load())return;auto*s=static_cast<Sint16*>(p);auto n=c.count.load();for(int i=0;i<len/2&&n<c.samples.size();++i)c.samples[n++]=s[i]/32768.f;c.count.store(n);}
void SDLCALL capture_float(int,void*p,int len,void*data){auto&c=*static_cast<Capture*>(data);if(!c.on.load())return;auto*s=static_cast<float*>(p);auto n=c.count.load();for(int i=0;i<len/4&&n<c.samples.size();++i)c.samples[n++]=s[i];c.count.store(n);}
struct RMS {double l=0,r=0;double mean()const{return(l+r)/2;}};
RMS record(Capture&c){c.on=false;SDL_Delay(15);c.count=0;c.on=true;SDL_Delay(100);c.on=false;SDL_Delay(15);RMS r;const auto n=c.count.load();CHECK(n>1000);for(unsigned i=0;i+1<n;i+=2){r.l+=double(c.samples[i])*c.samples[i];r.r+=double(c.samples[i+1])*c.samples[i+1];}r.l=std::sqrt(r.l/(n/2));r.r=std::sqrt(r.r/(n/2));return r;}
void tone(const std::filesystem::path&p){std::ofstream f(p,std::ios::binary);auto u16=[&](unsigned v){char b[]={char(v),char(v>>8)};f.write(b,2);};auto u32=[&](unsigned v){char b[]={char(v),char(v>>8),char(v>>16),char(v>>24)};f.write(b,4);};f.write("RIFF",4);u32(36+44100*2*2);f.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(44100);u32(44100*4);u16(4);u16(16);f.write("data",4);u32(44100*4);for(unsigned i=0;i<44100*2;++i)u16(6553);CHECK(bool(f));}
}
int main(int argc,char**argv){CHECK(argc==3);SDL_GetMemoryFunctions(&original_malloc,&original_calloc,&original_realloc,&original_free);CHECK(SDL_SetMemoryFunctions(traced_malloc,traced_calloc,traced_realloc,traced_free)==0);CHECK(SDL_setenv("SDL_AUDIODRIVER","dummy",1)==0);CHECK(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0);CHECK(Mix_OpenAudio(44100,AUDIO_S16SYS,2,256)==0);CHECK(Mix_AllocateChannels(3)==3);Mix_SetPostMix(existing,nullptr);CHECK(world_atlas_init());
 std::array<Sint16,512> legacy_pcm{};legacy_pcm.fill(1000);Mix_Chunk legacy{0,reinterpret_cast<Uint8*>(legacy_pcm.data()),unsigned(legacy_pcm.size()*2),MIX_MAX_VOLUME};CHECK(Mix_PlayChannel(0,&legacy,-1)==0);Mix_Volume(0,0);
 char error[256]{};auto*a=world_demo_audio_create(argv[1],error,sizeof(error));CHECK(a&&world_demo_audio_stats(a).ready&&Mix_AllocateChannels(-1)==17&&world_demo_audio_stats(a).voice_capacity==14);
 // Simultaneous actual cinematic owner, permanent carrier channels and hooks.
 std::string ce;world_cinematics_audio::Options options;auto*c=world_cinematics_audio::create(options,ce);CHECK(c&&world_cinematics_audio::stats(c).first_channel==17);const int channels=Mix_AllocateChannels(-1);
 CHECK(!world_demo_audio_preload_thunder(a,"/no-such-optional-thunder.wav",error,sizeof(error))&&error[0]&&world_demo_audio_stats(a).ready&&!world_demo_audio_stats(a).thunder_ready);
 CHECK(world_demo_audio_preload_thunder(a,argv[2],error,sizeof(error)));const auto bytes=world_demo_audio_stats(a).thunder_bytes;CHECK(bytes>1000000&&bytes<4000000);
 char temporary[]="/tmp/valwood-thunder-Café-XXXXXX";CHECK(mkdtemp(temporary));auto dir=std::filesystem::u8path(temporary),path=dir/std::filesystem::u8path("Orage forêt.wav");tone(path);CHECK(world_demo_audio_preload_thunder(a,path.u8string().c_str(),error,sizeof(error)));CHECK(!world_demo_audio_preload_thunder(a,(dir/"missing.wav").u8string().c_str(),error,sizeof(error))&&world_demo_audio_stats(a).thunder_ready);
 // A tiny low-rate source with a large declared PCM payload must fail BEFORE
 // SDL's upsampling allocation. Sparse file payload makes the real bound cheap.
 auto oversized=dir/"oversized.wav";{std::ifstream in(path,std::ios::binary);std::array<char,44> header{};CHECK(bool(in.read(header.data(),44)));auto put=[&](unsigned at,unsigned v){for(unsigned i=0;i<4;++i)header[at+i]=char(v>>(8*i));};put(4,36+8*1024*1024);put(24,8000);put(28,32000);put(40,8*1024*1024);std::ofstream out(oversized,std::ios::binary);out.write(header.data(),44);out.seekp(44+8*1024*1024-1);out.put(0);CHECK(bool(out));}const auto prior_pcm=world_demo_audio_stats(a).thunder_bytes;CHECK(!world_demo_audio_preload_thunder(a,oversized.u8string().c_str(),error,sizeof(error))&&world_demo_audio_stats(a).thunder_bytes==prior_pcm&&world_demo_audio_stats(a).thunder_ready);
 auto cannot_steal=[&]{std::array<int,256> allocated{};unsigned n=0;int ch=0;while((ch=Mix_PlayChannel(-1,&legacy,-1))>=0){CHECK(n<allocated.size());CHECK(ch!=15&&ch!=16);allocated[n++]=ch;}CHECK(Mix_Playing(15)&&Mix_Playing(16));for(unsigned i=0;i<n;++i)Mix_HaltChannel(allocated[i]);};cannot_steal();
 WorldDemoAudioFrame frame;frame.dt=.01;CHECK(world_demo_audio_set_ambience_gain(a,0));CHECK(world_demo_audio_update(a,frame));Capture cap;CHECK(Mix_RegisterEffect(MIX_CHANNEL_POST,capture,nullptr,&cap));auto quiet=record(cap);CHECK(quiet.mean()<.0001);
 world_storm::Thunder e{1,100,0,0,.8f};CHECK(world_demo_audio_thunder(a,&e,1,frame));auto right=record(cap);cannot_steal();CHECK(right.r>right.l*2&&right.mean()>.02);const auto starts=world_demo_audio_stats(a).thunder_started;CHECK(world_demo_audio_thunder(a,&e,1,frame)&&world_demo_audio_stats(a).thunder_started==starts);
 frame.muted=true;CHECK(world_demo_audio_thunder(a,nullptr,0,frame));CHECK(record(cap).mean()<.0001);cannot_steal();frame.muted=false;e={3,-100,0,0,.8f};CHECK(world_demo_audio_thunder(a,&e,1,frame));auto left=record(cap);CHECK(left.l>left.r*2);
 // Distance scheduling can deliver an older serial AFTER a newer nearby strike.
 e.serial=2;e.x=0;CHECK(world_demo_audio_thunder(a,&e,1,frame));SDL_Delay(20);CHECK(world_demo_audio_stats(a).thunder_started==starts+2&&world_demo_audio_stats(a).active_thunder==2);e.serial=4;CHECK(world_demo_audio_thunder(a,&e,1,frame));SDL_Delay(20);CHECK(world_demo_audio_stats(a).thunder_replaced>0&&world_demo_audio_stats(a).active_thunder==2);
 const auto before=world_demo_audio_stats(a).thunder_started;tracking=true;for(unsigned i=0;i<1000;++i){++e.serial;CHECK(world_demo_audio_thunder(a,&e,1,frame));CHECK(world_demo_audio_update(a,frame));}tracking=false;std::fprintf(stderr,"HOT cpp=%u sdl=%u starts=%llu before=%llu ignored=%llu channels=%d expected=%d legacy=%d hooks=%u\n",allocations,sdl_allocations.load(),(unsigned long long)world_demo_audio_stats(a).thunder_started,(unsigned long long)before,(unsigned long long)world_demo_audio_stats(a).thunder_ignored,Mix_AllocateChannels(-1),channels,Mix_Playing(0),hooks.load());CHECK(allocations==0);CHECK(sdl_allocations.load()==0);CHECK(world_demo_audio_stats(a).thunder_started>before);CHECK(world_demo_audio_stats(a).thunder_started<=before+1000);CHECK(world_demo_audio_stats(a).thunder_ignored>0);CHECK(Mix_AllocateChannels(-1)==channels);CHECK(Mix_Playing(0)&&hooks.load()>0);
 e.volume=2;CHECK(!world_demo_audio_thunder(a,&e,1,frame));e.volume=.8;CHECK(!world_demo_audio_thunder(a,&e,9,frame));
 for(unsigned mode=0;mode<4;++mode){e.serial=2000+mode;CHECK(world_demo_audio_thunder(a,&e,1,frame));if(mode==0)frame.focused=false;if(mode==1)frame.paused=true;if(mode==2)frame.loading=true;if(mode==3)frame.muted=true;CHECK(world_demo_audio_update(a,frame));CHECK(record(cap).mean()<.0001&&world_demo_audio_stats(a).active_thunder==0);frame.focused=true;frame.paused=frame.loading=frame.muted=false;}
 const auto late_before=world_demo_audio_stats(a).thunder_started;e.serial=1500;CHECK(world_demo_audio_thunder(a,&e,1,frame)&&world_demo_audio_stats(a).thunder_started==late_before+1);CHECK(world_demo_audio_thunder(a,&e,1,frame)&&world_demo_audio_stats(a).thunder_started==late_before+1);
 e.serial=3000;CHECK(world_demo_audio_thunder(a,&e,1,frame));SDL_Delay(1100);CHECK(world_demo_audio_stats(a).active_thunder==0);cannot_steal();
 world_cinematics_audio::destroy(c);CHECK(Mix_Playing(0));world_demo_audio_free(a);CHECK(Mix_AllocateChannels(-1)==3&&Mix_Playing(0)&&Mix_QuerySpec(nullptr,nullptr,nullptr));CHECK(Mix_UnregisterEffect(MIX_CHANNEL_POST,capture));Mix_HaltChannel(0);Mix_SetPostMix(nullptr,nullptr);Mix_CloseAudio();
 // The same owned-channel callback handles real float32 device PCM too.
 CHECK(Mix_OpenAudio(48000,AUDIO_F32SYS,2,256)==0);CHECK(Mix_AllocateChannels(3)==3);a=world_demo_audio_create(argv[1],error,sizeof(error));CHECK(a&&world_demo_audio_preload_thunder(a,path.u8string().c_str(),error,sizeof(error)));CHECK(world_demo_audio_set_ambience_gain(a,0)&&world_demo_audio_update(a,frame));CHECK(Mix_RegisterEffect(MIX_CHANNEL_POST,capture_float,nullptr,&cap));e={1,0,0,0,.8f};CHECK(world_demo_audio_thunder(a,&e,1,frame));auto floating=record(cap);CHECK(floating.mean()>.045&&floating.mean()<.055&&std::abs(floating.l-floating.r)<.001);CHECK(Mix_UnregisterEffect(MIX_CHANNEL_POST,capture_float));world_demo_audio_free(a);Mix_CloseAudio();world_atlas_shutdown();SDL_Quit();CHECK(std::filesystem::remove_all(dir)==3);
 std::printf("THUNDER_RMS quiet=%.7f rightL=%.7f rightR=%.7f leftL=%.7f leftR=%.7f hot_allocations=%u SDL_hot_allocations=%u\nPASS %u storm audio checks: owned WAV/UTF8 optional rollback, actual panning/mute PCM,2voice bound/serial dedup/out-of-order delay, no hot allocations, existing cinematic/legacy channel+hook coexistence and teardown\n",quiet.mean(),right.l,right.r,left.l,left.r,allocations,sdl_allocations.load(),checks);
}
