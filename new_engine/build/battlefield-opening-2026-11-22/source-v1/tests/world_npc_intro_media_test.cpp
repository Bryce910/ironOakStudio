#define STB_IMAGE_IMPLEMENTATION
#define VALWOOD_NPC_INTRO_MEDIA_TESTING
#include "engine/world_npc_intro_media.h"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
using namespace world_npc_intro_media;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
static std::atomic<unsigned> legacy_callbacks{0};
static std::atomic<bool> capture_audio{false};static std::atomic<int> captured_peak{0};
static void SDLCALL legacy_hook(void*,Uint8 *stream,int bytes){
 ++legacy_callbacks;if(!capture_audio.load())return;
 const auto *pcm=reinterpret_cast<const Sint16*>(stream);int peak=0;
 for(int i=0;i<bytes/2;++i)peak=std::max(peak,std::abs(int(pcm[i])));
 auto old=captured_peak.load();while(old<peak&&!captured_peak.compare_exchange_weak(old,peak)){}
}
template<class F>static void wait(F predicate){const auto start=SDL_GetTicks();while(!predicate()){CHECK(SDL_GetTicks()-start<3000);SDL_Delay(1);}}
static Frame frame(Media &media){Frame f;wait([&]{return media.take_frame(f)||media.failed();});CHECK(!media.failed());return f;}
int main(int argc,char **argv){
 try{
  if(argc==3&&std::string(argv[1])=="--inspect"){
   Media probe;std::string message;if(!probe.load(argv[2],message)){std::cerr<<message<<'\n';return 2;}
   if(probe.mode()==Mode::Video)for(unsigned i=0;i<=60;++i){probe.seek(probe.duration()*i/60.);auto item=frame(probe);CHECK(item.rgba.size()>0);}
   if(!probe.has_audio()){probe.play();probe.update(0,false,1);CHECK(probe.ready()&&probe.stats().audio_channel==-1);}
   std::cout<<"{\"ready\":true}\n";return 0;
  }
  CHECK(argc==4);std::string error;Media media;
  // Loading precedes the host mixer initialization and does not need a device.
  CHECK(media.load(argv[1],error));CHECK(media.mode()==Mode::Video);CHECK(media.ready());CHECK(media.stats().audio_channel==-1);
  CHECK(std::abs(media.duration()-1.25)<.002);CHECK(media.has_subtitles());CHECK(!media.subtitle(.01));CHECK(std::string(media.subtitle(.3))=="A supplied line.\nSecond line.");CHECK(!media.subtitle(1.2));
  auto first=frame(media);CHECK(first.index==0&&first.width==320&&first.height==180&&first.rgba.size()==320u*180u*4);
  const auto letter=letterbox(1000,1000,first.width,first.height);CHECK(letter.width==1000&&std::abs(letter.height-562.5)<.001&&std::abs(letter.y-218.75)<.001);
  CHECK(SDL_setenv("SDL_AUDIODRIVER","dummy",1)==0);CHECK(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0);CHECK(Mix_OpenAudio(48000,AUDIO_S16SYS,2,256)==0);CHECK(Mix_AllocateChannels(2)==2);
  std::array<Sint16,1024> legacy={};Mix_Chunk background={0,reinterpret_cast<Uint8*>(legacy.data()),Uint32(sizeof(legacy)),MIX_MAX_VOLUME};CHECK(Mix_PlayChannel(0,&background,-1)==0);Mix_SetPostMix(legacy_hook,nullptr);
  media.play();CHECK(media.ready()&&media.stats().audio_channel==2&&Mix_Playing(0));
  auto wall=SDL_GetTicks();double time=0;unsigned video_frames=1;std::size_t max_queued=0;
  while((time=double(SDL_GetTicks()-wall)/1000.)<.42){media.update(time,false,.7f);Frame next;if(media.take_frame(next)){CHECK(next.pts<=time+1e-7);++video_frames;}max_queued=std::max(max_queued,media.stats().queued_frames);SDL_Delay(3);}
  CHECK(media.ready()&&video_frames>=3&&media.stats().audio_callbacks>10&&legacy_callbacks>10);CHECK(max_queued<=2);
  media.update(time,true,.7f);SDL_Delay(15);const auto paused=media.stats().audio_frames;SDL_Delay(40);CHECK(media.stats().audio_frames==paused);
  media.update(time,false,.7f);SDL_Delay(25);CHECK(media.stats().audio_frames>paused);
  media.seek(.8);auto sought=frame(media);CHECK(sought.index==9);media.update(.8,true,0);CHECK(media.ready());
  media.seek(.1);auto reverse=frame(media);CHECK(reverse.index==1);
  media.stop();SDL_Delay(15);const auto stopped=media.stats().audio_frames;SDL_Delay(20);CHECK(media.stats().audio_frames==stopped&&Mix_Playing(0));
  const auto stats=media.stats();CHECK(stats.maximum_audio_drift_seconds<.040&&stats.audio_resyncs==0);
  media.unload();CHECK(Mix_Playing(0)&&Mix_AllocateChannels(-1)==2&&legacy_callbacks>0);
  CHECK(media.load(argv[1],error));media.testing_decode_delay(120);media.play();wall=SDL_GetTicks();unsigned slow_frames=0;double max_age=0;
  while((time=double(SDL_GetTicks()-wall)/1000.)<1.10){media.update(time,false,.5f);Frame next;if(media.take_frame(next)){CHECK(next.pts<=time+1e-7);max_age=std::max(max_age,time-next.pts);++slow_frames;}CHECK(media.stats().queued_frames<=2);SDL_Delay(4);}
  CHECK(media.ready()&&slow_frames>=5&&max_age<=.5);media.testing_decode_delay(0);media.stop();media.play(0);media.update(0,true,0);auto restarted=frame(media);CHECK(restarted.index==0);media.unload();
  CHECK(media.load(argv[2],error)&&media.mode()==Mode::Voice);media.play(.2);media.update(.2,false,.4f);SDL_Delay(20);CHECK(media.stats().audio_frames>=9600);CHECK(!media.take_frame(first));
  media.unload();CHECK(Mix_Playing(0)&&Mix_AllocateChannels(-1)==2);
  CHECK(media.load(argv[2],error));captured_peak=0;capture_audio=true;media.play(.4);SDL_Delay(30);CHECK(captured_peak==0);
  media.update(.4,false,0);SDL_Delay(25);CHECK(captured_peak==0);
  media.update(.425,false,1);SDL_Delay(30);capture_audio=false;CHECK(captured_peak>100);media.unload();CHECK(Mix_Playing(0));
  // A foreign explicit replacement is detected without halting that sound.
  CHECK(media.load(argv[2],error));media.play();const auto own=media.stats().audio_channel;CHECK(Mix_PlayChannel(own,&background,-1)==own);media.update(0,false,1);CHECK(media.failed());media.unload();CHECK(Mix_Playing(own)&&Mix_GetChunk(own)==&background);Mix_HaltChannel(own);Mix_AllocateChannels(2);
  CHECK(!media.load(argv[3],error)&&!media.ready()&&!error.empty());
  CHECK(!detail::relative("../outside")&&!detail::relative("safe/../../outside")&&!detail::relative("safe\\outside")&&!detail::relative("/absolute")&&detail::relative("media-123/audio.wav"));
  Mix_HaltChannel(0);Mix_SetPostMix(nullptr,nullptr);Mix_CloseAudio();SDL_Quit();
  std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"immediate_play_muted_until_update\":true,\"zero_gain_pcm_silent\":true,\"audible_pcm_after_gain_update\":true,\"video_frames\":"<<video_frames<<",\"maximum_queued_frames\":"<<max_queued<<",\"maximum_audio_drift_seconds\":"<<stats.maximum_audio_drift_seconds<<",\"resyncs\":"<<stats.audio_resyncs<<",\"slow_decode_delay_ms\":120,\"slow_decode_displayed_frames\":"<<slow_frames<<",\"slow_decode_maximum_age_seconds\":"<<max_age<<"}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
