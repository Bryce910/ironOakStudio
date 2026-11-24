#include "engine/world_transform_math.h"
#include "engine/world_authoring_identity.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace world_authoring {
namespace {
using namespace world_entity;
bool ground(Ground callback,const void *data,double x,double z,double &y,std::string &error){
    y=callback?callback(x,z,data):0;
    if(!std::isfinite(y)||y< -2000||y>12000){error="Invalid terrain support while binding entities";return false;}
    return true;
}
bool source_pose(const world_studio::Document &d,const std::vector<Source> &sources,unsigned i,Transform &t,std::string &e){
    if(i>=sources.size()){e="Referenced source model is missing";return false;}
    const auto &s=sources[i];t={s.x,s.y,s.z,s.yaw,s.scale,s.pitch,s.roll,s.sx,s.sy,s.sz};
    for(const auto &o:d.overrides)if(o.index==i){t.x=o.x;t.y+=o.offset_y-s.offset_y;t.z=o.z;t.yaw=o.yaw;t.scale=o.scale;vw_transform::extra(t,o);break;}
    return valid_transform(t);
}
bool append(Graph &g,Binding binding,const Transform &t,const std::string &name,const std::string &key,std::vector<std::pair<Binding,Transform>> &poses,std::string &e){
    auto at=std::find_if(g.nodes.begin(),g.nodes.end(),[&](const Node &n){return n.binding==binding;});
    if(at==g.nodes.end()){
        Id id=0;if(!add(g,0,name,binding,t,id,e))return false;
        at=std::find_if(g.nodes.begin(),g.nodes.end(),[&](const Node &n){return n.id==id;});
    }
    if(binding.kind==Kind::SourceModel){
        if(!at->source_key.empty()&&at->source_key!=key){e="Source identity changed; repair this reference explicitly";return false;}
        at->source_key=key;
    }
    poses.push_back({binding,t});return true;
}
std::vector<unsigned> referenced(const world_studio::Document &d,const Graph &g){
    std::vector<unsigned> out;
    for(const auto &o:d.overrides)out.push_back(o.index);
    for(const auto &group:d.groups)for(unsigned key:group.members)if(key<WORLD_ADDED_KEY_BASE)out.push_back(key);
    for(const auto &n:g.nodes)if(n.binding.kind==Kind::SourceModel)out.push_back(n.binding.index);
    std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());return out;
}
}
std::string source_key(const Source &s){
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)
        <<std::quoted(s.alias)<<' '<<std::quoted(s.file)<<' '<<s.x<<' '<<s.z<<' '
        <<s.yaw<<' '<<s.scale<<' '<<s.burial<<' '<<s.elevation<<' '<<s.offset_y;
    if(s.pitch!=0||s.roll!=0||s.sx!=1||s.sy!=1||s.sz!=1)out<<' '<<s.pitch<<' '<<s.roll<<' '<<s.sx<<' '<<s.sy<<' '<<s.sz;
    return out.str();
}
bool erase_record(world_studio::Document &d,Binding b,std::string &e){
    e.clear();if(!world_entity::editable(d.entities,b)){e="This entity or its parent is locked";return false;}
    size_t count=0;
    switch(b.kind){
        case Kind::Placement:count=d.placements.size();break;
        case Kind::Light:count=d.lights.size();break;
        case Kind::Actor:count=d.actors.size();break;
        case Kind::Particle:count=d.particles.size();break;
        case Kind::Sound:count=d.sounds.size();break;
        case Kind::Weather:count=d.weather_areas.size();break;
        case Kind::Water:count=d.waters.size();break;
        case Kind::Navigation:count=d.navigation.size();break;
        default:e="This scene record cannot be removed";return false;
    }
    if(b.index>=count){e="Scene record no longer exists";return false;}
    auto next=d;
    if(!erased(next.entities,b,e))return false;
    if(b.kind==Kind::Placement){
        next.placements.erase(next.placements.begin()+b.index);const unsigned key=WORLD_ADDED_KEY_BASE+b.index;
        for(auto &g:next.groups){g.members.erase(std::remove(g.members.begin(),g.members.end(),key),g.members.end());for(auto &k:g.members)if(k>key)--k;}
        next.groups.erase(std::remove_if(next.groups.begin(),next.groups.end(),[](const auto &g){return g.members.empty();}),next.groups.end());
    }else{
        switch(b.kind){
            case Kind::Light:next.lights.erase(next.lights.begin()+b.index);break;
            case Kind::Actor:next.actors.erase(next.actors.begin()+b.index);break;
            case Kind::Particle:next.particles.erase(next.particles.begin()+b.index);break;
            case Kind::Sound:next.sounds.erase(next.sounds.begin()+b.index);break;
            case Kind::Weather:next.weather_areas.erase(next.weather_areas.begin()+b.index);break;
            case Kind::Water:next.waters.erase(next.waters.begin()+b.index);break;
            case Kind::Navigation:
                for(auto &a:next.actors){if(a.navigation==int(b.index))a.navigation=-1;else if(a.navigation>int(b.index))--a.navigation;}
                next.navigation.erase(next.navigation.begin()+b.index);break;
            default:break;
        }
    }
    if(!world_studio::valid(next,e))return false;
    d=std::move(next);return true;
}
bool synchronize(const world_studio::Document &d,Graph &g,const std::vector<Source> &sources,Ground callback,const void *data,std::string &e){
    e.clear();if(sources.size()>WORLD_ADDED_KEY_BASE||!world_studio::valid(d,e)||!validate(g,&d,e))return false;
    Graph next=g;std::vector<std::pair<Binding,Transform>> poses;double y=0;
    std::vector<std::pair<Id,Transform>> prior;if(!world_transforms(g,prior,e))return false;
    auto retained=[&](Binding binding,Transform fallback){for(const auto &node:g.nodes)if(node.binding==binding)for(const auto &p:prior)if(p.first==node.id)return p.second;return fallback;};
    for(unsigned i:referenced(d,g)){
        Transform t;if(!source_pose(d,sources,i,t,e))return false;
        if(!append(next,{Kind::SourceModel,i},t,sources[i].alias,source_key(sources[i]),poses,e))return false;
    }
    for(unsigned i=0;i<d.placements.size();i++){
        const auto &p=d.placements[i];if(!ground(callback,data,p.x,p.z,y,e))return false;
        if(!append(next,{Kind::Placement,i},{p.x,y+p.offset_y+p.elevation-p.burial,p.z,p.yaw,p.scale,p.pitch,p.roll,p.sx,p.sy,p.sz},p.alias,{},poses,e))return false;
    }
    for(unsigned i=0;i<d.lights.size();i++){
        const auto &p=d.lights[i];if(!append(next,{Kind::Light,i},{p.x,p.y,p.z,p.yaw,1},"Light "+std::to_string(i+1),{},poses,e))return false;
    }
    for(unsigned i=0;i<d.actors.size();i++){
        const auto &p=d.actors[i];if(!p.floor_heights.empty())y=p.floor_heights[0];else if(!ground(callback,data,p.points[0][0],p.points[0][1],y,e))return false;
        if(!append(next,{Kind::Actor,i},{p.points[0][0],y,p.points[0][1],p.yaw,1},p.name,{},poses,e))return false;
    }
    for(unsigned i=0;i<d.particles.size();i++){
        const auto &p=d.particles[i];if(!append(next,{Kind::Particle,i},{p.x,p.y,p.z,0,1},"Emitter "+std::to_string(i+1),{},poses,e))return false;
    }
    for(unsigned i=0;i<d.sounds.size();i++){
        const auto &p=d.sounds[i];auto t=retained({Kind::Sound,i},{p.x,p.y,p.z,0,1});t.x=p.x;t.y=p.y;t.z=p.z;
        if(!append(next,{Kind::Sound,i},t,p.name,{},poses,e))return false;
        for(auto &node:next.nodes)if(node.binding==Binding{Kind::Sound,i})node.name=p.name;
    }
    for(unsigned i=0;i<d.weather_areas.size();i++){
        const auto &p=d.weather_areas[i];auto t=retained({Kind::Weather,i},{p.x,p.y,p.z,0,1});t.x=p.x;t.y=p.y;t.z=p.z;
        if(!append(next,{Kind::Weather,i},t,p.name,{},poses,e))return false;
        for(auto &node:next.nodes)if(node.binding==Binding{Kind::Weather,i})node.name=p.name;
    }
    for(unsigned i=0;i<d.waters.size();i++){
        const auto &p=d.waters[i];auto t=retained({Kind::Water,i},{p.x,p.level,p.z,0,1});t.x=p.x;t.y=p.level;t.z=p.z;
        if(!append(next,{Kind::Water,i},t,"Water "+std::to_string(i+1),{},poses,e))return false;
    }
    for(unsigned i=0;i<d.navigation.size();i++){
        const auto &p=d.navigation[i];auto t=retained({Kind::Navigation,i},{p.x,p.source_floor?p.floor_y:0,p.z,0,p.span});t.x=p.x;t.z=p.z;if(p.source_floor)t.y=p.floor_y;
        // Navigation is an axis-aligned envelope in the scene, but its entity
        // retains the original rotated square. Avoid expanding the envelope
        // again after each synchronize/rotate cycle.
        const double a=t.yaw*.017453292519943295;t.scale=p.span/(std::abs(std::cos(a))+std::abs(std::sin(a)));
        if(!append(next,{Kind::Navigation,i},t,p.name,{},poses,e))return false;
    }
    if(!world_entity::synchronize(next,poses,e)||!validate(next,&d,e))return false;
    g=std::move(next);return true;
}
bool reconcile(world_studio::Document &d,Graph &g,const std::vector<Source> &sources,const std::string &signature,Ground callback,const void *data,std::string &e){
    e.clear();if(sources.size()>WORLD_ADDED_KEY_BASE||!world_studio::valid(d,e)||!validate(g,&d,e))return false;
    auto next=d;auto graph=g;const auto refs=referenced(d,g);
    if(!refs.empty()&&d.source_signature!=signature){
        std::vector<std::string> old(WORLD_ADDED_KEY_BASE),current;
        for(const auto &s:sources)current.push_back(source_key(s));
        for(const auto &n:g.nodes)if(n.binding.kind==Kind::SourceModel)old[n.binding.index]=n.source_key;
        std::vector<std::pair<unsigned,unsigned>> mapping;
        if(!source_index_map(old,current,refs,mapping,e)){e="Cannot rebind saved source objects: "+e;return false;}
        std::vector<std::pair<unsigned,unsigned>> graph_mapping;
        if(!remap_source_bindings(graph,old,current,graph_mapping,e))return false;
        auto remap=[&](unsigned key){for(auto m:mapping)if(m.first==key)return m.second;return key;};
        for(auto &o:next.overrides)o.index=remap(o.index);
        for(auto &group:next.groups)for(auto &key:group.members)if(key<WORLD_ADDED_KEY_BASE)key=remap(key);
    }
    next.source_signature=signature;
    if(!synchronize(next,graph,sources,callback,data,e)||!world_studio::valid(next,e))return false;
    d=std::move(next);g=std::move(graph);return true;
}
bool sound_emitters(const world_studio::Document &d,std::vector<world_soundscape::Emitter> &out,std::string &e){
 if(!world_studio::valid(d,e))return false;std::vector<world_soundscape::Emitter> next;next.reserve(d.sounds.size());
 for(unsigned i=0;i<d.sounds.size();++i){const auto at=std::find_if(d.entities.nodes.begin(),d.entities.nodes.end(),[&](const Node &n){return n.binding==Binding{Kind::Sound,i};});if(at==d.entities.nodes.end()){e="Sound needs its persistent entity binding before playback";return false;}
  auto sound=d.sounds[i];sound.enabled=sound.enabled&&world_entity::visible(d.entities,{Kind::Sound,i});next.push_back({at->id,std::move(sound)});
 }out=std::move(next);e.clear();return true;
}
bool weather_areas(const world_studio::Document &d,std::vector<world_weather_authoring::BoundArea> &out,std::string &e){
 if(!world_studio::valid(d,e))return false;std::vector<world_weather_authoring::BoundArea> next;next.reserve(d.weather_areas.size());
 for(unsigned i=0;i<d.weather_areas.size();++i){const auto at=std::find_if(d.entities.nodes.begin(),d.entities.nodes.end(),[&](const Node &n){return n.binding==Binding{Kind::Weather,i};});if(at==d.entities.nodes.end()){e="Weather area needs its persistent entity binding before preview";return false;}
  auto area=d.weather_areas[i];area.enabled=area.enabled&&world_entity::visible(d.entities,{Kind::Weather,i});next.push_back({at->id,std::move(area)});
 }out=std::move(next);e.clear();return true;
}
bool erased(Graph &g,Binding binding,std::string &e){
    std::vector<Binding> removed;return erase_binding(g,binding,ChildPolicy::PreserveWorld,removed,e);
}
}
