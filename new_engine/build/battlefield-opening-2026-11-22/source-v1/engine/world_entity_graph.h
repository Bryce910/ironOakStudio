#ifndef VALWOOD_WORLD_ENTITY_GRAPH_H
#define VALWOOD_WORLD_ENTITY_GRAPH_H
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
namespace world_studio {struct Document;}
// Authoring metadata only. This graph does not allocate runtime render objects.
namespace world_entity {
using Id=std::uint64_t;
constexpr unsigned node_limit=1024;
enum class Kind : unsigned {None,Placement,SourceModel,Light,Actor,Particle,Water,Navigation,Sound,Weather};
struct Binding {Kind kind=Kind::None;unsigned index=0;};
bool operator==(Binding,Binding);
bool operator!=(Binding,Binding);
// Metres and degrees; positive scale only. Axis factors multiply uniform scale.
struct Transform {double x=0,y=0,z=0,yaw=0,scale=1,pitch=0,roll=0,sx=1,sy=1,sz=1;};
struct Node {Id id=0,parent=0;std::string name;Binding binding;Transform local;std::string source_key={};bool visible=true,locked=false;};
struct Graph {Id next_id=1;std::vector<Node> nodes;};
const Node *find(const Graph &,Id);
Node *find(Graph &,Id);
// Parent flags inherit. Unregistered legacy objects are visible/editable.
bool visible(const Graph &,Binding);
bool editable(const Graph &,Binding);
Transform compose(const Transform &parent,const Transform &local);
Transform relative(const Transform &parent,const Transform &world);
bool valid_transform(const Transform &);
// A null document validates graph structure only. SourceModel has index<65536;
// source scene fingerprint / actual source ownership stays with the caller.
bool validate(const Graph &,const world_studio::Document *,std::string &error);
bool world_transform(const Graph &,Id,Transform &,std::string &error);
// One validated traversal for hierarchy overlays/inspectors. Cache until the
// draft changes; querying every node with world_transform repeats validation.
bool world_transforms(const Graph &,std::vector<std::pair<Id,Transform>> &,std::string &error);
// All mutations preserve the original graph on failure. Outputs change only
// on success. IDs monotonically increase and never wrap or get recycled.
bool add(Graph &,Id parent,const std::string &,Binding,const Transform &local,Id &created,std::string &error);
bool set_world(Graph &,Id,const Transform &,std::string &error);
bool reparent(Graph &,Id,Id new_parent,bool preserve_world,std::string &error);
bool erase_subtree(Graph &,Id,std::vector<Binding> &removed,std::string &error);
enum class ChildPolicy {PreserveWorld,EraseSubtree};
// Call with the PRE-compaction binding when its scene-array element is erased.
// PreserveWorld promotes children to the deleted node's parent. EraseSubtree
// returns all removed bindings; caller owns deletion of their scene objects.
// Later indices of the erased kind decrement even if it had no graph node.
bool erase_binding(Graph &,Binding,ChildPolicy,std::vector<Binding> &removed,std::string &error);
// Reconcile records already edited numerically/by a legacy gizmo. Unlisted
// nodes keep their world transforms. Use transform_instance for connected
// bundle moves; synchronizing never mutates the authoring Document.
bool synchronize(Graph &,const std::vector<std::pair<Binding,Transform>> &,std::string &error);
// Full canonical source metadata is supplied by Studio. Referenced keys must
// occur exactly once in BOTH tables; changed/removed/ambiguous objects fail.
// No nearest-position guessing. Outputs/graph remain unchanged on failure.
bool source_index_map(const std::vector<std::string> &old_keys,const std::vector<std::string> &new_keys,
                      const std::vector<unsigned> &referenced,std::vector<std::pair<unsigned,unsigned>> &,std::string &error);
bool remap_source_bindings(Graph &,const std::vector<std::string> &old_keys,
                           const std::vector<std::string> &new_keys,std::vector<std::pair<unsigned,unsigned>> &,std::string &error);
// Bound descendants require an explicit mapping to distinct destination
// objects. This prevents two graph nodes accidentally editing the same object.
bool clone_subtree(Graph &,Id,Id new_parent,const std::vector<std::pair<Binding,Binding>> &,
                   Id &created,std::vector<std::pair<Id,Id>> &ids,std::string &error);
// Durable sidecar / prefab metadata. Streamed-world Document persistence is
// intentionally owned by Studio, rather than a second renderer/file format.
bool encode(const Graph &,std::string &text,std::string &error);
bool decode(const std::string &,Graph &,std::string &error);
}
#endif
