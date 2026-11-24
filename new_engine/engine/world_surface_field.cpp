#include "engine/world_surface_field.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
namespace world_surface {
namespace {
double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);}
const TerrainTile *tile(const TerrainField &field,double x,double z){
 if(!std::isfinite(x)||!std::isfinite(z)||std::abs(x)>50400||std::abs(z)>28400)return nullptr;
 int tx=int(std::floor(x/128)),tz=int(std::floor(z/128));unsigned at=paint_hash(tx,tz);
 for(unsigned i=0;i<paint_hash_size;i++){auto key=field.lookup[at];if(key[2]<0)return nullptr;if(key[0]==tx&&key[1]==tz){return unsigned(key[2])<field.tiles.size()?&field.tiles[unsigned(key[2])]:nullptr;}at=(at+1)&(paint_hash_size-1);}
 return nullptr;
}
}
bool same_paths(const Data &a,const Data &b){
 if(a.ribbons.size()!=b.ribbons.size())return false;
 for(unsigned i=0;i<a.ribbons.size();i++){const auto &x=a.ribbons[i],&y=b.ribbons[i];if(x.kind!=y.kind||x.layer!=y.layer||x.width!=y.width||x.shoulder!=y.shoulder||x.depth!=y.depth||x.smooth!=y.smooth||x.points.size()!=y.points.size())return false;for(unsigned j=0;j<x.points.size();j++)if(x.points[j].x!=y.points[j].x||x.points[j].y!=y.points[j].y||x.points[j].z!=y.points[j].z)return false;}
 return true;
}
bool bake_terrain(const Data &data,Ground ground,void *userdata,TerrainField &out,std::string &error){
 if(!validate(data,error))return false;
 if(!ground){error="Missing road/river ground sampler";return false;}
 TerrainField next;for(auto &v:next.lookup)v={0,0,-1,0};
 std::map<std::pair<int,int>,std::vector<unsigned>> keys;
 for(unsigned i=0;i<data.ribbons.size();i++){
  const auto &source=data.ribbons[i];if(source.kind==RibbonKind::River&&(source.width<4||source.shoulder<2)){error="River channels need at least4m width and2m banks on the terrain grid";return false;}
  BakedRibbon ribbon;if(!bake_ribbon(source,ribbon,error))return false;
  auto b=ribbon.bounds;
  for(int z=int(std::floor(b[1]/128));z<=int(std::floor(b[3]/128));z++)for(int x=int(std::floor(b[0]/128));x<=int(std::floor(b[2]/128));x++){
   // Inflate by one grid cell so boundary copies include the same influences.
   for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
    int tx=x+dx,tz=z+dz;if((tx+1)*128.<b[0]-2||tx*128.>b[2]+2||(tz+1)*128.<b[1]-2||tz*128.>b[3]+2)continue;
    auto &ids=keys[{tx,tz}];if(ids.empty()||ids.back()!=i)ids.push_back(i);
   }
  }
  next.ribbons.push_back(std::move(ribbon));
  if(keys.size()>paint_tile_limit){error="Road/river terrain exceeds64local tiles";return false;}
 }
 for(const auto &[key,ids]:keys){
  TerrainTile t;t.x=key.first;t.z=key.second;t.count=unsigned(ids.size());for(unsigned i=0;i<ids.size();i++)t.ribbons[i]=int(ids[i]);
  for(unsigned z=0;z<paint_side;z++)for(unsigned x=0;x<paint_side;x++){
   double wx=t.x*128.+x*2,wz=t.z*128.+z*2;float original=ground(wx,wz,userdata),height=original;
   if(!std::isfinite(original)){error="Non-finite ground below spline";return false;}
   // World-sorted document order controls overlaps and gives identical seams.
   for(unsigned id:ids){const auto &r=next.ribbons[id];auto h=nearest(r,wx,wz);const double half=r.source.width*.5;
    if(h.distance>=half+r.source.shoulder||wx<r.bounds[0]||wz<r.bounds[1]||wx>r.bounds[2]||wz>r.bounds[3])continue;
    double target=h.height,weight=1-smooth((h.distance-half)/r.source.shoulder);
    if(r.source.kind==RibbonKind::Road)t.road[z*paint_side+x]=std::max(t.road[z*paint_side+x],float(weight));
    if(r.source.kind==RibbonKind::River){
     if(h.distance<half){double q=h.distance/half;target=h.height-r.source.depth+(r.source.depth+.15)*q*q*q*q;}
     else target=std::max(double(height),h.height+.15);
    }
    height=float(height+(target-height)*weight);
   }
   float offset=height-original;if(!std::isfinite(offset)||std::abs(offset)>32){error="Road/river cut exceeds32m; adjust spline height/profile";return false;}t.delta[z*paint_side+x]=offset;
  }
  unsigned at=paint_hash(t.x,t.z);while(next.lookup[at][2]>=0)at=(at+1)&(paint_hash_size-1);
  next.lookup[at]={t.x,t.z,int(next.tiles.size()),0};next.tiles.push_back(std::move(t));
 }
 out=std::move(next);error.clear();return true;
}
float terrain_offset(const TerrainField &field,double x,double z){
 const auto *t=tile(field,x,z);if(!t)return 0;double u=(x-t->x*128)*.5,v=(z-t->z*128)*.5;
 unsigned ix=std::min(63u,unsigned(std::floor(u))),iz=std::min(63u,unsigned(std::floor(v)));float tx=float(u-ix),tz=float(v-iz);
 auto at=[&](unsigned px,unsigned pz){return t->delta[pz*paint_side+px];};return (at(ix,iz)*(1-tx)+at(ix+1,iz)*tx)*(1-tz)+(at(ix,iz+1)*(1-tx)+at(ix+1,iz+1)*tx)*tz;
}
float road_mask(const TerrainField &field,double x,double z){
 const auto *t=tile(field,x,z);if(!t)return 0;double u=(x-t->x*128)*.5,v=(z-t->z*128)*.5;
 unsigned ix=std::min(63u,unsigned(std::floor(u))),iz=std::min(63u,unsigned(std::floor(v)));float tx=float(u-ix),tz=float(v-iz);
 auto at=[&](unsigned px,unsigned pz){return t->road[pz*paint_side+px];};return std::clamp((at(ix,iz)*(1-tx)+at(ix+1,iz)*tx)*(1-tz)+(at(ix,iz+1)*(1-tx)+at(ix+1,iz+1)*tx)*tz,0.f,1.f);
}
bool river(const TerrainField &field,double x,double z,float &level,float &shore){
 const auto *t=tile(field,x,z);if(!t)return false;bool found=false;float chosen=-std::numeric_limits<float>::infinity(),edge=0;
 for(unsigned i=0;i<t->count;i++){const auto &r=field.ribbons[unsigned(t->ribbons[i])];if(r.source.kind!=RibbonKind::River)continue;auto hit=nearest(r,x,z);if(!hit.inside)continue;if(hit.height>chosen){chosen=float(hit.height);edge=float(1-smooth(hit.distance/(r.source.width*.5)));found=true;}}
 if(found){level=chosen;shore=edge;}return found;
}
}
