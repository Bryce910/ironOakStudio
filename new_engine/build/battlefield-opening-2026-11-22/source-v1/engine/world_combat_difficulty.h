#ifndef VALWOOD_WORLD_COMBAT_DIFFICULTY_H
#define VALWOOD_WORLD_COMBAT_DIFFICULTY_H
#include "engine/world_platform.h"
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace world_combat_difficulty {
enum class Difficulty {Story,Normal,Hard,Expert};
inline constexpr bool valid(Difficulty d){return d>=Difficulty::Story&&d<=Difficulty::Expert;}
inline constexpr const char *name(Difficulty d){switch(d){
    case Difficulty::Story:return "Story";case Difficulty::Normal:return "Normal";
    case Difficulty::Hard:return "Hard";case Difficulty::Expert:return "Expert";
}return nullptr;}
inline constexpr const char *description(Difficulty d){switch(d){
    case Difficulty::Story:return "Slower attacks and more time to change your attack angle.";
    case Difficulty::Normal:return "Balanced attack tempo and readable defensive reactions.";
    case Difficulty::Hard:return "Faster attacks and stronger reads of repeated attack angles.";
    case Difficulty::Expert:return "Fast attacks and tight defensive reactions. Vary your cuts.";
}return "";}
struct Profile {
    float guard_reaction_delay_s,guard_tracking_interval_s,repeated_angle_read_bonus_s;
    float attack_tempo_scale; // Enemy attack duration divisor; Normal1 preserves authored clocks.
    float minimum_reaction_delay_s=.065f;
};
inline constexpr Profile profile(Difficulty d){switch(d){
    case Difficulty::Story:return {.36f,.24f,0.f,.82f};
    case Difficulty::Hard:return {.16f,.12f,.045f,1.12f};
    case Difficulty::Expert:return {.11f,.08f,.06f,1.22f};
    case Difficulty::Normal:return {.24f,.18f,.025f,1.f};
}return {.24f,.18f,.025f,1.f};}
inline constexpr float reaction_delay_s(Difficulty d,bool repeated_same_angle){
    const auto p=profile(d);const float delay=p.guard_reaction_delay_s-(repeated_same_angle?p.repeated_angle_read_bonus_s:0.f);
    return delay<p.minimum_reaction_delay_s?p.minimum_reaction_delay_s:delay;
}
// Arena-only defensive reads. Story/Normal retain their exact accepted
// profile. Faster reads request a source guard transition; physical contact
// still decides whether the moving blade actually intercepts the attack.
inline constexpr Profile arena_guard_profile(Difficulty d){
    if(d==Difficulty::Hard)return {.10f,.10f,.04f,1.12f,.050f};
    if(d==Difficulty::Expert)return {.055f,.065f,.02f,1.22f,.035f};
    return profile(d);
}
inline constexpr float arena_reaction_delay_s(Difficulty d,bool repeated_same_angle){
    const auto p=arena_guard_profile(d);const float delay=p.guard_reaction_delay_s-(repeated_same_angle?p.repeated_angle_read_bonus_s:0.f);
    return delay<p.minimum_reaction_delay_s?p.minimum_reaction_delay_s:delay;
}
// A defender already measuring a visible committed cut may finish raising
// its real guard before choosing an attack. This bounded patience cannot
// cancel Windup/Strike/Recovery, extend a parry stun or deny body damage.
inline constexpr double arena_defensive_hold_s(Difficulty d){return d==Difficulty::Expert?.85:d==Difficulty::Hard?.65:0.;}
inline constexpr bool arena_remembers_repeated_angle(Difficulty d){return d==Difficulty::Expert||d==Difficulty::Hard;}
// Difficulty changes AI timing/reads only. Existing damage assistance stays an
// independent preference: no tier secretly changes health, damage or immunity.
struct Preferences {Difficulty difficulty=Difficulty::Normal;bool story_assist=true;};
inline constexpr bool operator==(const Preferences &a,const Preferences &b){return a.difficulty==b.difficulty&&a.story_assist==b.story_assist;}
inline constexpr bool operator!=(const Preferences &a,const Preferences &b){return !(a==b);}
inline constexpr std::size_t settings_max_bytes=1024;
enum class LoadResult {Loaded,Missing,Rejected,IOError};
namespace detail {
inline std::string_view trim(std::string_view s){
    const auto space=[](char c){return c==' '||c=='\t'||c=='\r';};
    while(!s.empty()&&space(s.front()))s.remove_prefix(1);
    while(!s.empty()&&space(s.back()))s.remove_suffix(1);
    return s;
}
inline bool path_valid(const char *path){return path&&*path&&std::strlen(path)<4000;}
}
// V1 is the legacy assist-only record. V2 requires both difficulty and assist.
// Parse transactionally; rejected/unknown/duplicate/incomplete data changes
// neither the caller's preferences nor any unrelated presentation setting.
inline bool decode(std::string_view file,Preferences &out){
    if(file.size()>settings_max_bytes||file.find('\0')!=std::string_view::npos)return false;
    Preferences next;int version=0;unsigned seen=0;
    while(!file.empty()){
        const auto end=file.find('\n');const auto line=detail::trim(file.substr(0,end));
        if(end==std::string_view::npos)file={};else file.remove_prefix(end+1);
        if(line.empty()||line.front()=='#')continue;
        const auto equal=line.find('=');if(equal==std::string_view::npos)return false;
        const auto key=detail::trim(line.substr(0,equal)),value=detail::trim(line.substr(equal+1));unsigned bit=0;
        if(key=="version"){
            bit=1;const auto r=std::from_chars(value.data(),value.data()+value.size(),version);
            if(r.ec!=std::errc{}||r.ptr!=value.data()+value.size()||(version!=1&&version!=2))return false;
        }else if(key=="difficulty"){
            bit=2;bool found=false;for(auto d:{Difficulty::Story,Difficulty::Normal,Difficulty::Hard,Difficulty::Expert})
                if(value==name(d)){next.difficulty=d;found=true;break;}
            if(!found)return false;
        }else if(key=="story_assist"){
            bit=4;if(value=="0")next.story_assist=false;else if(value=="1")next.story_assist=true;else return false;
        }else return false;
        if(seen&bit)return false;
        seen|=bit;
    }
    if((version!=1&&version!=2)||(version==1&&seen!=5)||(version==2&&seen!=7))return false;
    out=next;return true;
}
inline bool encode(const Preferences &settings,std::string &out){
    if(!valid(settings.difficulty))return false;
    std::string next="version=2\ndifficulty=";next+=name(settings.difficulty);next+="\nstory_assist=";next+=settings.story_assist?'1':'0';next+='\n';out=std::move(next);return true;
}
inline LoadResult load(const char *path,Preferences &out,std::string &error){
    error.clear();if(!detail::path_valid(path)){error="Invalid combat settings path";return LoadResult::Rejected;}
    const int fd=vw_open(path,O_RDONLY|O_CLOEXEC|O_NONBLOCK);
    if(fd<0){if(errno==ENOENT)return LoadResult::Missing;error="Could not open combat settings";return LoadResult::IOError;}
    struct stat info{};if(fstat(fd,&info)||!S_ISREG(info.st_mode)){close(fd);error="Combat settings is not a regular file";return LoadResult::IOError;}
    char bytes[settings_max_bytes+1];std::size_t count=0;bool ok=true;
    while(count<sizeof bytes){const auto n=read(fd,bytes+count,sizeof bytes-count);if(n<0&&errno==EINTR)continue;if(n<0){ok=false;break;}if(!n)break;count+=std::size_t(n);}
    if(close(fd))ok=false;
    if(!ok){error="Could not read combat settings";return LoadResult::IOError;}
    Preferences next;if(!decode(std::string_view(bytes,count),next)){error="Rejected invalid or incomplete combat settings";return LoadResult::Rejected;}
    out=next;return LoadResult::Loaded;
}
inline bool save(const char *path,const Preferences &settings,std::string &error){
    error.clear();std::string bytes;
    if(!detail::path_valid(path)||!encode(settings,bytes)){error="Invalid combat settings";return false;}
    char temporary[4096];const int length=std::snprintf(temporary,sizeof temporary,"%s.tmp.XXXXXX",path);
    if(length<0||std::size_t(length)>=sizeof temporary){error="Combat settings path is too long";return false;}
    const int fd=mkstemp(temporary);if(fd<0){error="Could not create temporary combat settings";return false;}
    bool ok=true;std::size_t count=0;
    while(count<bytes.size()){const auto n=write(fd,bytes.data()+count,bytes.size()-count);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}count+=std::size_t(n);}
    if(ok&&fsync(fd))ok=false;
    if(close(fd))ok=false;
    if(ok&&vw_replace(temporary,path))ok=false;
    if(!ok){unlink(temporary);error="Could not save combat settings";}return ok;
}
}
#endif
