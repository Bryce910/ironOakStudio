// Isolate the actual production PCF function; no duplicated visibility math.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
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
constexpr unsigned side=24;
using Pixel=std::array<float,4>;
std::string file(const char *path){std::ifstream input(path);assert(input);std::stringstream text;text<<input.rdbuf();return text.str();}
GLuint compile(GLenum type,const std::string &source) {
    GLuint result=glCreateShader(type);const char *text=source.c_str();glShaderSource(result,1,&text,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[8192];glGetShaderInfoLog(result,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);return result;
}
GLuint program(std::string fragment) {
    const auto main=fragment.find("void main()");assert(main!=std::string::npos);
    fragment.erase(main);fragment+="uniform vec3 tEndpointNormal;\nvoid main(){float visible=visibility(tEndpointNormal);FragColor=vec4(visible,visible,visible,1);IndirectColor=vec4(0);}\n";
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
    SDL_Window *window=SDL_CreateWindow("Shadow endpoint correctness",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    const auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Shadow endpoint renderer: %s\n",glGetString(GL_RENDERER));
    GLuint vao,fbo,target,shadow;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,side,side,0,GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glGenTextures(1,&shadow);glBindTexture(GL_TEXTURE_2D,shadow);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glViewport(0,0,side,side);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    const std::string guard="    if(length(vRelative.xz)>=160.) return 1.;\n";
    unsigned comparisons=0;float minimum_negative=1;
    for(const auto path:{"shaders/world_foliage.fs.glsl","shaders/world_terrain.fs.glsl"}) {
        const std::string actual_source=file(path);std::string legacy_source=actual_source,wrong_source=actual_source;
        const auto at=actual_source.find(guard);assert(at!=std::string::npos&&actual_source.find(guard,at+1)==std::string::npos);
        const bool foliage=std::string(path).find("foliage")!=std::string::npos;
        legacy_source.replace(at,guard.size(),foliage?"    if(length(vRelative.xz)>195.) return 1.;\n":"");
        wrong_source.replace(at,guard.size(),"    if(length(vRelative.xz)>=150.) return 1.;\n");
        const GLuint actual=program(actual_source),legacy=program(legacy_source),wrong=program(wrong_source);
        auto render=[&](GLuint p,float distance,float angle,float span,bool soft,unsigned normal) {
            glUseProgram(p);glUniform1i(glGetUniformLocation(p,"uShadow"),0);glUniform1i(glGetUniformLocation(p,"uSoftShadows"),soft);
            glUniform3f(glGetUniformLocation(p,"uSun"),.3f,.8f,.5f);
            glUniform3f(glGetUniformLocation(p,"tEndpointNormal"),normal?.6f:0,normal?.8f:1,0);
            glUniform3f(glGetUniformLocation(p,"tRelative"),distance*std::cos(angle),-2,distance*std::sin(angle));
            glUniform1f(glGetUniformLocation(p,"tSpan"),span);
            // Finite orthographic shadow input keeps the complete sampled
            // range inside the light map and exercises noncentral PCF weights.
            const float light[16]={.001f,0,0,0,0,0,.02f,0,0,.001f,0,0,.017f,-.023f,0,1};
            glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,light);
            glDrawArrays(GL_TRIANGLES,0,3);std::vector<Pixel> pixels(side*side);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,pixels.data());
            for(const auto &pixel:pixels)for(float channel:pixel)assert(std::isfinite(channel));
            return pixels;
        };
        for(unsigned pattern=0;pattern<4;pattern++) {
            std::array<float,81> depths={};
            for(unsigned y=0;y<9;y++)for(unsigned x=0;x<9;x++)depths[y*9+x]=pattern==0?0:pattern==1?1:pattern==2?float((x+y)&1):float(x+y)/16;
            glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,9,9,0,GL_RED,GL_FLOAT,depths.data());
            for(bool soft:{false,true})for(unsigned normal=0;normal<2;normal++)
                for(float distance:{0.f,62.f,124.99f,125.f,140.f,155.f,159.9f,std::nextafter(160.f,0.f),160.f,
                                    std::nextafter(160.f,INFINITY),160.1f,175.f,195.f,250.f})
                    for(float span:{0.f,6.f}) {
                        const float angle=normal?.73f:0;
                        const auto a=render(actual,distance,angle,span,soft,normal),b=render(legacy,distance,angle,span,soft,normal);
                        assert(std::memcmp(a.data(),b.data(),a.size()*sizeof(Pixel))==0);comparisons++;
                        if(!span&&distance>=160.1f)for(const auto &pixel:a)assert(pixel[0]==1&&pixel[1]==1&&pixel[2]==1);
                    }
            if(!pattern) {
                const auto real=render(actual,155,0,0,true,0),bad=render(wrong,155,0,0,true,0);
                const float delta=std::abs(real[0][0]-bad[0][0]);assert(delta>.04f);minimum_negative=std::min(minimum_negative,delta);
                // Below the fade endpoint, replacing shadow data must still
                // change the real nine-tap result, proving active PCF remains.
                const auto shaded=render(actual,140,0,0,true,0);depths.fill(1);
                glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,9,9,0,GL_RED,GL_FLOAT,depths.data());
                const auto lit=render(actual,140,0,0,true,0);assert(lit[0][0]>shaded[0][0]+.5f);
            }
        }
        glDeleteProgram(actual);glDeleteProgram(legacy);glDeleteProgram(wrong);
    }
    assert(glGetError()==GL_NO_ERROR);glDeleteTextures(1,&shadow);glDeleteTextures(1,&target);glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS shadow endpoint GL: %u complete RGBA32F terrain/foliage frames bitexact to legacy across125--160 fade, endpoints, angles, source patterns and both9/4-tap modes; beyond160 exactly1; premature150m negative delta%.6f and active9-tap shadow control PASS\n",comparisons,minimum_negative);
}
