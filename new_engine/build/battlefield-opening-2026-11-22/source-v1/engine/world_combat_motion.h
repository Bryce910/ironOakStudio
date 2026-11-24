#ifndef VALWOOD_WORLD_COMBAT_MOTION_H
#define VALWOOD_WORLD_COMBAT_MOTION_H
#include <cstddef>
#include <array>
#include "world_combat_capacity.h"
#include "world_arena_team_contact.h"
#include "engine/world_training_combat.h"
struct WorldCombatMotion;
struct WorldArenaBowPose;
// Stable appearance follows the persistent actor ID, including queued enemies,
// corpses and detached parts. Four appearances share a single pose bank.
unsigned world_combat_motion_variant_mask(unsigned actor_id);
struct WorldCombatVariantStats {
 bool loaded=false;unsigned available_mask=0;
 std::array<unsigned long long,4> body_draws={},crowd_draws={},piece_draws={},support_captures={};
};
WorldCombatVariantStats world_combat_motion_variant_stats(const WorldCombatMotion *);
namespace world_combat_motion {
inline constexpr float reaction_duration=.32f;
inline constexpr float death_duration=1.8f;
}
struct WorldCombatBlade {double base[3]={},tip[3]={};};
struct WorldCombatContinuityStats {
 unsigned long long guarded_entries=0,late_combo_finishes=0,recovery_settles=0,reaction_settles=0;
 // Rendered bridges/recoils and authoritative new-serial reaction takeovers.
 unsigned long long moving_entries=0,guard_recoils=0,reaction_counters=0,shield_hit_returns=0;
};
WorldCombatContinuityStats world_combat_motion_continuity_stats(const WorldCombatMotion *);
struct WorldCombatGripStats {
 bool loaded=false;unsigned banks=0,certified_pairs=0,fp_certified_pairs=0;
 unsigned long long frames=0,body_frames=0,arms_frames=0,disabled_frames=0,failures=0;
 unsigned long long body_draws=0,arms_draws=0,uploads=0,metadata_bytes=0,resident_cpu_bytes=0,extra_gpu_bytes=0;
 double total_cpu_ms=0,maximum_cpu_ms=0;
};
WorldCombatGripStats world_combat_motion_grip_stats(const WorldCombatMotion *);
// Loading boundary only, after loadouts and locomotion. Optional projects
// without this metadata preserve their existing rendering and controls.
bool world_combat_motion_load_grips(WorldCombatMotion *,const char *file,char *error,std::size_t capacity);
struct WorldCombatPose {unsigned slot=0,guard_direction=2;float guard_weight=0,blend=0;bool active=false;unsigned reaction_direction=2;bool defeated=false;float defeat_phase=0;bool combo_transition=false;unsigned combo_from=2,combo_to=2;};
// Read the pose committed by the renderer, rather than the requested input.
WorldCombatPose world_combat_motion_pose(const WorldCombatMotion *,unsigned actor);
// Includes the renderer's damped enemy-facing and visual attack-root turn.
bool world_combat_motion_facing(const WorldCombatMotion *,unsigned actor,float &yaw);
bool world_combat_motion_blade(const WorldCombatMotion *,unsigned actor,WorldCombatBlade &,bool viewmodel=false);
// Loading-only side data on the existing immutable enemy bank; no duplicate
// full-body models or material images. Contact and wounds use WORLD metres.
bool world_combat_motion_load_anatomy(WorldCombatMotion *,const char *directory);
struct WorldCombatContactFrame {
 double player[3]={},defender[3]={};float player_yaw=0,defender_yaw=0;
 // Optional exact WORLD cutting-edge samples at the query's evenly spaced
 // phase_begin..phase_end keys. The caller owns them for this synchronous
 // query only. Null/zero retains the original baked-weapon sampler.
 // Keeping these local to the query prevents old view/teleport/weapon samples
 // from becoming an authoritative sweep after the renderer changes rigs.
 const WorldCombatBlade *attacker_blades=nullptr;unsigned attacker_blade_count=0;
 // Separate-species defenders may use the existing player's legacy bank when
 // exact skeletal samples are unavailable. Never sample the Orc as player0.
 const WorldCombatMotion *attacker_motion=nullptr;
};
bool world_combat_motion_contact(const WorldCombatMotion *,const TrainingAnatomicalQuery &,const WorldCombatContactFrame &,TrainingAnatomicalHit &);
// Incoming strikes use the attacking NPC's actual immutable weapon sockets.
// Defender edges are WORLD samples over the same phase interval. Zero samples
// means there is no raised weapon, not an inferred directional block. Body
// capsules are supplied by the player's movement/rig controller in WORLD metres.
struct WorldCombatIncomingFrame {
 double opponent[3]={};float opponent_yaw=0;
 const WorldCombatBlade *defender_blades=nullptr;unsigned defender_blade_count=0;
 const world_combat_anatomy::Capsule *defender_body=nullptr;unsigned defender_body_count=0;
 const world_combat_anatomy::Capsule *defender_guards=nullptr;unsigned defender_guard_count=0;
 // Exact source shield triangles; optional second pose must retain index
 // order. Actual moving surfaces replace any inferred directional volume.
 const std::array<double,9> *defender_guard_triangles=nullptr,*defender_guard_triangles_end=nullptr;
 unsigned defender_guard_triangle_count=0;
};
// Pure source forecast at one linear combat-clock phase. Uses the identical
// eased source sampling, hand and WORLD placement as incoming contact; it
// neither requires defender geometry nor publishes events/telemetry.
bool world_combat_motion_incoming_blade(const WorldCombatMotion *,unsigned opponent_id,const TrainingAnatomicalQuery &,const WorldCombatIncomingFrame &,double linear_canonical_phase,WorldCombatBlade &);
bool world_combat_motion_incoming_contact(const WorldCombatMotion *,unsigned opponent_id,const TrainingAnatomicalQuery &,const WorldCombatIncomingFrame &,TrainingAnatomicalHit &);
// Pure NPC-to-NPC geometry bridge. Persistent IDs are0..29; caller roots and
// returned point share one metre frame. No gameplay events or damage occur here.
bool world_combat_motion_team_contact(const WorldCombatMotion *attacker,const WorldCombatMotion *defender,const world_arena_combat::TeamContactQuery &,TrainingAnatomicalHit &);
bool world_combat_motion_stump(const WorldCombatMotion *,unsigned actor,TrainingBodyPart,double point[3],double axis[3]);
bool world_combat_motion_stump_at(const WorldCombatMotion *,TrainingBodyPart,float collapse,double x,double feet,double z,float yaw,TrainingWeaponHand,double point[3],double axis[3],unsigned actor_id=0);
struct WorldCombatAnatomyStats {bool loaded=false;std::array<unsigned,8> launches={},active={},settled={};std::array<unsigned long long,8> detached_draws={};unsigned long long body_draws=0,cap_draws=0,mirrored_draws=0,contact_queries=0,contact_hits=0,weapon_contacts=0;unsigned stumps=0;double maximum_ground_penetration=0,support_capture_us_max=0;unsigned support_failures=0,support_hull_points_peak=0;unsigned long long support_retained_bytes=0;
 unsigned long long physics_substeps=0,physics_contacts=0,physics_sleeps=0,physics_budget_clamps=0,physics_invalid=0;
 unsigned long long physics_recycles=0,physics_floor_samples=0;
 std::size_t support_points_active=0;double physics_update_us_max=0;
};
WorldCombatAnatomyStats world_combat_motion_anatomy_stats(const WorldCombatMotion *);
struct WorldCombatDetachedPieceState {
 bool active=false,settled=false;unsigned actor=0,part=0;
 double position[3]={},orientation[4]={0,0,0,1},velocity[3]={},angular_velocity[3]={};
 double lowest_support=0,ground=0;
};
unsigned world_combat_motion_detached_states(const WorldCombatMotion *,WorldCombatDetachedPieceState *,unsigned capacity);
// Render-only continuation for focused victory results. Host calls only when
// the ordinary combat update is paused, avoiding a second physics tick.
void world_combat_motion_settle_pieces(WorldCombatMotion *,float dt);
struct WorldCombatDeathStats {
 bool loaded=false;unsigned long long captures=0,frames=0,completions=0,body_draws=0,cap_draws=0,crowd_draws=0,failures=0;
 unsigned long long source_cpu_bytes=0,staging_cpu_bytes=0,extra_gpu_bytes=0;double maximum_update_ms=0;
};
struct WorldCombatDeathPose {
 bool valid=false,bridging=false;unsigned source_slot=0,slot=148;
 float source_weight=0,weight=0,phase=0,alpha=1,yaw=0;
};
WorldCombatDeathStats world_combat_motion_death_stats(const WorldCombatMotion *);
// Includes the authored corpse pose after its stable actor ID enters crowd rendering.
WorldCombatDeathPose world_combat_motion_death_pose(const WorldCombatMotion *,unsigned actor);
struct WorldCombatMotionStats{unsigned pose_assets=0,actors=0,crowd_instances=0,rigid_weapon_pairs=0,reaction_gait_interrupts=0;std::array<unsigned long long,4> rigid_body_draws={},rigid_arms_draws={};unsigned long long uploads=0;bool polished=false;unsigned reaction_assets=0,death_assets=0,fallen_instances=0,peak_fallen_instances=0;unsigned long long directional_reactions=0,death_starts=0;unsigned active_enemies=0,peak_actors=0,combo_batches=0;unsigned long long combo_transitions=0;bool aquatic_loaded=false,aquatic_active=false;unsigned aquatic_pose_assets=0,aquatic_mode=0;float aquatic_phase=0;unsigned long long aquatic_updates=0,aquatic_uploads=0;bool locomotion_loaded=false;float locomotion_phase=0,locomotion_weight=0,locomotion_speed=0;std::array<float,4> locomotion_direction={1,0,0,0};unsigned long long locomotion_uploads=0,locomotion_gpu_bytes=0;};
// All pose/transition batches are allocated at loading time. Shared texture keys
// are the purchased shield knight's existing original material images.
// Polished banks require172 poses plus validated sockets; their visual sampling
// adds attack weight while retaining the same gameplay contact boundaries.
WorldCombatMotion *world_combat_motion_load(const char *directory,char *error,std::size_t capacity,bool polished=false);
// Loading-boundary opponent bank. This never loads/draws player arms or accepts
// player equipment. Its own measured weapon pivots, sockets, 172 body poses,
// anatomy labels and cut caps must all validate before a selectable bank exists.
// Failure returns nullptr; another species' geometry is never substituted.
WorldCombatMotion *world_combat_motion_load_opponents(const char *body_directory,const char *anatomy_directory,char *error,std::size_t capacity);
bool world_combat_motion_opponents_ready(const WorldCombatMotion *);
// Exact source-pose streaming: one shared base UV/index/material asset, mapped
// immutable PNT packet, and bounded current actor/frozen-piece GPU buffers.
// Actor masks use persistent IDs0..30. Orc policy retains its captured motion;
// Human policy supports legacy player0 as well as NPCs without FP arm banks.
WorldCombatMotion *world_combat_motion_load_compact(const char *body_directory,const char *anatomy_directory,const char *packet_file,unsigned actor_mask,bool opponent_policy,char *error,std::size_t capacity,unsigned max_texture_edge=0);
struct WorldCombatCompactStats {bool loaded=false,first_person_ready=false;unsigned actor_mask=0,logical_poses=0,resident_body_assets=0,rigid_pairs=0;unsigned long long mapped_bytes=0,staging_bytes=0,gpu_bytes=0,uploads=0,failures=0,base_geometry_bytes=0,cap_geometry_bytes=0,anatomy_gpu_bytes=0,death_gpu_bytes=0;unsigned packet_poses=0;};
WorldCombatCompactStats world_combat_motion_compact_stats(const WorldCombatMotion *);
// Legacy shared bank can disable arena NPC ownership while retaining player0.
// Compact masks are immutable: changing them requires a fresh loading boundary.
bool world_combat_motion_set_actor_mask(WorldCombatMotion *,unsigned actor_mask);
unsigned world_combat_motion_actor_mask(const WorldCombatMotion *);

struct WorldCombatOpponentBankStats {bool body_only=false,ready=false;unsigned body_poses=0,arm_poses=0,measured_pivots=0,certified_weapon_pairs=0;double walk_stride_metres=1.3,body_radius_metres=.34,body_height_metres=1.8;};
WorldCombatOpponentBankStats world_combat_motion_opponents_stats(const WorldCombatMotion *);
// Opponent-only distance per complete captured walk cycle. Human player and
// the original shared knight bank retain their unchanged1.3m sampling.
bool world_combat_motion_set_opponent_stride(WorldCombatMotion *,double metres);
// Clear run-owned poses, wounds, crowd and detached parts before a fresh arena
// run. Keeps loaded assets, certification and cumulative telemetry; no loading
// or GL allocation. Also valid for the original human bank between species.
void world_combat_motion_reset_arena_opponents(WorldCombatMotion *);
// Optional loading-boundary unarmed bank: same owned hero, dedicated cyclic
// tread/surface-stroke/dive skeletal poses, including first-person arms.
bool world_combat_motion_load_aquatics(WorldCombatMotion *,const char *directory,char *error,std::size_t capacity);
// Call AFTER the ordinary update, with real gameplay movement/water state.
// Animation only: never moves the physics body or changes gameplay controls.
void world_combat_motion_aquatics(WorldCombatMotion *,bool swimming,bool underwater,bool moving,float dt,bool paused=false);
void world_combat_motion_arena(WorldCombatMotion *,double origin_x,double origin_z,float feet);
// Explicit arena-only behavior; ordinary worlds retain their existing poses.
void world_combat_motion_arena_enemy_walk(WorldCombatMotion *,bool walking);
// Restore an already-earned defeated actor directly to its settled death pose.
// Does not launch heads, count a kill or generate a combat contact.
void world_combat_motion_restore_defeated(WorldCombatMotion *,unsigned actor);
struct WorldCombatCrowdInstance {double x=0,y=0,z=0;float yaw=0;bool yielded=false;float defeat_phase=0;bool decapitated=false;std::uint16_t severed_parts=0;TrainingWeaponHand weapon_hand=TrainingWeaponHand::Right;unsigned actor_id=0;};
// Loading-boundary allocation, then bounded uploads sharing the already loaded
// idle/collapse pose VBOs and texture references. At most30 additional combatants.
bool world_combat_motion_prepare_crowd(WorldCombatMotion *);
bool world_combat_motion_crowd(WorldCombatMotion *,const WorldCombatCrowdInstance *,unsigned count);
void world_combat_motion_draw_crowd(WorldCombatMotion *,unsigned program,double eye_x,double eye_y,double eye_z,bool shadow=false);
// A newly published body/weapon contact commits once before its hit-stop;
// subsequent stopped frames retain the committed pose and upload buffers.
void world_combat_motion_update(WorldCombatMotion *,const WorldTrainingCombat &,double x,double feet,double z,float yaw,float dt,bool show_enemy,bool paused=false);
// Stable slots1..30 belong to arena NPCs0..29 (24 enemies, six allies). These are read-only pose
// snapshots; only the arena owns simulation and the shared player clock.
struct WorldCombatArenaActor {WorldTrainingCombat combat;double x=0,y=0,z=0;bool visible=false,walking=false;bool facing_target_valid=false;double facing_target_x=0,facing_target_z=0;};
void world_combat_motion_update_arena(WorldCombatMotion *,const WorldTrainingCombat &,double x,double feet,double z,float yaw,float dt,const WorldCombatArenaActor *,unsigned count,bool paused=false);
void world_combat_motion_draw(WorldCombatMotion *,unsigned program,double x,double y,double z,bool shadow,bool show_player=true);
// Actual authored Bow TP/FP poses in an appearance packet. Uses the same
// source-hand sockets and world placement as the ordinary arena Bow renderer.
bool world_combat_motion_bow_pose(WorldCombatMotion *,const WorldArenaBowPose &);
void world_combat_motion_draw_bow_actor(WorldCombatMotion *,unsigned program,double eye_x,double eye_y,double eye_z,bool first_person,bool shadow=false);
// Optional owned, unarmed idle/walk bank. Allocated only during loading.
bool world_combat_motion_load_loadouts(WorldCombatMotion *,const char *directory);
// Loading only, after loadouts and unarmed. Signed, speed-matched cardinal leg
// templates share the existing complete body draw; no new texture dependency.
bool world_combat_motion_load_locomotion(WorldCombatMotion *,const char *directory,char *error,std::size_t capacity);
bool world_combat_motion_loadouts_ready(const WorldCombatMotion *);
unsigned world_combat_motion_loadout(const WorldCombatMotion *);
bool world_combat_motion_load_unarmed(WorldCombatMotion *,const char *);
bool world_combat_motion_load_fatalities(WorldCombatMotion *,const char *);
struct WorldCombatFatalityStats {unsigned launches=0,heads=0,headless=0,peak=0,settled=0;};
WorldCombatFatalityStats world_combat_motion_fatality_stats(const WorldCombatMotion *);
void world_combat_motion_fatality_ground(WorldCombatMotion *,double (*)(double,double,void*),void *);
bool world_combat_motion_neck_emitter(const WorldCombatMotion *,unsigned actor,double point[3],double axis[3],float collapse=-1,float yaw=0);
bool world_combat_motion_neck(const WorldCombatMotion *,unsigned actor,double point[3]);
bool world_combat_motion_unarmed_ready(const WorldCombatMotion *);
void world_combat_motion_unarmed(WorldCombatMotion *,bool,float,float);
void world_combat_motion_draw_arms(WorldCombatMotion *,unsigned program);
void world_combat_motion_equipment(WorldCombatMotion *,unsigned mask);
unsigned world_combat_motion_equipment(const WorldCombatMotion *);
WorldCombatMotionStats world_combat_motion_stats(const WorldCombatMotion *);
void world_combat_motion_free(WorldCombatMotion *);
#endif
