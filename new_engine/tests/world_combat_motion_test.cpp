#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_combat_motion.h"
#include "engine/world_models.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <new>
namespace {unsigned loads=0,live_assets=0,creates=0,live_batches=0,targets=0,fail_load=0,fail_create=0,fail_target=0,draws=0,drawn=0,uploads=0,allocations=0;unsigned last_arms_slot=0,last_arms_target=0;float last_arms_weight=0;bool track=false;std::array<bool,172> seen={};std::array<unsigned,172> body_draw_counts={};std::array<std::array<WorldModelInstance,9>,172> body_draw_instances={};std::array<std::array<float,9>,172> body_draw_weights={};
#include "engine/world_combat_weapon_sockets.inc"
}
void *operator new(std::size_t n){if(track)allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new(std::size_t n,const std::nothrow_t &)noexcept{if(track)allocations++;return std::malloc(n?n:1);}
void operator delete(void *p)noexcept{std::free(p);}void operator delete(void *p,std::size_t)noexcept{std::free(p);}
void operator delete(void *p,const std::nothrow_t &)noexcept{std::free(p);}
struct WorldModelAsset{unsigned slot;bool arms;};
struct WorldModelPoseBlend{WorldModelAsset*a=nullptr,*b=nullptr;unsigned count=0;std::array<WorldModelInstance,9> instances={};std::array<float,9> weights={};};
WorldModelAsset *load(const char *path,bool origin){loads++;if(loads==fail_load)return nullptr;unsigned slot;char kind[16];const char *name=std::strrchr(path,'/');name=name?name+1:path;assert(std::sscanf(name,"combat_%15[^_]_%u.glb",kind,&slot)==2&&slot<172);const bool arms=!std::strcmp(kind,"arms");assert(origin==(arms||slot>=108));live_assets++;return new WorldModelAsset{slot,arms};}
extern "C" WorldModelAsset*world_model_load_pose(const char*p,unsigned v){assert(v==1);return load(p,false);}
extern "C" WorldModelAsset*world_model_load_pose_origin(const char*p,unsigned v){assert(v==1);return load(p,true);}
extern "C" void world_model_free(WorldModelAsset*p){if(p){live_assets--;delete p;}}
extern "C" WorldModelPoseBlend*world_model_pose_blend_create(std::size_t n,unsigned v){assert((n==1||n==2||n==8||n==9)&&v==1);creates++;if(creates==fail_create)return nullptr;live_batches++;return new WorldModelPoseBlend;}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend*b,WorldModelAsset*a,WorldModelAsset*c){assert(b&&a&&c&&a->arms==c->arms);targets++;if(targets==fail_target)return false;b->a=a;b->b=c;return true;}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend*b,const WorldModelInstance*i,const float*w,std::size_t n){assert(b&&b->a&&n&&n<=9);for(unsigned k=0;k<n;k++){assert(std::isfinite(i[k].x)&&std::isfinite(i[k].y)&&std::isfinite(i[k].z)&&i[k].scale>0&&std::isfinite(w[k])&&w[k]>=0&&w[k]<=1);b->instances[k]=i[k];b->weights[k]=w[k];}b->count=n;uploads++;return true;}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend*b,unsigned p){assert(p&&b&&b->count);draws++;drawn+=b->count;seen[b->a->slot]=true;if(b->a->arms){last_arms_slot=b->a->slot;last_arms_target=b->b->slot;last_arms_weight=b->weights[0];}else{body_draw_counts[b->a->slot]+=b->count;body_draw_instances[b->a->slot]=b->instances;body_draw_weights[b->a->slot]=b->weights;}}
extern "C" void world_model_pose_blend_free(WorldModelPoseBlend*b){if(b){live_batches--;delete b;}}
extern "C" void world_atlas_village_origin(double*x,double*z){*x=1000000;*z=-2000000;}
extern "C" void APIENTRY glUseProgram(GLuint){}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint,const GLchar*){return -1;}
extern "C" void APIENTRY glUniform1i(GLint,GLint){}
extern "C" void APIENTRY glUniform1f(GLint,GLfloat){}
extern "C" void APIENTRY glUniform2f(GLint,GLfloat,GLfloat){}
extern "C" void APIENTRY glUniform3f(GLint,GLfloat,GLfloat,GLfloat){}
void reset(){loads=creates=targets=0;fail_load=fail_create=fail_target=0;assert(!live_assets&&!live_batches);}
int main(){char error[128];assert(!world_combat_motion_load(nullptr,error,sizeof error));
 for(unsigned i=1;i<=216;i++){reset();fail_load=i;assert(!world_combat_motion_load("models",error,sizeof error)&&error[0]&&!live_assets&&!live_batches);}
 for(unsigned i=1;i<=408;i++){reset();fail_create=i;assert(!world_combat_motion_load("models",error,sizeof error)&&error[0]&&!live_assets&&!live_batches);reset();fail_target=i;assert(!world_combat_motion_load("models",error,sizeof error)&&error[0]&&!live_assets&&!live_batches);}
 reset();auto*m=world_combat_motion_load("models",error,sizeof error);assert(m&&!error[0]&&live_assets==216&&live_batches==408);
 WorldTrainingCombat c;c.state=TrainingState::Fighting;
 track=true;
 for(unsigned direction=0;direction<5;direction++)for(unsigned frame=0;frame<120;frame++){
  c.attack_direction=c.enemy_direction=TrainingDirection(direction);c.selected=c.enemy_guard=TrainingDirection(direction);c.player_phase=frame<45?TrainingPlayerPhase::Windup:frame<65?TrainingPlayerPhase::Strike:TrainingPlayerPhase::Recovery;c.enemy_phase=frame<45?TrainingEnemyPhase::Windup:frame<65?TrainingEnemyPhase::Strike:TrainingEnemyPhase::Recovery;
  c.player_duration=c.enemy_duration=1;c.player_time=c.enemy_time=(frame%40)/40.;
  world_combat_motion_update(m,c,999950,24,-2000073,1.57f,1.f/60,true);
  const unsigned prior=uploads;world_combat_motion_draw(m,11,999950,25.75,-2000073,false);world_combat_motion_draw(m,22,999950,25.75,-2000073,true);world_combat_motion_draw(m,11,999950,25.75,-2000073,false,false);world_combat_motion_draw_arms(m,33);assert(prior==uploads);
 }
 for(unsigned i=0;i<5;i++)assert(seen[12+i*12]||seen[13+i*12]);
 c.player_phase=TrainingPlayerPhase::Ready;c.enemy_phase=TrainingEnemyPhase::Measure;c.guard=true;
 // Verify the pose actually drawn, not only the requested gameplay angle.
 // Every side/high/low/center hold must finish in its own bank within .3s,
 // including direction changes while LT remains held at common frame rates.
 for(unsigned rate:{30u,60u,144u})for(unsigned d=0;d<5;d++){
  c.selected=c.enemy_guard=TrainingDirection(d);
  for(unsigned i=0;i<unsigned(std::ceil(.3*rate));i++)world_combat_motion_update(m,c,999950,24,-2000073,1.57f,1.f/rate,true);
  const auto pose=world_combat_motion_pose(m,0),enemy=world_combat_motion_pose(m,1);
  assert(pose.active&&pose.guard_direction==d&&pose.guard_weight>.999f&&pose.slot==75+4*d);
  assert(enemy.active&&enemy.guard_direction==d&&enemy.guard_weight>.999f&&enemy.slot==75+4*d);
  world_combat_motion_draw(m,11,999950,25.75,-2000073,false);
  world_combat_motion_draw_arms(m,33);assert(last_arms_slot==75+4*d&&last_arms_weight<.001f);
 }
 assert(!world_combat_motion_pose(nullptr,0).active&&!world_combat_motion_pose(m,2).active);
 c.guard=false;c.state=TrainingState::Idle;
 for(unsigned i=0;i<600;i++){world_combat_motion_update(m,c,999950+(i<400?i*.02:8),24,-2000073,1.57f,1.f/60,false);world_combat_motion_draw(m,11,999950,25.75,-2000073,false);}
 // A held left guard lowers continuously to the shared neutral before a
 // high attack. It must not jump directly from a block to a raised windup.
 c.state=TrainingState::Fighting;c.selected=TrainingDirection::Left;c.guard=true;c.player_phase=TrainingPlayerPhase::Ready;
 for(unsigned i=0;i<60;i++)world_combat_motion_update(m,c,999958,24,-2000073,1.57f,1.f/60,false);
 world_combat_motion_draw_arms(m,33);assert(last_arms_slot==75);
 c.guard=false;c.attack_direction=TrainingDirection::High;c.player_phase=TrainingPlayerPhase::Windup;c.player_duration=.3;c.player_time=0;
 world_combat_motion_update(m,c,999958,24,-2000073,1.57f,1.f/60,false);world_combat_motion_draw_arms(m,33);assert(last_arms_slot==75);
 c.player_time=.089;world_combat_motion_update(m,c,999958,24,-2000073,1.57f,1.f/60,false);world_combat_motion_draw_arms(m,33);assert(last_arms_slot==72&&last_arms_weight<.01f);
 c.player_time=.09001;world_combat_motion_update(m,c,999958,24,-2000073,1.57f,1.f/60,false);world_combat_motion_draw_arms(m,33);assert(last_arms_slot==36&&last_arms_weight<.001f);
 c.player_time=.3;world_combat_motion_update(m,c,999958,24,-2000073,1.57f,1.f/60,false);world_combat_motion_draw_arms(m,33);assert(last_arms_slot==39&&std::abs(last_arms_weight-.96f)<.001f);
 // Contact counters and hit-stop are published together. The rendered banks
 // must commit the contact/reaction in that update, then hold without uploads
 // until the stop expires. Exercise both body hits and every metal contact.
 auto tick=[&](){world_combat_motion_update(m,c,999958,24,-2000073,1.57f,1.f/60,true);};
 auto settle=[&](){
  c.state=TrainingState::Fighting;c.guard=false;c.impact_pause=0;
  c.player_phase=TrainingPlayerPhase::Ready;c.enemy_phase=TrainingEnemyPhase::Measure;
  c.player_time=c.enemy_time=0;c.selected=c.enemy_guard=TrainingDirection::Thrust;
  c.attack_direction=c.enemy_direction=TrainingDirection::High;
  for(unsigned i=0;i<90;i++)tick();
 };
 auto same_pose=[](const WorldCombatPose&a,const WorldCombatPose&b){return a.slot==b.slot&&a.guard_direction==b.guard_direction&&a.guard_weight==b.guard_weight&&a.blend==b.blend&&a.active==b.active;};
 auto hold_contact=[&](){
  const auto player=world_combat_motion_pose(m,0),enemy=world_combat_motion_pose(m,1);
  const auto committed=uploads;
  for(unsigned i=0;i<12;i++)tick();
  assert(uploads==committed&&same_pose(player,world_combat_motion_pose(m,0))&&same_pose(enemy,world_combat_motion_pose(m,1)));
 };
 settle();c.player_phase=TrainingPlayerPhase::Windup;c.player_duration=.20;c.player_time=.199;tick();
 const auto windup=world_combat_motion_pose(m,0);const auto before_contact=uploads;
 c.player_phase=TrainingPlayerPhase::Strike;c.player_duration=.10;c.player_time=0;c.hits++;c.impact_pause=.032;tick();
 auto contact=world_combat_motion_pose(m,0),reaction=world_combat_motion_pose(m,1);
 assert(uploads>before_contact&&contact.slot==39&&std::abs(contact.blend-.96f)<.001f&&!same_pose(windup,contact));
 assert(reaction.slot==92&&reaction.blend==0);
 world_combat_motion_draw_arms(m,33);assert(last_arms_slot==39&&std::abs(last_arms_weight-.96f)<.001f);
 hold_contact();c.impact_pause=0;c.player_time=.025;tick();
 assert(!same_pose(contact,world_combat_motion_pose(m,0))&&world_combat_motion_pose(m,1).blend>0);
 for(unsigned kind=0;kind<4;kind++){
  settle();c.enemy_phase=TrainingEnemyPhase::Strike;c.enemy_duration=.14;c.enemy_time=0;
  unsigned actor=0,slot=92;
  if(kind==0)c.enemy_hits++;
  if(kind==1){c.parries++;c.enemy_phase=TrainingEnemyPhase::Stagger;actor=1;slot=100;}
  if(kind==2){c.blocks++;slot=100;}
  if(kind==3){c.guarded++;c.player_phase=TrainingPlayerPhase::Strike;c.player_duration=.10;c.player_time=0;c.enemy_phase=TrainingEnemyPhase::Measure;actor=1;slot=100;}
  const auto before=uploads;c.impact_pause=.032;tick();
  const auto committed=world_combat_motion_pose(m,actor);
  assert(uploads>before&&committed.slot==slot&&committed.blend==0);
  hold_contact();c.impact_pause=0;tick();
  assert(!same_pose(committed,world_combat_motion_pose(m,actor)));
 }
 // Enemy gait follows the root's metres rather than wall-clock time. Fast
 // pursuit and slow approach must cover different parts of the same stride;
 // identical travel at another frame rate must agree, and a stop holds feet.
 settle();world_combat_motion_arena_enemy_walk(m,true);
 world_combat_motion_arena(m,1000000,-2000000,24);
 for(unsigned i=0;i<60;i++)tick();
 auto travel_pose=[&](double metres,unsigned frames){
  world_combat_motion_arena(m,1000020,-2000000,24);tick();
  world_combat_motion_arena(m,1000000,-2000000,24);tick();
  for(unsigned i=0;i<frames;i++){
   world_combat_motion_arena(m,1000000+metres*(i+1)/frames,-2000000,24);tick();
  }
  return world_combat_motion_pose(m,1);
 };
 const auto slow_gait=travel_pose(.13,60),fast_gait=travel_pose(.39,60);
 assert(slow_gait.slot==4&&std::abs(slow_gait.blend-.8f)<.001f);
 assert(fast_gait.slot==6&&std::abs(fast_gait.blend-.4f)<.001f);
 const auto other_rate=travel_pose(.39,30);
 assert(other_rate.slot==fast_gait.slot&&std::abs(other_rate.blend-fast_gait.blend)<.001f);
 for(unsigned i=0;i<120;i++){tick();const auto stopped=world_combat_motion_pose(m,1);assert(stopped.slot==other_rate.slot&&stopped.blend==other_rate.blend);}
 world_combat_motion_arena(m,1000020,-2000000,24);tick();
 const auto cut_gait=world_combat_motion_pose(m,1);assert(cut_gait.slot==4&&cut_gait.blend==0);
 world_combat_motion_arena_enemy_walk(m,false);
 // Holding a published paused pose must not re-stream the same three banks.
 const auto paused_uploads=uploads;for(unsigned i=0;i<120;i++)world_combat_motion_update(m,c,999958,24,-2000073,1.57f,1.f/60,false,true);assert(uploads==paused_uploads);
 auto previous=world_combat_motion_stats(m);world_combat_motion_update(m,c,NAN,24,0,0,1.f/60,false);assert(previous.uploads==world_combat_motion_stats(m).uploads);
 track=false;assert(!allocations);assert(world_combat_motion_stats(m).pose_assets==216);world_combat_motion_free(m);assert(!live_assets&&!live_batches);
 // The expanded bank is explicit: legacy launchers still need only108 poses.
 // Validate every new socket before loading any model or creating GL objects.
 const auto fixture=std::filesystem::temp_directory_path()/("valwood-motion-polished-"+std::to_string(reinterpret_cast<std::uintptr_t>(&error)));
 std::filesystem::create_directory(fixture);const auto directory=fixture.string(),socket_path=(fixture/"combat_weapon_sockets.bin").string();
 auto write_sockets=[&](unsigned bad=0){
  auto*f=std::fopen(socket_path.c_str(),"wb");assert(f);std::fwrite(bad==1?"INVALID!":"VWMOT172",1,8,f);
  auto word=[&](std::uint32_t bits){unsigned char bytes[4]={static_cast<unsigned char>(bits),static_cast<unsigned char>(bits>>8),static_cast<unsigned char>(bits>>16),static_cast<unsigned char>(bits>>24)};assert(std::fwrite(bytes,1,4,f)==4);};
  word(bad==2?171:172);
  for(unsigned i=0;i<172;i++)for(unsigned j=0;j<2;j++)for(unsigned k=0;k<3;k++){
   float value=i<108?combat_weapon_sockets[i][j][k]:float(i-108)*.001f+float(k)*.1f;
   if(i==0&&j==0&&k==0&&bad==3)value+=.001f;
   if(i==171&&j==1&&k==2&&bad==4)value=NAN;
   if(i==171&&j==1&&k==2&&bad==5)value=17;
   std::uint32_t bits=0;std::memcpy(&bits,&value,4);word(bits);
  }
  if(bad==6)std::fputc(1,f);
  std::fclose(f);
 };
 reset();assert(!world_combat_motion_load(directory.c_str(),error,sizeof error,true)&&loads==0&&!live_assets&&!live_batches);
 for(unsigned bad=1;bad<=6;bad++){write_sockets(bad);reset();assert(!world_combat_motion_load(directory.c_str(),error,sizeof error,true)&&error[0]&&loads==0&&!live_assets&&!live_batches);}
 write_sockets();
 for(unsigned i=1;i<=344;i++){reset();fail_load=i;assert(!world_combat_motion_load(directory.c_str(),error,sizeof error,true)&&error[0]&&!live_assets&&!live_batches);}
 for(unsigned i=1;i<=600;i++){reset();fail_create=i;assert(!world_combat_motion_load(directory.c_str(),error,sizeof error,true)&&error[0]&&!live_assets&&!live_batches);reset();fail_target=i;assert(!world_combat_motion_load(directory.c_str(),error,sizeof error,true)&&error[0]&&!live_assets&&!live_batches);}
 // Crowd preparation is transactional and can be retried after any allocation
 // or target failure, leaving the already loaded motion bank intact.
 for(unsigned kind=0;kind<2;kind++)for(unsigned i=1;i<=25;i++){
  reset();auto*bank=world_combat_motion_load(directory.c_str(),error,sizeof error,true);assert(bank);
  if(kind)fail_target=targets+i;else fail_create=creates+i;
  assert(!world_combat_motion_prepare_crowd(bank)&&live_assets==344&&live_batches==600);
  fail_target=fail_create=0;assert(world_combat_motion_prepare_crowd(bank)&&live_batches==625);world_combat_motion_free(bank);assert(!live_assets&&!live_batches);
 }
 reset();m=world_combat_motion_load(directory.c_str(),error,sizeof error,true);assert(m&&live_assets==344&&live_batches==600);assert(world_combat_motion_prepare_crowd(m)&&world_combat_motion_prepare_crowd(m)&&live_batches==625);
 auto polished=world_combat_motion_stats(m);assert(polished.polished&&polished.pose_assets==344&&polished.reaction_assets==40&&polished.death_assets==24);
 world_combat_motion_arena(m,0,0,64);c=WorldTrainingCombat{};c.state=TrainingState::Fighting;c.enemy_phase=TrainingEnemyPhase::Measure;c.player_phase=TrainingPlayerPhase::Ready;
 auto polished_tick=[&](float dt=1.f/60,bool paused=false,bool show_enemy=true){world_combat_motion_update(m,c,0,64,-5,0,dt,show_enemy,paused);};
 auto*linear=world_combat_motion_load("models/longsword_mocap",error,sizeof error);assert(linear);world_combat_motion_arena(linear,0,0,64);
 world_combat_motion_arena_enemy_walk(m,true);world_combat_motion_arena_enemy_walk(linear,true);
 auto linear_tick=[&](float dt=1.f/60){world_combat_motion_update(linear,c,0,64,-5,0,dt,true);};
 track=true;
 // Visual timing may add preparation/follow-through weight without moving
 // the core's contact or recovery boundaries. Compare a legacy linear bank
 // against the polished bank using identical gameplay phase/time and roots.
 auto phase_of=[](unsigned direction,const WorldCombatPose&pose){const unsigned base=12+12*direction;assert(pose.slot>=base&&pose.slot<base+12&&std::isfinite(pose.blend)&&pose.blend>=0&&pose.blend<=1);return (pose.slot-base+pose.blend)/11;};
 auto equal_blade=[&](unsigned who){
  WorldCombatBlade a,b;assert(world_combat_motion_blade(m,who,a)&&world_combat_motion_blade(linear,who,b));
  for(unsigned k=0;k<3;k++){assert(std::abs(a.base[k]-b.base[k])<1.e-5&&std::abs(a.tip[k]-b.tip[k])<1.e-5);}
  if(who==0){assert(world_combat_motion_blade(m,0,a,true)&&world_combat_motion_blade(linear,0,b,true));for(unsigned k=0;k<3;k++)assert(std::abs(a.base[k]-b.base[k])<1.e-5&&std::abs(a.tip[k]-b.tip[k])<1.e-5);}
 };
 for(unsigned rate:{30u,60u,144u})for(unsigned direction=0;direction<5;direction++){
  c.attack_direction=c.enemy_direction=TrainingDirection(direction);float previous[2]={0,0};
  for(unsigned phase=0;phase<3;phase++){
   const double duration=phase==0?(direction==4?.17:.20):phase==1?.10:.25;
   c.player_phase=phase==0?TrainingPlayerPhase::Windup:phase==1?TrainingPlayerPhase::Strike:TrainingPlayerPhase::Recovery;
   c.enemy_phase=phase==0?TrainingEnemyPhase::Windup:phase==1?TrainingEnemyPhase::Strike:TrainingEnemyPhase::Recovery;
   c.player_duration=c.enemy_duration=duration;const unsigned frames=unsigned(std::ceil(duration*rate));
   for(unsigned frame=0;frame<=frames;frame++){
    const float fraction=float(frame)/frames;c.player_time=c.enemy_time=duration*frame/frames;polished_tick(1.f/rate);linear_tick(1.f/rate);
    for(unsigned who=0;who<2;who++){
     const float curved=phase_of(direction,world_combat_motion_pose(m,who)),straight=phase_of(direction,world_combat_motion_pose(linear,who));
     const float low=phase==0?0:phase==1?.36f:.60f,high=phase==0?.36f:phase==1?.60f:1;
     assert(curved>=low-1.e-6f&&curved<=high+1.e-6f&&curved>=previous[who]-1.e-6f);previous[who]=curved;
     assert(std::abs(straight-(low+(high-low)*fraction))<1.e-6f);
     if(frame==0||frame==frames){assert(std::abs(curved-straight)<1.e-6f);equal_blade(who);}
     else if(phase==0)assert(curved<straight);
     else if(phase==1)assert(curved>straight);
     else if(fraction<.49f)assert(curved<straight);
     else if(fraction>.51f)assert(curved>straight);
    }
   }
  }
 }
 c.attack_direction=c.enemy_direction=TrainingDirection::High;c.player_phase=TrainingPlayerPhase::Windup;c.enemy_phase=TrainingEnemyPhase::Windup;c.player_duration=c.enemy_duration=.20;c.player_time=c.enemy_time=.199;
 polished_tick();linear_tick();c.player_phase=TrainingPlayerPhase::Strike;c.player_duration=.10;c.player_time=0;c.hits++;c.impact_pause=.032;polished_tick();linear_tick();
 const auto weighted_contact=world_combat_motion_pose(m,0),linear_contact=world_combat_motion_pose(linear,0);
 assert(weighted_contact.slot==39&&std::abs(weighted_contact.blend-.96f)<.001f&&weighted_contact.slot==linear_contact.slot&&weighted_contact.blend==linear_contact.blend);equal_blade(0);
 const unsigned weighted_hold_uploads=uploads;for(unsigned i=0;i<12;i++){polished_tick();linear_tick();}assert(uploads==weighted_hold_uploads);equal_blade(0);
 c.impact_pause=0;c.player_phase=TrainingPlayerPhase::Ready;c.enemy_phase=TrainingEnemyPhase::Measure;c.player_time=c.enemy_time=0;
 for(unsigned i=0;i<90;i++)polished_tick();
 c.hits=0;polished_tick();const auto directional_before=world_combat_motion_stats(m).directional_reactions;
 world_combat_motion_arena_enemy_walk(m,false);
 for(unsigned rate:{30u,60u,144u})for(unsigned d=0;d<5;d++){
  c.guard=true;c.selected=c.enemy_guard=TrainingDirection(d);
  for(unsigned i=0;i<unsigned(std::ceil(.3*rate));i++)polished_tick(1.f/rate);
  assert(world_combat_motion_pose(m,0).slot==75+4*d&&world_combat_motion_pose(m,1).slot==75+4*d);
 }
 c.guard=false;
 for(unsigned who=0;who<2;who++)for(unsigned d=0;d<5;d++){
  for(unsigned i=0;i<60;i++)polished_tick();
  c.attack_direction=c.enemy_direction=TrainingDirection(d);
  if(who==0)c.enemy_hits++;else c.hits++;
  c.impact_pause=.032;polished_tick();const auto committed=world_combat_motion_pose(m,who);
  assert(committed.slot==108+8*d&&committed.blend==0&&committed.reaction_direction==d&&!committed.defeated);
  const unsigned held_uploads=uploads;
  for(unsigned i=0;i<12;i++)polished_tick();
  const auto held=world_combat_motion_pose(m,who);assert(held.slot==committed.slot&&held.blend==committed.blend&&uploads==held_uploads);
  WorldCombatBlade blade;assert(world_combat_motion_blade(m,who,blade));for(double value:blade.tip)assert(std::isfinite(value));
  c.impact_pause=0;for(unsigned i=0;i<8;i++)polished_tick(.02f);
  const auto midpoint=world_combat_motion_pose(m,who);assert(midpoint.slot==111+8*d&&std::abs(midpoint.blend-.5f)<.001f);
  for(unsigned i=0;i<9;i++)polished_tick(.02f);
  const auto recovered=world_combat_motion_pose(m,who);assert(recovered.slot<108&&!recovered.defeated);
 }
 assert(world_combat_motion_stats(m).directional_reactions==directional_before+10);
 // A defeat begins exactly once, commits the first collapse under hit-stop,
 // progresses by unpaused time, and holds the grounded final frame forever.
 c.enemy_hits++;c.health=0;c.state=TrainingState::Lost;c.impact_pause=.032;polished_tick();
 auto fallen=world_combat_motion_pose(m,0);assert(fallen.defeated&&fallen.slot==148&&fallen.defeat_phase==0&&fallen.blend==0);
 const unsigned death_uploads=uploads;for(unsigned i=0;i<12;i++)polished_tick();assert(uploads==death_uploads&&world_combat_motion_stats(m).death_starts==1);
 c.impact_pause=0;for(unsigned i=0;i<54;i++)polished_tick();fallen=world_combat_motion_pose(m,0);assert(std::abs(fallen.defeat_phase-.5f)<.001f&&fallen.slot==159&&std::abs(fallen.blend-.5f)<.001f);
 const auto before_pause=fallen;const unsigned before_pause_uploads=uploads;for(unsigned i=0;i<120;i++)polished_tick(1.f/60,true);fallen=world_combat_motion_pose(m,0);assert(fallen.defeat_phase==before_pause.defeat_phase&&fallen.slot==before_pause.slot&&fallen.blend==before_pause.blend&&uploads==before_pause_uploads);
 for(unsigned i=0;i<120;i++)polished_tick();
 fallen=world_combat_motion_pose(m,0);assert(fallen.slot==171&&fallen.blend==0&&fallen.defeat_phase==1&&world_combat_motion_stats(m).death_starts==1);
 WorldCombatBlade corpse_blade;assert(world_combat_motion_blade(m,0,corpse_blade));for(double value:corpse_blade.base)assert(std::isfinite(value));
 c.state=TrainingState::Idle;c.health=100;c.hits=c.enemy_hits=0;polished_tick();assert(!world_combat_motion_pose(m,0).defeated);
 // The enemy can hand off to crowd on the fatal frame while hidden; this
 // records one actual defeat without drawing the active actor a second time.
 c.state=TrainingState::Won;c.hits=1;c.enemy_health=0;polished_tick(1.f/60,false,false);assert(world_combat_motion_stats(m).death_starts==2&&!world_combat_motion_pose(m,1).active);
 std::array<WorldCombatCrowdInstance,8> crowd={};for(unsigned i=0;i<8;i++)crowd[i]={double(i*2),64,double(i),.25f*float(i),true,float(i)/7};crowd[0].yielded=false;
 assert(world_combat_motion_crowd(m,crowd.data(),8));body_draw_counts.fill(0);const unsigned draw_uploads=uploads;world_combat_motion_draw_crowd(m,11,0,65,0);world_combat_motion_draw_crowd(m,22,0,65,0,true);assert(draw_uploads==uploads);
 assert(body_draw_counts[0]==2&&body_draw_counts[171]==2);
 for(unsigned i=1;i<8;i++){const float f=crowd[i].defeat_phase*23;const unsigned key=std::min(23u,unsigned(f)),slot=148+key;assert(body_draw_counts[slot]==2&&std::abs(body_draw_weights[slot][0]-(f-key))<.0001f);const auto at=body_draw_instances[slot][0];assert(at.x==crowd[i].x&&at.y==64&&at.z==crowd[i].z&&at.yaw==crowd[i].yaw);}
 polished=world_combat_motion_stats(m);assert(polished.crowd_instances==8&&polished.fallen_instances==7&&polished.peak_fallen_instances==7);
 for(auto&person:crowd){person.yielded=true;person.defeat_phase=1;}
 assert(world_combat_motion_crowd(m,crowd.data(),8));body_draw_counts.fill(0);world_combat_motion_draw_crowd(m,11,0,65,0);assert(body_draw_counts[171]==8&&world_combat_motion_stats(m).fallen_instances==8&&world_combat_motion_stats(m).peak_fallen_instances==8);
 crowd[3].defeat_phase=NAN;assert(!world_combat_motion_crowd(m,crowd.data(),8)&&world_combat_motion_stats(m).fallen_instances==8);crowd[3].defeat_phase=1;
 assert(!world_combat_motion_crowd(m,crowd.data(),9));assert(world_combat_motion_crowd(m,nullptr,0));body_draw_counts.fill(0);world_combat_motion_draw_crowd(m,11,0,65,0);for(auto n:body_draw_counts)assert(n==0);assert(world_combat_motion_stats(m).fallen_instances==0&&world_combat_motion_stats(m).peak_fallen_instances==8);
 // Nine independent actors share the same344 assets, while first-person
 // drawing excludes only actor0. Identical guards exercise all9 in one batch.
 c=WorldTrainingCombat{};c.state=TrainingState::Fighting;c.guard=true;
 c.selected=TrainingDirection::High;c.player_phase=TrainingPlayerPhase::Ready;
 std::array<WorldCombatArenaActor,8> live={};
 for(unsigned i=0;i<8;++i){live[i].combat=c;live[i].combat.enemy_phase=TrainingEnemyPhase::Measure;live[i].combat.enemy_guard=TrainingDirection::High;live[i].x=double(i*2)-7;live[i].y=64;live[i].z=3;live[i].visible=true;}
 auto arena_tick=[&](float dt=1.f/60,bool paused=false){world_combat_motion_update_arena(m,c,0,64,-5,0,dt,live.data(),8,paused);};
 for(unsigned i=0;i<120;++i)arena_tick();
 auto multi=world_combat_motion_stats(m);assert(multi.pose_assets==344&&multi.actors==9&&multi.active_enemies==8&&multi.peak_actors==9&&multi.combo_batches==25);
 const auto valid_uploads=uploads;world_combat_motion_update_arena(m,c,0,64,-5,0,1.f/60,live.data(),9);
 live[7].x=NAN;arena_tick();assert(uploads==valid_uploads&&world_combat_motion_stats(m).actors==9);live[7].x=7;
 for(unsigned actor=0;actor<9;++actor){auto pose=world_combat_motion_pose(m,actor);assert(pose.active&&pose.slot==83);WorldCombatBlade blade;assert(world_combat_motion_blade(m,actor,blade));for(double value:blade.tip)assert(std::isfinite(value));}
 assert(!world_combat_motion_pose(m,9).active&&!world_combat_motion_blade(m,9,corpse_blade));
 body_draw_counts.fill(0);unsigned before_draw=drawn,uploads_before_draw=uploads,relinks_before=targets;
 world_combat_motion_draw(m,11,0,65,-5,false);assert(drawn-before_draw==9&&body_draw_counts[83]==9);
 body_draw_counts.fill(0);before_draw=drawn;world_combat_motion_draw(m,11,0,65,-5,false,false);assert(drawn-before_draw==8&&body_draw_counts[83]==8);
 assert(uploads==uploads_before_draw&&targets==relinks_before);
 // One enemy's reaction cannot replace the other seven guards. The player
 // reacts to a concurrent independent hit; both commit before global hit-stop.
 c.enemy_hits++;c.enemy_direction=TrainingDirection::Left;c.impact_pause=.032;
 live[5].combat.hits++;live[5].combat.attack_direction=TrainingDirection::Low;
 arena_tick();assert(world_combat_motion_pose(m,0).slot==108&&world_combat_motion_pose(m,6).slot==132);
 for(unsigned id=1;id<=8;++id)if(id!=6)assert(world_combat_motion_pose(m,id).slot==83);
 const auto held_multi=uploads;for(unsigned n=0;n<20;++n)arena_tick();assert(uploads==held_multi);
 c.impact_pause=0;c.guard=false;
 for(unsigned i=0;i<80;++i)arena_tick();
 const auto deaths_before=world_combat_motion_stats(m).death_starts;
 live[5].combat.state=TrainingState::Won;live[5].combat.enemy_health=0;live[5].combat.hits++;live[5].visible=false;
 arena_tick();assert(world_combat_motion_stats(m).death_starts==deaths_before+1&&!world_combat_motion_pose(m,6).active);
 for(unsigned i=0;i<120;++i)arena_tick();
 assert(world_combat_motion_stats(m).death_starts==deaths_before+1);
 // Independent moving roots choose distinct phases of the shared gait bank.
 for(unsigned i=0;i<8;++i){live[i].visible=i<2;live[i].combat=WorldTrainingCombat{};live[i].walking=i<2;}
 live[0].x=-4;live[1].x=4;arena_tick();
 for(unsigned n=0;n<60;++n){live[0].x+=.13/60;live[1].x+=.39/60;arena_tick();}
 auto gait0=world_combat_motion_pose(m,1),gait1=world_combat_motion_pose(m,2);
 assert(gait0.slot==4&&std::abs(gait0.blend-.8f)<.001f&&gait1.slot==6&&std::abs(gait1.blend-.4f)<.001f);
 // All25 direction pairs use prelinked endpoint targets, detect the new
 // committed attack serial without a Ready gap, and publish matching sockets.
 const auto combo_before=world_combat_motion_stats(m).combo_transitions;
 for(unsigned from=0;from<5;++from)for(unsigned to=0;to<5;++to){
  c=WorldTrainingCombat{};c.state=TrainingState::Fighting;c.attack_direction=TrainingDirection(from);
  c.player_phase=TrainingPlayerPhase::Recovery;c.player_duration=.24;c.player_time=.08;c.combo_count=1;
  c.pending_attack=true;c.attack_start_serial=100+2*(from*5+to);
  // A Ready boundary between separate test pairs is ordinary explicit input.
  auto neutral=c;neutral.player_phase=TrainingPlayerPhase::Ready;neutral.pending_attack=false;
  for(unsigned n=0;n<30;++n)world_combat_motion_update_arena(m,neutral,0,64,-5,0,1.f/60,nullptr,0);
  world_combat_motion_update_arena(m,c,0,64,-5,0,1.f/60,nullptr,0);
  WorldCombatBlade endpoint;assert(world_combat_motion_blade(m,0,endpoint));
  const auto end_pose=world_combat_motion_pose(m,0);assert(end_pose.slot==12+12*from+8&&std::abs(end_pose.blend)<1.e-5f);
  c.attack_direction=TrainingDirection(to);c.attack_start_serial++;c.combo_count=2;c.pending_attack=false;
  c.player_phase=TrainingPlayerPhase::Windup;c.player_duration=.14;c.player_time=0;
  world_combat_motion_update_arena(m,c,0,64,-5,0,1.f/60,nullptr,0);
  auto cross=world_combat_motion_pose(m,0);assert(cross.combo_transition&&cross.combo_from==from&&cross.combo_to==to&&cross.blend==0);
  WorldCombatBlade start;assert(world_combat_motion_blade(m,0,start));for(unsigned k=0;k<3;++k){assert(std::abs(start.base[k]-endpoint.base[k])<1.e-6&&std::abs(start.tip[k]-endpoint.tip[k])<1.e-6);}
  c.player_time=.0225;world_combat_motion_update_arena(m,c,0,64,-5,0,1.f/60,nullptr,0);
  cross=world_combat_motion_pose(m,0);assert(cross.combo_transition&&std::abs(cross.blend-.5f)<1.e-6);
  world_combat_motion_draw_arms(m,33);assert(last_arms_slot==12+12*from+8&&last_arms_target==12+12*to&&std::abs(last_arms_weight-.5f)<1.e-6);
  body_draw_counts.fill(0);world_combat_motion_draw(m,11,0,65,-5,false);
  const auto instance=body_draw_instances[12+12*from+8][0];WorldCombatBlade actual;assert(world_combat_motion_blade(m,0,actual));
  for(unsigned point=0;point<2;++point){double q[3];for(unsigned k=0;k<3;++k)q[k]=.5*(combat_weapon_sockets[12+12*from+8][point][k]+combat_weapon_sockets[12+12*to][point][k]);
   const double expected[]={std::cos(instance.yaw)*q[0]+std::sin(instance.yaw)*q[2]+instance.x,q[1]+instance.y,-std::sin(instance.yaw)*q[0]+std::cos(instance.yaw)*q[2]+instance.z-128};const auto *value=point?actual.tip:actual.base;
   for(unsigned k=0;k<3;++k)assert(std::abs(value[k]-expected[k])<1.e-6);
  }
  const unsigned fixed_targets=targets;c.player_time=.0451;world_combat_motion_update_arena(m,c,0,64,-5,0,1.f/60,nullptr,0);
  assert(!world_combat_motion_pose(m,0).combo_transition&&targets==fixed_targets);
  c.player_phase=TrainingPlayerPhase::Strike;c.player_duration=.09;c.player_time=0;
  world_combat_motion_update_arena(m,c,0,64,-5,0,1.f/60,nullptr,0);
  assert(world_combat_motion_pose(m,0).slot==12+12*to+3&&std::abs(world_combat_motion_pose(m,0).blend-.96f)<.001f);
 }
 assert(world_combat_motion_stats(m).combo_transitions==combo_before+25);
 const auto fixed_uploads=uploads,fixed_targets=targets;
 for(unsigned n=0;n<1000;++n)world_combat_motion_update_arena(m,c,0,64,-5,0,1.f/60,nullptr,0,true);
 assert(uploads==fixed_uploads&&targets==fixed_targets);
 track=false;assert(!allocations);world_combat_motion_free(m);world_combat_motion_free(linear);assert(!live_assets&&!live_batches);std::filesystem::remove_all(fixture);
 std::puts("PASS nine shared-asset independentactors;TP9/FP8 in same heldguardbatch;stable0..8blade/pose;perenemy reactions/death/gait;all25serialcomboendpointpairs/socketexactness/contactanchors;nohotalloc/relink/drawuploads");
 std::puts("PASS owned combat motion: legacy216 assets/408 batches; polished344 assets/600 batches+25 fixed crowd groups;2626 atomic load/target/preparation failure points;7 malformed socket fixtures;all5 held guards and attacks preserved;polished visual weight curves monotonic/bounded at30/60/144Hz with exact legacy contact/recovery/neutral blade and viewmodel anchors and hit-stop;all5 directional body reactions commit before hit-stop, recover in.32s;real1.8s defeat freezes/resumes/holds and resets;8 corpse instances retain positions/yaws with independent interpolated collapse phases;no hot allocations or draw-time uploads;origin-preserved new poses/arms;clean ownership");
}
