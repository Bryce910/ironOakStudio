#include "engine/world_graphics_settings.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <sys/stat.h>

namespace {
using Preset=WorldGraphicsPreset;
using Result=WorldGraphicsSettingsLoadResult;
bool equal(const WorldGraphicsSettings &a,const WorldGraphicsSettings &b){
    return a.preset==b.preset&&a.auto_quality==b.auto_quality&&a.target_fps==b.target_fps&&
        a.render_scale==b.render_scale&&a.min_render_scale==b.min_render_scale&&a.shadow_side==b.shadow_side&&
        a.reflections==b.reflections&&a.contact_shadows==b.contact_shadows&&a.volumetric_fog==b.volumetric_fog&&
        a.clouds==b.clouds&&a.reflection_scale==b.reflection_scale&&a.display_mode==b.display_mode&&
        a.window_width==b.window_width&&a.window_height==b.window_height&&a.vsync==b.vsync;
}
std::string read(const std::filesystem::path &path){
    std::ifstream file(path,std::ios::binary);
    assert(file);return {std::istreambuf_iterator<char>(file),{}};
}
void write(const std::filesystem::path &path,const std::string &text){
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    assert(file);file.write(text.data(),std::streamsize(text.size()));assert(file);
}
std::string replace(std::string text,const std::string &key,const std::string &value){
    const auto begin=text.find(key+'=');assert(begin!=std::string::npos);
    const auto end=text.find('\n',begin);
    text.replace(begin,end-begin,key+'='+value);return text;
}
std::size_t entries(const std::filesystem::path &path){
    return std::size_t(std::distance(std::filesystem::directory_iterator(path),std::filesystem::directory_iterator{}));
}
}

int main(){
    auto settings=world_graphics_settings_default();
    assert(world_graphics_settings_valid(settings));
    assert(settings.preset==Preset::Balanced&&settings.auto_quality&&settings.target_fps==60);
    assert(settings.render_scale==1&&settings.min_render_scale==.70f&&settings.shadow_side==2048);
    assert(settings.reflections&&settings.contact_shadows&&settings.volumetric_fog&&settings.clouds&&settings.reflection_scale==.5f);
    const auto defaults=settings;
    assert(settings.display_mode==WorldDisplayMode::Windowed&&settings.window_width==0&&settings.window_height==0&&settings.vsync);

    // Presets replace visual policy while retaining the player's timing choice.
    settings.auto_quality=false;settings.target_fps=120;
    settings.clouds=settings.reflections=settings.volumetric_fog=false;
    settings.min_render_scale=.95f;
    assert(world_graphics_settings_apply_preset(&settings,Preset::Performance));
    assert(!settings.auto_quality&&settings.target_fps==120&&settings.render_scale==.85f);
    assert(settings.min_render_scale==.7f&&settings.shadow_side==2048&&settings.reflection_scale==.25f);
    assert(settings.reflections&&!settings.contact_shadows&&settings.volumetric_fog&&settings.clouds);
    assert(world_graphics_settings_apply_preset(&settings,Preset::Cinematic));
    assert(settings.render_scale==1&&settings.shadow_side==4096&&settings.reflection_scale==1);
    assert(settings.reflections&&settings.contact_shadows&&settings.volumetric_fog&&settings.clouds);
    assert(!settings.auto_quality&&settings.target_fps==120);
    const auto cinematic=settings;
    assert(world_graphics_settings_apply_preset(&settings,Preset::Custom));
    auto custom=cinematic;custom.preset=Preset::Custom;assert(equal(settings,custom));
    assert(!world_graphics_settings_apply_preset(&settings,static_cast<Preset>(99))&&equal(settings,custom));
    assert(!world_graphics_settings_apply_preset(nullptr,Preset::Balanced));
    assert(world_graphics_preset_name(static_cast<Preset>(-1))==nullptr);
    assert(world_graphics_settings_apply_preset(&settings,Preset::Balanced));
    auto balanced=defaults;balanced.auto_quality=false;balanced.target_fps=120;assert(equal(settings,balanced));

    for(int fps:{45,60,90,120}){settings=defaults;settings.target_fps=fps;assert(world_graphics_settings_valid(settings));}
    for(int side:{1024,2048,3072,4096}){settings=defaults;settings.shadow_side=side;assert(world_graphics_settings_valid(settings));}
    for(float scale:{.25f,.5f,1.f}){settings=defaults;settings.reflection_scale=scale;assert(world_graphics_settings_valid(settings));}
    for(float scale:{.70f,.85f,1.f}){settings=defaults;settings.render_scale=settings.min_render_scale=scale;assert(world_graphics_settings_valid(settings));}
    for(unsigned kind=0;kind<16;kind++){
        settings=defaults;
        switch(kind){
            case 0:settings.preset=static_cast<Preset>(99);break;
            case 1:settings.target_fps=59;break;
            case 2:settings.target_fps=0;break;
            case 3:settings.render_scale=std::nanf("");break;
            case 4:settings.render_scale=INFINITY;break;
            case 5:settings.render_scale=std::nextafter(.70f,0.f);break;
            case 6:settings.render_scale=std::nextafter(1.f,2.f);break;
            case 7:settings.min_render_scale=std::nanf("");break;
            case 8:settings.min_render_scale=-INFINITY;break;
            case 9:settings.min_render_scale=std::nextafter(.70f,0.f);break;
            case 10:settings.render_scale=.7f;settings.min_render_scale=.71f;break;
            case 11:settings.shadow_side=1536;break;
            case 12:settings.shadow_side=0;break;
            case 13:settings.reflection_scale=.3f;break;
            case 14:settings.reflection_scale=INFINITY;break;
            case 15:settings.reflection_scale=std::nanf("");break;
        }
        const auto bad=settings;
        assert(!world_graphics_settings_valid(settings));
        assert(!world_graphics_settings_apply_preset(&settings,Preset::Balanced));
        // Check unchanged fields individually, allowing the original NaN.
        assert(settings.preset==bad.preset&&settings.target_fps==bad.target_fps&&settings.shadow_side==bad.shadow_side);
        assert((settings.render_scale==bad.render_scale)||(std::isnan(settings.render_scale)&&std::isnan(bad.render_scale)));
        assert((settings.min_render_scale==bad.min_render_scale)||(std::isnan(settings.min_render_scale)&&std::isnan(bad.min_render_scale)));
        assert((settings.reflection_scale==bad.reflection_scale)||(std::isnan(settings.reflection_scale)&&std::isnan(bad.reflection_scale)));
    }

    char directory[]="/tmp/valwood-graphics-settings-XXXXXX";assert(mkdtemp(directory));
    const std::filesystem::path root=directory,path=root/"graphics.settings";
    settings=custom;char error[256]="stale";
    assert(world_graphics_settings_load(path.c_str(),&settings,error,sizeof(error))==Result::Missing);
    assert(equal(settings,custom)&&!error[0]);
    assert(world_graphics_settings_load(nullptr,&settings,error,sizeof(error))==Result::Rejected&&error[0]);
    assert(world_graphics_settings_load(path.c_str(),nullptr,error,sizeof(error))==Result::Rejected);
    assert(world_graphics_settings_load(root.c_str(),&settings,error,sizeof(error))==Result::IOError&&equal(settings,custom));

    for(auto preset:{Preset::Performance,Preset::Balanced,Preset::Cinematic,Preset::Custom})for(int fps:{45,60,90,120}){
        settings=defaults;settings.target_fps=fps;settings.auto_quality=(fps!=90);
        assert(world_graphics_settings_apply_preset(&settings,preset));
        if(preset==Preset::Custom){
            settings.render_scale=.73123455f;settings.min_render_scale=.70f;settings.shadow_side=3072;
            settings.reflections=settings.contact_shadows=settings.volumetric_fog=settings.clouds=false;
            settings.reflection_scale=.25f;
            settings.display_mode=WorldDisplayMode::Borderless;settings.window_width=3840;settings.window_height=2160;settings.vsync=false;
        }
        assert(world_graphics_settings_save(path.c_str(),settings,error,sizeof(error))&&!error[0]);
        auto loaded=cinematic;
        assert(world_graphics_settings_load(path.c_str(),&loaded,error,sizeof(error))==Result::Loaded&&!error[0]);
        assert(equal(loaded,settings)&&entries(root)==1);
    }
    assert(world_graphics_settings_save(path.c_str(),defaults));
    const auto valid=read(path);assert(valid.size()<world_graphics_settings_max_bytes);
    struct stat mode{};assert(!stat(path.c_str(),&mode));assert((mode.st_mode&0777)==0600);
    unsigned rejected=0;
    auto bad_file=[&](const std::string &bytes){
        write(path,bytes);settings=custom;
        struct {char before='a';char text[2]={'x','x'};char after='b';} bounded;
        assert(world_graphics_settings_load(path.c_str(),&settings,bounded.text,sizeof(bounded.text))==Result::Rejected);
        assert(equal(settings,custom)&&bounded.before=='a'&&bounded.after=='b'&&bounded.text[0]&&bounded.text[1]==0);
        rejected++;
    };
    bad_file("");bad_file("# only a comment\n");bad_file(valid+"target_fps=45\n");
    bad_file(valid+"typo_setting=1\n");bad_file(valid+"trailing garbage\n");
    bad_file(replace(valid,"version","3"));bad_file(replace(valid,"version","1.0"));
    bad_file(replace(valid,"display_mode","Exclusive"));bad_file(replace(valid,"window_width","99999"));
    bad_file(replace(valid,"window_width","640"));bad_file(replace(valid,"window_height","480"));
    bad_file(replace(valid,"vsync","true"));bad_file(replace(valid,"vsync","2"));
    bad_file(replace(valid,"preset","balanced"));bad_file(replace(valid,"preset","Unknown"));
    for(const auto *key:{"auto_quality","reflections","contact_shadows","volumetric_fog","clouds"}){
        bad_file(replace(valid,key,"true"));bad_file(replace(valid,key,"2"));bad_file(replace(valid,key,"-1"));
    }
    for(const auto *number:{"nan","NaN","inf","-inf","1e999","1garbage","0x1p0","0,85",""})
        bad_file(replace(valid,"render_scale",number));
    bad_file(replace(valid,"render_scale","0.69"));bad_file(replace(valid,"render_scale","1.01"));
    bad_file(replace(valid,"min_render_scale","0.69"));bad_file(replace(valid,"min_render_scale","1.01"));
    bad_file(replace(replace(valid,"render_scale","0.65"),"min_render_scale","0.65"));
    bad_file(replace(valid,"min_render_scale","0.65"));
    bad_file(replace(replace(valid,"render_scale","0.8"),"min_render_scale","0.9"));
    bad_file(replace(valid,"min_render_scale","nan"));
    bad_file(replace(valid,"target_fps","60.0"));bad_file(replace(valid,"target_fps","61"));
    bad_file(replace(valid,"target_fps","99999999999999999999999"));
    bad_file(replace(valid,"shadow_side","1536"));bad_file(replace(valid,"reflection_scale","0.4"));
    bad_file(replace(valid,"reflection_scale","nan"));
    for(const auto *key:{"version","preset","auto_quality","target_fps","render_scale","min_render_scale","shadow_side","reflections","contact_shadows","volumetric_fog","clouds","reflection_scale","display_mode","window_width","window_height","vsync"}){
        auto missing=valid;const auto begin=missing.find(std::string(key)+'=');const auto end=missing.find('\n',begin);
        missing.erase(begin,end-begin+1);bad_file(missing);
        bad_file(valid+std::string(key)+"=1\n");
    }
    auto nul=valid;nul.insert(nul.size()/2,1,'\0');bad_file(nul);
    // Existing project preferences migrate without changing picture quality.
    auto legacy=replace(valid,"version","1");
    legacy.erase(legacy.find("display_mode="));write(path,legacy);settings=custom;
    assert(world_graphics_settings_load(path.c_str(),&settings)==Result::Loaded&&equal(settings,defaults));
    bad_file(legacy+"vsync=1\n"); // Incomplete mixed-version files are rejected.
    // Exact boundary accepts a complete file plus a bounded full-line comment.
    auto maximum=valid+'#';maximum.append(world_graphics_settings_max_bytes-maximum.size(),'x');
    assert(maximum.size()==world_graphics_settings_max_bytes);write(path,maximum);settings=custom;
    assert(world_graphics_settings_load(path.c_str(),&settings)==Result::Loaded&&equal(settings,defaults));
    bad_file(maximum+'x');
    std::string whitespace="  # human-readable configuration\r\n\n";
    for(std::size_t begin=0;begin<valid.size();){
        const auto end=valid.find('\n',begin);const auto line=valid.substr(begin,end-begin);
        const auto equal_at=line.find('=');
        whitespace+=" \t"+line.substr(0,equal_at)+" \t= \t"+line.substr(equal_at+1)+" \t\r\n";
        begin=end+1;
    }
    write(path,whitespace);settings=custom;
    assert(world_graphics_settings_load(path.c_str(),&settings)==Result::Loaded&&equal(settings,defaults));
    write(path,valid.substr(0,valid.size()-1));
    assert(world_graphics_settings_load(path.c_str(),&settings)==Result::Loaded&&equal(settings,defaults));

    // Saving bad data cannot overwrite an existing usable settings file.
    assert(world_graphics_settings_save(path.c_str(),defaults));const auto saved=read(path);
    settings=defaults;settings.render_scale=std::nanf("");
    assert(!world_graphics_settings_save(path.c_str(),settings,error,sizeof(error))&&error[0]);
    assert(read(path)==saved&&entries(root)==1);
    assert(!world_graphics_settings_save(nullptr,defaults));assert(!world_graphics_settings_save("",defaults));
    assert(!world_graphics_settings_save((root/"missing"/"settings").c_str(),defaults));
    const auto blocked=root/"blocked";assert(std::filesystem::create_directory(blocked));
    write(blocked/"keep","unchanged");const auto count=entries(root);
    assert(!world_graphics_settings_save(blocked.c_str(),defaults,error,sizeof(error))&&error[0]);
    assert(entries(root)==count&&read(blocked/"keep")=="unchanged");
    char untouched='z';settings=custom;
    assert(world_graphics_settings_load(path.c_str(),&settings,&untouched,0)==Result::Loaded&&untouched=='z');
    assert(equal(settings,defaults));
    assert(world_graphics_settings_save(path.c_str(),custom,&untouched,0)&&untouched=='z');
    std::filesystem::remove_all(root);
    std::printf("PASS graphics settings CPU: presets/preferences, finite bounded fields,16 exact roundtrips,%u whole-file rejection cases,4096-byte boundary,locale-independent format,missing/IO retention,atomic replacement/failure cleanup\n",rejected);
}
