#ifndef VALWOOD_WORLD_OPENING_CINEMATIC_SETTINGS_H
#define VALWOOD_WORLD_OPENING_CINEMATIC_SETTINGS_H
#include "world_opening_prologue.h"
#include <fstream>

namespace world_opening_cinematic_settings {
// Optional project input, parsed transactionally. A damaged shot file must not
// leave a partially configured camera sequence or silently change the opening.
inline bool parse(std::istream &input,const world_opening_prologue::Config &base,
                  std::vector<world_opening_prologue::HouseShot> &out,std::string &error){
    using namespace world_opening_prologue;
    std::string bytes;char ch;
    while(input.get(ch)){if(ch=='\0'||bytes.size()>=8192)return detail::fail(error,"Apartment cinematic exceeds bounds");bytes+=ch;}
    if(input.bad()||(input.fail()&&!input.eof()))return detail::fail(error,"Unreadable apartment cinematic");
    std::istringstream file(bytes);file.imbue(std::locale::classic());
    std::string tag,version;
    if(!(file>>tag>>version)||tag!="VALWOOD_HOME_CINEMATIC"||version!="1")return detail::fail(error,"Invalid apartment cinematic header");
    std::vector<HouseShot> staged;bool ended=false;
    auto vec=[&](Vec &v){return bool(file>>v.x>>v.y>>v.z);};
    while(file>>tag){
        if(tag=="end"){ended=true;break;}
        if(tag!="shot"||staged.size()>=6)return detail::fail(error,"Invalid apartment cinematic record");
        HouseShot shot;
        if(!(file>>shot.duration)||!vec(shot.rail.eye_from)||!vec(shot.rail.eye_to)||
           !vec(shot.rail.target_from)||!vec(shot.rail.target_to)||
           !(file>>shot.rail.fov_from>>shot.rail.fov_to))return detail::fail(error,"Malformed apartment camera shot");
        file>>std::ws;
        if(file.peek()!='"'||!(file>>std::quoted(shot.subtitle)))return detail::fail(error,"Apartment shot subtitle must be quoted");
        staged.push_back(std::move(shot));
    }
    file>>std::ws;
    if(!ended||!file.eof()||staged.empty())return detail::fail(error,"Incomplete apartment cinematic");
    Config candidate=base;candidate.house_shots=staged;
    if(!validate(candidate,error))return false;
    const auto &end=staged.back().rail;const auto &handoff=base.house_rail;
    auto same=[](Vec a,Vec b){return std::abs(a.x-b.x)<1e-6&&std::abs(a.y-b.y)<1e-6&&std::abs(a.z-b.z)<1e-6;};
    auto direction=[](Vec eye,Vec target){
        Vec look{target.x-eye.x,target.y-eye.y,target.z-eye.z};
        const double length=std::hypot(std::hypot(look.x,look.z),look.y);
        return Vec{look.x/length,look.y/length,look.z/length};
    };
    // A target describes orientation, not focus distance. The same supported
    // eye, unit look direction and FOV are the same gameplay camera even when
    // one author chooses a target one metre away and another five metres away.
    if(!same(end.eye_to,handoff.eye_to)||!same(direction(end.eye_to,end.target_to),direction(handoff.eye_to,handoff.target_to))||std::abs(end.fov_to-handoff.fov_to)>1e-6)
        return detail::fail(error,"Apartment cinematic must end at the supported gameplay camera");
    out=std::move(staged);error.clear();return true;
}
}
#endif
