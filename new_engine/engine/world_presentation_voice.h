#ifndef VALWOOD_WORLD_PRESENTATION_VOICE_H
#define VALWOOD_WORLD_PRESENTATION_VOICE_H
#include <SDL2/SDL_mixer.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
// Optional human recordings: decoded at loading, never on a dialogue/UI frame.
// Missing recordings retain readable dialogue. No synthetic voice is substituted.
struct WorldPresentationVoices {
 static constexpr std::array<std::uint64_t,4> ids={730011,730012,730013,730021};
 std::array<Mix_Chunk*,4> clips={};int channel=-1;unsigned available=0,playbacks=0;std::uint64_t last=0;
 bool init(const char *directory="audio/presentation/dialogue"){
  if(!Mix_QuerySpec(nullptr,nullptr,nullptr))return false;
  std::uint64_t bytes=0;for(unsigned i=0;i<ids.size();++i){const auto p=std::filesystem::path(directory)/(std::to_string(ids[i])+".wav");std::error_code error;if(!std::filesystem::is_regular_file(p,error))continue;const auto size=std::filesystem::file_size(p,error);if(error||size>8*1024*1024)continue;auto *clip=Mix_LoadWAV(p.u8string().c_str());if(!clip)continue;if(bytes+clip->alen>32*1024*1024){Mix_FreeChunk(clip);continue;}clips[i]=clip;bytes+=clip->alen;available++;}
  if(!available)return true;
  channel=Mix_AllocateChannels(-1);if(channel<0||channel>120||Mix_AllocateChannels(channel+1)!=channel+1){channel=-1;release();return false;}return true;
 }
 void update(std::uint64_t id,bool audible,bool paused,float volume){
  if(channel<0)return;
  if(id!=last){Mix_HaltChannel(channel);last=id;for(unsigned i=0;i<ids.size();++i)if(ids[i]==id&&clips[i]){if(Mix_PlayChannel(channel,clips[i],0)>=0)playbacks++;break;}}
  Mix_Volume(channel,int(100*std::max(0.f,std::min(1.f,volume))));
  if(!audible||paused){if(Mix_Playing(channel))Mix_Pause(channel);}else if(Mix_Paused(channel))Mix_Resume(channel);
 }
 void release(){if(channel>=0){Mix_HaltChannel(channel);if(Mix_AllocateChannels(-1)==channel+1)Mix_AllocateChannels(channel);channel=-1;}for(auto *&c:clips){if(c)Mix_FreeChunk(c);c=nullptr;}available=0;last=0;}
};
#endif
