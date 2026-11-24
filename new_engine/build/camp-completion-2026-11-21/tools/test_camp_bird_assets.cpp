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

#include <array>
#include <string>
int main(int argc,char **argv){
 assert(argc==2);SDL_setenv("SDL_VIDEODRIVER","offscreen",1);assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto *w=SDL_CreateWindow("Camp bird pose asset test",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);auto c=SDL_GL_CreateContext(w);assert(c);
 std::array<WorldModelAsset*,12> poses{};
 for(unsigned i=0;i<12;++i){char name[80];std::snprintf(name,sizeof name,"bird_%s_%02u.glb",i<8?"walk":"idle",i<8?i:i-8);auto path=std::string(argv[1])+"/"+name;poses[i]=world_model_load_pose_origin(path.c_str(),WORLD_MODEL_POSE_BLEND_VERSION);assert(poses[i]&&world_model_pose_mesh_count(poses[i])==5);}
 for(unsigned i=0;i<12;++i){auto *batch=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);assert(batch);assert(world_model_pose_blend_set_targets(batch,poses[i],poses[i<8?(i+1)%8:8+(i-8+1)%4]));WorldModelInstance instance{0,0,-12,0,1};float weight=.5;assert(world_model_pose_blend_upload(batch,&instance,&weight,1));world_model_pose_blend_free(batch);}
 for(auto *p:poses)world_model_free(p);assert(glGetError()==GL_NO_ERROR);assert(texture_cache_stats().gpu_bytes==0);texture_cache_shutdown();
 std::printf("PASS owned woodland-bird assets: all12actualGLBs accepted by production pose loader; all12adjacent GPUblendtargets compatible; transformuploads and fullownershipteardown; renderer=%s\\n",glGetString(GL_RENDERER));
 SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();
}
