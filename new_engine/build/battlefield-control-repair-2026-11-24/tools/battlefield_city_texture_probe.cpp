#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "engine/world_model_cpu.h"
#include "engine/world_scenery_interior_residency.h"
#include <iostream>
int main(int argc,char**argv){for(int i=1;i<argc;++i){std::string e;uint64_t previous=0;for(unsigned edge:{512u,2048u}){world_model_cpu::Options o;o.cpu_bytes=768ull*1024*1024;o.max_texture_edge=edge;o.bc7=true;o.reuse_decoder_scratch=true;auto a=world_model_cpu::prepare(argv[i],o,e);if(!a){std::cerr<<argv[i]<<": "<<e<<'\n';return 1;}std::cout<<argv[i]<<" edge="<<edge<<" textures="<<a->texture_bytes<<" geometry="<<a->geometry_bytes<<" cpu="<<a->cpu_bytes<<'\n';if(edge==512)previous=a->texture_bytes;else if(a->texture_bytes<=previous)return 2;}}}
