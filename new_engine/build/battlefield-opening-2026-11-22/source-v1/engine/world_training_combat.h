#ifndef VALWOOD_WORLD_TRAINING_COMBAT_H
#define VALWOOD_WORLD_TRAINING_COMBAT_H
#include <cstdint>
#include "world_combat_capacity.h"
#include "world_weapon_loadout.h"
#include "world_combat_timing.h"
#include "world_combat_anatomy.h"
#include "world_combat_difficulty.h"
#include <array>
#include <cstddef>
// Local village metres. A safe, non-lethal prototype; independent of rendering.
enum class TrainingDirection {Left,Right,High,Low,Thrust};
enum class TrainingState {Idle,Countdown,Fighting,RoundBreak,Won,Lost,Cancelled};
enum class TrainingEnemyPhase {Measure,Windup,Strike,Recovery,Stagger};
enum class TrainingPlayerPhase {Ready,Windup,Strike,Recovery};
// One enemy's simulation state. The player clock, stamina and actions are owned
// only by WorldTrainingCombat, even when eight opponents fight concurrently.
struct TrainingOpponent {
    float health=90,max_health=90;
    TrainingDirection direction=TrainingDirection::Left,guard=TrainingDirection::High;
    TrainingEnemyPhase phase=TrainingEnemyPhase::Measure;
    double time=0,duration=.65;
    std::uint32_t rng=0x5a17u;
    bool resolved=false,swoosh=false,defeat_announced=false,decapitated=false;
    unsigned hits_received=0,hits_dealt=0,swings=0,parries_received=0,blocks_received=0,blocks_made=0;
    std::uint64_t attack_start_serial=0,blocked_attack_serial=0;
    double blocked_attack_phase=.36,blocked_attack_time=0,attack_windup_seconds=.72;
    unsigned blocked_pose_slot=~0u;double blocked_pose_weight=0;
    TrainingDirection reaction_direction=TrainingDirection::High;
    std::uint64_t seen_attack_serial=0;
    std::uint16_t severed_parts=0;
    TrainingWeaponHand weapon_hand=TrainingWeaponHand::Right;
    TrainingDirection pending_guard=TrainingDirection::High;
    double guard_reaction_at=0,guard_tracking_until=0;
    bool guard_reaction_pending=false;
    unsigned guard_reactions=0;
};
struct TrainingAnatomicalQuery {
    unsigned opponent_id=0;
    world_weapon_loadout::Weapon weapon=world_weapon_loadout::Weapon::Sword;
    TrainingDirection direction=TrainingDirection::High;
    std::uint64_t attack_serial=0;
    double local_x=0,local_z=0,distance=0;
    bool spatial=false,weapon_guard=false,incoming=false;
    double interval_seconds=0;
    double contact_clock=0,windup_seconds=0,attack_commit_clock=-1;
    double strike_seconds=.09,recovery_seconds=.24,combo_link_seconds=.08;
    // Canonical visible attack phase: sweep the last precontact interval into
    // the exact contact key. Never sample a stale last-render attack pose.
    double phase_begin=.26,phase_end=.36;
    TrainingEnemyPhase defender_phase=TrainingEnemyPhase::Measure;
    TrainingDirection defender_direction=TrainingDirection::High,defender_guard=TrainingDirection::High;
    double defender_time=0,defender_duration=1;
    std::uint64_t defender_clash_serial=0;
    double defender_clash_phase=.36,defender_clash_time=0;
    unsigned defender_clash_pose_slot=~0u;double defender_clash_pose_weight=0;
    std::uint16_t severed_parts=0;
};
struct TrainingAnatomicalHit {
    bool valid=false,weapon_contact=false,cutting_edge=false;
    TrainingBodyPart part=TrainingBodyPart::None;
    double point[3]={}; // WORLD metres, shared with blood/steel presentation.
    double sweep_t=0;
    // A source renderer can map the linear gameplay clock to an authored
    // eased pose. Negative retains the caller's canonical interpolation.
    double attack_phase=-1;
    double defender_attack_phase=-1;
    unsigned defender_pose_slot=~0u;double defender_pose_weight=0;
};
using TrainingContactQueryFn=bool(*)(const TrainingAnatomicalQuery &,TrainingAnatomicalHit &,void *);
struct TrainingTarget {
    TrainingOpponent *opponent=nullptr;
    unsigned id=0;
    double distance=0;
    bool in_player_arc=true,player_can_guard=true;
    // Optional actor-local ground coordinates. Existing distance-only story
    // callers retain a conservative forward contact query.
    double local_x=0,local_z=0;
    bool spatial=false;
    bool advance_ai=true; // Arena advances NPCs assigned to another NPC itself.
    bool can_commit_attack=true; // Entering actors can perceive/defend before reaching formation.
};
enum class TrainingContactKind {PlayerBody,EnemyBody,PlayerParry,PlayerBlock,EnemyBlock,EnemyDefeated,PlayerSwing,EnemySwing,PlayerMiss};
// A committed player's swing resolves once. A miss is not a steel contact;
// the historical misses counter also counts stopped swings and is unsuitable
// as presentation authority.
enum class TrainingAttackOutcome {None,Miss,EnemyBlock,Body};
struct TrainingContactEvent {
    std::uint64_t serial=0,attack_serial=0;
    unsigned opponent_id=0;
    TrainingContactKind kind=TrainingContactKind::PlayerBody;
    TrainingDirection direction=TrainingDirection::High;
    float damage=0;
    double time=0;
    bool projectile=false,guard_broken=false;
    double point[3]={}; // WORLD when anatomical; arena-local when projectile.
    bool finisher=false;
    bool decapitated=false; // authoritative edged anatomical head slash only
    bool anatomical=false;
    TrainingBodyPart body_part=TrainingBodyPart::None,severed_part=TrainingBodyPart::None;
    std::uint16_t severed_parts=0;
    TrainingWeaponHand weapon_hand=TrainingWeaponHand::Right; // affected opponent's current remaining hand
};
struct TrainingEvents {
    static constexpr unsigned capacity=64;
    std::array<TrainingContactEvent,capacity> values={};
    unsigned count=0,overflow=0;
    void clear(){count=overflow=0;}
};
struct WorldTrainingCombat {
    TrainingState state=TrainingState::Idle;
    TrainingDirection selected=TrainingDirection::High,attack_direction=TrainingDirection::High;
    TrainingDirection enemy_direction=TrainingDirection::Left,enemy_guard=TrainingDirection::High,last_attack=TrainingDirection::High;
    TrainingEnemyPhase enemy_phase=TrainingEnemyPhase::Measure;
    TrainingPlayerPhase player_phase=TrainingPlayerPhase::Ready;
    world_weapon_loadout::Weapon weapon=world_weapon_loadout::Weapon::Sword;
    float gear_resistance=1;
    float town_damage=1,town_resistance=1,town_recovery=1;
    float health=100,stamina=100,enemy_health=48,enemy_max_health=48;
    double phase_time=0,enemy_time=0,player_time=0,guard_age=100,regen_delay=0,dodge_time=0,dodge_cooldown=0,feedback_time=0;
    double enemy_duration=.9,player_duration=0,clock=0,impact_pause=0;
    double attack_commit_clock=-1; // Latched physical commit; -1 means a legacy/manual fixture.
    // Optional candidate Sword overhead timing, configured before input. Zero
    // preserves legacy loadouts; commit_attack latches the physical duration.
    double sword_high_windup_min_seconds=0;
    unsigned player_swings=0,enemy_swings=0,guarded=0,dodges=0;
    bool player_swoosh=false,enemy_swoosh=false,impact_pause_enabled=true;
    static constexpr double player_reach=2.35,enemy_reach=2.55;
    // Contacts resolve at the windup/strike boundary. Buffered links shorten
    // committed recovery without widening reach or charging input twice.
    static constexpr double cut_windup_seconds=.18,thrust_windup_seconds=.17;
    static constexpr double chain_windup_seconds=.14,player_strike_seconds=.09,player_recovery_seconds=.24;
    static constexpr double combo_link_seconds=.08,attack_buffer_seconds=.30;
    static constexpr unsigned maximum_combo_swings=3;
    static constexpr double parry_window_seconds=.24,parry_stagger_seconds=.80;
    static constexpr float ready_stamina_per_second=26,guard_stamina_per_second=6;
    // Counter authority: hits/enemy_hits are body damage; guarded/blocks and
    // parries are weapon contacts. A missed swing increments neither kind.
    unsigned round=0,hits=0,parries=0,blocks=0,misses=0,rejected_attacks=0,direction_mask=0,repeated=0,enemy_hits=0;
    std::uint32_t rng=0x5a17u;
    bool guard=false,enemy_resolved=false,player_resolved=false;
    bool trail_threat=false,story_assist=false,watch_reward=false;
    // Per-session policy: ordinary training keeps its recovery between rounds;
    // consecutive escape threats can preserve the host's carried health.
    // reset_session() restores this default before starting any other mode.
    bool round_health_recovery=true;
    bool arena_mode=false;
    bool shared_arena=false;
    bool enemy_decapitated=false; // ephemeral corpse state, reset with each run/round
    std::uint16_t enemy_severed_parts=0;
    TrainingWeaponHand enemy_weapon_hand=TrainingWeaponHand::Right;
    std::uint64_t enemy_seen_attack_serial=0;
    TrainingDirection enemy_pending_guard=TrainingDirection::High;
    double enemy_guard_reaction_at=0,enemy_guard_tracking_until=0;
    bool enemy_guard_reaction_pending=false;
    unsigned enemy_guard_reactions=0;
    world_combat_difficulty::Difficulty difficulty=world_combat_difficulty::Difficulty::Normal;
    // Loading/viewer-owned read-only geometry bridge. Reset/start preserves it;
    // callback failure is a miss and never falls back to direction-only cuts.
    TrainingContactQueryFn contact_query=nullptr;
    void *contact_query_context=nullptr;
    // Optional per-frame presentation sink for ordinary one-opponent fights.
    // Shared arenas supply their own buffer to update_shared.
    TrainingEvents *contact_events=nullptr;
    // Fresh input edges may queue exactly one direction; energy is paid only
    // when the queued swing actually commits. Serial changes on every link.
    std::uint64_t attack_start_serial=0,contact_serial=0;
    std::uint64_t resolved_attack_serial=0;
    TrainingAttackOutcome resolved_attack_outcome=TrainingAttackOutcome::None;
    TrainingDirection resolved_attack_direction=TrainingDirection::High;
    std::array<double,3> resolved_attack_point={};
    bool resolved_attack_anatomical=false;
    unsigned resolved_attack_opponent=world_combat_capacity::npcs;
    double resolved_attack_time=0,resolved_attack_phase=.36;
    unsigned resolved_attack_pose_slot=~0u;double resolved_attack_pose_weight=0;
    std::uint64_t enemy_attack_start_serial=0,enemy_blocked_attack_serial=0;
    double enemy_blocked_attack_phase=.36,enemy_blocked_attack_time=0,enemy_attack_windup_seconds=.72;
    unsigned enemy_blocked_pose_slot=~0u;double enemy_blocked_pose_weight=0;
    float resolved_attack_damage=0;
    bool pending_attack=false;
    TrainingDirection pending_direction=TrainingDirection::High;
    double pending_time=0;
    unsigned combo_count=0,combo_links=0,buffered_attacks=0,expired_buffers=0;
    // Default values preserve synthetic/uncommitted legacy pose fixtures.
    // A real attack latches all durations before publishing its serial.
    world_combat_timing::Durations player_attack_timing;
    const world_combat_timing::Durations &committed_timing()const{return player_attack_timing;}
    std::uint64_t pending_attack_serial=0;
    float arena_enemy_health=90;
    const char *feedback="Approach the master of arms";
    static constexpr double instructor_x=-42.5,instructor_z=-68,enemy_x=-47,enemy_z=-73,start_x=-50,start_z=-73;
    bool engaged()const;
    bool near(double x,double eye_y,double z,bool flying=false)const;
    bool start(double x,double eye_y,double z,bool flying=false);
    bool start_threat(bool assist=false,bool reward=false,bool recover_health=true);
    bool start_arena(float player_health=100,float player_stamina=100,float opponent_health=90,bool assist=false);
    bool start_shared_arena(bool assist=false);
    void reset_session();
    // Caller supplies distinct live opponents and owns spatial movement. This
    // advances the player once, then independent enemy phases, in fixed steps.
    // Events append; caller clears the frame buffer before update/arrow hits.
    bool update_shared(double dt,TrainingTarget *,std::size_t,TrainingEvents &,bool paused=false);
    bool damage_opponent(TrainingOpponent &,unsigned id,float damage,TrainingDirection,TrainingEvents &);
    void finish_shared_arena(bool won);
    WorldTrainingCombat opponent_view(const TrainingOpponent &)const;
    void clear_buffer();
    void leave();
    void choose(TrainingDirection d);
    void set_guard(bool down);
    bool attack();
    bool dodge();
    void update(double dt,double player_x,double player_z,bool paused=false);
    float enemy_animation()const;
    float player_animation()const;
    const char *opponent_name()const;
private:
    void message(const char *text,double duration=1.25);
    void step(double dt,double distance);
    void next_enemy(TrainingEnemyPhase phase,double duration);
    void begin_round();
    unsigned random(unsigned n);
    bool commit_attack(TrainingDirection,bool chain);
    void shared_step(double,TrainingTarget *,std::size_t,TrainingEvents *);
    void event(TrainingEvents *,unsigned,TrainingContactKind,TrainingDirection,float damage=0,bool finisher=false,const TrainingOpponent *opponent=nullptr);
    void defeated(TrainingOpponent &,unsigned,TrainingEvents *);
};
const char *world_training_direction_name(TrainingDirection d);
#endif
