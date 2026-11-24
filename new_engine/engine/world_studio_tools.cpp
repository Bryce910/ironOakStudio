#include "engine/world_transform_math.h"
#include "engine/world_studio_tools.h"
#include <algorithm>
#include <cmath>
#include <iterator>
namespace world_studio {
namespace {
bool finite(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool selection(const Document &d,const std::vector<Pose> &source,const std::vector<unsigned> &keys,std::string &e){
 if(keys.empty()||keys.size()>selection_limit){e="Select1–128 objects";return false;}
 for(size_t i=0;i<keys.size();i++){const world_entity::Binding binding={keys[i]<8192?world_entity::Kind::SourceModel:world_entity::Kind::Placement,keys[i]<8192?keys[i]:keys[i]-8192};if(!world_entity::editable(d.entities,binding)){e="Selected entity or its parent is locked";return false;}Pose p;if(!effective_pose(d,source,keys[i],p)){e="Selected object no longer exists";return false;}for(size_t j=0;j<i;j++)if(keys[i]==keys[j]){e="Duplicate selection key";return false;}}
 return true;
}
}
bool ray_bounds(const Ray &ray,const Pose &pose,double &distance){
 if(!finite(ray.origin)||!finite(ray.direction)||!finite(pose.root)||!finite(pose.minimum)||!finite(pose.maximum)||!std::isfinite(pose.yaw)||!std::isfinite(pose.scale)||pose.scale<=0||!world_entity::valid_transform(vw_transform::pose(pose))||pose.hidden)return false;
 const auto inverse=vw_transform::inverse(vw_transform::matrix(vw_transform::pose(pose)));
 auto local=[&](Vec v){const auto q=vw_transform::vector(inverse,{v.x,v.y,v.z});return Vec{q[0],q[1],q[2]};};
 const Vec o=local({ray.origin.x-pose.root.x,ray.origin.y-pose.root.y,ray.origin.z-pose.root.z}),v=local(ray.direction);
 double lo=0,hi=std::numeric_limits<double>::infinity();
 const double a[]={o.x,o.y,o.z},b[]={v.x,v.y,v.z},mn[]={pose.minimum.x,pose.minimum.y,pose.minimum.z},mx[]={pose.maximum.x,pose.maximum.y,pose.maximum.z};
 if(v.x*v.x+v.y*v.y+v.z*v.z<1e-20)return false;
 for(unsigned k=0;k<3;k++){if(mn[k]>mx[k])return false;if(std::abs(b[k])<1e-12){if(a[k]<mn[k]||a[k]>mx[k])return false;}else{double p=(mn[k]-a[k])/b[k],q=(mx[k]-a[k])/b[k];if(p>q)std::swap(p,q);lo=std::max(lo,p);hi=std::min(hi,q);if(lo>hi)return false;}}
 distance=lo;return std::isfinite(distance);
}
bool ray_plane(const Ray &ray,Vec point,Vec normal,Vec &hit){
 if(!finite(ray.origin)||!finite(ray.direction)||!finite(point)||!finite(normal))return false;
 const double den=ray.direction.x*normal.x+ray.direction.y*normal.y+ray.direction.z*normal.z;
 if(std::abs(den)<1e-10)return false;
 const double t=((point.x-ray.origin.x)*normal.x+(point.y-ray.origin.y)*normal.y+(point.z-ray.origin.z)*normal.z)/den;
 if(!std::isfinite(t)||t<0||t>10000)return false;
 hit={ray.origin.x+ray.direction.x*t,ray.origin.y+ray.direction.y*t,ray.origin.z+ray.direction.z*t};return true;
}
bool effective_pose(const Document &d,const std::vector<Pose> &source,unsigned key,Pose &out){
 if(key<added_key){if(key>=source.size())return false;out=source[key];for(const auto &o:d.overrides)if(o.index==key){out.root.x=o.x;out.root.z=o.z;out.root.y+=o.offset_y-out.offset_y;out.offset_y=o.offset_y;out.yaw=o.yaw*.017453292519943295;out.scale=o.scale;out.pitch=o.pitch*.017453292519943295;out.roll=o.roll*.017453292519943295;out.sx=o.sx;out.sy=o.sy;out.sz=o.sz;out.hidden=o.hidden;break;}out.hidden|=!world_entity::visible(d.entities,{world_entity::Kind::SourceModel,key});return true;}
 const size_t i=key-added_key;if(i>=d.placements.size())return false;const auto &p=d.placements[i];
 out={};out.alias=p.alias;out.root={p.x,p.offset_y,p.z};out.yaw=p.yaw*.017453292519943295;out.scale=p.scale;out.pitch=p.pitch*.017453292519943295;out.roll=p.roll*.017453292519943295;out.sx=p.sx;out.sy=p.sy;out.sz=p.sz;out.burial=p.burial;out.elevation=p.elevation;out.offset_y=p.offset_y;out.hidden=!world_entity::visible(d.entities,{world_entity::Kind::Placement,unsigned(i)});return true;
}
bool transform_selection(Document &d,const std::vector<Pose> &source,const std::vector<unsigned> &keys,Vec pivot,Vec translation,double yaw,double scale,std::string &e){
 if(!selection(d,source,keys,e))return false;
 if(!finite(pivot)||!finite(translation)||!std::isfinite(yaw)||!std::isfinite(scale)||scale<=0){e="Invalid group transform";return false;}
 Document next=d;const double c=std::cos(yaw*.017453292519943295),s=std::sin(yaw*.017453292519943295);
 for(unsigned key:keys){Pose pose;effective_pose(d,source,key,pose);const double dx=(pose.root.x-pivot.x)*scale,dz=(pose.root.z-pivot.z)*scale;
  const double x=pivot.x+c*dx+s*dz+translation.x,z=pivot.z-s*dx+c*dz+translation.z;
  const float new_yaw=float(std::remainder(pose.yaw*57.29577951308232+yaw,360.)),new_scale=float(pose.scale*scale),offset=float(pose.offset_y+translation.y);
  if(key<added_key){auto it=std::find_if(next.overrides.begin(),next.overrides.end(),[&](const auto &o){return o.index==key;});if(it==next.overrides.end()){next.overrides.push_back({key,x,z,new_yaw,new_scale,pose.hidden,offset});it=std::prev(next.overrides.end());}else{it->x=x;it->z=z;it->yaw=new_yaw;it->scale=new_scale;it->offset_y=offset;}vw_transform::extra(*it,vw_transform::pose(pose));}
  else{auto &p=next.placements[key-added_key];p.x=x;p.z=z;p.yaw=new_yaw;p.scale=new_scale;p.offset_y=offset;}
 }
 if(!valid(next,e))return false;
 d=std::move(next);return true;
}
bool duplicate_selection(Document &d,const std::vector<Pose> &source,const std::vector<unsigned> &keys,Vec delta,std::vector<unsigned> &created,std::string &e){
 created.clear();if(!selection(d,source,keys,e)||!finite(delta))return false;
 if(d.placements.size()+keys.size()>placement_limit){e="1024 added objects reached";return false;}
 Document next=d;for(unsigned key:keys){Pose p;effective_pose(d,source,key,p);if(p.hidden){e="Cannot duplicate a hidden object";return false;}Placement a;a.alias=p.alias;a.x=p.root.x+delta.x;a.z=p.root.z+delta.z;a.yaw=float(p.yaw*57.29577951308232);a.scale=float(p.scale);a.burial=p.burial;a.elevation=p.elevation;a.offset_y=p.offset_y+float(delta.y);a.pitch=float(p.pitch*57.29577951308232);a.roll=float(p.roll*57.29577951308232);a.sx=float(p.sx);a.sy=float(p.sy);a.sz=float(p.sz);next.placements.push_back(a);created.push_back(added_key+unsigned(next.placements.size()-1));}
 if(!valid(next,e)){created.clear();return false;}d=std::move(next);return true;
}
void erase_placement(Document &d,unsigned index){
 if(index>=d.placements.size())return;
 std::vector<world_entity::Binding> erased;std::string error;if(!world_entity::erase_binding(d.entities,{world_entity::Kind::Placement,index},world_entity::ChildPolicy::PreserveWorld,erased,error))return;
 d.placements.erase(d.placements.begin()+index);const unsigned key=added_key+index;
 for(auto &g:d.groups){g.members.erase(std::remove(g.members.begin(),g.members.end(),key),g.members.end());for(auto &k:g.members)if(k>key)--k;}
 d.groups.erase(std::remove_if(d.groups.begin(),d.groups.end(),[](const auto &g){return g.members.empty();}),d.groups.end());
}
bool delete_selection(Document &d,const std::vector<Pose> &source,const std::vector<unsigned> &keys,std::string &e){
 if(!selection(d,source,keys,e))return false;
 Document next=d;std::vector<unsigned> ordered=keys;std::sort(ordered.rbegin(),ordered.rend());
 for(unsigned key:ordered){if(key>=added_key)erase_placement(next,key-added_key);else{Pose p;effective_pose(next,source,key,p);auto it=std::find_if(next.overrides.begin(),next.overrides.end(),[&](const auto &o){return o.index==key;});if(it==next.overrides.end()){next.overrides.push_back({key,p.root.x,p.root.z,float(p.yaw*57.29577951308232),float(p.scale),true,p.offset_y});vw_transform::extra(next.overrides.back(),vw_transform::pose(p));}else it->hidden=true;}}
 if(!valid(next,e))return false;
 d=std::move(next);return true;
}
}
