#include "engine/world_graphics_settings.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Tier=WorldEnvironmentTextures;
using Result=WorldGraphicsSettingsLoadResult;
unsigned checks=0,roundtrips=0,migrations=0,rejections=0;
void require(bool okay,const char *message){++checks;if(!okay)throw std::runtime_error(message);}
bool equal(const WorldGraphicsSettings &a,const WorldGraphicsSettings &b){
 return a.preset==b.preset&&a.auto_quality==b.auto_quality&&a.target_fps==b.target_fps&&
 a.render_scale==b.render_scale&&a.min_render_scale==b.min_render_scale&&a.shadow_side==b.shadow_side&&
 a.reflections==b.reflections&&a.water_quality==b.water_quality&&a.environment_effects==b.environment_effects&&
 a.contact_shadows==b.contact_shadows&&a.volumetric_fog==b.volumetric_fog&&a.clouds==b.clouds&&
 a.reflection_scale==b.reflection_scale&&a.display_mode==b.display_mode&&a.window_width==b.window_width&&
 a.window_height==b.window_height&&a.vsync==b.vsync&&a.foliage_distance==b.foliage_distance&&
 a.foliage_density==b.foliage_density&&a.character_textures==b.character_textures&&
 a.environment_textures==b.environment_textures&&a.frame_rate_limit==b.frame_rate_limit;
}
WorldGraphicsSettings distinct(){
 WorldGraphicsSettings s;
 s.preset=WorldGraphicsPreset::Custom;s.auto_quality=false;s.target_fps=90;
 s.render_scale=.88f;s.min_render_scale=.76f;s.shadow_side=3072;
 s.reflections=false;s.contact_shadows=true;s.volumetric_fog=false;s.clouds=true;s.reflection_scale=.5f;
 s.display_mode=WorldDisplayMode::Borderless;s.window_width=1920;s.window_height=1080;s.vsync=false;
 s.foliage_distance=WorldFoliageDistance::Ultra;s.foliage_density=WorldFoliageDensity::Low;s.frame_rate_limit=144;
 s.character_textures=WorldCharacterTextures::Ultra;s.water_quality=WorldWaterQuality::Low;
 s.environment_effects=WorldEnvironmentEffects::Ultra;s.environment_textures=Tier::Low;
 return s;
}
std::string read(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);require(bool(f),"read file");return {std::istreambuf_iterator<char>(f),{}};}
void write(const std::filesystem::path &p,const std::string &s){std::ofstream f(p,std::ios::binary|std::ios::trunc);require(bool(f),"write open");f.write(s.data(),std::streamsize(s.size()));require(bool(f),"write bytes");}
std::string replace(std::string s,const std::string &key,const std::string &value){
 const auto a=s.find(key+'=');require(a!=std::string::npos,"replacement key exists");const auto b=s.find('\n',a);
 s.replace(a,(b==std::string::npos?s.size():b)-a,key+'='+value);return s;
}
std::string remove(std::string s,const std::string &key){
 const auto a=s.find(key+'=');require(a!=std::string::npos,"removal key exists");const auto b=s.find('\n',a);
 s.erase(a,(b==std::string::npos?s.size():b+1)-a);return s;
}
// Independent, exact version fixtures. Do not derive historical fields from
// the current serializer: that can conceal an accidental format migration.
std::string legacy(unsigned version){
 std::string s="version="+std::to_string(version)+"\npreset=Custom\nauto_quality=0\ntarget_fps=90\nrender_scale=.88\nmin_render_scale=.76\nshadow_side=3072\nreflections=0\ncontact_shadows=1\nvolumetric_fog=0\nclouds=1\nreflection_scale=.5\n";
 if(version>=2)s+="display_mode=Borderless\nwindow_width=1920\nwindow_height=1080\nvsync=0\n";
 if(version>=3)s+="foliage_distance=Ultra\n";
 if(version>=4)s+="foliage_density=Low\nframe_rate_limit=144\n";
 if(version>=5)s+="character_textures=Ultra\n";
 if(version>=6)s+="water_quality=Low\n";
 if(version>=7)s+="environment_effects=Ultra\n";
 return s;
}
WorldGraphicsSettings migrated(unsigned version){
 auto s=distinct();s.environment_textures=Tier::High;
 if(version<7)s.environment_effects=WorldEnvironmentEffects::Medium;
 if(version<6)s.water_quality=WorldWaterQuality::High;
 if(version<5)s.character_textures=WorldCharacterTextures::Medium;
 if(version<4){s.foliage_density=WorldFoliageDensity::Ultra;s.frame_rate_limit=0;}
 if(version<3)s.foliage_distance=WorldFoliageDistance::Medium;
 if(version<2){s.display_mode=WorldDisplayMode::Windowed;s.window_width=s.window_height=0;s.vsync=true;}
 return s;
}
}
int main(){
 std::filesystem::path root;
 try{
  char directory[]="/tmp/valwood-environment-textures-XXXXXX";require(mkdtemp(directory)!=nullptr,"temporary directory");root=directory;
  const auto path=root/"graphics.settings";
  constexpr std::array<Tier,4> tiers={Tier::Low,Tier::Medium,Tier::High,Tier::Ultra};
  constexpr std::array<unsigned,4> edges={512,1024,2048,4096};
  constexpr std::array<std::uint64_t,4> budgets={768ull<<20,1280ull<<20,1536ull<<20,2048ull<<20};
  constexpr std::array<const char *,4> names={"Low","Medium","High","Ultra"};
  const auto defaults=world_graphics_settings_default();
  require(defaults.environment_textures==Tier::High,"default explicitly High2K");
  for(unsigned i=0;i<4;++i){
   require(std::string(world_environment_textures_name(tiers[i]))==names[i],"tier label");
   require(world_environment_texture_edge(tiers[i])==edges[i],"native edge policy");
   require(world_environment_texture_budget_bytes(tiers[i])==budgets[i],"exact byte budget policy");
   // Character, water, effects, density and frame limits are independently set.
   for(unsigned character=0;character<4;++character){
    auto s=distinct();s.environment_textures=tiers[i];s.character_textures=static_cast<WorldCharacterTextures>(character);
    require(world_graphics_settings_valid(s),"all texture combinations valid");
    require(world_graphics_settings_save(path.c_str(),s),"save v8");const auto bytes=read(path);
    require(bytes.rfind("version=8\n",0)==0,"serializer declares version8");
    require(bytes.find(std::string("environment_textures=")+names[i]+"\n")!=std::string::npos,"serializer includes exact tier");
    auto loaded=defaults;require(world_graphics_settings_load(path.c_str(),&loaded)==Result::Loaded,"load v8");
    require(equal(s,loaded),"roundtrip preserves every setting");++roundtrips;
    for(auto preset:{WorldGraphicsPreset::Performance,WorldGraphicsPreset::Balanced,WorldGraphicsPreset::Cinematic,WorldGraphicsPreset::Custom}){
     auto changed=s;require(world_graphics_settings_apply_preset(&changed,preset),"preset accepts valid model");
     require(changed.environment_textures==tiers[i]&&changed.character_textures==s.character_textures,"preset preserves both restart-required texture tiers");
    }
   }
  }
  for(unsigned version=1;version<=7;++version){
   const auto fixture=legacy(version);write(path,fixture);auto loaded=distinct();
   require(world_graphics_settings_load(path.c_str(),&loaded)==Result::Loaded,"historical version accepted");
   require(equal(loaded,migrated(version)),"historical migration preserves present fields and exact omitted-field defaults");
   require(world_graphics_settings_save(path.c_str(),loaded),"save migrated model");
   auto again=distinct();require(world_graphics_settings_load(path.c_str(),&again)==Result::Loaded&&equal(loaded,again),"migration stable after v8 save");++migrations;
   // Old versions must not silently accept fields from a newer schema.
   write(path,fixture+"environment_textures=High\n");auto unchanged=distinct();
   require(world_graphics_settings_load(path.c_str(),&unchanged)==Result::Rejected&&equal(unchanged,distinct()),"mixed-version environment key rejected atomically");++rejections;
  }
  auto sentinel=distinct();require(world_graphics_settings_save(path.c_str(),sentinel),"save rejection fixture");const auto valid=read(path);
  auto bad=[&](const std::string &bytes){
   write(path,bytes);auto unchanged=sentinel;char error[128]={};
   require(world_graphics_settings_load(path.c_str(),&unchanged,error,sizeof(error))==Result::Rejected,"malformed v8 rejected");
   require(equal(unchanged,sentinel),"rejected load leaves every field intact");require(error[0]!=0,"rejection reported");++rejections;
  };
  bad(remove(valid,"environment_textures"));bad(valid+"environment_textures=Low\n");
  bad(valid+"environment_textures=High\n");bad(replace(valid,"environment_textures","low"));
  bad(replace(valid,"environment_textures","UltraPlus"));bad(replace(valid,"environment_textures","4096"));
  bad(replace(valid,"environment_textures",""));bad(replace(valid,"environment_textures","High # inline comment"));
  bad(replace(valid,"version","9"));bad(replace(valid,"version","0"));bad(replace(valid,"version","8.0"));
  bad(valid+"environment_texture=High\n");bad(replace(valid,"render_scale","nan"));
  bad(replace(valid,"min_render_scale",".9"));bad(replace(valid,"shadow_side","8192"));
  auto nul=valid;nul.insert(nul.find("environment_textures"),1,'\0');bad(nul);
  // Legal order/whitespace/comments remain accepted in the strict schema.
  std::string whitespace="\t# environment texture fixture\r\n\n";
  for(std::size_t begin=0;begin<valid.size();){const auto end=valid.find('\n',begin);const auto line=valid.substr(begin,end-begin);const auto eq=line.find('=');whitespace+=" \t"+line.substr(0,eq)+" \t= \t"+line.substr(eq+1)+" \t\r\n";begin=end+1;}
  write(path,whitespace);auto loaded=defaults;require(world_graphics_settings_load(path.c_str(),&loaded)==Result::Loaded&&equal(loaded,sentinel),"v8 whitespace/comments accepted");
  auto reordered=remove(valid,"environment_textures");reordered=std::string("environment_textures=Low\n")+reordered;
  write(path,reordered);require(world_graphics_settings_load(path.c_str(),&loaded)==Result::Loaded&&equal(loaded,sentinel),"v8 field order independent");
  for(int invalid:{-1,4,99}){
   auto s=sentinel;s.environment_textures=static_cast<Tier>(invalid);
   require(world_environment_textures_name(s.environment_textures)==nullptr,"invalid enum has no label");
   require(world_environment_texture_edge(s.environment_textures)==0,"invalid enum has no edge");
   require(world_environment_texture_budget_bytes(s.environment_textures)==0,"invalid enum has no budget");
   require(!world_graphics_settings_valid(s),"invalid environment enum rejected");
   write(path,valid);require(!world_graphics_settings_save(path.c_str(),s),"invalid save rejected before disk mutation");require(read(path)==valid,"invalid save preserves usable file");
   const auto before=s;require(!world_graphics_settings_apply_preset(&s,WorldGraphicsPreset::Balanced)&&equal(s,before),"invalid preset request leaves original model intact");
  }
  loaded=sentinel;require(world_graphics_settings_load((root/"missing").c_str(),&loaded)==Result::Missing&&equal(loaded,sentinel),"missing file leaves tier and all fields intact");
  std::filesystem::remove_all(root);
  std::printf("{\"passed\":true,\"checks\":%u,\"v8_roundtrips\":%u,\"historical_migrations\":%u,\"strict_rejections\":%u,\"source\":\"CPU settings only; no GPU or scene mutation\"}\n",checks,roundtrips,migrations,rejections);return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"Environment texture settings FAILED after%uchecks: %s\n",checks,e.what());if(!root.empty())std::filesystem::remove_all(root);return 1;}
}
