#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_bloom.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <new>
#include <sstream>
#include <string>

namespace {
GLuint shader(GLenum type,const char *path){
    if(!path)return 0;
    std::ifstream file(path,std::ios::binary);
    if(!file){std::fprintf(stderr,"[Bloom] Cannot read shader: %s\n",path);return 0;}
    std::ostringstream stream;stream<<file.rdbuf();std::string source=stream.str();
    if(source.empty()||source.size()>1024*1024)return 0;
    GLuint result=glCreateShader(type);if(!result)return 0;
    const char *p=source.c_str();glShaderSource(result,1,&p,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048]={};glGetShaderInfoLog(result,sizeof(log),nullptr,log);std::fprintf(stderr,"[Bloom] %s: %s\n",path,log);glDeleteShader(result);return 0;}
    return result;
}
struct Program {
    GLuint id=0;GLint color=-1,source_size=-1,direction=-1,exposure=-1;
    bool init(const char *vertex,const char *fragment){
        GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment);
        if(!vs||!fs){glDeleteShader(vs);glDeleteShader(fs);return false;}
        id=glCreateProgram();if(!id){glDeleteShader(vs);glDeleteShader(fs);return false;}
        glAttachShader(id,vs);glAttachShader(id,fs);glLinkProgram(id);glDeleteShader(vs);glDeleteShader(fs);
        GLint ok=0;glGetProgramiv(id,GL_LINK_STATUS,&ok);
        if(!ok){char log[2048]={};glGetProgramInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"[Bloom] link: %s\n",log);return false;}
        color=glGetUniformLocation(id,"uColor");source_size=glGetUniformLocation(id,"uSourceSize");
        direction=glGetUniformLocation(id,"uDirection");exposure=glGetUniformLocation(id,"uExposure");
        return true;
    }
};
struct Targets {
    GLuint fbo[2]={},texture[2]={};
    void release(){glDeleteFramebuffers(2,fbo);glDeleteTextures(2,texture);for(int i=0;i<2;i++)fbo[i]=texture[i]=0;}
};
bool valid_size(int w,int h){return w>0&&h>0&&w<=8192&&h<=8192&&std::uint64_t(w)*h<=4096ull*4096ull;}
void bind(GLuint texture){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);}
}
struct WorldBloom {
    Program extract,blur;GLuint vao=0;Targets targets;WorldBloomStats stats;
};

WorldBloom *world_bloom_create(const char *vertex,const char *extract,const char *blur){
    if(!SDL_GL_GetCurrentContext())return nullptr;
    WorldBloom *b=new(std::nothrow) WorldBloom;if(!b)return nullptr;
    if(!b->extract.init(vertex,extract)||!b->blur.init(vertex,blur)){world_bloom_free(b);return nullptr;}
    glGenVertexArrays(1,&b->vao);if(!b->vao){world_bloom_free(b);return nullptr;}
    return b;
}
bool world_bloom_prepare(WorldBloom *b,int w,int h){
    if(!b)return false;
    if(!valid_size(w,h)){b->stats.failures++;return false;}
    const int qw=(w+3)/4,qh=(h+3)/4;
    if(qw==b->stats.quarter_width&&qh==b->stats.quarter_height){b->stats.width=w;b->stats.height=h;return true;}
    Targets next;glGenFramebuffers(2,next.fbo);glGenTextures(2,next.texture);
    for(int i=0;i<2;i++){
        if(!next.fbo[i]||!next.texture[i]){next.release();b->stats.failures++;return false;}
        glBindFramebuffer(GL_FRAMEBUFFER,next.fbo[i]);bind(next.texture[i]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,qw,qh,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,next.texture[i],0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){next.release();b->stats.failures++;return false;}
    }
    b->targets.release();b->targets=next;
    b->stats.width=w;b->stats.height=h;b->stats.quarter_width=qw;b->stats.quarter_height=qh;
    b->stats.bytes=2ull*qw*qh*8;b->stats.allocations++;return true;
}
unsigned world_bloom_resolve(WorldBloom *b,unsigned color,int w,int h,float exposure){
    if(!b)return 0;
    if(!color||!valid_size(w,h)||!std::isfinite(exposure)||exposure<.001f||exposure>64.f||
       color==b->targets.texture[0]||color==b->targets.texture[1]){b->stats.failures++;return 0;}
    if(!world_bloom_prepare(b,w,h))return 0;
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glViewport(0,0,b->stats.quarter_width,b->stats.quarter_height);glBindVertexArray(b->vao);
    glBindFramebuffer(GL_FRAMEBUFFER,b->targets.fbo[0]);bind(color);glUseProgram(b->extract.id);
    glUniform1i(b->extract.color,0);glUniform2f(b->extract.source_size,float(w),float(h));glUniform1f(b->extract.exposure,exposure);
    glDrawArrays(GL_TRIANGLES,0,3);
    glUseProgram(b->blur.id);glUniform1i(b->blur.color,0);
    glUniform2f(b->blur.source_size,float(b->stats.quarter_width),float(b->stats.quarter_height));
    glBindFramebuffer(GL_FRAMEBUFFER,b->targets.fbo[1]);bind(b->targets.texture[0]);glUniform2f(b->blur.direction,1,0);glDrawArrays(GL_TRIANGLES,0,3);
    glBindFramebuffer(GL_FRAMEBUFFER,b->targets.fbo[0]);bind(b->targets.texture[1]);glUniform2f(b->blur.direction,0,1);glDrawArrays(GL_TRIANGLES,0,3);
    b->stats.resolves++;return b->targets.texture[0];
}
WorldBloomStats world_bloom_stats(const WorldBloom *b){return b?b->stats:WorldBloomStats{};}
void world_bloom_free(WorldBloom *b){if(!b)return;b->targets.release();glDeleteProgram(b->extract.id);glDeleteProgram(b->blur.id);glDeleteVertexArrays(1,&b->vao);delete b;}
