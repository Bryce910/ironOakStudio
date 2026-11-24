#include "engine/world_cutscenes_host.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <fstream>
#include <unistd.h>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
int main(){try{
 using world_cinematics::Vec3;using world_cutscenes_host::interaction_visible;
 const Vec3 eye{0,1.75,0},feet{0,0,-2};unsigned casts=0;bool closed=true;
 // A closed door at z=-1 blocks the exact eye-to-chest ray. Opening it permits interaction.
 auto scene=[&](Vec3 a,Vec3 b){++casts;CHECK(a.x==0&&a.y==1.75&&a.z==0);CHECK(b.x==0&&b.y==1.35&&b.z==-2);
  const double t=(-1-a.z)/(b.z-a.z),y=a.y+(b.y-a.y)*t;return !(closed&&t>=0&&t<=1&&y>=0&&y<=2.2);};
 CHECK(!interaction_visible(eye,0,feet,3,scene)&&casts==1);closed=false;CHECK(interaction_visible(eye,0,feet,3,scene)&&casts==2);
 auto clear=[&](Vec3,Vec3){++casts;return true;};auto occluded=[](Vec3,Vec3){return false;};
 CHECK(!interaction_visible(eye,0,feet,3,occluded));
 CHECK(!interaction_visible(eye,3.141592653589793,feet,3,clear));CHECK(casts==2);
 CHECK(!interaction_visible(eye,0,{0,2.5,-2},3,clear));CHECK(casts==2);
 CHECK(!interaction_visible(eye,0,{0,-2.5,-2},3,clear));CHECK(casts==2);
 CHECK(!interaction_visible(eye,0,{0,0,-9},100,clear));CHECK(casts==2);
 CHECK(!interaction_visible(eye,0,{0,0,-.6},.01,clear));CHECK(casts==2);
 CHECK(interaction_visible(eye,0,{0,0,-.4},.01,clear));CHECK(casts==3);
 CHECK(interaction_visible(eye,0,{0,0,-8},100,clear));CHECK(casts==4);
 CHECK(interaction_visible(eye,3.141592653589793,{0,0,-.1},3,clear));CHECK(casts==5);
 for(unsigned i=0;i<8;++i){Vec3 e=eye,f=feet;double yaw=0,radius=3;const double invalid=std::numeric_limits<double>::quiet_NaN();
  if(i==0)e.x=invalid;else if(i==1)e.y=invalid;else if(i==2)e.z=invalid;else if(i==3)f.x=invalid;else if(i==4)f.y=invalid;else if(i==5)f.z=invalid;else if(i==6)yaw=invalid;else radius=invalid;
  CHECK(!interaction_visible(e,yaw,f,radius,clear));CHECK(casts==5);
 }
 CHECK(!interaction_visible(eye,std::numeric_limits<double>::infinity(),feet,3,clear));
 world_cutscenes_host::InteractionContext gate;gate.library_ready=gate.audio_ready=gate.progress_ready=true;CHECK(world_cutscenes_host::interaction_allowed(gate));
 bool world_cutscenes_host::InteractionContext::*blocking[]={&world_cutscenes_host::InteractionContext::editor,&world_cutscenes_host::InteractionContext::locked,&world_cutscenes_host::InteractionContext::return_neutral,&world_cutscenes_host::InteractionContext::opening_priority,&world_cutscenes_host::InteractionContext::loading,&world_cutscenes_host::InteractionContext::graphics,&world_cutscenes_host::InteractionContext::map,&world_cutscenes_host::InteractionContext::inventory,&world_cutscenes_host::InteractionContext::keyboard_owned,&world_cutscenes_host::InteractionContext::opening_dialogue,&world_cutscenes_host::InteractionContext::component_dialogue,&world_cutscenes_host::InteractionContext::journey_dialogue,&world_cutscenes_host::InteractionContext::camera_preview,&world_cutscenes_host::InteractionContext::production_freeze,&world_cutscenes_host::InteractionContext::flying,&world_cutscenes_host::InteractionContext::fighting,&world_cutscenes_host::InteractionContext::menu};
 for(auto member:blocking){gate.*member=true;CHECK(!world_cutscenes_host::interaction_allowed(gate));gate.*member=false;}
 for(auto member:{&world_cutscenes_host::InteractionContext::library_ready,&world_cutscenes_host::InteractionContext::audio_ready,&world_cutscenes_host::InteractionContext::progress_ready}){gate.*member=false;CHECK(!world_cutscenes_host::interaction_allowed(gate));gate.*member=true;}
 const auto folder=std::filesystem::temp_directory_path()/("cutscene-host-"+std::to_string(getpid()));std::filesystem::create_directories(folder);const auto path=(folder/"cutscenes.progress").string();
 world_cutscenes::Progress history;history.completed_ids={9};std::string error;
 CHECK(world_cutscenes_host::read_progress(path,history,error)&&history.completed_ids.empty());history.completed_ids={9};
 const std::string corrupt="VALWOOD_CUTSCENE_PROGRESS broken signature\n";{std::ofstream f(path);f<<corrupt;}
 CHECK(!world_cutscenes_host::read_progress(path,history,error)&&history.completed_ids==std::vector<std::uint64_t>{9});
 {std::ifstream f(path);std::string bytes((std::istreambuf_iterator<char>(f)),{});CHECK(bytes==corrupt);}
 CHECK(!world_cutscenes_host::read_progress((folder/std::string(500,'x')).string(),history,error)&&history.completed_ids==std::vector<std::uint64_t>{9});
 std::filesystem::remove(path);std::filesystem::create_symlink(folder/"absent",path);CHECK(!world_cutscenes_host::read_progress(path,history,error)&&history.completed_ids==std::vector<std::uint64_t>{9});std::filesystem::remove(path);
 CHECK(world_cutscenes::save_progress(path.c_str(),history,error));history.completed_ids={12};CHECK(world_cutscenes_host::read_progress(path,history,error)&&history.completed_ids==std::vector<std::uint64_t>{9});
 std::filesystem::remove_all(folder);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"actual_callback_endpoints\":true,\"closed_door_blocked\":true,\"open_door_allowed\":true,\"other_storey_rejected\":true,\"nonfinite_rejected_before_scene_query\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
