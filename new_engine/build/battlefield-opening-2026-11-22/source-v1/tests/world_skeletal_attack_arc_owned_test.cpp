#include "engine/world_skeletal_hero_loadouts.h"
#include "engine/world_skeletal_attack_arc_cache.h"
#include "engine/world_knight_appearance.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
using namespace world_animation;
static unsigned checks=0,queries=0,real_mutations=0,cache_hits=0;
static void require(bool value,const std::string&message){++checks;if(!value){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
// Any GPU allocation, warmup, upload or draw is a test failure. Releasing the
// CPU-only controllers calls destroy(nullptr), which is deliberately allowed.
namespace world_animation_render {
Renderer*create(const Asset&,unsigned,std::string&,std::uint64_t){std::abort();}
void destroy(Renderer*p){if(p)std::abort();}
bool warm(Renderer*,unsigned,std::string&){std::abort();}
bool upload(Renderer*,const Pose*,const Instance*,std::size_t,const Bindings&,std::string&){std::abort();}
bool draw(Renderer*,unsigned,const Bindings&,std::string&){std::abort();}
}
static world_skeletal_attack_arc_cache::Inputs identity(const WorldSkeletalHero&h,const char*prefix){
 const auto&a=h.asset();world_skeletal_attack_arc_cache::Inputs result;result.asset=&a;result.held_begin=h.held_primitives_begin();
 const std::string family=prefix?prefix:"";result.stationary_cut_adaptation=family.empty();result.grip_profile=family=="Shield_"?1u:family.empty()?0u:2u;
 constexpr const char*names[]={"Idle","Walk_Fwd","Walk_FwdRight","Walk_Right","Walk_BwdRight","Walk_Bwd","Walk_BwdLeft","Walk_Left","Walk_FwdLeft","Run","Attack_Left","Attack_Right","Attack_High","Attack_Low","Attack_Thrust"};
 for(unsigned k=0;k<15;++k){unsigned found=0;for(unsigned n=0;n<a.clips.size();++n)if(a.clips[n].name==family+names[k]){result.clips[k]=n;++found;}require(found==1,"real fixture clip contract");}
 for(unsigned n=0;n<a.skeleton.joints.size();++n){const auto&name=a.skeleton.joints[n].name;if(name=="pelvis")result.pelvis=n;if(name=="spine_01")result.spine=n;if(name=="hand_r")result.hand=result.right_hand=n;}
 for(unsigned n=0;n<a.skeleton.joints.size();++n)if(a.skeleton.joints[n].name=="valwood_weapon_r")result.hand=n;
 return result;
}
static void seed(WorldSkeletalHero&a,WorldSkeletalHero&b,unsigned weapon,std::string&error){
 WorldTrainingCombat fight;const world_weapon_loadout::Weapon weapons[]={world_weapon_loadout::Weapon::Sword,world_weapon_loadout::Weapon::SwordShield,world_weapon_loadout::Weapon::GreatAxe,world_weapon_loadout::Weapon::Staff};fight.weapon=weapons[weapon];
 WorldSkeletalHero::Input in;in.dt=1./60.;in.reset=true;
 for(unsigned n=0;n<3;++n){require(a.update_cpu(in,fight,error),error);require(b.update_cpu(in,fight,error),error);in.reset=false;fight.clock+=in.dt;}
 require(a.equipment_combat_ready()&&b.equipment_combat_ready(),"contact comparison starts with drawn weapons");
}
int main(int argc,char**argv){
 require(argc==8,"civilian.cfg armored.cfg sword.glb shield.glb axe.glb staff.glb manifest");
 world_knight_appearance::Config config[2];Asset body[2];std::string error;
 for(unsigned n=0;n<2;++n)require(world_knight_appearance::load(argv[n+1],config[n],body[n],error),error);
 constexpr const char*prefix[]={nullptr,"Shield_","Axe_","Staff_"};
 double cold_arc_ms=0,reused_arc_ms=0,cold_total_ms=0,reused_total_ms=0;
 std::cout<<std::setprecision(12)<<"{\"cases\":[";
 for(unsigned weapon=0;weapon<4;++weapon){
  WorldSkeletalHero live,cold,cached;
  require(live.load_cpu(argv[3+weapon],error,prefix[weapon]),error);require(live.select_appearance(body[0],config[0].first_person,error),error);
  require(cold.load_cpu(argv[3+weapon],error,prefix[weapon]),error);require(cached.load_cpu(argv[3+weapon],error,prefix[weapon],&live),error);
  const auto&normal=cold.cpu_load_stats();const auto&reuse=cached.cpu_load_stats();
  require(!normal.arc_cache_reused&&normal.generated_arc_samples==5*1321,"control computes the full accepted table");require(reuse.arc_cache_reused&&reuse.generated_arc_samples==0,"real civilian-to-armor stage reuses exact table");++cache_hits;
  auto source=identity(live,prefix[weapon]),target=identity(cached,prefix[weapon]);require(world_skeletal_attack_arc_cache::compatible(source,target),"real held geometry survives body recomposition exactly");
  {Asset mutated=cached.asset();auto changed=target;changed.asset=&mutated;auto&value=mutated.clips[target.clips[10]].channels.front().values.front()[0];value=std::nextafter(value,value==0?1.f:0.f);require(!world_skeletal_attack_arc_cache::compatible(source,changed),"one real attack key change rejects reuse");++real_mutations;}
  {Asset mutated=cached.asset();auto changed=target;changed.asset=&mutated;auto&vertex=mutated.mesh.vertices[mutated.mesh.indices[mutated.mesh.primitives[target.held_begin].first_index]];vertex.position.x=std::nextafter(vertex.position.x,vertex.position.x==0?1.f:0.f);require(!world_skeletal_attack_arc_cache::compatible(source,changed),"one real held-position bit change rejects reuse");++real_mutations;}
  require(cold.select_appearance(body[1],config[1].first_person,error),error);require(cached.select_appearance(body[1],config[1].first_person,error),error);seed(cold,cached,weapon,error);
  for(unsigned d=0;d<5;++d)for(unsigned n=0;n<=240;++n){double a[3],b[3],c[3],d_tip[3];const double phase=n/240.;
   require(cold.contact_blade(d,phase,11,2,-7,.41f,a,b,error),error);require(cached.contact_blade(d,phase,11,2,-7,.41f,c,d_tip,error),error);
   require(std::memcmp(a,c,sizeof a)==0&&std::memcmp(b,d_tip,sizeof b)==0,"every tested physical blade endpoint remains bit exact");++queries;
  }
  cold_arc_ms+=normal.arc_ms;reused_arc_ms+=reuse.arc_ms;cold_total_ms+=normal.total_ms;reused_total_ms+=reuse.total_ms;
  if(weapon)std::cout<<',';
  std::cout<<"{\"weapon\":"<<weapon<<",\"cold_parse_ms\":"<<normal.parse_ms<<",\"cold_setup_ms\":"<<normal.setup_ms<<",\"cold_arc_ms\":"<<normal.arc_ms<<",\"cold_total_ms\":"<<normal.total_ms<<",\"reused_parse_ms\":"<<reuse.parse_ms<<",\"reused_setup_ms\":"<<reuse.setup_ms<<",\"reused_arc_ms\":"<<reuse.arc_ms<<",\"reused_total_ms\":"<<reuse.total_ms<<'}';
 }
 // Exercise the real atomic loadout caller as well as direct controllers.
 // No GPU resources are prepared, and both outfit swaps must forward all4
 // previously loaded immutable arc tables, including a composed source body.
 WorldSkeletalHeroLoadouts bank;world_weapon_loadout::Weapon weapon=world_weapon_loadout::Weapon::Sword;bank.bind_weapon(&weapon);require(bank.load_cpu(argv[3],error,argv[7]),error);
 const world_weapon_loadout::Weapon weapons[]={world_weapon_loadout::Weapon::Sword,world_weapon_loadout::Weapon::SwordShield,world_weapon_loadout::Weapon::GreatAxe,world_weapon_loadout::Weapon::Staff};
 for(unsigned outfit=0;outfit<2;++outfit){require(bank.select_appearance(argv[1+outfit],error),error);for(auto selected:weapons){weapon=selected;const auto&s=bank.active().cpu_load_stats();require(s.arc_cache_reused&&s.generated_arc_samples==0,"atomic wardrobe caller forwards every exact table");++cache_hits;}}
 std::cout<<"],\"passed\":true,\"gpu_used\":false,\"cache_hits\":"<<cache_hits<<",\"physical_contact_queries_bit_exact\":"<<queries<<",\"real_asset_mutations_rejected\":"<<real_mutations<<",\"assertions\":"<<checks<<",\"four_rig_control_arc_ms\":"<<cold_arc_ms<<",\"four_rig_reused_arc_ms\":"<<reused_arc_ms<<",\"four_rig_control_total_cpu_ms\":"<<cold_total_ms<<",\"four_rig_reused_total_cpu_ms\":"<<reused_total_ms<<",\"timing_scope\":\"CPU-only load boundary; control disables arc reuse, not filesystem caching. GPU upload, wardrobe composition and normal-play frame pacing are separate.\"}\n";
}
