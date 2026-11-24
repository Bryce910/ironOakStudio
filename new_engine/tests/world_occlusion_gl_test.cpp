/* Actual half-resolution contact/ambient composition, no performance claims.
 * Software by default; VALWOOD_TEST_NATIVE_GL=1 preserves caller's driver. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_occlusion.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
namespace {
using Pixel=std::array<float,4>;
struct Input {
    GLuint color=0,depth=0,indirect=0;int w=160,h=100;
    GLuint texture(GLenum internal,GLenum format,const float *data){
        GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,internal,w,h,0,format,GL_FLOAT,data);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);return t;
    }
    void fill(bool block,bool sky=false,float ambient=.6f){
        glDeleteTextures(1,&color);glDeleteTextures(1,&depth);glDeleteTextures(1,&indirect);
        std::vector<Pixel> c(w*h,{1,.8f,.6f,.62f}),a(w*h,{ambient,ambient*.8f,ambient*.6f,0});std::vector<float>d(w*h);
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){
            float rayx=((x+.5f)/w*2-1)*float(w)/h*.5994296f,rayy=((y+.5f)/h*2-1)*.5994296f;
            float z=block&&std::abs(rayx*4.4f)<1&&std::abs(rayy*4.4f)<1?4.4f:5.f;
            d[y*w+x]=sky?1.f:(15000.f+.4f-2*.4f*15000.f/z)/(15000.f-.4f)*.5f+.5f;
            if(sky)a[y*w+x]={0,0,0,0};
        }
        color=texture(GL_RGBA16F,GL_RGBA,reinterpret_cast<float*>(c.data()));depth=texture(GL_R32F,GL_RED,d.data());indirect=texture(GL_RGBA16F,GL_RGBA,reinterpret_cast<float*>(a.data()));
    }
    ~Input(){glDeleteTextures(1,&color);glDeleteTextures(1,&depth);glDeleteTextures(1,&indirect);}
};
std::vector<Pixel> pixels(GLuint t,int w,int h){std::vector<Pixel>p(w*h);glBindTexture(GL_TEXTURE_2D,t);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,p.data());for(auto &v:p)for(float c:v)assert(std::isfinite(c));return p;}
WorldOcclusionFrame frame(const Input&i){WorldOcclusionFrame f;f.color=i.color;f.depth=i.depth;f.indirect=i.indirect;f.width=i.w;f.height=i.h;f.sun_view[0]=.70710678f;f.sun_view[1]=0;f.sun_view[2]=.70710678f;return f;}
float minimum(const std::vector<Pixel>&p){float m=10;for(auto v:p)m=std::min(m,v[0]);return m;}
}
int main(int argc,char**argv){
    std::filesystem::path root=argc>1?argv[1]:".";
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window*window=SDL_CreateWindow("Contact lighting tests",0,0,160,100,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);SDL_GLContext ctx=SDL_GL_CreateContext(window);assert(ctx);
    std::printf("Contact lighting renderer: %s\n",glGetString(GL_RENDERER));
    auto create=[&](){return world_occlusion_create((root/"shaders/world_post.vs.glsl").c_str(),(root/"shaders/world_occlusion.fs.glsl").c_str(),(root/"shaders/world_occlusion_blur.fs.glsl").c_str(),(root/"shaders/world_occlusion_compose.fs.glsl").c_str());};
    assert(!world_occlusion_create("/nonexistent",nullptr,nullptr,nullptr));
    WorldOcclusion *o=create();assert(o&&world_occlusion_stats(o).bytes==0);
    {
        Input in;in.fill(false);auto f=frame(in);f.ambient_strength=1;f.contact_strength=1;
        GLuint output=world_occlusion_resolve(o,f);assert(output&&output!=in.color&&output!=in.indirect&&output!=in.depth);
        auto p=pixels(output,in.w,in.h);assert(minimum(p)>.995f);
        for(auto v:p)assert(std::abs(v[3]-.62f)<.001);
        auto stats=world_occlusion_stats(o);assert(stats.resizes==1&&stats.resolves==1&&stats.half_width==80&&stats.half_height==50&&stats.bytes==192000);
        in.fill(true);f=frame(in);f.contact_strength=0;f.ambient_strength=1;
        output=world_occlusion_resolve(o,f);p=pixels(output,in.w,in.h);float ambient_min=minimum(p);
        assert(ambient_min<.99f&&ambient_min>.5f);assert(p[50*in.w+80][0]>.995f&&p[5*in.w+5][0]>.995f);
        auto field=pixels(world_occlusion_field(o),80,50);assert(minimum(field)<.98f);
        f.ambient_strength=f.contact_strength=0;output=world_occlusion_resolve(o,f);p=pixels(output,in.w,in.h);assert(minimum(p)>.999f);
        in.fill(true,false,0);f=frame(in);f.ambient_strength=1;f.contact_strength=0;output=world_occlusion_resolve(o,f);p=pixels(output,in.w,in.h);assert(minimum(p)>.999f); // AO cannot darken direct-only light.
        f.ambient_strength=0;f.contact_strength=1;output=world_occlusion_resolve(o,f);p=pixels(output,in.w,in.h);float contact_min=minimum(p);assert(contact_min<.95f&&contact_min>=.10f);
        assert(p[50*in.w+80][0]>.995f);
        in.fill(false,true);f=frame(in);output=world_occlusion_resolve(o,f);p=pixels(output,in.w,in.h);assert(minimum(p)>.999f);
        auto before=world_occlusion_stats(o);for(int i=0;i<4;i++)assert(world_occlusion_resolve(o,f));auto after=world_occlusion_stats(o);assert(after.resizes==before.resizes&&after.resolves==before.resolves+4);
        f.width=0;assert(world_occlusion_resolve(o,f)==in.color&&world_occlusion_stats(o).resolves==after.resolves);
        f=frame(in);f.sun_view[0]=NAN;assert(world_occlusion_resolve(o,f)==in.color);
        f=frame(in);f.color=output;assert(world_occlusion_resolve(o,f)==output); // reject own outputfeedback
        for(int input=0;input<3;input++){
            f=frame(in);GLuint owned=world_occlusion_field(o);
            if(input==0)f.color=owned;else if(input==1)f.depth=owned;else f.indirect=owned;
            auto previous=world_occlusion_stats(o);
            assert(world_occlusion_resolve(o,f)==f.color&&world_occlusion_stats(o).resolves==previous.resolves);
        }
        in.w=127;in.h=73;in.fill(true);f=frame(in);output=world_occlusion_resolve(o,f);assert(output);stats=world_occlusion_stats(o);assert(stats.resizes==2&&stats.half_width==64&&stats.half_height==37);
        GLuint saved_field=world_occlusion_field(o),saved_output=output;world_occlusion_free(o);assert(!glIsTexture(saved_field)&&!glIsTexture(saved_output));o=nullptr;
        std::printf("PASS contact lighting: planar self-occlusion rejected, ambient-only darkness %.4f, sun contact %.4f, depth-edge/sky separation, unchanged stabilityalpha, stable reuse/oddresize/feedback rejection/GL teardown\n",ambient_min,contact_min);
    }
    assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(window);SDL_Quit();
}
