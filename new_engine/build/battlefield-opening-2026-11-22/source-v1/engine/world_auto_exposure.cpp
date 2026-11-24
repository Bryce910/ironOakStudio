#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_auto_exposure.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "engine/world_shader_bank.h"
#include <new>
#include <sstream>
#include <string>

namespace {
constexpr int kSide=64,kLevels=7,kRing=4;
GLuint shader(GLenum type,const char *path){
    if(!path)return 0;
    const std::string banked_path=world_shader_bank_path(path);
    std::ifstream file(banked_path,std::ios::binary);
    if(!file){std::fprintf(stderr,"[Exposure] Cannot read shader: %s\n",path);return 0;}
    std::ostringstream stream;stream<<file.rdbuf();std::string source=stream.str();
    if(source.empty()||source.size()>1024*1024)return 0;
    GLuint result=glCreateShader(type);if(!result)return 0;
    const char *p=source.c_str();glShaderSource(result,1,&p,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048]={};glGetShaderInfoLog(result,sizeof(log),nullptr,log);std::fprintf(stderr,"[Exposure] %s: %s\n",path,log);glDeleteShader(result);return 0;}
    return result;
}
float env_float(const char *name,float fallback,float lo,float hi){
    const char *text=std::getenv(name);if(!text||!*text)return fallback;
    char *end=nullptr;const float v=std::strtof(text,&end);
    return (end!=text&&std::isfinite(v))?std::clamp(v,lo,hi):fallback;
}
}
struct WorldAutoExposure {
    GLuint program=0,vao=0,texture=0,meter_fbo=0,read_fbo=0,pbo[kRing]={};GLsync fence[kRing]={};
    GLint color=-1,source_size=-1;int head=0;bool has_reading=false;
    // Neutral band measured on the authored views (2026-11-20): apartment
    // -5.57..-5.11, forest camp -5.08 (-5.29 at hour 20), city street -3.3,
    // castle -2.57, rainforest camp -2.38 log2 luminance. Inside the band the
    // authored exposure is final; outside it a share of the excess is undone.
    float key_low=-4.7f /* AAA merge: was -5.6; forest camp now opens ~+0.3 EV */,key_high=-2.3f,strength=.65f,min_ev=-1.f,max_ev=1.5f,speed_up=1.1f,speed_down=2.6f;
    WorldAutoExposureStats stats;
};

WorldAutoExposure *world_auto_exposure_create(const char *vertex,const char *meter){
    if(!SDL_GL_GetCurrentContext())return nullptr;
    WorldAutoExposure *a=new(std::nothrow) WorldAutoExposure;if(!a)return nullptr;
    const char *mode=std::getenv("VALWOOD_AUTO_EXPOSURE");
    a->stats.enabled=!(mode&&!std::strcmp(mode,"0"));
    a->key_low=env_float("VALWOOD_AUTO_EXPOSURE_KEY_LOW",a->key_low,-14.f,8.f);
    a->key_high=std::max(a->key_low,env_float("VALWOOD_AUTO_EXPOSURE_KEY_HIGH",a->key_high,-14.f,8.f));
    a->strength=env_float("VALWOOD_AUTO_EXPOSURE_STRENGTH",a->strength,0.f,1.f);
    a->min_ev=env_float("VALWOOD_AUTO_EXPOSURE_MIN_EV",a->min_ev,-6.f,0.f);
    a->max_ev=env_float("VALWOOD_AUTO_EXPOSURE_MAX_EV",a->max_ev,0.f,6.f);
    if(!a->stats.enabled)return a;
    GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,meter);
    if(!vs||!fs){glDeleteShader(vs);glDeleteShader(fs);world_auto_exposure_free(a);return nullptr;}
    a->program=glCreateProgram();glAttachShader(a->program,vs);glAttachShader(a->program,fs);glLinkProgram(a->program);
    glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=0;glGetProgramiv(a->program,GL_LINK_STATUS,&ok);
    if(!ok){world_auto_exposure_free(a);return nullptr;}
    a->color=glGetUniformLocation(a->program,"uColor");a->source_size=glGetUniformLocation(a->program,"uSourceSize");
    glGenVertexArrays(1,&a->vao);
    glGenTextures(1,&a->texture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,a->texture);
    for(int level=0,side=kSide;level<kLevels;level++,side/=2)
        glTexImage2D(GL_TEXTURE_2D,level,GL_RGBA16F,side,side,0,GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,kLevels-1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1,&a->meter_fbo);glBindFramebuffer(GL_FRAMEBUFFER,a->meter_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,a->texture,0);
    bool complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glGenFramebuffers(1,&a->read_fbo);glBindFramebuffer(GL_FRAMEBUFFER,a->read_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,a->texture,kLevels-1);
    complete=complete&&glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glGenBuffers(kRing,a->pbo);
    for(int i=0;i<kRing;i++){glBindBuffer(GL_PIXEL_PACK_BUFFER,a->pbo[i]);glBufferData(GL_PIXEL_PACK_BUFFER,4*sizeof(float),nullptr,GL_STREAM_READ);}
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
    if(!complete||!a->vao){std::fprintf(stderr,"[Exposure] Metering target incomplete\n");world_auto_exposure_free(a);return nullptr;}
    std::printf("[Exposure] Auto exposure on: neutral log2 %.2f..%.2f, strength %.2f, range %+.1f..%+.1f EV\n",a->key_low,a->key_high,a->strength,a->min_ev,a->max_ev);
    return a;
}

float world_auto_exposure_update(WorldAutoExposure *a,unsigned color,int w,int h,float dt){
    if(!a||!a->stats.enabled||!color||w<=0||h<=0)return a&&a->stats.enabled?std::exp2(a->stats.adapted_ev):1.f;
    // 1. Collect the oldest completed reading, if any (never blocks).
    for(int n=0;n<kRing;n++){
        const int i=(a->head+n)%kRing;if(!a->fence[i])continue;
        const GLenum state=glClientWaitSync(a->fence[i],0,0);
        if(state!=GL_ALREADY_SIGNALED&&state!=GL_CONDITION_SATISFIED)continue;
        glDeleteSync(a->fence[i]);a->fence[i]=nullptr;
        glBindBuffer(GL_PIXEL_PACK_BUFFER,a->pbo[i]);
        if(const float *v=static_cast<const float *>(glMapBufferRange(GL_PIXEL_PACK_BUFFER,0,4*sizeof(float),GL_MAP_READ_BIT))){
            const float sum=v[0],weight=v[1];glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            if(std::isfinite(sum)&&std::isfinite(weight)&&weight>1e-4f){
                const float measured=sum/weight;auto &s=a->stats;
                s.measured_log2=measured;
                s.min_measured=s.readbacks?std::min(s.min_measured,measured):measured;
                s.max_measured=s.readbacks?std::max(s.max_measured,measured):measured;
                const float excess=std::max(a->key_low-measured,0.f)-std::max(measured-a->key_high,0.f);
                s.target_ev=std::clamp(a->strength*excess,a->min_ev,a->max_ev);
                s.readbacks++;
                if(!a->has_reading){s.adapted_ev=s.target_ev;a->has_reading=true;}
            }
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
    }
    // 2. Adapt: darkening to a bright scene is quicker than opening up in the dark.
    if(a->has_reading){
        auto &s=a->stats;const float speed=s.target_ev<s.adapted_ev?a->speed_down:a->speed_up;
        s.adapted_ev+=(s.target_ev-s.adapted_ev)*(1.f-std::exp(-std::clamp(dt,0.f,.25f)*speed));
    }
    // 3. Meter this frame into a free ring slot.
    const int slot=a->head;
    if(a->fence[slot]){a->stats.skipped++;}
    else {
        glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDepthMask(GL_FALSE);
        glBindFramebuffer(GL_FRAMEBUFFER,a->meter_fbo);glViewport(0,0,kSide,kSide);
        glUseProgram(a->program);glBindVertexArray(a->vao);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,color);
        glUniform1i(a->color,0);glUniform2f(a->source_size,float(w),float(h));
        glDrawArrays(GL_TRIANGLES,0,3);
        glBindTexture(GL_TEXTURE_2D,a->texture);glGenerateMipmap(GL_TEXTURE_2D);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,a->read_fbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,a->pbo[slot]);
        glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,nullptr);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        a->fence[slot]=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
        a->head=(a->head+1)%kRing;a->stats.meters++;
    }
    return std::exp2(a->stats.adapted_ev);
}

WorldAutoExposureStats world_auto_exposure_stats(const WorldAutoExposure *a){return a?a->stats:WorldAutoExposureStats{};}
void world_auto_exposure_free(WorldAutoExposure *a){
    if(!a)return;
    if(a->stats.enabled&&a->stats.readbacks)
        std::printf("[Exposure] meters=%llu readbacks=%llu skipped=%llu last_log2=%.3f range_log2=%.3f..%.3f adapted=%+.2f EV\n",
            (unsigned long long)a->stats.meters,(unsigned long long)a->stats.readbacks,(unsigned long long)a->stats.skipped,
            a->stats.measured_log2,a->stats.min_measured,a->stats.max_measured,a->stats.adapted_ev);
    for(int i=0;i<kRing;i++)if(a->fence[i])glDeleteSync(a->fence[i]);
    if(a->pbo[0])glDeleteBuffers(kRing,a->pbo);
    glDeleteFramebuffers(1,&a->meter_fbo);glDeleteFramebuffers(1,&a->read_fbo);glDeleteTextures(1,&a->texture);
    glDeleteVertexArrays(1,&a->vao);glDeleteProgram(a->program);delete a;
}
