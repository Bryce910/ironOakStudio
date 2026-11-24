/* Actual weather module/shaders; software GL by default. No frame-time claim.
 * Controlled terrain callback/stub avoids coupling renderer tests to a map. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_weather.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <vector>
namespace {
// This module owns no worker threads. Check every C++ allocation on its calling
// render thread; an unrelated Mesa compiler worker must not race this counter.
thread_local bool track=false;
thread_local std::size_t allocations=0;
std::size_t queries=0;
using Pixel=std::array<float,4>;
float height(double,double,void *value){queries++;return *static_cast<float *>(value);}
struct Input {
    GLuint color=0,depth=0,macro=0,shadow=0;int w=192,h=120;
    GLuint texture(GLenum internal,GLenum format,int width,int height,const float *data){
        GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
        glTexImage2D(GL_TEXTURE_2D,0,internal,width,height,0,format,GL_FLOAT,data);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);return t;
    }
    void fill(int mode){ // 0ground,1sky,2nearopaque,3wall80m
        glDeleteTextures(1,&color);glDeleteTextures(1,&depth);glDeleteTextures(1,&macro);glDeleteTextures(1,&shadow);
        std::vector<Pixel> c(w*h,{.65f,.57f,.43f,.80f});std::vector<float>d(w*h);
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){
            float ray_y=((y+.5f)/h*2-1)*.5994296f;
            float z=mode==3?80.f:mode==2?.4f:ray_y<0?1.75f/-ray_y:15000.f;
            d[y*w+x]=(15000.f+.4f-2*.4f*15000.f/z)/(15000.f-.4f)*.5f+.5f;
            if(mode==1)d[y*w+x]=1;
        }
        color=texture(GL_RGBA16F,GL_RGBA,w,h,reinterpret_cast<float*>(c.data()));depth=texture(GL_R32F,GL_RED,w,h,d.data());
        float zero=0,one=1;macro=texture(GL_R32F,GL_RED,1,1,&zero);shadow=texture(GL_R32F,GL_RED,1,1,&one);
    }
    ~Input(){glDeleteTextures(1,&color);glDeleteTextures(1,&depth);glDeleteTextures(1,&macro);glDeleteTextures(1,&shadow);}
};
std::vector<Pixel> pixels(GLuint t,int w,int h){
    std::vector<Pixel>p(w*h);glBindTexture(GL_TEXTURE_2D,t);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,p.data());
    for(auto &v:p)for(float c:v)assert(std::isfinite(c));
    return p;
}
double difference(const std::vector<Pixel>&a,const std::vector<Pixel>&b){
    assert(a.size()==b.size());double result=0;
    for(std::size_t i=0;i<a.size();i++)for(int c=0;c<4;c++)result+=std::abs(a[i][c]-b[i][c]);
    return result;
}
WorldWeatherFrame frame(const Input &in,float &ground){
    WorldWeatherFrame f;f.color=in.color;f.depth=in.depth;f.shadow=in.shadow;f.macro_height=in.macro;f.width=in.w;f.height=in.h;
    f.camera[1]=1.75;f.time=17.37f;f.sun[1]=0;f.sun[2]=-1;f.surface_height=height;f.height_userdata=&ground;
    f.light_vp[0]=f.light_vp[5]=f.light_vp[10]=.005f;f.light_vp[15]=1;return f;
}
void prepare(WorldWeather *w,const WorldWeatherFrame &f){
    auto before=world_weather_stats(w).height_queries;
    for(int row=0;row<9;row++){
        world_weather_update(w,f,999);auto s=world_weather_stats(w);assert(s.terrain_rows_this_update<=8);
    }
    assert(world_weather_stats(w).height_queries-before==65*65);
}
void state(int w,int h){
    assert(!glIsEnabled(GL_DEPTH_TEST)&&!glIsEnabled(GL_BLEND)&&!glIsEnabled(GL_CULL_FACE)&&!glIsEnabled(GL_SCISSOR_TEST));
    GLint viewport[4]={};glGetIntegerv(GL_VIEWPORT,viewport);assert(viewport[0]==0&&viewport[1]==0&&viewport[2]==w&&viewport[3]==h);
    GLboolean mask=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);assert(!mask);
    GLboolean color[4]={};glGetBooleanv(GL_COLOR_WRITEMASK,color);for(bool c:color)assert(c);
}
}
void *operator new(std::size_t size){
    if(track)allocations++;
    if(void *p=std::malloc(size?size:1))return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p,std::size_t) noexcept {std::free(p);}
extern "C" float world_atlas_height(double,double){return 0;}
int main(int argc,char **argv){
    std::filesystem::path root=argc>1?argv[1]:".";
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Weather test",0,0,192,120,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    SDL_GLContext ctx=SDL_GL_CreateContext(window);assert(ctx);std::printf("Weather renderer: %s\n",glGetString(GL_RENDERER));
    assert(!world_weather_create("/nonexistent"));
    WorldWeather *w=world_weather_create((root/"shaders").c_str());assert(w&&world_weather_stats(w).bytes==0);
    {
        Input input;input.fill(0);float zero=0,raised=100;auto f=frame(input,zero);
        world_weather_update(w,f);assert(!queries);
        assert(world_weather_resolve(w,f)==input.color&&world_weather_stats(w).bytes==0);
        auto source=pixels(input.color,input.w,input.h);
        f.intensity=1;prepare(w,f);assert(queries==65*65);
        GLuint result=world_weather_resolve(w,f);assert(result&&result!=input.color&&result!=input.depth);
        auto rainy=pixels(result,input.w,input.h);double rain_delta=difference(source,rainy);assert(rain_delta>1);
        float min_alpha=1;unsigned moving_pixels=0,untouched_pixels=0;
        for(std::size_t i=0;i<rainy.size();i++){
            const auto &p=rainy[i];assert(p[3]<=source[0][3]+.0001f);min_alpha=std::min(min_alpha,p[3]);
            if(p[3]==0){moving_pixels++;}
            else {assert(p==source[i]);untouched_pixels++;}
        }
        assert(min_alpha==0&&moving_pixels>0&&untouched_pixels>0);
        auto stats=world_weather_stats(w);assert(stats.terrain_cached&&stats.cache_generations==1&&stats.rain_instances==4096&&stats.splash_instances==768);
        assert(stats.bytes==(std::uint64_t(input.w)*input.h+std::uint64_t((input.w+1)/2)*((input.h+1)/2))*8+65*65*8);
        state(input.w,input.h);
        assert(difference(pixels(input.color,input.w,input.h),source)==0);
        world_weather_resolve(w,f);assert(difference(pixels(result,input.w,input.h),rainy)==0);
        float dark_rain=0;glBindTexture(GL_TEXTURE_2D,input.shadow);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RED,GL_FLOAT,&dark_rain);
        world_weather_resolve(w,f);auto shaded_rain=pixels(result,input.w,input.h);
        assert(difference(shaded_rain,rainy)>.1);
        for(std::size_t i=0;i<rainy.size();i++)assert(shaded_rain[i][3]==rainy[i][3]);
        float bright_rain=1;glBindTexture(GL_TEXTURE_2D,input.shadow);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RED,GL_FLOAT,&bright_rain);
        world_weather_resolve(w,f);assert(difference(pixels(result,input.w,input.h),rainy)==0);
        f.wind_xz[0]=2.1f;f.wind_xz[1]=.7f;world_weather_resolve(w,f);
        double wind_delta=difference(pixels(result,input.w,input.h),rainy);assert(wind_delta>1);
        f.time+=.07f;world_weather_resolve(w,f);assert(difference(pixels(result,input.w,input.h),rainy)>1);
        // An occluder at the near clip hides every weather fragment. This
        // proves depth rejection, not merely a conventional GL depth state.
        input.fill(2);f=frame(input,zero);f.intensity=1;world_weather_resolve(w,f);
        assert(difference(pixels(result,input.w,input.h),pixels(input.color,input.w,input.h))<.0001);
        // Rain cannot exist below the cached surface. A large raised surface
        // also puts every horizontal splash outside this level camera's FOV.
        input.fill(1);f=frame(input,raised);f.intensity=1;prepare(w,f);world_weather_resolve(w,f);
        source=pixels(input.color,input.w,input.h);assert(difference(pixels(result,input.w,input.h),source)<.0001);
        // A supplied pool cap overrides the interpolated raised cache with an
        // exact0m support plane, restoring real3D rain/splash geometry.
        f.pool_bounds[0]=0;f.pool_bounds[1]=-15;f.pool_bounds[2]=f.pool_bounds[3]=100;
        world_weather_resolve(w,f);assert(difference(pixels(result,input.w,input.h),source)>1);
        // Conservative roof caps stop rain without becoming floating splash
        // surfaces. Until a complete cover generation arrives, no indoor leak.
        f=frame(input,zero);f.intensity=1;prepare(w,f);world_weather_resolve(w,f);
        auto uncovered=pixels(result,input.w,input.h);assert(difference(uncovered,source)>1);
        f.cover_height=height;f.cover_userdata=&raised;
        auto cover_before=world_weather_stats(w).cover_queries;
        world_weather_update(w,f,1);assert(world_weather_stats(w).cover_queries-cover_before==65);
        world_weather_resolve(w,f);assert(difference(pixels(result,input.w,input.h),source)<.0001);
        for(int row=0;row<8;row++)world_weather_update(w,f);
        assert(world_weather_stats(w).cover_queries-cover_before==65*65);
        world_weather_resolve(w,f);assert(difference(pixels(result,input.w,input.h),source)<.0001);
        float nonfinite=NAN;f.cover_userdata=&nonfinite;prepare(w,f);world_weather_resolve(w,f);
        assert(difference(pixels(result,input.w,input.h),uncovered)==0);
        // Fog is independently enabled when rain is zero. Clear sky remains
        // exactly neutral, and only shadowed direct in-scattering changes.
        f=frame(input,zero);f.fog_strength=1;prepare(w,f);world_weather_resolve(w,f);
        assert(difference(pixels(result,input.w,input.h),source)<.0001);
        input.fill(3);f=frame(input,zero);f.fog_strength=1;world_weather_resolve(w,f);
        auto lit=pixels(result,input.w,input.h);source=pixels(input.color,input.w,input.h);double fog_delta=difference(lit,source);assert(fog_delta>1);
        for(auto p:lit)assert(std::abs(p[3]-source[0][3])<.0001f);
        f.cover_height=height;f.cover_userdata=&raised;prepare(w,f);world_weather_resolve(w,f);
        assert(difference(pixels(result,input.w,input.h),lit)==0);
        float dark=0;glBindTexture(GL_TEXTURE_2D,input.shadow);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RED,GL_FLOAT,&dark);
        world_weather_resolve(w,f);auto shadowed=pixels(result,input.w,input.h);
        double shaft_delta=0;for(std::size_t i=0;i<lit.size();i++)shaft_delta+=lit[i][0]-shadowed[i][0];assert(shaft_delta>.1);
        // Invalid state/feedback fail cleanly, without size-dependent work.
        auto before=world_weather_stats(w);auto invalid=f;invalid.forward[2]=0;
        assert(world_weather_resolve(w,invalid)==input.color&&world_weather_stats(w).resolves==before.resolves);
        invalid=f;invalid.camera[0]=NAN;assert(world_weather_resolve(w,invalid)==input.color);
        for(GLuint own:{result,world_weather_fog_field(w)})for(int slot=0;slot<4;slot++){
            invalid=f;if(slot==0)invalid.color=own;if(slot==1)invalid.depth=own;if(slot==2)invalid.shadow=own;if(slot==3)invalid.macro_height=own;
            assert(world_weather_resolve(w,invalid)==invalid.color);
        }
        // Complete lazy driver work before measuring the application hot path.
        // Then vary position across a cache anchor as well as wind/uniforms.
        for(int i=0;i<10;i++){
            f.camera[0]=i*4.;f.wind_xz[0]=i*.03f;world_weather_update(w,f);world_weather_resolve(w,f);
        }
        glFinish();
        // Both stationary and changed camera/wind updates use fixed storage.
        allocations=0;track=true;
        for(int i=0;i<10;i++){
            f.camera[0]=i*4.;f.wind_xz[0]=i*.03f;world_weather_update(w,f);world_weather_resolve(w,f);
        }
        track=false;assert(!allocations&&world_weather_stats(w).resizes==before.resizes);
        f.intensity=f.fog_strength=0;auto disabled=world_weather_stats(w);assert(world_weather_resolve(w,f)==input.color&&world_weather_stats(w).resolves==disabled.resolves);
        auto edited=frame(input,zero);edited.intensity=1;
        world_weather_invalidate_surface(w);prepare(w,edited);auto prior_generation=world_weather_stats(w).cache_generations;
        world_weather_resolve(w,edited);assert(world_weather_stats(w).cache_generations==prior_generation+1);
        // Same function and userdata pointer; only the committed height changed.
        zero=.5f;world_weather_invalidate_surface(w);prepare(w,edited);prior_generation=world_weather_stats(w).cache_generations;
        world_weather_resolve(w,edited);assert(world_weather_stats(w).cache_generations==prior_generation+1);
        zero=0;world_weather_invalidate_surface(w);
        input.w=127;input.h=73;input.fill(0);f=frame(input,zero);f.fog_strength=.7f;
        result=world_weather_resolve(w,f);auto odd=world_weather_stats(w);assert(odd.half_width==64&&odd.half_height==37&&odd.resizes==2);
        pixels(result,input.w,input.h);GLuint field=world_weather_fog_field(w);world_weather_free(w);w=nullptr;
        assert(!glIsTexture(result)&&!glIsTexture(field));
        std::printf("PASS weather: depth-hidden/rain-above-surface/exactpoolcap/roof-exclusion, rain delta%.3f wind delta%.3f alpha%.3f, fog delta%.3f shadowed shafts%.3f, roof caps preserve fog/never create floating impacts, invalid-cover fallback, all drawn-particle alpha0/undrawn pixels unchanged, sky neutral, frozen determinism, 8row cache with explicit edit invalidation, caps4096/768, zero steady calling-thread allocations after driver warmup, state/oddresize/feedback/teardown\n",rain_delta,wind_delta,min_alpha,fog_delta,shaft_delta);
    }
    assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
