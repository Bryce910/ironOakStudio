// Old/new lean production fragment shader comparison. Software GL only.
// Correctness fixture; no real-GPU performance or scene-quality claim.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
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
#include <filesystem>
namespace {
constexpr unsigned side=32;
using Pixel=std::array<float,4>;
const char *vertex=R"GLSL(#version 330 core
layout(location=0)in vec2 p;
uniform vec3 testNormal;uniform float testNormalSpan;uniform float testDistance;
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;out vec2 vUV;
flat out float vSeed,vCoveragePhase,vScale,vRootHeight,vInstanceFade;
flat out vec3 vRootRelative;out float vFacadePaint;out vec3 vSourcePaint;out vec4 vBakedLight;out float vRootedGrassWindSpeed;flat out vec2 vItemWind;
void main(){vPos=vec3(20.+p.x,24.3+p.y,40.);vRelative=vec3(p,-testDistance);vRootRelative=vec3(0,0,-testDistance);vRootHeight=24.;
vNormal=normalize(testNormal+vec3(p*testNormalSpan,0));vTangent=abs(testNormal.x)>.9?vec3(0,0,1):vec3(1,0,0);vBitangent=normalize(cross(vNormal,vTangent));
vUV=p*.5+.5;vSeed=.37;vCoveragePhase=0.;vScale=1.;vInstanceFade=1.;vFacadePaint=.3;vSourcePaint=vec3(.65+.3*vUV.x);vBakedLight=vec4(.03,.025,.02,.25);vRootedGrassWindSpeed=.18;vItemWind=vec2(.5,.3);gl_Position=vec4(p,0,1);})GLSL";
std::string read(const char *path){std::ifstream input(path);assert(input);std::ostringstream text;text<<input.rdbuf();return text.str();}
std::string expand(const std::filesystem::path&path,unsigned depth=0){assert(depth<32);std::istringstream input(read(path.c_str()));std::string line,out;while(std::getline(input,line)){const auto key=line.find("#pragma include ");if(key!=std::string::npos){const auto begin=line.find('"',key),end=line.find('"',begin+1);assert(begin!=std::string::npos&&end!=std::string::npos);out+=expand(path.parent_path()/line.substr(begin+1,end-begin-1),depth+1);}else out+=line+"\n";}return out;}
std::string source(const std::filesystem::path&bank,bool grass){auto text=expand(bank/"world_foliage.fs.glsl");const std::string defines=std::string("#define VALWOOD_QA_DISABLE_TEXTURE_GATHER 1\n#define VALWOOD_LOCAL_LIGHT_SHADOW 1\n")+(grass?"#define VALWOOD_GENERATED_GRASS_FRAGMENT 1\n":"#define VALWOOD_VEGETATION_FRAGMENT 1\n");text.insert(text.find('\n')+1,defines);return text;}
GLuint program(const std::string &source){const auto p=GLH_program(vertex,source.c_str());GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);assert(okay);return p;}
void f(GLuint p,const char *key,float value){glUniform1f(glGetUniformLocation(p,key),value);}
void i(GLuint p,const char *key,int value){glUniform1i(glGetUniformLocation(p,key),value);}
void v(GLuint p,const char *key,float x,float y,float z){glUniform3f(glGetUniformLocation(p,key),x,y,z);}
void q(GLuint p,const char *key,float x,float y,float z,float w){glUniform4f(glGetUniformLocation(p,key),x,y,z,w);}
void defaults(GLuint p){
    glUseProgram(p);i(p,"uLocalLightShadow",16);i(p,"uFarShadow",18);i(p,"uBase",0);i(p,"uNormal",1);i(p,"uARM",2);i(p,"uEmissive",3);i(p,"uLocalProbe0",4);i(p,"uLocalProbe1",5);i(p,"uShadow",7);i(p,"uFacadeAux0",8);i(p,"uFacadeAux1",9);i(p,"uFacadeAux2",10);i(p,"uMacroHeight",11);i(p,"uVillageSkyMask",12);i(p,"uVillageSkyDirectional",13);i(p,"uVillageSkyUp",14);
    i(p,"uHasBase",1);i(p,"uHasNormal",1);i(p,"uHasARM",1);i(p,"uHasEmissive",0);i(p,"uLodLevel",-1);i(p,"uSoftShadows",1);i(p,"uFacadeMode",0);i(p,"uLocalProbeCount",0);i(p,"uArchitectureWeathering",0);i(p,"uVillageSkyEnabled",0);i(p,"uVillageDirectionalEnabled",0);i(p,"uForestShelterEnabled",0);i(p,"uClipBelowWater",0);i(p,"uTavernReturnEnabled",0);i(p,"uWindHistoryEnabled",1);
    q(p,"uBaseFactor",.63f,.42f,.19f,1);v(p,"uEmissiveFactor",0,0,0);v(p,"uSunColor",1.3f,1.1f,.92f);v(p,"uAmbient",.19f,.22f,.28f);v(p,"uSkyLight",.29f,.34f,.45f);v(p,"uGroundLight",.08f,.10f,.07f);v(p,"uFog",.17f,.22f,.28f);v(p,"uPropLodRange",0,0,0);v(p,"uVillageBounceAlbedo",0,0,0);
    f(p,"uLocalIrradianceWeight",0);f(p,"uLocalIrradianceSecondaryWeight",0);f(p,"uHaze",.0015f);f(p,"uCloud",.8f);f(p,"uTime",217);f(p,"uTreeHeight",25);f(p,"uPixelScale",1000);f(p,"uActorReactive",0);i(p,"uOrganicBody",0);f(p,"testDistance",30);f(p,"uWind",.65f);f(p,"uWindFrameDelta",1.f/60);f(p,"uNormalScale",1);
    glUniform2f(glGetUniformLocation(p,"uWorldSize"),100800,56800);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);glUniform2f(glGetUniformLocation(p,"uWeatherWind"),.7f,.3f);
    const float projection[16]={.01f,0,0,0,0,.01f,0,0,0,0,.01f,0,0,0,0,1};glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,projection);
    q(p,"uLocalLightShadowPositionRadius",20,26,43,12);glUniform2f(glGetUniformLocation(p,"uLocalLightShadowNearFar"),.05f,12.f);f(p,"uLocalLightShadowEnabled",1);f(p,"uLocalLightShadowSide",256);
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
    assert(argc==3);assert(std::getenv("LIBGL_ALWAYS_SOFTWARE")&&std::string(std::getenv("LIBGL_ALWAYS_SOFTWARE"))=="1");SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto *window=SDL_CreateWindow("Valwood BRDF zero support",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);std::printf("BRDF zero renderer: %s\n",glGetString(GL_RENDERER));
    std::array<std::array<GLuint,2>,2> variants;
    for(unsigned variant=0;variant<2;++variant){const auto before=source(argv[1],variant),after=source(argv[2],variant);assert(before!=after);variants[variant]={program(before),program(after)};
      assert(glGetUniformLocation(variants[variant][1],"uActorReactive")==-1);assert(glGetUniformLocation(variants[variant][1],"uOrganicBody")==-1);
      if(variant==0)assert(glGetUniformLocation(variants[variant][0],"uActorReactive")>=0);
    }
    auto &programs=variants[0];
    GLuint framebuffer,targets[2],vao,vbo,textures[5];glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glGenTextures(2,targets);
    for(unsigned target=0;target<2;target++){glBindTexture(GL_TEXTURE_2D,targets[target]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,side,side,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+target,GL_TEXTURE_2D,targets[target],0);}const GLenum draw_buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,draw_buffers);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glGenTextures(5,textures);const unsigned units[5]={0,1,2,7,11};for(unsigned role=0;role<5;role++){std::array<Pixel,16*16> pixels;for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){const float a=x/15.f,b=y/15.f;if(role==0)pixels[y*16+x]={.18f+.7f*a,.25f+.5f*b,.11f+.3f*a*b,.2f+.8f*a};else if(role==1)pixels[y*16+x]={.5f+.08f*std::sin(a*7),.5f+.07f*std::cos(b*6),.99f,1};else if(role==2)pixels[y*16+x]={.65f+.35f*b,.7f+.3f*a,1,1};else pixels[y*16+x]={role==3?.52f+.30f*a:24.f,0,0,1};}glActiveTexture(GL_TEXTURE0+units[role]);glBindTexture(GL_TEXTURE_2D,textures[role]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,16,16,0,GL_RGBA,GL_FLOAT,pixels.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
    GLuint cube=0,sky=0;glGenTextures(1,&cube);glActiveTexture(GL_TEXTURE16);glBindTexture(GL_TEXTURE_CUBE_MAP,cube);const float depth=.95f;for(unsigned face=0;face<6;++face)glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&depth);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    glGenTextures(1,&sky);glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,sky);std::array<Pixel,16*16*6> field;for(unsigned layer=0;layer<6;++layer)for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)field[(layer*16+y)*16+x]={.40f+.2f*x/15.f,.48f,.90f,.5f+.2f*y/15.f};glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,16,16,6,0,GL_RGBA,GL_FLOAT,field.data());glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glActiveTexture(GL_TEXTURE0);
    const float quad[]={-1,-1,1,-1,1,1,-1,-1,1,1,-1,1};glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);glViewport(0,0,side,side);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    for(const auto&variant:variants)for(auto p:variant)defaults(p);
    const float normals[4][3]={{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}},suns[4][3]={{0,1,0},{0,-1,0},{0,.01f,.99995f},{0,-.01f,-.99995f}};
    unsigned cases=0,max_byte=0;std::uint64_t comparisons=0,unequal=0,covered=0,discarded=0;float max_hdr=0,max_color=0,max_indirect=0;
    for(unsigned variant=0;variant<2;++variant)for(unsigned kind=0;kind<(variant?1u:7u);kind++)for(float rough:{.25f,.8f})for(float metal:{0.f,.95f})for(float wet:{0.f,1.f})for(unsigned normal=0;normal<4;normal++)for(const auto &sun:suns)for(int practical:{0,1}){
        std::array<Image,2> rendered;
        for(unsigned version=0;version<2;version++){const auto p=variants[variant][version];glUseProgram(p);i(p,"uVegetation",kind==0||kind==3?0:kind==1?1:kind==2?3:kind==4?4:kind==5?5:6);i(p,"uGrass",kind==3);f(p,"uAlphaCutoff",variant?.45f:kind==0?0:.45f);f(p,"uRoughness",rough);f(p,"uMetallic",metal);f(p,"uWetness",wet);v(p,"testNormal",normals[normal][0],normals[normal][1],normals[normal][2]);f(p,"testNormalSpan",normal%2?.35f:0);v(p,"uSun",sun[0],sun[1],sun[2]);i(p,"uOrganicBody",0);i(p,"uSkinMaterial",0);f(p,"uActorReactive",0);i(p,"uSourcePaintEnabled",normal%2);i(p,"uSourceMaterialAO",practical);i(p,"uVegetationFinish",1);i(p,"uSoftShadows",practical);f(p,"testDistance",kind==4?350:normal%2?90:30);
        i(p,"uWetStateEnabled",wet>0);q(p,"uWetStateGlobal",wet*.7f,wet*.5f,0,0);i(p,"uWetStateCount",wet>0?1:0);q(p,"uWetStateBounds[0]",20,40,.5f,5);q(p,"uWetStateChannels[0]",.85f,.6f,0,.75f);i(p,"uAuthoredWeatherCount",wet>0?1:0);i(p,"uAuthoredWeatherAreaCount",wet>0?1:0);q(p,"uAuthoredWeatherBounds[0]",20,40,.5f,5);q(p,"uAuthoredWeatherChannels[0]",.9f,.8f,.3f,.8f);
        i(p,"uForestShelterEnabled",practical);q(p,"uForestShelterBounds",0,0,.01f,.01f);glUniform2f(glGetUniformLocation(p,"uForestShelterHeights"),0,50);i(p,"uVillageSkyEnabled",practical);q(p,"uVillageSkyBounds",0,0,.01f,.01f);glUniform2f(glGetUniformLocation(p,"uVillageSkyHeights"),24,36);q(p,"uLeafTransmission",.8f,.9f,.55f,.4f);glUniform4f(glGetUniformLocation(p,"uStudioTintRough"),1,1,1,1);glUniform2f(glGetUniformLocation(p,"uStudioNormalUV"),1,1);
        i(p,"uCastleLightsEnabled",practical);i(p,"uTorchCount",practical?8:0);rendered[version]=draw(p);}
        for(const auto&pixel:rendered[0].pixels[0]){covered+=pixel[3]>0.;discarded+=pixel[3]==0.;for(unsigned c=0;c<3;++c)max_color=std::max(max_color,pixel[c]);}
        for(const auto&pixel:rendered[0].pixels[1])for(unsigned c=0;c<3;++c)max_indirect=std::max(max_indirect,pixel[c]);
        if(variant==0&&kind==1&&rough>.7f&&metal==0&&wet==0&&normal==0&&sun[1]>.9f&&practical){for(unsigned version=0;version<2;++version){std::ofstream out(std::string("new_engine/build/camp-completion-2026-11-21/review/lean-validation/")+(version?"folded":"unfolded")+"-tree.ppm",std::ios::binary);out<<"P6\n"<<side<<' '<<side<<"\n255\n";for(const auto&pixel:rendered[version].pixels[0])for(unsigned c=0;c<3;++c){const unsigned char value=static_cast<unsigned char>(byte(pixel[c]));out.write(reinterpret_cast<const char*>(&value),1);}}}
        for(unsigned target=0;target<2;target++)for(unsigned pixel=0;pixel<side*side;pixel++)for(unsigned channel=0;channel<4;channel++){const float a=rendered[0].pixels[target][pixel][channel],b=rendered[1].pixels[target][pixel][channel];assert(std::isfinite(a)&&std::isfinite(b));const float difference=std::abs(a-b);max_hdr=std::max(max_hdr,difference);unequal+=a!=b;comparisons++;const unsigned byte_difference=unsigned(std::abs(int(byte(a))-int(byte(b))));max_byte=std::max(max_byte,byte_difference);if(byte_difference>1||difference>.0001f*std::max({1.f,std::abs(a),std::abs(b)})){std::fprintf(stderr,"BRDF mismatch case%u kind%u rough%.2f metal%.2f wet%.2f normal%u practical%d target%u pixel%u channel%u %.9g vs %.9g\n",cases,kind,rough,metal,wet,normal,practical,target,pixel,channel,a,b);assert(false);}if(channel==3)assert(a==b);}
        cases++;
    }
    assert(covered>100000&&discarded>100000&&max_color>.01f&&max_indirect>.01f);
    std::printf("Nontrivial raster output: covered=%llu discarded=%llu max_HDR_color=%.6f max_HDR_indirect=%.6f\n",(unsigned long long)covered,(unsigned long long)discarded,max_color,max_indirect);
    std::printf("PASS lean constant fold: %u lean+generated-grass front/back/grazing opaque/tree/fern/grass/card/bush cases; dry/wet reservoir, source paint, source leaf transmission, normal/ARM maps, canopy+village sky fields, clouds/fog, PCF shadow and cubeshadow torches; %llu HDR/indirect/coverage components, %llu unequal, maximum HDR difference %.9g, mapped byte difference %u\n",cases,static_cast<unsigned long long>(comparisons),static_cast<unsigned long long>(unequal),max_hdr,max_byte);
    glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);glDeleteTextures(5,textures);glDeleteTextures(2,targets);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&cube);glDeleteTextures(1,&sky);for(const auto&variant:variants)for(auto p:variant)glDeleteProgram(p);assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
