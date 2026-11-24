#ifndef VALWOOD_COMBAT_VFX_H
#define VALWOOD_COMBAT_VFX_H
#include "engine/world_training_combat.h"
#include "world_combat_capacity.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace world_combat_vfx {
struct Point {double x=0,y=0,z=0;};
struct Blade {Point base,tip;bool valid=false;};
struct Contact {Point point;bool valid=false;};
// Already resolved gameplay contacts retain their individual opponent and
// position even when several swords make contact in the same frame.
struct ResolvedContact {
 std::uint64_t serial=0;unsigned kind=0; // hit, parry, block, incoming, enemy block
 Point point,feet,direction={0,0,1};float strength=1;bool finisher=false;
 // NPC-only clashes emit the same visible material response without taking
 // ownership of player camera feedback. Legacy/player events default true.
 bool player_feedback=true;
};
struct ActorTrail {Blade blade;bool active=false;std::uint64_t swing=0;};
// Optional resolved contacts are indexed by DEFENDER, e.g. body_contacts[1]
// carries a physical arrow's enemy-capsule hit. Otherwise the visible torso
// surface is derived from defender feet and the attacker's weapon segment.
struct Input {
 Blade blades[2],trail_blades[2];Point feet[2];float right[3]={1,0,0},up[3]={0,1,0};bool first_person=false,paused=false;Contact body_contacts[2];
 // Defender-indexed direction and damage weight affect only body visuals.
 // A zero direction preserves the ordinary outward torso-surface fallback.
 Point body_directions[2];float body_strength[2]={1,1};bool body_finisher[2]={false,false};
 bool resolved_events=false;unsigned contact_count=0;
 std::array<ResolvedContact,64> contacts={};
 std::array<ActorTrail,world_combat_capacity::actors> actor_trails={};
 std::uint64_t attack_serial=0;
};
struct Instance {std::array<float,4> corners[4];std::array<float,4> color,style;};
constexpr unsigned particle_capacity=192,trail_capacity=40,instance_capacity=particle_capacity+trail_capacity;
constexpr unsigned outgoing_body_particles=32,outgoing_finisher_particles=48;
constexpr unsigned incoming_body_particles=26,incoming_finisher_particles=40;
constexpr float blood_gravity=9.81f,blood_mist_lifetime=.12f,blood_particle_lifetime=.62f;
// An anchored fading steel streak survives several ordinary display frames,
// while sparks still expire promptly. This never applies to an air swing.
constexpr float steel_core_lifetime=.09f;
struct Stage {std::array<Instance,instance_capacity> instances={};unsigned count=0,particles=0,trails=0;};
enum class Material {None,Body,Metal};
struct Stats {
 unsigned hits=0,parries=0,blocks=0,incoming=0,dust_bursts=0,trail_segments=0,dropped=0,active_particles=0,active_trails=0;
 unsigned neck_spurts=0,neck_droplets=0;
 unsigned body_bursts=0,metal_bursts=0,finisher_bursts=0,grounded_blood_particles=0;Material last_material=Material::None;
 Point last_contact={},last_body_contact={},last_metal_contact={};
 Point last_body_contacts[2]={}; // defender0 player, defender1 opponent
};
class System {
public:
 void update(const WorldTrainingCombat &,const Input &,double delta);
 void stage(Point camera,const float right[3],const float up[3],Stage &)const;
 void clear();
 bool neck_spurt(Point point,Point axis,double floor_y,float strength,unsigned droplets);
 // A scene-owned landing can raise ground dust without inventing combat
 // damage, blood, trails or a fake TrainingContactEvent.
 bool ground_impact(Point floor,float strength=1);
 Stats stats()const{return stats_;}
 float camera_impulse()const;
private:
 struct Particle {Point p,v;double floor_y=0;float age=0,life=0,size=0;std::array<float,3> color={};unsigned kind=0;};
 struct Trail {Blade from,to;float age=0;unsigned actor=0;bool active=false,overlay=false;};
 std::array<Particle,particle_capacity> particles_={};std::array<Trail,trail_capacity> trails_={};
 std::array<Blade,world_combat_capacity::actors> previous_={};std::array<std::uint64_t,world_combat_capacity::actors> swing_seen_={};
 std::uint64_t event_seen_=0;
 std::array<unsigned,6> seen_={};Stats stats_;
 std::uint32_t random_=0x76241u;double combat_clock_=-1;float impulse_age_=10,impulse_strength_=0;bool paused_=false;
 float random();void burst(Point,unsigned kind,unsigned count,Point outward={0,1,0},float strength=1,double floor_y=0);void trail(unsigned,Blade,bool overlay=false);
};
}
#endif
