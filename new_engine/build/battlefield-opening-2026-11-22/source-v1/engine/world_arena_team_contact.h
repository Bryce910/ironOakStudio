#ifndef VALWOOD_WORLD_ARENA_TEAM_CONTACT_H
#define VALWOOD_WORLD_ARENA_TEAM_CONTACT_H
#include "world_training_combat.h"
#include "world_combat_capacity.h"
#include <array>
namespace world_arena_combat {
// Persistent NPC IDs, not render IDs: opponent0..23, teammate24..29.
// Roots/points share the caller's arena-local metre frame. A host translates
// the frame once when querying exact posed source geometry. No geometry
// callback means no NPC-to-NPC damage, never a direction-only substitute.
struct TeamContactQuery {
    unsigned attacker_id=world_combat_capacity::npcs,defender_id=world_combat_capacity::npcs;
    TrainingDirection direction=TrainingDirection::High;
    std::uint64_t attack_serial=0;
    double phase_begin=.26,phase_end=.36,interval_seconds=0,clock=0,strike_seconds=.25;
    std::array<double,3> attacker_position={},defender_position={};
    double attacker_yaw=0,defender_yaw=0;
    TrainingOpponent attacker_state={},defender_state={};
};
using TeamContactQueryFn=bool(*)(const TeamContactQuery&,TrainingAnatomicalHit&,void*);
struct TeamContactEvent {
    std::uint64_t serial=0,attack_serial=0;
    unsigned attacker_id=world_combat_capacity::npcs,defender_id=world_combat_capacity::npcs;
    TrainingContactKind kind=TrainingContactKind::EnemyBody;
    TrainingDirection direction=TrainingDirection::High;
    float damage=0;double time=0;
    TrainingBodyPart body_part=TrainingBodyPart::None,severed_part=TrainingBodyPart::None;
    std::array<double,3> point={};
};
struct TeamContactEvents {
    static constexpr unsigned capacity=128;
    std::array<TeamContactEvent,capacity> values={};
    unsigned count=0,overflow=0;
    void clear(){count=overflow=0;}
};
}
#endif
