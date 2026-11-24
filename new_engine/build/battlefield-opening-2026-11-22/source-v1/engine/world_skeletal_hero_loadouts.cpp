#include "engine/world_skeletal_hero_loadouts.h"
#include "engine/world_skeletal_hero_viewmodel.h"
#include "engine/world_knight_appearance.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
using Weapon=world_weapon_loadout::Weapon;
WorldSkeletalHeroLoadouts::WorldSkeletalHeroLoadouts(){for(auto &hero:heroes_)hero.reset(new WorldSkeletalHero);}
WorldSkeletalHeroLoadouts::~WorldSkeletalHeroLoadouts(){release();}
bool WorldSkeletalHeroLoadouts::supports_weapon(Weapon w){return w==Weapon::Sword||w==Weapon::SwordShield||w==Weapon::GreatAxe||w==Weapon::Staff;}
unsigned WorldSkeletalHeroLoadouts::slot(Weapon w){return w==Weapon::SwordShield?1:w==Weapon::GreatAxe?2:w==Weapon::Staff?3:0;}
unsigned WorldSkeletalHeroLoadouts::selected()const{return slot(weapon_?*weapon_:Weapon::Sword);}
WorldSkeletalHero &WorldSkeletalHeroLoadouts::active(){return *heroes_[selected()];}
const WorldSkeletalHero &WorldSkeletalHeroLoadouts::active()const{return *heroes_[selected()];}
void WorldSkeletalHeroLoadouts::cache_swim_head_joints(){
 swim_head_joints_.fill(~0u);
 for(unsigned s=0;s<heroes_.size();++s){const auto&joints=heroes_[s]->asset().skeleton.joints;
  for(unsigned j=0;j<joints.size();++j)if(joints[j].name=="head"){swim_head_joints_[s]=j;break;}
 }
}
bool WorldSkeletalHeroLoadouts::load_cpu(const char*sword,std::string&e,const char*manifest,const WorldSkeletalHeroLoadouts*arc_sources){
 if(loaded_){e="Skeletal loadouts already loaded";return false;}
 if(!manifest)manifest=std::getenv("VALWOOD_SKELETAL_LOADOUTS");
 if(!manifest||!*manifest){e="Missing VALWOOD_SKELETAL_LOADOUTS manifest";return false;}
 std::ifstream file(manifest);std::string signature;unsigned version=0;
 if(!(file>>signature>>version)||signature!="VALWOOD_SKELETAL_LOADOUTS"||version!=1){e="Invalid skeletal loadout manifest";return false;}
 constexpr const char*prefixes[]={"Shield_","Axe_","Staff_"};
 std::array<std::string,3> paths;
 for(unsigned i=0;i<3;++i){unsigned weapon=0;std::string prefix;if(!(file>>weapon>>std::quoted(paths[i])>>std::quoted(prefix))||weapon!=i+2||prefix!=prefixes[i]){e="Incomplete/out-of-order skeletal loadout manifest";return false;}}
 std::string extra;if(file>>extra){e="Unexpected skeletal loadout manifest fields";return false;}
 const bool reusable=arc_sources&&arc_sources->loaded_;
 if(!heroes_[0]->load_cpu(sword,e,nullptr,reusable?arc_sources->heroes_[0].get():nullptr))return false;
 for(unsigned i=0;i<3;++i)if(!heroes_[i+1]->load_cpu(paths[i].c_str(),e,prefixes[i],reusable?arc_sources->heroes_[i+1].get():nullptr)){e=std::string(prefixes[i])+e;return false;}
 cache_swim_head_joints();
 source_sword_=sword;source_manifest_=manifest;
 loaded_=true;
 std::cerr<<"[skeletal-loadouts] loaded4 purchased rigs Sword/Shield/Axe/Staff; shared embedded texture cache; no legacy body fallback\n";
 double parse_ms=0,setup_ms=0,arc_ms=0,total_ms=0;unsigned generated=0,reused=0;
 for(const auto&hero:heroes_){const auto&s=hero->cpu_load_stats();parse_ms+=s.parse_ms;setup_ms+=s.setup_ms;arc_ms+=s.arc_ms;total_ms+=s.total_ms;generated+=s.generated_arc_samples;reused+=s.arc_cache_reused;}
 std::cerr<<"[skeletal-loadouts-cpu] parse_ms="<<parse_ms<<" setup_ms="<<setup_ms<<" arcs_ms="<<arc_ms<<" total_ms="<<total_ms<<" generated_arc_samples="<<generated<<" reused_arc_tables="<<reused<<"/4\n";
 return true;
}
bool WorldSkeletalHeroLoadouts::select_appearance(const char*path,std::string&e){
 if(!loaded_){e="Load the equipped knight rigs first";return false;}
 if(path&&appearance_config_==path)return true;
 using Clock=std::chrono::steady_clock;const auto start=Clock::now();
 world_knight_appearance::Config config;world_animation::Asset body;
 if(!world_knight_appearance::load(path,config,body,e))return false;
 const auto body_end=Clock::now();
 // The original controllers remain usable until all four world meshes, arms
 // meshes, material uploads and shader warmups have succeeded in the staging
 // owner. Its destructor also releases partial resources on any failure.
 WorldSkeletalHeroLoadouts next;next.bind_weapon(weapon_);
 if(!next.load_cpu(source_sword_.c_str(),e,source_manifest_.c_str(),this))return false;
 const auto rigs_end=Clock::now();
 for(auto &hero:next.heroes_)if(!hero->select_appearance(body,config.first_person,e))return false;
 next.cache_swim_head_joints();
 const auto compose_end=Clock::now();
 if(prepared_color_&&!next.prepare(prepared_color_,prepared_shadow_,prepared_epoch_,e))return false;
 const auto world_end=Clock::now();
 if(first_person_program_&&!next.prepare_first_person(first_person_program_,first_person_epoch_,e))return false;
 const auto first_person_end=Clock::now();
 heroes_.swap(next.heroes_);first_person_.swap(next.first_person_);first_person_held_.swap(next.first_person_held_);
 swim_head_joints_.swap(next.swim_head_joints_);swim_viewmodel_.reset();
 seeded_.fill(false);last_=4;appearance_config_=path;
 auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
 std::cerr<<"[skeletal-appearance] body_ms="<<ms(start,body_end)<<" rigs_ms="<<ms(body_end,rigs_end)<<" compose_ms="<<ms(rigs_end,compose_end)<<" world_prepare_ms="<<ms(compose_end,world_end)<<" fp_prepare_ms="<<ms(world_end,first_person_end)<<" staged_total_ms="<<ms(start,first_person_end)<<"\n";
 e.clear();return true;
}
bool WorldSkeletalHeroLoadouts::supports_swimming()const{return loaded_&&std::all_of(heroes_.begin(),heroes_.end(),[](const auto &h){return h->supports_swimming();});}
bool WorldSkeletalHeroLoadouts::supports_death()const{return loaded_&&std::all_of(heroes_.begin(),heroes_.end(),[](const auto &h){return h->supports_death();});}
bool WorldSkeletalHeroLoadouts::ready()const{return loaded_&&supports_weapon(weapon_?*weapon_:Weapon::Sword)&&last_==selected()&&seeded_[selected()]&&active().ready();}
bool WorldSkeletalHeroLoadouts::prepare(unsigned color,unsigned shadow,std::uint64_t epoch,std::string&e){
 if(!loaded_){e="Skeletal loadouts not loaded";return false;}
 // Warm outside the draw path. Inactive controllers never sample/update/upload.
 for(auto &hero:heroes_)if(!hero->prepare(color,shadow,epoch,e))return false;
 prepared_color_=color;prepared_shadow_=shadow;prepared_epoch_=epoch;
 return true;
}
bool WorldSkeletalHeroLoadouts::prepare_first_person(unsigned program,std::uint64_t epoch,std::string&e){
 if(!loaded_||!program){e="Cannot prepare first-person loadouts";return false;}
 if(first_person_program_==program&&first_person_epoch_==epoch&&std::all_of(first_person_.begin(),first_person_.end(),[](auto*p){return p!=nullptr;}))return true;
 for(unsigned s=0;s<4;++s){
  if(!first_person_[s]){
   const auto subset=world_skeletal_hero_viewmodel::arms_subset(heroes_[s]->asset(),heroes_[s]->first_person_primitives(),heroes_[s]->first_person_body_parts());
   if(subset.mesh.primitives.empty()||subset.mesh.vertices.empty()){e="Empty first-person arms subset";return false;}
   unsigned position=0,begin=0,end=0;const auto&parts=heroes_[s]->first_person_primitives();const auto&asset=heroes_[s]->asset();
   for(unsigned n=0;n<asset.mesh.primitives.size();++n){const bool include=parts.empty()?(n==0||n==5||n==9||n>=19):std::find(parts.begin(),parts.end(),n)!=parts.end();if(!include||!asset.mesh.primitives[n].index_count)continue;if(n<heroes_[s]->held_primitives_begin())++begin;++position;}
   end=position;first_person_held_[s]={begin,end};
   first_person_[s]=world_animation_render::create(subset,1,e);
   if(!first_person_[s])return false;
  }
  if(!world_animation_render::warm(first_person_[s],program,e))return false;
 }
 first_person_program_=program;first_person_epoch_=epoch;return true;
}
bool WorldSkeletalHeroLoadouts::update_cpu(const Input&in,const WorldTrainingCombat&fight,std::string&e){
 if(!loaded_||!supports_weapon(fight.weapon)){e="Unsupported skeletal loadout";return false;}
 if(weapon_&&*weapon_!=fight.weapon){e="Skeletal loadout binding disagrees with committed weapon";return false;}
 const unsigned s=slot(fight.weapon);Input next=in;
 if(last_!=s){next.reset=true;seeded_[s]=false;last_=s;}
 if(!heroes_[s]->update_cpu(next,fight,e))return false;
 swim_viewmodel_.update(heroes_[s]->stats().swimming,next.dt,next.paused,next.reset);
 seeded_[s]=true;return true;
}
bool WorldSkeletalHeroLoadouts::upload(std::string&e){
 if(!ready()){e="Skeletal loadout has not been seeded";return false;}
 if(!active().upload(e))return false;
 if(first_person_[selected()]){
  // Grounded combat retains its exact trail-aligned cosmetic transform.
  // Swim arms register to the captured head, which moves forward during the
  // stroke. Only this FP instance changes; the world pose remains committed.
  world_animation::Vec3 head{0,world_skeletal_hero_viewmodel::camera_height,0};
  const auto joint=swim_head_joints_[selected()];const auto&pose=active().pose();
  if(swim_viewmodel_.weight()>0&&joint<pose.global.size()){
   const auto&m=pose.global[joint].m;head={m[12],m[13],m[14]};
  }
  const auto view=swim_viewmodel_.apply(world_skeletal_hero_viewmodel::instance(),head);
  world_animation_render::Bindings restore;
  if(!world_animation_render::upload(first_person_[selected()],&active().pose(),&view,1,restore,e))return false;
 }
 return true;
}
bool WorldSkeletalHeroLoadouts::warm_extra(unsigned program,std::string&e){if(!loaded_)return true;for(auto &hero:heroes_)if(hero&&!hero->warm_extra(program,e))return false;return true;}
bool WorldSkeletalHeroLoadouts::draw(unsigned program,std::string&e){return ready()?active().draw(program,e,blood_wear_):true;}
bool WorldSkeletalHeroLoadouts::first_person_ready()const{return ready()&&first_person_[selected()]&&first_person_program_;}
bool WorldSkeletalHeroLoadouts::draw_first_person(unsigned program,std::string&e){
 if(!first_person_ready())return true;
 if(program!=first_person_program_){e="First-person program needs prepare_first_person";return false;}
 world_animation_render::Bindings restore;restore.blood_wear=blood_wear_;if(!active().equipment_weapon_visible()){restore.hidden_primitive_begin=first_person_held_[selected()][0];restore.hidden_primitive_end=first_person_held_[selected()][1];}return world_animation_render::draw(first_person_[selected()],program,restore,e);
}
bool WorldSkeletalHeroLoadouts::contact_blade(unsigned direction,double phase,double x,double y,double z,float yaw,double base[3],double tip[3],std::string&e,double windup,std::uint64_t serial,double strike,double recovery,double commit,Weapon weapon)const{
 const unsigned s=slot(weapon);
 if(!loaded_||!supports_weapon(weapon)||last_!=s||!seeded_[s]){e="Contact requested before explicit loadout transition seed";return false;}
 return heroes_[s]->contact_blade(direction,phase,x,y,z,yaw,base,tip,e,windup,serial,strike,recovery,commit);
}
bool WorldSkeletalHeroLoadouts::guard_blade(unsigned direction,double seconds,double x,double y,double z,float yaw,double base[3],double tip[3],std::string&e,Weapon weapon)const{
 const unsigned s=slot(weapon);
 if(!loaded_||!supports_weapon(weapon)||last_!=s||!seeded_[s]){e="Guard requested before explicit loadout transition seed";return false;}
 return heroes_[s]->guard_blade(direction,seconds,x,y,z,yaw,base,tip,e);
}
bool WorldSkeletalHeroLoadouts::guard_shield(unsigned direction,double seconds,double x,double y,double z,float yaw,std::vector<std::array<double,9>>&triangles,std::string&e,Weapon weapon)const{
 const unsigned s=slot(weapon);
 if(!loaded_||!supports_weapon(weapon)||last_!=s||!seeded_[s]){e="Shield requested before explicit loadout transition seed";return false;}
 return heroes_[s]->guard_shield(direction,seconds,x,y,z,yaw,triangles,e);
}
bool WorldSkeletalHeroLoadouts::body_capsules(double x,double y,double z,float yaw,std::vector<world_combat_anatomy::Capsule>&capsules,std::string&e,unsigned direction,double seconds)const{
 if(!loaded_||last_!=selected()||!seeded_[selected()]){e="Body requested before explicit loadout transition seed";return false;}
 return active().body_capsules(x,y,z,yaw,capsules,e,direction,seconds);
}
bool WorldSkeletalHeroLoadouts::guard_target_preview(unsigned direction,const double*target,double x,double y,double z,float yaw,double base[3],double tip[3],std::vector<world_combat_anatomy::Capsule>&body,std::string&e,Weapon weapon,std::vector<std::array<double,9>>*surfaces)const{
 const unsigned s=slot(weapon);
 if(!loaded_||!supports_weapon(weapon)||last_!=s||!seeded_[s]){e="Guard target requested before explicit loadout transition seed";return false;}
 return heroes_[s]->guard_target_preview(direction,target,x,y,z,yaw,base,tip,body,e,surfaces);
}
const WorldSkeletalHeroLoadouts::Stats&WorldSkeletalHeroLoadouts::stats()const{
 aggregate_=active().stats();
 aggregate_.updates=aggregate_.uploads=aggregate_.draws=aggregate_.failures=aggregate_.attacks=aggregate_.hitstop_frames=aggregate_.grip_solves=aggregate_.foot_plants=aggregate_.foot_releases=aggregate_.foot_reach_releases=aggregate_.body_reactions=aggregate_.clash_reactions=aggregate_.turn_steps=0;
 aggregate_.swim_frames=aggregate_.submerged_frames=aggregate_.death_frames=aggregate_.special_entries=aggregate_.special_exits=aggregate_.special_mode_changes=0;
 for(const auto&hero:heroes_){const auto&s=hero->stats();aggregate_.updates+=s.updates;aggregate_.uploads+=s.uploads;aggregate_.draws+=s.draws;aggregate_.failures+=s.failures;aggregate_.attacks+=s.attacks;aggregate_.hitstop_frames+=s.hitstop_frames;aggregate_.grip_solves+=s.grip_solves;aggregate_.foot_plants+=s.foot_plants;aggregate_.foot_releases+=s.foot_releases;aggregate_.foot_reach_releases+=s.foot_reach_releases;aggregate_.body_reactions+=s.body_reactions;aggregate_.clash_reactions+=s.clash_reactions;aggregate_.turn_steps+=s.turn_steps;aggregate_.swim_frames+=s.swim_frames;aggregate_.submerged_frames+=s.submerged_frames;aggregate_.death_frames+=s.death_frames;aggregate_.special_entries+=s.special_entries;aggregate_.special_exits+=s.special_exits;aggregate_.special_mode_changes+=s.special_mode_changes;}
 aggregate_.ready=ready();return aggregate_;
}
void WorldSkeletalHeroLoadouts::release(){for(auto&gpu:first_person_){world_animation_render::destroy(gpu);gpu=nullptr;}for(auto&hero:heroes_)hero->release();loaded_=false;first_person_held_={};swim_head_joints_.fill(~0u);swim_viewmodel_.reset();seeded_.fill(false);last_=4;first_person_program_=0;first_person_epoch_=0;prepared_color_=prepared_shadow_=0;prepared_epoch_=0;source_sword_.clear();source_manifest_.clear();appearance_config_.clear();}
