#include "engine/world_studio.h"
#include <cstdio>
#include <string>
int main(int argc,char **argv){
    if(argc!=2)return 2;
    world_studio::Document document;std::string error,encoded;
    if(!world_studio::load(argv[1],document,error)||!world_studio::encode(document,encoded,error)){
        std::fprintf(stderr,"Studio document invalid: %s\n",error.c_str());return 1;
    }
    std::printf("{\"valid\":true,\"placements\":%zu,\"assets\":%zu,\"terrain_tiles\":%zu,\"groups\":%zu,\"emitters\":%zu,\"lights\":%zu}\n",
        document.placements.size(),document.assets.size(),document.terrain.size(),document.groups.size(),document.particles.size(),document.lights.size());
    return 0;
}
