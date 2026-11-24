#include "engine/world_gameplay.h"
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <unistd.h>
namespace {std::atomic<bool> tracking{false};std::atomic<unsigned> allocations{0};}
[[gnu::noinline]] void *operator new(std::size_t n){if(tracking.load())allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void *operator new(std::size_t n,const std::nothrow_t &)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
[[gnu::noinline]] void operator delete(void *p,const std::nothrow_t &)noexcept{std::free(p);}
namespace {
using namespace world_gameplay;
constexpr Id trainer=(Id(1)<<60)+1,arena=trainer+1,enemy_a=trainer+2,enemy_b=trainer+3,knight_a=trainer+4,knight_b=trainer+5;
struct Scene {Vec arena_position={100,0,100};bool reject=false;};
bool exists(void *context,Id id){return !static_cast<Scene *>(context)->reject&&id>=trainer&&id<=knight_b;}
bool position(void *context,Id id,Vec &p){if(id==arena)p=static_cast<Scene *>(context)->arena_position;else p={};return true;}
Document example(){Document d;d.name="Training court prototype";d.entities={{trainer,"Trainer",true,{}},{arena,"Training court",true,{100,0,100}},{enemy_a,"Raider A",true,{}},{enemy_b,"Raider B",true,{}},{knight_a,"Knight A",true,{}},{knight_b,"Knight B",true,{}}};d.flags={{1,"Patrol accepted",false},{2,"Court discovered",false}};d.counters={{10,"Raiders defeated",0,0,2},{11,"Conversations",0,0,10000},{12,"Discoveries",0,0,10000}};
 Quest q;q.id=20;q.name="Secure the gate";q.objectives.push_back({200,10,"Defeat two raiders",2,EventKind::Kill,0});d.quests.push_back(q);
 d.encounters={{30,arena,"Gate raiders",false,{enemy_a,enemy_b}},{31,arena,"Knight practice",true,{knight_a,knight_b}}};Trigger t;t.id=60;t.entity=arena;t.name="Training entrance";t.center={2,0,0};t.extent={1,2,1};d.triggers.push_back(t);
 Dialogue dialogue;dialogue.id=40;dialogue.speaker=trainer;dialogue.entry=400;dialogue.name="Training invitation";Line intro;intro.id=400;intro.text="Would you patrol the gate or practice?";Choice patrol;patrol.text="I will secure the gate.";patrol.next=401;patrol.conditions={{ConditionKind::Flag,1,Compare::Equal,0}};patrol.actions={{ActionKind::SetFlag,1,1},{ActionKind::SetQuest,20,int(Progress::Active)},{ActionKind::StartEncounter,30,1}};Choice practice;practice.text="Let us practice swordplay.";practice.next=402;practice.actions={{ActionKind::StartTraining,31,1}};Choice leave;leave.text="Another time.";leave.actions={{ActionKind::AddCounter,12,1}};intro.choices={patrol,practice,leave};Line task;task.id=401;task.text="Defeat the raiders beyond the gate.";task.choices={{"Ready.",0,{},{}}};Line training;training.id=402;training.text="Your practice knights are ready.";dialogue.lines={intro,task,training};d.dialogues.push_back(dialogue);
 Rule talk;talk.id=50;talk.name="Talk to trainer";talk.entity=trainer;talk.cooldown=.5;talk.actions={{ActionKind::Speak,40,1},{ActionKind::AddCounter,11,1}};d.rules.push_back(talk);Rule enter;enter.id=51;enter.name="Discover training court";enter.event=EventKind::TriggerEnter;enter.entity=arena;enter.key=60;enter.actions={{ActionKind::SetFlag,2,1},{ActionKind::AddCounter,12,1}};d.rules.push_back(enter);return d;}
bool has(const Commands &c,CommandKind kind,Id key=0){for(unsigned i=0;i<c.count;i++)if(c.values[i].kind==kind&&(!key||c.values[i].key==key))return true;return false;}
std::string read(const std::string &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::string state_bytes(const Snapshot &s){const auto p="/tmp/valwood-gameplay-state-compare-"+std::to_string(getpid());std::string e;assert(save_state(p.c_str(),s,e));const auto text=read(p);unlink(p.c_str());return text;}
void run(Runtime *r,Commands &out,double dt=.016,Vec p={}){assert(tick(r,dt,p,out)==Result::Ok);}
}
int main(){
 using namespace world_gameplay;std::string error;auto d=example();Scene scene;Bindings bindings{&scene,exists,position};assert(validate(d,error,bindings));auto *runtime=create();assert(runtime&&configure(runtime,d,error,bindings));Commands out;
 assert(enqueue(runtime,{EventKind::Interact,trainer}));run(runtime,out);assert(has(out,CommandKind::Speak,40)&&current_line(runtime)->id==400&&choices(runtime).count==3&&counter(runtime,11)==1);
 assert(choose(runtime,0));run(runtime,out);assert(flag(runtime,1)==1&&quest(runtime,20)==Progress::Active&&encounter(runtime,30)==Progress::Active&&has(out,CommandKind::StartEncounter,30)&&current_line(runtime)->id==401);assert(choose(runtime,0));run(runtime,out);assert(!current_line(runtime)&&has(out,CommandKind::EndDialogue));
 assert(enqueue(runtime,{EventKind::Kill,enemy_a}));run(runtime,out);assert(counter(runtime,10)==1&&quest(runtime,20)==Progress::Active);assert(enqueue(runtime,{EventKind::Kill,enemy_a}));run(runtime,out);assert(counter(runtime,10)==1); // duplicate death cannot farm progress
 // Trigger volumes follow actual host entity positions and fire once across
 // exits, re-entry, save/resume and attachment movement.
 run(runtime,out,.016,{102,0,100});assert(flag(runtime,2)==1&&counter(runtime,12)==1);run(runtime,out,.016,{});run(runtime,out,.016,{102,0,100});assert(counter(runtime,12)==1);scene.arena_position={200,0,100};run(runtime,out,.016,{202,0,100});assert(counter(runtime,12)==1);
 const auto snapshot_mid=snapshot(runtime);const auto doc_path="/tmp/valwood-gameplay-test-"+std::to_string(getpid())+".gameplay",state_path=doc_path+".state";assert(save(doc_path.c_str(),d,error)&&save_state(state_path.c_str(),snapshot_mid,error));Document loaded;Snapshot loaded_state;assert(load(doc_path.c_str(),loaded,error,bindings)&&load_state(state_path.c_str(),loaded_state,error));assert(loaded.entities[0].id==trainer&&state_bytes(loaded_state)==state_bytes(snapshot_mid));
 auto *resumed=create();assert(configure(resumed,loaded,error,bindings)&&restore(resumed,loaded_state,error));assert(enqueue(resumed,{EventKind::Interact,trainer}));run(resumed,out,0);assert(!current_line(resumed)); // cooldown survived save
 assert(enqueue(resumed,{EventKind::Kill,enemy_a}));run(resumed,out);assert(counter(resumed,10)==1);assert(enqueue(resumed,{EventKind::Kill,enemy_b}));run(resumed,out);assert(counter(resumed,10)==2&&quest(resumed,20)==Progress::Complete&&encounter(resumed,30)==Progress::Complete&&has(out,CommandKind::QuestChanged,20)&&has(out,CommandKind::EncounterChanged,30));
 assert(enqueue(resumed,{EventKind::Interact,trainer}));run(resumed,out,.6);assert(current_line(resumed)->id==400&&choices(resumed).count==2);assert(choose(resumed,0));run(resumed,out);assert(current_line(resumed)->id==400); // condition blocks accepted quest branch
 assert(choose(resumed,1));run(resumed,out);assert(encounter(resumed,31)==Progress::Active&&has(out,CommandKind::StartTraining,31)&&current_line(resumed)->id==402);assert(close_dialogue(resumed));run(resumed,out);assert(!current_line(resumed));assert(enqueue(resumed,{EventKind::EncounterFinished,0,31,0,1}));run(resumed,out);assert(encounter(resumed,31)==Progress::Complete);assert(enqueue(resumed,{EventKind::EncounterFinished,0,31,0,1}));run(resumed,out);assert(out.count==0);
 // Graph and file validation is atomic, including cyclic/unreachable dialogue,
 // missing scene references, invalid targets and corrupted save state.
 const auto saved_doc=read(doc_path),before=state_bytes(snapshot(resumed));auto cycle=d;cycle.dialogues[0].lines[0].choices[0].next=400;assert(!validate(cycle,error)&&!save(doc_path.c_str(),cycle,error)&&read(doc_path)==saved_doc);auto dangling=d;dangling.dialogues[0].lines[0].choices[0].next=999;assert(!validate(dangling,error));scene.reject=true;assert(!configure(resumed,d,error,bindings)&&state_bytes(snapshot(resumed))==before);scene.reject=false;
 auto bad_state=loaded_state;bad_state.signature^=1;assert(!restore(resumed,bad_state,error)&&state_bytes(snapshot(resumed))==before);bad_state=loaded_state;bad_state.counters[0].value=3;assert(!restore(resumed,bad_state,error));bad_state=loaded_state;bad_state.encounters[0].defeated=65535;assert(!restore(resumed,bad_state,error));bad_state=loaded_state;bad_state.rules[0].cooldown=std::numeric_limits<double>::quiet_NaN();const auto saved_state=read(state_path);assert(!save_state(state_path.c_str(),bad_state,error)&&read(state_path)==saved_state);
 {std::ofstream f(doc_path);f<<saved_doc<<"rule 999 \"Bad\" 0 999999 0 0 0 0 1 3 777777 1\n";}loaded.name="Preserved";assert(!load(doc_path.c_str(),loaded,error,bindings)&&loaded.name=="Preserved");{std::ofstream f(state_path);f<<saved_state<<"trailing";}loaded_state.signature=123;assert(!load_state(state_path.c_str(),loaded_state,error)&&loaded_state.signature==123);
 // Work and output budgets roll back all flag/counter/choice/command changes.
 auto *limited=create();Budget tiny;tiny.actions=1;assert(configure(limited,d,error,bindings,tiny));assert(enqueue(limited,{EventKind::Interact,trainer}));const auto pristine=state_bytes(snapshot(limited));out.count=77;assert(tick(limited,.016,{},out)==Result::BudgetExceeded&&out.count==77&&state_bytes(snapshot(limited))==pristine);assert(tick(limited,.016,{},out)==Result::BudgetExceeded);destroy(limited);
 limited=create();tiny={};tiny.commands=1;assert(configure(limited,d,error,bindings,tiny));assert(enqueue(limited,{EventKind::Interact,trainer}));run(limited,out);assert(choose(limited,0));const auto dialogue_before=state_bytes(snapshot(limited));assert(tick(limited,.016,{},out)==Result::BudgetExceeded&&state_bytes(snapshot(limited))==dialogue_before&&flag(limited,1)==0);destroy(limited);
 // Fixed queue capacity rejects excess input; accepted backlog drains through
 // bounded ticks instead of being lost or creating an unbounded event loop.
 auto queue_doc=d;queue_doc.rules[0].cooldown=0;auto *queued=create();assert(configure(queued,queue_doc,error,bindings));for(unsigned i=0;i<queue_limit;i++)assert(enqueue(queued,{EventKind::Interact,trainer}));assert(!enqueue(queued,{EventKind::Interact,trainer}));run(queued,out,0);assert(counter(queued,11)==32);for(unsigned i=0;i<3;i++)run(queued,out,0);assert(counter(queued,11)==128);assert(enqueue(queued,{EventKind::Interact,trainer}));run(queued,out,0);assert(counter(queued,11)==129);
 const auto allocation_before=allocations.load();tracking=true;for(unsigned i=0;i<10000;i++){if(i%60==0)assert(enqueue(queued,{EventKind::Interact,trainer}));assert(tick(queued,1./60,{},out)==Result::Ok);const auto available=choices(queued);assert(available.count<=8);}tracking=false;assert(allocations==allocation_before);
 const auto stable=state_bytes(snapshot(queued));out.count=65;assert(tick(queued,std::numeric_limits<double>::quiet_NaN(),{},out)==Result::InvalidInput&&out.count==65&&state_bytes(snapshot(queued))==stable);assert(!enqueue(queued,{EventKind::Kill,enemy_a,0,0,100}));assert(!enqueue(queued,{EventKind::EncounterFinished,0,31,0,-1}));
 // Explicit cooldown actions override a rule's default, including self-targets.
 auto cooldown_doc=d;cooldown_doc.rules[0].actions.push_back({ActionKind::SetCooldown,50,1000});auto *cooldown_runtime=create();assert(configure(cooldown_runtime,cooldown_doc,error,bindings));assert(enqueue(cooldown_runtime,{EventKind::Interact,trainer}));run(cooldown_runtime,out);assert(enqueue(cooldown_runtime,{EventKind::Interact,trainer}));run(cooldown_runtime,out,.6);assert(counter(cooldown_runtime,11)==1);assert(enqueue(cooldown_runtime,{EventKind::Interact,trainer}));run(cooldown_runtime,out,.41);assert(counter(cooldown_runtime,11)==2);destroy(cooldown_runtime);
 // Deactivating a speaker from a choice closes the conversation without
 // emitting a dangling/zero dialogue command or reopening an inactive speaker.
 auto inactive_doc=d;inactive_doc.dialogues[0].lines[0].choices[0].actions.push_back({ActionKind::ActivateEntity,trainer,0});auto *inactive_runtime=create();assert(configure(inactive_runtime,inactive_doc,error,bindings));assert(enqueue(inactive_runtime,{EventKind::Interact,trainer}));run(inactive_runtime,out);assert(choose(inactive_runtime,0));run(inactive_runtime,out);assert(!current_line(inactive_runtime));for(unsigned i=0;i<out.count;i++)if(out.values[i].kind==CommandKind::EndDialogue)assert(out.values[i].key==40);destroy(inactive_runtime);
 // Deferred automatic trigger events are committed to the live queue and
 // survive save/load. The once flag advances when consumed, not when queued.
 auto deferred_doc=d;deferred_doc.rules[0].cooldown=0;Budget one_event;one_event.events=1;
 auto *deferred=create();assert(configure(deferred,deferred_doc,error,bindings,one_event));
 scene.arena_position={100,0,100};assert(enqueue(deferred,{EventKind::Interact,trainer}));
 run(deferred,out,.016,{102,0,100});assert(flag(deferred,2)==0&&snapshot(deferred).queued_count==1);
 const auto deferred_snapshot=snapshot(deferred);assert(deferred_snapshot.queued[0].kind==EventKind::TriggerEnter);
 assert(save_state(state_path.c_str(),deferred_snapshot,error)&&load_state(state_path.c_str(),loaded_state,error));
 assert(configure(deferred,deferred_doc,error,bindings,one_event)&&restore(deferred,loaded_state,error));
 run(deferred,out,.016,{102,0,100});assert(flag(deferred,2)==1&&counter(deferred,12)==1&&snapshot(deferred).queued_count==0);
 run(deferred,out,.016,{});run(deferred,out,.016,{102,0,100});assert(counter(deferred,12)==1);destroy(deferred);
 // Killing the final encounter actor produces the same typed finished event
 // as host-reported training. Quotas may defer it, and persistence retains it.
 auto finished_doc=d;Rule finished_rule;finished_rule.id=52;finished_rule.name="Encounter reward";
 finished_rule.event=EventKind::EncounterFinished;finished_rule.key=30;finished_rule.actions={{ActionKind::AddCounter,12,1}};finished_doc.rules.push_back(finished_rule);
 auto *finished=create();assert(configure(finished,finished_doc,error,bindings,one_event));
 assert(enqueue(finished,{EventKind::Interact,trainer}));run(finished,out);assert(choose(finished,0));run(finished,out);
 assert(!enqueue(finished,{EventKind::Kill,knight_a,30}));
 assert(enqueue(finished,{EventKind::Kill,enemy_a}));run(finished,out);
 assert(enqueue(finished,{EventKind::Kill,enemy_b}));run(finished,out);
 assert(encounter(finished,30)==Progress::Active&&snapshot(finished).queued_count==1&&counter(finished,12)==0);
 const auto finishing=snapshot(finished);assert(finishing.queued[0].kind==EventKind::EncounterFinished);
 assert(save_state(state_path.c_str(),finishing,error)&&load_state(state_path.c_str(),loaded_state,error));
 assert(configure(finished,finished_doc,error,bindings,one_event)&&restore(finished,loaded_state,error));run(finished,out);
 assert(encounter(finished,30)==Progress::Complete&&counter(finished,12)==1&&has(out,CommandKind::EncounterChanged,30));
 auto corrupt_queue=snapshot(finished);corrupt_queue.queued_count=1;corrupt_queue.queued[0]={EventKind::Kill,999999};
 const auto finished_before=state_bytes(snapshot(finished));assert(!restore(finished,corrupt_queue,error)&&state_bytes(snapshot(finished))==finished_before);destroy(finished);
 // Invalid/out-of-range choices cannot fire dialogue choice rules.
 auto choice_doc=d;Rule choice_rule;choice_rule.id=53;choice_rule.name="Choice observed";choice_rule.event=EventKind::DialogueChoice;choice_rule.key=40;choice_rule.actions={{ActionKind::AddCounter,12,1}};choice_doc.rules.push_back(choice_rule);
 auto *invalid_choice=create();assert(configure(invalid_choice,choice_doc,error,bindings));assert(enqueue(invalid_choice,{EventKind::Interact,trainer}));run(invalid_choice,out);
 assert(enqueue(invalid_choice,{EventKind::DialogueChoice,0,40,400,7}));run(invalid_choice,out);assert(counter(invalid_choice,12)==0&&current_line(invalid_choice)->id==400);destroy(invalid_choice);
 // Legacy state v1 loads with an empty pending queue. Negative/signed ID
 // spellings and embedded controls are rejected atomically in all codecs.
 assert(save_state(state_path.c_str(),snapshot(runtime),error));auto state_v1=read(state_path);const auto header_end=state_v1.find('\n');std::string header=state_v1.substr(0,header_end);header.replace(23,1,"1");header.erase(header.find_last_of(' '));state_v1=header+state_v1.substr(header_end);
 {std::ofstream f(state_path);f<<state_v1;}assert(load_state(state_path.c_str(),loaded_state,error)&&loaded_state.queued_count==0);
 auto negative_doc=d;negative_doc.entities[0].name=std::string("bad\0name",8);assert(!validate(negative_doc,error));
 std::string negative_bytes=saved_doc;const auto entity_at=negative_bytes.find("entity ")+7;negative_bytes.replace(entity_at,negative_bytes.find(' ',entity_at)-entity_at,"-1");
 {std::ofstream f(doc_path);f<<negative_bytes;}loaded.name="Preserved";assert(!load(doc_path.c_str(),loaded,error)&&loaded.name=="Preserved");
 std::string negative_state=state_v1;const auto signature_at=negative_state.find("1 ")+2;negative_state.replace(signature_at,negative_state.find(' ',signature_at)-signature_at,"-1");
 {std::ofstream f(state_path);f<<negative_state;}loaded_state.signature=123;assert(!load_state(state_path.c_str(),loaded_state,error)&&loaded_state.signature==123);
 destroy(queued);destroy(resumed);destroy(runtime);unlink(doc_path.c_str());unlink(state_path.c_str());
 std::cout<<"Gameplay: dialogue branches/conditions, training/encounter host commands, unique deaths, attached trigger once, quest progress, persistent cooldown/save resume,64-bit identities, malformed cycles/references, atomic budgets/codec failures, deferred trigger/completion queue save-resume, strict unsigned IDs, bounded queue carry-over and10000 allocation-free fixed ticks passed\n";
}
