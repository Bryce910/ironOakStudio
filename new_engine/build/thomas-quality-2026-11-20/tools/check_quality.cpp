#include "engine/world_character_quality.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

static std::string contents(const std::filesystem::path &p){std::ifstream f(p);std::ostringstream s;s<<f.rdbuf();return s.str();}
static void write(const std::filesystem::path &p,const std::string &s){std::ofstream f(p);f<<s;assert(f.good());}
int main(int argc,char **argv){
    assert(argc==2);const std::filesystem::path root(argv[1]);
    std::filesystem::create_directories(root);
    const auto bank=root/"bank",settings_path=root/"graphics.settings";
    char message[512]={};std::string error;
    using Quality=WorldCharacterTextures;
    using Result=WorldGraphicsSettingsLoadResult;
    using Paths=world_character_quality::StoryAppearances;
    const auto defaults=world_graphics_settings_default();
    assert(defaults.character_textures==Quality::Medium);
    unsigned tiers=0;
    for(auto tier:{Quality::Low,Quality::Medium,Quality::High,Quality::Ultra}){
        const auto name=world_character_textures_name(tier);
        for(const char *outfit:{"civilian","armored"}){
            const auto p=bank/name/"story_hero"/outfit;std::filesystem::create_directories(p);
            write(p/"appearance.cfg","body \"body.glb\"\n");write(p/"body.glb","fixture");
        }
        auto saved=defaults;saved.character_textures=tier;
        assert(world_graphics_settings_save(settings_path.c_str(),saved,message,sizeof(message)));
        auto loaded=defaults;assert(world_graphics_settings_load(settings_path.c_str(),&loaded,message,sizeof(message))==Result::Loaded);
        assert(loaded.character_textures==tier);
        Paths paths;assert(world_character_quality::resolve_story(bank.c_str(),loaded.character_textures,paths,error)&&error.empty()&&paths.selected);
        assert(paths.civilian==(bank/name/"story_hero/civilian/appearance.cfg").string());
        assert(paths.armored==(bank/name/"story_hero/armored/appearance.cfg").string());
        // An applied preference cannot mutate already loaded model paths.
        loaded.character_textures=Quality((unsigned(tier)+1)%4);
        assert(paths.civilian==(bank/name/"story_hero/civilian/appearance.cfg").string());
        // Global presets must preserve the independently chosen model tier.
        for(auto preset:{WorldGraphicsPreset::Performance,WorldGraphicsPreset::Balanced,WorldGraphicsPreset::Cinematic}){
            assert(world_graphics_settings_apply_preset(&saved,preset));assert(saved.character_textures==tier);
        }
        ++tiers;
    }
    assert(world_graphics_settings_save(settings_path.c_str(),defaults));
    const auto version6=contents(settings_path);
    // Versions 1 through 4 migrate to Medium without changing their file.
    for(int version=1;version<=4;++version){
        std::istringstream input(version6);std::string line,legacy;
        while(std::getline(input,line)){
            if(line.rfind("version=",0)==0){legacy+="version="+std::to_string(version)+"\n";continue;}
            if(line.rfind("water_quality=",0)==0||line.rfind("character_textures=",0)==0)continue;
            if(version<4&&(line.rfind("foliage_density=",0)==0||line.rfind("frame_rate_limit=",0)==0))continue;
            if(version<3&&line.rfind("foliage_distance=",0)==0)continue;
            if(version<2&&(line.rfind("display_mode=",0)==0||line.rfind("window_width=",0)==0||line.rfind("window_height=",0)==0||line.rfind("vsync=",0)==0))continue;
            legacy+=line+'\n';
        }
        write(settings_path,legacy);auto loaded=defaults;loaded.character_textures=Quality::Ultra;
        assert(world_graphics_settings_load(settings_path.c_str(),&loaded)==Result::Loaded&&loaded.character_textures==Quality::Medium);
        assert(contents(settings_path)==legacy);
        Paths paths;assert(world_character_quality::resolve_story(bank.c_str(),loaded.character_textures,paths,error)&&paths.civilian.find("/Medium/")!=std::string::npos);
    }
    Paths paths;assert(world_character_quality::resolve_story(nullptr,Quality::Medium,paths,error)&&!paths.selected);
    assert(!world_character_quality::resolve_story("relative",Quality::Medium,paths,error));
    assert(!world_character_quality::resolve_story(bank.c_str(),Quality(99),paths,error));
    auto loaded=defaults;write(settings_path,version6+"character_textures=Ultra\n");
    assert(world_graphics_settings_load(settings_path.c_str(),&loaded)==Result::Rejected&&loaded.character_textures==Quality::Medium);
    std::filesystem::remove(bank/"High/story_hero/armored/body.glb");
    assert(!world_character_quality::resolve_story(bank.c_str(),Quality::High,paths,error));
    std::cout<<"{\"passed\":true,\"tiers\":"<<tiers<<",\"legacy_versions\":4,\"settings_round_trip\":true,\"model_paths_follow_loaded_setting\":true,\"resident_unchanged_until_restart\":true,\"incomplete_tier_rejected\":true,\"independent_preset_preference_preserved\":true}\n";
}
