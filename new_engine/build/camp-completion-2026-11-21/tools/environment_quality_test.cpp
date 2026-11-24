#include "engine/world_graphics_settings.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <string>
#include <iostream>
#include <unistd.h>
int main(int argc,char**argv){
 assert(argc==2);const auto p=std::filesystem::path(argv[1]);std::filesystem::create_directories(p);auto file=p/"settings";unsigned checks=0;
 WorldGraphicsSettings s;char error[256]={};unsigned lastSmoke=0,lastAmbient=0;float lastRange=0;int lastSteps=0;
 for(auto q:{WorldEnvironmentEffects::Low,WorldEnvironmentEffects::Medium,WorldEnvironmentEffects::High,WorldEnvironmentEffects::Ultra}){
  auto a=world_environment_effects_profile(q);assert(a.smoke_particles>lastSmoke&&a.ambient_particles>lastAmbient&&a.smoke_particles+a.ambient_particles<=512&&a.smoke_distance>lastRange&&a.fog_steps>lastSteps);lastSmoke=a.smoke_particles;lastAmbient=a.ambient_particles;lastRange=a.smoke_distance;lastSteps=a.fog_steps;++checks;
  s.environment_effects=q;s.character_textures=WorldCharacterTextures::Ultra;s.window_width=1280;s.window_height=720;s.render_scale=.85;s.min_render_scale=.75;s.water_quality=WorldWaterQuality::High;s.vsync=false;
  assert(world_graphics_settings_save(file.c_str(),s,error,sizeof error));WorldGraphicsSettings t;assert(world_graphics_settings_load(file.c_str(),&t,error,sizeof error)==WorldGraphicsSettingsLoadResult::Loaded);assert(t.environment_effects==q&&t.character_textures==s.character_textures&&t.window_width==1280&&!t.vsync&&t.water_quality==s.water_quality&&t.render_scale==s.render_scale);++checks;
  std::ifstream in(file);std::string text((std::istreambuf_iterator<char>(in)),{});const auto key=text.find("environment_effects=");assert(key!=std::string::npos);
  // Legacy version6 retains existing water, texture, display and timing choices.
  std::string legacy=text.substr(0,key);legacy.replace(legacy.find("version=7"),9,"version=6");{std::ofstream out(file);out<<legacy;}t.environment_effects=WorldEnvironmentEffects::Ultra;assert(world_graphics_settings_load(file.c_str(),&t,error,sizeof error)==WorldGraphicsSettingsLoadResult::Loaded);assert(t.environment_effects==WorldEnvironmentEffects::Medium&&t.water_quality==s.water_quality&&t.character_textures==s.character_textures&&!t.vsync);++checks;
  for(const std::string suffix:{"environment_effects=Bad\n","environment_effects=High\nenvironment_effects=Low\n","unknown_effect=Ultra\n"}){auto invalid=text.substr(0,key)+suffix;{std::ofstream out(file);out<<invalid;}t=s;assert(world_graphics_settings_load(file.c_str(),&t,error,sizeof error)==WorldGraphicsSettingsLoadResult::Rejected);assert(t.environment_effects==s.environment_effects&&t.window_width==s.window_width);++checks;}
 }
 auto bad=s;bad.environment_effects=WorldEnvironmentEffects(99);assert(!world_graphics_settings_valid(bad));assert(!world_graphics_settings_save(file.c_str(),bad,error,sizeof error));assert(!world_environment_effects_name(bad.environment_effects));assert(world_environment_effects_profile(bad.environment_effects).smoke_particles==0);
 std::cout<<"PASS environment quality: "<<checks<<" checks; bounded tier budgets, version7 roundtrip, version6 migration, corruption rollback, display/texture preservation\n";
}
