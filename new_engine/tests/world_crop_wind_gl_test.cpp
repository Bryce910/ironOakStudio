// Actual color/shadow VS transform feedback and actual crop fragment coverage.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
namespace {
struct Vertex {float p[3],n[3],uv[2],t[4];};
struct Output {float pos[3],relative[3],normal[3],tangent[3],bitangent[3],root[3];};
std::string source(const char *path){std::ifstream f(path);assert(f);std::ostringstream s;s<<f.rdbuf();return s.str();}
GLuint shader(GLenum kind,const std::string &text){GLuint s=glCreateShader(kind);const char*p=text.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192]={};glGetShaderInfoLog(s,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);return s;}
GLuint program(const std::string &vs,const std::string &fs,bool feedback){GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs),p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);
    if(feedback){const char *names[]={"vPos","vRelative","vNormal","vTangent","vBitangent","vRootRelative"};glTransformFeedbackVaryings(p,6,names,GL_INTERLEAVED_ATTRIBS);}
    glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[8192]={};glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);glDeleteShader(v);glDeleteShader(f);return p;}
void i(GLuint p,const char*n,int x){glUniform1i(glGetUniformLocation(p,n),x);}void f(GLuint p,const char*n,float x){glUniform1f(glGetUniformLocation(p,n),x);}void v(GLuint p,const char*n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);}
void defaults(GLuint p){glUseProgram(p);i(p,"uPoseBlendEnabled",0);i(p,"uInstanceFade",0);i(p,"uVegetation",6);i(p,"uGrass",0);i(p,"uLodLevel",-1);f(p,"uWind",.6f);f(p,"uTime",3);f(p,"uTreeHeight",2.2f);
    const char *samplers[]={"uBase","uNormal","uARM","uShadow","uMacroHeight","uVillageSkyMask","uVillageSkyDirectional","uVillageSkyUp","uFacadeAux0","uFacadeAux1","uFacadeAux2"};const int units[]={0,1,8,7,11,12,13,14,8,9,10};for(unsigned k=0;k<11;k++)i(p,samplers[k],units[k]);
    i(p,"uLocalProbe0",4);i(p,"uLocalProbe1",5);i(p,"uLocalProbeCount",0);f(p,"uLocalIrradianceWeight",0);
    float m[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,m);glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,m);
    v(p,"uOffset",0,-24,0);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),-1310.5f,-10207.f);glUniform2f(glGetUniformLocation(p,"uWeatherWind"),0,0);
    glUniform2f(glGetUniformLocation(p,"uWorldSize"),100800,56708.8f);i(p,"uHasBase",0);i(p,"uHasNormal",0);i(p,"uHasARM",0);i(p,"uFacadeMode",0);f(p,"uAlphaCutoff",0);glUniform4f(glGetUniformLocation(p,"uBaseFactor"),.3f,.4f,.1f,1);f(p,"uRoughness",.8f);
    v(p,"uAmbient",.2f,.2f,.2f);v(p,"uSun",0,1,0);v(p,"uSunColor",0,0,0);f(p,"uPixelScale",900);i(p,"uWindHistoryEnabled",1);f(p,"uWindFrameDelta",1.f/60);
}
double length(const float *a){return std::sqrt(double(a[0])*a[0]+double(a[1])*a[1]+double(a[2])*a[2]);}
double dot(const float*a,const float*b){return double(a[0])*b[0]+double(a[1])*b[1]+double(a[2])*b[2];}
std::array<Output,5> capture(GLuint p,GLuint buffer){glUseProgram(p);glEnable(GL_RASTERIZER_DISCARD);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);glBeginTransformFeedback(GL_POINTS);glDrawArraysInstanced(GL_POINTS,0,5,1);glEndTransformFeedback();std::array<Output,5> out;glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(out),out.data());assert(glGetError()==GL_NO_ERROR);return out;}
const char *flat_vs=R"GLSL(#version 330 core
uniform float testDistance;
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;
out vec2 vUV;out float vFacadePaint;
flat out vec3 vRootRelative;
flat out float vRootHeight,vSeed,vScale,vCoveragePhase,vInstanceFade;
void main(){vec2 q=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(q*2.-1.,0,1);
vPos=vec3(0,24,0);vRelative=vec3(0,-1,-max(testDistance,1.));vRootRelative=vec3(0,0,-testDistance);
vNormal=vec3(0,1,0);vTangent=vec3(1,0,0);vBitangent=vec3(0,0,-1);vUV=vec2(.5);vFacadePaint=0.;
vRootHeight=24.;vSeed=.5;vScale=1.;vCoveragePhase=.17;vInstanceFade=1.;})GLSL";
using Mask=std::array<bool,64*64>;
Mask mask(GLuint p,float distance,int lod,bool shadow=false){glUseProgram(p);f(p,"testDistance",distance);v(p,"uPropLodRange",60,4,float(lod));
    glDisable(GL_RASTERIZER_DISCARD);glDepthMask(GL_TRUE);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(shadow)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);glDrawArrays(GL_TRIANGLES,0,3);Mask m;
    if(shadow){std::array<float,64*64> d;glReadPixels(0,0,64,64,GL_DEPTH_COMPONENT,GL_FLOAT,d.data());for(unsigned j=0;j<m.size();j++)m[j]=d[j]<.75f;}
    else{std::array<float,64*64*4> c;glReadPixels(0,0,64,64,GL_RGBA,GL_FLOAT,c.data());for(unsigned j=0;j<m.size();j++){for(unsigned k=0;k<4;k++)assert(std::isfinite(c[j*4+k]));m[j]=c[j*4+3]>0;}}
    assert(glGetError()==GL_NO_ERROR);return m;
}
}
int main(){if(!std::getenv("VALWOOD_TEST_NATIVE_GL")){setenv("SDL_VIDEODRIVER","offscreen",1);setenv("LIBGL_ALWAYS_SOFTWARE","1",1);}assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Crop wind correctness",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Renderer: %s\n",glGetString(GL_RENDERER));
    const auto vs=source("shaders/world_foliage.vs.glsl"),fs=source("shaders/world_foliage.fs.glsl"),shadow=source("shaders/world_foliage_shadow.fs.glsl");
    GLuint color=program(vs,fs,true),depth=program(vs,shadow,true),vao=0,vbo=0,instances=0,tf=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,5*sizeof(Vertex),nullptr,GL_DYNAMIC_DRAW);
    const int sizes[]={3,3,2,4},offsets[]={0,3,6,8};for(unsigned k=0;k<4;k++){glEnableVertexAttribArray(k);glVertexAttribPointer(k,sizes[k],GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(std::uintptr_t(offsets[k]*sizeof(float))));}
    glGenBuffers(1,&instances);glBindBuffer(GL_ARRAY_BUFFER,instances);float instance[]={.25f,24,.5f,1,.73f};glBufferData(GL_ARRAY_BUFFER,sizeof(instance),instance,GL_DYNAMIC_DRAW);glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(instance),nullptr);glVertexAttribDivisor(4,1);glEnableVertexAttribArray(5);glVertexAttribPointer(5,1,GL_FLOAT,GL_FALSE,sizeof(instance),reinterpret_cast<void*>(4*sizeof(float)));glVertexAttribDivisor(5,1);
    glGenBuffers(1,&tf);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,tf);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,5*sizeof(Output),nullptr,GL_DYNAMIC_READ);
    defaults(color);defaults(depth);double peak=0;std::size_t vertices=0;
    for(float height:{1.1f,2.2f})for(float scale:{.15f,1.f,4.f}) {
        std::array<Vertex,5> data={};const float ys[]={0,height*.5f,height,height*.5f-.005f,height*.5f+.005f};
        for(unsigned k=0;k<5;k++){data[k]={{1.3f,ys[k],-.7f},{1,0,0},{.5f,.5f},{0,1,0,1}};}
        glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(data),data.data());instance[3]=scale;glBindBuffer(GL_ARRAY_BUFFER,instances);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(instance),instance);
        glUseProgram(color);f(color,"uTreeHeight",height);f(color,"uWind",0);const auto still=capture(color,tf);f(color,"uWind",.6f);
        for(unsigned tick=0;tick<128;tick++)for(float weather:{0.f,3.f}) {
            const float time=tick*.5f;glUseProgram(color);f(color,"uTime",time);glUniform2f(glGetUniformLocation(color,"uWeatherWind"),weather*.8f,weather*.6f);const auto a=capture(color,tf);
            assert(!std::memcmp(a[0].pos,still[0].pos,3*sizeof(float)));
            for(unsigned j=0;j<5;j++){const double dx=a[j].pos[0]-still[j].pos[0],dz=a[j].pos[2]-still[j].pos[2];peak=std::max(peak,std::hypot(dx,dz));assert(std::hypot(dx,dz)<.35001&&a[j].pos[1]==still[j].pos[1]);assert(std::abs(length(a[j].normal)-1)<.00001&&std::abs(length(a[j].tangent)-1)<.00001&&std::abs(dot(a[j].normal,a[j].tangent))<.00001);vertices++;}
            float derivative[3];for(unsigned k=0;k<3;k++)derivative[k]=(a[4].pos[k]-a[3].pos[k])/(.01f*scale);
            const double dnorm=length(derivative);for(auto &q:derivative)q=float(q/dnorm);
            assert(dot(derivative,a[1].tangent)>.9999&&std::abs(dot(derivative,a[1].normal))<.004);
            glUseProgram(depth);f(depth,"uTreeHeight",height);f(depth,"uTime",time);glUniform2f(glGetUniformLocation(depth,"uWeatherWind"),weather*.8f,weather*.6f);const auto b=capture(depth,tf);assert(!std::memcmp(a.data(),b.data(),sizeof(a)));
        }
        glUseProgram(color);const auto a=capture(color,tf),b=capture(color,tf);assert(!std::memcmp(a.data(),b.data(),sizeof(a)));
    }
    assert(peak>.10);
    // Contact must move short foliage tops without floating roots, affecting
    // plants on a different floor, or separating the shadow from the mesh.
    std::array<Vertex,5> grass_data={};const float grass_y[]={0,.2f,.6f,.195f,.205f};
    for(unsigned k=0;k<5;k++)grass_data[k]={{.18f,grass_y[k],0},{1,0,0},{.5f,.5f},{0,1,0,1}};
    glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(grass_data),grass_data.data());
    instance[3]=1;glBindBuffer(GL_ARRAY_BUFFER,instances);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(instance),instance);
    for(auto p:{color,depth}){glUseProgram(p);i(p,"uVegetation",3);f(p,"uTreeHeight",.8f);f(p,"uWind",0);glUniform2f(glGetUniformLocation(p,"uWeatherWind"),0,0);}
    const auto grass_still=capture(color,tf);
    for(auto p:{color,depth}){glUseProgram(p);glUniform4f(glGetUniformLocation(p,"uGrassInteractor"),-1310.25f,-10206.5f,24,.95f);}
    const auto contact=capture(color,tf),contact_shadow=capture(depth,tf),contact_repeat=capture(color,tf);
    assert(!std::memcmp(contact.data(),contact_shadow.data(),sizeof(contact))&&!std::memcmp(contact.data(),contact_repeat.data(),sizeof(contact)));
    assert(!std::memcmp(contact[0].pos,grass_still[0].pos,3*sizeof(float)));
    double contact_peak=0;
    for(unsigned j=0;j<5;j++){const double d=std::hypot(contact[j].pos[0]-grass_still[j].pos[0],contact[j].pos[2]-grass_still[j].pos[2]);contact_peak=std::max(contact_peak,d);assert(d<=.18001&&contact[j].pos[1]==grass_still[j].pos[1]);assert(std::abs(length(contact[j].normal)-1)<.00001&&std::abs(length(contact[j].tangent)-1)<.00001);}
    assert(contact_peak>.1);
    glUseProgram(color);glUniform4f(glGetUniformLocation(color,"uGrassInteractor"),-1310.25f,-10206.5f,26,.95f);
    const auto other_floor=capture(color,tf);assert(!std::memcmp(other_floor.data(),grass_still.data(),sizeof(other_floor)));
    for(auto p:{color,depth}){glUseProgram(p);glUniform4f(glGetUniformLocation(p,"uGrassInteractor"),0,0,0,0);}
    std::printf("PASS grass contact: %.6fm bounded displacement, roots/Y fixed, other-floor rejection, exact shadow/repeat\n",contact_peak);
    glUseProgram(color);const auto origin_before=capture(color,tf);
    instance[0]-=128;instance[2]-=128;glBindBuffer(GL_ARRAY_BUFFER,instances);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(instance),instance);
    glUniform2f(glGetUniformLocation(color,"uWorldAnchor"),-1182.5f,-10079.f);v(color,"uOffset",128,-24,128);
    const auto origin_after=capture(color,tf);
    for(unsigned j=0;j<5;j++) {
        assert(!std::memcmp(origin_before[j].normal,origin_after[j].normal,9*sizeof(float)));
        for(unsigned k=0;k<3;k++){assert(std::abs(origin_before[j].relative[k]-origin_after[j].relative[k])<.00003);assert(origin_before[j].root[k]==origin_after[j].root[k]);}
    }
    // Actual color and shadow fragment programs use exactly complementary
    // stationary near/mid masks. Crop final fade is independent of grass caps.
    GLuint field=program(flat_vs,fs,false),field_shadow=program(flat_vs,shadow,false),fbo=0,tex[2]={};defaults(field);defaults(field_shadow);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(2,tex);
    for(unsigned k=0;k<2;k++){glBindTexture(GL_TEXTURE_2D,tex[k]);glTexImage2D(GL_TEXTURE_2D,0,k?GL_DEPTH_COMPONENT32F:GL_RGBA32F,64,64,0,k?GL_DEPTH_COMPONENT:GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glFramebufferTexture2D(GL_FRAMEBUFFER,k?GL_DEPTH_ATTACHMENT:GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex[k],0);}assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,64,64);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    for(float d:{55.99f,56.f,56.01f,59.99f,60.f,60.01f,63.99f,64.f,64.01f}){const auto near=mask(field,d,1),mid=mask(field,d,2),caster=mask(field_shadow,d,1,true);for(unsigned j=0;j<near.size();j++){assert(near[j]!=mid[j]);assert(near[j]==caster[j]);}}
    for(auto sample:{std::pair<float,float>{90,1},{280,1},{300,.5f},{320,0}}){const auto m=mask(field,sample.first,0);const float fraction=float(std::count(m.begin(),m.end(),true))/m.size();assert(std::abs(fraction-sample.second)<.015f);}
    const auto held=mask(field,300,0);
    for(unsigned phase=0;phase<16;phase++){glUseProgram(field);f(field,"uTemporalPhase",(phase%8)/8.f);f(field,"uTime",phase*.37f);assert(mask(field,300,0)==held);}
    glUseProgram(field);i(field,"uVegetation",3);const auto fern=mask(field,320,0);assert(std::count(fern.begin(),fern.end(),true)==4096);i(field,"uVegetation",5);const auto grass=mask(field,90,0);assert(std::count(grass.begin(),grass.end(),true)==0);
    assert(glGetError()==GL_NO_ERROR);for(GLuint p:{color,depth,field,field_shadow})glDeleteProgram(p);GLuint buffers[]={vbo,instances,tf};glDeleteBuffers(3,buffers);glDeleteVertexArrays(1,&vao);glDeleteTextures(2,tex);glDeleteFramebuffers(1,&fbo);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS actual crop shaders: %zu windy vertices, maxdisplacement%.6fm<.35, fixed roots/Y, analytical finite-difference normal/tangent, exact color-shadow/repeated poses,128m rebase stable, complementary56–64m masks, crop280–320m stationary16phase fade, unchanged fern/grass endpoints\n",vertices,peak);
}
