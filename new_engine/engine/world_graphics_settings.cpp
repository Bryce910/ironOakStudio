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
    "display_mode","window_width","window_height","vsync"};
constexpr unsigned all_fields=(1u<<12)-1;
constexpr unsigned version2_fields=(1u<<16)-1;

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
        unsigned field=0;while(field<16&&key!=keys[field])field++;
        if(field==16||(seen&(1u<<field)))return false;
        seen|=1u<<field;
        bool ok=false;
        switch(field){
            case 0:ok=number(value,version)&&(version==1||version==2);break;
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
        }
        if(!ok)return false;
    }
    if(seen!=(version==1?all_fields:version2_fields)||!world_graphics_settings_valid(next))return false;
    out=next;return true;
}
}

WorldGraphicsSettings world_graphics_settings_default(){return {};}
const char *world_graphics_preset_name(WorldGraphicsPreset preset){
    switch(preset){
        case WorldGraphicsPreset::Performance:return "Performance";
        case WorldGraphicsPreset::Balanced:return "Balanced";
        case WorldGraphicsPreset::Cinematic:return "Cinematic";
        case WorldGraphicsPreset::Custom:return "Custom";
    }
    return nullptr;
}
bool world_graphics_settings_valid(const WorldGraphicsSettings &s){
    return world_graphics_preset_name(s.preset)&&
        (s.target_fps==45||s.target_fps==60||s.target_fps==90||s.target_fps==120)&&
        std::isfinite(s.render_scale)&&s.render_scale>=.70f&&s.render_scale<=1.f&&
        std::isfinite(s.min_render_scale)&&s.min_render_scale>=.70f&&s.min_render_scale<=s.render_scale&&
        (s.shadow_side==1024||s.shadow_side==2048||s.shadow_side==3072||s.shadow_side==4096)&&
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
        next.shadow_side=preset==WorldGraphicsPreset::Cinematic?4096:2048;
        next.reflections=next.volumetric_fog=next.clouds=true;
        next.contact_shadows=preset!=WorldGraphicsPreset::Performance;
        next.reflection_scale=preset==WorldGraphicsPreset::Performance?.25f:(preset==WorldGraphicsPreset::Balanced?.5f:1.f);
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
        "version=2\npreset=%s\nauto_quality=%d\ntarget_fps=%d\nrender_scale=%s\nmin_render_scale=%s\nshadow_side=%d\nreflections=%d\ncontact_shadows=%d\nvolumetric_fog=%d\nclouds=%d\nreflection_scale=%s\ndisplay_mode=%s\nwindow_width=%d\nwindow_height=%d\nvsync=%d\n",
        world_graphics_preset_name(s.preset),int(s.auto_quality),s.target_fps,scale,minimum,s.shadow_side,
        int(s.reflections),int(s.contact_shadows),int(s.volumetric_fog),int(s.clouds),reflection,
        s.display_mode==WorldDisplayMode::Borderless?"Borderless":"Windowed",s.window_width,s.window_height,int(s.vsync));
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
