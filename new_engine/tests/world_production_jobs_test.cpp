#include "engine/world_production_jobs.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
int main(){
    namespace fs=std::filesystem;using namespace world_production;
    char pattern[]="/tmp/valwood-production-job-XXXXXX";const char *tmp=mkdtemp(pattern);assert(tmp);fs::path root(tmp);fs::create_directory(root/"tools");
    std::string error;std::vector<std::string> argv;Request r;r.engine=root.string();r.project=(root/"project").string();r.output=(root/"result").string();
    assert(command(r,argv,error));assert(argv.back()=="--validate-only");
    std::ofstream(root/"tools/build_world_package.py")<<"import json,sys\nprint(json.dumps(sys.argv[1:]))\n";
    r.project+=" $(touch SHOULD_NOT_EXIST) `pwd` ' spaces";auto result=run(r);assert(result.ok&&result.output.find(r.project)!=std::string::npos);assert(!fs::exists("SHOULD_NOT_EXIST"));
    std::ofstream(root/"tools/build_world_package.py")<<"import sys\nprint('x'*200000)\nsys.exit(3)\n";
    result=run(r);assert(!result.ok&&result.exit_code==3&&result.output.size()<66000&&result.output.find("limited")!=std::string::npos);
    r.engine="relative";assert(!command(r,argv,error));r.engine=root.string();r.task=Task::AuditFrames;r.profile=(root/"frames.json").string();assert(command(r,argv,error));assert(argv[1].find("check_world_performance.py")!=std::string::npos);
    fs::remove_all(root);std::cout<<"world production worker argv/output: passed\n";
}
