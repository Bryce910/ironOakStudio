#ifndef VALWOOD_WORLD_OPENING_ROOM_LIGHTS_H
#define VALWOOD_WORLD_OPENING_ROOM_LIGHTS_H
// Authored apartment electrical controls. Device reads, filesystem IO and
// rendering stay in the opening host; this helper only validates targeting,
// debounce and the mapping from practical fixtures to the four room circuits.
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <sstream>
#include <istream>
#include <vector>

namespace world_opening_room_lights {
constexpr unsigned room_count=4,all_on=(1u<<room_count)-1u;
// Read by the renderer's bounded home-fixture shader branch. Defaults leave
// every other project/material unchanged, including editor and showcase views.
inline std::array<float,room_count> emission_weights={{1,1,1,1}};
inline void render_mask(bool enabled,unsigned mask){for(unsigned i=0;i<room_count;++i)emission_weights[i]=!enabled||(mask&(1u<<i))?1.f:0.f;}
struct Vec {double x=0,y=0,z=0;};
struct Switch {const char *name;Vec position,normal;};
// Positions are on the visible face of their physical switch plates. The
// inward-facing normal prevents reaching a switch through its partition.
inline std::array<Switch,room_count> switches={{{"Living room",{-3499.10,601.40,-10995.108},{0,0,-1}},
    {"Kitchen",{-3498.838,601.40,-11002.10},{-1,0,0}},
    {"Bedroom",{-3496.08,601.40,-11001.588},{0,0,-1}},
    {"Bathroom",{-3496.212,601.38,-10998.57},{1,0,0}}}};
struct Practical {Vec position;unsigned room;};
// The cool window key is intentionally absent: daylight is not an electrical
// circuit. Matching positions instead of array indices survives other lights
// being inserted into the Studio document.
inline std::vector<Practical> practicals={{{-3504.69,601.60,-10996.48},0},
    {{-3501.,602.95,-10998.70},0},{{-3500.22,601.58,-11004.48},1},
    {{-3500.22,602.45,-11001.25},1},{{-3494.97,600.99,-11003.97},2},
    {{-3495.65,602.55,-10999.55},3}};
inline bool authored_controls=false;
inline std::array<Vec,room_count> room_min={},room_max={};
inline std::array<std::vector<Vec>,5> authored_paths;
inline bool valid(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline bool valid_mask(unsigned mask){return mask<=all_on;}
inline bool on(unsigned mask,unsigned room){return room<room_count&&(mask&(1u<<room))!=0;}
inline int practical_room(Vec p){
    if(!valid(p))return -1;
    for(const auto &fixture:practicals)if(std::abs(p.x-fixture.position.x)<.015&&std::abs(p.y-fixture.position.y)<.015&&std::abs(p.z-fixture.position.z)<.015)return int(fixture.room);
    return -1;
}
inline int target(Vec eye,Vec forward){
    if(!valid(eye)||!valid(forward))return -1;
    const double direction_length=std::sqrt(forward.x*forward.x+forward.y*forward.y+forward.z*forward.z);
    if(!std::isfinite(direction_length)||direction_length<.0001)return -1;
    int closest=-1;double closest_distance=std::numeric_limits<double>::max();
    for(unsigned i=0;i<room_count;++i){
        const auto &s=switches[i];const Vec d{s.position.x-eye.x,s.position.y-eye.y,s.position.z-eye.z};
        // The bathroom plate also lies close to its front partition: the
        // player must have entered the room instead of aiming through that
        // perpendicular wall from the living room.
        if(authored_controls){
            const auto &a=room_min[i],&b=room_max[i];
            if(eye.x<a.x||eye.y<a.y||eye.z<a.z||eye.x>b.x||eye.y>b.y||eye.z>b.z)continue;
        }else if(i==3&&(eye.x<=-3496.21||eye.x>=-3494.60||eye.z<=-11001.40||eye.z>=-10998.28))continue;
        const double distance=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
        const double front=-(d.x*s.normal.x+d.y*s.normal.y+d.z*s.normal.z);
        if(distance<.10||distance>1.65||front<.05)continue;
        const double aim=(d.x*forward.x+d.y*forward.y+d.z*forward.z)/(distance*direction_length);
        if(aim>=.985&&distance<closest_distance){closest=int(i);closest_distance=distance;}
    }
    return closest;
}

// Optional authored room controls for source-scene layouts. Parse into temporary
// storage, then publish atomically; a malformed file cannot mutate defaults.
inline bool parse_authored(std::istream &input,std::string &error){
    auto fail=[&](const char *why){error=why;return false;};
    std::string line;size_t bytes=0;
    if(!std::getline(input,line)||line!="VALWOOD_OPENING_ROOM_CONTROLS 1")return fail("Invalid room-controls header");
    auto next_switches=switches;
    std::array<Vec,room_count> minima={},maxima={};
    std::array<bool,room_count> seen={},powered={};
    std::vector<Practical> fixtures;
    std::array<std::vector<Vec>,5> paths;
    bool ended=false;
    auto bounded=[](Vec p){return valid(p)&&std::abs(p.x)<=1000000&&std::abs(p.y)<=1000000&&std::abs(p.z)<=1000000;};
    auto vec=[](std::istringstream &r,Vec &v){return bool(r>>v.x>>v.y>>v.z);};
    while(std::getline(input,line)){
        bytes+=line.size()+1;if(bytes>65536||line.size()>1024)return fail("Oversized room-controls file");
        std::istringstream row(line);std::string tag;row>>tag;
        if(tag.empty())continue;
        if(ended)return fail("Trailing room-controls records");
        if(tag=="end"){row>>std::ws;if(!row.eof())return fail("Malformed room-controls end");ended=true;continue;}
        unsigned id=99;if(!(row>>id))return fail("Missing room-controls index");
        if(tag=="switch"){
            Vec position,normal,a,b;
            if(id>=room_count||seen[id]||!vec(row,position)||!vec(row,normal)||!vec(row,a)||!vec(row,b))return fail("Malformed or duplicate room switch");
            if(!bounded(position)||!bounded(a)||!bounded(b)||!valid(normal)||a.x>=b.x||a.y>=b.y||a.z>=b.z)return fail("Invalid room switch bounds");
            const double length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
            if(std::abs(length-1.)>.001)return fail("Room switch normal must be unit length");
            if(position.x<a.x-.1||position.y<a.y-.1||position.z<a.z-.1||position.x>b.x+.1||position.y>b.y+.1||position.z>b.z+.1)return fail("Switch lies outside room");
            next_switches[id].position=position;next_switches[id].normal=normal;minima[id]=a;maxima[id]=b;seen[id]=true;
        }else if(tag=="practical"){
            Vec position;if(id>=room_count||!vec(row,position)||!bounded(position)||fixtures.size()>=32)return fail("Invalid room practical");
            for(const auto &old:fixtures)if(std::abs(old.position.x-position.x)<.015&&std::abs(old.position.y-position.y)<.015&&std::abs(old.position.z-position.z)<.015)return fail("Duplicate room practical");
            fixtures.push_back({position,id});powered[id]=true;
        }else if(tag=="path"){
            Vec position;if(id>=paths.size()||!vec(row,position)||!bounded(position)||paths[id].size()>=128)return fail("Invalid room walking path");
            paths[id].push_back(position);
        }else return fail("Unknown room-controls record");
        row>>std::ws;if(!row.eof())return fail("Trailing room-controls fields");
    }
    if(!ended)return fail("Missing room-controls end");
    for(unsigned i=0;i<room_count;++i)if(!seen[i]||!powered[i])return fail("Every room needs a switch and a practical");
    for(const auto &path:paths)if(path.size()<2)return fail("Every room walking path needs at least two nodes");
    switches=next_switches;room_min=minima;room_max=maxima;practicals=fixtures;authored_paths=paths;authored_controls=true;error.clear();return true;
}

class Controls {
    bool armed_=false,last_down_=false;
public:
    std::array<unsigned,room_count> toggles={};
    int tick(bool playable,bool down,int selected){
        const bool pressed=down&&!last_down_;last_down_=down;
        if(!playable){armed_=false;return -1;}
        if(!down)armed_=true;
        if(pressed&&armed_&&selected>=0&&selected<int(room_count)){
            armed_=false;++toggles[unsigned(selected)];return selected;
        }
        return -1;
    }
    void release(){armed_=false;}
};
}
#endif
