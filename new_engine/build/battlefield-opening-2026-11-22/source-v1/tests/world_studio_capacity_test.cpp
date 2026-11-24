#include "engine/world_studio.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unistd.h>
static unsigned checks=0;
static void check(bool value,const char *label){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::abort();}}
int main(){using namespace world_studio;
    std::string error,text;Document legacy;legacy.placements.push_back({"fixture"});legacy.groups.push_back({"Mixed",{16383,WORLD_ADDED_KEY_BASE}});
    check(encode(legacy,text,error),"legacy group encodes");check(text.rfind("VALWOOD_STUDIO 6\n",0)==0&&text.find("group \"Mixed\" 2 16383 16384\n")!=std::string::npos,"legacy canonical bytes retain old group namespace");
    char path[]="/tmp/valwood-studio-capacity-XXXXXX";int fd=mkstemp(path);check(fd>=0,"temporary file");close(fd);
    {std::ofstream out(path);out<<text;}Document loaded;check(load(path,loaded,error)&&loaded.groups[0].members==legacy.groups[0].members,"old placement16384 migrates once into65536");
    std::string again;check(encode(loaded,again,error)&&again==text,"legacy save/reopen canonical bytes unchanged");
    Document expanded=legacy;expanded.overrides.push_back({65535});expanded.groups[0].members.insert(expanded.groups[0].members.begin(),16384);
    world_entity::Id id=0;check(world_entity::add(expanded.entities,0,"High source",{world_entity::Kind::SourceModel,65535},{},id,error),"highest source binding accepted");
    check(encode(expanded,text,error)&&text.rfind("VALWOOD_STUDIO 15\n",0)==0,"expanded references selectv15");
    check(text.find("group \"Mixed\" 3 16384 16383 65536\n")!=std::string::npos,"source16384 and added65536 remain distinct");
    check(save(path,expanded,error)&&load(path,loaded,error)&&loaded.groups[0].members==expanded.groups[0].members&&loaded.overrides[0].index==65535,"expanded save/reopen preserves references");
    check(encode(loaded,again,error)&&again==text,"v15 canonical roundtrip");
    expanded.overrides[0].index=65536;check(!save(path,expanded,error),"override65536 cannot collide with placements");expanded.overrides[0].index=65535;
    expanded.entities.nodes[0].binding.index=65536;check(!save(path,expanded,error),"entity65536 source binding rejected");
    check(load(path,loaded,error)&&loaded.overrides[0].index==65535,"failed saves preserve previous file");
    // A legacy-format file cannot smuggle an expanded override. Failed loads
    // preserve the previously loaded document rather than reinterpreting IDs.
    {std::ofstream out(path);out<<"VALWOOD_STUDIO 1\nlook 0.2 1 1 1 17 0 0 0 0 0\noverride 16384 0 0 0 1 0\n";}
    check(!load(path,loaded,error)&&loaded.overrides[0].index==65535,"legacy out-of-range override remains rejected atomically");
    {std::ofstream out(path);out<<"VALWOOD_STUDIO 2\nlook 0.2 1 1 1 17 0 0 0 0 0\ngroup \"Invalid\" 1 65536\n";}
    check(!load(path,loaded,error),"legacy malformed high group cannot become new source reference");
    unlink(path);std::printf("PASS studio capacity: %u checks; legacy canonical bytes, migration, expanded source IDs and atomic persistence\n",checks);
}
