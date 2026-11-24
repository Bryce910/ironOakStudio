#ifndef VALWOOD_WORLD_EDITOR_SCENE_GUARD_H
#define VALWOOD_WORLD_EDITOR_SCENE_GUARD_H
#include "engine/world_studio_tools.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

// Structural geometry is recooked from the same source triangles/UCX and
// exact prepared render transforms. This index detects when that transactional
// cook is required; structural objects are no longer blanket-locked.
namespace world_editor_scene_guard {
enum class Mode {None,City,Grandstone};
inline bool protected_alias(Mode mode,const std::string &alias){
    if(mode==Mode::Grandstone)return alias.rfind("nm_scene_",0)==0;
    if(mode!=Mode::City)return false;
    if(alias.rfind("bep_country_terrain_",0)==0)return true;
    if(alias.rfind("nm_city_",0)!=0)return false;
    // Compact publisher UCX also belongs to props outside the house families.
    // Their movement must use the same collision transaction.
    return true;

}
inline bool same_transform(const world_entity::Transform &a,const world_entity::Transform &b){
    auto near=[](double x,double y,double tolerance=1.e-5){return std::abs(x-y)<=tolerance;};
    auto angle=[&](double x,double y){return near(std::remainder(x-y,360.),0,1.e-4);};
    return near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z)&&angle(a.yaw,b.yaw)&&angle(a.pitch,b.pitch)&&angle(a.roll,b.roll)&&near(a.scale,b.scale,1.e-6)&&near(a.sx,b.sx,1.e-6)&&near(a.sy,b.sy,1.e-6)&&near(a.sz,b.sz,1.e-6);
}
struct Index {
    std::unordered_map<std::uint64_t,world_entity::Transform> poses;
    std::unordered_map<std::uint64_t,bool> visibility;
    std::unordered_map<unsigned,const world_studio::Override *> overrides;
    static std::uint64_t key(world_entity::Binding b){return (std::uint64_t(unsigned(b.kind))<<32)|b.index;}
    bool build(const world_studio::Document &d,std::string &error){
        std::vector<std::pair<world_entity::Id,world_entity::Transform>> worlds;
        if(!world_entity::world_transforms(d.entities,worlds,error))return false;
        std::unordered_map<world_entity::Id,world_entity::Transform> by_id;by_id.reserve(worlds.size());
        for(const auto &entry:worlds)by_id.emplace(entry.first,entry.second);
        std::unordered_map<world_entity::Id,size_t> indices;indices.reserve(d.entities.nodes.size());
        for(size_t i=0;i<d.entities.nodes.size();++i)indices.emplace(d.entities.nodes[i].id,i);
        std::vector<signed char> visible(d.entities.nodes.size(),-1);
        auto inherited=[&](auto &&self,size_t i)->bool{if(visible[i]>=0)return visible[i]!=0;const auto &n=d.entities.nodes[i];const bool result=n.visible&&(!n.parent||self(self,indices.at(n.parent)));visible[i]=result?1:0;return result;};
        poses.reserve(d.entities.nodes.size());visibility.reserve(d.entities.nodes.size());
        for(size_t i=0;i<d.entities.nodes.size();++i){const auto &node=d.entities.nodes[i];if(node.binding.kind!=world_entity::Kind::None){poses.emplace(key(node.binding),by_id.at(node.id));visibility.emplace(key(node.binding),inherited(inherited,i));}}
        overrides.reserve(d.overrides.size());for(const auto &o:d.overrides)overrides.emplace(o.index,&o);
        return true;
    }
    const world_entity::Transform *find(world_entity::Binding b)const{
        const auto found=poses.find(key(b));return found==poses.end()?nullptr:&found->second;
    }
    bool visible(world_entity::Binding b)const{const auto at=visibility.find(key(b));return at==visibility.end()||at->second;}
    const world_studio::Override *override_for(unsigned source)const{const auto at=overrides.find(source);return at==overrides.end()?nullptr:at->second;}
};
inline world_entity::Transform source_pose(const world_studio::Document &,const Index &index,const world_studio::Pose &source,unsigned key){
    constexpr double degrees=57.29577951308232;
    world_entity::Transform t{source.root.x,source.root.y,source.root.z,source.yaw*degrees,source.scale,source.pitch*degrees,source.roll*degrees,source.sx,source.sy,source.sz};
    if(const auto *o=index.override_for(key)){t.x=o->x;t.y+=o->offset_y-source.offset_y;t.z=o->z;t.yaw=o->yaw;t.scale=o->scale;t.pitch=o->pitch;t.roll=o->roll;t.sx=o->sx;t.sy=o->sy;t.sz=o->sz;}
    if(const auto *entity=index.find({world_entity::Kind::SourceModel,key}))t=*entity;
    return t;
}
inline bool source_visible(const Index &index,unsigned key){
    if(const auto *o=index.override_for(key))if(o->hidden)return false;
    return index.visible({world_entity::Kind::SourceModel,key});
}
inline bool unchanged(const world_studio::Document &before,const world_studio::Document &after,
                     const std::vector<world_studio::Pose> &sources,Mode mode,std::string &error){
    if(mode==Mode::None)return true;
    Index a,b;if(!a.build(before,error)||!b.build(after,error))return false;
    auto fail=[&](const std::string &alias){error="Source collision protects "+alias+". Move, hide, replace or copy its architecture in the original source scene, then rebuild collision/ground and open a reviewed workcopy. Decorative props and materials remain editable here.";return false;};
    for(unsigned i=0;i<sources.size();++i)if(protected_alias(mode,sources[i].alias)){
        if(!same_transform(source_pose(before,a,sources[i],i),source_pose(after,b,sources[i],i))||source_visible(a,i)!=source_visible(b,i))return fail(sources[i].alias);
    }
    // Compare filtered records rather than raw array indices: deleting an
    // unrelated prop may compact the placement array without moving a structure.
    struct Placement {std::string alias;world_entity::Transform pose;bool visible;};
    auto placements=[&](const world_studio::Document &d,const Index &index){std::vector<Placement> out;
        for(unsigned i=0;i<d.placements.size();++i){const auto &p=d.placements[i];if(!protected_alias(mode,p.alias))continue;
            world_entity::Transform pose{p.x,p.offset_y+p.elevation-p.burial,p.z,p.yaw,p.scale,p.pitch,p.roll,p.sx,p.sy,p.sz};
            if(const auto *node=index.find({world_entity::Kind::Placement,i}))pose=*node;
            out.push_back({p.alias,pose,index.visible({world_entity::Kind::Placement,i})});
        }return out;};
    const auto old=placements(before,a),next=placements(after,b);
    if(old.size()!=next.size())return fail(next.size()>old.size()?next.back().alias:old.back().alias);
    for(size_t i=0;i<old.size();++i)if(old[i].alias!=next[i].alias||old[i].visible!=next[i].visible||!same_transform(old[i].pose,next[i].pose))return fail(old[i].alias);
    for(const auto &asset:before.assets)if(protected_alias(mode,asset.alias)){
        const auto found=std::find_if(after.assets.begin(),after.assets.end(),[&](const auto &p){return p.alias==asset.alias;});
        if(found==after.assets.end()||asset.stem!=found->stem||asset.lods!=found->lods||asset.collision!=found->collision||asset.vegetation!=found->vegetation||asset.units!=found->units)return fail(asset.alias);
    }
    return true;
}
inline bool immutable_ground_alias(const std::string &alias){
    return alias.rfind("bep_country_terrain_",0)==0||alias.find("_sm_ground_mesh")!=std::string::npos||alias.rfind("nm_scene_geology_",0)==0||alias.rfind("nm_scene_backdrop_",0)==0;
}
inline bool validate(const world_studio::Document &before,const world_studio::Document &after,
                     const std::vector<world_studio::Pose> &sources,Mode mode,std::string &error){
    if(mode==Mode::None){error.clear();return true;}
    Index old,next;if(!old.build(before,error)||!next.build(after,error))return false;
    auto fail=[&]{error="Imported base ground is a fixed environment field with matching terrain coverage and materials. Edit ordinary terrain with Landscape; houses, walls, doors and stairs can be transformed or copied with automatic source collision.";return false;};
    for(unsigned i=0;i<sources.size();++i)if(immutable_ground_alias(sources[i].alias)&&
       (!same_transform(source_pose(before,old,sources[i],i),source_pose(after,next,sources[i],i))||source_visible(old,i)!=source_visible(next,i)))return fail();
    for(const auto &asset:before.assets)if(immutable_ground_alias(asset.alias)){
        const auto found=std::find_if(after.assets.begin(),after.assets.end(),[&](const auto &p){return p.alias==asset.alias;});
        if(found==after.assets.end()||asset.stem!=found->stem||asset.lods!=found->lods||asset.vegetation!=found->vegetation||asset.collision!=found->collision)return fail();
    }
    auto ground_placements=[](const world_studio::Document &d){std::vector<world_studio::Placement> out;for(const auto &p:d.placements)if(immutable_ground_alias(p.alias))out.push_back(p);return out;};
    const auto a=ground_placements(before),b=ground_placements(after);if(a.size()!=b.size())return fail();
    for(std::size_t i=0;i<a.size();++i)if(a[i].alias!=b[i].alias||a[i].x!=b[i].x||a[i].z!=b[i].z||a[i].offset_y!=b[i].offset_y||a[i].yaw!=b[i].yaw||a[i].scale!=b[i].scale||a[i].pitch!=b[i].pitch||a[i].roll!=b[i].roll||a[i].sx!=b[i].sx||a[i].sy!=b[i].sy||a[i].sz!=b[i].sz)return fail();
    error.clear();return true;
}
inline bool geometry_changed(const world_studio::Document &before,const world_studio::Document &after,
                             const std::vector<world_studio::Pose> &sources,Mode mode){
    std::string ignored;return !unchanged(before,after,sources,mode,ignored);
}
}
#endif
