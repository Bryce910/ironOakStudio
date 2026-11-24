#ifndef VALWOOD_WORLD_OPENING_INTERIOR_H
#define VALWOOD_WORLD_OPENING_INTERIOR_H

// Optional project-owned room domains. No filesystem or renderer state here.
// Absent settings leave the existing atlas lighting/streaming path unchanged.
#include <array>
#include <cmath>
#include <cstring>
#include <istream>
#include <locale>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace world_opening_interior {
using Vec=std::array<double,3>;
struct Box {Vec minimum={},maximum={};};
struct Probe {Vec position={};Box box;};
struct Config {
    bool enabled=false,release_city_fallbacks=false;
    Box bounds;
    std::vector<Probe> probes;
};
inline bool finite(Vec p){for(double x:p)if(!std::isfinite(x)||std::abs(x)>1.e7)return false;return true;}
inline bool valid(Box b){
    if(!finite(b.minimum)||!finite(b.maximum))return false;
    for(unsigned k=0;k<3;k++)if(b.maximum[k]-b.minimum[k]<.1||b.maximum[k]-b.minimum[k]>1024.)return false;
    return true;
}
inline bool contains(Box b,Vec p){
    if(!finite(p))return false;
    for(unsigned k=0;k<3;k++)if(p[k]<=b.minimum[k]||p[k]>=b.maximum[k])return false;
    return true;
}
inline bool contains(const Config &c,Vec p){return c.enabled&&contains(c.bounds,p);}
inline bool city_alias(const char *p){return p&&std::strncmp(p,"nm_city_",8)==0;}
// The prologue apartment and outdoor city have disjoint residency domains.
inline bool apartment_alias(const char *p){
    return p&&(std::strncmp(p,"nm_opening_home",15)==0||std::strncmp(p,"nm_opening_reference_",21)==0);
}
inline bool suspend_city(const Config &c,const char *alias,Vec camera){
    return c.release_city_fallbacks&&city_alias(alias)&&contains(c,camera);
}
inline bool parse(std::istream &input,Config &result,std::string &error){
    Config next;bool header=false,bounds=false,release=false,end=false;unsigned lines=0;
    std::string line;
    auto fail=[&](const char *message){error=message;return false;};
    while(std::getline(input,line)){
        if(++lines>64||line.size()>512)return fail("Oversized interior settings");
        if(line.empty()||line.find_first_not_of(" \t\r")==std::string::npos)continue;
        std::istringstream row(line);row.imbue(std::locale::classic());std::string kind,extra;row>>kind;
        if(end)return fail("Records after interior end");
        if(!header){unsigned version=0;if(kind!="VALWOOD_OPENING_INTERIOR"||!(row>>version)||version!=1||(row>>extra))return fail("Invalid interior header");header=true;continue;}
        auto vec=[&](Vec &v){return bool(row>>v[0]>>v[1]>>v[2]);};
        auto box=[&](Box &b){return vec(b.minimum)&&vec(b.maximum);};
        if(kind=="bounds"){
            if(bounds||!box(next.bounds)||!valid(next.bounds))return fail("Invalid or duplicate interior bounds");
            bounds=true;
        }else if(kind=="probe"){
            Probe p;if(next.probes.size()>=8||!vec(p.position)||!box(p.box)||!valid(p.box)||!contains(p.box,p.position))return fail("Invalid interior probe");
            next.probes.push_back(p);
        }else if(kind=="release_city_fallbacks"){
            unsigned value=0;if(release||!(row>>value)||value>1)return fail("Invalid interior city fallback policy");
            next.release_city_fallbacks=value==1;release=true;
        }else if(kind=="end")end=true;
        else return fail("Unknown interior setting");
        if(row>>extra)return fail("Unexpected interior record fields");
    }
    if(input.bad()||!header||!bounds||!end||next.probes.empty())return fail("Incomplete interior settings");
    for(const auto &p:next.probes)for(unsigned k=0;k<3;k++)
        if(p.box.minimum[k]<next.bounds.minimum[k]||p.box.maximum[k]>next.bounds.maximum[k])return fail("Probe domain escapes interior bounds");
    next.enabled=true;result=std::move(next);error.clear();return true;
}
}
#endif
