#ifndef VALWOOD_WORLD_NPC_MEMORY_H
#define VALWOOD_WORLD_NPC_MEMORY_H
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Authored NPC knowledge, not a language model or authoritative world state.
// Load/configure only at an explicit owner-thread boundary. Recall has fixed
// storage, no IO, no GPU work and no allocations; call on decisions, not draws.
namespace world_npc_memory {
constexpr unsigned memory_limit=256,tag_limit=8,query_tag_limit=16,recall_limit=8,depth_limit=16;
enum class Kind : unsigned {Event,Relationship,Belief,Goal,Knowledge,Skill};
enum class Source : unsigned {Authored,Witnessed,Hearsay};
// Authored descriptors only. No model morphs or behavior inference is applied.
// Missing numeric values stay unspecified; age zero remains a valid value.
struct Appearance {
    std::optional<double> age_years,height_cm,weight_kg;
    std::string build,skin_tone,eye_color,hair_color,hair_style,hair_texture;
    std::string hair_thickness,hair_density,facial_hair,distinguishing_features;
};
struct Profile {
    std::string npc_id,name,role,faction,summary;
    std::uint32_t actor_id=0; // 0 unbound; otherwise persistent world_living::Actor::id
    std::vector<std::string> traits;
    Appearance appearance;
};
struct Memory {
    std::string id,parent_id="core",title,body;
    Kind kind=Kind::Event;Source source=Source::Authored;
    double importance=.7,confidence=1,emotion=0,occurred_hour=0,half_life_hours=0;
    bool shareable=false,enabled=true;
    std::vector<std::string> tags;
};
struct Document {Profile profile;std::vector<Memory> memories;};
struct Query {
    std::array<std::string_view,query_tag_limit> tags={};unsigned count=0;
    double game_hour=0;bool shareable_only=false;
};
struct Hit {unsigned index=0,matched_tags=0;double score=0,decay=1;};
struct Result {std::array<Hit,recall_limit> hits={};unsigned count=0;bool valid=false;};
bool valid(const Document &,std::string &error);
const char *kind_name(Kind);
const char *source_name(Source);
class Bank {
public:
    bool configure(const Document &,std::string &error);
    // Versioned bounded text emitted by the browser's Export to engine action.
    // Failure preserves the previously committed bank and query results.
    bool load(const char *path,std::string &error);
    // Bounded portable codec for engine saves and explicit runtime bindings.
    bool decode(std::string_view,std::string &error);
    bool encode(std::string &out,std::string &error) const;
    Result recall(const Query &) const;
    const Document &document() const{return document_;}
    bool ready() const{return ready_;}
private:
    Document document_;std::array<int,memory_limit> parents_={};bool ready_=false;
};
}
#endif
