#include "engine/world_transform_math.h"
#include "engine/world_designer.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace world_designer {
namespace {
using Binding=world_entity::Binding;
using EntityKind=world_entity::Kind;
using Transform=world_entity::Transform;
bool catalogued(const Context &c){return c.source_catalog&&c.source_identities==&c.source_catalog->identities()&&(!c.source_keys||c.source_keys==&c.source_catalog->keys());}
bool named(const std::string &s){return !s.empty()&&s.size()<=96&&s.find_first_of("\r\n\t") == std::string::npos;}
const world_entity::Node *bound(const world_studio::Document &d,Binding b){for(const auto &n:d.entities.nodes)if(n.binding==b&&b.kind!=EntityKind::None)return &n;return nullptr;}
std::string source_key(const Context &c,unsigned index){if(catalogued(c)&&index<c.source_catalog->keys().size())return c.source_catalog->keys()[index];return c.source_identities&&index<c.source_identities->size()?world_authoring::source_key((*c.source_identities)[index]):std::string{};}
bool source_index(const Context &c,const Ref &r,unsigned &out,std::string &e){
    if(!c.source_poses||c.source_poses->size()>8192){e="Source pose table is missing or exceeds 8192 records";return false;}
    out=r.source;
    if(!r.source_key.empty()){
        if(!c.source_identities||c.source_identities->size()!=c.source_poses->size()){e="Source identity table is unavailable";return false;}
        if(catalogued(c)){
            if(!c.source_catalog->unique_index(r.source_key,out)){e="Selected source identity was removed or is ambiguous; select it again";return false;}
        }else{
            unsigned matches=0;for(unsigned i=0;i<c.source_identities->size();++i)if(source_key(c,i)==r.source_key){out=i;++matches;}
            if(matches!=1){e="Selected source identity was removed or is ambiguous; select it again";return false;}
        }
    }
    if(out>=c.source_poses->size()){e="Selected source record no longer exists";return false;}return true;
}
bool inherited_locked(const world_studio::Document &d,Id id,bool self=true){
    const auto *n=world_entity::find(d.entities,id);if(n&&!self)n=n->parent?world_entity::find(d.entities,n->parent):nullptr;
    while(n){if(n->locked)return true;n=n->parent?world_entity::find(d.entities,n->parent):nullptr;}return false;
}
bool child_of(const world_entity::Graph &g,Id id,Id root){for(const auto *n=world_entity::find(g,id);n;n=n->parent?world_entity::find(g,n->parent):nullptr)if(n->id==root)return true;return false;}
bool locked_subtree(const world_studio::Document &d,Id root){if(inherited_locked(d,root))return true;for(const auto &n:d.entities.nodes)if(n.locked&&child_of(d.entities,n.id,root))return true;return false;}
bool effective_visible(const world_studio::Document &d,Id id){for(const auto *n=world_entity::find(d.entities,id);n;n=n->parent?world_entity::find(d.entities,n->parent):nullptr)if(!n->visible)return false;return true;}
bool same(const Transform &a,const Transform &b){return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.yaw==b.yaw&&a.scale==b.scale&&a.pitch==b.pitch&&a.roll==b.roll&&a.sx==b.sx&&a.sy==b.sy&&a.sz==b.sz;}
bool binding_exists(const world_studio::Document &d,const Context &c,Binding b){
    switch(b.kind){
    case EntityKind::None:return b.index==0;
    case EntityKind::SourceModel:return c.source_poses&&c.source_poses->size()<=8192&&b.index<c.source_poses->size();
    case EntityKind::Placement:return b.index<d.placements.size();
    case EntityKind::Light:return b.index<d.lights.size();
    case EntityKind::Actor:return b.index<d.actors.size()&&!d.actors[b.index].points.empty();
    case EntityKind::Particle:return b.index<d.particles.size();
    case EntityKind::Water:return b.index<d.waters.size();
    case EntityKind::Navigation:return b.index<d.navigation.size();
    }return false;
}
bool source_pose(const world_studio::Document &d,const Context &c,unsigned index,world_studio::Pose &out,std::string &e){
    if(!c.source_poses||index>=c.source_poses->size()||!world_studio::effective_pose(d,*c.source_poses,index,out)){e="Selected source pose is missing";return false;}
    if(c.ground.height){const auto &base=(*c.source_poses)[index];out.root.y+=c.ground.height(out.root.x,out.root.z,c.ground.context)-c.ground.height(base.root.x,base.root.z,c.ground.context);}
    if(!std::isfinite(out.root.y)){e="Source ground support is invalid";return false;}return true;
}
bool ensure_entity(world_studio::Document &d,const Context &c,Ref ref,Id &id,std::string &e){
    Ref normalized;Binding b;if(!resolve(d,c,ref,normalized,b,e))return false;
    if(normalized.kind==Kind::Entity){id=normalized.id;return true;}
    world_studio::Pose p;if(!source_pose(d,c,b.index,p,e))return false;
    if(!world_entity::add(d.entities,0,p.alias,b,{p.root.x,p.root.y,p.root.z,p.yaw*57.29577951308232,p.scale,p.pitch*57.29577951308232,p.roll*57.29577951308232,p.sx,p.sy,p.sz},id,e))return false;
    if(c.source_identities&&b.index<c.source_identities->size())world_entity::find(d.entities,id)->source_key=source_key(c,b.index);
    d.source_signature=c.source_signature;return true;
}
bool hide_source(world_studio::Document &d,const Context &c,unsigned index,bool &changed,std::string &e){
    for(auto &o:d.overrides)if(o.index==index){if(!o.hidden){o.hidden=true;changed=true;}return true;}
    if(d.overrides.size()>=512){e="Source override capacity is 512; the source was preserved";return false;}
    world_studio::Pose p;if(!source_pose(d,c,index,p,e))return false;
    d.overrides.push_back({index,p.root.x,p.root.z,float(p.yaw*57.29577951308232),float(p.scale),true,p.offset_y});vw_transform::extra(d.overrides.back(),vw_transform::pose(p));d.source_signature=c.source_signature;changed=true;return true;
}
void retain_child_visibility(world_studio::Document &d,Id id){const auto *n=world_entity::find(d.entities,id);if(!n||n->visible)return;for(auto &child:d.entities.nodes)if(child.parent==id)child.visible=false;}
bool remove_group(world_studio::Document &d,Id id,std::string &e){
    const auto *n=world_entity::find(d.entities,id);if(!n)return true;const Id parent=n->parent;
    retain_child_visibility(d,id);std::vector<Id> children;for(const auto &child:d.entities.nodes)if(child.parent==id)children.push_back(child.id);
    for(Id child:children)if(!world_entity::reparent(d.entities,child,parent,true,e))return false;
    auto &nodes=d.entities.nodes;nodes.erase(std::remove_if(nodes.begin(),nodes.end(),[&](const auto &node){return node.id==id;}),nodes.end());return true;
}
std::vector<Ref> subjects(const Selection &selection,const Command &command){return !command.subject.empty()?std::vector<Ref>{command.subject}:selection.items;}
bool append_unique(std::vector<Binding> &v,Binding b){if(std::find(v.begin(),v.end(),b)!=v.end())return false;v.push_back(b);return true;}
bool write_source_pose(world_studio::Document &d,const Context &context,unsigned index,const Transform &pose,std::string &error){
    if(!context.source_poses||index>=context.source_poses->size()){error="Source transform pose is missing";return false;}
    const auto &base=(*context.source_poses)[index];double ground_delta=0;
    if(context.ground.height)ground_delta=context.ground.height(pose.x,pose.z,context.ground.context)-context.ground.height(base.root.x,base.root.z,context.ground.context);
    const double offset=base.offset_y+pose.y-base.root.y-ground_delta;
    auto at=std::find_if(d.overrides.begin(),d.overrides.end(),[&](const auto &o){return o.index==index;});
    if(at==d.overrides.end()){if(d.overrides.size()>=512){error="Transform exceeds 512 source overrides";return false;}d.overrides.push_back({index,pose.x,pose.z,float(pose.yaw),float(pose.scale),false,float(offset)});}
    else{at->x=pose.x;at->z=pose.z;at->yaw=float(pose.yaw);at->scale=float(pose.scale);at->offset_y=float(offset);}
    auto &extra=*std::find_if(d.overrides.begin(),d.overrides.end(),[&](const auto &o){return o.index==index;});vw_transform::extra(extra,pose);
    d.source_signature=context.source_signature;return true;
}
void preserve_captured_metadata(const world_studio::Document &d,const world_prefab::Selection &selection,Id source_root,world_prefab::Prefab &prefab){
    if(source_root)world_entity::find(prefab.graph,prefab.root)->visible=effective_visible(d,source_root);
    std::array<unsigned,8> counts={};std::vector<unsigned> navigation;
    auto nav=[&](unsigned index){auto at=std::find(navigation.begin(),navigation.end(),index);if(at==navigation.end()){navigation.push_back(index);return unsigned(navigation.size()-1);}return unsigned(at-navigation.begin());};
    for(auto binding:selection.objects){
        if(binding.kind==EntityKind::Actor&&d.actors[binding.index].navigation>=0)nav(unsigned(d.actors[binding.index].navigation));
        Binding captured=binding;
        if(binding.kind==EntityKind::SourceModel)captured.kind=EntityKind::Placement;
        captured.index=binding.kind==EntityKind::Navigation?nav(binding.index):counts[unsigned(captured.kind)]++;
        for(auto &node:prefab.graph.nodes)if(node.binding==captured){
            if(const auto *original=bound(d,binding)){
                node.name=original->name;node.visible=true;
                for(const auto *ancestor=original;ancestor&&ancestor->id!=source_root;ancestor=ancestor->parent?world_entity::find(d.entities,ancestor->parent):nullptr)node.visible&=ancestor->visible;
            }
            break;
        }
    }
}
bool transform_group(world_studio::Document &d,const Context &context,Id root,const Transform &target,std::string &error){
    struct SourceNode {Id id;Binding binding;std::string key;};std::vector<SourceNode> sources;
    for(auto &node:d.entities.nodes)if(node.binding.kind==EntityKind::SourceModel&&child_of(d.entities,node.id,root)){sources.push_back({node.id,node.binding,node.source_key});node.binding={};node.source_key.clear();}
    // Source records cannot be copied or erased from the base scene. Hide their
    // binding only inside this independent candidate while the connected API
    // transforms every world pose and typed authored attachment.
    if(!world_prefab::transform_instance(d,d.entities,root,target,error,context.ground))return false;
    for(const auto &source:sources){
        auto *node=world_entity::find(d.entities,source.id);node->binding=source.binding;node->source_key=source.key;
    }
    // Restoring bindings changes metadata, not poses. Validate and compose the
    // restored graph once instead of revalidating every node for each source.
    // Keep the single-source path allocation-light for the common small bundle.
    if(sources.size()==1){Transform pose;if(!world_entity::world_transform(d.entities,sources[0].id,pose,error)||!write_source_pose(d,context,sources[0].binding.index,pose,error))return false;}
    else if(!sources.empty()){
        std::vector<std::pair<Id,Transform>> worlds;if(!world_entity::world_transforms(d.entities,worlds,error))return false;
        std::unordered_map<Id,const Transform *> by_id;by_id.reserve(worlds.size());for(const auto &world:worlds)by_id.emplace(world.first,&world.second);
        for(const auto &source:sources){const auto at=by_id.find(source.id);if(at==by_id.end()){error="Transformed source world pose is missing";return false;}if(!write_source_pose(d,context,source.binding.index,*at->second,error))return false;}
    }
    if(!sources.empty())d.source_signature=context.source_signature;
    return true;
}
bool transform_bound(world_studio::Document &d,const Context &context,Id id,const Transform &previous,const Transform &target,std::string &error){
    if(locked_subtree(d,id)){error="Transformed entity, ancestor or attachment is locked";return false;}
    if(!world_entity::valid_transform(target)){error="Transform exceeds finite position, rotation or positive axis-scale limits";return false;}
    const auto *node=world_entity::find(d.entities,id);const Binding binding=node->binding;const Id parent=node->parent;
    if(binding.kind==EntityKind::Actor&&target.y!=previous.y){const double ground=context.ground.height?context.ground.height(target.x,target.z,context.ground.context):0;if(std::abs(target.y-ground)>1e-6){error="NPC feet follow the terrain; edit their ground position or route instead of a floating height";return false;}}
    if(binding.kind==EntityKind::Navigation&&target.y!=previous.y){error="Navigation areas are ground projections and have no independent height";return false;}
    const bool isolated=std::none_of(d.entities.nodes.begin(),d.entities.nodes.end(),[&](const auto &child){return child.parent==id;});
    if(isolated&&(binding.kind==EntityKind::Placement||binding.kind==EntityKind::SourceModel)){
        // A leaf model has no typed attachments to move. Updating the same
        // authored fields directly avoids constructing and repeatedly validating
        // an entire temporary prefab graph for each numeric drag sample.
        if(binding.kind==EntityKind::SourceModel){if(!write_source_pose(d,context,binding.index,target,error))return false;}
        else{auto &p=d.placements[binding.index];const double ratio=target.scale/previous.scale;const double ground=context.ground.height?context.ground.height(target.x,target.z,context.ground.context):0;p.x=target.x;p.z=target.z;p.yaw=float(target.yaw);p.scale=float(target.scale);vw_transform::extra(p,target);p.burial=float(p.burial*ratio);p.elevation=float(p.elevation*ratio);p.offset_y=float(target.y-ground+p.burial-p.elevation);}
        // This graph belongs to execute's private, already validated candidate.
        // Resolve a parent once, then leave the final whole-document validation
        // to execute before it publishes anything. No second graph copy needed.
        Transform parent_pose;if(parent&&!world_entity::world_transform(d.entities,parent,parent_pose,error))return false;
        world_entity::find(d.entities,id)->local=world_entity::relative(parent_pose,target);
        return true;
    }
    if(d.entities.nodes.size()==world_entity::node_limit){error="Connected transform needs one temporary authoring slot; the 1024-entity graph is full";return false;}
    if(!world_entity::synchronize(d.entities,{{binding,previous}},error))return false;
    Id temporary=0;if(!world_entity::add(d.entities,0,"Designer transform transaction",{},previous,temporary,error)||!world_entity::reparent(d.entities,id,temporary,true,error))return false;
    if(!transform_group(d,context,temporary,target,error)||!world_entity::reparent(d.entities,id,parent,true,error))return false;
    return remove_group(d,temporary,error);
}
}
SourceCatalog::SourceCatalog(std::vector<world_authoring::Source> sources):identities_(std::move(sources)){
    keys_.reserve(identities_.size());indices_.reserve(identities_.size());
    for(unsigned i=0;i<identities_.size();++i){keys_.push_back(world_authoring::source_key(identities_[i]));auto inserted=indices_.emplace(keys_.back(),i);if(!inserted.second)inserted.first->second=std::numeric_limits<unsigned>::max();}
}
bool SourceCatalog::unique_index(const std::string &key,unsigned &index)const{const auto at=indices_.find(key);if(at==indices_.end()||at->second==std::numeric_limits<unsigned>::max())return false;index=at->second;return true;}
Ref Ref::entity(Id value){Ref r;if(value){r.kind=Kind::Entity;r.id=value;}return r;}
Ref Ref::source_model(unsigned value){Ref r;r.kind=Kind::Source;r.source=value;return r;}
bool operator==(const Ref &a,const Ref &b){if(a.kind!=b.kind)return false;if(a.kind==Kind::Entity)return a.id==b.id;if(a.kind==Kind::Source)return !a.source_key.empty()&&!b.source_key.empty()?a.source_key==b.source_key:a.source==b.source;return true;}
bool operator!=(const Ref &a,const Ref &b){return !(a==b);}
void Selection::clear(){items.clear();active={};}
Ref from_binding(const world_studio::Document &d,Binding b){if(const auto *n=bound(d,b))return Ref::entity(n->id);return b.kind==EntityKind::SourceModel?Ref::source_model(b.index):Ref{};}
bool resolve(const world_studio::Document &d,const Context &c,const Ref &r,Ref &out,Binding &binding,std::string &e){
    if(r.kind==Kind::Entity){const auto *n=world_entity::find(d.entities,r.id);if(!n){e="Selected persistent entity no longer exists";return false;}if(!binding_exists(d,c,n->binding)){e="Selected entity has a missing or invalid scene binding";return false;}out=r;binding=n->binding;e.clear();return true;}
    if(r.kind!=Kind::Source){e="Select an entity first";return false;}
    unsigned index=0;if(!source_index(c,r,index,e))return false;binding={EntityKind::SourceModel,index};
    if(const auto *n=bound(d,binding))out=Ref::entity(n->id);
    else{out=Ref::source_model(index);if(c.source_identities&&index<c.source_identities->size())out.source_key=source_key(c,index);}
    e.clear();return true;
}
bool select(const world_studio::Document &d,const Context &c,Selection &s,Ref ref,bool toggle,std::string &e){
    Ref resolved;Binding b;if(!resolve(d,c,ref,resolved,b,e))return false;
    Selection next=s;reconcile(d,c,next);if(!toggle)next.clear();auto at=std::find(next.items.begin(),next.items.end(),resolved);
    if(toggle&&at!=next.items.end()){next.items.erase(at);next.active=next.items.empty()?Ref{}:next.items.back();}
    else{if(next.items.size()>=128){e="Designer selection capacity is 128 entities";return false;}next.items.push_back(resolved);next.active=resolved;}
    s=std::move(next);e.clear();return true;
}
void reconcile(const world_studio::Document &d,const Context &c,Selection &s){
    Selection next;std::string ignored;Binding b;Ref resolved;
    for(const auto &ref:s.items)if(resolve(d,c,ref,resolved,b,ignored)&&std::find(next.items.begin(),next.items.end(),resolved)==next.items.end())next.items.push_back(resolved);
    if(resolve(d,c,s.active,resolved,b,ignored)&&std::find(next.items.begin(),next.items.end(),resolved)!=next.items.end())next.active=resolved;else if(!next.items.empty())next.active=next.items.back();s=std::move(next);
}
LegacySelection project(const world_studio::Document &d,const Context &c,const Selection &s){
    LegacySelection out;std::string ignored;Ref r;Binding b;
    for(const auto &item:s.items)if(resolve(d,c,item,r,b,ignored)){if(b.kind==EntityKind::SourceModel)out.keys.push_back(b.index);else if(b.kind==EntityKind::Placement)out.keys.push_back(8192+b.index);}
    if(!resolve(d,c,s.active,r,b,ignored))return out;
    if(r.kind==Kind::Entity)out.entity=r.id;
    switch(b.kind){case EntityKind::SourceModel:out.selected=int(b.index);out.tool=2;break;case EntityKind::Placement:out.selected_added=int(b.index);out.tool=2;break;case EntityKind::Light:out.light=int(b.index);out.tool=3;break;case EntityKind::Actor:out.actor=int(b.index);out.tool=10;break;case EntityKind::Navigation:out.area=int(b.index);out.tool=10;break;case EntityKind::Particle:out.particle=int(b.index);out.tool=11;break;case EntityKind::Water:out.water=int(b.index);out.tool=12;break;case EntityKind::None:break;}return out;
}
bool world_pose(const world_studio::Document &d,const Context &c,Ref ref,Transform &out,std::string &e){
    Ref r;Binding b;if(!resolve(d,c,ref,r,b,e))return false;
    if(r.kind==Kind::Entity&&!world_entity::world_transform(d.entities,r.id,out,e))return false;
    auto ground=[&](double x,double z){return c.ground.height?c.ground.height(x,z,c.ground.context):0.;};
    switch(b.kind){
    case EntityKind::SourceModel:{world_studio::Pose p;if(!source_pose(d,c,b.index,p,e))return false;out={p.root.x,p.root.y,p.root.z,p.yaw*57.29577951308232,p.scale,p.pitch*57.29577951308232,p.roll*57.29577951308232,p.sx,p.sy,p.sz};break;}
    case EntityKind::Placement:{const auto &p=d.placements[b.index];out={p.x,ground(p.x,p.z)+p.offset_y+p.elevation-p.burial,p.z,p.yaw,p.scale,p.pitch,p.roll,p.sx,p.sy,p.sz};break;}
    case EntityKind::Light:{const auto &p=d.lights[b.index];out.x=p.x;out.y=p.y;out.z=p.z;out.yaw=p.yaw;break;}
    case EntityKind::Actor:{const auto &p=d.actors[b.index];out.x=p.points[0][0];out.z=p.points[0][1];out.y=ground(out.x,out.z);out.yaw=p.yaw;break;}
    case EntityKind::Particle:{const auto &p=d.particles[b.index];out.x=p.x;out.y=p.y;out.z=p.z;break;}
    case EntityKind::Water:{const auto &p=d.waters[b.index];out.x=p.x;out.y=p.level;out.z=p.z;break;}
    case EntityKind::Navigation:{const auto &p=d.navigation[b.index];out.x=p.x;out.z=p.z;break;}
    case EntityKind::None:break;
    }
    if(!world_entity::valid_transform(out)){e="Selected component has invalid ground support or transform";return false;}return true;
}
std::string name(const world_studio::Document &d,const Context &c,Ref ref){Ref r;Binding b;std::string ignored;if(!resolve(d,c,ref,r,b,ignored))return "Missing entity";if(r.kind==Kind::Entity)return world_entity::find(d.entities,r.id)->name;return (*c.source_poses)[b.index].alias;}
static bool execute_impl(const world_studio::Document &input,const Selection &selection,const Context &context,const Command &command,world_studio::Document &output,Result &result,std::string &error,bool input_validated,Id issued_floor=0){
    if(!input_validated&&!world_studio::valid(input,error))return false;
    auto next=input;Result accepted;accepted.selection=selection;reconcile(next,context,accepted.selection);
    const Ref subject=command.subject.empty()?accepted.selection.active:command.subject;
    if(command.parent&&(!world_entity::find(next.entities,command.parent)||inherited_locked(next,command.parent))){error="Destination parent is missing or locked";return false;}
    if(command.operation==Operation::CreateGroup){
        if(!named(command.name)){error="Group name must contain 1–96 characters without line breaks";return false;}
        Transform parent;if(command.parent&&!world_entity::world_transform(next.entities,command.parent,parent,error))return false;
        if(!world_entity::add(next.entities,command.parent,command.name,{},world_entity::relative(parent,command.transform),accepted.created,error))return false;
        accepted.changed=true;accepted.selection.items={Ref::entity(accepted.created)};accepted.selection.active=accepted.selection.items.back();
    }else if(command.operation==Operation::Duplicate){
        const auto chosen=subjects(accepted.selection,command);if(chosen.empty()){error="Select an entity to duplicate";return false;}
        world_prefab::Selection capture;capture.ground=context.ground;std::vector<Id> roots;
        for(const auto &item:chosen){Ref ref;Binding binding;if(!resolve(next,context,item,ref,binding,error))return false;
            if(ref.kind==Kind::Entity){if(locked_subtree(next,ref.id)){error="Selected entity, ancestor or attachment is locked";return false;}roots.push_back(ref.id);for(const auto &node:next.entities.nodes)if(node.binding.kind!=EntityKind::None&&child_of(next.entities,node.id,ref.id))append_unique(capture.objects,node.binding);}
            else append_unique(capture.objects,binding);
        }
        Transform origin;if(!world_pose(next,context,subject,origin,error))return false;Transform destination=origin;destination.x+=command.offset.x;destination.y+=command.offset.y;destination.z+=command.offset.z;
        if(!world_entity::valid_transform(destination)){error="Duplicate destination exceeds transform limits";return false;}
        const auto copy_name=(command.name.empty()?name(next,context,subject)+" copy":command.name).substr(0,64);
        if(capture.objects.empty()){
            if(roots.size()!=1){error="Duplicate empty groups individually";return false;}std::vector<std::pair<Id,Id>> ids;
            if(!world_entity::clone_subtree(next.entities,roots[0],command.parent,{},accepted.created,ids,error)||!world_entity::set_world(next.entities,accepted.created,destination,error))return false;
            world_entity::find(next.entities,accepted.created)->name=copy_name;
        }else{
            for(auto b:capture.objects)if(b.kind==EntityKind::SourceModel){world_studio::Pose p;if(!source_pose(next,context,b.index,p,error))return false;world_prefab::SourceModel source;source.index=b.index;source.root_y=p.root.y;source.hidden=p.hidden;source.placement={p.alias,p.root.x,p.root.z,float(p.yaw*57.29577951308232),float(p.scale),p.burial,p.elevation,p.offset_y,float(p.pitch*57.29577951308232),float(p.roll*57.29577951308232),float(p.sx),float(p.sy),float(p.sz)};capture.source_models.push_back(std::move(source));}
            world_prefab::Prefab prefab;world_prefab::Instance instance;if(!world_prefab::capture(next,capture,origin,copy_name,prefab,error))return false;
            if(next.placements.size()+prefab.content.placements.size()>world_studio::placement_limit||next.lights.size()+prefab.content.lights.size()>24||next.actors.size()+prefab.content.actors.size()>32||next.particles.size()+prefab.content.particles.size()>16||next.waters.size()+prefab.content.waters.size()>8||next.navigation.size()+prefab.content.navigation.size()>4){error="Connected duplicate exceeds an authored capacity: 1024 models, 24 lights, 32 actors, 16 emitters, 8 water planes or 4 navigation areas";return false;}
            if(next.entities.nodes.size()+prefab.graph.nodes.size()>world_entity::node_limit){error="Connected duplicate exceeds 1024 persistent entities";return false;}
            const Id metadata_root=roots.size()==1&&world_entity::find(next.entities,roots[0])->binding.kind==EntityKind::None?roots[0]:0;
            preserve_captured_metadata(next,capture,metadata_root,prefab);
            if(!world_prefab::instantiate(prefab,next,next.entities,destination,command.parent,instance,error,context.ground))return false;
            for(const auto &mapping:instance.ids){const auto *source=world_entity::find(prefab.graph,mapping.first);auto *copy=world_entity::find(next.entities,mapping.second);copy->visible=source->visible;copy->locked=source->locked;}
            accepted.created=instance.root;
        }
        accepted.changed=accepted.geometry=true;accepted.selection.items={Ref::entity(accepted.created)};accepted.selection.active=accepted.selection.items.back();
    }else if(command.operation==Operation::Erase){
        const auto chosen=subjects(accepted.selection,command);if(chosen.empty()){error="Select an entity to delete";return false;}
        std::vector<Id> ids;std::vector<unsigned> sources;
        for(const auto &item:chosen){Ref ref;Binding binding;if(!resolve(next,context,item,ref,binding,error))return false;
            if(ref.kind==Kind::Source){sources.push_back(binding.index);continue;}
            if(inherited_locked(next,ref.id)){error="Selected entity or its parent is locked";return false;}
            for(const auto &node:next.entities.nodes)if(node.id==ref.id||(command.children==world_entity::ChildPolicy::EraseSubtree&&child_of(next.entities,node.id,ref.id)))if(std::find(ids.begin(),ids.end(),node.id)==ids.end())ids.push_back(node.id);
        }
        for(Id id:ids){const auto *node=world_entity::find(next.entities,id);if(node->locked){error="Deletion contains a locked attachment";return false;}if(std::find(context.protected_entities.begin(),context.protected_entities.end(),id)!=context.protected_entities.end()){error="Entity is referenced by active live components; remove its binding explicitly before deletion";return false;}}
        std::vector<Binding> records;std::vector<Id> groups;
        for(Id id:ids){const auto *node=world_entity::find(next.entities,id);if(node->binding.kind==EntityKind::SourceModel)sources.push_back(node->binding.index);else if(node->binding.kind==EntityKind::None)groups.push_back(id);else records.push_back(node->binding);}
        for(unsigned source:sources)if(!hide_source(next,context,source,accepted.changed,error))return false;
        std::sort(records.begin(),records.end(),[](Binding a,Binding b){return a.kind==b.kind?a.index>b.index:unsigned(a.kind)<unsigned(b.kind);});
        for(auto binding:records){if(const auto *node=bound(next,binding))retain_child_visibility(next,node->id);if(!world_authoring::erase_record(next,binding,error))return false;accepted.changed=true;}
        // Children that were retained (including hidden base sources) are
        // promoted with world poses and inherited hidden state intact.
        for(Id id:groups)if(world_entity::find(next.entities,id)){if(!remove_group(next,id,error))return false;accepted.changed=true;}
        accepted.geometry=accepted.changed;accepted.selection.clear();
    }else{
        Ref resolved;Binding binding;if(!resolve(next,context,subject,resolved,binding,error))return false;
        const bool ancestor_locked=resolved.kind==Kind::Entity&&inherited_locked(next,resolved.id,false);
        const auto *original=resolved.kind==Kind::Entity?world_entity::find(next.entities,resolved.id):nullptr;
        const bool self_locked=original&&original->locked;
        if(ancestor_locked||(self_locked&&!(command.operation==Operation::SetFlags&&command.change_locked&&!command.locked&&!command.change_visible))){error="Selected entity or its parent is locked";return false;}
        if(command.operation==Operation::Rename){
            if(!named(command.name)){error="Entity name must contain 1–96 characters without line breaks";return false;}
            if(name(next,context,subject)!=command.name){Id id=0;if(!ensure_entity(next,context,subject,id,error))return false;auto *node=world_entity::find(next.entities,id);node->name=command.name;if(node->binding.kind==EntityKind::Actor)next.actors[node->binding.index].name=command.name;accepted.changed=true;accepted.selection.items={Ref::entity(id)};accepted.selection.active=accepted.selection.items.back();}
        }else if(command.operation==Operation::SetFlags){
            bool hidden_override=false;if(binding.kind==EntityKind::SourceModel)for(const auto &o:next.overrides)if(o.index==binding.index)hidden_override=o.hidden;
            const bool changed_visible=command.change_visible&&((original?original->visible:true)!=command.visible||(command.visible&&hidden_override));
            const bool changed_locked=command.change_locked&&((original?original->locked:false)!=command.locked);
            if(changed_visible||changed_locked){Id id=0;if(!ensure_entity(next,context,subject,id,error))return false;auto *node=world_entity::find(next.entities,id);if(command.change_visible)node->visible=command.visible;if(command.change_locked)node->locked=command.locked;if(command.change_visible&&command.visible&&binding.kind==EntityKind::SourceModel)for(auto &o:next.overrides)if(o.index==binding.index)o.hidden=false;accepted.changed=true;accepted.geometry=changed_visible;accepted.selection.items={Ref::entity(id)};accepted.selection.active=accepted.selection.items.back();}
        }else if(command.operation==Operation::Reparent){
            if(resolved.kind!=Kind::Source||command.parent){
                Id id=0;if(!ensure_entity(next,context,subject,id,error))return false;const auto *node=world_entity::find(next.entities,id);
                if(node->parent!=command.parent){if(locked_subtree(next,id)){error="Reparented entity contains a locked attachment";return false;}if(!world_entity::reparent(next.entities,id,command.parent,true,error))return false;accepted.changed=true;accepted.selection.items={Ref::entity(id)};accepted.selection.active=accepted.selection.items.back();}
            }
        }else if(command.operation==Operation::Transform){
            Transform previous;if(!world_pose(next,context,subject,previous,error))return false;
            if(!same(previous,command.transform)){
                Id id=0;if(!ensure_entity(next,context,subject,id,error))return false;
                const auto *entity=world_entity::find(next.entities,id);
                if(entity->binding.kind==EntityKind::None){if(!transform_group(next,context,id,command.transform,error))return false;}
                else if(!transform_bound(next,context,id,previous,command.transform,error))return false;
                accepted.changed=accepted.geometry=true;accepted.selection.items={Ref::entity(id)};accepted.selection.active=accepted.selection.items.back();
            }
        }else{error="Unsupported designer command";return false;}
    }
    // Gesture previews can expose a freshly registered source ID before the
    // final sample returns to its unchanged baseline. Retain that issued ID
    // floor even for a no-op final sample, without allocating a new ID for
    // every intermediate sample of the same baseline transaction.
    next.entities.next_id=std::max(next.entities.next_id,issued_floor);
    if(!world_studio::valid(next,error))return false;
    if(accepted.changed&&context.validate_candidate&&!context.validate_candidate(next,context.validation_context,error))return false;
    reconcile(next,context,accepted.selection);
    output=std::move(next);result=std::move(accepted);error.clear();return true;
}
bool execute(const world_studio::Document &input,const Selection &selection,const Context &context,const Command &command,world_studio::Document &output,Result &result,std::string &error){return execute_impl(input,selection,context,command,output,result,error,false);}
bool snap_to_terrain(const world_studio::Document &input,const Selection &selected,const Context &context,ModelBounds bounds,world_studio::Document &output,Result &result,std::string &error){
    if(!world_studio::valid(input,error))return false;
    Selection selection=selected;reconcile(input,context,selection);
    if(selection.items.empty()){error="Select a model to snap to terrain.";return false;}
    std::vector<Ref> roots;
    for(const auto &ref:selection.items){
        bool nested=false;
        if(ref.kind==Kind::Entity)for(const auto &other:selection.items)if(other.kind==Kind::Entity&&other.id!=ref.id&&child_of(input.entities,ref.id,other.id)){nested=true;break;}
        if(!nested)roots.push_back(ref);
    }
    auto next=input;Result accepted;accepted.selection=selection;
    auto ground=[&](double x,double z){return context.ground.height?context.ground.height(x,z,context.ground.context):0.;};
    for(const auto &root:roots){
        Ref resolved;Binding binding;if(!resolve(next,context,root,resolved,binding,error))return false;
        if(resolved.kind==Kind::Entity&&locked_subtree(next,resolved.id)){error="Selected entity, ancestor or attachment is locked.";return false;}
        std::vector<Binding> models;
        auto include=[&](Binding b){
            if(b.kind==EntityKind::SourceModel||b.kind==EntityKind::Placement){append_unique(models,b);return true;}
            if(b.kind==EntityKind::None||b.kind==EntityKind::Light||b.kind==EntityKind::Particle)return true;
            error="Snap models separately from NPC, navigation or water attachments.";return false;
        };
        if(resolved.kind==Kind::Source){if(!include(binding))return false;}
        else for(const auto &node:next.entities.nodes)if(child_of(next.entities,node.id,resolved.id)&&!include(node.binding))return false;
        if(models.empty()){error="Select a model or a group containing models to snap to terrain.";return false;}
        double delta=-std::numeric_limits<double>::infinity();
        for(const auto &model:models){
            const auto ref=from_binding(next,model);Transform pose;if(!world_pose(next,context,ref,pose,error))return false;
            std::string alias;double burial=0,elevation=0,offset=0;
            if(model.kind==EntityKind::SourceModel){world_studio::Pose source;if(!source_pose(next,context,model.index,source,error))return false;alias=source.alias;burial=source.burial;elevation=source.elevation;offset=source.offset_y;}
            else{const auto &p=next.placements[model.index];alias=p.alias;burial=p.burial;elevation=p.elevation;offset=p.offset_y;}
            world_studio::Vec lo,hi;
            if(!bounds.get||!bounds.get(alias,lo,hi,bounds.context)){error="Inspect "+alias+" to load its bounds before snapping to terrain.";return false;}
            if(!std::isfinite(lo.x)||!std::isfinite(lo.y)||!std::isfinite(lo.z)||!std::isfinite(hi.x)||!std::isfinite(hi.y)||!std::isfinite(hi.z)||lo.x>hi.x||lo.y>hi.y||lo.z>hi.z){error="Model bounds are invalid.";return false;}
            auto shape=pose;shape.x=shape.y=shape.z=0;const auto matrix=vw_transform::matrix(shape);
            double bottom=1e30,support=-1e30;
            for(unsigned corner=0;corner<8;++corner){const auto p=vw_transform::vector(matrix,{corner&1?hi.x:lo.x,corner&2?hi.y:lo.y,corner&4?hi.z:lo.z});bottom=std::min(bottom,p[1]);}
            // Sample both projected faces, so tilted and nonuniformly scaled
            // bounds use their complete footprint. Keep the object's rotation;
            // a rigid model rests above the highest sampled terrain support.
            for(unsigned face=0;face<2;++face)for(unsigned z=0;z<=4;++z)for(unsigned x=0;x<=4;++x){
                const auto p=vw_transform::vector(matrix,{lo.x+(hi.x-lo.x)*x*.25,face?hi.y:lo.y,lo.z+(hi.z-lo.z)*z*.25});
                const double height=ground(pose.x+p[0],pose.z+p[2]);
                if(!std::isfinite(height)){error="Terrain below the model is invalid.";return false;}
                support=std::max(support,height);
            }
            double actual=pose.y;
            if(vw_transform::simple(pose)){
                double neutral=ground(pose.x,pose.z)-burial+elevation;
                if(bounds.grounded_pivot&&!bounds.grounded_pivot(alias,pose,burial,elevation,context.ground,neutral,bounds.context)){error="Could not resolve terrain support for "+alias+".";return false;}
                actual=neutral+offset;
            }
            if(!std::isfinite(actual)){error="Model terrain support is invalid.";return false;}
            delta=std::max(delta,support-bottom-actual);
        }
        if(std::abs(delta)<=.0001)continue;
        Command command;command.operation=Operation::Transform;command.subject=resolved;
        if(!world_pose(next,context,resolved,command.transform,error))return false;
        command.transform.y+=delta;world_studio::Document candidate;Result changed;
        if(!execute_impl(next,selection,context,command,candidate,changed,error,false))return false;
        accepted.changed|=changed.changed;accepted.geometry|=changed.geometry;next=std::move(candidate);
    }
    reconcile(next,context,accepted.selection);output=std::move(next);result=std::move(accepted);error.clear();return true;
}
bool Clipboard::copy(const world_studio::Document &d,const Selection &selected,const Context &context,ModelBounds bounds,std::string &error){
    if(!world_studio::valid(d,error))return false;
    Selection selection=selected;reconcile(d,context,selection);
    if(selection.items.empty()){error="Select an object to copy.";return false;}
    Clipboard next;world_prefab::Selection capture;capture.ground=context.ground;std::vector<Id> roots;
    for(const auto &item:selection.items){
        Ref ref;Binding binding;if(!resolve(d,context,item,ref,binding,error))return false;
        if(ref.kind==Kind::Entity){
            if(locked_subtree(d,ref.id)){error="Selected entity, ancestor or attachment is locked";return false;}
            roots.push_back(ref.id);
            for(const auto &node:d.entities.nodes)if(node.binding.kind!=EntityKind::None&&child_of(d.entities,node.id,ref.id))append_unique(capture.objects,node.binding);
        }else append_unique(capture.objects,binding);
    }
    if(capture.objects.empty()){error="Select an asset or a group containing objects to copy.";return false;}
    for(auto b:capture.objects)if(b.kind==EntityKind::SourceModel){
        world_studio::Pose p;if(!source_pose(d,context,b.index,p,error))return false;
        world_prefab::SourceModel source;source.index=b.index;source.root_y=p.root.y;source.hidden=p.hidden;
        source.placement={p.alias,p.root.x,p.root.z,float(p.yaw*57.29577951308232),float(p.scale),p.burial,p.elevation,p.offset_y,float(p.pitch*57.29577951308232),float(p.roll*57.29577951308232),float(p.sx),float(p.sy),float(p.sz)};
        capture.source_models.push_back(std::move(source));
    }
    Transform origin;if(!world_pose(d,context,selection.active,origin,error))return false;
    const auto label=(name(d,context,selection.active)+" copy").substr(0,64);
    if(!world_prefab::capture(d,capture,origin,label,next.prefab_,error))return false;
    const Id metadata_root=roots.size()==1&&world_entity::find(d.entities,roots[0])->binding.kind==EntityKind::None?roots[0]:0;
    preserve_captured_metadata(d,capture,metadata_root,next.prefab_);
    Ref active;Binding binding;
    next.single_model_=capture.objects.size()==1&&resolve(d,context,selection.active,active,binding,error)&&(binding.kind==EntityKind::SourceModel||binding.kind==EntityKind::Placement);
    next.exact_bounds_=true;
    // Project all eight transformed mesh corners, including roof overhangs,
    // pitch/roll and independent axis scales. No guessed house spacing.
    for(const auto &node:next.prefab_.graph.nodes){
        if(node.binding.kind==EntityKind::None)continue;
        Transform world;if(!world_entity::world_transform(next.prefab_.graph,node.id,world,error))return false;
        world_studio::Vec lo={-.5,-.5,-.5},hi={.5,.5,.5};
        if(node.binding.kind==EntityKind::Placement){
            const auto &p=next.prefab_.content.placements[node.binding.index];
            bool found=bounds.get&&bounds.get(p.alias,lo,hi,bounds.context);
            if(!found&&context.source_poses)for(const auto &source:*context.source_poses)if(source.alias==p.alias){lo=source.minimum;hi=source.maximum;found=true;break;}
            const bool valid=found&&std::isfinite(lo.x)&&std::isfinite(lo.y)&&std::isfinite(lo.z)&&std::isfinite(hi.x)&&std::isfinite(hi.y)&&std::isfinite(hi.z)&&hi.x>lo.x&&hi.y>lo.y&&hi.z>lo.z;
            if(!valid){next.exact_bounds_=false;continue;}
        }else if(node.binding.kind==EntityKind::Water){const auto &p=next.prefab_.content.waters[node.binding.index];lo={-p.radius_x,0,-p.radius_z};hi={p.radius_x,0,p.radius_z};}
        const auto matrix=vw_transform::matrix(world);
        for(unsigned corner=0;corner<8;++corner){const auto p=vw_transform::point(matrix,{corner&1?hi.x:lo.x,corner&2?hi.y:lo.y,corner&4?hi.z:lo.z});next.footprint_.push_back({p[0]-origin.x,p[1]-origin.y,p[2]-origin.z});}
    }
    next.anchors_.push_back(selection.active);*this=std::move(next);error.clear();return true;
}
bool Clipboard::paste(const world_studio::Document &input,const Context &context,const PasteOptions &options,world_studio::Document &output,Result &result,std::string &error){
    if(!ready()){error="Copy an object first (Ctrl+C).";return false;}
    if(options.direction<0||options.direction>3||!std::isfinite(options.gap)||options.gap<0||options.gap>100){error="Paste direction or gap is invalid (0–100 m).";return false;}
    if(options.snap&&!exact_bounds_){error="Model bounds are unavailable. Inspect the asset, copy again, or turn off Snap pasted assets.";return false;}
    if(!world_studio::valid(input,error))return false;
    Transform anchor=prefab_.origin;
    // Locate the latest surviving paste, then the original. This also follows
    // an object's manual move and handles Undo/Redo without clipboard history.
    for(auto at=anchors_.rbegin();at!=anchors_.rend();++at){Transform pose;std::string ignored;if(world_pose(input,context,*at,pose,ignored)){anchor.x=pose.x;anchor.y=pose.y;anchor.z=pose.z;break;}}
    const double yaw=prefab_.origin.yaw*.017453292519943295;
    double dx=std::cos(yaw),dz=-std::sin(yaw);
    if(options.direction>=2){dx=std::sin(yaw);dz=std::cos(yaw);}
    if(options.direction==1||options.direction==3){dx=-dx;dz=-dz;}
    double step=2;
    if(options.snap){
        double lo=std::numeric_limits<double>::infinity(),hi=-lo;
        for(const auto &p:footprint_){const double projection=p.x*dx+p.z*dz;lo=std::min(lo,projection);hi=std::max(hi,projection);}
        step=hi-lo+options.gap;
        if(!std::isfinite(step)||step<=1e-6){error="Copied selection has no usable footprint.";return false;}
    }
    struct Occupied {std::string alias;double x,z,offset;};std::vector<Occupied> occupied;
    auto relevant=[&](const std::string &alias){return std::any_of(prefab_.content.placements.begin(),prefab_.content.placements.end(),[&](const auto &p){return p.alias==alias;});};
    for(unsigned i=0;i<input.placements.size();++i){const auto &p=input.placements[i];if(relevant(p.alias)&&world_entity::visible(input.entities,{EntityKind::Placement,i}))occupied.push_back({p.alias,p.x,p.z,p.elevation+p.offset_y});}
    if(context.source_poses)for(unsigned i=0;i<context.source_poses->size();++i)if(relevant((*context.source_poses)[i].alias)&&world_entity::visible(input.entities,{EntityKind::SourceModel,i})){
        world_studio::Pose p;if(!source_pose(input,context,i,p,error))return false;
        if(!p.hidden)occupied.push_back({p.alias,p.root.x,p.root.z,p.elevation+p.offset_y});
    }
    auto ground=[&](double x,double z){return context.ground.height?context.ground.height(x,z,context.ground.context):0.;};
    Transform target=prefab_.origin;
    // Recopying the original, reversing direction or moving a previous copy
    // must not recreate an occupied root and invalidate the entire Apply.
    for(size_t slot=1;slot<=occupied.size()+1;++slot){
        target.x=anchor.x+dx*step*slot;target.y=anchor.y;target.z=anchor.z+dz*step*slot;
        if(!prefab_.content.placements.empty())target.y+=ground(target.x,target.z)-ground(anchor.x,anchor.z);
        bool overlap=false;
        for(unsigned i=0;i<prefab_.content.placements.size()&&!overlap;++i){const auto &p=prefab_.content.placements[i];const double x=p.x+target.x-prefab_.origin.x,z=p.z+target.z-prefab_.origin.z;
            const double offset=prefab_.model_heights[i]+target.y-prefab_.origin.y-ground(x,z)+p.burial;
            for(const auto &old:occupied)if(old.alias==p.alias&&std::abs(old.offset-offset)<.011&&std::hypot(old.x-x,old.z-z)<.011){overlap=true;break;}
        }
        if(!overlap)break;
        if(slot==occupied.size()+1){error="No free paste position is available along this row.";return false;}
    }
    if(!world_entity::valid_transform(target)){error="Pasted destination exceeds transform limits.";return false;}
    auto next=input;const auto &p=prefab_.content;
    if(next.placements.size()+p.placements.size()>world_studio::placement_limit||next.lights.size()+p.lights.size()>24||next.actors.size()+p.actors.size()>32||next.particles.size()+p.particles.size()>16||next.waters.size()+p.waters.size()>8||next.navigation.size()+p.navigation.size()>4){error="Paste exceeds an authored object capacity.";return false;}
    world_prefab::Instance instance;if(!world_prefab::instantiate(prefab_,next,next.entities,target,0,instance,error,context.ground))return false;
    for(const auto &mapping:instance.ids){const auto *source=world_entity::find(prefab_.graph,mapping.first);auto *copy=world_entity::find(next.entities,mapping.second);copy->visible=source->visible;copy->locked=source->locked;}
    Id selected=instance.root;
    if(single_model_){
        const auto found=std::find_if(next.entities.nodes.begin(),next.entities.nodes.end(),[&](const auto &node){return node.parent==instance.root&&node.binding.kind==EntityKind::Placement;});
        if(found==next.entities.nodes.end()){error="Pasted model binding is missing.";return false;}
        selected=found->id;
        if(!world_entity::reparent(next.entities,selected,0,true,error))return false;
        world_entity::find(next.entities,selected)->name=prefab_.name;
        auto &nodes=next.entities.nodes;nodes.erase(std::remove_if(nodes.begin(),nodes.end(),[&](const auto &node){return node.id==instance.root;}),nodes.end());
    }
    if(!world_studio::valid(next,error)||(context.validate_candidate&&!context.validate_candidate(next,context.validation_context,error)))return false;
    Result accepted;accepted.changed=accepted.geometry=true;accepted.created=selected;accepted.selection.items={Ref::entity(selected)};accepted.selection.active=accepted.selection.items.back();
    // Prune undone pastes before appending: a new branch must not retain an
    // extinct anchor, even if an external caller later reuses an entity ID.
    anchors_.erase(std::remove_if(anchors_.begin(),anchors_.end(),[&](const Ref &r){return r.kind==Kind::Entity&&!world_entity::find(input.entities,r.id);}),anchors_.end());
    anchors_.push_back(accepted.selection.active);output=std::move(next);result=std::move(accepted);error.clear();return true;
}
bool separate_overlapping_models(const world_studio::Document &input,const Context &context,ModelBounds bounds,world_studio::Document &output,Result &result,std::string &error){
    if(!world_studio::valid(input,error))return false;
    auto next=input;Result accepted;
    for(unsigned i=0;i<next.placements.size();++i){
        const auto model=next.placements[i];if(!world_entity::visible(next.entities,{EntityKind::Placement,i}))continue;
        auto overlapping=[&](double x,double z,bool all=false){
            for(unsigned j=0;j<(all?next.placements.size():i);++j){const auto &p=next.placements[j];if(j!=i&&world_entity::visible(next.entities,{EntityKind::Placement,j})&&p.alias==model.alias&&std::abs(p.elevation+p.offset_y-model.elevation-model.offset_y)<.011&&std::hypot(p.x-x,p.z-z)<.011)return true;}
            if(context.source_poses)for(unsigned j=0;j<context.source_poses->size();++j)if((*context.source_poses)[j].alias==model.alias&&world_entity::visible(next.entities,{EntityKind::SourceModel,j})){
                world_studio::Pose p;std::string ignored;if(source_pose(next,context,j,p,ignored)&&!p.hidden&&std::abs(p.elevation+p.offset_y-model.elevation-model.offset_y)<.011&&std::hypot(p.root.x-x,p.root.z-z)<.011)return true;
            }
            return false;
        };
        if(!overlapping(model.x,model.z))continue;
        world_studio::Vec lo,hi;if(!bounds.get||!bounds.get(model.alias,lo,hi,bounds.context)){error="Inspect "+model.alias+" before separating overlapping copies.";return false;}
        Transform pose{model.x,0,model.z,model.yaw,model.scale,model.pitch,model.roll,model.sx,model.sy,model.sz};const auto matrix=vw_transform::matrix(pose);
        const double yaw=model.yaw*.017453292519943295,dx=std::cos(yaw),dz=-std::sin(yaw);double low=1e30,high=-1e30;
        for(unsigned corner=0;corner<8;++corner){const auto p=vw_transform::point(matrix,{corner&1?hi.x:lo.x,corner&2?hi.y:lo.y,corner&4?hi.z:lo.z});const double v=p[0]*dx+p[2]*dz;low=std::min(low,v);high=std::max(high,v);}
        const double stride=high-low+.25;if(!std::isfinite(stride)||stride<=.01){error="Model footprint is invalid.";return false;}
        unsigned slot=1;const auto limit=next.placements.size()+(context.source_poses?context.source_poses->size():0)+1;
        while(slot<=limit&&overlapping(model.x+dx*stride*slot,model.z+dz*stride*slot,true))++slot;
        // Reuse the existing persistent ID, parent and authored bindings.
        if(!bound(next,{EntityKind::Placement,i})){
            const std::vector<world_authoring::Source> empty;
            if(!world_authoring::synchronize(next,next.entities,context.source_identities?*context.source_identities:empty,context.ground.height,context.ground.context,error))return false;
        }
        auto ref=from_binding(next,{EntityKind::Placement,i});Id root=ref.id;
        const auto *node=world_entity::find(next.entities,root);
        if(node->parent){const auto *parent=world_entity::find(next.entities,node->parent);if(parent&&parent->binding.kind==EntityKind::None&&std::count_if(next.entities.nodes.begin(),next.entities.nodes.end(),[&](const auto &n){return n.parent==parent->id;})==1)root=parent->id;}
        if(locked_subtree(next,root)){error="An overlapping copy is locked. Unlock it before separating copies.";return false;}
        Transform before;if(!world_pose(next,context,Ref::entity(root),before,error))return false;auto target=before;target.x+=dx*stride*slot;target.z+=dz*stride*slot;
        if(context.ground.height)target.y+=context.ground.height(target.x,target.z,context.ground.context)-context.ground.height(before.x,before.z,context.ground.context);
        if(world_entity::find(next.entities,root)->binding.kind==EntityKind::None){if(!transform_group(next,context,root,target,error))return false;}
        else if(!transform_bound(next,context,root,before,target,error))return false;
        accepted.changed=accepted.geometry=true;accepted.selection.items={ref};accepted.selection.active=ref;
    }
    if(!world_studio::valid(next,error)||(accepted.changed&&context.validate_candidate&&!context.validate_candidate(next,context.validation_context,error)))return false;
    output=std::move(next);result=std::move(accepted);error.clear();return true;
}
bool EditScope::begin(const world_studio::Document &d,const Selection &s){if(active_)return false;before_=d;selection_=s;issued_=d.entities.next_id;active_=true;changed_=false;baseline_checked_=baseline_valid_=false;baseline_error_.clear();return true;}
bool EditScope::update(const Context &context,const Command &command,world_studio::Document &d,Selection &s,Result &result,std::string &error){
    if(!active_){error="No designer edit gesture is active";return false;}
    // before_ is an owned snapshot, inaccessible to callers throughout this
    // gesture. Validate it once; every produced candidate and host guard still
    // runs on every update, including when Context or the live world changes.
    if(!baseline_checked_){baseline_valid_=world_studio::valid(before_,baseline_error_);baseline_checked_=true;}
    if(!baseline_valid_){error=baseline_error_;return false;}
    world_studio::Document next;Result accepted;if(!execute_impl(before_,selection_,context,command,next,accepted,error,true,issued_))return false;
    issued_=std::max(issued_,next.entities.next_id);changed_=accepted.changed;d=std::move(next);s=accepted.selection;result=std::move(accepted);return true;
}
bool EditScope::commit(world_studio::History &history){if(!active_)return false;if(changed_)history.checkpoint(before_);active_=false;return changed_;}
bool EditScope::cancel(world_studio::Document &d,Selection &s){if(!active_)return false;d=std::move(before_);d.entities.next_id=std::max(d.entities.next_id,issued_);s=std::move(selection_);active_=false;changed_=false;return true;}
}
