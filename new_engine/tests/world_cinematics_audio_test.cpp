#include "engine/world_cinematics_audio.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <unistd.h>
namespace {std::atomic<bool> tracking{false};std::atomic<unsigned> allocations{0};}
[[gnu::noinline]] void *operator new(std::size_t n){if(tracking.load())allocations++;if(auto *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void *operator new(std::size_t n,const std::nothrow_t &)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
[[gnu::noinline]] void operator delete(void *p,const std::nothrow_t &)noexcept{std::free(p);}
namespace {
using namespace world_cinematics_audio;
std::atomic<unsigned> legacy_hooks{0};
void SDLCALL legacy_hook(void *,Uint8 *,int){legacy_hooks++;}
struct Capture {std::array<float,131072> values={};std::atomic<unsigned> count{0};std::atomic<bool> recording{false};};
void SDLCALL capture(int,void *stream,int length,void *data){auto &c=*static_cast<Capture *>(data);if(!c.recording.load(std::memory_order_acquire))return;const auto *samples=static_cast<const Sint16 *>(stream);unsigned count=c.count.load(std::memory_order_relaxed);for(int i=0;i<length/2&&count<c.values.size();i++)c.values[count++]=samples[i]/32768.f;c.count.store(count,std::memory_order_release);}
template<class F>void wait(F f){const auto start=SDL_GetTicks();while(!f()){assert(SDL_GetTicks()-start<3000);SDL_Delay(1);}}
void buffers(Audio *a,unsigned n=2){const auto before=stats(a).callbacks;wait([&]{return stats(a).callbacks>=before+n;});}
bool near(double a,double b,double eps=1e-5){return std::abs(a-b)<eps;}
world_cinematics::Step step(double time){world_cinematics::Step s;s.time=s.previous=time;s.jumped=true;return s;}
Configuration config(){Configuration c;c.document.duration=4;world_cinematics::Zone z;z.id=10;z.asset="forest.wav";z.bus=1;z.extent={8,8,8};z.blend=2;z.gain=.8f;z.dry=.5f;z.reverb=.6f;c.document.zones.push_back(z);world_cinematics::Clip clip;clip.id=20;clip.asset="cue.wav";clip.bus=2;clip.start=.2;clip.duration=2;clip.source_offset=.01;clip.loop=true;clip.gain=.75f;clip.fade_in=clip.fade_out=0;c.document.clips.push_back(clip);world_cinematics::BusEnvelope e;e.bus=1;e.keys={{0,.5f,world_cinematics::Curve::Linear},{4,1.5f,world_cinematics::Curve::Linear}};c.document.envelopes.push_back(e);c.outside_asset="outside.wav";c.outside_gain=.25f;return c;}
void wav(const std::string &path,const std::vector<Sint16> &mono,int hz){std::ofstream f(path,std::ios::binary);auto u16=[&](unsigned v){char b[]={char(v),char(v>>8)};f.write(b,2);};auto u32=[&](unsigned v){char b[]={char(v),char(v>>8),char(v>>16),char(v>>24)};f.write(b,4);};const unsigned bytes=unsigned(mono.size()*2);f.write("RIFF",4);u32(36+bytes);f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(unsigned(hz));u32(unsigned(hz*2));u16(2);u16(16);f.write("data",4);u32(bytes);for(auto v:mono)u16(unsigned(Uint16(v)));}
}
int main(){
 using namespace world_cinematics;std::string error;
 assert(SDL_setenv("SDL_AUDIODRIVER","dummy",1)==0&&SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0);assert(Mix_OpenAudio(44100,AUDIO_S16SYS,2,256)==0);assert(Mix_AllocateChannels(3)==3);Mix_SetPostMix(legacy_hook,nullptr);
 std::vector<Sint16> legacy(512*2,500),pcm(4096*2);for(unsigned i=0;i<4096;i++){pcm[i*2]=Sint16(5000+int(i%1000));pcm[i*2+1]=Sint16(-2000-int(i%1000));}Mix_Chunk background{0,reinterpret_cast<Uint8 *>(legacy.data()),Uint32(legacy.size()*2),MIX_MAX_VOLUME};assert(Mix_PlayChannel(0,&background,-1)==0);
 Options options;options.zone_voices=2;options.cue_voices=2;options.pcm_budget=1024*1024;options.cue_send=.2f;auto *audio=create(options,error);if(!audio)std::cerr<<error<<'\n';assert(audio);assert(stats(audio).first_channel==3&&stats(audio).channels==5&&Mix_Playing(0));assert(!create(options,error));
 Capture capture_state;assert(Mix_RegisterEffect(MIX_CHANNEL_POST,capture,nullptr,&capture_state));
 auto c=config();std::vector<PcmAsset> assets={{"forest.wav",pcm.data(),pcm.size()*2},{"cue.wav",pcm.data(),pcm.size()*2},{"outside.wav",pcm.data(),pcm.size()*2}};assert(configure_pcm(audio,c,assets,error));CameraPose camera;auto s=step(.7);assert(update(audio,s,camera)==UpdateResult::Ok);buffers(audio);
 VoiceState zone,cue,outside;assert(voice(audio,0,zone)&&zone.active&&zone.id==10&&near(zone.dry_gain,.27)&&near(zone.reverb_send,.324));assert(voice(audio,2,cue)&&cue.active&&cue.id==20&&near(cue.dry_gain,.75)&&near(cue.reverb_send,.15));const std::uint64_t loop_start=441,seek=std::uint64_t(std::floor(.51*44100+1e-6));assert(cue.loop_start_frame==loop_start&&cue.start_frame==loop_start+(seek-loop_start)%(4096-loop_start));assert(cue.cursor_frame>=loop_start&&cue.cursor_frame<=4096);assert(voice(audio,4,outside)&&outside.active&&near(outside.dry_gain,0));assert(stats(audio).outside_gain==0&&Mix_Playing(0)&&legacy_hooks>0);
 // Failed partial loads or PCM preparation preserve the playing generation.
 const auto generation=stats(audio).generation,starts=stats(audio).starts;auto unaligned=assets;unaligned[1].bytes=3;assert(!configure_pcm(audio,c,unaligned,error)&&stats(audio).generation==generation);auto over=assets;over[0].bytes=options.pcm_budget+4;assert(!configure_pcm(audio,c,over,error)&&stats(audio).generation==generation);
 const auto path="/tmp/valwood-cinematic-audio-"+std::to_string(getpid())+".wav";std::vector<Sint16> mono(4096,8000);wav(path,mono,22050);auto files=c;files.files={{"forest.wav",path},{"cue.wav",path},{"outside.wav",path+"-missing"}};assert(!configure_files(audio,files,error)&&stats(audio).generation==generation);assert(voice(audio,2,cue)&&cue.active&&cue.id==20&&stats(audio).starts==starts);
 // Real outside/inside power blending and bus envelopes drive target gains.
 camera.position={100,0,0};s.jumped=false;assert(update(audio,s,camera)==UpdateResult::Ok);buffers(audio);assert(stats(audio).outside_gain==1&&voice(audio,0,zone)&&zone.dry_gain==0&&voice(audio,4,outside)&&near(outside.dry_gain,.25));
 camera.position={0,0,0};s=step(1.5);assert(update(audio,s,camera)==UpdateResult::Ok);buffers(audio);assert(voice(audio,0,zone)&&near(zone.dry_gain,.35)&&near(zone.reverb_send,.42));
 // Hot updates/callbacks use fixed buffers. Queue saturation is explicit and
 // bounded rather than allocating or silently dropping an accepted command.
 s.jumped=false;const auto allocations_before=allocations.load();tracking=true;unsigned full=0;for(unsigned i=0;i<10000;i++){const auto result=update(audio,s,camera);assert(result==UpdateResult::Ok||result==UpdateResult::QueueFull);full+=result==UpdateResult::QueueFull;}buffers(audio,3);tracking=false;assert(allocations==allocations_before&&full>0&&stats(audio).queue_full>=full);
 auto invalid=s;invalid.time=std::numeric_limits<double>::quiet_NaN();assert(update(audio,invalid,camera)==UpdateResult::InvalidFrame);auto bad_camera=camera;bad_camera.rotation.w=2;assert(update(audio,s,bad_camera)==UpdateResult::InvalidFrame);
 assert(stop_all(audio));buffers(audio);assert(stats(audio).active_voices==0&&Mix_Playing(0));const auto count_before=legacy_hooks.load();SDL_Delay(20);assert(legacy_hooks>count_before);
 // Actual WAV conversion into the current device format succeeds after all
 // references resolve; subsequent temporary input buffers can be destroyed.
 files.files[2].path=path;if(!configure_files(audio,files,error)){std::cerr<<"WAV configure: "<<error<<'\n';assert(false);}buffers(audio);assert(collect(audio)&&stats(audio).assets==3&&stats(audio).decoded_bytes>3*mono.size()*2);assert(update(audio,step(.7),camera)==UpdateResult::Ok);buffers(audio);
 // Render a short dry impulse through actual comb/allpass rings; the delayed
 // stereo tail remains audible after the source ends, then StopAll clears it.
 Mix_Volume(0,0);std::vector<Sint16> impulse(4410*2);for(unsigned i=0;i<32;i++)impulse[i*2]=impulse[i*2+1]=28000;Configuration reverb;reverb.document.duration=1;Clip hit;hit.id=31;hit.asset="impulse.wav";hit.start=0;hit.duration=.01;hit.gain=1;hit.fade_in=hit.fade_out=0;reverb.document.clips.push_back(hit);assert(configure_pcm(audio,reverb,{{"impulse.wav",impulse.data(),impulse.size()*2}},error));buffers(audio);capture_state.recording=false;SDL_Delay(20);capture_state.count=0;capture_state.recording=true;assert(update(audio,step(0),camera)==UpdateResult::Ok);SDL_Delay(250);capture_state.recording=false;SDL_Delay(20);const unsigned samples=capture_state.count.load(std::memory_order_acquire);assert(samples>16000);double dry_energy=0,tail_energy=0,stereo_difference=0;for(unsigned i=0;i+1<samples;i+=2){if(i<2000)dry_energy+=std::abs(capture_state.values[i]);if(i>4000&&i<16000){tail_energy+=std::abs(capture_state.values[i]);stereo_difference+=std::abs(capture_state.values[i]-capture_state.values[i+1]);}}assert(dry_energy>.001&&tail_energy>.001&&stereo_difference>.0001);
 assert(stop_all(audio));buffers(audio,3);capture_state.count=0;capture_state.recording=true;SDL_Delay(30);capture_state.recording=false;SDL_Delay(20);for(unsigned i=0;i<capture_state.count.load();i++)assert(capture_state.values[i]==0);
 // Cue PCM seeking is frame aligned, preserves the authored loop segment,
 // and ends at the timeline duration even without another render update.
 assert(configure_pcm(audio,c,assets,error));buffers(audio);assert(update(audio,step(2.19),camera)==UpdateResult::Ok);SDL_Delay(50);assert(voice(audio,2,cue)&&!cue.active);
 // Stop/destroy only touch owned channels and our registered effect. An audio
 // producer appended later and an existing postmix callback survive teardown.
 const int extra=Mix_AllocateChannels(-1);assert(Mix_AllocateChannels(extra+1)==extra+1&&Mix_PlayChannel(extra,&background,-1)==extra);destroy(audio);assert(Mix_Playing(0)&&Mix_Playing(extra)&&Mix_AllocateChannels(-1)==extra+1);assert(Mix_UnregisterEffect(MIX_CHANNEL_POST,capture));const auto hook_before=legacy_hooks.load();SDL_Delay(20);assert(legacy_hooks>hook_before);Mix_HaltChannel(extra);Mix_HaltChannel(0);Mix_AllocateChannels(3);
 // Destruction during actual callbacks and repeated bank replacement keep
 // wrappers/PCM alive until the posteffect is synchronously unregistered.
 for(unsigned iteration=0;iteration<8;iteration++){audio=create(options,error);assert(audio&&configure_pcm(audio,c,assets,error));assert(update(audio,step(.7),camera)==UpdateResult::Ok);buffers(audio,1);assert(configure_pcm(audio,reverb,{{"impulse.wav",impulse.data(),impulse.size()*2}},error));assert(update(audio,step(0),camera)==UpdateResult::Ok);destroy(audio);}
 Mix_SetPostMix(nullptr,nullptr);Mix_CloseAudio();assert(Mix_OpenAudio(16000,AUDIO_F32SYS,2,256)==0);audio=create(options,error);assert(audio);Configuration floating;floating.document.duration=1;Clip f;f.asset="float.wav";f.duration=.2;f.loop=true;f.fade_in=f.fade_out=0;floating.document.clips.push_back(f);std::vector<float> floats(1600*2,.2f);assert(configure_pcm(audio,floating,{{"float.wav",floats.data(),floats.size()*4}},error));assert(update(audio,step(.05),camera)==UpdateResult::Ok);buffers(audio);assert(voice(audio,2,cue)&&cue.active&&cue.start_frame==800);destroy(audio);Mix_CloseAudio();
 assert(Mix_OpenAudio(22050,AUDIO_U8,2,256)==0);assert(!create(options,error)&&!error.empty());Mix_CloseAudio();SDL_Quit();unlink(path.c_str());
 std::cout<<"Cinematic audio: real dummy-device PCM seek/loop timing, zone/outside/envelope gains, recorded Schroeder tail/stereo, bounded allocation-free hot updates, partial-load rollback, S16/F32 support, unsupported format rejection, owned-channel/hook isolation and callback-safe teardown passed\n";
}
