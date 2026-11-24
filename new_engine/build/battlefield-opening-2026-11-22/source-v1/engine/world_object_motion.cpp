#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_object_motion.h"
#include <cstdio>
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
    if(!file){std::fprintf(stderr,"[Motion] Cannot read shader: %s\n",path);return 0;}
    std::ostringstream stream;stream<<file.rdbuf();std::string source=stream.str();
    if(source.empty()||source.size()>1024*1024)return 0;
    GLuint result=glCreateShader(type);if(!result)return 0;
    const char *p=source.c_str();glShaderSource(result,1,&p,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048]={};glGetShaderInfoLog(result,sizeof(log),nullptr,log);std::fprintf(stderr,"[Motion] %s: %s\n",path,log);glDeleteShader(result);return 0;}
    return result;
}
}
struct WorldObjectMotion {
    GLuint program=0,fbo=0,texture=0,attached_depth=0;int width=0,height=0;
    GLint saved_depth_func=GL_LESS;GLboolean saved_depth_test=GL_FALSE,saved_cull=GL_FALSE,saved_offset=GL_FALSE,saved_mask=GL_TRUE;GLfloat saved_factor=0,saved_units=0;
};

WorldObjectMotion *world_object_motion_create(const char *vertex,const char *fragment){
    if(!SDL_GL_GetCurrentContext())return nullptr;
    WorldObjectMotion *m=new(std::nothrow) WorldObjectMotion;if(!m)return nullptr;
    GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment);
    if(!vs||!fs){glDeleteShader(vs);glDeleteShader(fs);delete m;return nullptr;}
    m->program=glCreateProgram();glAttachShader(m->program,vs);glAttachShader(m->program,fs);glLinkProgram(m->program);
    glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=0;glGetProgramiv(m->program,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048]={};glGetProgramInfoLog(m->program,sizeof(log),nullptr,log);std::fprintf(stderr,"[Motion] link: %s\n",log);world_object_motion_free(m);return nullptr;}
    glGenFramebuffers(1,&m->fbo);glGenTextures(1,&m->texture);
    if(!m->fbo||!m->texture){world_object_motion_free(m);return nullptr;}
    return m;
}
unsigned world_object_motion_program(const WorldObjectMotion *m){return m?m->program:0;}
unsigned world_object_motion_texture(const WorldObjectMotion *m){return m?m->texture:0;}

bool world_object_motion_begin(WorldObjectMotion *m,int w,int h,unsigned depth){
    if(!m||w<=0||h<=0||!depth)return false;
    glBindFramebuffer(GL_FRAMEBUFFER,m->fbo);
    if(w!=m->width||h!=m->height){
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m->texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m->texture,0);
        m->width=w;m->height=h;m->attached_depth=0;
    }
    if(m->attached_depth!=depth){glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);m->attached_depth=depth;}
    const GLenum buffer=GL_COLOR_ATTACHMENT0;glDrawBuffers(1,&buffer);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glBindFramebuffer(GL_FRAMEBUFFER,0);m->attached_depth=0;return false;}
    glGetIntegerv(GL_DEPTH_FUNC,&m->saved_depth_func);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&m->saved_mask);glGetFloatv(GL_POLYGON_OFFSET_FACTOR,&m->saved_factor);glGetFloatv(GL_POLYGON_OFFSET_UNITS,&m->saved_units);
    m->saved_depth_test=glIsEnabled(GL_DEPTH_TEST);m->saved_cull=glIsEnabled(GL_CULL_FACE);m->saved_offset=glIsEnabled(GL_POLYGON_OFFSET_FILL);
    glViewport(0,0,w,h);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
    // The scene pass used a different program; allow for non-invariant depth.
    glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-1.f,-4.f);
    glUseProgram(m->program);
    return true;
}
void world_object_motion_end(WorldObjectMotion *m){
    if(!m)return;
    glDepthFunc(GLenum(m->saved_depth_func));
    if(m->saved_depth_test)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
    if(m->saved_cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
    if(m->saved_offset)glEnable(GL_POLYGON_OFFSET_FILL);else glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(m->saved_factor,m->saved_units);glDepthMask(m->saved_mask);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
void world_object_motion_free(WorldObjectMotion *m){
    if(!m)return;
    glDeleteFramebuffers(1,&m->fbo);glDeleteTextures(1,&m->texture);glDeleteProgram(m->program);delete m;
}
