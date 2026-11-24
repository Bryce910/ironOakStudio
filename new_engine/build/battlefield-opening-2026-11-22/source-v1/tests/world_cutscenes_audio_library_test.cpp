#include "engine/world_cinematics_audio.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <array>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
static std::atomic<long long> sum{0};static std::atomic<unsigned> samples{0},callbacks{0};static std::atomic<bool> capture{false};
static void SDLCALL hook(void*,Uint8*stream,int bytes){++callbacks;if(capture){long long s=0;const auto *pcm=reinterpret_cast<Sint16*>(stream);for(int i=0;i<bytes/2;++i)s+=pcm[i];sum+=s;samples+=bytes/2;}}
static void wav(const std::string&path,Sint16 value){
 std::ofstream f(path,std::ios::binary);auto w16=[&](unsigned x){const char b[]={char(x),char(x>>8)};f.write(b,2);};auto w32=[&](unsigned x){const char b[]={char(x),char(x>>8),char(x>>16),char(x>>24)};f.write(b,4);};
 const unsigned frames=12000;f.write("RIFF",4);w32(36+frames*4);f.write("WAVEfmt ",8);w32(16);w16(1);w16(2);w32(48000);w32(192000);w16(4);w16(16);f.write("data",4);w32(frames*4);for(unsigned i=0;i<frames*2;++i)w16(Uint16(value));
}
static double mean(){capture=false;SDL_Delay(12);sum=0;samples=0;capture=true;SDL_Delay(35);capture=false;SDL_Delay(12);CHECK(samples>1000);return double(sum.load())/samples.load();}
int main(){try{
 using namespace world_cinematics_audio;using namespace world_cinematics;std::string error;
 CHECK(SDL_setenv("SDL_AUDIODRIVER","dummy",1)==0&&SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0&&Mix_OpenAudio(48000,AUDIO_S16SYS,2,256)==0);CHECK(Mix_AllocateChannels(2)==2);
 std::array<Sint16,1024> zero={};Mix_Chunk existing{0,reinterpret_cast<Uint8*>(zero.data()),Uint32(sizeof(zero)),MIX_MAX_VOLUME};CHECK(Mix_PlayChannel(0,&existing,-1)==0);Mix_SetPostMix(hook,nullptr);
 const auto folder=std::filesystem::temp_directory_path()/("cutscene-union-"+std::to_string(getpid()));std::filesystem::create_directories(folder);
 wav((folder/"one.wav").string(),5000);wav((folder/"two.wav").string(),-5000);wav((folder/"world.wav").string(),1000);
 Options options;options.zone_voices=2;options.cue_voices=2;options.master=1;options.cue_send=0;auto *audio=create(options,error);CHECK(audio);
 std::vector<FileAsset> files;for(const auto *name:{"one.wav","two.wav","world.wav"})files.push_back({name,(folder/name).string()});CHECK(configure_library_files(audio,files,error)&&library_ready(audio));
 const auto bank=stats(audio);CHECK(bank.assets==3&&bank.decoded_bytes==3*48000u&&bank.first_channel==2);
 Document one;one.duration=2;Clip cue;cue.id=9;cue.asset="one.wav";cue.duration=2;cue.loop=true;cue.fade_in=cue.fade_out=0;cue.gain=.5f;one.clips.push_back(cue);Document two=one;two.clips[0].asset="two.wav";CHECK(prepared_document_valid(audio,one,error)&&prepared_document_valid(audio,two,error));
 auto invalid=one;invalid.clips[0].asset="absent.wav";CHECK(!prepared_document_valid(audio,invalid,error)&&stats(audio).generation==bank.generation);
 SDL_Delay(15);CHECK(collect(audio));auto invalid_files=files;invalid_files.push_back({"missing.wav",(folder/"missing.wav").string()});
 CHECK(!configure_library_files(audio,invalid_files,error)&&library_ready(audio)&&stats(audio).generation==bank.generation);
 CHECK(prepared_document_valid(audio,one,error)&&prepared_document_valid(audio,two,error));
 for(const auto&f:files)CHECK(std::filesystem::remove(f.path)); // No playback path can reopen these assets.
 world_soundscape::Emitter emitter;emitter.id=77;emitter.sound.asset="world.wav";emitter.sound.volume=.2f;emitter.sound.fade_in=emitter.sound.fade_out=0;emitter.sound.spatial=false;
 SoundscapeFrame world;world.dt=.016;CHECK(update_soundscape_prepared(audio,&emitter,1,world)==UpdateResult::Ok);const double world_before=mean();CHECK(world_before>180&&world_before<220&&stats(audio).soundscape_mode);
 CameraPose camera;Step step;step.time=.04;step.previous=0;step.jumped=true;CHECK(update_prepared_document(audio,one,step,camera,1)==UpdateResult::Ok);const double first=mean();CHECK(first>2400&&first<2600&&!stats(audio).soundscape_mode);
 step.paused=true;CHECK(update_prepared_document(audio,one,step,camera,1)==UpdateResult::Ok);const double paused=mean();CHECK(paused==0&&stats(audio).active_voices==0&&Mix_Playing(0));
 step.paused=false;step.jumped=true;CHECK(update_prepared_document(audio,one,step,camera,0)==UpdateResult::Ok);CHECK(mean()==0);
 CHECK(update_prepared_document(audio,two,step,camera,1)==UpdateResult::Ok);const double second=mean();CHECK(second< -2400&&second> -2600);CHECK(stats(audio).generation==bank.generation&&stats(audio).decoded_bytes==bank.decoded_bytes);
 CHECK(update_soundscape_prepared(audio,&emitter,1,world)==UpdateResult::Ok);const double world_after=mean();CHECK(world_after>180&&world_after<220&&stats(audio).soundscape_mode);
 SoundState sound;CHECK(world_cinematics_audio::sound(audio,77,sound)&&sound.playing);CHECK(Mix_Playing(0)&&callbacks>10);
 const auto before=callbacks.load();destroy(audio);SDL_Delay(15);CHECK(Mix_Playing(0)&&callbacks>before&&Mix_AllocateChannels(-1)==2);Mix_SetPostMix(nullptr,nullptr);Mix_HaltChannel(0);Mix_CloseAudio();SDL_Quit();std::filesystem::remove(folder);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"world_before_pcm\":"<<world_before<<",\"first_timeline_pcm\":"<<first<<",\"paused_pcm\":"<<paused<<",\"second_timeline_pcm\":"<<second<<",\"world_restored_pcm\":"<<world_after<<",\"one_bank_generation\":"<<bank.generation<<",\"decoded_bytes\":"<<bank.decoded_bytes<<",\"playback_files_deleted\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
