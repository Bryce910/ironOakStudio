#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_models.h"
#include "engine/world_model_cpu.h"
#include <cstdio>
#include <cassert>
int main(int argc,char **argv){
 assert(argc==2);SDL_setenv("SDL_VIDEODRIVER","offscreen",1);assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto *w=SDL_CreateWindow("Camp cache test",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);auto c=SDL_GL_CreateContext(w);assert(c);
 auto *m=world_model_load(argv[1]);assert(m);const auto before=texture_cache_stats();world_model_cpu::Options o;o.bc7=true;std::string error;auto cpu=world_model_cpu::prepare(argv[1],o,error);assert(cpu);auto *s=world_model_stage(cpu,4,8);assert(s);const auto missing=world_model_staged_new_texture_bytes(s);std::printf("resident=%llu staged_missing=%zu cpu_texture=%llu meshes=%zu renderer=%s\n",(unsigned long long)before.gpu_bytes,missing,(unsigned long long)cpu->texture_bytes,cpu->meshes.size(),glGetString(GL_RENDERER));
 world_model_stage_free(s);world_model_free(m);assert(texture_cache_stats().gpu_bytes==0);texture_cache_shutdown();SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();return missing?1:0;
}
