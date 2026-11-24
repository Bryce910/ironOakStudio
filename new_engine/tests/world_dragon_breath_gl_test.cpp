#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_dragon_breath.h"
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
constexpr unsigned side=192;
using Pixels=std::vector<float>;
void clear(GLuint fbo,float depth=1,float confidence=1) {
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);const GLenum draw[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,draw);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_RASTERIZER_DISCARD);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    const float color[]={.12f,.14f,.16f,confidence},indirect[]={.19f,.21f,.24f,.37f};
    glClearBufferfv(GL_COLOR,0,color);glClearBufferfv(GL_COLOR,1,indirect);glClearBufferfv(GL_DEPTH,0,&depth);
}
Pixels read(GLuint fbo,GLenum attachment) {
    Pixels out(side*side*4);glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glReadBuffer(attachment);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,out.data());return out;
}
Pixels read_depth(GLuint fbo) {
    Pixels out(side*side);glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glReadPixels(0,0,side,side,GL_DEPTH_COMPONENT,GL_FLOAT,out.data());return out;
}
std::size_t changed(const Pixels &a,const Pixels &b) {
    std::size_t count=0;for(std::size_t i=0;i<a.size();i+=4)if(std::memcmp(a.data()+i,b.data()+i,4*sizeof(float)))count++;return count;
}
void capture(const char *path,const Pixels &pixels) {
    if(!path)return;
    std::ofstream output(path,std::ios::binary);assert(output);output<<"P6\n"<<side<<' '<<side<<"\n255\n";
    for(int y=int(side)-1;y>=0;y--)for(unsigned x=0;x<side;x++){
        unsigned char rgb[3];for(unsigned k=0;k<3;k++){float v=std::max(0.f,pixels[(std::size_t(y)*side+x)*4+k]);rgb[k]=static_cast<unsigned char>(std::clamp(std::pow(v/(1+v),1/2.2f)*255.f,0.f,255.f));}output.write(reinterpret_cast<const char*>(rgb),3);
    }
}
struct Snapshot {
    GLint fbo=0,read=0,viewport[4]={},program=0,vao=0,buffer=0,active=0,binding[3]={},polygon[2]={},draw[2]={};
    GLint blend[6]={};GLboolean mask[5]={},enabled[7]={};
    static constexpr GLenum caps[]={GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_FRAMEBUFFER_SRGB,GL_RASTERIZER_DISCARD};
    Snapshot() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_VIEWPORT,viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding[0]);
        glActiveTexture(GL_TEXTURE1);glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&binding[1]);glActiveTexture(GL_TEXTURE2);glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding[2]);glActiveTexture(GLenum(active));
        glGetIntegerv(GL_POLYGON_MODE,polygon);glGetIntegerv(GL_DRAW_BUFFER0,&draw[0]);glGetIntegerv(GL_DRAW_BUFFER1,&draw[1]);
        const GLenum names[]={GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA};
        for(unsigned i=0;i<6;i++)glGetIntegerv(names[i],&blend[i]);
        glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask[4]);
        for(unsigned i=0;i<7;i++)enabled[i]=glIsEnabled(caps[i]);
    }
    bool operator==(const Snapshot &s)const {
        return fbo==s.fbo&&read==s.read&&program==s.program&&vao==s.vao&&buffer==s.buffer&&active==s.active&&
            !std::memcmp(viewport,s.viewport,sizeof(viewport))&&!std::memcmp(binding,s.binding,sizeof(binding))&&
            !std::memcmp(polygon,s.polygon,sizeof(polygon))&&!std::memcmp(draw,s.draw,sizeof(draw))&&
            !std::memcmp(blend,s.blend,sizeof(blend))&&!std::memcmp(mask,s.mask,sizeof(mask))&&!std::memcmp(enabled,s.enabled,sizeof(enabled));
    }
};
float depth_for(float metres,const WorldDragonBreathFrame &f){return f.far_plane/(f.far_plane-f.near_plane)*(1.f-f.near_plane/metres);}
}
int main() {
    // Default to an isolated software fixture. The explicit opt-in mirrors
    // other GL regressions and leaves the caller's native driver intact.
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL")) {
        setenv("SDL_VIDEODRIVER","offscreen",1);setenv("LIBGL_ALWAYS_SOFTWARE","1",1);
    }
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Dragon breath isolated GL regression",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Renderer: %s\n",glGetString(GL_RENDERER));
    GLuint fbo=0,textures[3]={};glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(3,textures);
    for(unsigned i=0;i<3;i++) {
        glBindTexture(GL_TEXTURE_2D,textures[i]);glTexImage2D(GL_TEXTURE_2D,0,i==2?GL_DEPTH_COMPONENT32F:GL_RGBA32F,side,side,0,i==2?GL_DEPTH_COMPONENT:GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,i==2?GL_DEPTH_ATTACHMENT:GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,textures[i],0);
    }
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    auto *effect=world_dragon_breath_create();assert(effect);
    const bool purchased=std::getenv("VALWOOD_TEST_PURCHASED_PARTICLES")!=nullptr;
    if(purchased) {
        assert(world_dragon_breath_load_fire(effect,"projects/valwood_world/particles/purchased_fire.vfx"));
        assert(world_dragon_breath_load_smoke(effect,"projects/valwood_world/particles/purchased_smoke.vfx"));
        const auto loaded=world_dragon_breath_stats(effect);assert(loaded.purchased_fire&&loaded.purchased_smoke&&loaded.sprite_bytes==12583008&&loaded.smoke_texture_bytes==87380);
        assert(!world_dragon_breath_load_fire(effect,"missing.vfx"));assert(!world_dragon_breath_load_smoke(effect,"missing.vfx"));
        assert(world_dragon_breath_stats(effect).sprite_bytes==loaded.sprite_bytes);
    }
    WorldDragonBreathFrame frame;frame.color=textures[0];frame.depth=textures[2];frame.width=frame.height=side;
    frame.camera[1]=2;frame.near_plane=.4f;frame.far_plane=1000;frame.time=3.125;
    const float n=frame.near_plane,f=frame.far_plane,t=1/frame.tan_half_fov;
    frame.vp[0]=frame.vp[5]=t;frame.vp[10]=-(f+n)/(f-n);frame.vp[11]=-1;frame.vp[14]=-2*f*n/(f-n);
    WorldDragonBreathEmitter emitter;emitter.position[0]=-4;emitter.position[1]=2;emitter.position[2]=-8;
    emitter.direction[0]=.8f;emitter.direction[2]=-.6f;emitter.strength=1;emitter.length=15;emitter.seed=123;
    assert(world_dragon_breath_set_emitters(effect,&emitter,1));
    clear(fbo);const auto background=read(fbo,GL_COLOR_ATTACHMENT0),indirect=read(fbo,GL_COLOR_ATTACHMENT1),original_depth=read_depth(fbo);
    // Deliberately hostile entry state must be restored byte-for-byte.
    GLuint sentinel_vao=0,sentinel_buffer=0,sentinel_array=0;glGenVertexArrays(1,&sentinel_vao);glGenBuffers(1,&sentinel_buffer);glGenTextures(1,&sentinel_array);
    glBindVertexArray(sentinel_vao);glBindBuffer(GL_ARRAY_BUFFER,sentinel_buffer);glViewport(3,4,101,99);
    glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);glDepthMask(GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    for(GLenum cap:Snapshot::caps)glEnable(cap);
    glBlendFuncSeparate(GL_DST_COLOR,GL_SRC_ALPHA,GL_DST_ALPHA,GL_SRC_ALPHA);glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_MAX);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[1]);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D_ARRAY,sentinel_array);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,textures[0]);glActiveTexture(GL_TEXTURE7);
    const Snapshot before;assert(world_dragon_breath_draw(effect,frame));const Snapshot after;assert(before==after);
    const auto warm=read(fbo,GL_COLOR_ATTACHMENT0);assert(changed(warm,background)>100);
    float warm_peak=0;double red=0,blue=0;
    for(std::size_t i=0;i<warm.size();i+=4){for(unsigned k=0;k<4;k++)assert(std::isfinite(warm[i+k]));warm_peak=std::max(warm_peak,warm[i]);red+=std::max(0.f,warm[i]-background[i]);blue+=std::max(0.f,warm[i+2]-background[i+2]);}
    assert(warm_peak>1&&red>blue*1.3&&read(fbo,GL_COLOR_ATTACHMENT1)==indirect&&read_depth(fbo)==original_depth);
    capture(std::getenv("VALWOOD_DRAGON_BREATH_WARM_CAPTURE"),warm);
    // Loading-only preparation uses the exact shader/target/sprite layout,
    // preserves both hostile caller state and the prior authored emitter.
    const auto live_stats=world_dragon_breath_stats(effect);const Snapshot before_warm;
    assert(world_dragon_breath_prepare(effect,frame,true));const Snapshot after_warm;assert(before_warm==after_warm);
    const auto prepared_stats=world_dragon_breath_stats(effect);
    assert(prepared_stats.draws==live_stats.draws&&prepared_stats.instance_uploads==live_stats.instance_uploads&&
        prepared_stats.emitters==live_stats.emitters&&prepared_stats.warmups==1&&prepared_stats.warmup_draws==2&&prepared_stats.warmup_failures==0);
    WorldDragonBreathLight saved_light[2];assert(world_dragon_breath_lights(effect,frame.time,saved_light,2)==1);
    assert(saved_light[0].position[0]==emitter.position[0]+emitter.direction[0]*2.5f);
    const auto live_image=read(fbo,GL_COLOR_ATTACHMENT0);const Snapshot before_reject;
    assert(!world_dragon_breath_prepare(effect,frame,false));const Snapshot after_reject;assert(before_reject==after_reject&&read(fbo,GL_COLOR_ATTACHMENT0)==live_image);
    clear(fbo);assert(world_dragon_breath_draw(effect,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==warm);
    frame.time+=64;clear(fbo);assert(world_dragon_breath_draw(effect,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==warm);frame.time-=64;
    emitter.position[0]+=100000;emitter.position[1]-=5000;emitter.position[2]+=80000;
    frame.camera[0]+=100000;frame.camera[1]-=5000;frame.camera[2]+=80000;
    assert(world_dragon_breath_set_emitters(effect,&emitter,1));clear(fbo);assert(world_dragon_breath_draw(effect,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==warm);
    frame.time+=.125;clear(fbo);assert(world_dragon_breath_draw(effect,frame));assert(changed(read(fbo,GL_COLOR_ATTACHMENT0),warm)>100);frame.time-=.125;
    emitter.variant=WorldDragonBreathVariant::Frost;assert(world_dragon_breath_set_emitters(effect,&emitter,1));clear(fbo);assert(world_dragon_breath_draw(effect,frame));
    const auto cold=read(fbo,GL_COLOR_ATTACHMENT0);red=blue=0;
    for(std::size_t i=0;i<cold.size();i+=4){red+=std::max(0.f,cold[i]-background[i]);blue+=std::max(0.f,cold[i+2]-background[i+2]);}
    assert(blue>red*1.3&&changed(cold,warm)>100);
    clear(fbo,0);assert(world_dragon_breath_draw(effect,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==background&&read_depth(fbo)==Pixels(side*side,0)&&read(fbo,GL_COLOR_ATTACHMENT1)==indirect);
    // Moving the opaque plane through the jet softly truncates it without
    // touching depth. The renderer draws against sampled depth, never itself.
    clear(fbo,depth_for(12,frame));const auto partial_depth=read_depth(fbo);assert(world_dragon_breath_draw(effect,frame));
    const auto partial=read(fbo,GL_COLOR_ATTACHMENT0);assert(changed(partial,background)>10&&changed(partial,background)<changed(cold,background));assert(read_depth(fbo)==partial_depth);
    // Weak VFX over opaque, tagged rigid, and foliage backgrounds must never
    // inherit the .4–.99 static-coverage confidence band used by TAA. This
    // specifically catches the former ZERO/ONE_MINUS_SRC_ALPHA alpha blend:
    // a barely visible source previously left confidence near.98 or.64.
    std::size_t reactive_pixels=0,reactive_smoke=0;
    auto weak=emitter;weak.variant=WorldDragonBreathVariant::Fire;
    for(float strength:{.06f,.4f})for(float tag:{1.f,.995f,.65f}) {
        weak.strength=strength;assert(world_dragon_breath_set_emitters(effect,&weak,1));
        clear(fbo,depth_for(40,frame),tag);
        // A bright neutral receiver exposes the dark smoke-only fringe, so
        // the test covers both alpha smoke and additive flame/embers draws.
        const float receiver[]={.8f,.8f,.8f,tag};glClearBufferfv(GL_COLOR,0,receiver);
        const auto receiver_image=read(fbo,GL_COLOR_ATTACHMENT0),receiver_depth=read_depth(fbo);
        const Snapshot before_weak;assert(world_dragon_breath_draw(effect,frame));const Snapshot after_weak;assert(before_weak==after_weak);
        const auto result=read(fbo,GL_COLOR_ATTACHMENT0);std::size_t changed_rgb=0,smoke_only=0;
        for(std::size_t i=0;i<result.size();i+=4) {
            if(!std::memcmp(result.data()+i,receiver_image.data()+i,3*sizeof(float))) {
                assert(result[i+3]==receiver_image[i+3]);continue;
            }
            assert(result[i+3]==0.f); // Fully reactive, rather than coverage-tagged.
            assert(!(result[i+3]>=.4f&&result[i+3]<.99f));
            changed_rgb++;if(result[i]<receiver_image[i]-.00001f)smoke_only++;
        }
        assert(changed_rgb>10);reactive_pixels+=changed_rgb;
        if(strength>.1f){assert(smoke_only>4);reactive_smoke+=smoke_only;}
        assert(read_depth(fbo)==receiver_depth&&read(fbo,GL_COLOR_ATTACHMENT1)==indirect);
    }
    assert(world_dragon_breath_set_emitters(effect,&emitter,1));
    emitter.direction[0]=0;emitter.direction[2]=-1;assert(world_dragon_breath_set_emitters(effect,&emitter,1));clear(fbo);assert(world_dragon_breath_draw(effect,frame));
    for(float v:read(fbo,GL_COLOR_ATTACHMENT0))assert(std::isfinite(v));
    std::array<WorldDragonBreathEmitter,2> pair={emitter,emitter};pair[1].variant=WorldDragonBreathVariant::Fire;pair[1].position[0]+=4;
    assert(world_dragon_breath_set_emitters(effect,pair.data(),2));clear(fbo);assert(world_dragon_breath_draw(effect,frame));
    const auto stats=world_dragon_breath_stats(effect);assert(stats.visible_emitters==2&&stats.flames==96&&stats.cores==24&&stats.smoke==56&&stats.embers==48&&stats.gpu_buffer_bytes==224*64&&stats.failures==0);
    assert(read(fbo,GL_COLOR_ATTACHMENT1)==indirect&&read_depth(fbo)==original_depth);
    if(const char *path=std::getenv("VALWOOD_DRAGON_BREATH_CAPTURE"))capture(path,read(fbo,GL_COLOR_ATTACHMENT0));
    assert(world_dragon_breath_set_emitters(effect,nullptr,0));clear(fbo);assert(world_dragon_breath_draw(effect,frame));assert(read(fbo,GL_COLOR_ATTACHMENT0)==background);
    assert(glGetError()==GL_NO_ERROR);glDisable(GL_RASTERIZER_DISCARD);glUseProgram(0);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    world_dragon_breath_free(effect);glDeleteTextures(1,&sentinel_array);glDeleteBuffers(1,&sentinel_buffer);glDeleteVertexArrays(1,&sentinel_vao);glDeleteTextures(3,textures);glDeleteFramebuffers(1,&fbo);
    assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS actual dragon breath GL: %s fire/smoke, warm HDRpeak%.3f/cold palette, soft opaque depth/truncation, untouched indirect/depth, exact64s/rebase, end-on finite geometry, hostile GL state fully restored,%zu weak-plume reactive pixels/%zu smoke-only over opaque/rigid/foliage tags,224 quads/2 draws/14KiB,zero GLerrors\n",purchased?"purchased":"analytic",warm_peak,reactive_pixels,reactive_smoke);
}
