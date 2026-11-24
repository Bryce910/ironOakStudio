// Staged correctness only. No runtime files are changed by this fixture.
// Compile after the coordinator releases its native timing window:
// g++ -std=c++17 -O2 -Wall -Wextra -Werror -I/usr/include/SDL2 -D_REENTRANT
//   /tmp/world_zero_coverage_gl_test.cpp -o /tmp/valwood-zero-coverage-gl-test -lSDL2 -lGL
// Run from engine root, LIBGL_ALWAYS_SOFTWARE=1 for software correctness.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
namespace {
constexpr int side=128;
using Pixel=std::array<float,4>;
struct Frame {std::array<std::vector<Pixel>,2> color;std::vector<float> depth;};
std::string file(const char *path){std::ifstream input(path);assert(input);std::stringstream text;text<<input.rdbuf();return text.str();}
GLuint compile(GLenum type,const std::string &source){
    GLuint s=glCreateShader(type);const char *text=source.c_str();glShaderSource(s,1,&text,nullptr);glCompileShader(s);
    GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(s,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);return s;
}
GLuint program(const std::string &vertex,const std::string &fragment){
    GLuint v=compile(GL_VERTEX_SHADER,vertex),f=compile(GL_FRAGMENT_SHADER,fragment),p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);
    const char *capture="gl_Position";glTransformFeedbackVaryings(p,1,&capture,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[8192];glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);
    glDeleteShader(v);glDeleteShader(f);return p;
}
void integer(GLuint p,const char *n,int v){glUniform1i(glGetUniformLocation(p,n),v);}
void scalar(GLuint p,const char *n,float v){glUniform1f(glGetUniformLocation(p,n),v);}
void vector(GLuint p,const char *n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);}
void defaults(GLuint p){
    glUseProgram(p);const char *two_d[]={"uBase","uNormal","uARM","uEmissive","uShadow","uMacroHeight","uFacadeAux0","uFacadeAux1","uFacadeAux2"};
    const int units[]={0,1,2,3,7,11,8,9,10};for(unsigned k=0;k<9;k++)integer(p,two_d[k],units[k]);
    integer(p,"uLocalProbe0",4);integer(p,"uLocalProbe1",5);integer(p,"uVillageSkyMask",12);integer(p,"uVillageSkyDirectional",13);integer(p,"uVillageSkyUp",14);
    glUniform4f(glGetUniformLocation(p,"uBaseFactor"),.3f,.6f,.2f,1);scalar(p,"uRoughness",.8f);scalar(p,"uMetallic",0);
    vector(p,"uSun",.3f,.8f,.5f);vector(p,"uSunColor",.4f,.4f,.4f);vector(p,"uSkyLight",.35f,.4f,.45f);vector(p,"uGroundLight",.1f,.12f,.08f);
    vector(p,"uAmbient",.3f,.3f,.3f);vector(p,"uFog",.186f,.224f,.215f);scalar(p,"uHaze",.00005f);scalar(p,"uCloud",.96f);
    scalar(p,"uWind",.4f);integer(p,"uWindHistoryEnabled",1);scalar(p,"uWindFrameDelta",1.f/60);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,identity);
}
struct Config{float height,scale,distance,yaw,pitch;int lod;};
void vertices(GLuint buffer,float height){
    const float points[6][2]={{-.4f,0},{.4f,0},{.4f,1},{-.4f,0},{.4f,1},{-.4f,1}};std::array<float,72> data={};
    for(unsigned k=0;k<6;k++){const auto at=k*12;data[at]=points[k][0]*height;data[at+1]=points[k][1]*height;data[at+5]=1;data[at+6]=points[k][0]+.5f;data[at+7]=points[k][1];data[at+8]=1;data[at+11]=1;}
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(data),data.data(),GL_DYNAMIC_DRAW);
}
void setup(GLuint p,const Config &c,float projected,int vegetation,bool wet,bool color_cull=true){
    glUseProgram(p);integer(p,"uColorCoverageCull",color_cull);const float h=std::max(c.height,1.f)*c.scale,cy=std::cos(c.yaw),sy=std::sin(c.yaw),cp=std::cos(c.pitch),sp=std::sin(c.pitch);
    const float forward[3]={sy*cp,sp,-cy*cp},right[3]={cy,0,sy},up[3]={-sy*sp,cp,cy*sp};
    const float root[3]={12,80,26},eye[3]={root[0]-forward[0]*c.distance,root[1]+h*.5f-forward[1]*c.distance,root[2]-forward[2]*c.distance};
    const float offset[3]={-eye[0],-eye[1],-eye[2]};vector(p,"uOffset",offset[0],offset[1],offset[2]);
    // Magnify geometry while keeping the production projected-size uniform
    // independently controlled: even the tiny positive3.095 boundary has many
    // candidate pixels. This makes a premature3.1 clip a strong negative test.
    const float half=std::max(h*.65f,.1f);float vp[16]={};vp[15]=1;
    for(unsigned k=0;k<3;k++){vp[k*4]=right[k]/half;vp[k*4+1]=up[k]/half;}
    glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,vp);
    glVertexAttrib4f(4,root[0],root[1],root[2],c.scale);glVertexAttrib1f(5,.7f);glVertexAttrib1f(6,1);glVertexAttrib1f(7,0);
    scalar(p,"uTreeHeight",c.height);integer(p,"uVegetation",vegetation);integer(p,"uLodLevel",c.lod);scalar(p,"uTime",wet?7.1f:0);
    glUniform2f(glGetUniformLocation(p,"uWeatherWind"),wet?1.1f:0,wet?.38f:0);scalar(p,"uWetness",wet?.5f:0);
    // Same float root and centre arithmetic as the linked VS/FS, solely to
    // select a test input. Legacy production FS remains the output authority.
    const float rx=root[0]+offset[0],ry=(root[1]+offset[1])+h*.5f,rz=root[2]+offset[2];
    const float distance=std::sqrt(rx*rx+ry*ry+rz*rz);scalar(p,"uPixelScale",projected*std::max(distance,.1f)/h);
}
Frame frame(GLuint p){
    glUseProgram(p);const float clear0[4]={.013f,.02f,.03f,.04f},clear1[4]={.08f,.07f,.06f,.05f};glClearBufferfv(GL_COLOR,0,clear0);glClearBufferfv(GL_COLOR,1,clear1);
    glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,6);Frame f;
    for(unsigned a=0;a<2;a++){f.color[a].resize(side*side);glReadBuffer(GL_COLOR_ATTACHMENT0+a);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,f.color[a].data());}
    f.depth.resize(side*side);glReadPixels(0,0,side,side,GL_DEPTH_COMPONENT,GL_FLOAT,f.depth.data());
    for(const auto &attachment:f.color)for(const auto &pixel:attachment)for(float value:pixel)assert(std::isfinite(value));
    for(float value:f.depth)assert(std::isfinite(value));
    assert(glGetError()==GL_NO_ERROR);return f;
}
bool same(const Frame &a,const Frame &b){return a.color==b.color&&a.depth==b.depth;}
std::size_t occupied(const Frame &f){return std::count_if(f.depth.begin(),f.depth.end(),[](float value){return value<.999f;});}
}
int main(int argc,char **argv){
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Zero coverage correctness",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);const auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Zero coverage renderer: %s\n",glGetString(GL_RENDERER));
    std::string updated=file("shaders/world_foliage.vs.glsl");const std::string begin="    // Begin zero-coverage card guard.\n",end="    // End zero-coverage card guard.\n";
    if(updated.find(begin)==std::string::npos){updated=file("/tmp/valwood-zero-coverage-candidate.vs.glsl");std::puts("Using staged VS candidate; runtime source was not modified");}
    const auto a=updated.find(begin),b=updated.find(end);assert(a!=std::string::npos&&b!=std::string::npos&&b>a);
    std::string legacy=updated;legacy.erase(a,b+end.size()-a);std::string wrong=updated;const auto threshold=wrong.find("projected<2.99");assert(threshold!=std::string::npos);wrong.replace(threshold,std::strlen("projected<2.99"),"projected<3.1");
    const auto fs=file("shaders/world_foliage.fs.glsl");const GLuint current=program(updated,fs),old=program(legacy,fs),bad=program(wrong,fs);
    GLint default_cull=-1;glGetUniformiv(current,glGetUniformLocation(current,"uColorCoverageCull"),&default_cull);assert(default_cull==0);
    for(GLuint p:{current,old,bad})defaults(p);
    GLuint vao,vbo,fbo;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);vertices(vbo,25);
    for(const auto spec:{std::array<unsigned,3>{0,3,0},{1,3,3},{2,2,6},{3,4,8}}){glEnableVertexAttribArray(spec[0]);glVertexAttribPointer(spec[0],spec[1],GL_FLOAT,GL_FALSE,12*sizeof(float),(void*)(std::uintptr_t(spec[2])*sizeof(float)));}
    glVertexAttrib3f(8,0,0,0);glVertexAttrib3f(9,0,0,1);glVertexAttrib4f(10,1,0,0,1);glVertexAttrib1f(11,0);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);std::array<GLuint,3> outputs={};glGenTextures(3,outputs.data());
    for(unsigned k=0;k<3;k++){glBindTexture(GL_TEXTURE_2D,outputs[k]);glTexImage2D(GL_TEXTURE_2D,0,k==2?GL_DEPTH_COMPONENT32F:GL_RGBA32F,side,side,0,k==2?GL_DEPTH_COMPONENT:GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glFramebufferTexture2D(GL_FRAMEBUFFER,k==2?GL_DEPTH_ATTACHMENT:GL_COLOR_ATTACHMENT0+k,GL_TEXTURE_2D,outputs[k],0);}
    const GLenum draw[2]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,draw);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    std::array<GLuint,4> maps={};glGenTextures(4,maps.data());const Pixel white={1,1,1,1};
    glBindTexture(GL_TEXTURE_2D,maps[0]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,white.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glBindTexture(GL_TEXTURE_CUBE_MAP,maps[1]);for(unsigned face=0;face<6;face++)glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,white.data());glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    const std::array<Pixel,4> layers={white,white,white,white};glBindTexture(GL_TEXTURE_2D_ARRAY,maps[2]);glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,1,1,4,0,GL_RGBA,GL_FLOAT,layers.data());glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glBindTexture(GL_TEXTURE_3D,maps[3]);glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,1,1,1,0,GL_RGBA,GL_FLOAT,white.data());glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    for(unsigned unit=0;unit<16;unit++){glActiveTexture(GL_TEXTURE0+unit);if(unit==4||unit==5)glBindTexture(GL_TEXTURE_CUBE_MAP,maps[1]);else if(unit==12)glBindTexture(GL_TEXTURE_2D_ARRAY,maps[2]);else if(unit==13||unit==14)glBindTexture(GL_TEXTURE_3D,maps[3]);else glBindTexture(GL_TEXTURE_2D,maps[0]);}
    glViewport(0,0,side,side);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDepthMask(GL_TRUE);
    const Config configs[]={{3.2f,.35f,235,0,0,2},{10,1,800,.7f,-.15f,-1},{25,1.7f,3000,-.8f,.2f,3},{25,.5f,500,.3f,0,2},{10,1.7f,1800,-.2f,.1f,3},{3.2f,1,350,1.4f,-.1f,-1}};
    unsigned comparisons=0,zero_cases=0,positive_cases=0;
    for(const auto &c:configs){vertices(vbo,c.height);for(bool wet:{false,true})for(float projected:{0.f,2.f,2.989f,2.99f,2.9999f,3.f,3.0001f,3.095f,3.5f,6.9999f,7.f,7.0001f,12.f}){
        setup(current,c,projected,4,wet);const auto actual=frame(current);setup(old,c,projected,4,wet);const auto reference=frame(old);assert(same(actual,reference));comparisons++;
        if(projected<2.99f){assert(!occupied(reference));zero_cases++;}if(occupied(reference))positive_cases++;
    }}
    const Config negative={10,1,800,0,0,-1};vertices(vbo,negative.height);setup(current,negative,3.095f,4,false);const auto real=frame(current);setup(bad,negative,3.095f,4,false);const auto wrong_frame=frame(bad);assert(occupied(real)>0&&!occupied(wrong_frame)&&!same(real,wrong_frame));
    const Config noncard={10,1,20,0,0,-1};vertices(vbo,noncard.height);for(int kind:{0,1,2,3,5,6}){setup(current,noncard,2,kind,false);const auto actual=frame(current);setup(old,noncard,2,kind,false);const auto reference=frame(old);assert(occupied(reference)>0&&same(actual,reference));comparisons++;}
    // Invalid projected-input assumptions must fail open in VS. Capture the
    // actual clip positions while disabling rasterization, avoiding undefined
    // fragment behavior from NaN/Inf uniforms. Legacy geometry stays authority.
    GLuint feedback;glGenBuffers(1,&feedback);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,24*sizeof(float),nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback);
    auto positions=[&](GLuint p,unsigned invalid,bool color_cull=true){setup(p,negative,2,4,false,color_cull);if(invalid<4)scalar(p,"uPixelScale",invalid==0?0:invalid==1?-1:invalid==2?NAN:INFINITY);else if(invalid<6)scalar(p,"uTreeHeight",invalid==4?NAN:INFINITY);glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_TRIANGLES);glDrawArrays(GL_TRIANGLES,0,6);glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);std::array<float,24> result;glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result.data());return result;};
    for(unsigned invalid=0;invalid<6;invalid++){const auto actual=positions(current,invalid),reference=positions(old,invalid);assert(std::memcmp(actual.data(),reference.data(),sizeof(actual))==0);assert(actual[0]!=2||actual[1]!=2);}
    const auto disabled=positions(current,6,false),legacy_positions=positions(old,6,false),culled=positions(current,6,true);
    assert(std::memcmp(disabled.data(),legacy_positions.data(),sizeof(disabled))==0);
    for(unsigned k=0;k<6;k++)assert(culled[k*4]==2&&culled[k*4+1]==2&&culled[k*4+2]==2&&culled[k*4+3]==1);
    // This same VS is used by real shadow programs. Their defaultfalse flag
    // must preserve every caster even if a future caller sets pixel scale.
    const auto shadow_fs=file("shaders/world_foliage_shadow.fs.glsl");
    const GLuint shadow_current=program(updated,shadow_fs),shadow_old=program(legacy,shadow_fs);
    glGetUniformiv(shadow_current,glGetUniformLocation(shadow_current,"uColorCoverageCull"),&default_cull);assert(default_cull==0);
    defaults(shadow_current);defaults(shadow_old);
    for(bool wet:{false,true}) {
        setup(shadow_current,negative,2,4,wet,false);const auto actual_shadow=frame(shadow_current);
        setup(shadow_old,negative,2,4,wet,false);const auto legacy_shadow=frame(shadow_old);
        assert(occupied(legacy_shadow)>0&&same(actual_shadow,legacy_shadow));
    }
    glDeleteProgram(shadow_current);glDeleteProgram(shadow_old);
    assert(glGetError()==GL_NO_ERROR);glDeleteBuffers(1,&feedback);glDeleteTextures(4,maps.data());glDeleteTextures(3,outputs.data());glDeleteFramebuffers(1,&fbo);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);for(GLuint p:{current,old,bad})glDeleteProgram(p);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS zero-coverage VS: %u complete actual VS/FS color/depth/MRT frames bitexact, %u zero cases/%u positive cases, six invalid-input geometry fail-open controls, defaultfalse/color-only flag and actual shadow color/depth/MRT parity, premature3.1 positive-coverage negative control PASS\n",comparisons,zero_cases,positive_cases);
}
