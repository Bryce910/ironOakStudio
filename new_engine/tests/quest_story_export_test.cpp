#include "engine/world_gameplay.h"
#include <cassert>
#include <iostream>
#include <string>
// This intentionally uses the actual engine parser/runtime, not a second text
// parser copied into a test. It never loads or edits a game project/save.
int main(int argc,char **argv){
 using namespace world_gameplay;assert(argc==3);std::string error;Document d;
 assert(load(argv[1],d,error));assert(validate(d,error));
 const Bindings absent{nullptr,[](void *,Id){return false;},nullptr};
 assert(!validate(d,error,absent)); // sample IDs require real scene binding
 assert(d.entities.size()==2&&d.entities[0].id==9007199254740993ull&&d.entities[1].id==18446744073709551614ull);
 auto *r=create();assert(r&&configure(r,d,error));Commands out;
 auto tick=[&](){assert(world_gameplay::tick(r,0,{1000000,1000000,1000000},out)==Result::Ok);};
 auto talk=[&](){assert(enqueue(r,{EventKind::Interact,d.entities[0].id}));tick();};
 talk();assert(current_line(r)&&current_line(r)->id==41&&choices(r).count==2);
 assert(choose(r,1));tick();assert(!current_line(r)&&quest(r,30)==Progress::Inactive);
 // Inspecting before acceptance cannot pre-complete the quest.
 assert(enqueue(r,{EventKind::Interact,d.entities[1].id}));tick();assert(counter(r,20)==0);
 talk();assert(choose(r,0));tick();assert(flag(r,10)==1&&quest(r,30)==Progress::Active&&current_line(r)->id==42);
 assert(choose(r,0));tick();assert(!current_line(r));talk();assert(current_line(r)->id==71);assert(close_dialogue(r));tick();
 assert(enqueue(r,{EventKind::Interact,d.entities[1].id}));tick();assert(quest(r,30)==Progress::Complete&&counter(r,20)==1&&counter(r,21)==0);
 // Save/resume precedes claiming the reward; a compatible loaded document
 // must restore the exact phase and allow the one-time payout afterwards.
 const auto saved=snapshot(r);std::string encoded;assert(encode_state(saved,encoded,error));Snapshot decoded;assert(decode_state(encoded,decoded,error));
 auto *resumed=create();assert(resumed&&configure(resumed,d,error)&&restore(resumed,decoded,error));destroy(r);r=resumed;
 talk();assert(flag(r,11)==1&&counter(r,21)==25&&current_line(r)->id==61);assert(close_dialogue(r));tick();talk();assert(counter(r,21)==25&&!current_line(r));
 const auto report=snapshot(r);assert(report.quests[0].value==2);destroy(r);
 // A second fixture exercises all record types, C++ std::quoted escape
 // handling and maximum IDs; load catches enum/serialization disagreements.
 Document full;assert(load(argv[2],full,error));assert(full.entities[0].name=="Guide \\ \"stone\"");
 assert(full.triggers.size()==1&&full.encounters.size()==1&&full.rules.size()==3);
 auto *f=create();assert(f&&configure(f,full,error));assert(enqueue(f,{EventKind::Interact,full.entities[0].id}));assert(world_gameplay::tick(f,0,{99,99,99},out)==Result::Ok);assert(encounter(f,80)==Progress::Active);
 assert(enqueue(f,{EventKind::Kill,full.entities[1].id,80}));assert(world_gameplay::tick(f,0,{99,99,99},out)==Result::Ok);assert(encounter(f,80)==Progress::Complete);destroy(f);
 std::cout<<"{\"ok\":true,\"quest\":2,\"progress\":1,\"reward\":25,\"accepted\":1,\"paid\":1,\"engine\":\"world_gameplay\",\"scenarios\":[\"decline\",\"preaccept-no-progress\",\"accept\",\"reminder\",\"complete\",\"save-resume\",\"one-time-reward\",\"all-record-codec\",\"encounter-completion\"]}\n";
}
