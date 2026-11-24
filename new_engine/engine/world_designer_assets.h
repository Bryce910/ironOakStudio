#ifndef VALWOOD_WORLD_DESIGNER_ASSETS_H
#define VALWOOD_WORLD_DESIGNER_ASSETS_H

#include "world_scenery.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Registry metadata only. These helpers never open asset files or touch GL.
namespace world_designer_assets {
enum class Category { All, Buildings, Nature, Props, Characters };
constexpr std::size_t favorite_limit=128,recent_limit=16;
constexpr const char *payload_type="VALWOOD_ASSET_ALIAS";
struct AliasPayload {char alias[96]={};};
static_assert(sizeof(AliasPayload)==96,"Bounded ImGui alias payload");
struct Record {
    unsigned index=0;
    std::string alias,label,stem;
    Category category=Category::Props;
    unsigned lods=0,collision=0,vegetation=0;
    float distance=0,split=0,max_scale=1;
    bool tree=false;
};
const char *category_name(Category);
const char *collision_name(unsigned);
std::string humanize(std::string_view);
Category classify(const WorldSceneryAlias &);
Record record(unsigned,const WorldSceneryAlias &);
std::uint64_t signature(const WorldSceneryAlias &);
// The registry publishes a generation at its owner-thread transaction boundary.
// Unchanged frames compare two scalars rather than hashing the entire library.
// The sorted indirection owns no string_views or registry pointers: aliases and
// labels remain valid after the registry replaces its string storage.
class RegistryIndex {
public:
    using Getter=WorldSceneryAlias (*)(std::size_t);
    bool current(std::uint64_t generation,std::size_t count)const{return generation_==generation&&count_==count;}
    bool refresh(std::vector<Record> &,std::uint64_t generation,std::size_t count,Getter);
    int find(const std::vector<Record> &,std::string_view alias)const;
    std::uint64_t generation()const{return generation_;}
    std::uint64_t rebuilds()const{return rebuilds_;}
private:
    std::uint64_t generation_=0,rebuilds_=0;
    std::size_t count_=static_cast<std::size_t>(-1);
    std::vector<unsigned> sorted_;
};
// Every space-separated token must occur in alias, label, or source stem.
// Matching is ASCII case insensitive, allocation free and separator tolerant.
bool matches(const Record &,std::string_view query);
bool contains(const std::vector<std::string> &,std::string_view);
bool toggle_favorite(std::vector<std::string> &,std::string_view);
void remember(std::vector<std::string> &,std::string_view);
void normalize_preferences(std::vector<std::string> &,std::size_t limit);
bool valid_alias(std::string_view);
bool encode_payload(std::string_view,AliasPayload &);
bool decode_payload(const void *,std::size_t,std::string_view &);
// The Objects tool's default grounding is zero burial/elevation/offset.
// Sanitize new placement controls before creating an undoable draft record.
bool placement_parameters(const WorldSceneryAlias &,double x,double z,
                          float yaw,float scale,float &out_yaw,float &out_scale);
}
#endif
