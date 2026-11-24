#ifndef VALWOOD_WORLD_BATTLEFIELD_ACTORS_H
#define VALWOOD_WORLD_BATTLEFIELD_ACTORS_H
#include "engine/world_animation.h"
#include "engine/world_animation_render.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

// A synchronized two-body performance. Gameplay owns QTE outcomes/checkpoints;
// this class owns articulation, contact sockets and exact visible trajectories.
// All roles use the production material, shadow and previous-palette renderer.
class WorldBattlefieldActors {
public:
 enum class Role : unsigned {Player,Attacker};
 enum class Beat : unsigned {Arrival,Dazed,Rise,Ambush,Duck,Tackle,Struggle,Disarm,Stab,Recover,Death,Hidden,Aftermath};
 enum class Motion : unsigned {Idle,Walk,Run,AttackLeft,AttackRight,Guard,Death,Dead,Archer};
 struct Input {
  Beat beat=Beat::Hidden;double seconds=0,progress=0,dt=0;
  // A=.5 means neutral struggle; greater values draw the sword toward player.
  double struggle=.5;double x=0,y=0,z=0;float yaw=0;bool reset=false;
 };
 struct Background {double x=0,y=0,z=0;float yaw=0,scale=1;Motion motion=Motion::Idle;double progress=0;
  // TrainingBodyPart bit(part-1), persisted by gameplay after defeat. Applies
  // to this instance's actual canonical joint subtrees, including accessories.
  std::uint16_t severed_parts=0;
 };
 struct Anchors {
  std::array<double,3> head={},left_hand={},right_hand={},chest={},pelvis={},blade_base={},blade_tip={};
  bool visible=false,weapon_visible=false;
 };
 struct Stats {std::uint64_t samples=0,uploads=0,draws=0,failures=0;float max_hand_error_m=0;unsigned background_count=0;bool ready=false;};
 WorldBattlefieldActors();~WorldBattlefieldActors();
 WorldBattlefieldActors(const WorldBattlefieldActors&)=delete;
 WorldBattlefieldActors&operator=(const WorldBattlefieldActors&)=delete;
 // Body paths are optional GLB appearances on the existing canonical bind.
 bool load_cpu(const char *rig_path,const char *capture_path,const char *player_body_path,const char *knight_body_path,std::string &);
 bool prepare(unsigned color,unsigned shadow,std::uint64_t epoch,std::string &);
 bool warm_extra(unsigned program,std::string &);
 bool update_cpu(const Input &,std::string &);
 bool update_background(const Background *,unsigned count,std::string &);
 bool upload(const world_animation_render::Bindings &,std::string &);
 bool draw(Role,unsigned program,const world_animation_render::Bindings &,std::string &);
 bool draw_background(unsigned program,const world_animation_render::Bindings &,std::string &);
 bool draw_archers(unsigned program,const world_animation_render::Bindings &,std::string &);
 const world_animation::Pose &pose(Role)const;
 const world_animation::Asset &asset(Role)const;
 const world_animation_render::Instance &instance(Role)const;
 const Anchors &anchors(Role)const;
 const Stats &stats()const;
 const Anchors &background_anchors(unsigned logical_index)const;
 // Extras encode each actor's complete root transform in its skin palette,
 // relative to this fixed origin. Previous palettes then include translation
 // and yaw as well as limbs without borrowing one delta for the whole group.
 const std::array<double,3> &background_origin()const;
 // identity + uploads route existing NPC motion-vector plumbing without
 // confusing a previous actor's skin palette with the current performance.
 unsigned previous_palette(Role,const void *&identity,std::uint64_t &uploads,int *offset=nullptr)const;
 unsigned background_previous_palette(const void *&identity,std::uint64_t &uploads,int *offset=nullptr)const;
 unsigned archer_previous_palette(const void *&identity,std::uint64_t &uploads,int *offset=nullptr)const;
 void release();
private:struct Impl;std::unique_ptr<Impl> impl_;
};
#endif
