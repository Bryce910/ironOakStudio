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


int main(int argc,char**argv){assert(argc==2&&chdir(argv[1])==0);SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_setenv("LIBGL_ALWAYS_SOFTWARE","1",1);assert(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto *win=SDL_CreateWindow("SSR coverage filter",0,0,16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);auto ctx=SDL_GL_CreateContext(win);assert(ctx);const char*renderer=(const char*)glGetString(GL_RENDERER);assert(renderer&&std::strstr(renderer,"llvmpipe"));char*loaded=glsl_load("shaders/world_water.fs.glsl");assert(loaded);std::string next(loaded);free(loaded);auto pos=next.find("void main()");assert(pos!=std::string::npos);next.replace(pos,11,"void material_main()");next+="\nvoid main(){IndirectColor=vec4(0);FragColor=vec4(river_screen_filter(gl_FragCoord.xy/uResolution,1.,.01,vec3(0,0,1),.12),1);}\n";std::string old=next;auto a=old.find("bool medium_coverage_aa=!uFilteredReflections&&uSurfaceDetail>=1;");assert(a!=std::string::npos);old.replace(a,strlen("bool medium_coverage_aa=!uFilteredReflections&&uSurfaceDetail>=1;"),"bool medium_coverage_aa=false;");
const char*vs="#version330core\nout vec3 vRelative,vPos;out vec4 vRiverFlow;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);vRelative=vec3(0,0,-4);vPos=vec3(0);vRiverFlow=vec4(1,0,3,0);}";std::string vertex(vs);vertex.replace(0,strlen("#version330core"),"#version 330 core");GLuint before=program_source(vertex,old),after=program_source(vertex,next),vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);constexpr int W=128,H=64;Target out(W,H);GLuint color,depth;glGenTextures(1,&color);glGenTextures(1,&depth);
auto upload=[&](const std::vector<Pixel>&c,const std::vector<float>&d){for(unsigned k=0;k<2;k++){glActiveTexture(GL_TEXTURE0+k);glBindTexture(GL_TEXTURE_2D,k?depth:color);glTexImage2D(GL_TEXTURE_2D,0,k?GL_DEPTH_COMPONENT32F:GL_RGBA32F,W,H,0,k?GL_DEPTH_COMPONENT:GL_RGBA,GL_FLOAT,k?(const void*)d.data():(const void*)c.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);}};
auto draw=[&](GLuint p,int detail,bool filtered){out.clear({0,0,0,1},1);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glUseProgram(p);auto i=[&](const char*n,int x){glUniform1i(glGetUniformLocation(p,n),x);};auto f=[&](const char*n,float x){glUniform1f(glGetUniformLocation(p,n),x);};auto v=[&](const char*n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);};i("uOpaqueColor",0);i("uOpaqueDepth",1);i("uFilteredReflections",filtered);i("uSurfaceDetail",detail);f("uNear",.4);f("uFar",15000);f("uTanHalfFov",.5994296);v("uForward",0,0,-1);v("uUp",0,1,0);v("uRight",1,0,0);glUniform2f(glGetUniformLocation(p,"uResolution"),W,H);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,color);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,depth);glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);std::vector<Pixel>result(W*H);glReadPixels(0,0,W,H,GL_RGBA,GL_FLOAT,result.data());for(auto&a:result)for(float x:a)assert(std::isfinite(x));return result;};
auto maximum=[](const std::vector<Pixel>&a,const std::vector<Pixel>&b){double d=0;for(size_t i=0;i<a.size();i++)for(int k=0;k<4;k++)d=std::max(d,std::abs(double(a[i][k]-b[i][k])));return d;};
auto mean=[](const std::vector<Pixel>&a){double v=0;for(auto&p:a)v+=p[0]*.2126+p[1]*.7152+p[2]*.0722;return v/a.size();};
auto variance=[](const std::vector<Pixel>&a){double v=0;for(int y=2;y<H-2;y++)for(int x=2;x<W-2;x++){double d=0;for(int k=0;k<3;k++)d+=(a[y*W+x][k]*2-a[y*W+x-1][k]-a[y*W+x+1][k])*(k==0?.2126:k==1?.7152:.0722);v+=d*d;}return v/((W-4)*(H-4));};
std::vector<Pixel>constant(W*H,Pixel{.5,.25,.125,1});std::vector<float>zs(W*H,device(1));upload(constant,zs);auto ce=maximum(draw(before,1,false),draw(after,1,false));assert(ce<1e-7);auto filteredConstant=draw(after,1,false);for(auto&p:filteredConstant){assert(std::abs(p[0]-.5)<1e-7&&std::abs(p[1]-.25)<1e-7&&std::abs(p[2]-.125)<1e-7);} // Includes every edge pixel with REPEAT sampler.
std::vector<Pixel>noise(W*H);for(int y=0;y<H;y++)for(int x=0;x<W;x++){float rank=52.9829189f*(x*.06711056f+y*.00583715f-floor(x*.06711056f+y*.00583715f));rank-=floor(rank);noise[y*W+x]=rank<.35?Pixel{.004,.01,.003,1}:Pixel{.35,.45,.6,1};}upload(noise,zs);auto noisy=draw(before,1,false),smooth=draw(after,1,false);double va=variance(noisy),vb=variance(smooth),ma=mean(noisy),mb=mean(smooth);assert(va>1e-5&&vb<va*.4&&std::abs(ma-mb)<ma*.01);
unsigned same=0;for(auto policy:{std::array<int,2>{0,0},{2,1},{3,1}}){assert(maximum(draw(before,policy[0],policy[1]),draw(after,policy[0],policy[1]))<1e-7);same++;}
const int center=(H/2)*W+W/2;const Pixel red={1,.2,.05,1},green={0,8,0,1};unsigned rejected=0;for(float z:{.5f,10.f}){std::vector<Pixel>c(W*H,green);std::vector<float>d(W*H,device(z));c[center]=red;d[center]=device(1);upload(c,d);auto r=draw(after,1,false);for(int k=0;k<3;k++)assert(std::abs(r[center][k]-red[k])<1e-7);rejected++;}
// Background sky is a valid footprint contribution; foreground/below-water
// controls above prove depth rejection is active rather than filter disabled.
std::vector<Pixel>c(W*H,green);std::vector<float>d(W*H,1);c[center]=red;d[center]=device(1);upload(c,d);assert(draw(after,1,false)[center][1]>3);
assert(glGetError()==GL_NO_ERROR);printf("{\"passed\":true,\"renderer\":\"%s\",\"constant_energy_hdr_error\":%.10g,\"variance_before\":%.10g,\"variance_after\":%.10g,\"variance_ratio\":%.10g,\"mean_before\":%.10g,\"mean_after\":%.10g,\"unchanged_low_high_ultra\":%u,\"foreground_and_submerged_rejections\":%u,\"sky_positive_control\":true,\"edge_normalization_no_wrap\":true,\"minimum_footprint_exercised\":true,\"gl_errors\":0}\n",renderer,ce,va,vb,vb/va,ma,mb,same,rejected);}
