/* Actual terrain/foliage/water GLSL shadow filters. This is an offscreen
 * correctness fixture, not a native GPU or art-quality benchmark.
 * c++ -std=c++17 -O2 -Wall -Wextra -Werror -I. $(pkg-config --cflags sdl2)
 * tests/world_shadow_filter_gl_test.cpp -o /tmp/valwood-shadow-gl
 * $(pkg-config --libs sdl2) -lGL
 * LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-shadow-gl
 */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
constexpr int side=16;
using Pixel=std::array<float,4>;
const char *vertex=R"GLSL(#version 330 core
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;
out vec2 vUV;
flat out vec3 vRootRelative;
flat out float vRootHeight,vSeed,vScale,vCoveragePhase,vInstanceFade;
out float vFacadePaint;
void main(){
    vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
    gl_Position=vec4(p*2.-1.,0,1);
    vPos=vec3(0,50,0);vNormal=vec3(0,1,0);vRelative=vec3(0,-2,-10);
    vTangent=vec3(1,0,0);vBitangent=vec3(0,0,1);vUV=vec2(.5);
    vRootRelative=vRelative;vRootHeight=50.;vSeed=.5;vScale=1.;vCoveragePhase=0.;
    vInstanceFade=1.;vFacadePaint=0.;
})GLSL";
std::string source(const char *path){std::ifstream file(path);assert(file);std::ostringstream out;out<<file.rdbuf();return out.str();}
GLuint compile(GLenum kind,const std::string &text){
    GLuint s=glCreateShader(kind);const char *p=text.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);
    GLint okay=0;glGetShaderiv(s,GL_COMPILE_STATUS,&okay);
    if(!okay){char log[8192]={};glGetShaderInfoLog(s,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(okay);return s;
}
GLuint program(const char *path,bool water){
    std::string text=source(path);const std::string entry="void main()";auto start=text.find(entry);assert(start!=std::string::npos);
    text.replace(start,entry.size(),"void original_main()");
    text+="\nuniform bool uTestProduction;\nvoid main(){if(uTestProduction){original_main();return;}float v=";
    text+=water?"glint_visibility()":"visibility(vec3(0,1,0))";
    text+=";FragColor=vec4(v,v,v,1);IndirectColor=vec4(0);}\n";
    GLuint vs=compile(GL_VERTEX_SHADER,vertex),fs=compile(GL_FRAGMENT_SHADER,text),p=glCreateProgram();
    glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);assert(okay);
    glDeleteShader(vs);glDeleteShader(fs);return p;
}
void integer(GLuint p,const char *name,int v){glUniform1i(glGetUniformLocation(p,name),v);}
void scalar(GLuint p,const char *name,float v){glUniform1f(glGetUniformLocation(p,name),v);}
void vector(GLuint p,const char *name,float x,float y,float z){glUniform3f(glGetUniformLocation(p,name),x,y,z);}
std::array<Pixel,2> draw(GLuint p){
    glUseProgram(p);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);
    std::array<Pixel,2> result;
    for(int i=0;i<2;i++){glReadBuffer(GL_COLOR_ATTACHMENT0+i);glReadPixels(side/2,side/2,1,1,GL_RGBA,GL_FLOAT,result[i].data());for(float v:result[i])assert(std::isfinite(v));}
    glReadBuffer(GL_COLOR_ATTACHMENT0);assert(glGetError()==GL_NO_ERROR);return result;
}
void position(GLuint p,float x,float y){
    float m[16]={};m[12]=2*(x+.5f)/side-1;m[13]=2*(y+.5f)/side-1;m[15]=1;
    glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,m);
}
void setup(GLuint p,int kind){
    glUseProgram(p);integer(p,"uShadow",7);vector(p,"uSun",0,.8f,-.6f);vector(p,"uSunColor",1,1,1);
    vector(p,"uAmbient",.2f,.25f,.3f);vector(p,"uSkyLight",.2f,.25f,.3f);vector(p,"uGroundLight",.04f,.05f,.06f);
    glUniform2f(glGetUniformLocation(p,"uWorldSize"),100800,56708.8f);
    if(kind==0){
        const char *names[]={"uGrass","uRock","uSand","uDirt","uGrassNormal","uRockNormal","uClimate","uShadow","uGrassARM","uRockARM"};
        for(int i=0;i<10;i++)integer(p,names[i],i);
        integer(p,"uVillagePBR",10);integer(p,"uMacroHeight",11);integer(p,"uLandscape",12);integer(p,"uLitterPBR",13);integer(p,"uVillageGroundMask",14);integer(p,"uStartingMask",15);
        glUniform4f(glGetUniformLocation(p,"uTrailBounds"),1e6f,1e6f,1e6f,1e6f);
    }else if(kind==1){
        integer(p,"uLocalProbe0",4);integer(p,"uLocalProbe1",5);integer(p,"uLocalProbeCount",0);scalar(p,"uLocalIrradianceWeight",0);
        integer(p,"uBase",0);integer(p,"uNormal",1);integer(p,"uARM",8);integer(p,"uMacroHeight",11);integer(p,"uVillageSkyMask",12);integer(p,"uVillageSkyDirectional",13);integer(p,"uVillageSkyUp",14);
        glUniform4f(glGetUniformLocation(p,"uBaseFactor"),.4f,.5f,.2f,1);scalar(p,"uRoughness",.8f);
        integer(p,"uLodLevel",-1);scalar(p,"uTreeHeight",25);scalar(p,"uPixelScale",1000);
    }else{
        integer(p,"uOpaqueColor",0);integer(p,"uOpaqueDepth",1);integer(p,"uClimate",6);integer(p,"uReflection",10);
        vector(p,"uSkyZenith",.1f,.2f,.3f);scalar(p,"uNear",.4f);scalar(p,"uFar",15000);scalar(p,"uEyeHeight",2);
        glUniform2f(glGetUniformLocation(p,"uResolution"),side,side);
    }
}
void run(GLuint p,GLuint shadow,const char *name){
    glUseProgram(p);GLint flag=-1;glGetUniformiv(p,glGetUniformLocation(p,"uSoftShadows"),&flag);assert(flag==0);
    position(p,7.5f,8);auto legacy=draw(p);integer(p,"uSoftShadows",1);
    std::array<float,side*side> depths;
    for(int pattern=0;pattern<2;pattern++){
        for(int y=0;y<side;y++)for(int x=0;x<side;x++)depths[y*side+x]=(pattern==0?x>=8:x+y>=16)?1.f:0.f;
        glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,shadow);glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,side,side,0,GL_RED,GL_FLOAT,depths.data());
        float previous=-1;unsigned partial=0;float max_step=0;
        for(int step=0;step<=640;step++){
            position(p,5.5f+float(step)/128,8);float v=draw(p)[0][0];
            assert(v>=-1e-6f&&v<=1.000001f);if(previous>=0){assert(v>=previous-1e-6f);max_step=std::max(max_step,std::abs(v-previous));}
            partial+=v>1e-5f&&v<.99999f;previous=v;
            if(step==0)assert(v==0);
            if(step==640)assert(std::abs(v-1)<1e-6f);
        }
        assert(partial>180&&max_step<.013f);
        position(p,7.5f,8);const auto nearest=draw(p);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        const auto linear=draw(p);assert(nearest==linear); // Stored depths are compared before filtering.
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    // Fully lit and unlit inputs preserve complete production RGBA/MRT output,
    // not just the isolated visibility return. Shadow filtering cannot alter
    // temporal stability/coverage or the separately stored indirect radiance.
    integer(p,"uTestProduction",1);position(p,7.5f,8);
    for(float depth:{0.f,1.f}){
        depths.fill(depth);glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,shadow);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,side,side,GL_RED,GL_FLOAT,depths.data());
        integer(p,"uSoftShadows",0);auto before=draw(p);integer(p,"uSoftShadows",1);auto after=draw(p);
        for(int attachment=0;attachment<2;attachment++)for(int k=0;k<4;k++)assert(std::abs(before[attachment][k]-after[attachment][k])<3e-6f);
        assert(before[0][3]==after[0][3]&&before[1]==after[1]);
    }
    integer(p,"uTestProduction",0);integer(p,"uSoftShadows",0);
    // Initial map was wholly lit; flag reset restores the exact legacy value.
    assert(draw(p)==legacy);
    std::printf("PASS %s: nine real depth compares; smooth axis/diagonal edge translation; lit/unlit production RGBA+indirect unchanged; zero flag exact\n",name);
}
}
int main(int argc,char **argv){
    assert(argc<=2);if(argc==2)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Shadow filter regression",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Shadow filter renderer: %s\n",glGetString(GL_RENDERER));
    GLuint vao,fbo;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    GLuint targets[2];glGenTextures(2,targets);
    for(int i=0;i<2;i++){glBindTexture(GL_TEXTURE_2D,targets[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,side,side,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,targets[i],0);}
    const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glViewport(0,0,side,side);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    GLuint textures[16];glGenTextures(16,textures);
    for(int unit=0;unit<16;unit++){
        glActiveTexture(GL_TEXTURE0+unit);GLenum type=unit==12?GL_TEXTURE_2D_ARRAY:GL_TEXTURE_2D;glBindTexture(type,textures[unit]);
        Pixel p={.3f,.4f,.2f,1};if(unit==4||unit==5||unit==13)p={.5f,.5f,1,1};if(unit==6)p={0,.5f,0,1};if(unit==7)p={1,1,1,1};if(unit==8||unit==9||unit==14)p={1,.8f,0,1};if(unit==11||unit==12)p={0,0,0,0};
        if(unit==12){const float empty[12]={};glTexImage3D(type,0,GL_RGBA32F,1,1,3,0,GL_RGBA,GL_FLOAT,empty);}else glTexImage2D(type,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,p.data());
        glTexParameteri(type,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(type,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(type,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(type,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    GLuint litter;glGenTextures(1,&litter);glActiveTexture(GL_TEXTURE13);glBindTexture(GL_TEXTURE_2D_ARRAY,litter);
    const float litter_layers[]={.5f,.5f,1,1,1,.8f,0,1};glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,1,1,2,0,GL_RGBA,GL_FLOAT,litter_layers);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    const char *paths[]={"shaders/world_terrain.fs.glsl","shaders/world_foliage.fs.glsl","shaders/world_water.fs.glsl"};
    const char *names[]={"terrain","foliage","water"};
    for(int i=0;i<3;i++){
        Pixel lit={1,1,1,1};glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,textures[7]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,lit.data());
        GLuint p=program(paths[i],i==2);setup(p,i);run(p,textures[7],names[i]);glDeleteProgram(p);
    }
    glDeleteTextures(1,&litter);glDeleteTextures(16,textures);glDeleteTextures(2,targets);glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();std::puts("world_shadow_filter_gl_test: PASS");
}
