#ifndef VALWOOD_WORLD_KNIGHT_APPEARANCE_H
#define VALWOOD_WORLD_KNIGHT_APPEARANCE_H
#include "world_animation.h"
#include "world_knight_roster.h"
#include <cmath>
#include <set>

// An appearance replaces purchased body geometry, never the accepted capture,
// skeleton, sockets or equipped weapon. Explicit semantic FP primitives also
// cover outfits whose source doublet contains both torso and arm triangles.
namespace world_knight_appearance {
struct Config {std::string body;std::vector<unsigned> first_person;};
inline bool read(std::istream &in,Config &out,std::string &error){
 Config next;std::string magic,key,end,extra;unsigned version=0,count=0;
 if(!(in>>magic>>version>>key)||magic!="VALWOOD_KNIGHT_APPEARANCE"||version!=1||key!="body"||
    !world_knight_roster::quoted(in,next.body)||!world_knight_roster::valid_path(next.body)||
    !(in>>key>>count)||key!="first_person"||!count||count>96){error="Invalid knight appearance definition";return false;}
 std::set<unsigned> seen;
 for(unsigned i=0;i<count;++i){unsigned primitive=0;if(!(in>>primitive)||primitive>=128||!seen.insert(primitive).second){error="Invalid first-person knight parts";return false;}next.first_person.push_back(primitive);}
 if(!(in>>end)||end!="end"||(in>>extra)){error="Incomplete knight appearance definition";return false;}
 out=std::move(next);error.clear();return true;
}
inline bool load(const char *path,Config &config,world_animation::Asset &body,std::string &error){
 if(!path||!*path){error="Choose a knight appearance";return false;}
 std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);
 if(ec||bytes>4096){error="Knight appearance definition is missing or too large";return false;}
 std::ifstream file(path);Config next;if(!read(file,next,error))return false;
 const auto body_path=std::filesystem::path(path).parent_path()/next.body;
 const auto size=std::filesystem::file_size(body_path,ec);
 if(ec||size>128ull*1024*1024){error="Knight appearance geometry is missing or too large";return false;}
 world_animation::Asset asset;if(!world_animation::load_glb(body_path.u8string().c_str(),asset,error))return false;
 if(!asset.clips.empty()){error="An appearance must not replace the accepted animation clips";return false;}
 for(unsigned i:next.first_person)if(i>=asset.mesh.primitives.size()){error="First-person knight part is unavailable";return false;}
 config=std::move(next);body=std::move(asset);return true;
}
inline bool near(const world_animation::Mat4&a,const world_animation::Mat4&b){
 for(unsigned i=0;i<16;++i)if(!std::isfinite(a.m[i])||!std::isfinite(b.m[i])||std::abs(a.m[i]-b.m[i])>2.e-4f)return false;
 return true;
}
inline bool compose(const world_animation::Asset &base,const world_animation::Asset &body,
                    const Config &config,world_animation::Asset &out,
                    std::vector<unsigned> &first_person,std::string &error){
 using namespace world_animation;
 constexpr unsigned original_body_primitives=19;
 if(base.mesh.primitives.size()<=original_body_primitives||body.mesh.primitives.empty()){
  error="The knight or equipped weapon has no compatible geometry";return false;
 }
 Pose a,b;if(!rest_pose(base.skeleton,a,error)||!rest_pose(body.skeleton,b,error))return false;
 if(!near(a.global[base.skeleton.mesh_node],b.global[body.skeleton.mesh_node])){
  error="Knight mesh space differs from the accepted rig";return false;
 }
 std::vector<unsigned> palette(body.skeleton.palette.size(),~0u);
 for(unsigned j=0;j<palette.size();++j){
  const unsigned source=body.skeleton.palette[j];const auto &name=body.skeleton.joints[source].name;
  for(unsigned k=0;k<base.skeleton.palette.size();++k){const unsigned target=base.skeleton.palette[k];
   if(base.skeleton.joints[target].name==name){
    // Held-weapon offsets differ by equipped loadout; appearance vertices may
    // not use that socket. Every weighted body joint retains its exact basis.
    if(name!="valwood_weapon_r"&&(!near(a.global[target],b.global[source])||
       !near(base.skeleton.inverse_bind[k],body.skeleton.inverse_bind[j]))){error="Knight body bind differs at "+name;return false;}
    palette[j]=k;break;
   }
  }
  if(palette[j]==~0u){error="Knight body joint is unavailable: "+name;return false;}
 }
 Asset next=base;next.mesh={};next.materials.clear();next.images.clear();next.morph_targets.clear();
 std::vector<int> body_materials(body.materials.size(),-1),weapon_materials(base.materials.size(),-1);
 auto material_for=[&](const Asset &source,int original,std::vector<int>&remap){
  if(original<0)return -1;if(remap[unsigned(original)]>=0)return remap[unsigned(original)];
  auto material=source.materials[unsigned(original)];
  for(auto *map:{&material.base,&material.normal,&material.metallic_roughness,&material.occlusion,&material.emissive})if(map->image>=0){
   const auto &encoded=source.images[unsigned(map->image)].encoded;int found=-1;
   for(unsigned i=0;i<next.images.size();++i)if(next.images[i].encoded==encoded){found=int(i);break;}
   if(found<0){found=int(next.images.size());next.images.push_back(source.images[unsigned(map->image)]);}map->image=found;
  }
  const int index=int(next.materials.size());next.materials.push_back(material);remap[unsigned(original)]=index;return index;
 };
 auto append=[&](const Asset &source,unsigned p,bool replacement,std::vector<unsigned> &remap){
  auto primitive=source.mesh.primitives[p];const auto begin=primitive.first_index;
  primitive.first_index=next.mesh.indices.size();primitive.material=material_for(source,primitive.material,replacement?body_materials:weapon_materials);
  for(std::size_t i=begin;i<begin+primitive.index_count;++i){const unsigned old=source.mesh.indices[i];
   if(remap[old]==~0u){auto vertex=source.mesh.vertices[old];
    if(replacement)for(unsigned k=0;k<4;++k){
     if(vertex.weights[k]>0&&body.skeleton.joints[body.skeleton.palette[vertex.joints[k]]].name=="valwood_weapon_r")return false;
     vertex.joints[k]=palette[vertex.joints[k]];
    }
    remap[old]=unsigned(next.mesh.vertices.size());next.mesh.vertices.push_back(vertex);
   }
   next.mesh.indices.push_back(remap[old]);
  }
  next.mesh.primitives.push_back(primitive);return true;
 };
 std::vector<unsigned> remap(body.mesh.vertices.size(),~0u),weapon_remap(base.mesh.vertices.size(),~0u);
 for(unsigned p=0;p<body.mesh.primitives.size();++p)if(!append(body,p,true,remap)){error="A body appearance contains held weapon geometry";return false;}
 std::vector<unsigned> view=config.first_person;
 for(unsigned p=original_body_primitives;p<base.mesh.primitives.size();++p){
  view.push_back(unsigned(next.mesh.primitives.size()));if(!append(base,p,false,weapon_remap))return false;
 }
 // Weapon banks never supply Thomas facial shapes. Keep the body's authored
 // target names and remap its sparse vertices to the composed draw definition.
 next.morph_targets=body.morph_targets;
 for(auto &target:next.morph_targets){for(auto &delta:target.deltas){if(delta.vertex>=remap.size()||remap[delta.vertex]==~0u){error="Unmapped authored facial vertex";return false;}delta.vertex=remap[delta.vertex];}std::sort(target.deltas.begin(),target.deltas.end(),[](const MorphDelta &a,const MorphDelta &b){return a.vertex<b.vertex;});}
 if(!validate(next,error))return false;
 out=std::move(next);first_person=std::move(view);error.clear();return true;
}
}
#endif
