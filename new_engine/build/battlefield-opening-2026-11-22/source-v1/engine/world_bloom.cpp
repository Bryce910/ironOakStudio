#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_bloom.h"
#include <cmath>
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
    GLuint id=0;GLint color=-1,source_size=-1,direction=-1,exposure=-1,radius=-1,weight=-1;
    bool init(const char *vertex,const char *fragment){
        GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment);
        if(!vs||!fs){glDeleteShader(vs);glDeleteShader(fs);return false;}
        id=glCreateProgram();if(!id){glDeleteShader(vs);glDeleteShader(fs);return false;}
        glAttachShader(id,vs);glAttachShader(id,fs);glLinkProgram(id);glDeleteShader(vs);glDeleteShader(fs);
        GLint ok=0;glGetProgramiv(id,GL_LINK_STATUS,&ok);
        if(!ok){char log[2048]={};glGetProgramInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"[Bloom] link: %s\n",log);return false;}
        color=glGetUniformLocation(id,"uColor");source_size=glGetUniformLocation(id,"uSourceSize");
        direction=glGetUniformLocation(id,"uDirection");exposure=glGetUniformLocation(id,"uExposure");
        radius=glGetUniformLocation(id,"uRadius");weight=glGetUniformLocation(id,"uWeight");
        return true;
    }
};
// Mip chain: level 0 is the quarter-resolution thresholded extract, each
// further level halves both dimensions (Jimenez 2014 / CoD:AW).
constexpr int kMaxLevels=6;
struct Targets {
    int count=0,w[kMaxLevels]={},h[kMaxLevels]={};
    GLuint fbo[kMaxLevels]={},texture[kMaxLevels]={};
    void release(){glDeleteFramebuffers(kMaxLevels,fbo);glDeleteTextures(kMaxLevels,texture);for(int i=0;i<kMaxLevels;i++){fbo[i]=texture[i]=0;w[i]=h[i]=0;}count=0;}
};
bool valid_size(int w,int h){return w>0&&h>0&&w<=8192&&h<=8192&&std::uint64_t(w)*h<=4096ull*4096ull;}
void bind(GLuint texture){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);}
std::string sibling(const char *path,const char *leaf){
    std::string s=path?path:"";const auto slash=s.find_last_of('/');
    return (slash==std::string::npos?std::string():s.substr(0,slash+1))+leaf;
}
}
struct WorldBloom {
    Program extract,down,up;GLuint vao=0;Targets targets;WorldBloomStats stats;
};

WorldBloom *world_bloom_create(const char *vertex,const char *extract,const char *blur){
    if(!SDL_GL_GetCurrentContext())return nullptr;
    WorldBloom *b=new(std::nothrow) WorldBloom;if(!b)return nullptr;
    // The chain shaders live beside the configured blur shader.
    const std::string down=sibling(blur,"world_bloom_down.fs.glsl"),up=sibling(blur,"world_bloom_up.fs.glsl");
    if(!b->extract.init(vertex,extract)||!b->down.init(vertex,down.c_str())||!b->up.init(vertex,up.c_str())){world_bloom_free(b);return nullptr;}
    glGenVertexArrays(1,&b->vao);if(!b->vao){world_bloom_free(b);return nullptr;}
    return b;
}
bool world_bloom_prepare(WorldBloom *b,int w,int h){
    if(!b)return false;
    if(!valid_size(w,h)){b->stats.failures++;return false;}
    const int qw=(w+3)/4,qh=(h+3)/4;
    if(qw==b->stats.quarter_width&&qh==b->stats.quarter_height&&b->targets.count){b->stats.width=w;b->stats.height=h;return true;}
    Targets next;std::uint64_t bytes=0;
    int lw=qw,lh=qh;
    for(int i=0;i<kMaxLevels;i++){
        if(i>0&&(lw<4||lh<4))break;
        next.w[i]=lw;next.h[i]=lh;
        glGenFramebuffers(1,&next.fbo[i]);glGenTextures(1,&next.texture[i]);
        if(!next.fbo[i]||!next.texture[i]){next.release();b->stats.failures++;return false;}
        glBindFramebuffer(GL_FRAMEBUFFER,next.fbo[i]);bind(next.texture[i]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,lw,lh,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,next.texture[i],0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){next.release();b->stats.failures++;return false;}
        bytes+=std::uint64_t(lw)*lh*8;next.count=i+1;
        lw=(lw+1)/2;lh=(lh+1)/2;
    }
    b->targets.release();b->targets=next;
    b->stats.width=w;b->stats.height=h;b->stats.quarter_width=qw;b->stats.quarter_height=qh;
    b->stats.bytes=bytes;b->stats.allocations++;return true;
}
unsigned world_bloom_resolve(WorldBloom *b,unsigned color,int w,int h,float exposure){
    if(!b)return 0;
    if(!color||!valid_size(w,h)||!std::isfinite(exposure)||exposure<.001f||exposure>64.f){b->stats.failures++;return 0;}
    for(int i=0;i<b->targets.count;i++)if(color==b->targets.texture[i]){b->stats.failures++;return 0;}
    if(!world_bloom_prepare(b,w,h))return 0;
    Targets &t=b->targets;
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glBindVertexArray(b->vao);
    // 1. Exposure-aware soft-threshold extract to quarter resolution.
    glViewport(0,0,t.w[0],t.h[0]);
    glBindFramebuffer(GL_FRAMEBUFFER,t.fbo[0]);bind(color);glUseProgram(b->extract.id);
    glUniform1i(b->extract.color,0);glUniform2f(b->extract.source_size,float(w),float(h));glUniform1f(b->extract.exposure,exposure);
    glDrawArrays(GL_TRIANGLES,0,3);
    // 2. Downsample chain.
    glUseProgram(b->down.id);glUniform1i(b->down.color,0);
    for(int i=1;i<t.count;i++){
        glViewport(0,0,t.w[i],t.h[i]);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo[i]);bind(t.texture[i-1]);
        glUniform2f(b->down.source_size,float(t.w[i-1]),float(t.h[i-1]));glDrawArrays(GL_TRIANGLES,0,3);
    }
    // 3. Tent upsample, accumulating each smaller level onto the next larger one.
    glUseProgram(b->up.id);glUniform1i(b->up.color,0);glUniform1f(b->up.radius,1.f);glUniform1f(b->up.weight,1.f);
    GLint blend_state[6]={};
    glGetIntegerv(GL_BLEND_SRC_RGB,&blend_state[0]);glGetIntegerv(GL_BLEND_DST_RGB,&blend_state[1]);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&blend_state[2]);glGetIntegerv(GL_BLEND_DST_ALPHA,&blend_state[3]);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&blend_state[4]);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&blend_state[5]);
    glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_ONE,GL_ONE);
    for(int i=t.count-2;i>=0;i--){
        glViewport(0,0,t.w[i],t.h[i]);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo[i]);bind(t.texture[i+1]);
        glUniform2f(b->up.source_size,float(t.w[i+1]),float(t.h[i+1]));glDrawArrays(GL_TRIANGLES,0,3);
    }
    glDisable(GL_BLEND);
    glBlendFuncSeparate(GLenum(blend_state[0]),GLenum(blend_state[1]),GLenum(blend_state[2]),GLenum(blend_state[3]));
    glBlendEquationSeparate(GLenum(blend_state[4]),GLenum(blend_state[5]));
    glViewport(0,0,t.w[0],t.h[0]);
    b->stats.resolves++;return t.texture[0];
}
WorldBloomStats world_bloom_stats(const WorldBloom *b){return b?b->stats:WorldBloomStats{};}
void world_bloom_free(WorldBloom *b){if(!b)return;b->targets.release();glDeleteProgram(b->extract.id);glDeleteProgram(b->down.id);glDeleteProgram(b->up.id);glDeleteVertexArrays(1,&b->vao);delete b;}
