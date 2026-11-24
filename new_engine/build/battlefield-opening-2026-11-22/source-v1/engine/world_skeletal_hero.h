#ifndef VALWOOD_CANDIDATE_SKELETAL_HERO_H
#define VALWOOD_CANDIDATE_SKELETAL_HERO_H
#include "engine/world_animation_render.h"
#include "engine/world_training_combat.h"
#include "engine/world_equipment_presentation.h"
#include "engine/world_story_body_pose.h"
#include "engine/world_character_face.h"
#include <array>
// Isolated third-person longsword candidate. Physics and combat clocks remain
// owned by gameplay. Animation phase follows resolved travel, never input wish.
class WorldSkeletalHero {
public:
 struct Support {double height=0;float nx=0,ny=1,nz=0;};
 using GroundQuery=bool (*)(double x,double probe_y,double z,Support &,void *);
 struct Input {
  double x=0,y=0,z=0,dx=0,dz=0,dt=0;float yaw=0;
  bool paused=false,reset=false,grounded=true;
  bool equipment_stowed=false;
  bool swimming=false,submerged=false,dead=false;
  GroundQuery ground_query=nullptr;void *ground_context=nullptr;
  std::uint64_t reaction_serial=0;
  unsigned reaction_direction=2;
  bool guard_target_valid=false;
  unsigned guard_target_direction=2;
  std::array<double,3> guard_target_world={};
 };
 struct Stats {
  std::uint64_t updates=0,uploads=0,draws=0,failures=0,attacks=0,hitstop_frames=0,grip_solves=0,foot_plants=0,foot_releases=0,foot_reach_releases=0,body_reactions=0,clash_reactions=0,turn_steps=0;
  double gait_cycles=0;float speed=0,run=0,locomotion=0,guard=0,reaction=0,guard_recoil=0,attack_legs=0,maximum_grip_error_m=0,maximum_clavicle_degrees=0;
  std::array<float,2> foot_weight={},foot_residual_m={};
  std::array<bool,2> foot_planted={};
  float maximum_foot_residual_m=0,maximum_foot_correction_m=0,turn_weight=0,turn_progress=0,turn_debt_radians=0;
  float momentum_scale=0,momentum_seconds=0;
  std::uint64_t steel_rebounds=0;
  float steel_recoil=0,steel_contact_phase=0,steel_source_phase=0,steel_retreat_phase=0;
  float guard_adjustment_m=0,guard_target_residual_m=0;
  bool ready=false;
  bool equipment_transition=false,equipment_weapon_visible=true;
  float equipment_progress=0;
  bool swimming=false,submerged=false,dead=false,special_transition=false;
  unsigned special_mode=0;float special_progress=1;
  std::uint64_t swim_frames=0,submerged_frames=0,death_frames=0,special_entries=0,special_exits=0,special_mode_changes=0;
  float relaxed_walk_weight=0;
  std::uint64_t relaxed_walk_frames=0;
  bool full_unarmed=false,full_unarmed_transition=false;
  float full_unarmed_weight=0,full_unarmed_run_blend=0;
  double full_unarmed_cycles=0,unarmed_walk_stride_m=0,unarmed_run_stride_m=0;
  std::uint64_t full_unarmed_idle_frames=0,full_unarmed_walk_frames=0,full_unarmed_run_frames=0;
  unsigned captured_motion_mode=0,captured_motion_clip=0;
  float captured_motion_time=0,captured_motion_duration=0,captured_motion_acceptance=1;
  std::uint64_t captured_starts=0,captured_stops=0,captured_turns=0,captured_pivots=0,captured_blocked=0;
 };
 // Captured locomotion plans are requests to the existing physics sweeps.
 // update_cpu commits their accepted displacement; poses never move physics.
 struct MotionRequest {double dx=0,dz=0;float facing_yaw=0;bool captured=false;};
 bool plan_unarmed_motion(double wish_vx,double wish_vz,float current_yaw,double dt,
                          bool enabled,bool paused,MotionRequest &,std::string &);
 bool supports_unarmed_transitions()const{return unarmed_motion_available_;}
 struct CpuLoadStats {
  double parse_ms=0,setup_ms=0,arc_ms=0,total_ms=0;
  unsigned generated_arc_samples=0;
  bool arc_cache_reused=false;
 };
 ~WorldSkeletalHero();
 // An optional live controller can supply its combat table only after exact
 // skeleton, clip, held-geometry and weapon-context equality. The rig is still
 // parsed/validated and all other loading-boundary setup runs normally.
 bool load_cpu(const char *,std::string &,const char *clip_prefix=nullptr,const WorldSkeletalHero *arc_source=nullptr);
 // Loading boundary only; this instance must not own GPU buffers yet.
 bool select_appearance(const world_animation::Asset &,const std::vector<unsigned> &first_person,std::string &);
 const std::vector<unsigned> &first_person_primitives()const{return first_person_primitives_;}
 unsigned first_person_body_parts()const{return first_person_body_parts_;}
 unsigned held_primitives_begin()const{return held_primitives_begin_;}
 bool supports_equipment()const{return equipment_clips_available_;}
 bool equipment_combat_ready()const{return !story_hidden_&&equipment_.combat_ready();}
 bool equipment_weapon_visible()const{return !story_hidden_&&equipment_.weapon_visible();}
 bool supports_swimming()const{return story_swim_clips_;}
 bool supports_death()const{return story_death_clip_;}
 bool supports_relaxed_walk()const{return relaxed_walk_clip_available_;}
 bool supports_full_unarmed_locomotion()const{return full_unarmed_clips_;}
 bool supports_full_unarmed_run()const{return full_unarmed_run_;}
 bool full_unarmed_settled()const{return full_unarmed_body_active_&&equipment_stowed_settled();}
 bool equipment_stowed_settled()const{return !equipment_.combat_ready()&&!equipment_.transitioning();}
 bool prepare(unsigned color,unsigned shadow,std::uint64_t epoch,std::string &);
 bool update_cpu(const Input &,const WorldTrainingCombat &,std::string &);
 bool upload(std::string &);
 bool draw(unsigned program,std::string &,const std::array<float,3> &wear={});
 // Registers an additional skinned program (e.g. motion vectors) for draw().
 bool warm_extra(unsigned program,std::string &);
 // Last frame's palette for motion vectors; identity/uploads detect swaps.
 unsigned motion_previous_palette(const void *&identity,std::uint64_t &uploads,int *offset=nullptr)const{identity=gpu_;uploads=gpu_?world_animation_render::stats(gpu_).uploads:0;if(offset)*offset=gpu_?world_animation_render::previous_palette_offset(gpu_):0;return gpu_?world_animation_render::previous_palette(gpu_):0u;}
 void release();
 bool ready()const{return gpu_&&have_pose_;}
 bool supports_guards()const{return guard_clips_;}
 bool supports_reactions()const{return reaction_clips_;}
 const world_animation::Asset &asset()const{return asset_;}
 const world_animation::Pose &pose()const{return pose_;}
 const Stats &stats()const{return stats_;}
 const CpuLoadStats &cpu_load_stats()const{return cpu_load_stats_;}
 double x()const{return x_;} double z()const{return z_;}
 bool blade(double base[3],double tip[3])const;
 bool foot_support(unsigned side,unsigned &marker,double anchor[3],float &weight,bool &planted)const{
  if(side>=2)return false;
  const auto&foot=feet_[side];marker=foot.marker;for(unsigned n=0;n<3;++n)anchor[n]=foot.anchor[n];weight=foot.weight;planted=foot.planted;return true;
 }
 // Canonical combat clock: windup [0,.36], strike [.36,.60], recovery [.60,1].
 // Uses the same authored samples, root stripping and active visual layers as
 // update_cpu; it does not infer a weapon from a prior rendered frame.
 bool contact_blade(unsigned direction,double canonical_phase,double x,double y,
                    double z,float yaw,double base[3],double tip[3],std::string &,double windup_seconds=0,std::uint64_t attack_serial=0,double strike_seconds=0,double recovery_seconds=0,double attack_commit_clock=-1)const;
 // The current CPU pose is authoritative at zero lookahead. Prediction uses
 // the same authored guard, held-grip solve and bounded input filters as the
 // next visual update; requesting another angle never teleports a guard.
 bool guard_blade(unsigned direction,double lookahead_seconds,double x,double y,
                  double z,float yaw,double base[3],double tip[3],std::string &)const;
 bool guard_shield(unsigned direction,double lookahead_seconds,double x,double y,
                   double z,float yaw,std::vector<std::array<double,9>> &,std::string &)const;
 bool body_capsules(double x,double y,double z,float yaw,std::vector<world_combat_anatomy::Capsule> &,std::string &,unsigned guard_direction=~0u,double lookahead_seconds=0)const;
 bool guard_target_preview(unsigned direction,const double *target_world,double x,double y,double z,float yaw,double base[3],double tip[3],std::vector<world_combat_anatomy::Capsule> &,std::string &,std::vector<std::array<double,9>> *surfaces=nullptr)const;
 bool guard_target_preview(unsigned direction,const std::array<double,3>& target,double x,double y,double z,float yaw,double base[3],double tip[3],std::vector<world_combat_anatomy::Capsule>& body,std::string& e,std::vector<std::array<double,9>> *surfaces=nullptr)const{return guard_target_preview(direction,target.data(),x,y,z,yaw,base,tip,body,e,surfaces);}
private:
 world_character_face::State face_;
 world_story_body_pose::State story_pose_;
 world_animation::Pose story_entry_;
 std::array<unsigned,3> story_clips_={};
 bool story_swim_clips_=false,story_death_clip_=false,story_have_entry_=false,story_hidden_=false;
 bool equipment_pose(world_animation::Pose &,const Input &,double,std::string &);
 world_equipment_presentation::State equipment_;
 world_animation::Pose equipment_entry_;
 std::array<unsigned,3> equipment_clips_={};
 bool equipment_clips_available_=false;
 unsigned relaxed_walk_clip_=0;
 bool relaxed_walk_clip_available_=false;
 std::array<world_animation::Quat,world_animation::node_limit> relaxed_walk_rotations_={};
 bool have_relaxed_walk_rotations_=false;
 std::array<unsigned,3> full_unarmed_clip_={};
 bool full_unarmed_clips_=false,full_unarmed_run_=false,full_unarmed_body_active_=false;
 double full_unarmed_cycles_=0,full_unarmed_run_weight_=0;
 std::array<double,3> full_unarmed_floor_={};
 double quiet_pelvis_lowering_=0;
 struct UnarmedMotionCapture {
  unsigned clip=0;
  // Native source time, world-horizontal Root X/Z, unwrapped yaw, path length.
  std::vector<std::array<double,5>> trajectory;
  double floor=0,begin_speed=0,end_speed=0,angle=0;
  std::array<double,2> exit_phase={};
  std::array<std::array<float,128>,2> support={};
 };
 world_animation::Asset unarmed_motion_asset_;
 std::array<UnarmedMotionCapture,22> unarmed_motion_captures_={};
 unsigned unarmed_motion_root_=0,motion_mode_=0,motion_capture_=0;
 bool unarmed_motion_available_=false,motion_enabled_=false,motion_planned_=false,motion_have_heading_=false,motion_blocked_=false;
 double unarmed_motion_time_=0,motion_next_time_=0,motion_plan_dt_=0,motion_plan_remaining_=0,motion_rate_=1,motion_heading_=0,motion_facing_=0,motion_angle_scale_=1,motion_root_scale_=1;
 double motion_speed_cap_=0,motion_last_wish_speed_=0,motion_wish_yaw_=0,motion_blocked_time_=0,motion_acceptance_=1,motion_bridge_age_=.2,motion_capture_age_=0,motion_actual_speed_=0,motion_blend_duration_=.12,motion_bridge_duration_=.12;
 float motion_support_override_=0;
 double motion_entry_yaw_=0,motion_plan_x_=0,motion_plan_z_=0;
 struct UnarmedPoseVelocity {
  std::array<world_animation::Vec3,world_animation::node_limit> translation={},rotation={};
 };
 UnarmedPoseVelocity motion_entry_velocity_,motion_bridge_velocity_;
 world_animation::Pose motion_entry_pose_,motion_bridge_pose_,motion_previous_pose_;
 double motion_previous_dt_=0,motion_previous_yaw_=0,motion_pose_yaw_=0;
 bool motion_bridge_pending_=false;
 bool unarmed_pose_velocity(const world_animation::Pose &,const world_animation::Pose &,double,double,double,UnarmedPoseVelocity &,std::string &)const;
 bool continue_unarmed_pose(world_animation::Pose &,const UnarmedPoseVelocity &,double,double,std::string &)const;
 std::array<float,2> motion_entry_support_={},motion_current_support_={};
 bool load_unarmed_motion(const char *,std::string &);
 bool initialize_unarmed_motion_feet(std::string &);
 bool sample_unarmed_motion(unsigned,double,world_animation::Pose &,std::string &,bool strip=true)const;
 std::array<double,5> unarmed_trajectory(unsigned,double)const;
 bool unarmed_motion_pose(world_animation::Pose &,const Input &,double,std::string &);
 void reset_unarmed_motion();
 unsigned choose_unarmed_motion(unsigned,unsigned,std::string &)const;
 bool begin_unarmed_motion(unsigned,unsigned,double,double,std::string &);
 void commit_unarmed_motion(const Input &,double);

 std::array<double,2> full_unarmed_stride_={1,1};
 std::array<std::array<std::array<float,128>,2>,3> full_unarmed_support_={};
 bool initialize_unarmed_feet(std::string &);
 bool sample_unarmed_loop(world_animation::Pose &,std::string &)const;
 void release_stance_anchors();
 std::vector<unsigned> first_person_primitives_;
 std::vector<unsigned> guard_surface_held_offsets_;
 unsigned held_primitives_begin_=19;
 unsigned first_person_body_parts_=0;
 struct FootSupport {
  unsigned hip=0,knee=0,ankle=0,ball=0;std::array<unsigned,2> sole={};std::vector<unsigned> vertices;
  std::array<double,3> anchor={};world_animation::Vec3 previous;
  float weight=0;double contact_age=0,cooldown=0,toe_off=0;unsigned marker=0;
  bool planted=false,releasing=false,history=false;
 };
 struct LocomotionCapture {unsigned clip=0;std::vector<double> distance;};
 bool initialize_captures(std::string &);
 bool lower_locomotion(world_animation::Pose &,const Input &,double actual,double dt,std::string &);
 bool turn_lower_body(world_animation::Pose &,const Input &,double dt,bool allowed,std::string &);
 bool yaw_legs(world_animation::Pose &,double radians,std::string &)const;
 bool copy_legs(const world_animation::Pose &,float,world_animation::Pose &,std::string &)const;
 std::array<LocomotionCapture,12> locomotion_captures_={};
 std::array<unsigned,4> turn_clips_={};
 std::array<world_animation::Quat,4> turn_reference_rotation_={};
 world_animation::Pose turn_entry_;
 bool turn_clips_available_=false;unsigned turn_mode_=0,turn_capture_=0;
 double turn_reference_yaw_=0,turn_last_yaw_=0,turn_entry_yaw_=0,turn_time_=0,turn_duration_=0,turn_goal_angle_=0,turn_source_progress_=0;
 std::array<bool,world_animation::node_limit> leg_mask_={};
 world_animation::Pose locomotion_previous_,locomotion_entry_;
 bool capture_clips_=false,have_locomotion_pose_=false;
 unsigned locomotion_mode_=0,locomotion_capture_=0,locomotion_free_foot_=1;
 double locomotion_distance_=0,locomotion_time_=0,locomotion_entry_time_=0,locomotion_duration_=0,gait_offset_=0,previous_actual_=0;
 std::array<float,2> transition_contact_={-1,-1};
 bool initialize_feet(std::string &);
 bool sole_point(const world_animation::Pose &,unsigned foot,unsigned marker,world_animation::Vec3 &,std::string &)const;
 bool support_feet(world_animation::Pose &,const Input &,double dt,bool allowed,std::string &,const float *attack_contact=nullptr);
 bool attack_foot_contacts(const world_animation::Pose &,const WorldTrainingCombat &,std::array<float,2>&,std::string &)const;
 bool solve_leg(world_animation::Pose &,const FootSupport &,world_animation::Vec3,world_animation::Quat,std::string &)const;
 std::array<FootSupport,2> feet_={};
 std::array<std::array<std::array<float,128>,2>,10> support_phase_={};
 struct GripSocket {world_animation::Vec3 wrist,pole;world_animation::Quat rotation;float held=1;};
 unsigned grip_profile_=0; // 0: Sword hilt, 1: independent shield, 2: two-handed shaft.
 bool grip_socket(const world_animation::Pose &,GripSocket &,std::string &)const;
 static GripSocket mix_grips(const GripSocket &,const GripSocket &,float);
 bool solve_arm(world_animation::Pose &,unsigned clavicle,unsigned upper,unsigned lower,unsigned hand,
                world_animation::Vec3,world_animation::Vec3,world_animation::Quat,float &correction,float &residual,std::string &)const;
 bool cartesian_right(const world_animation::Pose &,const world_animation::Pose &,float,world_animation::Pose &,std::string &,float *correction=nullptr)const;
 bool constrain_grip(world_animation::Pose &,const GripSocket &,std::string &);
 bool constrain_grip_pose(world_animation::Pose &,const GripSocket &,float &,float &,std::string &)const;
 bool sample_guard_pose(unsigned,double,world_animation::Pose &,std::string &)const;
 bool fit_guard_pose(world_animation::Pose &,unsigned,std::string &,float *adjustment=nullptr,float *residual=nullptr)const;
 bool fit_guard_target(world_animation::Pose &,unsigned,const std::array<double,3> &,double,double,double,float,std::string &,float *,float *)const;
 bool pose_body_capsules(const world_animation::Pose &,double,double,double,float,std::vector<world_combat_anatomy::Capsule> &,std::string &)const;
 bool pose_guard_surfaces(const world_animation::Pose &,double,double,double,float,std::vector<std::array<double,9>> &,std::string &)const;
 bool travel_legs(world_animation::Pose &,float,std::string &)const;
 bool stationary_cut_legs(unsigned direction,double phase,const world_animation::Pose &,world_animation::Pose &,std::string &)const;
 bool sample_attack(unsigned direction,double source_frame,world_animation::Pose &,std::string &)const;
 // A time-window fit, not a previous-frame difference, carries the captured
 // upper body's angular momentum across a buffered serial. The compact tail
 // reaches exact zero before every physical contact window.
 static constexpr unsigned momentum_joint_count=11,momentum_history_limit=64;
 struct MotionSample {double time=0,clock=-1;std::array<world_animation::Quat,momentum_joint_count> rotations={};};
 struct Momentum {std::array<world_animation::Vec3,momentum_joint_count> angular={};double duration=0,entry_elapsed=0;float scale=0;};
 std::array<unsigned,momentum_joint_count> momentum_joints_={};
 std::array<float,momentum_joint_count> wrist_radius_={},elbow_radius_={};
 std::array<MotionSample,momentum_history_limit> motion_history_={};
 unsigned motion_count_=0,motion_next_=0;double motion_time_=0,motion_clock_interval_=0;Momentum attack_momentum_;
 void remember_motion(const world_animation::Pose &,double dt,double clock);
 Momentum momentum_snapshot(double windup_seconds,double attack_commit_clock=-1)const;
 bool apply_momentum(world_animation::Pose &,const Momentum &,double seconds,std::string &)const;
 bool attack_pose(unsigned direction,double canonical_phase,world_animation::Pose &,std::string &,double windup_seconds=0,GripSocket *target=nullptr,float *right_correction=nullptr,const world_animation::Pose *entry_override=nullptr,double strike_seconds=0,double recovery_seconds=0,const Momentum *momentum_override=nullptr)const;
 bool apply_reaction(world_animation::Pose &,std::string &,
                     const std::array<world_animation::Quat,world_animation::node_limit> *offset=nullptr)const;
 bool begin_steel_recoil(unsigned,double,const WorldTrainingCombat &,std::string &);
 bool sample_steel_pose(double,world_animation::Pose &,std::string &,world_animation::Pose *unreacted=nullptr)const;
 bool steel_recoil_pose(world_animation::Pose &,double,std::string &);
 bool blade_pose(const world_animation::Pose &,double,double,double,float,double[3],double[3])const;
 bool upper_body(const world_animation::Pose &,const world_animation::Pose &,float,
                 world_animation::Pose &,std::string &)const;
 world_animation::Asset asset_;
 GripSocket guard_socket_,guard_blend_socket_;
 world_animation::Pose pose_,unreacted_,entry_,rest_,base_,guard_base_,guard_pose_,reaction_pose_;
 world_animation::Pose steel_contact_,steel_retreat_,steel_unreacted_,resolved_query_entry_;
 Momentum resolved_query_momentum_;
 bool resolved_query_valid_=false;
 std::array<world_animation::Quat,world_animation::node_limit> resolved_query_offset_={};
 std::uint64_t resolved_seen_=0,steel_serial_=0;
 double steel_time_=0,feedback_clock_=-1;
 unsigned steel_direction_=0;
 double steel_contact_phase_=.36,steel_retreat_phase_=.29,steel_source_phase_=0;
 std::array<double,3> steel_timing_={.18,.09,.24};
 // Small per-contact arc table; reused throughout the fixed recovery window.
 std::array<double,33> steel_rewind_arc_={};
 world_animation_render::Renderer *gpu_=nullptr;
 // Each authored interval owns440 samples, including exact contact/recovery
 // keys. Distance retiming retains the source path at the gameplay clock.
 std::array<std::array<double,1321>,5> attack_arcs_={};
 CpuLoadStats cpu_load_stats_;
 std::array<unsigned,15> clips_={};std::array<unsigned,5> guards_={},reactions_={};
 std::array<bool,world_animation::node_limit> upper_mask_={};
 // Source torso deltas preserve each active attack's arm/finger capture. An
 // impulse retriggers from its current additive offset instead of dropping it.
 std::array<bool,world_animation::node_limit> reaction_mask_={};
 std::array<world_animation::Quat,world_animation::node_limit> reaction_reference_={},reaction_offset_={},reaction_entry_={};
 unsigned pelvis_=0,spine_=0,hand_=0,left_clavicle_=0,left_upper_=0,left_lower_=0,left_hand_=0,left_index_=0,left_ring_=0,right_clavicle_=0,right_upper_=0,right_lower_=0,right_hand_=0,chest_=0;std::uint64_t epoch_=0,attack_serial_=0,reaction_serial_=0,contact_token_=0;
 bool loaded_=false,have_pose_=false,was_attacking_=false,stationary_cut_adaptation_=true;
 bool guard_clips_=false,reaction_clips_=false,have_guard_pose_=false,reaction_clash_=false;
 bool guard_target_valid_=false;
 unsigned guard_target_direction_=2;
 std::array<double,3> guard_target_world_={};
 double x_=0,y_=0,z_=0,cycles_=0,idle_time_=0,heading_=0,exit_time_=1,reaction_time_=1;
 double windup_duration_=.18,strike_duration_=.09,recovery_duration_=.24;
 double reaction_duration_=.60;float reaction_strength_=1;std::uint64_t clash_token_=0;
 float yaw_=0,speed_=0,run_=0,locomotion_=0,guard_weight_=0,reaction_weight_=0,attack_legs_weight_=0,attack_legs_goal_=0,attack_legs_rate_=0;
 unsigned reaction_direction_=2;std::array<float,8> direction_={1,0,0,0,0,0,0,0};Stats stats_;
};
#endif
