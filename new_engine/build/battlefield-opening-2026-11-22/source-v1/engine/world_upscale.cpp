#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_upscale.h"
#include "engine/world_temporal.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include "engine/world_shader_bank.h"
#include <new>
#include <sstream>
#include <string>

namespace {
GLuint shader(GLenum type,const char *path){
    if(!path)return 0;
    const std::string banked_path=world_shader_bank_path(path);
    std::ifstream file(banked_path,std::ios::binary);
    if(!file){std::fprintf(stderr,"[Upscale] Cannot read shader: %s\n",path);return 0;}
    std::ostringstream stream;stream<<file.rdbuf();std::string source=stream.str();
    if(source.empty()||source.size()>1024*1024)return 0;
    GLuint result=glCreateShader(type);if(!result)return 0;
    const char *p=source.c_str();glShaderSource(result,1,&p,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048]={};glGetShaderInfoLog(result,sizeof(log),nullptr,log);std::fprintf(stderr,"[Upscale] %s: %s\n",path,log);glDeleteShader(result);return 0;}
    return result;
}
}

struct WorldUpscale {
    GLuint program=0,vao=0,fbo[2]={},color[2]={},sampler_linear=0,sampler_point=0;
    GLint raw=-1,stable=-1,depth=-1,history=-1,motion=-1,has_history=-1,has_motion=-1,reprojection=-1,in_size=-1,out_size=-1,jitter=-1,prev_jitter=-1;
    int width=0,height=0,front=0;bool has_output=false;
    float previous_vp[16]={},previous_jitter_uv[2]={};double previous_camera[3]={};
    unsigned long long frames=0;
};

WorldUpscale *world_upscale_create(const char *vertex,const char *fragment){
    auto *u=new(std::nothrow) WorldUpscale;if(!u)return nullptr;
    GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment);
    if(!vs||!fs){glDeleteShader(vs);glDeleteShader(fs);delete u;return nullptr;}
    u->program=glCreateProgram();glAttachShader(u->program,vs);glAttachShader(u->program,fs);glLinkProgram(u->program);glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=0;glGetProgramiv(u->program,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048]={};glGetProgramInfoLog(u->program,sizeof(log),nullptr,log);std::fprintf(stderr,"[Upscale] link: %s\n",log);world_upscale_free(u);return nullptr;}
    auto at=[&](const char *n){return glGetUniformLocation(u->program,n);};
    u->raw=at("uRaw");u->stable=at("uStable");u->depth=at("uDepth");u->history=at("uHistory");u->motion=at("uObjectMotion");
    u->has_history=at("uHasHistory");u->has_motion=at("uHasObjectMotion");u->reprojection=at("uReprojection");
    u->in_size=at("uInSize");u->out_size=at("uOutSize");u->jitter=at("uJitterUV");u->prev_jitter=at("uPrevJitterUV");
    glGenVertexArrays(1,&u->vao);
    glGenSamplers(1,&u->sampler_linear);glSamplerParameteri(u->sampler_linear,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glSamplerParameteri(u->sampler_linear,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glSamplerParameteri(u->sampler_linear,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glSamplerParameteri(u->sampler_linear,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenSamplers(1,&u->sampler_point);glSamplerParameteri(u->sampler_point,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glSamplerParameteri(u->sampler_point,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glSamplerParameteri(u->sampler_point,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glSamplerParameteri(u->sampler_point,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    std::printf("[Upscale] Temporal upscaler ready\n");
    return u;
}

static bool resize(WorldUpscale *u,int w,int h){
    if(u->width==w&&u->height==h&&u->fbo[0])return true;
    glDeleteFramebuffers(2,u->fbo);glDeleteTextures(2,u->color);u->fbo[0]=u->fbo[1]=u->color[0]=u->color[1]=0;
    glGenTextures(2,u->color);glGenFramebuffers(2,u->fbo);
    for(int i=0;i<2;++i){
        glBindTexture(GL_TEXTURE_2D,u->color[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_HALF_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER,u->fbo[i]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,u->color[i],0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){std::fprintf(stderr,"[Upscale] incomplete target\n");return false;}
    }
    glBindTexture(GL_TEXTURE_2D,0);
    u->width=w;u->height=h;u->has_output=false;return true;
}

unsigned world_upscale_resolve(WorldUpscale *u,const WorldUpscaleFrame &f){
    if(!u||!u->program||!f.raw||!f.stable||!f.depth||f.in_width<=0||f.in_height<=0||f.out_width<=0||f.out_height<=0)return 0;
    if(!resize(u,f.out_width,f.out_height))return 0;
    float reprojection[16]={};
    bool history=u->has_output&&!f.reset&&world_temporal_reprojection(f.view_projection,u->previous_vp,f.camera,u->previous_camera,reprojection);
    const int back=u->front,next=1-u->front;
    glBindFramebuffer(GL_FRAMEBUFFER,u->fbo[next]);glViewport(0,0,f.out_width,f.out_height);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDepthMask(GL_FALSE);
    glUseProgram(u->program);
    auto bind=[&](int unit,GLuint tex,GLuint sampler,GLint loc){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,tex);glBindSampler(unit,sampler);glUniform1i(loc,unit);};
    bind(0,f.raw,u->sampler_point,u->raw);bind(1,f.stable,u->sampler_linear,u->stable);bind(2,f.depth,u->sampler_point,u->depth);
    bind(3,u->color[back],u->sampler_linear,u->history);bind(4,f.object_motion,u->sampler_point,u->motion);
    glUniform1i(u->has_history,history?1:0);glUniform1i(u->has_motion,f.object_motion?1:0);
    glUniformMatrix4fv(u->reprojection,1,GL_FALSE,reprojection);
    glUniform2f(u->in_size,float(f.in_width),float(f.in_height));glUniform2f(u->out_size,float(f.out_width),float(f.out_height));
    glUniform2f(u->jitter,f.jitter_pixels[0]/f.in_width,f.jitter_pixels[1]/f.in_height);
    glUniform2f(u->prev_jitter,u->previous_jitter_uv[0],u->previous_jitter_uv[1]);
    glBindVertexArray(u->vao);glDrawArrays(GL_TRIANGLES,0,3);
    for(int unit=0;unit<5;++unit){glActiveTexture(GL_TEXTURE0+unit);glBindSampler(unit,0);}
    glActiveTexture(GL_TEXTURE0);
    std::memcpy(u->previous_vp,f.view_projection,sizeof(u->previous_vp));
    for(int k=0;k<3;++k)u->previous_camera[k]=f.camera[k];
    u->previous_jitter_uv[0]=f.jitter_pixels[0]/f.in_width;u->previous_jitter_uv[1]=f.jitter_pixels[1]/f.in_height;
    u->front=next;u->has_output=true;++u->frames;
    return u->color[next];
}
unsigned long long world_upscale_frames(const WorldUpscale *u){return u?u->frames:0;}
void world_upscale_free(WorldUpscale *u){
    if(!u)return;
    glDeleteFramebuffers(2,u->fbo);glDeleteTextures(2,u->color);glDeleteSamplers(1,&u->sampler_linear);glDeleteSamplers(1,&u->sampler_point);
    glDeleteVertexArrays(1,&u->vao);if(u->program)glDeleteProgram(u->program);delete u;
}
