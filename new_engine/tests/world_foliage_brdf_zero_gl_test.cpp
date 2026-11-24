// Old/new full production fragment shader comparison. Run only in the root's
// serialized native window; this is a correctness regression, not an FPS test.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include "stb_image.h"
#include "gl_helpers.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
namespace {
constexpr unsigned side=32;
using Pixel=std::array<float,4>;
const char *vertex=R"GLSL(#version 330 core
layout(location=0)in vec2 p;
uniform vec3 testNormal;uniform float testNormalSpan;
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;out vec2 vUV;
flat out float vSeed,vCoveragePhase,vScale,vRootHeight,vInstanceFade;
flat out vec3 vRootRelative;out float vFacadePaint;
void main(){vPos=vec3(20.+p.x,24.3+p.y,40.);vRelative=vec3(p,-30.);vRootRelative=vec3(0,0,-30);vRootHeight=24.;
vNormal=normalize(testNormal+vec3(p*testNormalSpan,0));vTangent=abs(testNormal.x)>.9?vec3(0,0,1):vec3(1,0,0);vBitangent=normalize(cross(vNormal,vTangent));
vUV=p*.5+.5;vSeed=.37;vCoveragePhase=0.;vScale=1.;vInstanceFade=1.;vFacadePaint=0.;gl_Position=vec4(p,0,1);})GLSL";
std::string read(const char *path){std::ifstream input(path);assert(input);std::ostringstream text;text<<input.rdbuf();return text.str();}
GLuint program(const std::string &source){const auto p=GLH_program(vertex,source.c_str());GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);assert(okay);return p;}
void f(GLuint p,const char *key,float value){glUniform1f(glGetUniformLocation(p,key),value);}
void i(GLuint p,const char *key,int value){glUniform1i(glGetUniformLocation(p,key),value);}
void v(GLuint p,const char *key,float x,float y,float z){glUniform3f(glGetUniformLocation(p,key),x,y,z);}
void q(GLuint p,const char *key,float x,float y,float z,float w){glUniform4f(glGetUniformLocation(p,key),x,y,z,w);}
void defaults(GLuint p){
    glUseProgram(p);i(p,"uBase",0);i(p,"uNormal",1);i(p,"uARM",2);i(p,"uEmissive",3);i(p,"uLocalProbe0",4);i(p,"uLocalProbe1",5);i(p,"uShadow",7);i(p,"uFacadeAux0",8);i(p,"uFacadeAux1",9);i(p,"uFacadeAux2",10);i(p,"uMacroHeight",11);i(p,"uVillageSkyMask",12);i(p,"uVillageSkyDirectional",13);i(p,"uVillageSkyUp",14);
    i(p,"uHasBase",1);i(p,"uHasNormal",1);i(p,"uHasARM",1);i(p,"uHasEmissive",0);i(p,"uLodLevel",-1);i(p,"uSoftShadows",1);i(p,"uFacadeMode",0);i(p,"uLocalProbeCount",0);i(p,"uArchitectureWeathering",0);i(p,"uVillageSkyEnabled",0);i(p,"uVillageDirectionalEnabled",0);i(p,"uForestShelterEnabled",0);i(p,"uClipBelowWater",0);i(p,"uTavernReturnEnabled",0);i(p,"uWindHistoryEnabled",1);
    q(p,"uBaseFactor",.63f,.42f,.19f,1);v(p,"uEmissiveFactor",0,0,0);v(p,"uSunColor",1.3f,1.1f,.92f);v(p,"uAmbient",.19f,.22f,.28f);v(p,"uSkyLight",.29f,.34f,.45f);v(p,"uGroundLight",.08f,.10f,.07f);v(p,"uFog",.17f,.22f,.28f);v(p,"uPropLodRange",0,0,0);v(p,"uVillageBounceAlbedo",0,0,0);
    f(p,"uLocalIrradianceWeight",0);f(p,"uLocalIrradianceSecondaryWeight",0);f(p,"uHaze",.0015f);f(p,"uCloud",.8f);f(p,"uTime",217);f(p,"uTreeHeight",25);f(p,"uPixelScale",1000);f(p,"uActorReactive",0);f(p,"uWind",.65f);f(p,"uWindFrameDelta",1.f/60);f(p,"uNormalScale",1);
    glUniform2f(glGetUniformLocation(p,"uWorldSize"),100800,56800);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);glUniform2f(glGetUniformLocation(p,"uWeatherWind"),.7f,.3f);
    const float projection[16]={.01f,0,0,0,0,.01f,0,0,0,0,.01f,0,0,0,0,1};glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,projection);
    v(p,"uTorchClusterMin",0,0,0);v(p,"uTorchClusterMax",100,50,100);v(p,"uCastleLightColor",2.1f,1.2f,.45f);q(p,"uCastleLightBounds",0,0,100,100);glUniform2f(glGetUniformLocation(p,"uCastleLightHeights"),0,50);
    const float lights[4][3]={{20,26,43},{20,26,37},{23,24.3f,40},{17,24.3f,40}};
    for(unsigned index=0;index<8;index++){
        char key[80];const auto &light=lights[index%4];std::snprintf(key,sizeof(key),"uTorchPositionRadius[%u]",index);q(p,key,light[0],light[1],light[2],12);std::snprintf(key,sizeof(key),"uTorchColorIntensity[%u]",index);q(p,key,1,.4f,.12f,3);std::snprintf(key,sizeof(key),"uTorchWallPlane[%u]",index);q(p,key,0,1,0,0);std::snprintf(key,sizeof(key),"uTorchClipBounds[%u]",index);q(p,key,0,0,100,100);std::snprintf(key,sizeof(key),"uTorchClipHeights[%u]",index);glUniform2f(glGetUniformLocation(p,key),0,50);
        if(index<4){std::snprintf(key,sizeof(key),"uCastleLightPositionRadius[%u]",index);q(p,key,light[0],light[1],light[2],12);}
    }
}
struct Image {std::array<std::vector<Pixel>,2> pixels;};
Image draw(GLuint p){glUseProgram(p);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,6);Image image;for(unsigned target=0;target<2;target++){image.pixels[target].resize(side*side);glReadBuffer(GL_COLOR_ATTACHMENT0+target);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,image.pixels[target].data());}assert(glGetError()==GL_NO_ERROR);return image;}
unsigned byte(float x){x=std::max(0.f,x);return unsigned(std::lround(255*x/(1+x)));}
}
int main(int argc,char **argv){
    assert(argc<=2);if(argc==2)assert(chdir(argv[1])==0);if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto *window=SDL_CreateWindow("Valwood BRDF zero support",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);std::printf("BRDF zero renderer: %s\n",glGetString(GL_RENDERER));
    const auto before=read("tests/fixtures/shaders/world_foliage_brdf.before.fs.glsl"),after=read("shaders/world_foliage.fs.glsl");assert(before!=after);
    const GLuint programs[2]={program(before),program(after)};
    GLuint framebuffer,targets[2],vao,vbo,textures[5];glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glGenTextures(2,targets);
    for(unsigned target=0;target<2;target++){glBindTexture(GL_TEXTURE_2D,targets[target]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,side,side,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+target,GL_TEXTURE_2D,targets[target],0);}const GLenum draw_buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,draw_buffers);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glGenTextures(5,textures);const unsigned units[5]={0,1,2,7,11};for(unsigned role=0;role<5;role++){std::array<Pixel,16*16> pixels;for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){const float a=x/15.f,b=y/15.f;if(role==0)pixels[y*16+x]={.18f+.7f*a,.25f+.5f*b,.11f+.3f*a*b,.2f+.8f*a};else if(role==1)pixels[y*16+x]={.5f+.08f*std::sin(a*7),.5f+.07f*std::cos(b*6),.99f,1};else if(role==2)pixels[y*16+x]={.65f+.35f*b,.7f+.3f*a,1,1};else pixels[y*16+x]={role==3?1.f:24.f,0,0,1};}glActiveTexture(GL_TEXTURE0+units[role]);glBindTexture(GL_TEXTURE_2D,textures[role]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,16,16,0,GL_RGBA,GL_FLOAT,pixels.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
    const float quad[]={-1,-1,1,-1,1,1,-1,-1,1,1,-1,1};glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);glViewport(0,0,side,side);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    for(auto p:programs)defaults(p);
    const float normals[4][3]={{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}},suns[4][3]={{0,1,0},{0,-1,0},{0,.01f,.99995f},{0,-.01f,-.99995f}};
    unsigned cases=0,max_byte=0;std::uint64_t comparisons=0,unequal=0;float max_hdr=0;
    for(unsigned kind=0;kind<5;kind++)for(float rough:{.25f,.8f})for(float metal:{0.f,.95f})for(float wet:{0.f,1.f})for(unsigned normal=0;normal<4;normal++)for(const auto &sun:suns)for(int practical:{0,1}){
        std::array<Image,2> rendered;
        for(unsigned version=0;version<2;version++){const auto p=programs[version];glUseProgram(p);i(p,"uVegetation",kind==0||kind==3?0:kind==1?1:kind==2?3:4);i(p,"uGrass",kind==3);f(p,"uAlphaCutoff",kind==0?0:.45f);f(p,"uRoughness",rough);f(p,"uMetallic",metal);f(p,"uWetness",wet);v(p,"testNormal",normals[normal][0],normals[normal][1],normals[normal][2]);f(p,"testNormalSpan",normal%2?.35f:0);v(p,"uSun",sun[0],sun[1],sun[2]);i(p,"uCastleLightsEnabled",practical);i(p,"uTorchCount",practical?8:0);rendered[version]=draw(p);}
        for(unsigned target=0;target<2;target++)for(unsigned pixel=0;pixel<side*side;pixel++)for(unsigned channel=0;channel<4;channel++){const float a=rendered[0].pixels[target][pixel][channel],b=rendered[1].pixels[target][pixel][channel];assert(std::isfinite(a)&&std::isfinite(b));const float difference=std::abs(a-b);max_hdr=std::max(max_hdr,difference);unequal+=a!=b;comparisons++;const unsigned byte_difference=unsigned(std::abs(int(byte(a))-int(byte(b))));max_byte=std::max(max_byte,byte_difference);if(byte_difference>1||difference>.0001f*std::max({1.f,std::abs(a),std::abs(b)})){std::fprintf(stderr,"BRDF mismatch case%u kind%u rough%.2f metal%.2f wet%.2f normal%u practical%d target%u pixel%u channel%u %.9g vs %.9g\n",cases,kind,rough,metal,wet,normal,practical,target,pixel,channel,a,b);assert(false);}if(channel==3)assert(a==b);}
        cases++;
    }
    std::printf("PASS BRDF zero support: %u front/back/grazing opaque/leaf/fern/grass/card cases; dry/wet rough/metal, normal/ARM maps, clouds/fog, 8 torches +4 castle lamps; %llu HDR/indirect/coverage components, %llu unequal, maximum HDR difference %.9g, mapped byte difference %u\n",cases,static_cast<unsigned long long>(comparisons),static_cast<unsigned long long>(unequal),max_hdr,max_byte);
    glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);glDeleteTextures(5,textures);glDeleteTextures(2,targets);glDeleteFramebuffers(1,&framebuffer);for(auto p:programs)glDeleteProgram(p);assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
