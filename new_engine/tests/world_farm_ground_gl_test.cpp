/* Probe the exact production function, not a fixture copy. No native FPS
 * claim: offscreen software GL is the default unless explicitly requested. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_village_farms.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
namespace {
using Pixel=std::array<float,4>;
constexpr double tau=6.283185307179586;
std::string read(const char *path) {std::ifstream f(path);assert(f);std::ostringstream s;s<<f.rdbuf();return s.str();}
std::string function(const std::string &text,const char *signature) {
    const auto begin=text.find(signature);assert(begin!=std::string::npos);
    auto end=text.find('{',begin);assert(end!=std::string::npos);unsigned depth=1;
    for(++end;end<text.size()&&depth;end++){if(text[end]=='{')depth++;else if(text[end]=='}')depth--;}
    assert(!depth);return text.substr(begin,end-begin)+"\n";
}
GLuint shader(GLenum type,const std::string &text) {
    GLuint s=glCreateShader(type);const char *p=text.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);
    GLint okay=0;glGetShaderiv(s,GL_COMPILE_STATUS,&okay);
    if(!okay){char log[8192]={};glGetShaderInfoLog(s,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(okay);return s;
}
GLuint probe() {
    const std::string source=read("shaders/world_terrain.fs.glsl");
    const std::string fs=std::string(R"GLSL(#version 330 core
uniform int uFarmCount;
uniform vec2 uFarmOriginXZ;
uniform vec4 uFarmBounds[4];
uniform vec2 uFarmRows[4];
uniform vec2 tWorld;
uniform float tFootprint;
layout(location=0) out vec4 color;
)GLSL")+function(source,"vec3 village_farm_surface(")+"void main(){color=vec4(village_farm_surface(tWorld,tFootprint),1.);}\n";
    const char *vs="#version 330 core\nvoid main(){vec2 q=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(q*2.-1.,0,1);}";
    const GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs),p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);
    if(!okay){char log[8192]={};glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(okay);
    glDeleteShader(v);glDeleteShader(f);return p;
}
double smooth(double a,double b,double x) {const double t=std::clamp((x-a)/(b-a),0.,1.);return t*t*(3.-2.*t);}
void close(double actual,double expected,double tolerance) {
    if(std::abs(actual-expected)>tolerance)std::fprintf(stderr,"actual %.10g expected %.10g tolerance %.3g\n",actual,expected,tolerance);
    assert(std::abs(actual-expected)<=tolerance);
}
void upload(GLuint p) {
    std::array<float,16> bounds={};std::array<float,8> rows={};
    for(std::size_t i=0;i<WORLD_VILLAGE_FARM_COUNT;i++) {
        const auto &f=world_village_farm_fields[i];bounds[i*4]=float(f.xmin);bounds[i*4+1]=float(f.zmin);
        bounds[i*4+2]=float(f.xmax);bounds[i*4+3]=float(f.zmax);rows[i*2]=float(f.row_spacing);rows[i*2+1]=float(f.row_phase);
    }
    glUseProgram(p);glUniform1i(glGetUniformLocation(p,"uFarmCount"),int(WORLD_VILLAGE_FARM_COUNT));
    glUniform2f(glGetUniformLocation(p,"uFarmOriginXZ"),0,0);
    glUniform4fv(glGetUniformLocation(p,"uFarmBounds[0]"),4,bounds.data());
    glUniform2fv(glGetUniformLocation(p,"uFarmRows[0]"),4,rows.data());
}
Pixel draw(GLuint p,double x,double z,double footprint=0.) {
    glUniform2f(glGetUniformLocation(p,"tWorld"),float(x),float(z));glUniform1f(glGetUniformLocation(p,"tFootprint"),float(footprint));
    glDrawArrays(GL_TRIANGLES,0,3);Pixel pixel;glReadPixels(8,8,1,1,GL_RGBA,GL_FLOAT,pixel.data());
    for(float c:pixel)assert(std::isfinite(c));
    assert(pixel[3]==1&&glGetError()==GL_NO_ERROR);return pixel;
}
void zero(Pixel p) {assert((p==Pixel{0,0,0,1}));}
}
int main() {
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL")){setenv("SDL_VIDEODRIVER","offscreen",1);setenv("LIBGL_ALWAYS_SOFTWARE","1",1);}
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Farm ground regression",0,0,16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Renderer: %s\n",glGetString(GL_RENDERER));
    GLuint fbo=0,target=0,vao=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,16,16,0,GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glViewport(0,0,16,16);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_DITHER);
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);const GLuint p=probe();upload(p);std::size_t samples=0;
    for(std::size_t field=0;field<WORLD_VILLAGE_FARM_COUNT;field++) {
        const auto &f=world_village_farm_fields[field];const double cx=(f.xmin+f.xmax)*.5,cz=(f.zmin+f.zmax)*.5;
        const double row=std::round((cx-f.row_phase)/f.row_spacing)*f.row_spacing+f.row_phase;
        const auto crest=draw(p,row,cz),trough=draw(p,row+f.row_spacing*.5,cz),quarter=draw(p,row+f.row_spacing*.25,cz);
        close(crest[0],1,0);close(crest[1],1,1e-6);close(crest[2],0,.00015);
        close(trough[1],-1,1e-6);close(quarter[1],0,.0005);close(quarter[2],-.012*tau/f.row_spacing,.0002);
        /* Row direction is Z: changing Z cannot slide the furrows. */
        for(double dz:{-12.,-1.,0.,1.,12.})assert(draw(p,row+f.row_spacing*.25,cz+dz)==quarter);
        /* Deliberately wrong phase is an effective negative control. */
        glUniform2f(glGetUniformLocation(p,field?"uFarmRows[1]":"uFarmRows[0]"),float(f.row_spacing),float(f.row_phase+f.row_spacing*.5));
        const auto wrong=draw(p,row,cz);assert(wrong[1]<-.999&&crest[1]-wrong[1]>1.99);upload(p);
        for(int station=0;station<=64;station++) {
            const double x=row+station*f.row_spacing/64.;const double theta=station*tau/64.;
            for(double ratio:{0.,.1,.2,.3,.425,.6,.65,1.,4.}) {
                const auto out=draw(p,x,cz,f.row_spacing*ratio);const double filtered=1.-smooth(.2,.65,ratio);
                close(out[0],1,0);close(out[1],std::cos(theta)*filtered,.0008);
                close(out[2],-std::sin(theta)*(.012*tau/f.row_spacing)*filtered,.0003);
                assert(std::abs(out[1])<=1.000001&&std::abs(out[2])<=.012*tau/f.row_spacing+.000001);
                if(ratio>=.65)assert(out[1]==0&&out[2]==0);
                samples++;
            }
        }
        /* Both boundary axes share the CPU C1 mask. Test corners as well as
         * edges; raw ridge/slope need not fade, their material influence does. */
        for(int axis=0;axis<2;axis++)for(int side=0;side<2;side++)for(int station=-32;station<=32;station++) {
            const double e=station/32.;const double x=axis?cx:(side?f.xmax-e:f.xmin+e),z=axis?(side?f.zmax-e:f.zmin+e):cz;
            const auto out=draw(p,x,z);close(out[0],world_village_farm_mask_local(x,z),.000035);
            if(e<=-.625)zero(out);
            samples++;
        }
        for(double dx:{-.7,-.4,0.,.2,.4})for(double dz:{-.7,-.4,0.,.2,.4}) {
            const auto out=draw(p,f.xmin+dx,f.zmin+dz);close(out[0],world_village_farm_mask_local(f.xmin+dx,f.zmin+dz),.000035);samples++;
        }
        for(double e:{-.6,.2}) {
            constexpr double h=.001;const auto a=draw(p,row,f.zmin+e-h),b=draw(p,row,f.zmin+e+h);
            close((b[0]-a[0])/(2*h),0,.005);
            close((b[0]*b[1]-a[0]*a[1])/(2*h),0,.005);
            close((b[0]*b[2]-a[0]*a[2])/(2*h),0,.005);
        }
        /* All positions remain village anchored. Exact dyadic offsets avoid
         * confusing large-world float quantization with camera movement. */
        for(double ox:{128.,-32768.})for(double oz:{-128.,32768.}) {
            glUniform2f(glGetUniformLocation(p,"uFarmOriginXZ"),float(ox),float(oz));
            assert(draw(p,row+f.row_spacing*.25+ox,cz+oz)==quarter);
        }
        upload(p);
    }
    /* Count and bounds negatives prove an empty/outside branch cannot invent
     * stripes, even with stale NaN array entries after a disabled feature. */
    zero(draw(p,0,0));zero(draw(p,1000,-160));zero(draw(p,141,-160));
    glUniform1i(glGetUniformLocation(p,"uFarmCount"),1);zero(draw(p,160,-160));
    glUniform1i(glGetUniformLocation(p,"uFarmCount"),0);glUniform2f(glGetUniformLocation(p,"uFarmOriginXZ"),NAN,NAN);
    glUniform4f(glGetUniformLocation(p,"uFarmBounds[0]"),NAN,NAN,NAN,NAN);
    zero(draw(p,120,-160));zero(draw(p,NAN,NAN));
    std::printf("PASS farm ground GL: %zu production-function samples; source row phases, phase negative control, corn/wheat analytic amplitudes, unresolved furrows/bump exact0, C1 mask/corners, bounded slopes, world-origin invariance, count0/outside exact0\n",samples);
    glDeleteProgram(p);glDeleteVertexArrays(1,&vao);glDeleteTextures(1,&target);glDeleteFramebuffers(1,&fbo);
    assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
