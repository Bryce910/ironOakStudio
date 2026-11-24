#ifndef VALWOOD_WORLD_LIVING_H
#define VALWOOD_WORLD_LIVING_H
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// CPU-only, owner-thread persistent simulation. Nothing is enabled until an
// authored Definition is configured. Offline progress is computed on resume;
// this module does not keep running while the computer/application is off.
namespace world_living {
using Id=std::uint32_t;
constexpr unsigned faction_limit=16,settlement_limit=32,actor_limit=256,event_limit=128,max_steps_per_call=64;
enum class Resource : unsigned { Food,Wood,Ore,Coin,Count };
constexpr unsigned resource_count=unsigned(Resource::Count);
using Quantities=std::array<std::uint64_t,resource_count>;
enum class Activity : unsigned { Sleep,Commute,Work,Eat,Guard,Leisure };
enum class Action : unsigned { None,Gather,Consume,Transfer,Patrol };
constexpr unsigned action_bit(Action a){return unsigned(a)<5?(1u<<unsigned(a)):0;}
constexpr unsigned all_actions=action_bit(Action::Gather)|action_bit(Action::Consume)|action_bit(Action::Transfer)|action_bit(Action::Patrol);
struct Faction {Id id=0;std::string name;std::vector<Id> allies;};
struct Settlement {Id id=0,faction=0;std::string name;Quantities stock={},reserve={},capacity={1000,1000,1000,1000};};
struct Actor {
    Id id=0,faction=0,home=0,trade_target=0;std::string name;
    Resource work_resource=Resource::Food;unsigned work_amount=2,ration=1,cooldown_seconds=3600,allowed_actions=all_actions;
    unsigned wake_minute=360,work_start=480,work_end=1020,sleep_minute=1320,meal_minute=720,meal_duration=60,commute_minutes=30;
    bool guard=false;
};
struct Definition {
    bool enabled=false;std::uint64_t seed=1;unsigned step_seconds=60,start_minute=420,maximum_debt_seconds=7*86400;
    std::vector<Faction> factions;std::vector<Settlement> settlements;std::vector<Actor> actors;
};
struct SettlementState {Id id=0;Quantities stock={},reserve={};};
struct ActorState {
    Id id=0;Activity activity=Activity::Leisure;
    std::uint64_t next_action_tick=0,last_meal_day=~std::uint64_t(0),actions=0,patrols=0;
};
struct Proposal {Id actor=0,source=0,target=0;Action action=Action::None;Resource resource=Resource::Food;std::uint64_t amount=0;};
struct Event {std::uint64_t sequence=0,tick=0;Proposal proposal;};
struct State {
    bool clock_initialized=false;std::uint64_t last_wall_milliseconds=0,debt_milliseconds=0,tick=0,dropped_milliseconds=0;
    Quantities consumed={};std::array<SettlementState,settlement_limit> settlements={};std::array<ActorState,actor_limit> actors={};
    std::array<Event,event_limit> events={};unsigned event_start=0,event_count=0;std::uint64_t event_sequence=0;
};
struct Stats {std::uint64_t observed_clocks=0,rollback_clocks=0,rejected_proposals=0,policy_failures=0,processed_steps=0;};
struct Decision {
    const Definition *definition=nullptr;const Actor *actor=nullptr;const ActorState *actor_state=nullptr;const SettlementState *home=nullptr;
    std::uint64_t tick=0,day=0,random=0;unsigned minute=0;
    const State *world_state=nullptr; // immutable current balances, never live render data
};
// Optional untrusted policy proposal hook (e.g. an application-owned small model).
// Return false for the deterministic default. Every returned proposal is checked
// by the same strict world rules; invalid/throwing hooks fall back to rules.
// At most one scheduled actor is polled per simulation step (max64 polls per
// process call), never every NPC/render frame. Hooks must return promptly;
// expensive inference belongs outside this scheduler, publishing cached results.
// No trained weights, network service or language-model inference is built in.
using Policy=bool (*)(const Decision &,Proposal &,void *userdata);
struct ProcessResult {unsigned steps=0;std::uint64_t pending_steps=0;bool at_tick_limit=false;};

bool valid(const Definition &,std::string &error);
const char *activity_name(Activity);
const char *resource_name(Resource);

class World {
public:
    // Transactional configure/reset. Editing the definition is explicit; it
    // cannot silently reinterpret an existing saved world's IDs/resources.
    bool configure(const Definition &,double unix_seconds,std::string &error);
    bool observe_clock(double unix_seconds,std::string &error);
    // Explicit editor Stop boundary: preserve resources, schedules and debt,
    // while excluding the discarded preview's wall time from later catch-up.
    bool rebase_clock(double unix_seconds,std::string &error);
    ProcessResult process(unsigned step_budget,Policy policy=nullptr,void *userdata=nullptr);
    bool advance(double unix_seconds,unsigned step_budget,ProcessResult &,std::string &error,
                 Policy policy=nullptr,void *userdata=nullptr);
    // Explicit scheduling/policy action. Validates IDs, activity, ownership,
    // alliance, resource balances/capacity and per-actor cooldown atomically.
    bool propose(const Proposal &,std::string &error);
    Proposal default_proposal(unsigned actor_index) const;
    const Definition &definition() const {return definition_;}
    const State &state() const {return state_;}
    Stats stats() const {return stats_;}
    Activity activity(unsigned actor_index) const;
    unsigned minute() const;
    std::uint64_t day() const;
    std::vector<Event> recent_events() const; // explicit inspector only
    // Versioned, validated atomic save/load. No IO occurs in scheduler ticks.
    bool save(const std::string &path,std::string &error) const;
    bool load(const std::string &path,std::string &error);
    bool encode(std::string &,std::string &error) const;
    bool decode(const std::string &,std::string &error);
private:
    Definition definition_;State state_;Stats stats_;
    int settlement_index(Id) const;
    int actor_index(Id) const;
    bool allied(Id,Id) const;
    void update_activities();
    bool valid_state(std::string &error) const;
};
}
#endif
