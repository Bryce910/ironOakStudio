// Native/offscreen GL regression: real worker cook, actual chunked uploads,
// shared texture lifetime and rendered equivalence. Never run concurrently with
// a frame-time route. Link --wrap=stbi_load_from_memory,--wrap=fopen,--wrap=pwrite.
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include "gl_helpers.h"
#include "engine/world_model_cpu.h"
#include "engine/world_models.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <string>
#include <unistd.h>
static bool owner_upload=false;
extern "C" unsigned char *__real_stbi_load_from_memory(const unsigned char *,int,int *,int *,int *,int);
extern "C" unsigned char *__wrap_stbi_load_from_memory(const unsigned char *p,int n,int *w,int *h,int *c,int desired){assert(!owner_upload);return __real_stbi_load_from_memory(p,n,w,h,c,desired);}
extern "C" FILE *__real_fopen(const char *,const char *);
extern "C" FILE *__wrap_fopen(const char *p,const char *mode){assert(!owner_upload);return __real_fopen(p,mode);}
extern "C" ssize_t __real_pwrite(int,const void *,size_t,off_t);
extern "C" ssize_t __wrap_pwrite(int fd,const void *p,size_t n,off_t off){assert(!owner_upload);return __real_pwrite(fd,p,n,off);}
int main(int argc,char **argv){
    const std::string root=argc>1?argv[1]:".";const auto path=root+"/projects/valwood_world/models/cinematic_deadwood.glb";
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Valwood model stream test",0,0,128,128,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Stream renderer: %s\n",glGetString(GL_RENDERER));
    unsigned anisotropy=1,facade_anisotropy=1;if(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")){float maximum=1;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&maximum);anisotropy=unsigned(std::min(4.f,maximum));facade_anisotropy=unsigned(std::min(8.f,maximum));}
    world_model_cpu::Options options;options.bc7=SDL_GL_ExtensionSupported("GL_ARB_texture_compression_bptc");
    auto cook=[&](){return std::async(std::launch::async,[&](){std::string error;auto cpu=world_model_cpu::prepare(path.c_str(),options,error);assert(cpu&&error.empty());return cpu;}).get();};
    auto *legacy=world_model_load(path.c_str());assert(legacy);auto cpu=cook();float lo[3],hi[3];assert(world_model_bounds(legacy,lo,hi));for(int i=0;i<3;i++){assert(lo[i]==cpu->minimum[i]);assert(hi[i]==cpu->maximum[i]);}
    auto upload=[&](const std::shared_ptr<const world_model_cpu::Asset> &source){owner_upload=true;auto *stage=world_model_stage(source,anisotropy,facade_anisotropy);owner_upload=false;assert(stage);bool complete=false;std::size_t total=0,frames=0;while(!complete&&frames++<4096){std::size_t consumed=0;owner_upload=true;assert(world_model_stage_step(stage,64*1024,&consumed,&complete));owner_upload=false;assert(consumed<=64*1024);total+=consumed;}assert(complete&&total<=source->upload_bytes);auto *model=world_model_stage_take(stage);assert(model);world_model_stage_free(stage);return std::make_pair(model,total);};
    auto staged=upload(cpu);assert(staged.second==cpu->geometry_bytes);auto *streamed=staged.first;
    const char *vs="#version 330 core\nlayout(location=0)in vec3 p;layout(location=1)in vec3 n;layout(location=2)in vec2 uv;layout(location=4)in vec4 instance;out vec2 tex;out vec3 norm;void main(){vec3 q=p*instance.w+instance.xyz;gl_Position=vec4(q.x*.4,q.y*1.2-.6,q.z*.2,1);tex=uv;norm=n;}";
    const char *fs="#version 330 core\nin vec2 tex;in vec3 norm;out vec4 color;uniform sampler2D uBase;uniform int uHasBase;uniform vec4 uBaseFactor;uniform float uAlphaCutoff;void main(){vec4 c=(uHasBase!=0?texture(uBase,tex):vec4(1))*uBaseFactor;if(c.a<uAlphaCutoff)discard;color=vec4(c.rgb*(.2+.8*max(0.,normalize(norm).y)),1);}";
    const GLuint program=GLH_program(vs,fs);assert(program);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glViewport(0,0,128,128);
    const WorldModelInstance instance={0,0,0,0,1};
    auto render=[&](WorldModelAsset *model){assert(world_model_upload_instances(model,&instance,1));glClearColor(.1f,.2f,.3f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);world_model_draw(model,program);std::array<unsigned char,128*128*4> pixels={};glReadPixels(0,0,128,128,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());assert(glGetError()==GL_NO_ERROR);return pixels;};
    const auto reference=render(legacy),shared=render(streamed);assert(reference==shared);world_model_free(legacy);assert(render(streamed)==reference);world_model_free(streamed);assert(texture_cache_stats().entries==0&&texture_cache_stats().references==0&&texture_cache_stats().gpu_bytes==0);
    // A fresh preparation uploads actual image mip rows, rather than relying on
    // the legacy image cache. No owner decoder/fopen/pwrite call is permitted.
    cpu=cook();staged=upload(cpu);assert(staged.second==cpu->upload_bytes);const auto fresh=render(staged.first);unsigned changed=0,max_difference=0;for(unsigned i=0;i<fresh.size();i++){const unsigned d=unsigned(std::abs(int(reference[i])-int(fresh[i])));changed+=d>2;max_difference=std::max(max_difference,d);}assert(changed<128);world_model_free(staged.first);
    // Cancellation of a partial real texture upload releases its GL objects and
    // cache references without modifying a different model's ownership.
    cpu=cook();auto *partial=world_model_stage(cpu,anisotropy,facade_anisotropy);assert(partial);std::size_t bytes=0;bool complete=false;assert(world_model_stage_step(partial,64*1024,&bytes,&complete)&&!complete);world_model_stage_free(partial);assert(texture_cache_stats().entries==0&&texture_cache_stats().references==0&&texture_cache_stats().gpu_bytes==0);
    std::printf("Stream GL: exact shared-cache pixels; fresh mip differences max %u / >2 %u; <=64 KiB owner chunks, no owner decoder/file IO, partial cancellation and full teardown pass\n",max_difference,changed);
    glDeleteProgram(program);texture_cache_shutdown();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
