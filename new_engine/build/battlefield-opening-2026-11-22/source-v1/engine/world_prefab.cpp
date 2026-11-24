#include "engine/world_platform.h"
#include "engine/world_transform_math.h"
#include "engine/world_prefab.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unistd.h>
namespace world_prefab {
namespace {
using namespace world_entity;
constexpr double radians=.017453292519943295;
bool near(double a,double b){return std::abs(a-b)<=1e-6+1e-10*std::max(std::abs(a),std::abs(b));}
bool same_transform(const Transform &a,const Transform &b){const auto x=vw_transform::matrix(a),y=vw_transform::matrix(b);for(unsigned i=0;i<3;i++){if(!near(x.p[i],y.p[i]))return false;for(unsigned j=0;j<3;j++)if(!near(x.a[i][j],y.a[i][j]))return false;}return true;}
bool name_ok(const std::string &s){return !s.empty()&&s.size()<=64&&s.find_first_of("\r\n\t")==std::string::npos;}
bool ground_y(Ground g,double x,double z,double &y,std::string &e){y=g.height?g.height(x,z,g.context):0.;if(!std::isfinite(y)||y< -2000||y>12000){e="Invalid prefab ground support height";return false;
}return true;}
Transform mapped(const Transform &from,const Transform &to,const Transform &value){return compose(to,relative(from,value));}
bool same(const world_studio::Asset &a,const world_studio::Asset &b){return a.alias==b.alias&&a.stem==b.stem&&a.lods==b.lods&&a.collision==b.collision&&a.vegetation==b.vegetation&&a.distance==b.distance&&a.split==b.split&&a.units==b.units;}
bool same(const world_studio::Material &a,const world_studio::Material &b){return a.alias==b.alias&&a.tint==b.tint&&a.roughness==b.roughness&&a.normal==b.normal&&a.uv==b.uv;}
bool same(const world_graph::Graph &a,const world_graph::Graph &b){if(a.alias!=b.alias||a.outputs!=b.outputs||a.nodes.size()!=b.nodes.size()||a.output_x!=b.output_x||a.output_y!=b.output_y)return false;
for(size_t i=0;i<a.nodes.size();i++){const auto &x=a.nodes[i],&y=b.nodes[i];if(x.id!=y.id||x.op!=y.op||x.inputs!=y.inputs||x.value!=y.value||x.x!=y.x||x.y!=y.y)return false;
}return true;}
template<class T>bool merge(std::vector<T> &to,const std::vector<T> &from,std::string &e){for(const auto &item:from){auto at=std::find_if(to.begin(),to.end(),[&](const T &v){return v.alias==item.alias;});if(at==to.end())to.push_back(item);else if(!same(*at,item)){e="Prefab dependency conflicts with project alias: "+item.alias;return false;
}}return true;}
template<class T>void dependency(const std::vector<T> &from,const std::string &alias,std::vector<T> &to){for(const auto &v:from)if(v.alias==alias&&std::none_of(to.begin(),to.end(),[&](const T &x){return x.alias==alias;}))to.push_back(v);}
std::size_t object_count(const world_studio::Document &d){return d.placements.size()+d.lights.size()+d.actors.size()+d.particles.size()+d.waters.size()+d.navigation.size()+d.sounds.size()+d.weather_areas.size();}
bool append_node(Graph &g,Id parent,Binding b,const Transform &world,const std::string &name,std::string &e){Transform p;if(parent&&!world_transform(g,parent,p,e))return false;
Id ignored=0;return add(g,parent,name,b,relative(p,world),ignored,e);}
bool object_world(const world_studio::Document &d,Binding b,Ground ground,const std::vector<double> *heights,Transform &out,std::string &e){
 out={};switch(b.kind){
 case Kind::Placement:{if(b.index>=d.placements.size())break;const auto &p=d.placements[b.index];double y=0;if(heights)y=(*heights)[b.index];else if(!ground_y(ground,p.x,p.z,y,e))return false;
else y+=p.offset_y+p.elevation-p.burial;
out={p.x,y,p.z,p.yaw,p.scale,p.pitch,p.roll,p.sx,p.sy,p.sz};return true;}
 case Kind::Light:{if(b.index>=d.lights.size())break;const auto &p=d.lights[b.index];out={p.x,p.y,p.z,p.yaw,1};return true;}
 case Kind::Actor:{if(b.index>=d.actors.size()||d.actors[b.index].points.empty())break;const auto &p=d.actors[b.index];double y=0;if(!p.floor_heights.empty())y=p.floor_heights[0];else if(!ground_y(ground,p.points[0][0],p.points[0][1],y,e))return false;
out={p.points[0][0],y,p.points[0][1],p.yaw,1};return true;}
 case Kind::Particle:{if(b.index>=d.particles.size())break;const auto &p=d.particles[b.index];out={p.x,p.y,p.z,0,1};return true;}
 case Kind::Sound:{if(b.index>=d.sounds.size())break;const auto &p=d.sounds[b.index];out={p.x,p.y,p.z,0,1};return true;}
 case Kind::Weather:{if(b.index>=d.weather_areas.size())break;const auto &p=d.weather_areas[b.index];out={p.x,p.y,p.z,0,1};return true;}
 case Kind::Water:{if(b.index>=d.waters.size())break;const auto &p=d.waters[b.index];out={p.x,p.level,p.z,0,1};return true;}
 case Kind::Navigation:{if(b.index>=d.navigation.size())break;const auto &p=d.navigation[b.index];out={p.x,p.source_floor?p.floor_y:0,p.z,0,p.span};return true;}
 default:break;
 }e="Prefab object binding is missing or unsupported";return false;

}
bool apply_object(world_studio::Document &d,Binding b,const Transform &old_world,const Transform &new_world,const Transform &from,const Transform &to,Ground ground,std::string &e){
 if(b.kind!=Kind::Placement&&(!vw_transform::simple(from)||!vw_transform::simple(to))){e="Typed water, route, light and particle attachments require a level, uniform-scale bundle. Transform the model separately.";return false;}
 const double ratio=to.scale/from.scale,angle=std::remainder(to.yaw-from.yaw,360.);auto point=[&](double x,double y,double z){return mapped(from,to,{x,y,z,0,1});};
 switch(b.kind){
 case Kind::Placement:{auto &p=d.placements[b.index];double gy=0;if(!ground_y(ground,new_world.x,new_world.z,gy,e))return false;
p.x=new_world.x;p.z=new_world.z;p.yaw=float(new_world.yaw);p.scale=float(new_world.scale);vw_transform::extra(p,new_world);p.burial=float(p.burial*ratio);p.elevation=float(p.elevation*ratio);p.offset_y=float(new_world.y-gy+p.burial-p.elevation);return true;}
 case Kind::Light:{auto &p=d.lights[b.index];p.x=new_world.x;p.y=new_world.y;p.z=new_world.z;p.yaw=float(new_world.yaw);p.radius=float(p.radius*ratio);return true;}
 case Kind::Actor:{auto &p=d.actors[b.index];for(unsigned i=0;i<p.points.size();++i){auto &v=p.points[i];const auto q=point(v[0],p.floor_heights.empty()?0:p.floor_heights[i],v[1]);v={q.x,q.z};if(!p.floor_heights.empty())p.floor_heights[i]=q.y;}p.yaw=float(std::remainder(p.yaw+angle,360.));return true;}
 case Kind::Particle:{auto &p=d.particles[b.index];p.ceiling=new_world.y+(p.ceiling-old_world.y)*ratio;p.x=new_world.x;p.y=new_world.y;p.z=new_world.z;p.radius=float(p.radius*ratio);p.height=float(p.height*ratio);p.size=float(p.size*ratio);return true;}
 case Kind::Sound:{auto &p=d.sounds[b.index];p.x=new_world.x;p.y=new_world.y;p.z=new_world.z;p.inner_radius=float(p.inner_radius*ratio);p.radius=float(p.radius*ratio);return true;}
 case Kind::Weather:{auto &p=d.weather_areas[b.index];p.x=new_world.x;p.y=new_world.y;p.z=new_world.z;p.inner_radius=float(p.inner_radius*ratio);p.radius=float(p.radius*ratio);return true;}
 case Kind::Water:{auto &p=d.waters[b.index];if(p.source!=3){e="Only local water can belong to a portable prefab";return false;
}if(!near(p.radius_x,p.radius_z)&&std::abs(std::remainder(angle,90.))>1e-6){e="Noncircular ponds support only quarter-turn rotations";return false;
}const bool swap=std::abs(std::remainder(angle,180.))>45;const float x=p.radius_x,z=p.radius_z;p.radius_x=float((swap?z:x)*ratio);p.radius_z=float((swap?x:z)*ratio);p.x=new_world.x;p.z=new_world.z;p.level=new_world.y;return true;}
 case Kind::Navigation:{auto &p=d.navigation[b.index];p.x=new_world.x;p.z=new_world.z;if(p.source_floor)p.floor_y=new_world.y;p.span=float(new_world.scale*(std::abs(std::cos(new_world.yaw*radians))+std::abs(std::sin(new_world.yaw*radians))));p.cell=float(p.cell*ratio);p.radius=float(p.radius*ratio);return true;}
 default:e="Unsupported prefab binding";return false;

 }
}
struct Temp {std::string path;~Temp(){if(!path.empty())unlink(path.c_str());}};
bool temp_file(Temp &t,std::string &e){std::string p=vw_temp_pattern("valwood-prefab-document-XXXXXX");int fd=mkstemp(p.data());if(fd<0){e="Cannot create prefab document transaction";return false;
}close(fd);t.path=p;return true;}
bool write_bytes(const char *path,const std::string &text,std::string &e){int fd=vw_open(path,O_WRONLY|O_TRUNC);if(fd<0){e="Cannot open prefab document transaction";return false;
}size_t done=0;bool ok=true;while(done<text.size()){ssize_t n=write(fd,text.data()+done,text.size()-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=size_t(n);}if(close(fd)!=0)ok=false;if(!ok)e="Cannot write prefab document transaction";return ok;}
bool read_bytes(const char *path,std::string &text,size_t limit,std::string &e){std::ifstream f(std::filesystem::u8path(path),std::ios::binary|std::ios::ate);if(!f||f.tellg()<0||f.tellg()>std::streamoff(limit)){e="Missing or oversized prefab file";return false;
}const auto size=size_t(f.tellg());f.seekg(0);std::string next(size,'\0');if(size&&!f.read(&next[0],std::streamsize(size))){e="Cannot read complete prefab file";return false;
}text=std::move(next);return true;}
}
bool validate(const Prefab &p,std::string &e){
 e.clear();const auto &d=p.content;
 if(!name_ok(p.name)||!valid_transform(p.origin)||!world_studio::valid(d,e)||!world_entity::validate(p.graph,&d,e)){if(e.empty())e="Invalid prefab";return false;
}
 if(!d.source_signature.empty()||!d.terrain.empty()||!d.overrides.empty()||!d.stamps.empty()||!d.ecology.empty()||!d.decals.empty()||!d.splines.empty()||!d.structures.empty()||d.next_structure_id!=1||!d.groups.empty()||!d.environment.path.empty()||!world_weather_authoring::default_global(d.weather_global)){e="Prefab contains nonportable global authoring data";return false;
}
 if(object_count(d)==0||p.graph.nodes.size()!=object_count(d)+1||p.model_heights.size()!=d.placements.size()){e="Prefab object/height metadata is incomplete";return false;
}
 const auto *root=find(p.graph,p.root);if(!root||root->parent||root->binding.kind!=Kind::None){e="Prefab root is missing or bound to an object";return false;
}
 Transform rw;if(!world_transform(p.graph,p.root,rw,e))return false;

 if(!same_transform(rw,p.origin)){e="Prefab origin disagrees with its root";return false;
}
 for(double y:p.model_heights)if(!std::isfinite(y)||y< -2000||y>12000){e="Invalid captured model support height";return false;
}
 for(const auto &a:d.actors)if(a.source!=-1){e="Prefab actor retains a source resident override";return false;
}
 for(const auto &fx:d.particles)if(fx.source!=-1){e="Prefab emitter retains a source override";return false;
}
 for(const auto &w:d.waters)if(w.source!=3){e="Fixed water overrides cannot be captured";return false;
}
 for(const auto &n:p.graph.nodes){if(n.id==p.root)continue;const auto *parent=&n;while(parent&&parent->parent&&parent->parent!=p.root)parent=find(p.graph,parent->parent);if(!parent||parent->parent!=p.root||n.binding.kind==Kind::None||n.binding.kind==Kind::SourceModel){e="Prefab has detached or unsupported entities";return false;
}
  Transform expected,actual;if(!object_world(d,n.binding,{},&p.model_heights,expected,e)||!world_transform(p.graph,n.id,actual,e))return false;

  if((n.binding.kind==Kind::Placement?!same_transform(expected,actual):(!near(expected.x,actual.x)||!near(expected.z,actual.z)||(n.binding.kind!=Kind::Sound&&n.binding.kind!=Kind::Weather&&(!near(std::remainder(expected.yaw-actual.yaw,360.),0)||!near(expected.scale,actual.scale)))||((n.binding.kind!=Kind::Actor&&n.binding.kind!=Kind::Navigation)&&!near(expected.y,actual.y))))){e="Prefab entity and content anchors disagree";return false;
}
 }
 return true;
}
bool capture(const world_studio::Document &source,const Selection &selection,const Transform &origin,const std::string &name,Prefab &out,std::string &e){
 e.clear();if(!world_studio::valid(source,e)||!valid_transform(origin)||selection.objects.empty()||selection.objects.size()>128||!name_ok(name)){if(e.empty())e="Select 1–128 portable objects and a valid origin";return false;
}
 Prefab p;p.name=name;p.origin=origin;std::vector<std::pair<unsigned,unsigned>> navigation;
 auto copy_nav=[&](unsigned old)->int{for(auto m:navigation)if(m.first==old)return int(m.second);if(old>=source.navigation.size()){e="Missing actor navigation dependency";return -1;}const unsigned index=unsigned(p.content.navigation.size());p.content.navigation.push_back(source.navigation[old]);navigation.push_back({old,index});return int(index);};
 for(size_t k=0;k<selection.objects.size();k++){const auto b=selection.objects[k];for(size_t j=0;j<k;j++)if(selection.objects[j]==b){e="Duplicate prefab selection";return false;
}
  switch(b.kind){
  case Kind::Placement:case Kind::SourceModel:{world_studio::Placement model;double y=0;if(b.kind==Kind::SourceModel){auto at=std::find_if(selection.source_models.begin(),selection.source_models.end(),[&](const SourceModel &s){return s.index==b.index;});if(b.index>=WORLD_ADDED_KEY_BASE||at==selection.source_models.end()||at->hidden){e="Resolve the visible source model before capture";return false;
}model=at->placement;y=at->root_y;}
   else{if(b.index>=source.placements.size()){e="Missing selected model";return false;
}model=source.placements[b.index];if(!ground_y(selection.ground,model.x,model.z,y,e))return false;
y+=model.offset_y+model.elevation-model.burial;}
   p.content.placements.push_back(model);p.model_heights.push_back(y);dependency(source.assets,model.alias,p.content.assets);dependency(source.materials,model.alias,p.content.materials);dependency(source.graphs,model.alias,p.content.graphs);break;}
  case Kind::Light:if(b.index>=source.lights.size()){e="Missing selected light";return false;
}p.content.lights.push_back(source.lights[b.index]);break;
  case Kind::Actor:{if(b.index>=source.actors.size()){e="Missing selected actor";return false;
}auto a=source.actors[b.index];a.source=-1;if(a.navigation>=0){a.navigation=copy_nav(unsigned(a.navigation));if(a.navigation<0)return false;
}p.content.actors.push_back(std::move(a));break;}
  case Kind::Particle:{if(b.index>=source.particles.size()){e="Missing selected emitter";return false;
}auto fx=source.particles[b.index];fx.source=-1;p.content.particles.push_back(fx);break;}
  case Kind::Sound:if(b.index>=source.sounds.size()){e="Missing selected sound";return false;}p.content.sounds.push_back(source.sounds[b.index]);break;
  case Kind::Weather:if(b.index>=source.weather_areas.size()){e="Missing selected weather area";return false;}p.content.weather_areas.push_back(source.weather_areas[b.index]);break;
  case Kind::Water:if(b.index>=source.waters.size()||source.waters[b.index].source!=3){e="Select a local pond, not a sea/moat override";return false;
}p.content.waters.push_back(source.waters[b.index]);break;
  case Kind::Navigation:if(copy_nav(b.index)<0)return false;
break;
  default:e="Unsupported prefab object kind";return false;

  }
 }
 if(!add(p.graph,0,name,{},origin,p.root,e))return false;

 const Kind kinds[]={Kind::Placement,Kind::Light,Kind::Actor,Kind::Particle,Kind::Water,Kind::Navigation,Kind::Sound,Kind::Weather};
 const size_t counts[]={p.content.placements.size(),p.content.lights.size(),p.content.actors.size(),p.content.particles.size(),p.content.waters.size(),p.content.navigation.size(),p.content.sounds.size(),p.content.weather_areas.size()};
 for(unsigned k=0;k<8;k++)for(unsigned i=0;i<counts[k];i++){Binding b{kinds[k],i};Transform world;if(!object_world(p.content,b,selection.ground,&p.model_heights,world,e)||!append_node(p.graph,p.root,b,world,name+" / "+std::to_string(unsigned(b.kind))+" "+std::to_string(i),e))return false;
}
 if(!validate(p,e))return false;
out=std::move(p);return true;
}
bool instantiate(const Prefab &p,world_studio::Document &document,Graph &graph,const Transform &target,Id parent,Instance &out,std::string &e,Ground ground){
 if(!validate(p,e)||!world_studio::valid(document,e)||!world_entity::validate(graph,&document,e)||!valid_transform(target)){if(e.empty())e="Invalid prefab destination";return false;
}
 Transform parent_world;if(parent&&!world_transform(graph,parent,parent_world,e))return false;

 auto next=document;auto entities=graph;Instance result;
 if(!merge(next.assets,p.content.assets,e)||!merge(next.materials,p.content.materials,e)||!merge(next.graphs,p.content.graphs,e))return false;

 std::vector<std::pair<Binding,Binding>> bindings;
 auto copy=[&](auto &to,const auto &from,Kind kind){const unsigned base=unsigned(to.size());to.insert(to.end(),from.begin(),from.end());for(unsigned i=0;i<from.size();i++)bindings.push_back({{kind,i},{kind,base+i}});return base;};
 const unsigned nav_base=copy(next.navigation,p.content.navigation,Kind::Navigation);copy(next.placements,p.content.placements,Kind::Placement);copy(next.lights,p.content.lights,Kind::Light);const unsigned actor_base=copy(next.actors,p.content.actors,Kind::Actor);copy(next.particles,p.content.particles,Kind::Particle);copy(next.waters,p.content.waters,Kind::Water);copy(next.sounds,p.content.sounds,Kind::Sound);copy(next.weather_areas,p.content.weather_areas,Kind::Weather);
 for(size_t i=actor_base;i<next.actors.size();i++)if(next.actors[i].navigation>=0)next.actors[i].navigation+=int(nav_base);
 if(entities.nodes.size()+p.graph.nodes.size()>node_limit){e="Prefab exceeds entity graph budget";return false;
}
 // Graph order is not a hierarchy contract; resolve parents before children.
 std::vector<Id> pending;for(const auto &n:p.graph.nodes)pending.push_back(n.id);
 while(!pending.empty()){bool progress=false;for(auto it=pending.begin();it!=pending.end();){const auto &node=*find(p.graph,*it);Id new_parent=parent;if(node.id!=p.root){auto at=std::find_if(result.ids.begin(),result.ids.end(),[&](const auto &m){return m.first==node.parent;});if(at==result.ids.end()){++it;continue;}new_parent=at->second;}
   Binding binding=node.binding;Transform local=node.id==p.root?relative(parent_world,target):node.local;
   if(binding.kind!=Kind::None){auto at=std::find_if(bindings.begin(),bindings.end(),[&](const auto &m){return m.first==binding;});if(at==bindings.end()){e="Missing prefab binding map";return false;
}binding=at->second;result.objects.push_back(binding);}
   Id created=0;if(!add(entities,new_parent,node.name,binding,local,created,e))return false;
result.ids.push_back({node.id,created});if(node.id==p.root)result.root=created;
   if(binding.kind!=Kind::None){Transform old_world,new_world;if(!world_transform(p.graph,node.id,old_world,e)||!world_transform(entities,created,new_world,e)||!apply_object(next,binding,old_world,new_world,p.origin,target,ground,e))return false;
}
   it=pending.erase(it);progress=true;
  }if(!progress){e="Prefab contains an unresolved parent";return false;
}
 }
 if(!world_studio::valid(next,e)||!world_entity::validate(entities,&next,e))return false;
document=std::move(next);graph=std::move(entities);out=std::move(result);return true;
}
bool transform_instance(world_studio::Document &document,Graph &graph,Id root,const Transform &target,std::string &e,Ground ground){
 if(!world_studio::valid(document,e)||!world_entity::validate(graph,&document,e))return false;
const auto *root_node=find(graph,root);if(!root_node||root_node->binding.kind!=Kind::None){e="Choose a prefab/group root";return false;
}
 for(const Node *ancestor=root_node;ancestor;ancestor=ancestor->parent?find(graph,ancestor->parent):nullptr)if(ancestor->locked){e="Bundle or its parent is locked";return false;}
 for(const auto &node:graph.nodes){const Node *ancestor=&node;while(ancestor&&ancestor->id!=root&&ancestor->parent)ancestor=find(graph,ancestor->parent);if(ancestor&&ancestor->id==root&&node.locked){e="Bundle contains a locked descendant";return false;}}
 Transform before;if(!world_transform(graph,root,before,e))return false;
auto next=document;auto entities=graph;if(!set_world(entities,root,target,e))return false;

 for(const auto &n:graph.nodes){const Node *ancestor=&n;while(ancestor&&ancestor->id!=root&&ancestor->parent)ancestor=find(graph,ancestor->parent);if(!ancestor||ancestor->id!=root||n.binding.kind==Kind::None)continue;Transform old_world,new_world;if(!world_transform(graph,n.id,old_world,e)||!world_transform(entities,n.id,new_world,e)||!apply_object(next,n.binding,old_world,new_world,before,target,ground,e))return false;
}
 if(!world_studio::valid(next,e)||!world_entity::validate(entities,&next,e))return false;
document=std::move(next);graph=std::move(entities);return true;
}
bool save(const char *path,const Prefab &p,std::string &e){
 if(!path||!*path||!validate(p,e))return false;
Temp document;if(!temp_file(document,e)||!world_studio::save(document.path.c_str(),p.content,e))return false;
std::string content,metadata;if(!read_bytes(document.path.c_str(),content,4*1024*1024,e)||!encode(p.graph,metadata,e))return false;

 std::ostringstream header;header<<std::setprecision(17)<<"VALWOOD_PREFAB 1\n"<<std::quoted(p.name)<<'\n'<<p.origin.x<<' '<<p.origin.y<<' '<<p.origin.z<<' '<<p.origin.yaw<<' '<<p.origin.scale<<' '<<p.root<<' '<<p.model_heights.size();for(double y:p.model_heights)header<<' '<<y;header<<'\n'<<content.size()<<' '<<metadata.size()<<'\n';const std::string bytes=header.str()+content+metadata;
 std::string pattern=std::string(path)+".tmp.XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);int fd=mkstemp(name.data());if(fd<0){e="Cannot create prefab save transaction";return false;
}Temp transaction;transaction.path=name.data();size_t done=0;bool ok=true;while(done<bytes.size()){ssize_t n=write(fd,bytes.data()+done,bytes.size()-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=size_t(n);}if(ok)ok=fsync(fd)==0;if(close(fd)!=0)ok=false;if(ok)ok=vw_replace(transaction.path.c_str(),path)==0;if(!ok){e="Prefab save failed; previous file preserved";return false;
}transaction.path.clear();return true;
}
bool load(const char *path,Prefab &out,std::string &e){
e.clear();if(!path||!*path){e="Missing prefab path";return false;
}std::string bytes;if(!read_bytes(path,bytes,7*1024*1024,e))return false;
std::istringstream in(bytes);std::string magic,name_line,origin_line,size_line,extra;if(!std::getline(in,magic)||magic!="VALWOOD_PREFAB 1"||!std::getline(in,name_line)||!std::getline(in,origin_line)||!std::getline(in,size_line)){e="Unsupported/truncated prefab wrapper";return false;
}
 Prefab p;std::istringstream names(name_line),origin(origin_line),sizes(size_line);size_t count=0,content_bytes=0,graph_bytes=0;
 if(!(names>>std::quoted(p.name))||(names>>extra)||!(origin>>p.origin.x>>p.origin.y>>p.origin.z>>p.origin.yaw>>p.origin.scale>>p.root>>count)||count>512){e="Malformed prefab header";return false;
}for(size_t i=0;i<count;i++){double y=0;if(!(origin>>y)){e="Truncated prefab model heights";return false;
}p.model_heights.push_back(y);}
 if((origin>>extra)||!(sizes>>content_bytes>>graph_bytes)||(sizes>>extra)||content_bytes>4*1024*1024||graph_bytes>2*1024*1024){e="Invalid prefab payload sizes";return false;
}const auto position=in.tellg();if(position<0||size_t(position)>bytes.size()||content_bytes>bytes.size()-size_t(position)||graph_bytes!=bytes.size()-size_t(position)-content_bytes){e="Prefab payload length mismatch";return false;
}
 const auto content=bytes.substr(size_t(position),content_bytes),metadata=bytes.substr(size_t(position)+content_bytes,graph_bytes);Temp document;if(!temp_file(document,e)||!write_bytes(document.path.c_str(),content,e)||!world_studio::load(document.path.c_str(),p.content,e)||!decode(metadata,p.graph,e))return false;
const auto *root=find(p.graph,p.root);if(!root){e="Missing prefab root";return false;}p.origin.pitch=root->local.pitch;p.origin.roll=root->local.roll;p.origin.sx=root->local.sx;p.origin.sy=root->local.sy;p.origin.sz=root->local.sz;if(!validate(p,e))return false;
out=std::move(p);return true;
}
}
