#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_occlusion.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
namespace {
GLuint shader(GLenum type,const char *path){
    if(!path)return 0;
    std::ifstream file(path,std::ios::binary);if(!file)return 0;
    std::ostringstream stream;stream<<file.rdbuf();std::string source=stream.str();
    if(source.empty()||source.size()>1024*1024)return 0;
    GLuint result=glCreateShader(type);const char *p=source.c_str();glShaderSource(result,1,&p,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048]={};glGetShaderInfoLog(result,sizeof(log),nullptr,log);std::fprintf(stderr,"[Occlusion] %s: %s\n",path,log);glDeleteShader(result);return 0;}
    return result;
}
struct Program {
    GLuint id=0;GLint depth=-1,color=-1,indirect=-1,field=-1,resolution=-1,half_size=-1,near_far=-1,tan_fov=-1,jitter=-1,sun=-1,direction=-1,ambient=-1,contact=-1;
    bool init(const char *vertex,const char *fragment){
        GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment);
        if(!vs||!fs){glDeleteShader(vs);glDeleteShader(fs);return false;}
        id=glCreateProgram();glAttachShader(id,vs);glAttachShader(id,fs);glLinkProgram(id);glDeleteShader(vs);glDeleteShader(fs);
        GLint ok=0;glGetProgramiv(id,GL_LINK_STATUS,&ok);if(!ok)return false;
#define U(member,name) member=glGetUniformLocation(id,name)
        U(depth,"uDepth");U(color,"uColor");U(indirect,"uIndirect");U(field,"uField");U(resolution,"uResolution");U(half_size,"uHalfSize");
        U(near_far,"uNearFar");U(tan_fov,"uTan");U(jitter,"uJitterNDC");U(sun,"uSunView");U(direction,"uDirection");U(ambient,"uAmbientStrength");U(contact,"uContactStrength");
#undef U
        return true;
    }
    void frame(const WorldOcclusionFrame &f,int hw,int hh){
        glUseProgram(id);glUniform1i(depth,0);glUniform1i(color,1);glUniform1i(indirect,2);glUniform1i(field,3);
        glUniform2f(resolution,float(f.width),float(f.height));glUniform2f(half_size,float(hw),float(hh));
        glUniform2f(near_far,f.near_plane,f.far_plane);glUniform1f(tan_fov,f.tan_half_fov);glUniform2fv(jitter,1,f.jitter_ndc);
        glUniform3fv(sun,1,f.sun_view);glUniform1f(ambient,std::clamp(f.ambient_strength,0.f,1.f));glUniform1f(contact,std::clamp(f.contact_strength,0.f,1.f));
    }
};
void bind(unsigned unit,GLuint texture){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,texture);}
}
struct WorldOcclusion {
    Program raw,blur,compose;GLuint vao=0,fbo[3]={},texture[3]={};WorldOcclusionStats stats;
    void release(){glDeleteFramebuffers(3,fbo);glDeleteTextures(3,texture);for(int i=0;i<3;i++)fbo[i]=texture[i]=0;stats.width=stats.height=stats.half_width=stats.half_height=0;stats.bytes=0;}
    bool resize(int w,int h){
        if(stats.width==w&&stats.height==h)return true;
        release();int hw=(w+1)/2,hh=(h+1)/2;glGenFramebuffers(3,fbo);glGenTextures(3,texture);
        for(int i=0;i<3;i++){
            glBindFramebuffer(GL_FRAMEBUFFER,fbo[i]);bind(3,texture[i]);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,i==2?w:hw,i==2?h:hh,0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture[i],0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){release();return false;}
        }
        stats.width=w;stats.height=h;stats.half_width=hw;stats.half_height=hh;stats.bytes=(std::uint64_t(w)*h+2ull*hw*hh)*8;stats.resizes++;return true;
    }
};
WorldOcclusion *world_occlusion_create(const char *vertex,const char *field,const char *blur,const char *compose){
    if(!SDL_GL_GetCurrentContext())return nullptr;
    WorldOcclusion *o=new(std::nothrow) WorldOcclusion;if(!o)return nullptr;
    if(!o->raw.init(vertex,field)||!o->blur.init(vertex,blur)||!o->compose.init(vertex,compose)){world_occlusion_free(o);return nullptr;}
    glGenVertexArrays(1,&o->vao);return o;
}
unsigned world_occlusion_resolve(WorldOcclusion *o,const WorldOcclusionFrame &f){
    if(!o||!f.color||!f.depth||!f.indirect||f.width<=0||f.height<=0||f.width>8192||f.height>8192||std::uint64_t(f.width)*f.height>4096ull*4096ull||
       !std::isfinite(f.near_plane)||!std::isfinite(f.far_plane)||!std::isfinite(f.tan_half_fov)||f.near_plane<=0||f.far_plane<=f.near_plane||f.tan_half_fov<=0||f.tan_half_fov>4||
       !std::isfinite(f.ambient_strength)||!std::isfinite(f.contact_strength))return f.color;
    for(float value:f.sun_view)if(!std::isfinite(value))return f.color;
    for(float value:f.jitter_ndc)if(!std::isfinite(value)||std::abs(value)>.1f)return f.color;
    for(unsigned texture:o->texture)
        if(texture&&(f.color==texture||f.depth==texture||f.indirect==texture))return f.color;
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    if(!o->resize(f.width,f.height))return f.color;
    glBindVertexArray(o->vao);bind(0,f.depth);bind(1,f.color);bind(2,f.indirect);
    glViewport(0,0,o->stats.half_width,o->stats.half_height);
    glBindFramebuffer(GL_FRAMEBUFFER,o->fbo[0]);o->raw.frame(f,o->stats.half_width,o->stats.half_height);glDrawArrays(GL_TRIANGLES,0,3);
    glBindFramebuffer(GL_FRAMEBUFFER,o->fbo[1]);bind(3,o->texture[0]);o->blur.frame(f,o->stats.half_width,o->stats.half_height);
    glUniform2f(o->blur.direction,1,0);glDrawArrays(GL_TRIANGLES,0,3);
    glBindFramebuffer(GL_FRAMEBUFFER,o->fbo[0]);bind(3,o->texture[1]);glUniform2f(o->blur.direction,0,1);glDrawArrays(GL_TRIANGLES,0,3);
    glBindFramebuffer(GL_FRAMEBUFFER,o->fbo[2]);glViewport(0,0,f.width,f.height);bind(3,o->texture[0]);o->compose.frame(f,o->stats.half_width,o->stats.half_height);glDrawArrays(GL_TRIANGLES,0,3);
    o->stats.resolves++;return o->texture[2];
}
WorldOcclusionStats world_occlusion_stats(const WorldOcclusion *o){return o?o->stats:WorldOcclusionStats{};}
unsigned world_occlusion_field(const WorldOcclusion *o){return o?o->texture[0]:0;}
void world_occlusion_free(WorldOcclusion *o){if(!o)return;o->release();glDeleteProgram(o->raw.id);glDeleteProgram(o->blur.id);glDeleteProgram(o->compose.id);glDeleteVertexArrays(1,&o->vao);delete o;}
