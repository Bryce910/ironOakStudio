#include "engine/world_cinematics_audio.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
namespace world_studio {struct Document{std::vector<world_soundscape::Emitter> emitters;};}
namespace world_authoring {static bool sound_emitters(const world_studio::Document&d,std::vector<world_soundscape::Emitter>&out,std::string&error){out=d.emitters;error.clear();return true;}}
struct Studio {world_cinematics_audio::Audio *cinematic_audio=nullptr;bool cinematic_audio_active=false,cinematic_audio_ready=false;std::string project_directory;};
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
static void wav(const std::string&path){std::ofstream f(path,std::ios::binary);auto w16=[&](unsigned x){const char b[]={char(x),char(x>>8)};f.write(b,2);};auto w32=[&](unsigned x){const char b[]={char(x),char(x>>8),char(x>>16),char(x>>24)};f.write(b,4);};f.write("RIFF",4);w32(36+4096*4);f.write("WAVEfmt ",8);w32(16);w16(1);w16(2);w32(48000);w32(192000);w16(4);w16(16);f.write("data",4);w32(4096*4);for(unsigned i=0;i<8192;++i)w16(500);}
int main(){try{
 CHECK(SDL_setenv("SDL_AUDIODRIVER","dummy",1)==0&&SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0&&Mix_OpenAudio(48000,AUDIO_S16SYS,2,256)==0);
 const auto folder=std::filesystem::temp_directory_path()/("cutscene-sound-apply-"+std::to_string(getpid()));std::filesystem::create_directories(folder);wav((folder/"voice.wav").string());wav((folder/"world.wav").string());
 Studio studio;studio.project_directory=folder.string();std::string error;studio.cinematic_audio=world_cinematics_audio::create({},error);CHECK(studio.cinematic_audio);
 CHECK(world_cinematics_audio::configure_library_files(studio.cinematic_audio,{{"voice.wav",(folder/"voice.wav").string()}},error));SDL_Delay(20);CHECK(world_cinematics_audio::collect(studio.cinematic_audio));
 const auto initial=world_cinematics_audio::stats(studio.cinematic_audio).generation;unsigned prepares=0;
 std::function<bool(std::string&)> cutscene_sound_reprepare;std::function<bool()> cutscene_sound_uses_library=[](){return true;};
 // Exact frozen production lambda extracted by the review producer. Only the
 // unrelated document→emitter adapter is replaced by a transparent test input.
 #include "../review/cutscenes-host/sound-prepare-under-test.inc"
 cutscene_sound_reprepare=[&](std::string&e){++prepares;if(!world_cinematics_audio::collect(studio.cinematic_audio)){e="busy";return false;}std::vector<world_cinematics_audio::FileAsset> files={{"voice.wav",(folder/"voice.wav").string()}};for(const auto&em:world_sound_emitters)files.push_back({em.sound.asset,(folder/em.sound.asset).string()});return world_cinematics_audio::configure_library_files(studio.cinematic_audio,files,e);};
 world_studio::Document document;world_soundscape::Emitter em;em.id=3;em.sound.asset="world.wav";document.emitters.push_back(em);
 CHECK(world_sound_prepare(document,error));CHECK(prepares==1&&world_sound_prepares==0);CHECK(world_cinematics_audio::stats(studio.cinematic_audio).generation==initial+1&&world_cinematics_audio::library_ready(studio.cinematic_audio));
 document.emitters[0].sound.volume=.24f;CHECK(world_sound_prepare(document,error));CHECK(prepares==1&&world_cinematics_audio::stats(studio.cinematic_audio).generation==initial+1);CHECK(world_sound_emitters[0].sound.volume==.24f);
 SDL_Delay(20);CHECK(world_cinematics_audio::collect(studio.cinematic_audio));document.emitters[0].sound.asset="missing.wav";
 CHECK(!world_sound_prepare(document,error)&&prepares==2);CHECK(world_sound_active&&world_sound_emitters.size()==1&&world_sound_emitters[0].sound.asset=="world.wav"&&world_sound_assets==std::vector<std::string>{"world.wav"});CHECK(world_cinematics_audio::library_ready(studio.cinematic_audio)&&world_cinematics_audio::stats(studio.cinematic_audio).generation==initial+1);
 world_cinematics_audio::destroy(studio.cinematic_audio);Mix_CloseAudio();SDL_Quit();std::filesystem::remove_all(folder);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"production_prepare_lambda\":true,\"single_union_switch\":true,\"same_asset_edit_no_reload\":true,\"failed_prepare_rollback\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
