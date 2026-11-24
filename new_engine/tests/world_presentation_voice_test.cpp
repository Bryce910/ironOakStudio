#include "engine/world_presentation_voice.h"
#include <SDL2/SDL.h>
#include <cassert>
#include <fstream>
#include <unistd.h>
#include <vector>
// A silent PCM fixture checks the playback contract. It is never a game asset.
static void wav(const std::filesystem::path &path){
 std::ofstream f(path,std::ios::binary);auto u16=[&](unsigned n){f.put(char(n));f.put(char(n>>8));};auto u32=[&](unsigned n){u16(n);u16(n>>16);};
 f.write("RIFF",4);u32(36+44100*2);f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(44100);u32(88200);u16(2);u16(16);f.write("data",4);u32(88200);std::vector<char> pcm(88200);f.write(pcm.data(),pcm.size());
}
int main(){
 assert(SDL_Init(SDL_INIT_AUDIO)==0);assert(Mix_OpenAudio(44100,AUDIO_S16SYS,2,512)==0);
 char dir[]="/tmp/valwood-dialogue-test-XXXXXX";assert(mkdtemp(dir));const auto root=std::filesystem::path(dir);const int channels=Mix_AllocateChannels(-1);
 WorldPresentationVoices missing;assert(missing.init(dir)&&missing.available==0&&missing.channel==-1);missing.update(730011,true,false,1);missing.release();assert(Mix_AllocateChannels(-1)==channels);
 wav(root/"730011.wav");wav(root/"730021.wav");std::ofstream(root/"730012.wav")<<"malformed fixture";
 WorldPresentationVoices voice;assert(voice.init(dir)&&voice.available==2&&voice.channel==channels);voice.update(730011,true,false,1);assert(voice.playbacks==1&&Mix_Playing(voice.channel));
 voice.update(730011,true,false,1);assert(voice.playbacks==1);voice.update(730011,true,true,1);assert(Mix_Paused(voice.channel));voice.update(730011,false,false,1);assert(Mix_Paused(voice.channel));voice.update(730011,true,false,1);assert(!Mix_Paused(voice.channel));
 voice.update(730021,true,false,.5f);assert(voice.playbacks==2&&Mix_Volume(voice.channel,-1)==50);voice.update(730012,true,false,1);assert(voice.playbacks==2&&!Mix_Playing(voice.channel));
 voice.update(730011,true,false,1);voice.update(0,true,false,1);assert(!Mix_Playing(voice.channel));voice.release();voice.release();assert(Mix_AllocateChannels(-1)==channels);
 std::filesystem::remove_all(root);Mix_CloseAudio();SDL_Quit();std::puts("PASS recorded dialogue: missing/corrupt fallback, preload, one playback per line, pause/focus, volume, stop and owned-channel cleanup");
}
