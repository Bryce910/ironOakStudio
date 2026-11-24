#include "engine/world_project_checkpoint.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
int main(){
    namespace fs=std::filesystem;using namespace world_project;char temp[]="/tmp/valwood-project-checkpoint-XXXXXX";assert(mkdtemp(temp));const std::string root=temp;std::string e;
    auto write=[&](const std::string &p,const std::string &s){fs::create_directories(fs::path(root)/fs::path(p).parent_path());std::ofstream(fs::path(root)/p,std::ios::binary)<<s;};
    Files original;assert(read(root,{"world.studio","world.physics","ai/person.vwmemory"},original,e));Files first={{"world.studio","scene A"},{"world.physics","body A"},{"ai/person.vwmemory","memory A"}};
    assert(commit(root,"world.vwproject",first,original,e));Files loaded;assert(load(root,"world.vwproject",loaded,e)&&loaded.size()==3&&loaded[2].bytes=="memory A");assert(!pending(root,"world.vwproject"));
    Files before;assert(read(root,{"world.studio","world.physics","ai/person.vwmemory"},before,e));auto second=first;second[0].bytes="scene B";second[1].bytes="body B";second[2].bytes="memory B";
    assert(stage(root,"world.vwproject",second,before,e));assert(pending(root,"world.vwproject"));assert(!stage(root,"world.vwproject",second,before,e));
    write("world.studio","scene B"); // crash after one of three publications
    assert(recover(root,"world.vwproject",false,e));Files restored;assert(read(root,{"world.studio","world.physics","ai/person.vwmemory"},restored,e)&&restored[0].bytes=="scene A"&&restored[1].bytes=="body A");
    assert(stage(root,"world.vwproject",second,before,e));write("world.physics","body B");assert(recover(root,"world.vwproject",true,e));assert(load(root,"world.vwproject.previous",loaded,e)&&loaded[0].bytes=="scene A");
    // External edits cannot be overwritten by stale background snapshots.
    assert(!commit(root,"world.vwproject",first,before,e));assert(!pending(root,"world.vwproject"));
    Files current;assert(read(root,{"world.studio","world.physics","ai/person.vwmemory"},current,e));assert(stage(root,"world.vwproject",first,current,e));write("ai/person.vwmemory","external");assert(!recover(root,"world.vwproject",true,e));assert(pending(root,"world.vwproject"));write("ai/person.vwmemory","memory B");assert(recover(root,"world.vwproject",true,e));
    Files invalid={{"../outside","bad"}};assert(!commit(root,"world.vwproject",invalid,invalid,e));invalid={{"world.vwproject","bad"}};assert(!commit(root,"world.vwproject",invalid,invalid,e));invalid={{"a","1"},{"a","2"}};assert(!validate(invalid,e));invalid={{"a",std::string(4*1024*1024+1,'x')}};assert(!validate(invalid,e));
    fs::create_symlink("/tmp",fs::path(root)/"escape");assert(!read(root,{"escape/evil"},loaded,e));
    // Rollback removes assets that did not exist at transaction start.
    Files absent;assert(read(root,{"new.asset"},absent,e));Files added={{"new.asset","new"}};assert(stage(root,"other.vwproject",added,absent,e));write("new.asset","new");assert(recover(root,"other.vwproject",false,e));assert(!fs::exists(fs::path(root)/"new.asset"));
    write("broken.vwproject","VALWOOD_PROJECT 1 1\n\"x\" 1 4 0\nfail");const auto retained=loaded;assert(!load(root,"broken.vwproject",loaded,e)&&loaded.size()==retained.size());
    fs::remove_all(root);std::cout<<"PASS project checkpoints: grouped save, previous generation, partial-write replay/rollback, conflict guard, new-file rollback, bounded codec, symlink/traversal protection\n";
}
