#include "engine/world_entity_graph.h"
#include "engine/world_studio.h"
#include "engine/world_transform_math.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
namespace world_entity {
namespace {
constexpr double radians=.017453292519943295;
bool named(const std::string &s){return !s.empty()&&s.size()<=96&&s.find_first_of("\r\n\t")==std::string::npos;}
bool source_key_ok(const std::string &s){return !s.empty()&&s.size()<=1024&&s.find_first_of("\r\n\t")==std::string::npos;}
bool read_id(std::istream &in,Id &id){std::string token;if(!(in>>token)||token.empty()||token.size()>20||!std::all_of(token.begin(),token.end(),[](char c){return c>='0'&&c<='9';}))return false;const auto parsed=std::from_chars(token.data(),token.data()+token.size(),id);return parsed.ec==std::errc{}&&parsed.ptr==token.data()+token.size();}
bool binding_valid(Binding b,const world_studio::Document *d){
 switch(b.kind){
 case Kind::None:return b.index==0;
 case Kind::SourceModel:return b.index<8192;
 case Kind::Placement:return b.index<(d?d->placements.size():world_studio::placement_limit);
 case Kind::Light:return b.index<(d?d->lights.size():24);
 case Kind::Actor:return b.index<(d?d->actors.size():32);
 case Kind::Particle:return b.index<(d?d->particles.size():16);
 case Kind::Water:return b.index<(d?d->waters.size():8);
 case Kind::Navigation:return b.index<(d?d->navigation.size():4);
 }return false;

}
bool allocate(Graph &g,Id &id,std::string &e){
 if(g.next_id==0||g.next_id==std::numeric_limits<Id>::max()){e="Persistent entity ID space exhausted";return false;
}
 id=g.next_id++;return true;
}
bool descendants(const Graph &g,Id root,std::vector<Id> &out,std::string &e){
 if(!validate(g,nullptr,e)||!find(g,root)){if(e.empty())e="Missing entity root";return false;
}
 out.push_back(root);for(size_t k=0;k<out.size();k++)for(const auto &n:g.nodes)if(n.parent==out[k])out.push_back(n.id);
 return true;
}
}
bool operator==(Binding a,Binding b){return a.kind==b.kind&&a.index==b.index;}
bool operator!=(Binding a,Binding b){return !(a==b);}
const Node *find(const Graph &g,Id id){for(const auto &n:g.nodes)if(n.id==id)return &n;return nullptr;}
Node *find(Graph &g,Id id){for(auto &n:g.nodes)if(n.id==id)return &n;return nullptr;}
bool visible(const Graph &g,Binding b){const Node *node=nullptr;for(const auto &n:g.nodes)if(n.binding==b&&b.kind!=Kind::None){node=&n;break;}unsigned depth=0;while(node){if(!node->visible||++depth>g.nodes.size())return false;node=node->parent?find(g,node->parent):nullptr;}return true;}
bool editable(const Graph &g,Binding b){const Node *node=nullptr;for(const auto &n:g.nodes)if(n.binding==b&&b.kind!=Kind::None){node=&n;break;}unsigned depth=0;while(node){if(node->locked||++depth>g.nodes.size())return false;node=node->parent?find(g,node->parent):nullptr;}return true;}
bool valid_transform(const Transform &t){
 return std::isfinite(t.x)&&std::abs(t.x)<=1e9&&std::isfinite(t.y)&&std::abs(t.y)<=1e9&&std::isfinite(t.z)&&std::abs(t.z)<=1e9&&std::isfinite(t.yaw)&&std::abs(t.yaw)<=360000&&std::isfinite(t.scale)&&t.scale>=1e-6&&t.scale<=1e6&&vw_transform::extra_valid(t.pitch,t.roll,t.sx,t.sy,t.sz);
}
Transform compose(const Transform &p,const Transform &l){if(vw_transform::identity(p))return l;if(vw_transform::identity(l))return p;if(!vw_transform::simple(p)||!vw_transform::simple(l))return vw_transform::decompose(vw_transform::multiply(vw_transform::matrix(p),vw_transform::matrix(l)));const double c=std::cos(p.yaw*radians),s=std::sin(p.yaw*radians);return {p.x+p.scale*(c*l.x+s*l.z),p.y+p.scale*l.y,p.z+p.scale*(-s*l.x+c*l.z),std::remainder(p.yaw+l.yaw,360.),p.scale*l.scale};}
Transform relative(const Transform &p,const Transform &w){if(vw_transform::identity(p))return w;if(!vw_transform::simple(p)||!vw_transform::simple(w))return vw_transform::decompose(vw_transform::multiply(vw_transform::inverse(vw_transform::matrix(p)),vw_transform::matrix(w)));const double c=std::cos(p.yaw*radians),s=std::sin(p.yaw*radians),x=(w.x-p.x)/p.scale,z=(w.z-p.z)/p.scale;return {c*x-s*z,(w.y-p.y)/p.scale,s*x+c*z,std::remainder(w.yaw-p.yaw,360.),w.scale/p.scale};}
bool validate(const Graph &g,const world_studio::Document *d,std::string &e){
 e.clear();auto fail=[&](const char *m){e=m;return false;
};
 if(g.nodes.size()>node_limit||g.next_id==0)return fail("Entity graph budget or ID counter invalid");
 std::unordered_map<Id,size_t> indices;std::unordered_set<std::uint64_t> bindings;
 for(size_t i=0;i<g.nodes.size();i++){const auto &n=g.nodes[i];if(n.id==0||n.id>=g.next_id||!indices.emplace(n.id,i).second||!named(n.name)||!valid_transform(n.local)||!binding_valid(n.binding,d))return fail("Invalid entity identity, transform or object binding");if(!n.source_key.empty()&&(n.binding.kind!=Kind::SourceModel||!source_key_ok(n.source_key)))return fail("Invalid source entity key");if(n.binding.kind!=Kind::None&&!bindings.insert((std::uint64_t(unsigned(n.binding.kind))<<32)|n.binding.index).second)return fail("Multiple entities bind the same scene object");}
 std::vector<unsigned> state(g.nodes.size());std::vector<Transform> worlds(g.nodes.size());
 std::function<bool(size_t)> visit=[&](size_t i){if(state[i]==2)return true;if(state[i]==1)return fail("Entity graph contains a parent cycle");state[i]=1;const auto &n=g.nodes[i];Transform p;if(n.parent){auto at=indices.find(n.parent);if(at==indices.end())return fail("Entity parent is missing");if(!visit(at->second))return false;
p=worlds[at->second];}worlds[i]=compose(p,n.local);if(!valid_transform(worlds[i]))return fail("Composed transform exceeds limits or introduces shear; use uniform parent scale or an unparented object");state[i]=2;return true;};
 for(size_t i=0;i<g.nodes.size();i++)if(!visit(i))return false;

 return true;
}
bool world_transform(const Graph &g,Id id,Transform &out,std::string &e){
 if(!validate(g,nullptr,e))return false;
std::vector<const Node *> chain;const Node *n=find(g,id);if(!n){e="Missing entity";return false;
}
 while(n){chain.push_back(n);n=n->parent?find(g,n->parent):nullptr;}Transform result;for(auto it=chain.rbegin();it!=chain.rend();it++)result=compose(result,(*it)->local);out=result;return true;
}
bool world_transforms(const Graph &g,std::vector<std::pair<Id,Transform>> &out,std::string &e){
 if(!validate(g,nullptr,e))return false;
 std::unordered_map<Id,size_t> indices;for(size_t i=0;i<g.nodes.size();i++)indices.emplace(g.nodes[i].id,i);
 std::vector<std::pair<Id,Transform>> result(g.nodes.size());std::vector<bool> ready(g.nodes.size());
 std::function<Transform(size_t)> resolve=[&](size_t i){if(ready[i])return result[i].second;const auto &n=g.nodes[i];Transform p;if(n.parent)p=resolve(indices.at(n.parent));result[i]={n.id,compose(p,n.local)};ready[i]=true;return result[i].second;};
 for(size_t i=0;i<g.nodes.size();i++)resolve(i);
 out=std::move(result);return true;
}
bool add(Graph &g,Id parent,const std::string &name,Binding binding,const Transform &local,Id &created,std::string &e){
 if(!validate(g,nullptr,e))return false;
if(parent&&!find(g,parent)){e="Missing entity parent";return false;
}Graph next=g;Id id=0;if(!allocate(next,id,e))return false;
next.nodes.push_back({id,parent,name,binding,local,{}});if(!validate(next,nullptr,e))return false;
g=std::move(next);created=id;return true;
}
bool set_world(Graph &g,Id id,const Transform &world,std::string &e){
 if(!validate(g,nullptr,e))return false;
Graph next=g;auto *n=find(next,id);if(!n){e="Missing entity";return false;
}Transform parent;if(n->parent&&!world_transform(next,n->parent,parent,e))return false;
n->local=relative(parent,world);if(!validate(next,nullptr,e))return false;
g=std::move(next);return true;
}
bool reparent(Graph &g,Id id,Id parent,bool preserve,std::string &e){
 Transform world;if(!world_transform(g,id,world,e))return false;
if(parent&&!find(g,parent)){e="Missing new parent";return false;
}Graph next=g;auto *node=find(next,id);node->parent=parent;
if(preserve){Transform p;if(parent&&!world_transform(g,parent,p,e))return false;node->local=relative(p,world);}
if(!validate(next,nullptr,e))return false;
g=std::move(next);return true;
}
bool erase_subtree(Graph &g,Id id,std::vector<Binding> &removed,std::string &e){
 std::vector<Id> ids;if(!descendants(g,id,ids,e))return false;
Graph next=g;std::vector<Binding> result;
 next.nodes.erase(std::remove_if(next.nodes.begin(),next.nodes.end(),[&](const Node &n){if(std::find(ids.begin(),ids.end(),n.id)==ids.end())return false;
if(n.binding.kind!=Kind::None)result.push_back(n.binding);
return true;}),next.nodes.end());
 if(!validate(next,nullptr,e))return false;
g=std::move(next);removed=std::move(result);return true;
}
bool erase_binding(Graph &g,Binding binding,ChildPolicy policy,std::vector<Binding> &removed,std::string &e){
 if(!validate(g,nullptr,e))return false;
 if(binding.kind==Kind::None||!binding_valid(binding,nullptr)){e="Invalid erased binding";return false;}
 Graph next=g;std::vector<Binding> result;auto at=std::find_if(next.nodes.begin(),next.nodes.end(),[&](const Node &n){return n.binding==binding;});
 if(at!=next.nodes.end()){
  const Id id=at->id,parent=at->parent;
  if(policy==ChildPolicy::EraseSubtree){if(!erase_subtree(next,id,result,e))return false;}
  else{
   std::vector<Id> children;for(const auto &n:next.nodes)if(n.parent==id)children.push_back(n.id);
   for(Id child:children)if(!reparent(next,child,parent,true,e))return false;
   next.nodes.erase(std::remove_if(next.nodes.begin(),next.nodes.end(),[&](const Node &n){return n.id==id;}),next.nodes.end());result.push_back(binding);
  }
 }
 for(auto &n:next.nodes)if(n.binding.kind==binding.kind&&n.binding.index>binding.index)--n.binding.index;
 if(!validate(next,nullptr,e))return false;
 g=std::move(next);removed=std::move(result);return true;
}
bool synchronize(Graph &g,const std::vector<std::pair<Binding,Transform>> &edits,std::string &e){
 if(!validate(g,nullptr,e))return false;
 std::vector<std::pair<Id,Transform>> desired;
 if(!world_transforms(g,desired,e))return false;
 for(size_t i=0;i<edits.size();i++){
  if(edits[i].first.kind==Kind::None||!valid_transform(edits[i].second)){e="Invalid reconciled entity transform";return false;}
  for(size_t j=0;j<i;j++)if(edits[j].first==edits[i].first){e="Duplicate reconciled binding";return false;}
  auto at=std::find_if(g.nodes.begin(),g.nodes.end(),[&](const Node &n){return n.binding==edits[i].first;});
  if(at==g.nodes.end()){e="Cannot reconcile a missing entity binding";return false;}
  for(auto &v:desired)if(v.first==at->id)v.second=edits[i].second;
 }
 Graph next=g;
 for(auto &n:next.nodes){Transform p,w;for(const auto &v:desired){if(v.first==n.id)w=v.second;if(v.first==n.parent)p=v.second;}n.local=relative(p,w);}
 if(!validate(next,nullptr,e))return false;
 g=std::move(next);return true;
}
bool source_index_map(const std::vector<std::string> &old_keys,const std::vector<std::string> &new_keys,const std::vector<unsigned> &referenced,std::vector<std::pair<unsigned,unsigned>> &out,std::string &e){
 e.clear();if(old_keys.size()>8192||new_keys.size()>8192||referenced.size()>8192){e="Source identity table exceeds scene budget";return false;}
 std::vector<std::pair<unsigned,unsigned>> result;
 for(unsigned old:referenced){
  if(std::any_of(result.begin(),result.end(),[&](const auto &m){return m.first==old;}))continue;
  if(old>=old_keys.size()||!source_key_ok(old_keys[old])){e="Missing previous source identity";return false;}
  const auto &key=old_keys[old];
  if(std::count(old_keys.begin(),old_keys.end(),key)!=1||std::count(new_keys.begin(),new_keys.end(),key)!=1){e="Source object was changed, removed or is ambiguous; repair explicitly";return false;}
  result.push_back({old,unsigned(std::find(new_keys.begin(),new_keys.end(),key)-new_keys.begin())});
 }
 out=std::move(result);return true;
}
bool remap_source_bindings(Graph &g,const std::vector<std::string> &old_keys,const std::vector<std::string> &new_keys,std::vector<std::pair<unsigned,unsigned>> &out,std::string &e){
 if(!validate(g,nullptr,e))return false;
 std::vector<unsigned> referenced;
 for(const auto &n:g.nodes)if(n.binding.kind==Kind::SourceModel){if(n.binding.index>=old_keys.size()||(!n.source_key.empty()&&n.source_key!=old_keys[n.binding.index])){e="Stored entity source identity disagrees with previous source index";return false;}referenced.push_back(n.binding.index);}
 std::vector<std::pair<unsigned,unsigned>> mapping;if(!source_index_map(old_keys,new_keys,referenced,mapping,e))return false;
 Graph next=g;
 for(auto &n:next.nodes)if(n.binding.kind==Kind::SourceModel){auto at=std::find_if(mapping.begin(),mapping.end(),[&](const auto &m){return m.first==n.binding.index;});if(at==mapping.end()){e="Missing source remap";return false;}n.binding.index=at->second;n.source_key=new_keys[n.binding.index];}
 if(!validate(next,nullptr,e))return false;
 g=std::move(next);out=std::move(mapping);return true;
}
bool clone_subtree(Graph &g,Id root,Id parent,const std::vector<std::pair<Binding,Binding>> &bindings,Id &created,std::vector<std::pair<Id,Id>> &out,std::string &e){
 std::vector<Id> ids;if(!descendants(g,root,ids,e))return false;
if(parent&&!find(g,parent)){e="Missing clone parent";return false;
}if(g.nodes.size()+ids.size()>node_limit){e="Entity graph budget exceeded";return false;
}
 Graph next=g;std::vector<std::pair<Id,Id>> mapping;Transform world,parent_world;if(!world_transform(g,root,world,e)||(parent&&!world_transform(g,parent,parent_world,e)))return false;

 for(Id id:ids){const auto *source=find(g,id);Node n=*source;if(!allocate(next,n.id,e))return false;
mapping.push_back({id,n.id});if(id==root){n.parent=parent;n.local=relative(parent_world,world);}else{auto at=std::find_if(mapping.begin(),mapping.end(),[&](const auto &p){return p.first==source->parent;});if(at==mapping.end()){e="Clone parent mapping missing";return false;
}n.parent=at->second;}
  if(n.binding.kind!=Kind::None){auto at=std::find_if(bindings.begin(),bindings.end(),[&](const auto &p){return p.first==n.binding;});if(at==bindings.end()||at->second.kind==Kind::None||(at->second.kind!=n.binding.kind&&!(n.binding.kind==Kind::SourceModel&&at->second.kind==Kind::Placement))){e="Cloned objects require explicit compatible new object bindings";return false;
}n.binding=at->second;if(n.binding.kind!=Kind::SourceModel)n.source_key.clear();}
  next.nodes.push_back(std::move(n));
 }
 if(!validate(next,nullptr,e))return false;
g=std::move(next);created=mapping.front().second;out=std::move(mapping);return true;
}
bool encode(const Graph &g,std::string &text,std::string &e){
 if(!validate(g,nullptr,e))return false;
std::ostringstream o;o<<std::setprecision(17)<<"VALWOOD_ENTITIES 4 "<<g.next_id<<' '<<g.nodes.size()<<'\n';for(const auto &n:g.nodes)o<<n.id<<' '<<n.parent<<' '<<std::quoted(n.name)<<' '<<unsigned(n.binding.kind)<<' '<<n.binding.index<<' '<<n.local.x<<' '<<n.local.y<<' '<<n.local.z<<' '<<n.local.yaw<<' '<<n.local.scale<<' '<<std::quoted(n.source_key)<<' '<<n.visible<<' '<<n.locked<<' '<<n.local.pitch<<' '<<n.local.roll<<' '<<n.local.sx<<' '<<n.local.sy<<' '<<n.local.sz<<'\n';text=o.str();return true;
}
bool decode(const std::string &text,Graph &g,std::string &e){
 e.clear();if(text.size()>2*1024*1024){e="Oversized entity metadata";return false;
}std::istringstream in(text);std::string magic,extra;unsigned version=0,count=0;Graph next;
 if(!(in>>magic>>version)||!read_id(in,next.next_id)||!(in>>count)||magic!="VALWOOD_ENTITIES"||(version<1||version>4)||count>node_limit){e="Unsupported entity metadata";return false;
}
 for(unsigned i=0;i<count;i++){Node n;unsigned kind=0;if(!read_id(in,n.id)||!read_id(in,n.parent)||!(in>>std::quoted(n.name)>>kind>>n.binding.index>>n.local.x>>n.local.y>>n.local.z>>n.local.yaw>>n.local.scale)||kind>unsigned(Kind::Navigation)){e="Malformed entity metadata";return false;
}if(version>=2&&!(in>>std::quoted(n.source_key))){e="Missing source key field";return false;}
if(version>=3){int show=0,lock=0;if(!(in>>show>>lock)||show<0||show>1||lock<0||lock>1){e="Invalid entity visibility/lock flags";return false;}n.visible=show;n.locked=lock;}
if(version>=4&&!(in>>n.local.pitch>>n.local.roll>>n.local.sx>>n.local.sy>>n.local.sz)){e="Missing full transform fields";return false;}
n.binding.kind=Kind(kind);next.nodes.push_back(std::move(n));}
 if((in>>extra)||!validate(next,nullptr,e)){if(e.empty())e="Trailing entity metadata";return false;
}g=std::move(next);return true;
}
}
