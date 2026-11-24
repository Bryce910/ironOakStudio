#include "engine/world_cinematics_audio.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <unistd.h>
namespace {std::atomic<bool> tracking{false};std::atomic<unsigned> allocations{0};unsigned checks=0;}
[[gnu::noinline]] void *operator new(std::size_t n){if(tracking.load())allocations++;if(auto *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL line"<<__LINE__<<": "<<#x<<'\n';std::abort();}}while(false)
namespace {
using namespace world_cinematics_audio;using world_soundscape::Emitter;
struct Capture {std::array<float,131072> values={};std::atomic<unsigned> count{0};std::atomic<bool> recording{false};};
std::atomic<unsigned> existing_hooks{0};
void SDLCALL existing(void *,Uint8 *,int){existing_hooks++;}
void SDLCALL capture(int,void *stream,int length,void *data){auto &c=*static_cast<Capture*>(data);if(!c.recording.load(std::memory_order_acquire))return;const auto *p=static_cast<const Sint16*>(stream);unsigned n=c.count.load(std::memory_order_relaxed);for(int i=0;i<length/2&&n<c.values.size();++i)c.values[n++]=p[i]/32768.f;c.count.store(n,std::memory_order_release);}
template<class F>void wait(F f){auto start=SDL_GetTicks();while(!f()){CHECK(SDL_GetTicks()-start<3000);SDL_Delay(1);}}
void buffers(Audio *a,unsigned n=2){auto before=stats(a).callbacks;wait([&]{return stats(a).callbacks>=before+n;});}
void pump(Audio *a,const std::vector<Emitter> &emitters,SoundscapeFrame &frame,unsigned milliseconds){for(unsigned i=0;i<milliseconds;i+=10){frame.dt=.01;const auto r=update_soundscape(a,emitters.data(),emitters.size(),frame);CHECK(r==UpdateResult::Ok||r==UpdateResult::QueueFull);SDL_Delay(10);}}
struct Rms {double left=0,right=0;unsigned frames=0;double mean()const{return(left+right)*.5;}};
Rms record(Audio *a,const std::vector<Emitter> &emitters,SoundscapeFrame &frame,Capture &c,unsigned ms=100){c.recording=false;SDL_Delay(15);c.count=0;c.recording=true;pump(a,emitters,frame,ms);c.recording=false;SDL_Delay(15);Rms r;const unsigned n=c.count.load(std::memory_order_acquire);for(unsigned i=0;i+1<n;i+=2){r.left+=double(c.values[i])*c.values[i];r.right+=double(c.values[i+1])*c.values[i+1];r.frames++;}CHECK(r.frames>256);r.left=std::sqrt(r.left/r.frames);r.right=std::sqrt(r.right/r.frames);return r;}
void wav(const std::filesystem::path &path,unsigned hz,unsigned frames,bool floating=false,unsigned channels=1){std::ofstream f(path,std::ios::binary);auto u16=[&](unsigned v){char p[]={char(v),char(v>>8)};f.write(p,2);};auto u32=[&](unsigned v){char p[]={char(v),char(v>>8),char(v>>16),char(v>>24)};f.write(p,4);};const unsigned width=floating?4:2,bytes=frames*channels*width;f.write("RIFF",4);u32(36+bytes);f.write("WAVEfmt ",8);u32(16);u16(floating?3:1);u16(channels);u32(hz);u32(hz*channels*width);u16(channels*width);u16(width*8);f.write("data",4);u32(bytes);for(unsigned i=0;i<frames*channels;++i){if(floating){float value=.2f;f.write(reinterpret_cast<const char*>(&value),4);}else u16(6553);}CHECK(bool(f));}
bool has_voice(Audio *a,std::uint64_t id,VoiceState *out=nullptr){for(unsigned i=0;i<stats(a).channels;++i){VoiceState v;if(voice(a,i,v)&&v.active&&v.id==id){if(out)*out=v;return true;}}return false;}
}
int main(){
 CHECK(SDL_setenv("SDL_AUDIODRIVER","dummy",1)==0);CHECK(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0);CHECK(Mix_OpenAudio(44100,AUDIO_S16SYS,2,256)==0);CHECK(Mix_AllocateChannels(3)==3);Mix_SetPostMix(existing,nullptr);
 std::vector<Sint16> base(512*2,1000);Mix_Chunk legacy{0,reinterpret_cast<Uint8*>(base.data()),Uint32(base.size()*2),MIX_MAX_VOLUME};CHECK(Mix_PlayChannel(0,&legacy,-1)==0);Mix_Volume(0,0);
 char temp_template[]="/tmp/valwood-world-sounds-Café-XXXXXX";const char *temp_created=mkdtemp(temp_template);CHECK(temp_created);const auto dir=std::filesystem::u8path(temp_created);const auto tone=dir/std::filesystem::u8path("Forêt long.wav"),short_file=dir/"short.wav",floating=dir/"float.wav",bad=dir/"bad.wav";wav(tone,22050,22050*4);wav(short_file,44100,4410);wav(floating,48000,48000,true,2);{std::ofstream f(bad);f<<"not a WAV";}
 std::string error;Options options;options.master=.5f;auto *a=create(options,error);CHECK(a);CHECK(stats(a).first_channel==3&&stats(a).channels==25);CHECK(!create(options,error));
 Emitter em;em.id=0x100000001ULL;em.sound.name="Water";em.sound.asset="audio_sources/forest.wav";em.sound.spatial=false;em.sound.fade_in=.2f;em.sound.fade_out=.2f;
 Emitter short_em=em;short_em.id=0x200000001ULL;short_em.sound.asset="audio_sources/short.wav";short_em.sound.loop=false;short_em.sound.enabled=false;
 Emitter float_em=em;float_em.id=3;float_em.sound.asset="audio_sources/float.wav";float_em.sound.enabled=false;
 SoundscapeConfiguration cfg;cfg.emitters={em,short_em,float_em};cfg.files={{em.sound.asset,tone.u8string()},{short_em.sound.asset,short_file.u8string()},{float_em.sound.asset,floating.u8string()}};
 CHECK(configure_soundscape_files(a,cfg,error));buffers(a);CHECK(stats(a).soundscape_mode&&stats(a).assets==3&&stats(a).soundscape_capacity==16&&stats(a).decoded_bytes<2*1024*1024);CHECK(collect(a));
 Capture captured;CHECK(Mix_RegisterEffect(MIX_CHANNEL_POST,capture,nullptr,&captured));SoundscapeFrame frame;std::vector<Emitter> live={em};frame.dt=.01;CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);SoundState state;CHECK(sound(a,em.id,state)&&state.gain>0&&state.gain<.1f);pump(a,live,frame,240);CHECK(sound(a,em.id,state)&&std::abs(state.gain-.6f)<1e-6&&state.cursor_frame>0);CHECK(has_voice(a,em.id));auto near=record(a,live,frame,captured);CHECK(near.mean()>.055&&near.mean()<.065&&std::abs(near.left-near.right)<.002);
 // Live weather conditions are continuous target gains, never asset reloads.
 live[0].condition_gain=.5f;pump(a,live,frame,230);auto conditioned=record(a,live,frame,captured);CHECK(conditioned.mean()>near.mean()*.45&&conditioned.mean()<near.mean()*.55&&stats(a).generation==1);
 live[0].condition_gain=0;pump(a,live,frame,230);CHECK(record(a,live,frame,captured).mean()<.0001);CHECK(sound(a,em.id,state)&&state.playing&&state.inside&&state.cursor_frame>0);
 frame.audition_id=em.id;frame.audition_serial=1;pump(a,live,frame,230);CHECK(record(a,live,frame,captured).mean()>near.mean()*.95);frame.audition_id=0;live[0].condition_gain=1;pump(a,live,frame,230);
 auto bad_condition=live;bad_condition[0].condition_gain=-.1f;CHECK(update_soundscape(a,bad_condition.data(),bad_condition.size(),frame)==UpdateResult::InvalidFrame);auto bad_cfg=cfg;bad_cfg.emitters[0].condition_gain=std::numeric_limits<float>::quiet_NaN();CHECK(!configure_soundscape_files(a,bad_cfg,error));
 // Independent finite fade-out and fade-in use seconds, rather than radius feather.
 live[0].sound.enabled=false;auto falling=record(a,live,frame,captured,100);CHECK(falling.mean()>near.mean()*.55&&falling.mean()<near.mean()*.9);CHECK(sound(a,em.id,state)&&state.gain>.20f&&state.gain<.40f);pump(a,live,frame,130);CHECK(sound(a,em.id,state)&&state.gain==0);CHECK(record(a,live,frame,captured).mean()<.0001);live[0].sound.enabled=true;auto rising=record(a,live,frame,captured,100);CHECK(rising.mean()>near.mean()*.1&&rising.mean()<near.mean()*.6);pump(a,live,frame,140);CHECK(sound(a,em.id,state)&&state.gain==.6f);
 // True3D attenuation and actual stereo orientation, with live same-bank edits.
 live[0].sound.spatial=true;live[0].sound.x=4;live[0].sound.inner_radius=5;live[0].sound.radius=10;pump(a,live,frame,240);auto right=record(a,live,frame,captured);CHECK(right.right>right.left*2);
 live[0].sound.x=-4;pump(a,live,frame,240);auto left=record(a,live,frame,captured);CHECK(left.left>left.right*2);
 frame.listener.position={0,20,0};pump(a,live,frame,240);CHECK(record(a,live,frame,captured).mean()<.0001);CHECK(stats(a).outside_gain==1);
 frame.listener.position={0,0,0};live[0].sound.x=0;live[0].sound.inner_radius=2;live[0].sound.radius=10;frame.listener.position.y=6;pump(a,live,frame,240);CHECK(sound(a,em.id,state)&&std::abs(state.target_gain-.3f)<1e-5);auto half=record(a,live,frame,captured);CHECK(half.mean()>near.mean()*.45&&half.mean()<near.mean()*.55);
 frame.listener.position={0,0,0};live[0].sound.spatial=false;pump(a,live,frame,230);
 // Invalid hot frames never allocate, decode, or replace a playing generation.
 const auto generation=stats(a).generation;auto invalid=live;invalid[0].sound.radius=-1;auto unprepared=live;unprepared[0].sound.asset="audio_sources/not-prepared.wav";auto bad_frame=frame;bad_frame.listener.rotation.w=2;const auto allocations_before=allocations.load();tracking=true;CHECK(update_soundscape(a,invalid.data(),invalid.size(),frame)==UpdateResult::InvalidFrame);CHECK(update_soundscape(a,unprepared.data(),unprepared.size(),frame)==UpdateResult::InvalidFrame);CHECK(update_soundscape(a,live.data(),live.size(),bad_frame)==UpdateResult::InvalidFrame);for(unsigned i=0;i<10000;++i){const auto result=update_soundscape(a,live.data(),live.size(),frame);CHECK(result==UpdateResult::Ok||result==UpdateResult::QueueFull);}buffers(a,3);tracking=false;CHECK(allocations.load()==allocations_before&&stats(a).queue_full>0&&stats(a).generation==generation&&has_voice(a,em.id));
 auto failure=cfg;failure.files[0].path=bad.u8string();CHECK(!configure_soundscape_files(a,failure,error)&&!error.empty()&&stats(a).generation==generation);failure=cfg;failure.files[0].path=(dir/"missing.wav").u8string();CHECK(!configure_soundscape_files(a,failure,error)&&stats(a).generation==generation&&has_voice(a,em.id));
 failure=cfg;failure.emitters.resize(65);CHECK(!configure_soundscape_files(a,failure,error)&&stats(a).generation==generation);failure=cfg;failure.files.resize(33);CHECK(!configure_soundscape_files(a,failure,error)&&stats(a).generation==generation);
 //64 logical IDs, exactly16 audible voices, deterministic slots and continuing
 // virtual loop cursors when a previously distant/lower-priority ID is selected.
 live.clear();for(unsigned i=1;i<=64;++i){auto v=em;v.id=i;v.sound.volume=.2f;v.sound.fade_in=v.sound.fade_out=0;live.push_back(v);}pump(a,live,frame,300);CHECK(stats(a).authored_emitters==64&&stats(a).active_voices==16&&stats(a).virtualized_emitters==48);CHECK(sound(a,64,state)&&state.virtualized&&state.cursor_frame>0);const auto virtual_cursor=state.cursor_frame;live[63].sound.volume=1;CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);buffers(a);VoiceState actual;CHECK(has_voice(a,64,&actual)&&actual.cursor_frame>=virtual_cursor&&actual.id==64);const auto starts=stats(a).starts;std::reverse(live.begin(),live.end());CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);buffers(a);CHECK(stats(a).starts==starts);
 // Removed audible nodes get a bounded tail; new nodes cannot exceed16 voices.
 for(auto &v:live)if(v.id==64)v.sound.fade_out=.15f;CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);live.erase(std::remove_if(live.begin(),live.end(),[](const auto &v){return v.id==64;}),live.end());CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);CHECK(sound(a,64,state)&&!state.present&&state.gain>0);pump(a,live,frame,200);CHECK(!sound(a,64,state));CHECK(stats(a).active_voices<=16);
 // Real one-shot starts once per entry, not each frame or after it completes.
 live={short_em};live[0].sound.enabled=true;live[0].sound.fade_in=live[0].sound.fade_out=0;pump(a,live,frame,250);CHECK(sound(a,short_em.id,state)&&!state.playing);const auto one_shot_starts=stats(a).starts;pump(a,live,frame,160);CHECK(stats(a).starts==one_shot_starts);frame.listener.position={100,0,0};live[0].sound.spatial=true;pump(a,live,frame,30);frame.listener.position={0,0,0};CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);buffers(a);CHECK(has_voice(a,short_em.id)&&stats(a).starts>one_shot_starts);
 // A weather gate neither re-enters nor restarts a completed one-shot.
 pump(a,live,frame,220);CHECK(sound(a,short_em.id,state)&&!state.playing);const auto completed_starts=stats(a).starts;for(unsigned i=0;i<4;++i){live[0].condition_gain=i%2?1.f:0.f;pump(a,live,frame,80);}CHECK(stats(a).starts==completed_starts);live[0].condition_gain=1;
 // Focus, pause, loading and editor mute are immediate; logical phase freezes
 // and resumes with authored fade-in instead of silently restarting loops.
 live={em};pump(a,live,frame,240);for(unsigned context=0;context<4;++context){if(context==0)frame.focused=false;if(context==1)frame.paused=true;if(context==2)frame.loading=true;if(context==3)frame.muted=true;CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);buffers(a);CHECK(stats(a).active_voices==0);if(context==0)CHECK(record(a,live,frame,captured,60).mean()<.0001);CHECK(sound(a,em.id,state));const auto frozen=state.cursor_frame;pump(a,live,frame,80);CHECK(sound(a,em.id,state)&&state.cursor_frame==frozen&&state.gain==0);frame.focused=true;frame.paused=frame.loading=frame.muted=false;CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);CHECK(sound(a,em.id,state)&&state.cursor_frame==frozen&&state.gain<.1f);pump(a,live,frame,220);}
 // Audition bypasses world distance and isolates/centers the selected source.
 live={short_em};live[0].sound.enabled=true;live[0].sound.spatial=true;live[0].sound.fade_in=live[0].sound.fade_out=0;frame.listener.position={100,0,0};pump(a,live,frame,30);frame.audition_id=short_em.id;frame.audition_serial=1;CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);buffers(a);CHECK(has_voice(a,short_em.id));pump(a,live,frame,150);CHECK(sound(a,short_em.id,state)&&!state.playing);const auto ended=stats(a).starts;pump(a,live,frame,50);CHECK(stats(a).starts==ended);++frame.audition_serial;CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);buffers(a);CHECK(has_voice(a,short_em.id)&&stats(a).starts>ended);frame.audition_id=0;
 // Overlapping map spots add; they do not replace the outside ambience bus.
 frame.listener.position={0,0,0};live={em};live[0].sound.volume=.2f;live[0].sound.fade_in=live[0].sound.fade_out=0;pump(a,live,frame,60);auto single=record(a,live,frame,captured);auto twice=live[0];twice.id=100;live.push_back(twice);pump(a,live,frame,60);auto two=record(a,live,frame,captured);CHECK(two.mean()>single.mean()*1.9&&two.mean()<single.mean()*2.1&&stats(a).outside_gain==1);
 // Same singleton can switch back to cinema; legacy hooks/channels survive.
 Configuration cinema;cinema.document.duration=1;world_cinematics::Zone zone;zone.asset="cinema.wav";zone.gain=.2f;zone.reverb=0;cinema.document.zones.push_back(zone);std::vector<Sint16> pcm(4410*2,4000);CHECK(configure_pcm(a,cinema,{{"cinema.wav",pcm.data(),pcm.size()*2}},error));buffers(a);CHECK(collect(a)&&!stats(a).soundscape_mode);CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::NotConfigured);world_cinematics::Step step;step.time=.1;CHECK(update(a,step,frame.listener)==UpdateResult::Ok);buffers(a);CHECK(stats(a).active_voices==1&&Mix_Playing(0)&&existing_hooks>0);
 CHECK(configure_soundscape_files(a,cfg,error));buffers(a);CHECK(update(a,step,frame.listener)==UpdateResult::NotConfigured);CHECK(update_soundscape(a,live.data(),live.size(),frame)==UpdateResult::Ok);buffers(a);const int extra=Mix_AllocateChannels(-1);CHECK(Mix_AllocateChannels(extra+1)==extra+1&&Mix_PlayChannel(extra,&legacy,-1)==extra);destroy(a);CHECK(Mix_Playing(0)&&Mix_Playing(extra)&&Mix_AllocateChannels(-1)==extra+1);CHECK(Mix_UnregisterEffect(MIX_CHANNEL_POST,capture));const auto old_hook=existing_hooks.load();SDL_Delay(20);CHECK(existing_hooks>old_hook);Mix_HaltChannel(extra);Mix_HaltChannel(0);Mix_AllocateChannels(3);
 Options tiny;tiny.pcm_budget=4096;a=create(tiny,error);CHECK(a&&!configure_soundscape_files(a,cfg,error)&&!error.empty());destroy(a);Mix_SetPostMix(nullptr,nullptr);Mix_CloseAudio();SDL_Quit();CHECK(std::filesystem::remove_all(dir)>=5);
 std::cout<<"MIXER_RMS near="<<near.mean()<<" fade_out_half="<<falling.mean()<<" fade_in_half="<<rising.mean()<<" right_L="<<right.left<<" right_R="<<right.right<<" left_L="<<left.left<<" left_R="<<left.right<<" half_distance="<<half.mean()<<" single="<<single.mean()<<" additive_two="<<two.mean()<<" hot_allocations="<<allocations.load()-allocations_before<<"\n";
 std::cout<<"PASS "<<checks<<" world sound mixer checks: actual WAV/UTF8 conversion, stereo RMS/distance/additive/fades/mute,64logical/16voice virtualization/cursors, entry-only shots/audition, allocation-free updates/queue saturation, transactional rollback, singleton cinema reuse and legacy isolation\n";
}
