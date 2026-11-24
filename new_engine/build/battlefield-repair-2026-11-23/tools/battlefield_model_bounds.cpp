#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "engine/world_model_cpu.h"
#include <cstdio>
#include <string>
int main(int argc,char**argv){for(int i=1;i<argc;i++){world_model_cpu::Bounds b;world_model_cpu::Options o;std::string e;if(!world_model_cpu::bounds_only(argv[i],o,b,e)){std::fprintf(stderr,"%s: %s\n",argv[i],e.c_str());return 1;}std::printf("%s %.9g %.9g %.9g %.9g %.9g %.9g\n",argv[i],b.minimum[0],b.minimum[1],b.minimum[2],b.maximum[0],b.maximum[1],b.maximum[2]);}return 0;}
