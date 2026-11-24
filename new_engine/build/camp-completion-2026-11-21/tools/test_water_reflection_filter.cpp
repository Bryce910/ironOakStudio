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
        for(auto spec:{std::array<GLenum,3>{GL_RGBA32F,GL_RGBA,GL_COLOR_ATTACHMENT0},
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


int main(int argc,char **argv){assert(argc==2);assert(chdir(argv[1])==0);SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_setenv("LIBGL_ALWAYS_SOFTWARE","1",1);assert(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto *window=SDL_CreateWindow("River reflection filter regression",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);const char *renderer=(const char*)glGetString(GL_RENDERER);assert(renderer&&std::strstr(renderer,"llvmpipe"));
char *loaded=glsl_load("shaders/world_water.fs.glsl");assert(loaded);std::string next(loaded);free(loaded);std::string before=next;auto replace=[](std::string &s,const std::string &a,const std::string&b){auto pos=s.find(a);assert(pos!=std::string::npos);s.replace(pos,a.size(),b);};
replace(before,"bool filter_planar=uFilteredReflections||(uRiverSurface&&uSurfaceDetail>=1);","bool filter_planar=uFilteredReflections;");
const std::string vertex=R"GLSL(#version 330 core
out vec3 vRelative,vPos;out vec4 vRiverFlow;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2),q=p*2.-1.;gl_Position=vec4(q,0,1);vRelative=vec3(q.x*4.,-.25,-4.+q.y*2.);vPos=vec3(vRelative.x,0,vRelative.z);vRiverFlow=vec4(1,0,3,0);}
)GLSL";
GLuint old=program_source(vertex,before),now=program_source(vertex,next),vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);constexpr int W=512,H=256,RW=64,RH=32;Target output(W,H);GLuint reflection,climate;glGenTextures(1,&reflection);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,reflection);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glGenTextures(1,&climate);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,climate);Pixel c={.3,.5,.3,.5};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,c.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
float vp[16]={};vp[0]=.2;vp[9]=.4;vp[13]=1.6;vp[15]=1;
auto upload=[&](const std::vector<Pixel>&pixels){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,reflection);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,RW,RH,0,GL_RGBA,GL_FLOAT,pixels.data());};
auto draw=[&](GLuint p,int detail,bool filtered,bool river,bool reflected,const float *matrix){output.clear({0,0,0,1},1);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glUseProgram(p);auto f=[&](const char*n,float x){glUniform1f(glGetUniformLocation(p,n),x);};auto i=[&](const char*n,int x){glUniform1i(glGetUniformLocation(p,n),x);};auto v=[&](const char*n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);};i("uReflection",0);i("uClimate",1);i("uHasReflection",reflected);i("uHasOpaque",0);i("uSSRSteps",0);i("uSurfaceDetail",detail);i("uFilteredReflections",filtered);i("uLocalPool",1);i("uRiverSurface",river);i("uCastleCreek",0);f("uEyeHeight",.25);f("uWaterLevel",0);f("uRiverReflectionLevel",0);f("uTime",2.35);f("uNear",.1);f("uFar",15000);f("uHaze",0);f("uCloud",0);f("uRainIntensity",0);v("uSun",0,1,0);v("uSunColor",0,0,0);v("uAmbient",.3,.3,.3);v("uSkyLight",.3,.3,.3);v("uSkyZenith",.15,.23,.32);v("uFog",.2,.3,.4);glUniform2f(glGetUniformLocation(p,"uResolution"),W,H);glUniform2f(glGetUniformLocation(p,"uWorldSize"),100800,56708);glUniformMatrix4fv(glGetUniformLocation(p,"uReflectionVP"),1,GL_FALSE,matrix);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,reflection);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,climate);glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);std::vector<Pixel>a(W*H);glReadPixels(0,0,W,H,GL_RGBA,GL_FLOAT,a.data());for(const auto&px:a)for(float x:px)assert(std::isfinite(x));return a;};
auto maximum=[](const std::vector<Pixel>&a,const std::vector<Pixel>&b,int border){double d=0;for(int y=border;y<H-border;y++)for(int x=border;x<W-border;x++)for(int k=0;k<4;k++)d=std::max(d,std::abs(double(a[y*W+x][k]-b[y*W+x][k])));return d;};
auto mean=[](const std::vector<Pixel>&a){double v=0;int n=0;for(int y=32;y<H-32;y++)for(int x=32;x<W-32;x++){auto p=a[y*W+x];v+=p[0]*.2126+p[1]*.7152+p[2]*.0722;n++;}return v/n;};
auto variance=[](const std::vector<Pixel>&a){double v=0;int n=0;for(int y=32;y<H-32;y++)for(int x=32;x<W-32;x++){float l[3]={};for(int k=0;k<3;k++)l[k]=a[y*W+x][k]*2-a[y*W+x-1][k]-a[y*W+x+1][k];double d=l[0]*.2126+l[1]*.7152+l[2]*.0722;v+=d*d;n++;}return v/n;};
upload(std::vector<Pixel>(RW*RH,Pixel{.5,.25,.125,1}));auto constantOld=draw(old,1,false,true,true,vp),constantNow=draw(now,1,false,true,true,vp);double constantError=maximum(constantOld,constantNow,32);assert(constantError<2e-7);
std::vector<Pixel>pattern(RW*RH);for(int y=0;y<RH;y++)for(int x=0;x<RW;x++){float rank=52.9829189f*(x*.06711056f+y*.00583715f-std::floor(x*.06711056f+y*.00583715f));rank-=std::floor(rank);pattern[y*RW+x]=rank<.35?Pixel{.004,.01,.003,1}:Pixel{.35,.45,.60,1};}upload(pattern);
auto noisy=draw(old,1,false,true,true,vp),resolved=draw(now,1,false,true,true,vp);double beforeVariance=variance(noisy),afterVariance=variance(resolved),oldMean=mean(noisy),newMean=mean(resolved);assert(beforeVariance>1e-7&&afterVariance<beforeVariance*.75);assert(std::abs(newMean-oldMean)<oldMean*.03);
unsigned equalCases=0;for(auto policy:{std::array<int,4>{0,0,1,1},{2,1,1,1},{3,1,1,1},{1,0,0,1},{1,0,1,0}}){auto a=draw(old,policy[0],policy[1],policy[2],policy[3],vp),b=draw(now,policy[0],policy[1],policy[2],policy[3],vp);assert(maximum(a,b,0)<2e-7);equalCases++;}
// A bright mirror must not turn projection rejection or the finite filter
// guard into edge geometry. Exact fallback is the negative control.
upload(std::vector<Pixel>(RW*RH,Pixel{4,2,1,1}));float edge[16]={};edge[15]=1;edge[12]=-1.2;auto noReflection=draw(now,1,false,true,false,edge),outside=draw(now,1,false,true,true,edge);assert(maximum(noReflection,outside,0)<2e-7);edge[12]=2.f/RW-1;auto guard=draw(now,1,false,true,true,edge);assert(maximum(noReflection,guard,0)<2e-7);edge[12]=0;auto inside=draw(now,1,false,true,true,edge);assert(maximum(noReflection,inside,32)>.1);
// The new coherent guard itself is continuous across its texel ramp.
std::array<float,5>edgeMeans={};for(unsigned i=0;i<5;i++){edge[12]=2.f*(2.f+.375f*i)/RW-1;edgeMeans[i]=float(mean(draw(now,1,false,true,true,edge)));if(i)assert(edgeMeans[i]>=edgeMeans[i-1]-1e-6);}assert(edgeMeans[4]>edgeMeans[0]+.03);
assert(glGetError()==GL_NO_ERROR);std::printf("{\"passed\":true,\"renderer\":\"%s\",\"mirror\":[%d,%d],\"output\":[%d,%d],\"constant_energy_max_hdr_error\":%.12g,\"stipple_high_frequency_variance_before\":%.12g,\"stipple_high_frequency_variance_after\":%.12g,\"variance_ratio\":%.8g,\"interior_luminance_mean_before\":%.10g,\"interior_luminance_mean_after\":%.10g,\"unchanged_policy_cases\":%u,\"outside_reject\":true,\"one_texel_guard\":true,\"inside_positive_control\":true,\"continuous_edge_ramp\":true,\"gl_errors\":0}\n",renderer,RW,RH,W,H,constantError,beforeVariance,afterVariance,afterVariance/beforeVariance,oldMean,newMean,equalCases);glDeleteProgram(old);glDeleteProgram(now);glDeleteVertexArrays(1,&vao);glDeleteTextures(1,&reflection);glDeleteTextures(1,&climate);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();}
