#ifndef VALWOOD_WORLD_LANDSCAPE_PROTECTION_H
#define VALWOOD_WORLD_LANDSCAPE_PROTECTION_H
#include "engine/world_studio_tools.h"
#include "engine/world_transform_math.h"
#include "engine/world_surface_authoring.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
namespace world_landscape_protection {
struct Footprint {double x=0,z=0,radius=0;};
// Include the complete transformed mesh, including tall tilted models and all
// three nonuniform axes. Translation is applied once at the root, not twice.
inline Footprint footprint(const world_studio::Pose &p,double margin){
 auto transform=vw_transform::pose(p);transform.x=transform.y=transform.z=0;
 const auto matrix=vw_transform::matrix(transform);double radius=0;
 for(unsigned c=0;c<8;++c){const auto v=vw_transform::vector(matrix,{c&1?p.maximum.x:p.minimum.x,c&2?p.maximum.y:p.minimum.y,c&4?p.maximum.z:p.minimum.z});
  if(!std::isfinite(v[0])||!std::isfinite(v[2]))return {p.root.x,p.root.z,std::numeric_limits<double>::infinity()};
  radius=std::max(radius,std::hypot(v[0],v[2]));}
 return {p.root.x,p.root.z,radius+margin};
}
inline bool contains(const Footprint &f,double x,double z){const double dx=x-f.x,dz=z-f.z;return std::abs(dx)<=f.radius&&std::abs(dz)<=f.radius&&dx*dx+dz*dz<f.radius*f.radius;}
// A drawn river must not inundate an existing model root, including decorative
// plants. These small guards are river-only: ordinary sculpting remains free
// to reground plants. Two metres conservatively cover the global 2m sampling
// lattice's maximum sqrt(2)m distance from a root to its nearest sample.
struct DryRoots {
 std::vector<Footprint> roots;
 bool prepare(const world_studio::Document &doc,const std::vector<world_studio::Pose> &source,std::string &error){
  if(source.size()>WORLD_ADDED_KEY_BASE||doc.placements.size()>world_studio::placement_limit){error="River root protection budget exceeded";return false;}
  std::vector<Footprint> next;next.reserve(source.size()+doc.placements.size());
  auto append=[&](unsigned key){world_studio::Pose p;if(!world_studio::effective_pose(doc,source,key,p)||p.hidden)return true;
   if(!std::isfinite(p.root.x)||!std::isfinite(p.root.z)){error="Existing model has an invalid river-protection position";return false;}
   next.push_back({p.root.x,p.root.z,2});return true;};
  for(unsigned i=0;i<source.size();++i)if(!append(i))return false;
  for(unsigned i=0;i<doc.placements.size();++i)if(!append(WORLD_ADDED_KEY_BASE+i))return false;
  roots=std::move(next);error.clear();return true;
 }
 bool blocked(double x,double z)const{
  if(!std::isfinite(x)||!std::isfinite(z))return true;
  for(const auto &root:roots)if(contains(root,x,z))return true;
  return false;
 }
 bool overlaps(double x,double z,double radius)const{
  if(!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(radius)||radius<0)return true;
  for(const auto &root:roots)if(std::hypot(x-root.x,z-root.z)<=radius+root.radius)return true;
  return false;
 }
};
// Road profiles protect the complete grading shoulder. River ribbons are not
// inserted here, so adding road safety never turns authored water into roads.
struct Roads {
 std::vector<world_surface::BakedRibbon> paths;
 bool prepare(const world_surface::Data &d,std::string &error){
  if(d.ribbons.size()>world_surface::ribbon_limit){error="Road protection path budget exceeded";return false;}
  Roads next;for(const auto &r:d.ribbons)if(r.kind==world_surface::RibbonKind::Road){world_surface::BakedRibbon path;if(!world_surface::bake_ribbon(r,path,error))return false;next.paths.push_back(std::move(path));}
  paths=std::move(next.paths);error.clear();return true;
 }
 bool blocked(double x,double z)const{
  if(!std::isfinite(x)||!std::isfinite(z))return true;
  for(const auto &r:paths){if(x<r.bounds[0]||z<r.bounds[1]||x>r.bounds[2]||z>r.bounds[3])continue;
   if(world_surface::nearest(r,x,z).distance<=r.source.width*.5+r.source.shoulder)return true;}
  return false;
 }
};
}
#endif
