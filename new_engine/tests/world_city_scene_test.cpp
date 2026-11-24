#include "engine/world_city_scene.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
int main(){
    char path[]="/tmp/valwood-city-look-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
    const std::string valid="VALWOOD_CITY_LOOK 1\nsun 1 .5 .25\nsky .1 .2 .3\nground .01 .02 .03\nfog .4 .5 .6\nzenith .03 .02 .01\nhaze .0002\n";
    auto write=[&](const std::string&s){std::ofstream out(path);out<<s;assert(out.good());};
    world_city_scene::Look look;std::string error;write(valid);assert(world_city_scene::load(path,look,error)&&error.empty());
    assert(look.sun[2]==.25f&&look.haze==.0002f);const auto prior=look;
    for(const auto&bad:{valid+"sun 1 1 1\n",valid+"unknown 1\n",valid.substr(0,valid.find("haze")),std::string("VALWOOD_CITY_LOOK 2\n"),valid+"haze nan\n",valid+"#"+std::string(600,'x')}){
        write(bad);assert(!world_city_scene::load(path,look,error)&&!error.empty());assert(look.sun==prior.sun&&look.haze==prior.haze);
    }
    unlink(path);assert(!world_city_scene::load(path,look,error));
    puts("PASS city source look: complete linear palette, bounded records, malformed/duplicate rejection and atomic failure");
}
