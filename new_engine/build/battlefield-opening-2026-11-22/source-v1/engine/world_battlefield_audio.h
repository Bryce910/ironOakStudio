#ifndef VALWOOD_WORLD_BATTLEFIELD_AUDIO_H
#define VALWOOD_WORLD_BATTLEFIELD_AUDIO_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

// Startup-only source admission; eight independent SDL_mixer channels. This
// module never opens/closes the common device or changes another owner's gain.
namespace world_battlefield_audio {
struct Point {double x=0,y=0,z=0;};
enum class Cue:unsigned {Slash,Clang,Arrow,Body,Stab,Death,Dash,Ambush,Count};
struct Frame {
 double dt=0,seconds=0;bool visible=false,paused=false,focused=true,loading=false;
 double listener[3]={},yaw=0;float volume=1; // caller's master*SFX, normalized0..1
};
struct Stats {unsigned assets=0,voices=0;std::uint64_t bytes=0,played=0,dropped=0,stopped=0,invalid=0,timeline_events=0;};
class System {
 static constexpr unsigned voice_count=8,asset_count=7;
 struct Voice {Point position;float gain=0;double distance=150;bool active=false;};
 std::array<Mix_Chunk*,asset_count> clips_={};std::array<Voice,voice_count> voices_={};
 int first_=-1;Frame frame_;Stats stats_;double previous_=-1;bool silent_=true;
 static bool finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
 static double smooth(double lo,double hi,double x){double t=std::clamp((x-lo)/(hi-lo),0.,1.);return t*t*(3-2*t);}
 static unsigned asset(Cue c){switch(c){case Cue::Slash:case Cue::Ambush:return 0;case Cue::Clang:return 1;case Cue::Body:case Cue::Stab:return 2;case Cue::Dash:return 3;case Cue::Arrow:return 4;case Cue::Death:return 6;default:return 0;}}
 void parameters(unsigned n){
  const auto&v=voices_[n];const double dx=v.position.x-frame_.listener[0],dy=v.position.y-frame_.listener[1],dz=v.position.z-frame_.listener[2],d=std::sqrt(dx*dx+dy*dy+dz*dz);
  const double fade=(1-smooth(v.distance*.65,v.distance,d))*std::min(1.,5./std::max(d,5.));
  const float gain=float(v.gain*fade)*frame_.volume;Mix_Volume(first_+int(n),int(std::lround(std::clamp(gain,0.f,1.f)*MIX_MAX_VOLUME)));
  const double lateral=std::hypot(dx,dz),pan=lateral<.01?0:std::clamp((dx*std::cos(frame_.yaw)+dz*std::sin(frame_.yaw))/lateral,-1.,1.);
  const double a=(pan*.72+1)*.7853981633974483;Mix_SetPanning(first_+int(n),Uint8(std::lround(std::cos(a)*255)),Uint8(std::lround(std::sin(a)*255)));
 }
 bool play(Cue cue,Point p,float gain,bool close){
  if(first_<0||silent_||frame_.volume<=0)return false;
  if(!finite(p)||!std::isfinite(gain)||gain<0||gain>4||unsigned(cue)>=unsigned(Cue::Count)){++stats_.invalid;return false;}
  const unsigned index=asset(cue);Mix_Chunk*clip=clips_[index];if(!clip)return false;
  unsigned slot=voice_count;const unsigned begin=close?0:2,end=close?2:voice_count;
  for(unsigned i=begin;i<end;++i)if(!Mix_Playing(first_+int(i))){slot=i;break;}
  if(slot==voice_count){++stats_.dropped;return false;}
  float level=close?.72f:.68f;if(cue==Cue::Clang)level=close?.72f:.52f;
  if(cue==Cue::Arrow)level=close?.50f:.58f;if(cue==Cue::Death)level=close?.50f:.32f;
  voices_[slot]={p,level*gain,close?60.:180.,true};parameters(slot);
  if(Mix_PlayChannel(first_+int(slot),clip,0)<0){voices_[slot].active=false;++stats_.dropped;return false;}
  ++stats_.played;return true;
 }
 void crossed(double now,double start,double period,Cue cue,Point p,float gain){
  if(previous_<0||now<start||now<=previous_)return;
  double at=start;
  if(period>0){const double n=std::floor((now-start)/period);at=start+std::max(n,0.)*period;}
  if(at>previous_&&at<=now){play(cue,p,gain,false);++stats_.timeline_events;}
 }
public:
 bool init(const char*project,const char*sfx,std::string&error){
  if(first_>=0)return true;error.clear();stats_=Stats{};int hz=0,c=0;Uint16 format=0;
  if(!Mix_QuerySpec(&hz,&format,&c)){error="Battlefield audio needs the existing open mixer";return false;}
  if(!project||!*project){error="Battlefield project audio root is required";return false;}
  const auto root=std::filesystem::path(project)/"audio/presentation";
  const std::array<std::filesystem::path,asset_count> paths={{root/"slash.wav",root/"parry.wav",root/"impact.wav",root/"dash.wav",std::filesystem::path(sfx?sfx:"")/"snd_arrow.wav",std::filesystem::path(sfx?sfx:"")/"snd_enemy_atk.wav",std::filesystem::path(sfx?sfx:"")/"snd_enemy_die.wav"}};
  for(unsigned i=0;i<asset_count;++i){
   if(i>=4&&(!sfx||!*sfx||!std::filesystem::is_regular_file(paths[i])))continue;
   clips_[i]=Mix_LoadWAV(paths[i].string().c_str());
   if(!clips_[i]&&i<4){error="Cannot load battlefield sound "+paths[i].string()+": "+Mix_GetError();release();return false;}
   if(clips_[i]){if(clips_[i]->alen>8u*1024u*1024u||stats_.bytes+clips_[i]->alen>24u*1024u*1024u){error="Battlefield decoded audio budget exceeded";release();return false;}stats_.bytes+=clips_[i]->alen;++stats_.assets;Mix_VolumeChunk(clips_[i],MIX_MAX_VOLUME);}
  }
  // Source fallbacks share pointers, with unique ownership checked on release.
  if(!clips_[4])clips_[4]=clips_[0];if(!clips_[5])clips_[5]=clips_[0];
  if(!clips_[6]){
   if(sfx&&*sfx){const auto p=std::filesystem::path(sfx)/"snd_death.wav";if(std::filesystem::is_regular_file(p)){clips_[6]=Mix_LoadWAV(p.string().c_str());if(clips_[6]){if(clips_[6]->alen>8u*1024u*1024u||stats_.bytes+clips_[6]->alen>24u*1024u*1024u){error="Battlefield death audio budget exceeded";release();return false;}stats_.bytes+=clips_[6]->alen;++stats_.assets;}}}
   if(!clips_[6])clips_[6]=clips_[2];
  }
  const int first=Mix_AllocateChannels(-1);if(first<0||first>248||Mix_AllocateChannels(first+int(voice_count))!=first+int(voice_count)){error="Cannot allocate battlefield audio voices";release();return false;}
  first_=first;stats_.voices=voice_count;for(unsigned i=0;i<voice_count;++i){Mix_Volume(first_+int(i),0);Mix_SetPanning(first_+int(i),255,255);}
  previous_=-1;silent_=true;std::printf("[Battlefield audio] %u assets, %llu decoded bytes, voices %d..%d\n",stats_.assets,(unsigned long long)stats_.bytes,first_,first_+int(voice_count)-1);return true;
 }
 void stop(){if(first_>=0)for(unsigned i=0;i<voice_count;++i){if(voices_[i].active){Mix_HaltChannel(first_+int(i));++stats_.stopped;}voices_[i].active=false;}}
 bool update(const Frame&f){
  if(first_<0)return false;
  if(!std::isfinite(f.dt)||f.dt<0||f.dt>1||!std::isfinite(f.seconds)||f.seconds<0||!finite({f.listener[0],f.listener[1],f.listener[2]})||!std::isfinite(f.yaw)||!std::isfinite(f.volume)||f.volume<0||f.volume>1){++stats_.invalid;stop();silent_=true;return false;}
  const bool blocked=!f.visible||f.paused||!f.focused||f.loading;frame_=f;
  if(blocked){stop();silent_=true;previous_=f.seconds;return true;}
  const bool newly_active=silent_;silent_=false;
  for(unsigned i=0;i<voice_count;++i)if(voices_[i].active){if(!Mix_Playing(first_+int(i)))voices_[i].active=false;else parameters(i);}
  // Resume/teleport does not replay missed combat. A hitch drops old events.
  if(newly_active||previous_<0||f.seconds<previous_||f.seconds-previous_>.35){previous_=f.seconds;return true;}
  static constexpr double pairs[3][2]={{-12103,5159},{-12091,5162},{-12061,5139}};
  for(unsigned i=0;i<3;++i){const Point p{pairs[i][0],12.5,pairs[i][1]};const double phase=-double(i)*.27*4.6;
   // Source attack phase .4 contact in each of the two .36-cycle exchanges.
   if(i==2||f.seconds<(i==0?8.6:18.4)){
    crossed(f.seconds,.6624+phase,4.6,Cue::Clang,p,1.f);crossed(f.seconds,3.0084+phase,4.6,Cue::Clang,p,.85f);
    crossed(f.seconds,.20+phase,4.6,Cue::Slash,p,.65f);crossed(f.seconds,2.55+phase,4.6,Cue::Slash,p,.60f);
   }
  }
  crossed(f.seconds,.42,4.2,Cue::Arrow,{-12129,12.8,5133},1.f);
  crossed(f.seconds,2.52,4.2,Cue::Arrow,{-12068,12.8,5132},1.f);
  crossed(f.seconds,1.18,0,Cue::Arrow,{-12129,12.8,5133},1.1f);
  crossed(f.seconds,2.4,0,Cue::Body,{-12109,12.6,5147},1.1f);
  crossed(f.seconds,2.46,0,Cue::Death,{-12109,12.6,5147},.85f);
  crossed(f.seconds,8.6,0,Cue::Body,{-12118,12.5,5138},.85f);
  crossed(f.seconds,18.4,0,Cue::Body,{-12091,12.5,5162},.85f);
  previous_=f.seconds;return true;
 }
 bool event(Cue cue,Point position,float gain=1){return play(cue,position,gain,true);}
 const Stats&stats()const{return stats_;}
 void release(){
  stop();if(first_>=0){if(Mix_AllocateChannels(-1)==first_+int(voice_count))Mix_AllocateChannels(first_);first_=-1;}
  for(unsigned i=0;i<clips_.size();++i)if(clips_[i]){auto*p=clips_[i];for(unsigned k=i;k<clips_.size();++k)if(clips_[k]==p)clips_[k]=nullptr;Mix_FreeChunk(p);}
  silent_=true;previous_=-1;stats_.voices=0;
 }
 // Explicit release before shared mixer shutdown. No destructor device calls.
};
}
#endif
