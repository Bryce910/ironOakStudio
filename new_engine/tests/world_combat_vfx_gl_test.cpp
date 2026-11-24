#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_combat_vfx_render.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
int main(){assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto *w=SDL_CreateWindow("Combat VFX GPU check",0,0,256,128,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);const auto gl=SDL_GL_CreateContext(w);assert(gl);std::printf("Renderer: %s\n",glGetString(GL_RENDERER));
 auto *renderer=world_combat_vfx_create();assert(renderer);GLuint texture[2],reader;glGenTextures(2,texture);glGenFramebuffers(1,&reader);std::vector<float> background(256*128*4,.04f),depth(256*128,1);for(unsigned i=3;i<background.size();i+=4)background[i]=1;
 for(unsigned i=0;i<2;++i){glBindTexture(GL_TEXTURE_2D,texture[i]);glTexImage2D(GL_TEXTURE_2D,0,i?GL_R32F:GL_RGBA16F,256,128,0,i?GL_RED:GL_RGBA,GL_FLOAT,i?depth.data():background.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
 world_combat_vfx::System fx;world_combat_vfx::Input in;in.feet[0]={0,-1,-4};in.feet[1]={0,-1,-2};in.blades[0]={{-.3,0,-3},{.3,.3,-2},true};in.blades[1]=in.blades[0];WorldTrainingCombat c;c.state=TrainingState::Fighting;c.player_phase=TrainingPlayerPhase::Strike;c.hits=c.parries=1;fx.update(c,in,.016);in.blades[0].tip.x+=.4;fx.update(c,in,.016);
 WorldCombatVfxFrame f;f.color=texture[0];f.depth=texture[1];f.width=256;f.height=128;f.near_plane=.4;f.far_plane=100;f.vp[0]=.75;f.vp[5]=1.5;f.vp[10]=-100.4f/99.6f;f.vp[11]=-1;f.vp[14]=-80.f/99.6f;
 auto read=[&](){std::vector<float> pixels(background.size());glBindFramebuffer(GL_FRAMEBUFFER,reader);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture[0],0);glReadPixels(0,0,256,128,GL_RGBA,GL_FLOAT,pixels.data());return pixels;};
 assert(world_combat_vfx_draw(renderer,fx,f));auto pixels=read();unsigned changed=0,untouched=0;for(unsigned i=0;i<pixels.size();i+=4){if(pixels[i]>.041){changed++;assert(pixels[i+3]<.01);}else untouched++;for(unsigned k=0;k<4;k++)assert(std::isfinite(pixels[i+k]));}assert(changed>20&&untouched>30000);
 glBindTexture(GL_TEXTURE_2D,texture[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,128,GL_RGBA,GL_FLOAT,background.data());depth.assign(depth.size(),0);glBindTexture(GL_TEXTURE_2D,texture[1]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,128,GL_RED,GL_FLOAT,depth.data());assert(world_combat_vfx_draw(renderer,fx,f));pixels=read();for(unsigned i=0;i<pixels.size();i+=4)assert(std::abs(pixels[i]-.04f)<.0001);
 // Body damage must be visibly red without HDR/emissive spark brightness.
 // Style4 remains an ordinary depth-tested world effect, never an overlay.
 world_combat_vfx::System blood;WorldTrainingCombat body;body.state=TrainingState::Fighting;body.hits=1;
 blood.update(body,in,0);glBindTexture(GL_TEXTURE_2D,texture[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,128,GL_RGBA,GL_FLOAT,background.data());
 depth.assign(depth.size(),1);glBindTexture(GL_TEXTURE_2D,texture[1]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,128,GL_RED,GL_FLOAT,depth.data());
 assert(world_combat_vfx_draw(renderer,blood,f));pixels=read();unsigned red_pixels=0;
 for(unsigned i=0;i<pixels.size();i+=4){red_pixels+=pixels[i]>.05f&&pixels[i]>pixels[i+1]*2&&pixels[i]>pixels[i+2]*2;for(unsigned k=0;k<3;++k)assert(pixels[i+k]<1.f);}
 assert(red_pixels>8);
 glBindTexture(GL_TEXTURE_2D,texture[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,128,GL_RGBA,GL_FLOAT,background.data());
 depth.assign(depth.size(),0);glBindTexture(GL_TEXTURE_2D,texture[1]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,128,GL_RED,GL_FLOAT,depth.data());
 assert(world_combat_vfx_draw(renderer,blood,f));pixels=read();for(unsigned i=0;i<pixels.size();i+=4)assert(std::abs(pixels[i]-.04f)<.0001);
 auto bad=f;bad.depth=bad.color;assert(!world_combat_vfx_draw(renderer,fx,bad));assert(world_combat_vfx_stats(renderer).failures==1);assert(glGetError()==GL_NO_ERROR);world_combat_vfx_free(renderer);assert(glIsTexture(texture[0])&&glIsTexture(texture[1]));glDeleteFramebuffers(1,&reader);glDeleteTextures(2,texture);assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(gl);SDL_DestroyWindow(w);SDL_Quit();std::printf("PASS native combat VFX: %u spark/dust/trail pixels, %u nonemissive red blood pixels, complete blood/metal depth occlusion, feedback rejection, warmed shader and caller texture ownership\n",changed,red_pixels);
}
