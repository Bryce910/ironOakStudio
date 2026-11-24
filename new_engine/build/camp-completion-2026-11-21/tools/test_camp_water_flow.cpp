/* Actual opaque-copy and water shader regression. A correctness fixture, not
 * an FPS test. Defaults to an offscreen context; VALWOOD_TEST_NATIVE_GL=1 uses
 * the desktop driver. Run from engine root or pass it as argv1. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_water_scene.h"
#include "engine/world_castle_creek.h"
#include "stb_image.h"
#include "gl_helpers.h"
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
using Pixel=std::array<float,4>;
GLuint shader(GLenum type,const std::string &text) {
    GLuint s=glCreateShader(type);const char *p=text.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);
    GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096];glGetShaderInfoLog(s,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}
    assert(ok);return s;
}
GLuint program_source(const std::string &vertex,const std::string &fragment) {
    GLuint v=shader(GL_VERTEX_SHADER,vertex),f=shader(GL_FRAGMENT_SHADER,fragment);
    GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);glDeleteShader(v);glDeleteShader(f);return p;
}
GLuint program(const std::string &vertex,const char *fragment) {
    char *loaded=glsl_load(fragment);assert(loaded);std::string source(loaded);std::free(loaded);
    return program_source(vertex,source);
}
struct Target {
    GLuint fbo=0,color=0,depth=0;int w,h;
    Target(int width,int height):w(width),h(height) {
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        for(auto spec:{std::array<GLenum,3>{GL_RGBA16F,GL_RGBA,GL_COLOR_ATTACHMENT0},
                       std::array<GLenum,3>{GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_DEPTH_ATTACHMENT}}) {
            GLuint &t=spec[2]==GL_COLOR_ATTACHMENT0?color:depth;
            glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
            glTexImage2D(GL_TEXTURE_2D,0,spec[0],w,h,0,spec[1],GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER,spec[2],GL_TEXTURE_2D,t,0);
        }
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    }
    void clear(Pixel p,float z) {
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,w,h);
        glDisable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);
        glClearColor(p[0],p[1],p[2],p[3]);glClearDepth(z);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    }
    ~Target(){glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteTextures(1,&depth);}
};
Pixel read(GLuint t,int w,int h) {
    GLuint fbo;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    Pixel p;glReadPixels(w/2,h/2,1,1,GL_RGBA,GL_FLOAT,p.data());glDeleteFramebuffers(1,&fbo);
    for(float v:p)assert(std::isfinite(v));
    return p;
}
float depth_at(GLuint t,int w,int h) {
    std::vector<float> values(w*h);glBindTexture(GL_TEXTURE_2D,t);glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,values.data());return values[(h/2)*w+w/2];
}
void expect(Pixel p,Pixel q,float e=.003f) {for(int i=0;i<4;i++)assert(std::abs(p[i]-q[i])<e);}
float device(float z) {const float n=.4f,f=15000;return .5f*((f+n)/(f-n)-2*f*n/((f-n)*z))+.5f;}
}

int main(int argc,char **argv) {
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Water regression",0,0,96,96,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Water test renderer: %s\n",glGetString(GL_RENDERER));
    WorldWaterScene *copy=world_water_scene_create();assert(copy);
    assert(!world_water_scene_capture(nullptr,1,64,64));
    assert(!world_water_scene_capture(copy,0,64,64));
    assert(!world_water_scene_capture(copy,1,0,64));
    assert(world_water_scene_color(copy)==0&&world_water_scene_depth(copy)==0&&world_water_scene_bytes(copy)==0);
    {
        Target source(64,64);const Pixel hdr={2.5f,.4f,.15f,1};source.clear(hdr,.37f);
        glEnable(GL_SCISSOR_TEST);glScissor(0,0,1,1);
        assert(world_water_scene_capture(copy,source.fbo,64,64));
        assert(!glIsEnabled(GL_SCISSOR_TEST));
        GLuint color=world_water_scene_color(copy),depth=world_water_scene_depth(copy);
        assert(color!=source.color&&depth!=source.depth);
        expect(read(color,64,64),hdr);assert(std::abs(depth_at(depth,64,64)-.37f)<.000001f);
        assert(world_water_scene_bytes(copy)==64*64*12);
        source.clear({.1f,.2f,.3f,1},.82f);
        // Snapshot remains distinct while the source changes after the copy.
        expect(read(color,64,64),hdr);assert(std::abs(depth_at(depth,64,64)-.37f)<.000001f);
        assert(world_water_scene_capture(copy,source.fbo,64,64));
        assert(world_water_scene_color(copy)==color&&world_water_scene_depth(copy)==depth);
        expect(read(color,64,64),{.1f,.2f,.3f,1});
        GLint read_fbo=0,draw_fbo=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_fbo);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fbo);
        // read() above rebinds a temporary FBO; capture must restore BOTH bindings.
        assert(world_water_scene_capture(copy,source.fbo,64,64));
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_fbo);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fbo);
        assert(GLuint(read_fbo)==source.fbo&&GLuint(draw_fbo)==source.fbo);
        Target larger(96,48);larger.clear(hdr,.64f);
        assert(world_water_scene_capture(copy,larger.fbo,96,48));
        assert(!glIsTexture(color)&&!glIsTexture(depth));
        expect(read(world_water_scene_color(copy),96,48),hdr);
        assert(world_water_scene_bytes(copy)==96*48*12);
        assert(std::abs(depth_at(world_water_scene_depth(copy),96,48)-.64f)<.000001f);
    }
    {
        Target opaque(64,64),result(64,64);
        const std::string vs=R"GLSL(#version 330 core
uniform float uTestDepth;uniform vec3 uTestWorld,uTestRelative;uniform bool uUseTestRelative,uTestFlow;out vec3 vRelative,vPos;out vec4 vRiverFlow;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,uTestDepth*2.-1.,1.);vRelative=uUseTestRelative?uTestRelative:vec3(0,-2,0);vPos=uTestWorld;vRiverFlow=vec4(1,0,.1,.12);if(uTestFlow){vRelative=vec3((p.x*2.-1.)*1.2,-2.,-(p.y*2.-1.)*1.2);vPos=vec3(vRelative.x,0,vRelative.z);vRiverFlow=vec4(1,0,1.2-abs(vPos.z),.12);}}
)GLSL";
        GLuint p=program(vs,"shaders/world_water.fs.glsl"),vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glUseProgram(p);
        auto f=[&](const char *n,float x){glUniform1f(glGetUniformLocation(p,n),x);};
        auto i=[&](const char *n,int x){glUniform1i(glGetUniformLocation(p,n),x);};
        auto v3=[&](const char *n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);};
        glUniform2f(glGetUniformLocation(p,"uResolution"),64,64);
        glUniform2f(glGetUniformLocation(p,"uWorldSize"),100800,56708.8f);
        v3("uForward",0,-1,0);v3("uRight",1,0,0);v3("uUp",0,0,-1);
        v3("uSun",0,1,0);v3("uSunColor",0,0,0);v3("uAmbient",.3f,.3f,.3f);v3("uSkyLight",.3f,.3f,.3f);v3("uSkyZenith",.1f,.1f,.1f);
        f("uNear",.4f);f("uFar",15000);f("uEyeHeight",2);f("uTestDepth",device(2));
        i("uOpaqueColor",0);i("uOpaqueDepth",1);i("uClimate",2);i("uReflection",3);i("uHasReflection",0);
        GLuint climate;glGenTextures(1,&climate);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,climate);
        const Pixel climate_pixel={.3f,.5f,.3f,.5f};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,climate_pixel.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        auto shade=[&](bool has) {
            assert(world_water_scene_capture(copy,opaque.fbo,64,64));
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,world_water_scene_color(copy));
            glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,world_water_scene_depth(copy));
            result.clear({0,0,0,1},1);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glUseProgram(p);i("uHasOpaque",has);
            glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);return read(result.color,64,64);
        };
        auto draw=[&](float z,bool has=true,Pixel bottom={1,.2f,.05f,1}) {
            opaque.clear(bottom,device(z));return shade(has);
        };
        Pixel shallow=draw(2.2f),deep=draw(10.f),fallback=draw(10.f,false),foreground=draw(1.f,true,{0,8,0,1});
        std::printf("Water transmission red: shallow%.5f deep%.5f fallback%.5f\n",shallow[0],deep[0],fallback[0]);
        assert(shallow[0]>deep[0]+.45f&&deep[0]>fallback[0]+.02f);
        expect(foreground,fallback,.004f); // Above-water geometry cannot leak into the water body.
        assert(std::abs(shallow[3]-.28f)<.001f); // alpha is TAA stability, not blending.
        // Original sample is submerged red ground; all adjacent texels are
        // bright above-water foreground. Any nonzero bilinear bend must fall
        // back to the original sample, not blend a bank into the water body.
        std::vector<Pixel> foreground_colors(64*64,Pixel{0,8,0,1});
        std::vector<float> foreground_depths(64*64,device(1));
        foreground_colors[32*64+32]={1,.2f,.05f,1};foreground_depths[32*64+32]=device(4);
        for(float phase:{0.f,3.f,7.f,15.f}) {
            f("uTime",phase);Pixel reference=draw(4);
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,opaque.color);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_RGBA,GL_FLOAT,foreground_colors.data());
            glBindTexture(GL_TEXTURE_2D,opaque.depth);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_DEPTH_COMPONENT,GL_FLOAT,foreground_depths.data());
            expect(shade(true),reference,.004f);
            if(phase==0) {
                // Positive control: identical adjacent green texels now lie
                // below water, so bilinear refraction must actually reach them.
                std::fill(foreground_depths.begin(),foreground_depths.end(),device(5));
                foreground_depths[32*64+32]=device(4);
                glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,opaque.depth);
                glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_DEPTH_COMPONENT,GL_FLOAT,foreground_depths.data());
                Pixel refracted=shade(true);assert(refracted[1]>reference[1]+.02f);
                std::fill(foreground_depths.begin(),foreground_depths.end(),device(1));
                foreground_depths[32*64+32]=device(4);
            }
        }
        // The actual water glint must respect nearby terrain/canopy shadows.
        GLuint shadow;glGenTextures(1,&shadow);glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,shadow);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        float shadow_depth=1;glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&shadow_depth);
        i("uShadow",4);const float light_matrix[16]={.1f,0,0,0,0,.1f,0,0,0,0,.1f,0,0,0,0,1};
        glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,light_matrix);v3("uSunColor",1,1,1);
        Pixel lit_glint=draw(10.f,false);
        glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,shadow);shadow_depth=0;
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RED,GL_FLOAT,&shadow_depth);
        Pixel shaded_glint=draw(10.f,false);
        std::printf("Water glint: lit%.5f shadowed%.5f\n",lit_glint[0],shaded_glint[0]);
        assert(lit_glint[0]>shaded_glint[0]+.005f);
        glDeleteTextures(1,&shadow);v3("uSunColor",0,0,0);
        f("uCloud",1);f("uTime",1000);auto night=draw(10.f,false);
        for(float channel:night)assert(std::isfinite(channel)&&channel>=0);
        // A raised forest pool uses its own finite cap and the real water
        // level for absorption/refraction. Absolute sea-level tests would
        // reject the submerged bottom after this vertical translation.
        f("uCloud",0);f("uTime",0);i("uLocalPool",1);
        glUniform4f(glGetUniformLocation(p,"uPoolBounds"),0,0,.1f,1.f/16.f);
        f("uEyeHeight",2);f("uWaterLevel",0);const Pixel pool_at_sea=draw(2.2f);
        f("uEyeHeight",136);f("uWaterLevel",134);const Pixel raised_pool=draw(2.2f);expect(pool_at_sea,raised_pool,.004f);
        const Pixel raised_deep=draw(10.f);assert(raised_pool[0]>raised_deep[0]+.20f);
        Pixel pool_foreground=draw(1,true,{0,8,0,1});expect(pool_foreground,{0,0,0,1});
        v3("uTestWorld",11,0,0);expect(draw(4),{0,0,0,1});v3("uTestWorld",0,0,0);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,climate);Pixel land={1,1,1,1},ocean={0,0,0,0};
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_FLOAT,land.data());Pixel local_land=draw(4);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,climate);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_FLOAT,ocean.data());expect(draw(4),local_land,.00001f);
        // Weather changes the actual normals/glint, not a screen color wash.
        i("uUseTestRelative",1);v3("uTestRelative",0,-2,-2);v3("uTestWorld",.25f,0,.25f);
        const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,identity);
        v3("uSun",0,.70710678f,-.70710678f);v3("uSunColor",1,1,1);
        float rain_delta=0,wind_delta=0;
        for(int frame=0;frame<12;++frame) {
            f("uTime",frame*.07f);f("uRainIntensity",0);glUniform2f(glGetUniformLocation(p,"uWeatherWind"),0,0);Pixel calm=draw(10,false);
            f("uRainIntensity",.6f);Pixel rainy=draw(10,false);rain_delta=std::max(rain_delta,std::abs(calm[0]-rainy[0]));assert(std::abs(rainy[3]-(.28f-.6f*.06f))<.001f);
            f("uRainIntensity",0);glUniform2f(glGetUniformLocation(p,"uWeatherWind"),1.1f,.35f);Pixel windy=draw(10,false);wind_delta=std::max(wind_delta,std::abs(calm[0]-windy[0]));
        }
        assert(rain_delta>.0001f&&wind_delta>.0001f);

        i("uLocalPool",0);i("uCastleCreek",0);i("uRiverSurface",1);i("uHasReflection",0);i("uSSRSteps",0);
        i("uUseTestRelative",0);i("uTestFlow",1);f("uEyeHeight",2);f("uWaterLevel",0);f("uRainIntensity",0);
        v3("uSun",0,1,0);v3("uSunColor",1,1,1);v3("uFog",.1f,.1f,.1f);f("uCloud",0);f("uHaze",0);
        v3("uTestWorld",0,0,0);f("uTime",2.35f);
        auto image=[&](){draw(2.12f,true,{.18f,.12f,.08f,1});glBindFramebuffer(GL_FRAMEBUFFER,result.fbo);std::vector<Pixel> out(64*64);glReadPixels(0,0,64,64,GL_RGBA,GL_FLOAT,out.data());for(auto x:out)for(float y:x)assert(std::isfinite(y));return out;};
        auto difference=[](const std::vector<Pixel>&a,const std::vector<Pixel>&b){double sum=0;for(size_t k=0;k<a.size();++k)for(unsigned c=0;c<3;++c)sum+=std::abs(a[k][c]-b[k][c]);return sum;};
        i("uSurfaceDetail",0);const auto low=image();i("uSurfaceDetail",1);const auto medium=image();
        i("uSurfaceDetail",2);const auto high=image();i("uSurfaceDetail",3);const auto ultra=image();
        const double low_medium=difference(low,medium),medium_high=difference(medium,high),high_ultra=difference(high,ultra);
        assert(low_medium>.001&&medium_high>.001&&high_ultra>.001);
        f("uTime",3.05f);const auto moving=image();assert(difference(ultra,moving)>.01);
        f("uTime",2.35f);assert(!std::memcmp(image().data(),ultra.data(),ultra.size()*sizeof(Pixel)));
        i("uTestFlow",0);i("uUseTestRelative",1);v3("uTestRelative",0,1,-2);f("uEyeHeight",-1);const auto below=image();assert(difference(ultra,below)>.1);
        std::printf("PASS flowing-bank water: all4detailtiers have measurable distinct HDR response %g/%g/%g, world flow changes over time, frozen deterministic, finite underwater side; independent of reflections\\n",low_medium,medium_high,high_ultra);
        glDeleteTextures(1,&climate);glDeleteVertexArrays(1,&vao);glDeleteProgram(p);
    }
    world_water_scene_free(copy);assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
