#include "engine/world_designer_scene_index.h"
#include <algorithm>
#include <utility>

namespace world_designer_scene {
namespace {
unsigned char lower(unsigned char c){return c>='A'&&c<='Z'?static_cast<unsigned char>(c+32):c;}
bool space(char c){return c==' '||c=='\t'||c=='\n'||c=='\r';}
}
void Index::sync(const world_entity::Graph &graph,const std::vector<std::string> &sources,
                 const std::string &generation,std::uint64_t revision,Labeler labeler){
    const bool registry=!initialized_||generation_!=generation||source_names_.size()!=sources.size();
    if(initialized_&&revision_==revision&&!registry)return;
    bool topology=!initialized_||metadata_.size()!=graph.nodes.size(),content=topology,bindings=topology,flags=topology;
    if(!topology)for(std::size_t i=0;i<metadata_.size();++i){
        const auto &old=metadata_[i];const auto &next=graph.nodes[i];++counters_.metadata_compared;
        topology|=old.id!=next.id||old.parent!=next.parent;
        bindings|=old.binding!=next.binding;
        content|=old.name!=next.name;flags|=old.visible!=next.visible||old.locked!=next.locked;
    }
    if(topology||bindings||content||flags){
        metadata_.resize(graph.nodes.size());
        for(std::size_t i=0;i<graph.nodes.size();++i){const auto &n=graph.nodes[i];metadata_[i]={n.id,n.parent,n.binding,n.name,n.visible,n.locked};}
        ++counters_.content_refreshes;
    }
    if(registry){
        generation_=generation;source_names_=sources;source_labels_.clear();source_labels_.reserve(sources.size());
        for(const auto &name:sources)source_labels_.push_back(labeler?labeler(name):name);
        ++counters_.source_registry_rebuilds;source_filter_dirty_=true;
    }
    if(topology)rebuild_topology();
    else if(flags)rebuild_flags();
    if(registry||bindings)rebuild_registered();
    rows_dirty_|=topology||content||flags;
    // A revision may change only terrain or component records. World data is
    // evaluated lazily, once, only if an inspector or Frame actually needs it.
    world_ready_=false;revision_=revision;initialized_=true;
}
void Index::rebuild_topology(){
    ++counters_.topology_rebuilds;indices_.clear();indices_.reserve(metadata_.size());
    for(std::size_t i=0;i<metadata_.size();++i)indices_.emplace(metadata_[i].id,i);
    parents_.assign(metadata_.size(),missing);children_.assign(metadata_.size(),{});roots_.clear();
    for(std::size_t i=0;i<metadata_.size();++i){
        const auto at=indices_.find(metadata_[i].parent);
        if(metadata_[i].parent&&at!=indices_.end()&&at->second!=i){parents_[i]=at->second;children_[at->second].push_back(i);}
        else if(!metadata_[i].parent)roots_.push_back(i);
    }
    // Iterative traversal handles the full 1024-level budget without recursion.
    // Invalid cycles/missing parents remain absent rather than hanging the UI.
    preorder_.clear();std::vector<std::size_t> stack(roots_.rbegin(),roots_.rend());std::vector<bool> seen(metadata_.size());
    while(!stack.empty()){const auto i=stack.back();stack.pop_back();if(seen[i])continue;seen[i]=true;preorder_.push_back(i);for(auto it=children_[i].rbegin();it!=children_[i].rend();++it)stack.push_back(*it);}
    rebuild_flags();
}
void Index::rebuild_flags(){
    inherited_locked_.assign(metadata_.size(),true);
    for(auto i:preorder_)inherited_locked_[i]=metadata_[i].locked||(parents_[i]!=missing&&inherited_locked_[parents_[i]]);
}
void Index::rebuild_registered(){
    std::vector<bool> next(source_names_.size());
    for(const auto &n:metadata_)if(n.binding.kind==world_entity::Kind::SourceModel&&n.binding.index<next.size())next[n.binding.index]=true;
    if(next!=registered_){registered_=std::move(next);source_filter_dirty_=true;}
}
bool Index::matches(const std::string &name)const{
    for(const auto &token:tokens_){bool found=false;
        for(std::size_t i=0;i+token.size()<=name.size();++i){std::size_t k=0;for(;k<token.size();++k)if(lower(static_cast<unsigned char>(name[i+k]))!=static_cast<unsigned char>(token[k]))break;if(k==token.size()){found=true;break;}}
        if(!found)return false;
    }return true;
}
void Index::filter(const char *query,const std::vector<world_entity::Id> &collapsed){
    const char *next_query=query?query:"";
    if(query_!=next_query){query_=next_query;const auto &next=query_;tokens_.clear();std::size_t i=0;while(i<next.size()){while(i<next.size()&&space(next[i]))++i;const auto begin=i;while(i<next.size()&&!space(next[i]))++i;if(i>begin){auto token=next.substr(begin,i-begin);for(char &c:token)c=char(lower(static_cast<unsigned char>(c)));tokens_.push_back(std::move(token));}}rows_dirty_=source_filter_dirty_=true;}
    if(collapsed_!=collapsed){collapsed_=collapsed;rows_dirty_=true;}
    if(rows_dirty_){
        ++counters_.authored_row_rebuilds;rows_.clear();std::vector<bool> included(metadata_.size(),tokens_.empty()),folded(metadata_.size());
        if(!tokens_.empty())for(auto i:preorder_)if(matches(metadata_[i].name)){std::size_t parent=i;while(parent!=missing&&!included[parent]){included[parent]=true;parent=parents_[parent];}}
        for(auto id:collapsed_){const auto at=indices_.find(id);if(at!=indices_.end())folded[at->second]=true;}
        std::vector<unsigned> depths(metadata_.size());std::vector<bool> hidden(metadata_.size());
        for(auto i:preorder_){const auto parent=parents_[i];if(parent!=missing){depths[i]=depths[parent]+1;hidden[i]=hidden[parent]||(tokens_.empty()&&folded[parent]);}if(included[i]&&!hidden[i])rows_.push_back({metadata_[i].id,i,depths[i],!children_[i].empty(),folded[i]});}
        rows_dirty_=false;
    }
    if(source_filter_dirty_){++counters_.source_filter_rebuilds;filtered_sources_.clear();for(unsigned i=0;i<source_names_.size();++i)if(!registered_[i]&&matches(source_names_[i]))filtered_sources_.push_back(i);source_filter_dirty_=false;}
}
const std::string &Index::source_label(unsigned index)const{static const std::string empty;return index<source_labels_.size()?source_labels_[index]:empty;}
const world_entity::Node *Index::node(const world_entity::Graph &graph,world_entity::Id id)const{
    const auto at=indices_.find(id);if(at==indices_.end()||at->second>=graph.nodes.size()||graph.nodes[at->second].id!=id)return nullptr;return &graph.nodes[at->second];
}
bool Index::local_visible(world_entity::Id id)const{const auto at=indices_.find(id);return at!=indices_.end()&&metadata_[at->second].visible;}
bool Index::inherited_locked(world_entity::Id id)const{const auto at=indices_.find(id);return at==indices_.end()||inherited_locked_[at->second];}
bool Index::world_pose(const world_entity::Graph &graph,world_entity::Id id,world_entity::Transform &out,std::string &error){
    if(!world_ready_){std::vector<std::pair<world_entity::Id,world_entity::Transform>> computed;++counters_.world_refreshes;world_ok_=world_entity::world_transforms(graph,computed,world_error_);worlds_.assign(metadata_.size(),{});if(world_ok_)for(const auto &entry:computed){const auto at=indices_.find(entry.first);if(at!=indices_.end())worlds_[at->second]=entry.second;}world_ready_=true;}
    const auto at=indices_.find(id);if(!world_ok_||at==indices_.end()){error=world_ok_?"Missing entity":world_error_;return false;}out=worlds_[at->second];error.clear();return true;
}
std::vector<std::size_t> Index::descendants(const std::vector<world_entity::Id> &roots){
    if(roots.empty())return {};
    std::vector<bool> selected(metadata_.size());for(auto id:roots){const auto at=indices_.find(id);if(at!=indices_.end())selected[at->second]=true;}
    std::vector<std::size_t> result;result.reserve(metadata_.size());
    for(auto i:preorder_){++counters_.descendant_visits;if(parents_[i]!=missing)selected[i]=selected[i]||selected[parents_[i]];if(selected[i])result.push_back(i);}return result;
}
}
