#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "world_water_scene.h"
#include <new>

struct WorldWaterScene {
    GLuint fbo=0, color=0, depth=0;
    int width=0, height=0;
};

namespace {
void release(WorldWaterScene &s) {
    glDeleteFramebuffers(1,&s.fbo);
    glDeleteTextures(1,&s.color);
    glDeleteTextures(1,&s.depth);
    s={};
}
void image(GLuint &id,int w,int h,GLint format,GLenum channel) {
    glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,channel,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,channel==GL_RGBA?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,channel==GL_RGBA?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}
}

WorldWaterScene *world_water_scene_create() {return new(std::nothrow) WorldWaterScene;}
bool world_water_scene_capture(WorldWaterScene *s,unsigned source,int w,int h) {
    if(!s||!source||w<1||h<1||w>8192||h>8192||source==s->fbo)return false;
    glDisable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);
    if(w!=s->width||h!=s->height) {
        WorldWaterScene next;
        glGenFramebuffers(1,&next.fbo);glBindFramebuffer(GL_FRAMEBUFFER,next.fbo);
        image(next.color,w,h,GL_RGBA16F,GL_RGBA);
        image(next.depth,w,h,GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,next.color,0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,next.depth,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {
            release(next);glBindFramebuffer(GL_FRAMEBUFFER,source);return false;
        }
        next.width=w;next.height=h;
        release(*s);*s=next;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,source);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,s->fbo);
    glBlitFramebuffer(0,0,w,h,0,0,w,h,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,source);
    return true;
}
unsigned world_water_scene_color(const WorldWaterScene *s) {return s?s->color:0;}
unsigned world_water_scene_depth(const WorldWaterScene *s) {return s?s->depth:0;}
std::uint64_t world_water_scene_bytes(const WorldWaterScene *s) {
    return s?std::uint64_t(s->width)*s->height*12:0;
}
void world_water_scene_free(WorldWaterScene *s) {if(s){release(*s);delete s;}}
