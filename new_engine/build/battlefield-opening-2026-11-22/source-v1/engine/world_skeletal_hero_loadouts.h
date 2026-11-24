#ifndef VALWOOD_SKELETAL_HERO_LOADOUTS_H
#define VALWOOD_SKELETAL_HERO_LOADOUTS_H
#include "engine/world_skeletal_hero.h"
#include "engine/world_swim_viewmodel.h"
#include <memory>

// One purchased body/rig, compact weapon-specific captured clip families. The
// selected weapon is explicit for contact queries, never inferred from a GPU
// pose left behind by a previous loadout. Only the active controller advances.
class WorldSkeletalHeroLoadouts {
public:
 using Input=WorldSkeletalHero::Input;
 using Support=WorldSkeletalHero::Support;
 using MotionRequest=WorldSkeletalHero::MotionRequest;
 bool plan_unarmed_motion(double vx,double vz,float yaw,double dt,bool enabled,bool paused,MotionRequest &out,std::string &error){
  if(!enabled){for(auto&hero:heroes_)if(hero){MotionRequest ignored;if(!hero->plan_unarmed_motion(vx,vz,yaw,dt,false,paused,ignored,error))return false;}out={};out.facing_yaw=yaw;return true;}
  return active().plan_unarmed_motion(vx,vz,yaw,dt,true,paused,out,error);
 }
 bool supports_unarmed_transitions()const{return active().supports_unarmed_transitions();}
 using Stats=WorldSkeletalHero::Stats;
 using Weapon=world_weapon_loadout::Weapon;
 WorldSkeletalHeroLoadouts();
 ~WorldSkeletalHeroLoadouts();
 void bind_weapon(const Weapon *weapon){weapon_=weapon;}
 void set_blood_wear(const std::array<float,3>&wear){blood_wear_=wear;}
 static bool supports_weapon(Weapon);
 bool load_cpu(const char *sword_path,std::string &error,const char *manifest_path=nullptr,const WorldSkeletalHeroLoadouts *arc_sources=nullptr);
 bool select_appearance(const char *appearance_config,std::string &error);
 bool prepare(unsigned color,unsigned shadow,std::uint64_t epoch,std::string &);
 bool prepare_first_person(unsigned program,std::uint64_t epoch,std::string &);
 bool update_cpu(const Input &,const WorldTrainingCombat &,std::string &);
 bool upload(std::string &);
 bool draw(unsigned program,std::string &);
 bool warm_extra(unsigned program,std::string &);
 unsigned motion_previous_palette(const void *&identity,std::uint64_t &uploads,int *offset=nullptr)const{if(!ready()){identity=nullptr;uploads=0;if(offset)*offset=0;return 0;}return active().motion_previous_palette(identity,uploads,offset);}
 bool draw_first_person(unsigned program,std::string &);
 bool first_person_ready()const;
 void release();
 WorldSkeletalHero &active();
 const WorldSkeletalHero &active()const;
 bool ready()const;
 bool supports_guards()const{return active().supports_guards();}
 bool supports_reactions()const{return active().supports_reactions();}
 bool supports_equipment()const{return active().supports_equipment();}
 bool supports_swimming()const;
 bool supports_death()const;
 bool equipment_combat_ready()const{return active().equipment_combat_ready();}
 bool equipment_weapon_visible()const{return active().equipment_weapon_visible();}
 bool equipment_stowed_settled()const{return active().equipment_stowed_settled();}
 const world_animation::Asset &asset()const{return active().asset();}
 const world_animation::Pose &pose()const{return active().pose();}
 const Stats &stats()const;
 double x()const{return active().x();} double z()const{return active().z();}
 bool blade(double base[3],double tip[3])const{return ready()&&equipment_combat_ready()&&active().blade(base,tip);}
 bool contact_blade(unsigned direction,double canonical_phase,double x,double y,double z,float yaw,double base[3],double tip[3],std::string &,double windup_seconds=0,std::uint64_t attack_serial=0,double strike_seconds=0,double recovery_seconds=0,double attack_commit_clock=-1,Weapon weapon=Weapon::Sword)const;
 bool guard_blade(unsigned direction,double lookahead_seconds,double x,double y,double z,float yaw,double base[3],double tip[3],std::string &,Weapon weapon=Weapon::Sword)const;
 bool guard_shield(unsigned direction,double lookahead_seconds,double x,double y,double z,float yaw,std::vector<std::array<double,9>> &,std::string &,Weapon weapon=Weapon::Sword)const;
 bool body_capsules(double x,double y,double z,float yaw,std::vector<world_combat_anatomy::Capsule> &,std::string &,unsigned guard_direction=~0u,double lookahead_seconds=0)const;
 bool guard_target_preview(unsigned direction,const double *target_world,double x,double y,double z,float yaw,double base[3],double tip[3],std::vector<world_combat_anatomy::Capsule> &,std::string &,Weapon weapon=Weapon::Sword,std::vector<std::array<double,9>> *surfaces=nullptr)const;
 bool guard_target_preview(unsigned direction,const std::array<double,3>& target,double x,double y,double z,float yaw,double base[3],double tip[3],std::vector<world_combat_anatomy::Capsule>& body,std::string& e,Weapon weapon=Weapon::Sword,std::vector<std::array<double,9>> *surfaces=nullptr)const{return guard_target_preview(direction,target.data(),x,y,z,yaw,base,tip,body,e,weapon,surfaces);}
private:
 std::string source_sword_,source_manifest_,appearance_config_;
 std::array<float,3> blood_wear_={};
 unsigned prepared_color_=0,prepared_shadow_=0;
 std::uint64_t prepared_epoch_=0;
 static unsigned slot(Weapon);
 unsigned selected()const;
 void cache_swim_head_joints();
 std::array<std::unique_ptr<WorldSkeletalHero>,4> heroes_;
 std::array<world_animation_render::Renderer*,4> first_person_={};
 std::array<std::array<unsigned,2>,4> first_person_held_={};
 const Weapon *weapon_=nullptr;
 unsigned last_=4;bool loaded_=false;
 std::array<bool,4> seeded_={};
 std::uint64_t first_person_epoch_=0;
 unsigned first_person_program_=0;
 world_swim_viewmodel::State swim_viewmodel_;
 std::array<unsigned,4> swim_head_joints_={{~0u,~0u,~0u,~0u}};
 mutable Stats aggregate_;
};
#endif
