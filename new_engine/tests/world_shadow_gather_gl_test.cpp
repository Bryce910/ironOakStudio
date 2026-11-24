// Actual production PCF differential, scalar reference and forced GLSL330 fallback.
// Isolate the actual production PCF function; no duplicated visibility math.
#include "world_shader_lossless_test_sources.h"
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
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
namespace {
constexpr unsigned side=12;
using Pixel=std::array<float,4>;
std::string file(const char *path){std::ifstream input(path);assert(input);std::stringstream text;text<<input.rdbuf();return text.str();}
GLuint compile(GLenum type,const std::string &source) {
    GLuint result=glCreateShader(type);const char *text=source.c_str();glShaderSource(result,1,&text,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[8192];glGetShaderInfoLog(result,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);return result;
}
GLuint program(std::string fragment) {
    const auto main=fragment.find("void main()");assert(main!=std::string::npos);
    fragment.erase(main);fragment+=R"GLSL(
uniform vec3 tEndpointNormal;uniform bool tShowGather;
void main(){
 float visible=visibility(tEndpointNormal);FragColor=vec4(visible,visible,visible,1);IndirectColor=vec4(0);
 if(tShowGather){
#ifdef VALWOOD_TEXTURE_GATHER_ACTIVE
  FragColor=vec4(1,0,0,1);
#else
  FragColor=vec4(0,0,0,1);
#endif
 }
})GLSL";
    const std::string vertex=R"GLSL(#version 330 core
uniform vec3 tRelative;uniform float tSpan;out vec3 vRelative;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);
vRelative=tRelative+vec3((p.x-.5)*tSpan,0,(p.y-.5)*tSpan);})GLSL";
    const GLuint v=compile(GL_VERTEX_SHADER,vertex),f=compile(GL_FRAGMENT_SHADER,fragment),p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[8192];glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);
    glDeleteShader(v);glDeleteShader(f);return p;
}
}
int main(int argc,char **argv) {
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Gather strict correctness",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    const auto context=SDL_GL_CreateContext(window);assert(context);
    const bool supported=SDL_GL_ExtensionSupported("GL_ARB_texture_gather");
    std::printf("Gather renderer: %s, extension%d\n",glGetString(GL_RENDERER),supported);
    GLuint vao,fbo,target,shadow;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,side,side,0,GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glGenTextures(1,&shadow);glBindTexture(GL_TEXTURE_2D,shadow);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    const float border[4]={1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
    glViewport(0,0,side,side);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    unsigned cases=0;float negative=0;
    for(const auto name:{"world_foliage","world_terrain"}){
        const auto runtime_path=std::string("shaders/")+name+".fs.glsl";
        const auto candidate_source=file(runtime_path.c_str()),old_source=world_shader_lossless_test::without_gather(candidate_source);
        auto disabled_source=candidate_source;const auto version=disabled_source.find('\n');assert(version!=std::string::npos);
        disabled_source.insert(version+1,"#define VALWOOD_QA_DISABLE_TEXTURE_GATHER 1\n");
        auto wrong_source=candidate_source;const auto wrong_at=wrong_source.find("vec3(a.w,a.z,b.z)");assert(wrong_at!=std::string::npos);
        wrong_source.replace(wrong_at,std::strlen("vec3(a.w,a.z,b.z)"),"vec3(a.x,a.z,b.z)");
        const GLuint actual=program(candidate_source),legacy=program(old_source),disabled=program(disabled_source),wrong=program(wrong_source);
        auto render=[&](GLuint p,int texture_side,int cx,int cy,float sub,float distance,bool soft,bool marker=false,bool gather_enabled=true){
            glUseProgram(p);glUniform1i(glGetUniformLocation(p,"uShadow"),0);glUniform1i(glGetUniformLocation(p,"uSoftShadows"),soft);
            glUniform1i(glGetUniformLocation(p,"uShadowGather"),gather_enabled);glUniform1i(glGetUniformLocation(p,"tShowGather"),marker);glUniform3f(glGetUniformLocation(p,"uSun"),.3f,.8f,.5f);
            glUniform3f(glGetUniformLocation(p,"tEndpointNormal"),.6f,.8f,0);glUniform3f(glGetUniformLocation(p,"tRelative"),distance,-2,0);
            glUniform1f(glGetUniformLocation(p,"tSpan"),.03f);
            // Each raster spans about0.6texel, crossing interpolation and PCF
            // weight boundaries without leaving the configured light-map area.
            const float gain=20.f/texture_side,tx=2.f*(cx+sub)/texture_side-1-gain*distance,ty=2.f*(cy+sub)/texture_side-1;
            const float light[16]={gain,0,0,0,0,0,0,0,0,gain,0,0,tx,ty,0,1};
            glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,light);
            glDrawArrays(GL_TRIANGLES,0,3);std::vector<Pixel> pixels(side*side);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,pixels.data());
            for(const auto &pixel:pixels)for(float channel:pixel)assert(std::isfinite(channel));
            assert(glGetError()==GL_NO_ERROR);return pixels;
        };
        for(int size:{3,9,17,257,2048,3072})for(GLint format:{GLint(GL_R32F),GLint(GL_DEPTH_COMPONENT24)})for(unsigned pattern=0;pattern<2;pattern++){
            std::vector<float> data(std::size_t(size)*size);
            for(int y=0;y<size;y++)for(int x=0;x<size;x++)data[std::size_t(y)*size+x]=pattern?float((x*13+y*7)&15)/15.f:float((x+y)&1);
            glBindTexture(GL_TEXTURE_2D,shadow);glTexImage2D(GL_TEXTURE_2D,0,format,size,size,0,format==GL_R32F?GL_RED:GL_DEPTH_COMPONENT,GL_FLOAT,data.data());
            const std::array<std::array<int,2>,9> positions={{{0,0},{1,1},{size-2,size-2},{size-1,size-1},{size/2,size/2},{0,size/2},{size-1,size/2},{size/2,0},{size/2,size-1}}};
            if(!pattern&&size==9&&format==GL_R32F){const auto marker=render(actual,size,4,4,.5f,62,true,true);assert(marker[0][0]==float(supported));const auto off=render(disabled,size,4,4,.5f,62,true,true);assert(off[0][0]==0);}
            for(const auto &xy:positions)for(float sub:{.0001f,.4999f,.9999f})for(float distance:{62.f,155.f,160.f})for(bool soft:{false,true}){
                const auto a=render(actual,size,xy[0],xy[1],sub,distance,soft),b=render(legacy,size,xy[0],xy[1],sub,distance,soft),c=render(disabled,size,xy[0],xy[1],sub,distance,soft),d=render(actual,size,xy[0],xy[1],sub,distance,soft,false,false);
                assert(std::memcmp(a.data(),b.data(),a.size()*sizeof(Pixel))==0);assert(std::memcmp(c.data(),b.data(),c.size()*sizeof(Pixel))==0);assert(std::memcmp(d.data(),b.data(),d.size()*sizeof(Pixel))==0);cases++;
                if(supported&&size==9&&distance==62&&soft){const auto bad=render(wrong,size,xy[0],xy[1],sub,distance,soft);for(unsigned k=0;k<a.size();k++)negative=std::max(negative,std::abs(a[k][0]-bad[k][0]));}
            }
        }
        glDeleteProgram(actual);glDeleteProgram(legacy);glDeleteProgram(disabled);glDeleteProgram(wrong);
    }
    if(supported)assert(negative>.01f);
    glDeleteTextures(1,&shadow);glDeleteTextures(1,&target);glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS gather%u strict RGBA32F visibility cases acrossD24/R32F,3/9/17/257/2048/3072px, borders/subtexels/fade/endpoints/bothPCFmodes; forced330fallback/uniformfalse bitexact; activegather%d wrongswizzle delta%.8f\n",cases,supported,negative);
}
