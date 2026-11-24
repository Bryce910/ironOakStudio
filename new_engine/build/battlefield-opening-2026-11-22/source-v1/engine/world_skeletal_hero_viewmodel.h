#ifndef VALWOOD_WORLD_SKELETAL_HERO_VIEWMODEL_H
#define VALWOOD_WORLD_SKELETAL_HERO_VIEWMODEL_H
#include "engine/world_animation_render.h"
#include "engine/world_skeletal_hero_sleeves.h"
#include <cmath>

// Cosmetic first-person presentation only. The committed world pose, weapon
// sockets, physical contacts, third-person draw and shadows stay unchanged.
namespace world_skeletal_hero_viewmodel {
constexpr float camera_height=1.75f,up=.25f,forward=.45f;
constexpr float yaw_offset=8.f*3.14159265358979323846f/180.f;
inline world_animation_render::Instance instance(){
 return {0,up-camera_height,-forward,1,3.14159265358979323846f+yaw_offset};
}
// Loading-time extraction keeps the original complete gloves, armplates,
// mittens and weapon triangles. Garment torso/shoulder cuts are world-only.
inline world_animation::Asset arms_subset(const world_animation::Asset&asset,const std::vector<unsigned>&parts={},unsigned body_parts=0){
 world_animation::Asset subset=asset;
 subset.morph_targets.clear(); // Face is absent from this independent arms buffer.
 subset.mesh.primitives.clear();subset.mesh.vertices.clear();subset.mesh.indices.clear();
 const auto&source=asset.mesh;
 std::vector<unsigned> remap(source.vertices.size(),~0u);
 for(unsigned i=0;i<source.primitives.size();++i){
  if(parts.empty()?!(i==0||i==5||i==9||i>=19):std::find(parts.begin(),parts.end(),i)==parts.end())continue;
  const auto&original=source.primitives[i];auto primitive=original;
  primitive.first_index=subset.mesh.indices.size();
  for(std::size_t k=original.first_index;k+2<original.first_index+original.index_count;k+=3){
   for(unsigned corner=0;corner<3;++corner){const unsigned old=source.indices[k+corner];
    if(remap[old]==~0u){remap[old]=unsigned(subset.mesh.vertices.size());subset.mesh.vertices.push_back(source.vertices[old]);}
    subset.mesh.indices.push_back(remap[old]);
   }
  }
  primitive.index_count=subset.mesh.indices.size()-primitive.first_index;
  if(primitive.index_count)subset.mesh.primitives.push_back(primitive);
 }
 // The purchased arm shell has three proximal boundary loops across its two
 // layered sleeves. Fail closed for another topology or reserved-UV collision.
 if(parts.empty()){
  if(world_skeletal_hero_sleeves::append(subset,asset)!=3){subset.mesh.primitives.clear();subset.mesh.vertices.clear();subset.mesh.indices.clear();}
 }else{
  // Each real outfit has its own source arm boundaries; never require the
  // custom09 shirt's three loops or add sleeves to a held weapon primitive.
  for(unsigned i=0;i<std::min(body_parts,unsigned(parts.size()));++i)world_skeletal_hero_sleeves::append(subset,asset,parts[i]);
 }
 // FP-only proximal garment treatment. Rest-skinned positions and real upper-
 // arm bones share the FBX conversion basis. The blend reaches zero before
 // the elbow; forearms, hands, held gear, world poses and contacts stay fixed.
 // Only this temporary FP buffer stores its blend in normal magnitude1..2.
 // The shader normalizes the original normal direction and reads that scalar;
 // source UVs/materials/positions/skin weights are never repacked.
 world_animation::Pose rest;std::string error;
 if(world_animation::rest_pose(asset.skeleton,rest,error)){
  using V=world_animation::Vec3;auto sub=[](V a,V b){return V{a.x-b.x,a.y-b.y,a.z-b.z};};auto dot=[](V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;};
  unsigned upper[2]={~0u,~0u},lower[2]={~0u,~0u};
  for(unsigned i=0;i<asset.skeleton.joints.size();++i){const auto&name=asset.skeleton.joints[i].name;for(unsigned k=0;k<2;++k){if(name==(k?"upperarm_r":"upperarm_l"))upper[k]=i;if(name==(k?"lowerarm_r":"lowerarm_l"))lower[k]=i;}}
  for(auto&vertex:subset.mesh.vertices){
   float influence=0;for(unsigned j=0;j<4;++j){const auto&name=asset.skeleton.joints[asset.skeleton.palette[vertex.joints[j]]].name;if(name.rfind("upperarm",0)==0||name.rfind("clavicle",0)==0||name=="spine_03")influence+=vertex.weights[j];}
   if(influence<.02f||vertex.uv[0]<-.25f||vertex.uv[0]>1.75f)continue;
   V skinned,norm;if(!world_animation::deform_vertex(vertex,rest,skinned,norm,error))continue;const V q=world_animation::transform_point(rest.global[asset.skeleton.mesh_node],skinned);float best=1e9f,progress=1;
   for(unsigned side=0;side<2;++side){if(upper[side]==~0u||lower[side]==~0u)continue;const auto&a=rest.global[upper[side]].m,&b=rest.global[lower[side]].m;const V u{a[12],a[13],a[14]},v{b[12],b[13],b[14]},axis=sub(v,u),offset=sub(q,u);const float length=dot(axis,axis),t=length>1e-8f?dot(offset,axis)/length:1;const V radial{offset.x-axis.x*t,offset.y-axis.y*t,offset.z-axis.z*t};const float distance=dot(radial,radial);if(distance<best){best=distance;progress=t;}}
   if(best>.22f*.22f)continue;float blend=std::clamp((.72f-progress)/.72f,0.f,1.f);blend=blend*blend*(3-2*blend);if(blend>.001f){vertex.normal.x*=1+blend;vertex.normal.y*=1+blend;vertex.normal.z*=1+blend;}
  }
 }
 return subset;
}
// A physical world-space blade point is rooted at the player's feet. Apply
// exactly the same cosmetic yaw and reach as instance(), so its trail follows
// the visible edge rather than the authoritative, unshifted contact edge.
template<class Point,class Root>
Point world_trail_point(const Point&point,const Root&root,double camera_yaw){
 const double dx=point.x-root.x,dz=point.z-root.z;
 const double c=std::cos(double(yaw_offset)),s=std::sin(double(yaw_offset));
 Point shown=point;
 shown.x=root.x+c*dx+s*dz+double(forward)*std::sin(camera_yaw);
 shown.y=point.y+double(up);
 shown.z=root.z-s*dx+c*dz-double(forward)*std::cos(camera_yaw);
 return shown;
}
}
#endif
