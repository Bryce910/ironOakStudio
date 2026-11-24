#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_combat_vfx_render.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <new>
#include <sstream>
namespace {
GLuint shader(GLenum kind,const char *path){std::ifstream file(path);if(!file){std::fprintf(stderr,"[Combat VFX] Cannot open shader: %s\n",path);return 0;}std::ostringstream stream;stream<<file.rdbuf();const auto source=stream.str();GLuint s=glCreateShader(kind);const char *p=source.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];glGetShaderInfoLog(s,sizeof log,nullptr,log);std::fprintf(stderr,"[Combat VFX] %s: %s\n",path,log);glDeleteShader(s);return 0;}return s;}
}
struct WorldCombatVfxRenderer {GLuint program=0,vao=0,buffer=0,fbo=0;GLint vp=-1,depth=-1,resolution=-1,near_far=-1;world_combat_vfx::Stage stage;WorldCombatVfxStats stats;};
static bool warm_pipeline(WorldCombatVfxRenderer *);
WorldCombatVfxRenderer *world_combat_vfx_create(){
 if(!SDL_GL_GetCurrentContext())return nullptr;
 auto *p=new(std::nothrow)WorldCombatVfxRenderer;if(!p)return nullptr;
 const auto vs=shader(GL_VERTEX_SHADER,"shaders/world_combat_vfx.vs.glsl"),fs=shader(GL_FRAGMENT_SHADER,"shaders/world_combat_vfx.fs.glsl");
 if(vs&&fs){p->program=glCreateProgram();glAttachShader(p->program,vs);glAttachShader(p->program,fs);glLinkProgram(p->program);}
 if(vs)glDeleteShader(vs);
 if(fs)glDeleteShader(fs);
 GLint ok=0;if(p->program)glGetProgramiv(p->program,GL_LINK_STATUS,&ok);if(!ok){world_combat_vfx_free(p);return nullptr;}
 p->vp=glGetUniformLocation(p->program,"uVP");p->depth=glGetUniformLocation(p->program,"uDepth");p->resolution=glGetUniformLocation(p->program,"uResolution");p->near_far=glGetUniformLocation(p->program,"uNearFar");
 glGenVertexArrays(1,&p->vao);glGenBuffers(1,&p->buffer);glGenFramebuffers(1,&p->fbo);glBindVertexArray(p->vao);glBindBuffer(GL_ARRAY_BUFFER,p->buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(p->stage.instances),nullptr,GL_STREAM_DRAW);
 static_assert(sizeof(world_combat_vfx::Instance)==96,"Combat VFX instance layout");
 for(unsigned i=0;i<6;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,4,GL_FLOAT,GL_FALSE,sizeof(world_combat_vfx::Instance),reinterpret_cast<void*>(std::uintptr_t(i*16)));glVertexAttribDivisor(i,1);}glBindVertexArray(0);
 if(!p->vao||!p->buffer||!p->fbo||glGetError()!=GL_NO_ERROR){world_combat_vfx_free(p);return nullptr;}if(!warm_pipeline(p)){world_combat_vfx_free(p);return nullptr;}return p;
}
bool world_combat_vfx_draw(WorldCombatVfxRenderer *p,const world_combat_vfx::System &system,const WorldCombatVfxFrame &f){
 if(!p)return false;
 auto fail=[&](){p->stats.failures++;return false;};
 if(!f.color||!f.depth||f.color==f.depth||f.width<1||f.height<1||f.width>8192||f.height>8192||!std::isfinite(f.near_plane)||!std::isfinite(f.far_plane)||f.near_plane<.01||f.far_plane<=f.near_plane)return fail();
 for(float v:f.vp)if(!std::isfinite(v))return fail();
 system.stage(f.camera,f.right,f.up,p->stage);if(!p->stage.count)return true;
 glBindFramebuffer(GL_FRAMEBUFFER,p->fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,f.color,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);return fail();}
 glViewport(0,0,f.width,f.height);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 glUseProgram(p->program);glBindVertexArray(p->vao);glBindBuffer(GL_ARRAY_BUFFER,p->buffer);glBufferSubData(GL_ARRAY_BUFFER,0,p->stage.count*sizeof(world_combat_vfx::Instance),p->stage.instances.data());
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,f.depth);glUniform1i(p->depth,0);glUniformMatrix4fv(p->vp,1,GL_FALSE,f.vp);glUniform2f(p->resolution,float(f.width),float(f.height));glUniform2f(p->near_far,f.near_plane,f.far_plane);
 glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ZERO);glDrawArraysInstanced(GL_TRIANGLES,0,6,p->stage.count);glDisable(GL_BLEND);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);
 p->stats.draws++;p->stats.uploads++;p->stats.peak_particles=std::max(p->stats.peak_particles,p->stage.particles);p->stats.peak_trails=std::max(p->stats.peak_trails,p->stage.trails);return true;
}
static bool warm_pipeline(WorldCombatVfxRenderer *p){
 constexpr int size=64;std::array<float,size*size*4> empty={};std::array<float,size*size> far={};far.fill(1);GLuint textures[2]={};glGenTextures(2,textures);
 for(unsigned i=0;i<2;++i){glBindTexture(GL_TEXTURE_2D,textures[i]);glTexImage2D(GL_TEXTURE_2D,0,i?GL_R32F:GL_RGBA16F,size,size,0,i?GL_RED:GL_RGBA,GL_FLOAT,i?far.data():empty.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
 world_combat_vfx::System system;world_combat_vfx::Input input;input.feet[0]={0,-1,-3};input.feet[1]={0,-1,-2};input.blades[0]={{-.4,0,-3},{.4,.3,-2},true};input.blades[1]=input.blades[0];WorldTrainingCombat c;c.state=TrainingState::Fighting;c.player_phase=TrainingPlayerPhase::Strike;c.hits=c.parries=c.dodges=1;system.update(c,input,.01);input.first_person=true;input.blades[0].tip.x+=.2;system.update(c,input,.01);
 WorldCombatVfxFrame f;f.color=textures[0];f.depth=textures[1];f.width=f.height=size;f.near_plane=.4f;f.far_plane=100;f.vp[0]=f.vp[5]=1.5f;f.vp[10]=-(100.4f/99.6f);f.vp[11]=-1;f.vp[14]=-80.f/99.6f;
 const bool ready=world_combat_vfx_draw(p,system,f);glFinish();glDeleteTextures(2,textures);glBindFramebuffer(GL_FRAMEBUFFER,0);p->stats={};return ready&&glGetError()==GL_NO_ERROR;
}
WorldCombatVfxStats world_combat_vfx_stats(const WorldCombatVfxRenderer *p){return p?p->stats:WorldCombatVfxStats{};}
void world_combat_vfx_free(WorldCombatVfxRenderer *p){if(!p)return;glUseProgram(0);glBindVertexArray(0);glDeleteFramebuffers(1,&p->fbo);glDeleteBuffers(1,&p->buffer);glDeleteVertexArrays(1,&p->vao);glDeleteProgram(p->program);delete p;}
