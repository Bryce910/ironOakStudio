#include "engine/world_platform.h"
#include "engine/world_graphics_settings.h"
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr const char *keys[]={"version","preset","auto_quality","target_fps","render_scale",
    "min_render_scale","shadow_side","reflections","contact_shadows","volumetric_fog","clouds","reflection_scale",
    "display_mode","window_width","window_height","vsync","foliage_distance","foliage_density","frame_rate_limit","character_textures","water_quality","environment_effects","environment_textures"};
constexpr unsigned all_fields=(1u<<12)-1;
constexpr unsigned version2_fields=(1u<<16)-1;
constexpr unsigned version3_fields=(1u<<17)-1;
constexpr unsigned version4_fields=(1u<<19)-1;
constexpr unsigned version5_fields=(1u<<20)-1;
constexpr unsigned version6_fields=(1u<<21)-1;
constexpr unsigned version7_fields=(1u<<22)-1;
constexpr unsigned version8_fields=(1u<<23)-1;

void message(char *out,std::size_t capacity,const char *text){
    if(out&&capacity)std::snprintf(out,capacity,"%s",text);
}
bool valid_path(const char *path){return path&&*path&&std::strlen(path)<4000;}
bool space(char c){return c==' '||c=='\t'||c=='\r';}
std::string_view trim(std::string_view value){
    while(!value.empty()&&space(value.front()))value.remove_prefix(1);
    while(!value.empty()&&space(value.back()))value.remove_suffix(1);
    return value;
}
template<class T> bool number(std::string_view value,T &out){
    if(value.empty())return false;
    T next{};
    const auto result=std::from_chars(value.data(),value.data()+value.size(),next);
    if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())return false;
    out=next;return true;
}
bool boolean(std::string_view value,bool &out){
    if(value=="0"){out=false;return true;}
    if(value=="1"){out=true;return true;}
    return false;
}
bool parse(const char *bytes,std::size_t count,WorldGraphicsSettings &out){
    if(std::memchr(bytes,0,count))return false;
    WorldGraphicsSettings next;
    int version=0;
    unsigned seen=0;
    std::string_view file(bytes,count);
    while(!file.empty()){
        const auto end=file.find('\n');
        auto line=trim(file.substr(0,end));
        if(end==std::string_view::npos)file={};else file.remove_prefix(end+1);
        if(line.empty()||line.front()=='#')continue;
        const auto equal=line.find('=');
        if(equal==std::string_view::npos)return false;
        const auto key=trim(line.substr(0,equal)),value=trim(line.substr(equal+1));
        unsigned field=0;while(field<23&&key!=keys[field])field++;
        if(field==23||(seen&(1u<<field)))return false;
        seen|=1u<<field;
        bool ok=false;
        switch(field){
            case 0:ok=number(value,version)&&(version>=1&&version<=8);break;
            case 1:
                for(auto preset:{WorldGraphicsPreset::Performance,WorldGraphicsPreset::Balanced,WorldGraphicsPreset::Cinematic,WorldGraphicsPreset::Custom})
                    if(value==world_graphics_preset_name(preset)){next.preset=preset;ok=true;break;}
                break;
            case 2:ok=boolean(value,next.auto_quality);break;
            case 3:ok=number(value,next.target_fps);break;
            case 4:ok=number(value,next.render_scale);break;
            case 5:ok=number(value,next.min_render_scale);break;
            case 6:ok=number(value,next.shadow_side);break;
            case 7:ok=boolean(value,next.reflections);break;
            case 8:ok=boolean(value,next.contact_shadows);break;
            case 9:ok=boolean(value,next.volumetric_fog);break;
            case 10:ok=boolean(value,next.clouds);break;
            case 11:ok=number(value,next.reflection_scale);break;
            case 12:
                if(value=="Windowed"){next.display_mode=WorldDisplayMode::Windowed;ok=true;}
                else if(value=="Borderless"){next.display_mode=WorldDisplayMode::Borderless;ok=true;}
                break;
            case 13:ok=number(value,next.window_width);break;
            case 14:ok=number(value,next.window_height);break;
            case 15:ok=boolean(value,next.vsync);break;
            case 16:
                for(auto distance:{WorldFoliageDistance::Low,WorldFoliageDistance::Medium,WorldFoliageDistance::High,WorldFoliageDistance::Ultra})
                    if(value==world_foliage_distance_name(distance)){next.foliage_distance=distance;ok=true;break;}
                break;
            case 17:
                for(auto density:{WorldFoliageDensity::Low,WorldFoliageDensity::Medium,WorldFoliageDensity::High,WorldFoliageDensity::Ultra})
                    if(value==world_foliage_density_name(density)){next.foliage_density=density;ok=true;break;}
                break;
            case 18:ok=number(value,next.frame_rate_limit);break;
            case 19:
                for(auto quality:{WorldCharacterTextures::Low,WorldCharacterTextures::Medium,WorldCharacterTextures::High,WorldCharacterTextures::Ultra})
                    if(value==world_character_textures_name(quality)){next.character_textures=quality;ok=true;break;}
                break;
            case 20:
                for(auto quality:{WorldWaterQuality::Low,WorldWaterQuality::Medium,WorldWaterQuality::High,WorldWaterQuality::Ultra})
                    if(value==world_water_quality_name(quality)){next.water_quality=quality;ok=true;break;}
                break;
            case 21:
                for(auto quality:{WorldEnvironmentEffects::Low,WorldEnvironmentEffects::Medium,WorldEnvironmentEffects::High,WorldEnvironmentEffects::Ultra})
                    if(value==world_environment_effects_name(quality)){next.environment_effects=quality;ok=true;break;}
                break;
            case 22:
                for(auto quality:{WorldEnvironmentTextures::Low,WorldEnvironmentTextures::Medium,WorldEnvironmentTextures::High,WorldEnvironmentTextures::Ultra})
                    if(value==world_environment_textures_name(quality)){next.environment_textures=quality;ok=true;break;}
                break;
        }
        if(!ok)return false;
    }
    if(version<=5)next.water_quality=WorldWaterQuality::High;
    if(version<=3){next.foliage_density=WorldFoliageDensity::Ultra;next.frame_rate_limit=0;}
    if(seen!=(version==1?all_fields:version==2?version2_fields:version==3?version3_fields:version==4?version4_fields:version==5?version5_fields:version==6?version6_fields:version==7?version7_fields:version8_fields)||!world_graphics_settings_valid(next))return false;
    out=next;return true;
}
}

WorldGraphicsSettings world_graphics_settings_default(){return {};}
const char *world_environment_effects_name(WorldEnvironmentEffects value){
    switch(value){case WorldEnvironmentEffects::Low:return "Low";case WorldEnvironmentEffects::Medium:return "Medium";case WorldEnvironmentEffects::High:return "High";case WorldEnvironmentEffects::Ultra:return "Ultra";}return nullptr;
}
WorldEnvironmentEffectsProfile world_environment_effects_profile(WorldEnvironmentEffects value){
    switch(value){
        case WorldEnvironmentEffects::Low:return {48,24,100,35,.75f,4};
        case WorldEnvironmentEffects::Medium:return {96,48,160,50,1.f,6};
        case WorldEnvironmentEffects::High:return {160,96,240,70,1.15f,8};
        case WorldEnvironmentEffects::Ultra:return {224,160,300,95,1.25f,12};
    }
    return {0,0,0,0,0,0};
}
const char *world_water_quality_name(WorldWaterQuality value){
    switch(value){case WorldWaterQuality::Low:return "Low";case WorldWaterQuality::Medium:return "Medium";case WorldWaterQuality::High:return "High";case WorldWaterQuality::Ultra:return "Ultra";}return nullptr;
}
WorldWaterQualityProfile world_water_quality_profile(WorldWaterQuality value){
    switch(value){
        case WorldWaterQuality::Low:return {false,.25f,0,0,false};
        case WorldWaterQuality::Medium:return {true,.25f,12,4,false};
        case WorldWaterQuality::High:return {true,.5f,24,6,true};
        case WorldWaterQuality::Ultra:return {true,1.f,32,8,true};
    }
    return {}; // Invalid enum is rejected by settings_valid; policy fails closed.
}
const char *world_character_textures_name(WorldCharacterTextures value){
    switch(value){case WorldCharacterTextures::Low:return "Low";case WorldCharacterTextures::Medium:return "Medium";case WorldCharacterTextures::High:return "High";case WorldCharacterTextures::Ultra:return "Ultra";}return nullptr;
}
unsigned world_character_texture_edge(WorldCharacterTextures value){
    switch(value){case WorldCharacterTextures::Low:return 512;case WorldCharacterTextures::Medium:return 1024;case WorldCharacterTextures::High:return 2048;case WorldCharacterTextures::Ultra:return 4096;}return 0;
}
const char *world_environment_textures_name(WorldEnvironmentTextures value){
    switch(value){case WorldEnvironmentTextures::Low:return "Low";case WorldEnvironmentTextures::Medium:return "Medium";case WorldEnvironmentTextures::High:return "High";case WorldEnvironmentTextures::Ultra:return "Ultra";}return nullptr;
}
unsigned world_environment_texture_edge(WorldEnvironmentTextures value){
    switch(value){case WorldEnvironmentTextures::Low:return 512;case WorldEnvironmentTextures::Medium:return 1024;case WorldEnvironmentTextures::High:return 2048;case WorldEnvironmentTextures::Ultra:return 4096;}return 0;
}
std::uint64_t world_environment_texture_budget_bytes(WorldEnvironmentTextures value){
    switch(value){case WorldEnvironmentTextures::Low:return 1024ull*1024*1024;case WorldEnvironmentTextures::Medium:return 1280ull*1024*1024;case WorldEnvironmentTextures::High:return 1536ull*1024*1024;case WorldEnvironmentTextures::Ultra:return 2048ull*1024*1024;}return 0;
}
const char *world_graphics_preset_name(WorldGraphicsPreset preset){
    switch(preset){
        case WorldGraphicsPreset::Performance:return "Performance";
        case WorldGraphicsPreset::Balanced:return "Balanced";
        case WorldGraphicsPreset::Cinematic:return "Cinematic";
        case WorldGraphicsPreset::Custom:return "Custom";
    }
    return nullptr;
}
const char *world_foliage_distance_name(WorldFoliageDistance distance){
    switch(distance){
        case WorldFoliageDistance::Low:return "Low";
        case WorldFoliageDistance::Medium:return "Medium";
        case WorldFoliageDistance::High:return "High";
        case WorldFoliageDistance::Ultra:return "Ultra";
    }
    return nullptr;
}
float world_foliage_distance_scale(WorldFoliageDistance distance){
    switch(distance){
        case WorldFoliageDistance::Low:return .65f;
        case WorldFoliageDistance::Medium:return 1.f;
        case WorldFoliageDistance::High:return 1.5f;
        case WorldFoliageDistance::Ultra:return 2.f;
    }
    return 1.f;
}
const char *world_foliage_density_name(WorldFoliageDensity density){
    switch(density){
        case WorldFoliageDensity::Low:return "Low";
        case WorldFoliageDensity::Medium:return "Medium";
        case WorldFoliageDensity::High:return "High";
        case WorldFoliageDensity::Ultra:return "Ultra";
    }
    return nullptr;
}
float world_foliage_density_scale(WorldFoliageDensity density){
    switch(density){
        case WorldFoliageDensity::Low:return .35f;
        case WorldFoliageDensity::Medium:return .60f;
        case WorldFoliageDensity::High:return .80f;
        case WorldFoliageDensity::Ultra:return 1.f;
    }
    return 1.f;
}
const char *world_graphics_frame_limit_name(int fps){
    switch(fps){
        case 0:return "Off";
        case 30:return "30 FPS";
        case 45:return "45 FPS";
        case 60:return "60 FPS";
        case 90:return "90 FPS";
        case 120:return "120 FPS";
        case 144:return "144 FPS";
    }
    return nullptr;
}
const char *world_graphics_shadow_quality_name(int side){
    switch(side){
        case 1024:return "Low";
        case 2048:return "Medium";
        case 3072:return "High";
        case 4096:return "Ultra";
    }
    return nullptr;
}
int world_graphics_effective_target_fps(const WorldGraphicsSettings &s,int refresh_hz){
    int target=s.target_fps;
    if(s.frame_rate_limit>0&&s.frame_rate_limit<target)target=s.frame_rate_limit;
    if(s.vsync&&refresh_hz>0&&refresh_hz<target)target=refresh_hz;
    return target;
}
bool world_graphics_settings_valid(const WorldGraphicsSettings &s){
    return world_environment_textures_name(s.environment_textures)&&world_environment_effects_name(s.environment_effects)&&world_water_quality_name(s.water_quality)&&world_character_textures_name(s.character_textures)&&world_graphics_preset_name(s.preset)&&world_foliage_distance_name(s.foliage_distance)&&world_foliage_density_name(s.foliage_density)&&
        (s.target_fps==30||s.target_fps==45||s.target_fps==60||s.target_fps==90||s.target_fps==120||s.target_fps==144)&&
        world_graphics_frame_limit_name(s.frame_rate_limit)&&
        std::isfinite(s.render_scale)&&s.render_scale>=.70f&&s.render_scale<=1.f&&
        std::isfinite(s.min_render_scale)&&s.min_render_scale>=.70f&&s.min_render_scale<=s.render_scale&&
        world_graphics_shadow_quality_name(s.shadow_side)&&
        std::isfinite(s.reflection_scale)&&(s.reflection_scale==.25f||s.reflection_scale==.5f||s.reflection_scale==1.f)&&
        (s.display_mode==WorldDisplayMode::Windowed||s.display_mode==WorldDisplayMode::Borderless)&&
        ((s.window_width==0&&s.window_height==0)||(s.window_width>=640&&s.window_width<=7680&&s.window_height>=480&&s.window_height<=4320));
}
bool world_graphics_settings_apply_preset(WorldGraphicsSettings *s,WorldGraphicsPreset preset){
    if(!s||!world_graphics_settings_valid(*s)||!world_graphics_preset_name(preset))return false;
    auto next=*s;
    if(preset!=WorldGraphicsPreset::Custom){
        next.render_scale=preset==WorldGraphicsPreset::Performance?.85f:1.f;
        next.min_render_scale=.70f;
        next.foliage_distance=preset==WorldGraphicsPreset::Performance?WorldFoliageDistance::Low:
            preset==WorldGraphicsPreset::Cinematic?WorldFoliageDistance::Ultra:WorldFoliageDistance::Medium;
        next.foliage_density=preset==WorldGraphicsPreset::Performance?WorldFoliageDensity::Low:
            preset==WorldGraphicsPreset::Cinematic?WorldFoliageDensity::Ultra:WorldFoliageDensity::High;
        next.water_quality=preset==WorldGraphicsPreset::Performance?WorldWaterQuality::Low:
            preset==WorldGraphicsPreset::Cinematic?WorldWaterQuality::Ultra:WorldWaterQuality::Medium;
        next.environment_effects=preset==WorldGraphicsPreset::Performance?WorldEnvironmentEffects::Low:
            preset==WorldGraphicsPreset::Cinematic?WorldEnvironmentEffects::Ultra:WorldEnvironmentEffects::Medium;
        next.shadow_side=preset==WorldGraphicsPreset::Performance?1024:(preset==WorldGraphicsPreset::Cinematic?4096:2048);
        next.reflections=next.volumetric_fog=next.clouds=preset!=WorldGraphicsPreset::Performance;
        next.contact_shadows=preset!=WorldGraphicsPreset::Performance;
        next.reflection_scale=preset==WorldGraphicsPreset::Cinematic?1.f:.25f;
    }
    next.preset=preset;*s=next;return true;
}
WorldGraphicsSettingsLoadResult world_graphics_settings_load(const char *path,WorldGraphicsSettings *s,char *error,std::size_t capacity){
    message(error,capacity,"");
    if(!s||!valid_path(path)){message(error,capacity,"Invalid graphics settings load arguments");return WorldGraphicsSettingsLoadResult::Rejected;}
    const int fd=vw_open(path,O_RDONLY|O_CLOEXEC|O_NONBLOCK);
    if(fd<0){
        if(errno==ENOENT)return WorldGraphicsSettingsLoadResult::Missing;
        message(error,capacity,"Could not open graphics settings");return WorldGraphicsSettingsLoadResult::IOError;
    }
    struct stat info{};
    if(fstat(fd,&info)||!S_ISREG(info.st_mode)){
        close(fd);message(error,capacity,"Graphics settings is not a readable regular file");return WorldGraphicsSettingsLoadResult::IOError;
    }
    char bytes[world_graphics_settings_max_bytes+1];std::size_t count=0;
    bool io_error=false;
    while(count<sizeof(bytes)){
        const auto n=read(fd,bytes+count,sizeof(bytes)-count);
        if(n<0){if(errno==EINTR)continue;io_error=true;break;}
        if(!n)break;
        count+=std::size_t(n);
    }
    if(close(fd))io_error=true;
    if(io_error){message(error,capacity,"Could not read graphics settings");return WorldGraphicsSettingsLoadResult::IOError;}
    WorldGraphicsSettings next;
    if(count>world_graphics_settings_max_bytes||!parse(bytes,count,next)){
        message(error,capacity,"Rejected graphics settings: invalid, incomplete or oversized file");
        return WorldGraphicsSettingsLoadResult::Rejected;
    }
    *s=next;return WorldGraphicsSettingsLoadResult::Loaded;
}
bool world_graphics_settings_save(const char *path,const WorldGraphicsSettings &s,char *error,std::size_t capacity){
    message(error,capacity,"");
    if(!valid_path(path)||!world_graphics_settings_valid(s)){
        message(error,capacity,"Invalid graphics settings save arguments");return false;
    }
    // Numeric serialization uses to_chars, so the file format is unaffected by
    // a desktop/user locale changing the process decimal separator.
    char scale[32],minimum[32],reflection[32];
    const auto a=std::to_chars(scale,scale+sizeof(scale)-1,s.render_scale);
    const auto b=std::to_chars(minimum,minimum+sizeof(minimum)-1,s.min_render_scale);
    const auto c=std::to_chars(reflection,reflection+sizeof(reflection)-1,s.reflection_scale);
    if(a.ec!=std::errc{}||b.ec!=std::errc{}||c.ec!=std::errc{}){
        message(error,capacity,"Could not encode graphics settings");return false;
    }
    *a.ptr=*b.ptr=*c.ptr=0;
    char bytes[world_graphics_settings_max_bytes];
    const int length=std::snprintf(bytes,sizeof(bytes),
        "version=8\npreset=%s\nauto_quality=%d\ntarget_fps=%d\nrender_scale=%s\nmin_render_scale=%s\nshadow_side=%d\nreflections=%d\ncontact_shadows=%d\nvolumetric_fog=%d\nclouds=%d\nreflection_scale=%s\ndisplay_mode=%s\nwindow_width=%d\nwindow_height=%d\nvsync=%d\nfoliage_distance=%s\nfoliage_density=%s\nframe_rate_limit=%d\ncharacter_textures=%s\nwater_quality=%s\nenvironment_effects=%s\nenvironment_textures=%s\n",
        world_graphics_preset_name(s.preset),int(s.auto_quality),s.target_fps,scale,minimum,s.shadow_side,
        int(s.reflections),int(s.contact_shadows),int(s.volumetric_fog),int(s.clouds),reflection,
        s.display_mode==WorldDisplayMode::Borderless?"Borderless":"Windowed",s.window_width,s.window_height,int(s.vsync),world_foliage_distance_name(s.foliage_distance),world_foliage_density_name(s.foliage_density),s.frame_rate_limit,world_character_textures_name(s.character_textures),world_water_quality_name(s.water_quality),world_environment_effects_name(s.environment_effects),world_environment_textures_name(s.environment_textures));
    if(length<0||std::size_t(length)>=sizeof(bytes)){
        message(error,capacity,"Could not encode graphics settings");return false;
    }
    char temporary[4096];
    const int path_length=std::snprintf(temporary,sizeof(temporary),"%s.tmp.XXXXXX",path);
    if(path_length<0||std::size_t(path_length)>=sizeof(temporary)){
        message(error,capacity,"Graphics settings path is too long");return false;
    }
    const int fd=mkstemp(temporary);
    if(fd<0){message(error,capacity,"Could not create temporary graphics settings");return false;}
    bool ok=true;std::size_t count=0;
    while(count<std::size_t(length)){
        const auto n=write(fd,bytes+count,std::size_t(length)-count);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){ok=false;break;}
        count+=std::size_t(n);
    }
    if(ok&&fsync(fd))ok=false;
    if(close(fd))ok=false;
    if(ok&&vw_replace(temporary,path))ok=false;
    if(!ok){unlink(temporary);message(error,capacity,"Could not atomically save graphics settings");}
    return ok;
}
