#ifndef VALWOOD_WORLD_SKELETAL_ATTACK_ARC_CACHE_H
#define VALWOOD_WORLD_SKELETAL_ATTACK_ARC_CACHE_H
#include "world_animation.h"
#include <cstring>
#include <limits>

// Loading-boundary reuse only. A staged wardrobe change can retain the exact
// accepted combat table while replacing body geometry. There is no persistent
// cache, approximate comparison, hash collision, or change to attack sampling.
namespace world_skeletal_attack_arc_cache {
struct Inputs {
 const world_animation::Asset *asset=nullptr;
 unsigned held_begin=0;
 std::array<unsigned,15> clips={};
 unsigned pelvis=0,spine=0,hand=0,right_hand=0,grip_profile=0;
 bool stationary_cut_adaptation=false;
};
namespace detail {
inline bool same_float(const float &a,const float &b){return std::memcmp(&a,&b,sizeof a)==0;}
inline bool same_double(const double &a,const double &b){return std::memcmp(&a,&b,sizeof a)==0;}
template<std::size_t N> inline bool same_floats(const std::array<float,N>&a,const std::array<float,N>&b){
 return std::memcmp(a.data(),b.data(),N*sizeof(float))==0;
}
inline bool same_vec(const world_animation::Vec3&a,const world_animation::Vec3&b){return same_float(a.x,b.x)&&same_float(a.y,b.y)&&same_float(a.z,b.z);}
inline bool same_quat(const world_animation::Quat&a,const world_animation::Quat&b){return same_float(a.x,b.x)&&same_float(a.y,b.y)&&same_float(a.z,b.z)&&same_float(a.w,b.w);}
inline bool same_transform(const world_animation::Transform&a,const world_animation::Transform&b){return same_vec(a.translation,b.translation)&&same_quat(a.rotation,b.rotation)&&same_vec(a.scale,b.scale);}
inline bool same_skeleton(const world_animation::Skeleton&a,const world_animation::Skeleton&b){
 if(a.mesh_node!=b.mesh_node||a.palette!=b.palette||a.joints.size()!=b.joints.size()||a.inverse_bind.size()!=b.inverse_bind.size())return false;
 for(std::size_t n=0;n<a.joints.size();++n){const auto&x=a.joints[n];const auto&y=b.joints[n];if(x.name!=y.name||x.parent!=y.parent||!same_transform(x.rest,y.rest))return false;}
 for(std::size_t n=0;n<a.inverse_bind.size();++n)if(!same_floats(a.inverse_bind[n].m,b.inverse_bind[n].m))return false;
 return true;
}
inline bool same_clips(const std::vector<world_animation::Clip>&a,const std::vector<world_animation::Clip>&b){
 if(a.size()!=b.size())return false;
 for(std::size_t n=0;n<a.size();++n){const auto&x=a[n];const auto&y=b[n];
  if(x.name!=y.name||!same_double(x.duration,y.duration)||x.channels.size()!=y.channels.size()||x.events.size()!=y.events.size())return false;
  for(std::size_t k=0;k<x.channels.size();++k){const auto&u=x.channels[k];const auto&v=y.channels[k];
   if(u.node!=v.node||u.path!=v.path||u.interpolation!=v.interpolation||u.times.size()!=v.times.size()||u.values.size()!=v.values.size())return false;
   if(!u.times.empty()&&std::memcmp(u.times.data(),v.times.data(),u.times.size()*sizeof(float)))return false;
   for(std::size_t i=0;i<u.values.size();++i)if(!same_floats(u.values[i],v.values[i]))return false;
  }
  for(std::size_t k=0;k<x.events.size();++k)if(x.events[k].name!=y.events[k].name||!same_double(x.events[k].time,y.events[k].time))return false;
 }
 return true;
}
inline bool same_vertex(const world_animation::SkinVertex&a,const world_animation::SkinVertex&b){
 return same_vec(a.position,b.position)&&same_vec(a.normal,b.normal)&&same_floats(a.uv,b.uv)&&a.joints==b.joints&&same_floats(a.weights,b.weights)&&same_floats(a.tangent,b.tangent);
}
inline bool valid_range(const world_animation::SkinMesh&m,const world_animation::Primitive&p){return p.first_index<=m.indices.size()&&p.index_count<=m.indices.size()-p.first_index;}
inline bool same_held_mesh(const world_animation::SkinMesh&a,unsigned begin_a,const world_animation::SkinMesh&b,unsigned begin_b){
 if(begin_a>=a.primitives.size()||begin_b>=b.primitives.size()||a.primitives.size()-begin_a!=b.primitives.size()-begin_b)return false;
 // Recomposition changes absolute vertex/index offsets. Require a bijection
 // of referenced held vertices, preserving shared topology across primitives.
 constexpr auto unset=std::numeric_limits<std::uint32_t>::max();
 if(a.vertices.size()>=unset||b.vertices.size()>=unset)return false;
 std::vector<std::uint32_t> a_to_b(a.vertices.size(),unset),b_to_a(b.vertices.size(),unset);
 bool any=false;
 for(std::size_t p=0;p<a.primitives.size()-begin_a;++p){const auto&x=a.primitives[begin_a+p];const auto&y=b.primitives[begin_b+p];
  if(x.index_count!=y.index_count||x.has_normals!=y.has_normals||x.has_uv!=y.has_uv||x.has_tangents!=y.has_tangents||!valid_range(a,x)||!valid_range(b,y))return false;
  for(std::size_t k=0;k<x.index_count;++k){const auto u=a.indices[x.first_index+k],v=b.indices[y.first_index+k];
   if(u>=a.vertices.size()||v>=b.vertices.size())return false;
   if((a_to_b[u]!=unset&&a_to_b[u]!=v)||(b_to_a[v]!=unset&&b_to_a[v]!=u))return false;
   if(!same_vertex(a.vertices[u],b.vertices[v]))return false;
   a_to_b[u]=v;b_to_a[v]=u;any=true;
  }
 }
 return any;
}
}
inline bool compatible(const Inputs&a,const Inputs&b){
 if(!a.asset||!b.asset||a.clips!=b.clips||a.pelvis!=b.pelvis||a.spine!=b.spine||a.hand!=b.hand||a.right_hand!=b.right_hand||a.grip_profile!=b.grip_profile||a.stationary_cut_adaptation!=b.stationary_cut_adaptation)return false;
 return detail::same_skeleton(a.asset->skeleton,b.asset->skeleton)&&detail::same_clips(a.asset->clips,b.asset->clips)&&detail::same_held_mesh(a.asset->mesh,a.held_begin,b.asset->mesh,b.held_begin);
 // Body vertices, materials, encoded images, and source filenames do not feed
 // sample_attack/blade_pose. All held geometry, skinning and weapon transforms
 // do; even a one-bit change forces the original full table computation.
}
}
#endif
