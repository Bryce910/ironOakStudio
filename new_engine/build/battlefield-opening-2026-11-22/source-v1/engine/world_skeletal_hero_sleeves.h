#ifndef VALWOOD_WORLD_SKELETAL_HERO_SLEEVES_H
#define VALWOOD_WORLD_SKELETAL_HERO_SLEEVES_H
#include "engine/world_animation.h"
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
namespace world_skeletal_hero_sleeves {
// First-person-only rounded lining closures for the original proximal cuts.
// Source arm/glove/weapon geometry and palettes stay unchanged. No triangle
// reaches a camera anchor or stretches a source texture across empty space.
inline unsigned append(world_animation::Asset&subset,const world_animation::Asset&source,unsigned source_primitive=5){
 using V=world_animation::Vec3;using Key=std::array<long long,3>;
 auto sub=[](V a,V b){return V{a.x-b.x,a.y-b.y,a.z-b.z};};
 auto cross=[](V a,V b){return V{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
 auto dot=[](V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;};
 auto unit=[&](V a){float l=std::sqrt(dot(a,a));return l>1e-8f?V{a.x/l,a.y/l,a.z/l}:V{0,1,0};};
 if(source.mesh.primitives.size()<=source_primitive)return 0;
 for(const auto&v:source.mesh.vertices)if(v.uv[0]<-4094.f)return 0;
 const auto&p=source.mesh.primitives[source_primitive];std::map<Key,unsigned> canonical;std::map<unsigned,unsigned> vertex;
 struct Edge{unsigned count=0;};std::map<std::pair<unsigned,unsigned>,Edge> edges;
 V body[2]{};unsigned body_count[2]={};
 for(std::size_t i=p.first_index;i<p.first_index+p.index_count;++i){const unsigned old=source.mesh.indices[i];const auto&q=source.mesh.vertices[old].position;
  Key key{std::llround(q.x*100000.),std::llround(q.y*100000.),std::llround(q.z*100000.)};auto it=canonical.emplace(key,old);vertex[old]=it.first->second;
 }
 for(const auto&kv:canonical){const auto&q=source.mesh.vertices[kv.second].position;const unsigned side=q.x<0;body[side].x+=q.x;body[side].y+=q.y;body[side].z+=q.z;++body_count[side];}
 for(unsigned side=0;side<2;++side)if(body_count[side]){body[side].x/=body_count[side];body[side].y/=body_count[side];body[side].z/=body_count[side];}
 for(std::size_t i=p.first_index;i+2<p.first_index+p.index_count;i+=3)for(unsigned k=0;k<3;++k){unsigned a=vertex[source.mesh.indices[i+k]],b=vertex[source.mesh.indices[i+(k+1)%3]];if(a!=b)++edges[std::minmax(a,b)].count;}
 std::map<unsigned,std::set<unsigned>> adjacent;
 for(const auto&kv:edges)if(kv.second.count==1){adjacent[kv.first.first].insert(kv.first.second);adjacent[kv.first.second].insert(kv.first.first);}
 world_animation::Material padding{};padding.base_factor={.20f,.18f,.15f,1};padding.roughness=1;padding.metallic=0;padding.normal_scale=0;padding.double_sided=true;
 const unsigned material=unsigned(subset.materials.size());subset.materials.push_back(padding);
 world_animation::Primitive cap;cap.first_index=subset.mesh.indices.size();cap.material=int(material);cap.has_normals=cap.has_uv=cap.has_tangents=true;
 std::set<unsigned> visited;unsigned loops=0;
 for(const auto&kv:adjacent){const unsigned start=kv.first;if(visited.count(start)||kv.second.size()!=2)continue;
  std::vector<unsigned> ring;unsigned current=start,previous=~0u;bool closed=false;
  for(unsigned guard=0;guard<256;++guard){if(visited.count(current))break;auto it=adjacent.find(current);if(it==adjacent.end()||it->second.size()!=2)break;
   ring.push_back(current);visited.insert(current);unsigned next=*it->second.begin();if(next==previous)next=*it->second.rbegin();previous=current;current=next;if(current==start){closed=true;break;}
  }
  if(!closed||ring.size()<12)continue;
  // Only proximal openings connect to the torso's native shoulder weighting.
  // Elbow seams, straps and wrist vents remain their authored armor surfaces.
  double trunk=0;for(unsigned index:ring){const auto&v=source.mesh.vertices[index];for(unsigned k=0;k<4;++k){const auto&name=source.skeleton.joints[source.skeleton.palette[v.joints[k]]].name;if(name=="spine_03"||name.rfind("clavicle_",0)==0)trunk+=v.weights[k];}}
  if(trunk/ring.size()<.02)continue;
  V center{},normal{};for(unsigned index:ring){const auto&q=source.mesh.vertices[index].position;center.x+=q.x;center.y+=q.y;center.z+=q.z;}center.x/=ring.size();center.y/=ring.size();center.z/=ring.size();
  float radius=0;for(unsigned k=0;k<ring.size();++k){const auto&a=source.mesh.vertices[ring[k]].position;const auto&b=source.mesh.vertices[ring[(k+1)%ring.size()]].position;const auto n=cross(sub(a,center),sub(b,center));normal.x+=n.x;normal.y+=n.y;normal.z+=n.z;radius=std::max(radius,std::sqrt(dot(sub(a,center),sub(a,center))));}
  if(radius<.025f||radius>.23f||dot(normal,normal)<1e-12f)continue;
  normal=unit(normal);const V inward=sub(body[center.x<0],center);if(dot(normal,inward)>0)normal={-normal.x,-normal.y,-normal.z};
  const unsigned first=unsigned(subset.mesh.vertices.size());constexpr unsigned rings=4;
  // A shallow convex cloth lining stays within the source cross-section.
  // Preserve each source seam vertex/weight at t=0, then blend its influences
  // toward the actual contour's average; no camera coordinate is encoded.
  std::map<unsigned,double> mean;
  for(unsigned index:ring){const auto&v=source.mesh.vertices[index];for(unsigned j=0;j<4;++j)mean[v.joints[j]]+=v.weights[j]/ring.size();}
  for(unsigned section=0;section<rings;++section){const float t=float(section)/(rings-1),scale=1-.96f*t,depth=radius*.28f*std::sin(t*1.57079632679f);
   for(unsigned k=0;k<ring.size();++k){auto v=source.mesh.vertices[ring[k]];const V d=sub(v.position,center);
    v.position={center.x+d.x*scale+normal.x*depth,center.y+d.y*scale+normal.y*depth,center.z+d.z*scale+normal.z*depth};
    v.normal=unit(V{normal.x+unit(d).x*(1-t),normal.y+unit(d).y*(1-t),normal.z+unit(d).z*(1-t)});
    const V tangent=unit(cross(normal,unit(d)));v.tangent={tangent.x,tangent.y,tangent.z,1};v.uv={float(k)/ring.size(),t};
    std::map<unsigned,double> weights;for(const auto&j:mean)weights[j.first]=j.second*t;for(unsigned j=0;j<4;++j)weights[v.joints[j]]+=v.weights[j]*(1-t);
    std::vector<std::pair<unsigned,double>> sorted(weights.begin(),weights.end());std::sort(sorted.begin(),sorted.end(),[](const auto&a,const auto&b){return a.second>b.second;});
    double sum=0;for(unsigned j=0;j<std::min(4u,unsigned(sorted.size()));++j)sum+=sorted[j].second;
    for(unsigned j=0;j<4;++j){v.joints[j]=j<sorted.size()?sorted[j].first:0;v.weights[j]=j<sorted.size()?float(sorted[j].second/sum):0;}
    subset.mesh.vertices.push_back(v);
   }
  }
  for(unsigned section=0;section+1<rings;++section)for(unsigned k=0;k<ring.size();++k){const unsigned a=first+section*ring.size()+k,b=first+section*ring.size()+(k+1)%ring.size(),c=a+ring.size(),d=b+ring.size();for(unsigned i:{a,b,c,b,d,c})subset.mesh.indices.push_back(i);}
  auto tip=subset.mesh.vertices[first+(rings-1)*ring.size()];tip.position={center.x+normal.x*radius*.28f,center.y+normal.y*radius*.28f,center.z+normal.z*radius*.28f};tip.normal=normal;
  const unsigned end=unsigned(subset.mesh.vertices.size());subset.mesh.vertices.push_back(tip);
  for(unsigned k=0;k<ring.size();++k)for(unsigned i:{first+(rings-1)*unsigned(ring.size())+k,first+(rings-1)*unsigned(ring.size())+(k+1)%unsigned(ring.size()),end})subset.mesh.indices.push_back(i);++loops;
 }
 cap.index_count=subset.mesh.indices.size()-cap.first_index;if(cap.index_count)subset.mesh.primitives.push_back(cap);return loops;
}
}
#endif
