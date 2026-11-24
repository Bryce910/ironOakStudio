/* Actual opaque-copy and water shader regression. A correctness fixture, not
 * an FPS test. Defaults to an offscreen context; VALWOOD_TEST_NATIVE_GL=1 uses
 * the desktop driver. Run from engine root or pass it as argv1. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_water_scene.h"
#include "engine/world_castle_creek.h"
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
    std::ifstream file(fragment);assert(file.good());std::stringstream source;source<<file.rdbuf();
    return program_source(vertex,source.str());
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
uniform float uTestDepth;uniform vec3 uTestWorld,uTestRelative;uniform bool uUseTestRelative;out vec3 vRelative,vPos;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,uTestDepth*2.-1.,1.);vRelative=uUseTestRelative?uTestRelative:vec3(0,-2,0);vPos=uTestWorld;}
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
        // The weather reflection kernel must preserve a constant environment
        // (weights sum to one), and still transmit real HDR reflection data.
        GLuint reflection;glGenTextures(1,&reflection);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,reflection);
        Pixel reflected={.1f,.1f,.1f,1};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,reflected.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        const float reflection_matrix[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
        glUniformMatrix4fv(glGetUniformLocation(p,"uReflectionVP"),1,GL_FALSE,reflection_matrix);
        v3("uSunColor",0,0,0);v3("uFog",.1f,.1f,.1f);f("uRainIntensity",.6f);f("uTime",.4f);
        i("uHasReflection",0);const Pixel environment=draw(10,false);
        i("uHasReflection",1);expect(draw(10,false),environment,.00001f);
        reflected={2,.5f,.2f,1};glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,reflection);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_FLOAT,reflected.data());
        assert(draw(10,false)[0]>environment[0]+.01f);
        i("uHasReflection",0);v3("uFog",0,0,0);glDeleteTextures(1,&reflection);
        f("uRainIntensity",0);glUniform2f(glGetUniformLocation(p,"uWeatherWind"),0,0);i("uLocalPool",0);f("uWaterLevel",0);f("uEyeHeight",2);i("uUseTestRelative",0);v3("uTestWorld",0,0,0);v3("uSunColor",0,0,0);f("uTime",0);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,climate);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_FLOAT,climate_pixel.data());expect(draw(10,false),fallback,.00001f);
        std::printf("Water weather/local pool: raised-level transmission/foreground/ellipse/climate controls PASS; rain glint delta%g wind%g; zero settings restore global baseline\n",rain_delta,wind_delta);
        // Even with no opaque terrain/depth available, the actual moat shader
        // must leave the entire outer-wall enclosure and former inner creek dry.
        static_assert(sizeof(world_castle_creek_points)/sizeof(world_castle_creek_points[0])<=32,"Shader creek capacity");
        std::vector<float> creek_points;
        for(const auto &point:world_castle_creek_points)
            creek_points.insert(creek_points.end(),{float(point.x),float(point.z),float(point.half_width)});
        i("uCastleCreekPointCount",int(creek_points.size()/3));
        glUniform3fv(glGetUniformLocation(p,"uCastleCreekPoints[0]"),GLsizei(creek_points.size()/3),creek_points.data());
        constexpr float origin_x=-1310.4f,origin_z=-10207.584747f;
        glUniform2f(glGetUniformLocation(p,"uCastleCreekOrigin"),origin_x,origin_z);
        i("uCastleCreek",1);i("uLocalPool",1);f("uWaterLevel",22.9f);f("uEyeHeight",24.9f);
        size_t dry_tests=0;
        for(int z=-205;z<=205;z+=41)for(int x=-205;x<=205;x+=41) {
            v3("uTestWorld",origin_x+x,0,origin_z+z);expect(draw(10,false),{0,0,0,1});dry_tests++;
        }
        for(const auto &point:std::array<std::array<float,2>,4>{{{0,-46},{80,-125},{-80,-125},{0,-88}}}) {
            v3("uTestWorld",origin_x+point[0],0,origin_z+point[1]);expect(draw(10,false),{0,0,0,1});dry_tests++;
        }
        for(const auto &point:world_castle_creek_points) {
            v3("uTestWorld",origin_x+float(point.x),0,origin_z+float(point.z));
            assert(std::abs(draw(10,false)[3]-.28f)<.001f);
        }
        std::printf("Water actual moat fragment: %zu dry enclosure/former-creek cases without opaque depth; %zu exterior source-point positive controls PASS\n",dry_tests,creek_points.size()/3);

        // Compare complete HDR water frames against the actual pre-overcast
        // shader, retaining every other production expression. This verifies
        // the default castle weather path bit-for-bit rather than comparing
        // two implementations of the new formula.
        std::ifstream water_file("shaders/world_water.fs.glsl");assert(water_file);
        std::stringstream water_source;water_source<<water_file.rdbuf();std::string legacy_source=water_source.str();
        const std::string begin_marker="    // Regional overcast fallback",end_marker="    // End regional overcast fallback.\n";
        const auto begin=legacy_source.find(begin_marker),end=legacy_source.find(end_marker);
        assert(begin!=std::string::npos&&end!=std::string::npos&&end>begin);
        legacy_source.replace(begin,end+end_marker.size()-begin,
            "    sky += uSunColor * pow(max(dot(reflection, uSun), 0.0), 28.0) * .065;\n");
        const GLuint actual=p,legacy=program_source(vs,legacy_source);
        GLuint dark_shadow;glGenTextures(1,&dark_shadow);glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,dark_shadow);
        const float dark=0;glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&dark);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,dark_shadow);
        auto fallback_frame=[&](GLuint selected,float clouds,float eye,float level,bool gray,bool solar,bool reflection_edge=false) {
            p=selected;glUseProgram(p);
            i("uLocalPool",0);i("uCastleCreek",0);i("uHasReflection",reflection_edge);i("uTorchCount",0);i("uUseTestRelative",1);
            i("uOpaqueColor",0);i("uOpaqueDepth",1);i("uClimate",2);i("uReflection",3);i("uShadow",4);
            v3("uTestWorld",0,0,0);v3("uTestRelative",0,-.25f,-2);
            v3("uSun",0,.124034735f,-.9922779f);v3("uSunColor",solar?1.f:0.f,solar?1.f:0.f,solar?1.f:0.f);
            v3("uAmbient",.3f,.3f,.3f);v3("uSkyLight",.3f,.3f,.3f);
            if(gray) {
                const float fog_y=.35f*.2126f+.15f*.7152f+.08f*.0722f;
                const float zenith_y=.03f*.2126f+.12f*.7152f+.4f*.0722f;
                v3("uFog",fog_y*.94f,fog_y,fog_y*.98f);v3("uSkyZenith",zenith_y*.94f,zenith_y,zenith_y*.98f);
            } else {v3("uFog",.35f,.15f,.08f);v3("uSkyZenith",.03f,.12f,.4f);}
            f("uEyeHeight",eye);f("uWaterLevel",level);f("uCloud",clouds);f("uTime",0);f("uHaze",0);f("uRainIntensity",0);
            f("uNear",.4f);f("uFar",15000);f("uTestDepth",device(2));
            glUniform2f(glGetUniformLocation(p,"uResolution"),64,64);glUniform2f(glGetUniformLocation(p,"uWorldSize"),100800,56708.8f);
            glUniform2f(glGetUniformLocation(p,"uWeatherWind"),0,0);
            glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,light_matrix);
            // Screen-edge fallback must match disabling the planar reflection.
            const float outside[16]={0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,1};
            glUniformMatrix4fv(glGetUniformLocation(p,"uReflectionVP"),1,GL_FALSE,outside);
            draw(10,false);glBindFramebuffer(GL_FRAMEBUFFER,result.fbo);
            std::vector<Pixel> frame(64*64);glReadPixels(0,0,64,64,GL_RGBA,GL_FLOAT,frame.data());
            for(const auto &pixel:frame)for(float value:pixel)assert(std::isfinite(value));
            return frame;
        };
        auto same_frame=[](const std::vector<Pixel> &a,const std::vector<Pixel> &b) {
            assert(a.size()==b.size());return std::memcmp(a.data(),b.data(),a.size()*sizeof(Pixel))==0;
        };
        auto frame_difference=[](const std::vector<Pixel> &a,const std::vector<Pixel> &b) {
            assert(a.size()==b.size());float maximum=0;
            for(size_t n=0;n<a.size();n++)for(unsigned c=0;c<3;c++)maximum=std::max(maximum,std::abs(a[n][c]-b[n][c]));
            return maximum;
        };
        for(float clouds:{0.f,.28f,.65f})
            assert(same_frame(fallback_frame(actual,clouds,134.25f,134,false,true),
                              fallback_frame(legacy,clouds,134.25f,134,false,true)));
        const auto overcast=fallback_frame(actual,.96f,134.25f,134,false,true);
        assert(same_frame(overcast,fallback_frame(actual,.96f,134.25f,134,false,false)));
        const auto gray_reference=fallback_frame(legacy,.96f,134.25f,134,true,false);
        assert(frame_difference(overcast,gray_reference)<.00002f);
        const float wrong_sky=frame_difference(overcast,fallback_frame(legacy,.96f,134.25f,134,false,true));
        const float old_solar=frame_difference(fallback_frame(legacy,.96f,134.25f,134,false,true),
                                              fallback_frame(legacy,.96f,134.25f,134,false,false));
        assert(wrong_sky>.01f&&old_solar>.002f); // Both negative controls are visibly active.
        assert(same_frame(overcast,fallback_frame(actual,.96f,134.25f,134,false,true,true)));
        // The reflection observes from the mirrored camera, not physical eye
        // height: a high observer above a low lake still reflects its low sky.
        assert(same_frame(overcast,fallback_frame(actual,.96f,5000,134,false,true)));
        assert(same_frame(fallback_frame(actual,.96f,5000.25f,5000,false,true),
                          fallback_frame(legacy,.96f,5000.25f,5000,false,true)));
        auto previous_frame=fallback_frame(actual,.65f,134.25f,134,false,true);float transition_step=0;
        for(unsigned step=1;step<=30;step++) {
            auto current=fallback_frame(actual,.65f+float(step)*.01f,134.25f,134,false,true);
            transition_step=std::max(transition_step,frame_difference(previous_frame,current));previous_frame=std::move(current);
        }
        assert(transition_step<.007f&&frame_difference(previous_frame,overcast)<.00002f);
        std::printf("Water overcast fallback: clear/default<=.65 bitexact; full cloud matches neutral sky/no solar glow; reflection edge and mirrored-altitude controls; 30-step max delta%g, legacy sky negative delta%g solar%g PASS\n",
            transition_step,wrong_sky,old_solar);
        p=actual;glDeleteTextures(1,&dark_shadow);glDeleteProgram(legacy);
        glDeleteTextures(1,&climate);glDeleteVertexArrays(1,&vao);glDeleteProgram(p);
    }
    {
        std::ifstream file("shaders/world_water.vs.glsl");assert(file);std::stringstream text;text<<file.rdbuf();
        const std::string fragment="#version 330 core\nin vec3 vRelative,vPos;out vec4 color;void main(){color=vec4(vRelative+vPos,1);}";
        GLuint vertex=shader(GL_VERTEX_SHADER,text.str()),fs=shader(GL_FRAGMENT_SHADER,fragment),p=glCreateProgram();
        glAttachShader(p,vertex);glAttachShader(p,fs);const char *outputs[]={"vRelative","vPos"};glTransformFeedbackVaryings(p,2,outputs,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p);GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);assert(okay);glDeleteShader(vertex);glDeleteShader(fs);
        GLuint buffer,vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,36*sizeof(float),nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
        glUseProgram(p);glUniform1f(glGetUniformLocation(p,"uSize"),1000);glUniform3f(glGetUniformLocation(p,"uOffset"),0,-136,0);glUniform2f(glGetUniformLocation(p,"uOrigin"),100,200);
        auto capture=[&](){glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_TRIANGLES);glDrawArrays(GL_TRIANGLES,0,6);glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);std::array<float,36> v;glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(v),v.data());for(float x:v)assert(std::isfinite(x));return v;};
        const auto global=capture();assert(global[3]==-1000&&global[4]==0&&global[5]==-1000&&global[1]==-136);
        glUniform1i(glGetUniformLocation(p,"uLocalPool"),1);glUniform1f(glGetUniformLocation(p,"uWaterLevel"),134);glUniform4f(glGetUniformLocation(p,"uPoolBounds"),110,225,.1f,1.f/16.f);
        const auto pool=capture();for(int k=0;k<6;++k){assert(std::abs(pool[k*6+1]+2)<.00001f&&pool[k*6+4]==134);assert(pool[k*6+3]>=0&&pool[k*6+3]<=20&&pool[k*6+5]>=9&&pool[k*6+5]<=41);}
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&buffer);glDeleteVertexArrays(1,&vao);glDeleteProgram(p);
        std::puts("Water actual vertex: legacy global plane and raised bounded local pool preserve world/camera coordinates PASS");
    }
    const GLuint color=world_water_scene_color(copy),depth=world_water_scene_depth(copy);
    world_water_scene_free(copy);world_water_scene_free(nullptr);
    assert(!glIsTexture(color)&&!glIsTexture(depth));
    assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::puts("PASS water GL: distinct HDR/depth snapshot, stable reuse, resize, source bindings, real-shader depth transmission, foreground rejection, fallback, finite cloud/night and resource teardown");
}
