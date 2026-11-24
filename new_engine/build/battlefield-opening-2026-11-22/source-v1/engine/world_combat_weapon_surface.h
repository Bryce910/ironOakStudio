#ifndef VALWOOD_COMBAT_WEAPON_SURFACE_H
#define VALWOOD_COMBAT_WEAPON_SURFACE_H
#include "cgltf.h"
#include "engine/world_combat_pose_packet.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <numeric>
#include <string_view>
#include <vector>
namespace world_combat_weapon_surface {
enum Role:unsigned char {Unknown=0,Structural=1,Decoration=2};
inline std::string_view source_name(const char *extras){
 if(!extras)return {};std::string_view value(extras);const auto key=value.find("\"source_object\"");if(key==value.npos)return {};std::size_t i=key+15;
 while(i<value.size()&&(value[i]==' '||value[i]=='\t'||value[i]=='\r'||value[i]=='\n'))++i;if(i==value.size()||value[i++]!=':')return {};
 while(i<value.size()&&(value[i]==' '||value[i]=='\t'||value[i]=='\r'||value[i]=='\n'))++i;if(i==value.size()||value[i++]!='\"')return {};const auto begin=i;
 while(i<value.size()&&value[i]!='\"'){if(value[i]=='\\'||i-begin>128)return {};++i;}return i<value.size()?value.substr(begin,i-begin):std::string_view{};
}
inline Role classify(const char *material,const char *extras){
 const std::string_view name=material?material:"",source=source_name(extras);
 if(name=="Owned Unity Knight roster weapon")return Structural;
 if(name=="Owned Unity Orc Gnur weapon")return source=="Sword_mesh"?Structural:Unknown;
 if(name!="Owned Unity Orc roster weapon")return Unknown;
 if(source=="Orc_04_Weapon_Feather_Mesh")return Decoration;
 for(const auto expected:{"Plane001___","Object003","Orc_03_Weapon_Detail_03_Mesh","Orc_04_Weapon_Mesh","Orc_05_Weapon","Orc_06_Weapon_Mesh","Weapon_Mesh","Sword_Mesh","Dar_Sword_Mesh","Sword_mesh","Axe_Mesh"})if(source==expected)return Structural;
 return Unknown;
}
inline bool roles(const char *file,std::size_t count,std::vector<Role>&out){
 out.clear();cgltf_options options={};cgltf_data*data=nullptr;if(cgltf_parse_file(&options,file,&data)!=cgltf_result_success)return false;
 for(std::size_t n=0;n<data->nodes_count;++n)if(data->nodes[n].mesh)for(std::size_t i=0;i<data->nodes[n].mesh->primitives_count;++i){const auto&p=data->nodes[n].mesh->primitives[i];out.push_back(classify(p.material?p.material->name:nullptr,p.extras.data));}
 cgltf_free(data);if(out.size()!=count){out.clear();return false;}return true;
}
using Point=world_rigid_weapon_pose::Point;
struct Node {Point lo={},hi={};unsigned begin=0,count=0,left=0,right=0;};
struct Tree {
 unsigned mesh=0;std::vector<unsigned>order;std::vector<Node>nodes;std::array<std::array<unsigned,3>,172>frames={};
 bool build(const world_combat_pose_packet::Packet&p,unsigned index){
  mesh=index;const auto&m=p.meshes()[mesh];if(!(m.flags&1)||m.indices.empty()||m.indices.size()%3)return false;order.resize(m.indices.size()/3);std::iota(order.begin(),order.end(),0);nodes.reserve(order.size()/3+3);for(unsigned pose=0;pose<172;++pose)frames[pose]=p.frame(m,pose);
  auto point=[&](unsigned triangle,unsigned corner){return p.position(0,m.offset+m.indices[triangle*3+corner]);};
  auto recurse=[&](auto&&self,unsigned begin,unsigned end)->unsigned{const unsigned id=nodes.size();nodes.emplace_back();Node node;node.begin=begin;node.count=end-begin;node.lo.fill(std::numeric_limits<float>::infinity());node.hi.fill(-std::numeric_limits<float>::infinity());
   for(unsigned i=begin;i<end;++i)for(unsigned c=0;c<3;++c){const auto v=point(order[i],c);for(unsigned k=0;k<3;++k){node.lo[k]=std::min(node.lo[k],v[k]);node.hi[k]=std::max(node.hi[k],v[k]);}}
   if(end-begin>12){unsigned axis=0;for(unsigned k=1;k<3;++k)if(node.hi[k]-node.lo[k]>node.hi[axis]-node.lo[axis])axis=k;const unsigned middle=(begin+end)/2;
    auto center=[&](unsigned t){return point(t,0)[axis]+point(t,1)[axis]+point(t,2)[axis];};std::nth_element(order.begin()+begin,order.begin()+middle,order.begin()+end,[&](unsigned a,unsigned b){return center(a)<center(b);});node.count=0;node.left=self(self,begin,middle);node.right=self(self,middle,end);}
   nodes[id]=node;return id;};recurse(recurse,0,order.size());return true;
 }
};
}
#endif
