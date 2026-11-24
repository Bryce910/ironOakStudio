// Actual production water sampler correctness; software/offscreen by default.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_pool_reflection_scissor.h"
#include <array>
#include <cassert>
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
GLuint compile(GLenum type,const std::string &source) {
    GLuint shader=glCreateShader(type);const char *text=source.c_str();glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096];glGetShaderInfoLog(shader,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);return shader;
}
GLuint program(const std::string &vertex,const std::string &fragment) {
    const GLuint v=compile(GL_VERTEX_SHADER,vertex),f=compile(GL_FRAGMENT_SHADER,fragment),p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);
    glDeleteShader(v);glDeleteShader(f);return p;
}
std::string file(const char *name){std::ifstream input(name);assert(input);std::stringstream text;text<<input.rdbuf();return text.str();}
struct Target {
    GLuint texture=0,fbo=0;int width,height;
    Target(int w,int h):width(w),height(h){
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    }
    void bind(){glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,width,height);}
    ~Target(){glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);}
};
}
int main(int argc,char **argv) {
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Pool scissor correctness",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    const auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Pool scissor renderer: %s\n",glGetString(GL_RENDERER));
    {
        GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        const std::string fullscreen=R"GLSL(#version 330 core
out vec2 uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.-1.,0,1);})GLSL";
        const GLuint scene=program(fullscreen,R"GLSL(#version 330 core
in vec2 uv;out vec4 color;uniform bool sky;
void main(){if(sky){color=vec4(.02+.03*uv.x,.06,.1,1);return;}
vec2 p=floor(gl_FragCoord.xy);float grid=mod(p.x+3.*p.y,11.)/11.;color=vec4(.3+grid*2.,.15+uv.y,.2+uv.x*.4,1);})GLSL");
        std::string vertex=file("shaders/world_water.vs.glsl");
        const std::string old_position="gl_Position=uVP*vec4(vRelative,1);";
        const auto position=vertex.find(old_position);assert(position!=std::string::npos);
        // Rasterize the entire water source rectangle into the test target;
        // retain the exact production positions and fragment/reflection code.
        vertex.replace(position,old_position.size(),"gl_Position=vec4(q[gl_VertexID],0,1);");
        const GLuint water=program(vertex,file("shaders/world_water.fs.glsl"));
        Target reflection(256,192),result(64,64),neutral(1,1);
        neutral.bind();glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT);
        auto uniform=[&](const char *name){return glGetUniformLocation(water,name);};
        glUseProgram(water);
        glUniform1i(uniform("uLocalPool"),1);glUniform1i(uniform("uHasReflection"),1);glUniform1i(uniform("uReflection"),3);
        glUniform1i(uniform("uClimate"),2);glUniform1i(uniform("uShadow"),4);glUniform1i(uniform("uOpaqueColor"),0);glUniform1i(uniform("uOpaqueDepth"),1);
        glUniform2f(uniform("uWorldSize"),100800,56708.8f);glUniform2f(uniform("uResolution"),64,64);
        glUniform3f(uniform("uSkyLight"),.2f,.25f,.3f);glUniform3f(uniform("uSkyZenith"),.08f,.09f,.10f);
        glUniform3f(uniform("uFog"),.186f,.224f,.215f);glUniform3f(uniform("uSun"),0,1,0);glUniform1f(uniform("uCloud"),.96f);
        glUniform1f(uniform("uNear"),.4f);glUniform1f(uniform("uFar"),15000);
        for(unsigned unit=0;unit<5;unit++){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,unit==3?reflection.texture:neutral.texture);}
        WorldPoolReflectionScissorInput in;
        in.width=reflection.width;in.height=reflection.height;in.pool_bounds[0]=-26715;in.pool_bounds[1]=9615.5f;
        in.pool_bounds[2]=1.f/11;in.pool_bounds[3]=1.f/16;in.origin[0]=-26723;in.origin[1]=9580.5f;
        in.water_level=134;in.offset_y=-136;in.reflection_shift_y=4;
        in.vp[0]=.024f;in.vp[9]=.015f;in.vp[6]=.005f;in.vp[11]=.004f;in.vp[15]=1;
        auto render=[&](bool scissor,bool broken,float time,float rain,float wind){
            reflection.bind();glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
            glUseProgram(scene);glUniform1i(glGetUniformLocation(scene,"sky"),1);glDrawArrays(GL_TRIANGLES,0,3); // Sky stays full.
            const auto box=world_pool_reflection_scissor(in);assert(box.restricted);
            if(scissor){glEnable(GL_SCISSOR_TEST);glScissor(box.x,box.y,broken?1:box.width,broken?1:box.height);}
            glUniform1i(glGetUniformLocation(scene,"sky"),0);glDrawArrays(GL_TRIANGLES,0,3);
            glDisable(GL_SCISSOR_TEST);result.bind();glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glUseProgram(water);
            glUniform4fv(uniform("uPoolBounds"),1,in.pool_bounds);glUniform2fv(uniform("uOrigin"),1,in.origin);
            glUniform3f(uniform("uOffset"),0,in.offset_y,0);glUniform1f(uniform("uWaterLevel"),in.water_level);
            glUniform1f(uniform("uEyeHeight"),-in.offset_y);glUniformMatrix4fv(uniform("uReflectionVP"),1,GL_FALSE,in.vp);
            glUniform3f(uniform("uReflectionShift"),0,in.reflection_shift_y,0);glUniform1f(uniform("uTime"),time);
            glUniform1f(uniform("uRainIntensity"),rain);glUniform2f(uniform("uWeatherWind"),wind,wind*.3f);
            glDrawArrays(GL_TRIANGLES,0,6);std::vector<Pixel> pixels(64*64);glReadPixels(0,0,64,64,GL_RGBA,GL_FLOAT,pixels.data());
            for(const auto &pixel:pixels)for(float channel:pixel)assert(std::isfinite(channel));
            return pixels;
        };
        unsigned comparisons=0;double minimum_saved=1;
        for(unsigned direction=0;direction<4;direction++) {
            in.vp[12]=float(direction)*.07f-.12f;in.vp[13]=float(direction)*-.09f;
            const auto box=world_pool_reflection_scissor(in);
            minimum_saved=std::min(minimum_saved,1.-double(box.width)*box.height/(in.width*in.height));
            for(const auto weather:{std::array<float,3>{0,0,0},{.4f,1,0},{7.1f,1,3}}) {
                const auto full=render(false,false,weather[0],weather[1],weather[2]);
                const auto cropped=render(true,false,weather[0],weather[1],weather[2]);
                assert(std::memcmp(full.data(),cropped.data(),full.size()*sizeof(Pixel))==0);comparisons++;
                if(!direction&&weather[1]==0) {
                    const auto broken=render(true,true,weather[0],weather[1],weather[2]);
                    assert(std::memcmp(full.data(),broken.data(),full.size()*sizeof(Pixel))!=0);
                }
            }
        }
        // A deliberately large source rectangle drives fwidth(world) high
        // enough to hit the production kernel's full five-texel clamp. Its
        // projection stays small, so a missing blur margin remains detectable.
        in.pool_bounds[2]=1.f/4096;in.pool_bounds[3]=1.f/6000;
        in.vp[0]=.000075f;in.vp[9]=.000055f;in.vp[11]=.0000005f;in.vp[12]=in.vp[13]=0;
        for(const auto weather:{std::array<float,3>{0,0,0},{7.1f,1,3}}) {
            const auto full=render(false,false,weather[0],weather[1],weather[2]);
            const auto cropped=render(true,false,weather[0],weather[1],weather[2]);
            assert(std::memcmp(full.data(),cropped.data(),full.size()*sizeof(Pixel))==0);comparisons++;
        }
        assert(minimum_saved>.5);assert(!glIsEnabled(GL_SCISSOR_TEST));
        std::printf("PASS pool reflection scissor GL: %u full HDR water frames bitexact against full reflection, calm/rain/wind/blur/edge cases; wrong-scissor negative control active; geometry coverage saved at least%.1f%%; sky stays full and scissor disabled after reflection\n",comparisons,minimum_saved*100);
        glDeleteProgram(scene);glDeleteProgram(water);glDeleteVertexArrays(1,&vao);
    }
    assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
