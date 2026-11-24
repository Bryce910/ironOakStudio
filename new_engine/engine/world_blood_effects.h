#ifndef VALWOOD_WORLD_BLOOD_EFFECTS_H
#define VALWOOD_WORLD_BLOOD_EFFECTS_H

#include "engine/world_combat_vfx.h"
#include <array>
#include <cstdint>

// Source-independent, CPU-only lifetime/event ownership. No assets, allocations,
// drawing, physics or editor document mutation occurs here.
namespace world_blood_effects {
using Point=world_combat_vfx::Point;
constexpr unsigned mesh_capacity=8,stain_capacity=128;
constexpr unsigned events_per_defender_limit=16;
constexpr double maximum_step=.25;
constexpr double maximum_persistent_stain_age=3600.;
constexpr double playback_rate=3.;
constexpr double source_start_seconds=.045;

struct Config {
    double mesh_duration=1.,stain_duration=25.;
    // Permanent floor marks survive duel rebases until reset/capacity replacement.
    // Timed marks retain the configured stain_duration expiration/fade behavior.
    bool persistent_stains=false;
    // Optional real-time clip lengths, in the same deterministic variant order
    // as the renderer. Zero count keeps the source-independent fallback above.
    std::array<double,8> variant_durations={};
    unsigned variant_count=0;
};
struct Contact {
    Point position,direction={0,0,1};
    double floor_y=0;
    bool valid=false;
    float strength=1;
    bool finisher=false;
};
struct ResolvedEvent {std::uint64_t serial=0;unsigned defender=1;Contact contact;};
struct Input {
    std::uint64_t outgoing_hits=0,incoming_hits=0;
    // Optional combat clock. Regression is diagnostic only: living effects
    // survive a new duel even when both the clock and counters restart.
    double combat_clock=0;
    Contact contacts[2]; // defender0 player, defender1 opponent
    bool paused=false;
    // Arena contacts carry their own resolved location; a batch of hits must
    // never reuse the last focused opponent's torso for every blood effect.
    bool resolved_events=false;
    const ResolvedEvent *events=nullptr;
    unsigned event_count=0;
};
struct MeshEffect {
    Point position,forward;
    double yaw=0,age=0,duration=0;
    std::uint64_t serial=0;
    unsigned defender=0;
    std::uint64_t hit_id=0;
    unsigned lobe=0;
    float strength=1;
    bool finisher=false;
    double delay=0;
    double floor_y=0;
};
struct Stain {
    Point position;
    double yaw=0,age=0,duration=0;
    std::uint64_t serial=0;
    unsigned defender=0;
    std::uint64_t hit_id=0;
    float strength=1;
    bool finisher=false;
    bool persistent=false;
};
struct Stage {
    std::array<MeshEffect,mesh_capacity> meshes={};
    std::array<Stain,stain_capacity> stains={};
    unsigned mesh_count=0,stain_count=0;
};
struct Stats {
    std::uint64_t outgoing_events=0,incoming_events=0;
    std::uint64_t mesh_spawned=0,stains_spawned=0,mesh_replaced=0,stains_replaced=0;
    std::uint64_t impacts_spawned=0,secondary_spawned=0,finisher_events=0;
    std::uint64_t invalid_inputs=0,invalid_contacts=0,limited_events=0;
    std::uint64_t counter_rebases=0,clock_rebases=0,resets=0,rejected_configurations=0;
    unsigned active_meshes=0,active_stains=0;
    double elapsed=0; // unpaused, bounded simulation time since explicit reset
    float max_strength=0;
};

class System {
public:
    // Valid duration ranges: mesh [1/240,30] s, stains [.1,120] s. Invalid
    // configuration leaves the previous configuration and living effects intact.
    bool configure(const Config &);
    Config config()const{return config_;}
    // Advances at most maximum_step seconds, then emits each new body event.
    // A counter rollback establishes its new baseline without replaying hits.
    // At most16 events per defender are emitted per update; excessive jumps are
    // counted, consumed and discarded, so work remains bounded for corrupt input.
    // Invalid contact drops/consumes only its defender's events. Invalid time
    // rejects the whole update. Pause changes no clocks, baselines or effects.
    bool update(const Input &,double delta);
    // Explicitly rebase after a duel/load without clearing living effects.
    bool rebase(std::uint64_t outgoing,std::uint64_t incoming,double combat_clock=0);
    // Explicit restart/unload. Clears pools, elapsed time and event baselines;
    // optional counters avoid replaying an already-running combat after clearing.
    // Cumulative diagnostics and monotonically increasing serials are retained.
    void reset(std::uint64_t outgoing=0,std::uint64_t incoming=0);
    Stage stage()const;
    Stats stats()const{return stats_;}
private:
    Config config_;
    std::array<MeshEffect,mesh_capacity> meshes_={};
    std::array<Stain,stain_capacity> stains_={};
    std::array<std::uint64_t,2> seen_={}; // outgoing,incoming
    std::uint64_t serial_=0,hit_id_=0;
    std::uint64_t event_seen_=0;
    double last_clock_=0;
    bool have_clock_=false;
    Stats stats_;
    void emit(const Contact &,unsigned defender);
    void refresh_counts();
};
}
#endif
