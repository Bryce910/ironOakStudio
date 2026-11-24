#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_torch_flames.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
namespace {
constexpr unsigned side=128;
using Pixels=std::vector<float>;
void clear(unsigned fbo,float depth=1) {
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);const GLenum draws[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,draws);
    const float color[]={.04f,.05f,.06f,1.f},indirect[]={.17f,.21f,.23f,.31f};
    glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDisable(GL_SCISSOR_TEST);
    glClearBufferfv(GL_COLOR,0,color);glClearBufferfv(GL_COLOR,1,indirect);glClearBufferfv(GL_DEPTH,0,&depth);
}
Pixels read(unsigned fbo,GLenum attachment) {
    Pixels p(side*side*4);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glReadBuffer(attachment);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,p.data());return p;
}
Pixels depths(unsigned fbo) {
    Pixels p(side*side);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glReadPixels(0,0,side,side,GL_DEPTH_COMPONENT,GL_FLOAT,p.data());return p;
}
std::size_t changed(const Pixels &a,const Pixels &b) {
    std::size_t n=0;for(std::size_t i=0;i<a.size();i+=4)if(std::memcmp(a.data()+i,b.data()+i,4*sizeof(float)))n++;return n;
}
}
int main() {
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL")){setenv("SDL_VIDEODRIVER","offscreen",1);setenv("LIBGL_ALWAYS_SOFTWARE","1",1);}
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Torch flame GL regression",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Renderer: %s\n",glGetString(GL_RENDERER));
    GLuint fbo=0,textures[3]={};glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(3,textures);
    for(unsigned i=0;i<3;i++) {
        glBindTexture(GL_TEXTURE_2D,textures[i]);glTexImage2D(GL_TEXTURE_2D,0,i==2?GL_DEPTH_COMPONENT32F:GL_RGBA32F,side,side,0,i==2?GL_DEPTH_COMPONENT:GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,i==2?GL_DEPTH_ATTACHMENT:GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,textures[i],0);
    }
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    auto *flames=world_torch_flames_create();assert(flames);
    const bool purchased=std::getenv("VALWOOD_TEST_PURCHASED_PARTICLES")!=nullptr;
    if(purchased) {
        assert(world_torch_flames_load_sprites(flames,"particles/purchased_fire.vfx"));
        assert(!world_torch_flames_load_sprites(flames,"particles/missing.vfx"));
        const auto art=world_torch_flames_stats(flames);assert(art.purchased_sprites&&art.sprite_bytes==25166016);
    }
    WorldTorchFlameFrame frame;frame.framebuffer=fbo;frame.width=frame.height=side;frame.camera[1]=1;frame.camera[2]=4;
    const float n=.4f,f=1000.f,t=1/frame.tan_half_fov;frame.vp[0]=frame.vp[5]=t;frame.vp[10]=-(f+n)/(f-n);frame.vp[11]=-1;frame.vp[14]=-2*f*n/(f-n);frame.time=3.125;
    WorldTorchFlameEmitter emitter;emitter.y=.7;emitter.height=.65f;emitter.width=.17f;emitter.seed=812;
    assert(world_torch_flames_set_emitters(flames,&emitter,1));
    clear(fbo);const auto background=read(fbo,GL_COLOR_ATTACHMENT0),indirect=read(fbo,GL_COLOR_ATTACHMENT1),depth=depths(fbo);
    assert(world_torch_flames_draw(flames,frame));const auto first=read(fbo,GL_COLOR_ATTACHMENT0);
    assert(changed(first,background)>30);float peak=0,alpha=1;
    for(std::size_t i=0;i<first.size();i+=4) {
        for(unsigned k=0;k<4;k++)assert(std::isfinite(first[i+k]));
        if(std::memcmp(first.data()+i,background.data()+i,4*sizeof(float)))assert(first[i+3]<=.300001f);
        if(first[i]>background[i]+.01f)assert(first[i]-background[i]>first[i+1]-background[i+1]&&first[i+1]-background[i+1]>first[i+2]-background[i+2]);
        peak=std::max(peak,first[i]);alpha=std::min(alpha,first[i+3]);
    }
    assert(peak>1&&alpha<.4f);assert(read(fbo,GL_COLOR_ATTACHMENT1)==indirect&&depths(fbo)==depth);
    if(const char *path=std::getenv("VALWOOD_TORCH_FLAME_CAPTURE")) {
        std::ofstream image(path,std::ios::binary);assert(image);image<<"P6\n"<<side<<" "<<side<<"\n255\n";
        for(int y=int(side)-1;y>=0;y--)for(unsigned x=0;x<side;x++) {
            unsigned char rgb[3];for(unsigned k=0;k<3;k++) {
                const float value=std::max(first[(std::size_t(y)*side+x)*4+k],0.f);
                rgb[k]=static_cast<unsigned char>(std::clamp(std::pow(value/(1+value),1/2.2f)*255.f,0.f,255.f));
            }
            image.write(reinterpret_cast<const char*>(rgb),3);
        }
    }
    GLint draw_buffer=0;glGetIntegerv(GL_DRAW_BUFFER0,&draw_buffer);assert(draw_buffer==GL_COLOR_ATTACHMENT0);
    GLboolean mask=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);assert(mask==GL_FALSE&&glIsEnabled(GL_DEPTH_TEST)&&!glIsEnabled(GL_BLEND));
    clear(fbo);assert(world_torch_flames_draw(flames,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==first);
    frame.time+=64.;clear(fbo);assert(world_torch_flames_draw(flames,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==first);frame.time-=64.;
    frame.wind_xz[0]=.8f;frame.wind_xz[1]=-.3f;
    clear(fbo);assert(world_torch_flames_draw(flames,frame));assert(changed(read(fbo,GL_COLOR_ATTACHMENT0),first)>20);
    frame.wind_xz[0]=frame.wind_xz[1]=0;
    frame.time+=.125;clear(fbo);assert(world_torch_flames_draw(flames,frame));assert(changed(read(fbo,GL_COLOR_ATTACHMENT0),first)>20);frame.time-=.125;
    emitter.seed++;assert(world_torch_flames_set_emitters(flames,&emitter,1));clear(fbo);assert(world_torch_flames_draw(flames,frame));assert(changed(read(fbo,GL_COLOR_ATTACHMENT0),first)>20);emitter.seed--;
    // Absolute-world rebasing cannot move the pattern, seed or shared flicker.
    emitter.x+=100000;emitter.y+=10000;emitter.z-=80000;frame.camera[0]+=100000;frame.camera[1]+=10000;frame.camera[2]-=80000;
    assert(world_torch_flames_set_emitters(flames,&emitter,1));clear(fbo);assert(world_torch_flames_draw(flames,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==first);
    // A nearer opaque surface rejects every flame/spark fragment and alpha.
    clear(fbo,0);const auto occluded=read(fbo,GL_COLOR_ATTACHMENT0),occluded_depth=depths(fbo);
    assert(world_torch_flames_draw(flames,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==occluded&&depths(fbo)==occluded_depth&&read(fbo,GL_COLOR_ATTACHMENT1)==indirect);
    frame.clip_below=true;frame.clip_height=float(emitter.y+emitter.height*3.3);clear(fbo);assert(world_torch_flames_draw(flames,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==background);
    // This intersects the live sheets, exercising the fragment clip rather
    // than only the CPU whole-effect rejection above (after a10km Y rebase).
    frame.clip_height=float(emitter.y+emitter.height*.35);clear(fbo);assert(world_torch_flames_draw(flames,frame));
    const auto partial=read(fbo,GL_COLOR_ATTACHMENT0);assert(changed(partial,background)>10&&changed(partial,background)<changed(first,background));
    assert(read(fbo,GL_COLOR_ATTACHMENT1)==indirect&&depths(fbo)==depth);
    frame.clip_below=false;frame.haze=.2f;clear(fbo);assert(world_torch_flames_draw(flames,frame));
    const auto fogged=read(fbo,GL_COLOR_ATTACHMENT0);float fogged_peak=0;
    for(std::size_t i=0;i<fogged.size();i+=4)fogged_peak=std::max(fogged_peak,fogged[i]);
    assert(fogged_peak<peak&&fogged_peak>.1f);frame.haze=0;
    frame.clip_below=false;std::array<WorldTorchFlameEmitter,32> maximum={};
    for(unsigned i=0;i<maximum.size();i++){maximum[i]=emitter;maximum[i].x+=(int(i%8)-4)*.03;maximum[i].seed=i;}
    assert(world_torch_flames_set_emitters(flames,maximum.data(),maximum.size()));clear(fbo);assert(world_torch_flames_draw(flames,frame));
    const auto state=world_torch_flames_stats(flames);assert(state.visible_emitters==32&&state.flame_slices==128&&state.sparks==256&&state.gpu_buffer_bytes==32*WORLD_TORCH_FLAME_CAPACITY);
    for(float a:read(fbo,GL_COLOR_ATTACHMENT0))assert(std::isfinite(a));
    assert(read(fbo,GL_COLOR_ATTACHMENT1)==indirect&&depths(fbo)==depth);
    assert(world_torch_flames_set_emitters(flames,nullptr,0));clear(fbo);assert(world_torch_flames_draw(flames,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==background);
    assert(glGetError()==GL_NO_ERROR);world_torch_flames_free(flames);glDeleteTextures(3,textures);glDeleteFramebuffers(1,&fbo);
    assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS actual torch shaders: warm HDR peak%.3f/reactive alpha%.4f, changing/coherent seeded motion/wind, exact64s repeat/origin/opaque-depth/indirect/exterior, water clip,32emitters/384effect quads/1KiB buffer, zeroGLerrors\n",peak,alpha);
}
