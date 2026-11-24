#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_atlas.h"
#include "engine/world_studio.h"
#include "engine/world_pond_field.h"
#include "engine/world_surface_field.h"
#include "engine/world_river_network.h"
#include "engine/world_scenery.h"
#include "engine/world_models.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include "engine/world_opening_interior.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
namespace {
bool records_only=false,reached_models=false;
std::string first_model;
using Clock=std::chrono::steady_clock;
const auto begin_time=Clock::now();
double elapsed(){return std::chrono::duration<double>(Clock::now()-begin_time).count();}
void phase(const char *text){std::fprintf(stderr,"[Preflight %.3fs] %s\n",elapsed(),text);std::fflush(stderr);}
struct NativeCallbacks {
 static float height(double x,double z,const void *data){const auto &doc=*static_cast<const world_studio::Document*>(data);return world_studio::height_delta(doc,x,z)+(doc.surface_field?world_surface::terrain_offset(*doc.surface_field,x,z):0);}
 static bool water(double x,double z,float *level,float *depth,const void *data){
  const auto &doc=*static_cast<const world_studio::Document*>(data);
  if(doc.surface_field){float y=0,shore=0;if(world_surface::river(*doc.surface_field,x,z,y,shore)){float d=y-world_atlas_height(x,z);if(d>.005f){if(level)*level=y;if(depth)*depth=d;return true;}}}
  for(const auto &w:doc.waters)if(w.source==3&&w.enabled&&world_entity::visible(doc.entities,{world_entity::Kind::Water,unsigned(&w-doc.waters.data())})){double dx=(x-w.x)/w.radius_x,dz=(z-w.z)/w.radius_z;if(dx*dx+dz*dz>=1)continue;float d=float(w.level)-world_atlas_height(x,z);if(d>.005f){if(level)*level=float(w.level);if(depth)*depth=d;return true;}}
  return false;
 }
};
}
extern "C" WorldModelAsset *__real_world_model_load(const char *);
extern "C" WorldModelAsset *__wrap_world_model_load(const char *path){
 if(records_only){reached_models=true;first_model=path?path:"";return nullptr;}
 return __real_world_model_load(path);
}
int main(int argc,char **argv){
 namespace fs=std::filesystem;
 if(argc<2){std::cerr<<"Usage: preflight_scene PROJECT [--records-only] [--camera camp|apartment] [--overlay FILE] [--report FILE]\n";return 2;}
 const auto project=fs::absolute(argv[1]);fs::path overlay=project.parent_path()/"vegetation/creek_dressing_instances.txt",report;
 bool apartment=false;
 for(int i=2;i<argc;++i){const std::string arg=argv[i];if(arg=="--records-only")records_only=true;else if(arg=="--camera"&&i+1<argc){const std::string value=argv[++i];if(value!="camp"&&value!="apartment")return 2;apartment=value=="apartment";}else if(arg=="--overlay"&&i+1<argc)overlay=fs::absolute(argv[++i]);else if(arg=="--report"&&i+1<argc)report=fs::absolute(argv[++i]);else return 2;}
 auto result=[&](bool passed,const std::string &stage,const std::string &error,const WorldSceneryStats *stats=nullptr){
  std::ostringstream json;json<<"{\n  \"passed\": "<<(passed?"true":"false")<<",\n  \"mode\": "<<std::quoted(records_only?"strict-records-only":"full-native-load")<<",\n  \"stage\": "<<std::quoted(stage)<<",\n  \"error\": "<<std::quoted(error)<<",\n  \"elapsed_seconds\": "<<elapsed()<<",\n  \"project\": "<<std::quoted(project.string())<<",\n  \"camera\": "<<std::quoted(apartment?"apartment":"camp")<<",\n  \"renderer\": "<<std::quoted(SDL_GL_GetCurrentContext()?reinterpret_cast<const char*>(glGetString(GL_RENDERER)):"no GL context")<<",\n  \"record_validation_completed\": "<<((passed||reached_models)?"true":"false")<<",\n  \"models_and_footprints_validated\": "<<((passed&&!records_only)?"true":"false");
  if(stats)json<<",\n  \"instances\": "<<stats->source_instances<<",\n  \"aliases\": "<<stats->source_aliases<<",\n  \"source_files\": "<<stats->source_files<<",\n  \"colliders\": "<<stats->source_colliders;
  json<<"\n}\n";std::cout<<json.str();std::cout.flush();if(!report.empty()){fs::create_directories(report.parent_path());std::ofstream out(report);out<<json.str();}
  return passed?0:1;
 };
 fs::current_path(project);
 world_studio::Document doc;std::string error;char why[1024]={};
 phase("initializing actual atlas and authored height regions");
 if(!world_atlas_init()||world_atlas_load_height_region("ground/showcase.height",why,sizeof why)!=1||world_atlas_load_fallback_height_region("ground/countryside.height",why,sizeof why)<0)return result(false,"atlas",why);
 phase("loading actual Studio document and preparing actual pond/river fields");
 if(!world_studio::load("world.studio",doc,error)||!world_pond_field::prepare(doc,{world_atlas_base_height},error))return result(false,"Studio/pond fields",error);
 world_atlas_set_height_edit(NativeCallbacks::height,&doc);world_atlas_set_water_edit(NativeCallbacks::water,&doc);
 std::vector<WorldSceneryDefinition> definitions;definitions.reserve(doc.assets.size());for(const auto &a:doc.assets)definitions.push_back({a.alias.c_str(),a.stem.c_str(),a.lods,a.collision,a.vegetation,a.distance,a.split});
 if(!world_scenery_configure(definitions.data(),definitions.size(),why,sizeof why))return result(false,"asset definitions",why);
 world_opening_interior::Config interior;std::ifstream settings("opening/interior.settings");if(settings&&!world_opening_interior::parse(settings,interior,error))return result(false,"interior settings",error);
 double camera_x=-11988,camera_y=13.55,camera_z=5172;
 if(apartment){std::ifstream opening("opening.config");std::string magic,key;unsigned version=0;double y=0,yaw=0,pitch=0;if(!(opening>>magic>>version>>key>>camera_x>>y>>camera_z>>yaw>>pitch)||magic!="VALWOOD_OPENING_SCENE"||version!=1||key!="house_spawn")return result(false,"opening spawn","Invalid native opening spawn");camera_y=y+1.75;}
 phase("creating software OpenGL context; no desktop window or actual GPU");
 SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_setenv("LIBGL_ALWAYS_SOFTWARE","1",1);if(SDL_Init(SDL_INIT_VIDEO)!=0)return result(false,"SDL",SDL_GetError());
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 auto *window=SDL_CreateWindow("Strict scene preflight",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!window)return result(false,"SDL window",SDL_GetError());auto context=SDL_GL_CreateContext(window);if(!context)return result(false,"OpenGL",SDL_GetError());
 const char *renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));if(!renderer||(!std::strstr(renderer,"llvmpipe")&&!std::strstr(renderer,"softpipe")))return result(false,"software GPU guard","Preflight requires llvmpipe/softpipe; native GPU was not used");
 std::vector<const char*> files={"scenery/authored_instances.txt","scenery/starting_area_instances.txt","scenery/rainforest_instances.txt","scenery/castle_village_instances.txt","scenery/tanthoon_bandit_camp_instances.txt","scenery/castle_streets_instances.txt","scenery/bep_environment_instances.txt","scenery/demo_finish_instances.txt","scenery/demo_polish_instances.txt"};
 if(fs::exists("scenery/demo_completion_instances.txt"))files.push_back("scenery/demo_completion_instances.txt");std::string overlay_string=overlay.string();if(fs::exists(overlay))files.push_back(overlay_string.c_str());
 WorldSceneryStreamingConfig config;config.geometry_bytes=1280ull*1024*1024;config.texture_bytes=1344ull*1024*1024;config.cpu_bytes=768ull*1024*1024;config.preload_meters=64;config.unload_meters=96;
 if(interior.enabled&&interior.release_city_fallbacks){config.interior_release_city_fallbacks=true;config.interior_start_active=world_opening_interior::contains(interior,{camera_x,camera_y,camera_z});for(unsigned k=0;k<3;++k){config.interior_minimum[k]=interior.bounds.minimum[k];config.interior_maximum[k]=interior.bounds.maximum[k];}}
 phase(records_only?"strict native records: stop only after ALL sources pass, at first model load":"full native streamed scene: strict records, actual models/bounds/collision footprints");
 auto *scene=world_scenery_load_files_streamed(files.data(),files.size(),"models",camera_x,camera_z,config,why,sizeof why);
 const bool passed=scene||(records_only&&reached_models);const auto stats=scene?world_scenery_stats(scene):WorldSceneryStats{};
 const int code=result(passed,records_only&&reached_models?"all strict records passed; model/footprint phase deliberately not run":scene?"full native scene loaded":"strict native scene load",passed?"":why,scene?&stats:nullptr);
 world_scenery_free(scene);world_scenery_configure(nullptr,0);world_atlas_set_height_edit(nullptr,nullptr);world_atlas_set_water_edit(nullptr,nullptr);world_atlas_shutdown();texture_cache_shutdown();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return code;
}
