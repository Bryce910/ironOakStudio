#include "engine/world_training_combat.h"
#include "engine/world_arena_combat.h"
#include <cassert>
#include <cstdio>
#include <limits>
namespace A=world_combat_anatomy;
using Part=TrainingBodyPart;using Hand=TrainingWeaponHand;using Dir=TrainingDirection;using EP=TrainingEnemyPhase;
static unsigned checks=0;
#define CHECK(x) do{++checks;assert(x);}while(false)
struct Geometry {
 Part part=Part::RightHand;bool miss=false,guard=false;unsigned calls=0,invalid=0;
 A::Sweep edge{{{-1,1.1,-.05},{-1,1.1,.05}},{{1,1.1,-.05},{1,1.1,.05}},.01};
};
static bool geometry(const TrainingAnatomicalQuery &q,TrainingAnatomicalHit &h,void *context){
 auto &g=*static_cast<Geometry*>(context);++g.calls;
 CHECK(q.contact_clock>=0&&q.phase_begin==.26&&q.phase_end==.36&&q.windup_seconds>0);
 A::Capsule c{g.guard&&q.weapon_guard?Part::None:g.part,{g.miss?4.:0.,1.1,0},{g.miss?4.:0.,1.1,0},.10};
 const auto hit=A::sweep_contact(g.edge,&c,1,q.severed_parts);if(!hit.hit)return false;
 h.valid=true;h.weapon_contact=c.part==Part::None;h.cutting_edge=true;h.part=hit.part;h.sweep_t=hit.time;h.point[0]=hit.point.x;h.point[1]=hit.point.y;h.point[2]=hit.point.z;
 if(g.invalid==1)h.valid=false;
 if(g.invalid==2)h.point[0]=NAN;
 if(g.invalid==3)h.sweep_t=2;
 if(g.invalid==4)h.part=Part(99);
 if(g.invalid==5)h.weapon_contact=true;
 return true;
}
struct Fight {WorldTrainingCombat c;TrainingOpponent o;TrainingTarget t;TrainingEvents events;Geometry g;
 Fight(){c.contact_query=geometry;c.contact_query_context=&g;CHECK(c.start_shared_arena());c.state=TrainingState::Fighting;c.impact_pause_enabled=false;o.phase=EP::Recovery;o.duration=20;t={&o,0,1.8,true,true,0,1.8,true};}
 void tick(double seconds,double hz=120){for(unsigned i=0;i<unsigned(std::ceil(seconds*hz));++i)CHECK(c.update_shared(1./hz,&t,1,events));}
 void swing(Dir d=Dir::Left){c.choose(d);CHECK(c.attack());const auto timing=c.committed_timing();tick(timing.windup+timing.strike+timing.recovery+.025);}
 const TrainingContactEvent &contact(TrainingContactKind kind=TrainingContactKind::PlayerBody)const{for(unsigned i=0;i<events.count;++i)if(events.values[i].kind==kind)return events.values[i];assert(false);return events.values[0];}
 unsigned contacts(TrainingContactKind kind)const{unsigned n=0;for(unsigned i=0;i<events.count;++i)n+=events.values[i].kind==kind;return n;}
};
int main(){
 // Continuous contact really distinguishes anatomy and picks the first surface.
 A::Sweep sweep{{{-1,1.7,-.05},{-1,1.7,.05}},{{1,1.7,-.05},{1,1.7,.05}},.01};
 A::Capsule caps[]={{Part::RightHand,{0,1.0,0},{0,1.0,0},.1},{Part::Head,{0,1.7,0},{0,1.7,0},.1}};
 auto hit=A::sweep_contact(sweep,caps,2);CHECK(hit.hit&&hit.part==Part::Head&&std::abs(hit.time-.445)<1e-4);
 CHECK(!A::sweep_contact(sweep,caps,2,A::bit(Part::Head)).hit);
 caps[0]={Part::LeftHand,{-.4,1.7,0},{-.4,1.7,0},.1};hit=A::sweep_contact(sweep,caps,2);CHECK(hit.part==Part::LeftHand&&hit.time<.25);
 for(auto &v:{&sweep.from.base,&sweep.from.tip,&sweep.to.base,&sweep.to.tip}){v->x+=100000;v->y-=30000;v->z+=200000;}
 for(auto &c:caps){c.a=c.a+A::Vec3{100000,-30000,200000};c.b=c.b+A::Vec3{100000,-30000,200000};}
 const auto translated=A::sweep_contact(sweep,caps,2);CHECK(translated.part==hit.part&&std::abs(translated.time-hit.time)<1e-9);
 auto invalid=sweep;invalid.to.tip.x=std::numeric_limits<double>::quiet_NaN();CHECK(!A::sweep_contact(invalid,caps,2).hit);CHECK(!A::sweep_contact(sweep,caps,25).hit);
 // Analytic point/sphere oracle, including very fast sweeps and near tangency.
 for(unsigned i=0;i<600;++i){const double offset=(int(i%61)-30)*.003;A::Sweep s{{{-2,offset,0},{-2,offset,0}},{{2,offset,0},{2,offset,0}},0};A::Capsule c{Part::Head,{0,0,0},{0,0,0},.1};auto h=A::sweep_contact(s,&c,1);CHECK(h.hit);const double exact=(2-std::sqrt(.01-offset*offset))/4;CHECK(std::abs(h.time-exact)<1e-4);}
 {A::Sweep s{{{-1,.1,0},{-1,.1,0}},{{1,.1,0},{1,.1,0}},0};A::Capsule c{Part::Head,{0,0,0},{0,0,0},.1};CHECK(A::sweep_contact(s,&c,1).hit);s.from.base.y=s.from.tip.y=s.to.base.y=s.to.tip.y=.1001;CHECK(!A::sweep_contact(s,&c,1).hit);}
 // A LEFT cut can take a real head; HIGH on a hand must never pretend neck.
 for(Part part:{Part::Head,Part::Torso,Part::LeftLeg,Part::RightLeg}){Fight f;f.g.part=part;f.swing(Dir::Left);CHECK(f.o.health==0&&f.contacts(TrainingContactKind::EnemyDefeated)==1);CHECK(f.o.severed_parts&A::bit(part));CHECK(f.o.decapitated==(part==Part::Head));CHECK(f.contact().anatomical&&f.contact().severed_part==part&&f.contact().finisher);}
 {Fight f;f.g.part=Part::RightHand;f.o.health=2;f.swing(Dir::High);CHECK(f.o.health==1&&!f.o.decapitated&&f.o.weapon_hand==Hand::Left);CHECK(f.contacts(TrainingContactKind::EnemyDefeated)==0);CHECK(f.contact().body_part==Part::RightHand&&f.contact().point[1]==1.1);const unsigned oldhits=f.c.hits;f.swing();CHECK(f.c.hits==oldhits);f.g.part=Part::LeftArm;f.swing();CHECK(f.o.health==1&&f.o.weapon_hand==Hand::None&&!A::can_fight(f.o.severed_parts));f.g.part=Part::Torso;f.swing(Dir::Thrust);CHECK(f.o.health==0&&f.contacts(TrainingContactKind::EnemyDefeated)==1);}
 {Fight f;f.g.part=Part::RightArm;f.swing();CHECK(f.o.health>0&&f.o.weapon_hand==Hand::Left);CHECK(A::removed(f.o.severed_parts,Part::RightHand));const auto view=f.c.opponent_view(f.o);CHECK(view.enemy_severed_parts==f.o.severed_parts&&view.enemy_weapon_hand==Hand::Left);}
 // A hand loss interrupts the committed old-hand windup, then the survivor
 // legitimately resumes attacks after the handover stagger and Measure.
 {Fight f;f.o.phase=EP::Windup;f.o.duration=.22;f.g.part=Part::RightHand;f.c.choose(Dir::Left);CHECK(f.c.attack());f.o.duration=f.c.committed_timing().windup+.08;f.tick(f.c.committed_timing().windup+.10);CHECK(f.o.phase==EP::Stagger&&f.o.weapon_hand==Hand::Left&&f.c.health==100&&f.o.hits_dealt==0);CHECK(f.o.duration==.35);f.tick(2.);CHECK(f.o.hits_dealt>0&&f.c.health<100&&f.o.health>0);CHECK(f.contact(TrainingContactKind::EnemyBody).weapon_hand==Hand::Left&&f.contact(TrainingContactKind::EnemyBody).severed_parts==A::bit(Part::RightHand));}
 {Fight f;f.g.part=Part::RightHand;f.swing();f.g.part=Part::LeftHand;f.swing();const auto hp=f.c.health;const auto hits=f.o.hits_dealt;CHECK(f.o.health>0&&f.o.weapon_hand==Hand::None);f.o.phase=EP::Windup;f.o.duration=.01;f.o.time=0;f.tick(2.);CHECK(f.c.health==hp&&f.o.hits_dealt==hits&&f.o.phase==EP::Measure);f.o.guard=Dir::High;f.g.guard=true;f.g.part=Part::Torso;f.swing(Dir::High);CHECK(f.o.health==0&&f.c.guarded==0);}
 {Fight f;f.g.part=Part::Torso;f.swing(Dir::Thrust);CHECK(f.o.health==0&&f.o.severed_parts==0&&!f.o.decapitated&&f.contact().severed_part==Part::None);}
 {Fight f;f.g.part=Part::Head;f.swing(Dir::Thrust);CHECK(f.o.health==0&&!f.o.severed_parts&&!f.o.decapitated);CHECK(f.c.feedback==std::string_view("LETHAL HEAD THRUST"));f.swing();CHECK(f.contacts(TrainingContactKind::EnemyDefeated)==1);}
 for(auto weapon:{world_weapon_loadout::Weapon::Staff,world_weapon_loadout::Weapon::SwordShield,world_weapon_loadout::Weapon::GreatAxe}){Fight f;f.c.weapon=weapon;f.g.part=Part::Head;f.swing();CHECK((f.o.health==0)==world_weapon_loadout::profile(weapon).edged);CHECK((f.o.severed_parts!=0)==world_weapon_loadout::profile(weapon).edged);}
 {Fight f;f.g.part=Part::Head;f.g.miss=true;f.swing(Dir::High);CHECK(f.o.health==90&&f.o.severed_parts==0&&f.c.misses==1);}
 for(unsigned bad=1;bad<=5;++bad){Fight f;f.g.invalid=bad;f.swing();CHECK(f.o.health==90&&f.o.severed_parts==0&&f.c.misses==1);}
 {Fight f;f.g.guard=true;f.o.phase=EP::Measure;f.o.guard=Dir::Left;f.swing();CHECK(f.o.health==90&&f.o.severed_parts==0&&f.c.guarded==1);CHECK(f.contact(TrainingContactKind::EnemyBlock).anatomical);}
 // No geometry callback can ever grant a direction-only sever, even lethal HIGH.
 {Fight f;f.c.contact_query=nullptr;f.o.health=10;f.swing(Dir::High);CHECK(f.o.health==0&&!f.o.decapitated&&!f.o.severed_parts);}
 // Actual guard geometry can miss while its direction matches; body wins then.
 {Fight f;f.o.phase=EP::Measure;f.o.guard=Dir::Left;f.g.part=Part::Torso;f.swing();CHECK(f.o.health==0&&f.c.guarded==0);}
 // Trainer lessons remain nonfatal presentation; arrows cannot acquire sever bits.
 {Fight f;f.c.arena_mode=f.c.trail_threat=false;f.g.part=Part::Head;f.swing();CHECK(f.o.health==75&&!f.o.severed_parts);}
 {Fight f;f.o.severed_parts=A::bit(Part::RightHand);f.o.weapon_hand=Hand::Left;CHECK(f.c.damage_opponent(f.o,0,100,Dir::High,f.events));CHECK(f.o.health==0&&f.o.severed_parts==A::bit(Part::RightHand)&&!f.o.decapitated);CHECK(f.contact().severed_parts==f.o.severed_parts&&f.contact().weapon_hand==Hand::Left&&f.contact().severed_part==Part::None);}
 // Difficulty reacts to observed attacks after a positive delay, never input-time.
 for(auto level:{world_combat_difficulty::Difficulty::Story,world_combat_difficulty::Difficulty::Normal,world_combat_difficulty::Difficulty::Hard,world_combat_difficulty::Difficulty::Expert})for(double hz:{30.,60.,144.}){
  Fight f;f.c.difficulty=level;f.g.miss=true;f.o.phase=EP::Measure;f.o.duration=20;f.o.guard=Dir::Right;f.c.choose(Dir::Left);CHECK(f.c.attack());CHECK(f.o.guard==Dir::Right);
  f.tick(.05,hz);CHECK(f.o.guard==Dir::Right);const double scheduled=f.o.guard_reaction_at;const auto clock=f.c.clock;CHECK(f.c.update_shared(.2,&f.t,1,f.events,true));CHECK(f.c.clock==clock&&f.o.guard==Dir::Right);
  while(f.c.clock<scheduled-.01)f.tick(1./hz,hz);
  while(f.o.guard_reactions==0&&f.c.clock<1)f.tick(1./hz,hz);
  CHECK(f.o.guard==Dir::Left&&f.o.guard_reactions==1&&f.c.clock>=scheduled);CHECK(scheduled>=world_combat_difficulty::reaction_delay_s(level,false));
  Fight attack;attack.c.difficulty=level;attack.o.phase=EP::Measure;attack.o.duration=.001;attack.tick(.01);CHECK(attack.o.phase==EP::Windup);CHECK(std::abs(attack.o.duration-.72/world_combat_difficulty::profile(level).attack_tempo_scale)<1e-9);
 }
 {Fight f;f.c.difficulty=world_combat_difficulty::Difficulty::Expert;f.g.miss=true;f.o.phase=EP::Stagger;f.o.duration=20;f.o.guard=Dir::Right;f.swing();CHECK(f.o.guard==Dir::Right&&f.o.guard_reactions==0&&f.o.guard_reaction_pending);}
 {Fight f;f.c.difficulty=world_combat_difficulty::Difficulty::Expert;f.g.miss=true;f.o.phase=EP::Measure;f.o.duration=20;f.o.guard=Dir::Right;f.swing();f.o.guard=Dir::Right;f.c.choose(Dir::Left);CHECK(f.c.attack()&&f.c.repeated==1&&f.o.guard==Dir::Right);f.tick(.01);CHECK(f.o.guard_reaction_at-f.c.clock>=.05&&f.o.guard_reaction_at-f.c.clock<=.0651);f.tick(.1);CHECK(f.o.guard==Dir::Left&&f.o.guard_reactions==2);}
 // Reset/start preserves the geometry bridge and preference, clears sever state.
 {WorldTrainingCombat c;Geometry g;c.contact_query=geometry;c.contact_query_context=&g;c.difficulty=world_combat_difficulty::Difficulty::Hard;c.enemy_severed_parts=255;c.reset_session();CHECK(c.contact_query==geometry&&c.contact_query_context==&g&&c.difficulty==world_combat_difficulty::Difficulty::Hard&&!c.enemy_severed_parts);world_arena_combat::Arena arena;CHECK(arena.start(c,{2,2}));CHECK(c.contact_query==geometry&&c.contact_query_context==&g);CHECK(arena.opponent(0)->severed_parts==0&&arena.opponent(0)->weapon_hand==Hand::Right);}
 for(unsigned mode=0;mode<3;++mode){WorldTrainingCombat c;Geometry g;c.contact_query=geometry;c.contact_query_context=&g;c.difficulty=world_combat_difficulty::Difficulty::Expert;const bool started=mode==0?c.start(c.instructor_x,25.75,c.instructor_z):mode==1?c.start_threat():c.start_arena();CHECK(started&&c.contact_query==geometry&&c.contact_query_context==&g&&c.difficulty==world_combat_difficulty::Difficulty::Expert);}
 {WorldTrainingCombat c;Geometry g;c.contact_query=geometry;c.contact_query_context=&g;CHECK(c.start_threat());c.state=TrainingState::Fighting;c.impact_pause_enabled=false;c.enemy_health=90;c.enemy_phase=EP::Recovery;c.enemy_duration=20;c.choose(Dir::Left);CHECK(c.attack());for(unsigned i=0;i<unsigned(std::ceil((c.committed_timing().windup+.10)/.01));++i)c.update(.01,c.enemy_x+1.8,c.enemy_z);CHECK(c.enemy_health>0&&c.enemy_severed_parts==A::bit(Part::RightHand)&&c.enemy_weapon_hand==Hand::Left&&c.enemy_phase==EP::Stagger);}
 {Fight f;const auto before=f.c.clock;f.o.severed_parts=256;CHECK(!f.c.update_shared(.1,&f.t,1,f.events)&&f.c.clock==before);f.o.severed_parts=0;f.o.weapon_hand=Hand(99);CHECK(!f.c.update_shared(.1,&f.t,1,f.events)&&f.c.clock==before);f.o.weapon_hand=Hand::Right;f.o.guard_reaction_at=NAN;CHECK(!f.c.update_shared(.1,&f.t,1,f.events)&&f.c.clock==before);}
 // Explicit geometric fixture helper cannot inject injury/health/reward state.
 {world_arena_combat::Arena a;WorldTrainingCombat c;CHECK(a.start(c,{1,1}));const auto player=a.player_spawn();for(unsigned i=0;i<1200&&a.active_count()!=1;++i)a.update(1./120.,c,player);CHECK(a.active_count()==1);const auto hp=a.opponent(0)->health;const auto mask=a.opponent(0)->severed_parts;const auto hand=a.opponent(0)->weapon_hand;const world_arena_combat::Vec3 held{1,0,1};CHECK(a.fixture_geometry_pose(0,held,1.f,Dir::Low,c.attack_start_serial));for(unsigned i=0;i<30;++i)a.update(1./120.,c,player);CHECK(a.challengers()[0].position.x==held.x&&a.challengers()[0].position.z==held.z);CHECK(a.opponent(0)->health==hp&&a.opponent(0)->severed_parts==mask&&a.opponent(0)->weapon_hand==hand&&!a.defeated());CHECK(!a.fixture_geometry_pose(1,held,1,Dir::Low,0));CHECK(!a.fixture_geometry_pose(0,{1,2,1},1,Dir::Low,0));CHECK(!a.fixture_geometry_pose(0,{100,0,1},1,Dir::Low,0));CHECK(!a.fixture_geometry_pose(0,held,NAN,Dir::Low,0));CHECK(!a.fixture_geometry_pose(0,held,1,Dir(5),0));a.fixture_release_geometry();a.update(.1,c,player);CHECK(a.challengers()[0].position.x!=held.x||a.challengers()[0].position.z!=held.z);CHECK(a.opponent(0)->health==hp&&a.opponent(0)->severed_parts==mask&&a.opponent(0)->weapon_hand==hand);}
 std::printf("PASS %u anatomy/swept-edge/oracle/sever-survival/state/defense/reset checks\n",checks);
}
