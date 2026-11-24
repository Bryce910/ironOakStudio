// Direct and staged material ownership must agree; winter state must not leak
// into subsequent actors or procedural drawing. Uses production mesh loaders.
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include "gl_helpers.h"
#include "engine/world_model_cpu.h"
#include "engine/world_models.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
namespace {
unsigned shader=0,draws=0;
const world_model_cpu::Asset *expected=nullptr;
}
extern "C" void __real_glDrawElementsInstanced(GLenum,GLsizei,GLenum,const void*,GLsizei);
extern "C" void __wrap_glDrawElementsInstanced(GLenum mode,GLsizei count,GLenum type,const void *indices,GLsizei instances){
    assert(expected&&draws<expected->meshes.size());float value[3]={};
    glGetUniformfv(shader,glGetUniformLocation(shader,"uWinterSurface"),value);
    for(unsigned k=0;k<3;k++)assert(std::abs(value[k]-expected->meshes[draws].winter[k])<.00001f);
    draws++;__real_glDrawElementsInstanced(mode,count,type,indices,instances);
}
int main(){
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Winter material contracts",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);
    shader=GLH_program("#version 330 core\nlayout(location=0)in vec3 aPos;void main(){gl_Position=vec4(aPos.xy*.01,0,1);}",
        "#version 330 core\nuniform vec3 uWinterSurface;out vec4 color;void main(){color=vec4(uWinterSurface,1);}");
    GLint linked=0;glGetProgramiv(shader,GL_LINK_STATUS,&linked);assert(linked);
    for(const char *name:{"cliff_layer_a","frozen_waterfall","source_arch"}){
        std::string path="projects/valwood_stronghold/models/stronghold/"+std::string(name)+".glb",error;
        auto cpu=world_model_cpu::prepare(path.c_str(),{},error);assert(cpu&&error.empty());expected=cpu.get();
        if(std::string(name)=="cliff_layer_a")assert(cpu->meshes[0].winter[0]>.9f);
        if(std::string(name)=="frozen_waterfall")assert(cpu->meshes[0].winter[1]==1);
        if(std::string(name)=="source_arch")for(const auto &m:cpu->meshes)for(float v:m.winter)assert(v==0);
        auto *direct=world_model_load(path.c_str());assert(direct);
        auto *stage=world_model_stage(cpu);assert(stage);bool complete=false;
        while(!complete){std::size_t consumed=0;assert(world_model_stage_step(stage,1024*1024,&consumed,&complete));}
        auto *staged=world_model_stage_take(stage);assert(staged);world_model_stage_free(stage);
        WorldModelInstance instance={0,0,0,0,1};
        for(auto *asset:{direct,staged}){
            assert(world_model_upload_instances(asset,&instance,1));glUseProgram(shader);draws=0;world_model_draw(asset,shader);
            assert(draws==cpu->meshes.size());float reset[3]={1,1,1};glGetUniformfv(shader,glGetUniformLocation(shader,"uWinterSurface"),reset);
            for(float v:reset)assert(v==0);
            world_model_free(asset);
        }
    }
    glDeleteProgram(shader);assert(glGetError()==GL_NO_ERROR);
    const auto cache=texture_cache_stats();assert(cache.references==0);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::puts("PASS: CPU/direct/staged winter material agreement, ordinary material defaults, draw-state reset, clean GL/cache teardown");
}
