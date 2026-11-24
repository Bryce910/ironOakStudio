#ifndef VALWOOD_WORLD_DESIGNER_SCENE_INDEX_H
#define VALWOOD_WORLD_DESIGNER_SCENE_INDEX_H
#include "engine/world_entity_graph.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

// Editor-only read model. Graph mutations must advance the document revision;
// replacing the immutable source registry must change its generation token.
// Local transforms never invalidate source labels or either filtered row list.
namespace world_designer_scene {
struct Row {world_entity::Id id=0;std::size_t node=0;unsigned depth=0;bool children=false,collapsed=false;};
struct Counters {
    std::uint64_t metadata_compared=0,topology_rebuilds=0,content_refreshes=0;
    std::uint64_t source_registry_rebuilds=0,authored_row_rebuilds=0,source_filter_rebuilds=0;
    std::uint64_t world_refreshes=0,descendant_visits=0;
};
class Index {
public:
    using Labeler=std::string (*)(const std::string &);
    void sync(const world_entity::Graph &,const std::vector<std::string> &sources,
              const std::string &source_generation,std::uint64_t revision,Labeler=nullptr);
    void filter(const char *query,const std::vector<world_entity::Id> &collapsed);
    const std::vector<Row> &rows()const{return rows_;}
    const std::vector<unsigned> &sources()const{return filtered_sources_;}
    const std::string &source_label(unsigned index)const;
    const world_entity::Node *node(const world_entity::Graph &,world_entity::Id)const;
    bool local_visible(world_entity::Id)const;
    bool inherited_locked(world_entity::Id)const;
    bool world_pose(const world_entity::Graph &,world_entity::Id,world_entity::Transform &,std::string &error);
    // Returns each node in the selected subtree union once, in parent-first order.
    std::vector<std::size_t> descendants(const std::vector<world_entity::Id> &roots);
    const Counters &counters()const{return counters_;}
    std::uint64_t revision()const{return revision_;}
private:
    struct Metadata {world_entity::Id id=0,parent=0;world_entity::Binding binding;std::string name;bool visible=true,locked=false;};
    static constexpr std::size_t missing=std::numeric_limits<std::size_t>::max();
    std::uint64_t revision_=~std::uint64_t(0);
    bool initialized_=false,rows_dirty_=true,source_filter_dirty_=true,world_ready_=false,world_ok_=false;
    std::string generation_,query_,world_error_;
    std::vector<Metadata> metadata_;
    std::unordered_map<world_entity::Id,std::size_t> indices_;
    std::vector<std::size_t> parents_,roots_,preorder_;
    std::vector<std::vector<std::size_t>> children_;
    std::vector<bool> inherited_locked_;
    std::vector<std::string> source_names_,source_labels_,tokens_;
    std::vector<bool> registered_;
    std::vector<world_entity::Id> collapsed_;
    std::vector<Row> rows_;
    std::vector<unsigned> filtered_sources_;
    std::vector<world_entity::Transform> worlds_;
    Counters counters_;
    void rebuild_topology();
    void rebuild_flags();
    void rebuild_registered();
    bool matches(const std::string &)const;
};
}
#endif
