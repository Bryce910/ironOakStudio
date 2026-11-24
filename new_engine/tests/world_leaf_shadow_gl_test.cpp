// Complete actual-production fragment regression; no scratch shader inputs.
// g++ -std=c++17 -O2 -Wall -Wextra -Werror tests/world_leaf_shadow_gl_test.cpp
//     -o /tmp/valwood-leaf-shadow-gl-test -lSDL2 -lGL
// LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-leaf-shadow-gl-test <engine-root>
// The complete production fragment main, both HDR outputs and depth are the
// authority. A synthetic VS controls finite material/distance boundary inputs.
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
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
namespace {
constexpr unsigned side=32;
using Pixel=std::array<float,4>;
struct Frame {std::array<std::vector<Pixel>,2> color;std::vector<float> depth;};
std::string read(const char *path){std::ifstream f(path);assert(f);std::stringstream s;s<<f.rdbuf();return s.str();}
std::string replaced(std::string s,const std::string &a,const std::string &b){const auto at=s.find(a);assert(at!=std::string::npos&&s.find(a,at+1)==std::string::npos);s.replace(at,a.size(),b);return s;}
GLuint compile(GLenum type,const std::string &s){GLuint id=glCreateShader(type);const char *p=s.c_str();glShaderSource(id,1,&p,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);return id;}
GLuint program(const std::string &fragment){
    const std::string vertex=R"GLSL(#version 330 core
uniform vec3 tWorld,tRelative,tNormal;
uniform float tSpan,tNormalSpan;
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;
out vec2 vUV;
flat out float vSeed,vCoveragePhase,vScale,vRootHeight,vInstanceFade;
flat out vec3 vRootRelative;
out float vFacadePaint;
void main(){
 vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2),q=p-.5;
 gl_Position=vec4(p*2.-1.,0.,1.);
 vPos=tWorld+vec3(q.x*7.,q.y*.7,q.y*9.);
 vRelative=tRelative+vec3(q.x*tSpan,0.,q.y*tSpan);
 vNormal=tNormal+vec3(0.,q.x*tNormalSpan,0.);
 vTangent=vec3(0.,0.,1.);vBitangent=vec3(1.,0.,0.);vUV=p*3.;
 vSeed=.37;vCoveragePhase=.61;vScale=1.;vRootHeight=tWorld.y-2.;
 vRootRelative=tRelative;vInstanceFade=1.;vFacadePaint=0.;
})GLSL";
    GLuint v=compile(GL_VERTEX_SHADER,vertex),f=compile(GL_FRAGMENT_SHADER,fragment),p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[8192];glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(ok);glDeleteShader(v);glDeleteShader(f);return p;
}
void i(GLuint p,const char *n,int x){glUniform1i(glGetUniformLocation(p,n),x);}
void f(GLuint p,const char *n,float x){glUniform1f(glGetUniformLocation(p,n),x);}
void v2(GLuint p,const char *n,float x,float y){glUniform2f(glGetUniformLocation(p,n),x,y);}
void v3(GLuint p,const char *n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);}
void v4(GLuint p,const char *n,float x,float y,float z,float w){glUniform4f(glGetUniformLocation(p,n),x,y,z,w);}
void defaults(GLuint p,bool terrain){
    glUseProgram(p);
    for(const char *n:{"uBase","uEmissive","uFacadeAux0","uFacadeAux1","uFacadeAux2","uGrass","uRock","uSand","uDirt","uVillageGroundMask","uStartingMask"})i(p,n,0);
    for(const char *n:{"uNormal","uGrassNormal","uRockNormal"})i(p,n,1);
    for(const char *n:{"uARM","uGrassARM","uRockARM"})i(p,n,2);
    i(p,"uClimate",6);i(p,"uShadow",7);i(p,"uMacroHeight",11);
    for(const char *n:{"uVillageSkyMask","uLitterPBR","uLandscape","uVillagePBR"})i(p,n,12);
    i(p,"uVillageSkyDirectional",13);i(p,"uVillageSkyUp",14);i(p,"uLocalProbe0",4);i(p,"uLocalProbe1",5);
    v3(p,"uSun",0,1,0);v3(p,"uSunColor",.7f,.65f,.6f);v3(p,"uAmbient",.3f,.3f,.3f);
    v3(p,"uSkyLight",.35f,.4f,.45f);v3(p,"uGroundLight",.1f,.12f,.08f);v3(p,"uFog",.15f,.18f,.17f);
    v2(p,"uWorldSize",200000,200000);v4(p,"uBaseFactor",.6f,.5f,.35f,1);
    f(p,"uRoughness",.7f);f(p,"uTreeHeight",10);f(p,"uPixelScale",600);i(p,"uLodLevel",-1);
    f(p,"uHaze",.00005f);f(p,"uCloud",.96f);f(p,"uTime",7.3f);
    i(p,"uHasBase",1);i(p,"uHasARM",1);i(p,"uSoftShadows",1);
    v3(p,"tWorld",1000,20,900);v3(p,"tRelative",20,-4,0);v3(p,"tNormal",0,1,0);
    v3(p,"uLocalProbeCenter[0]",1000,22,900);v3(p,"uLocalProbeCenter[1]",1006,22,900);
    v3(p,"uLocalProbeBoxMin[0]",990,0,890);v3(p,"uLocalProbeBoxMax[0]",1005,50,910);
    v3(p,"uLocalProbeBoxMin[1]",1000,0,890);v3(p,"uLocalProbeBoxMax[1]",1020,50,910);
    v2(p,"uLocalProbeWeights",.3f,.4f);f(p,"uLocalProbeMip",2);f(p,"uLocalProbeLightScale",1);
    // Constant depth .5 inside the shadow image; no perspective or invalid q.w.
    const float light[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,light);
    if(terrain){
        f(p,"uRainforest",1);v4(p,"uRainforestBounds",1000,900,.001f,.001f);
        for(unsigned k=0;k<3;k++){const auto n="uDistricts["+std::to_string(k)+"]";v4(p,n.c_str(),-10000,-10000,.001f,.001f);}
        v4(p,"uTrailBounds",-10000,-10000,-9000,-9000);
        for(unsigned k=0;k<6;k++){const auto n="uRainforestPath["+std::to_string(k)+"]";v2(p,n.c_str(),-10000+float(k)*10,-10000);}
    }
}
Frame frame(GLuint p){
    glUseProgram(p);const float a[4]={.013f,.02f,.03f,.04f},b[4]={.08f,.07f,.06f,.05f};glClearBufferfv(GL_COLOR,0,a);glClearBufferfv(GL_COLOR,1,b);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);Frame out;
    for(unsigned k=0;k<2;k++){out.color[k].resize(side*side);glReadBuffer(GL_COLOR_ATTACHMENT0+k);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,out.color[k].data());}
    out.depth.resize(side*side);glReadPixels(0,0,side,side,GL_DEPTH_COMPONENT,GL_FLOAT,out.depth.data());
    for(const auto &attachment:out.color)for(const auto &pixel:attachment)for(float x:pixel)assert(std::isfinite(x));
    for(float x:out.depth)assert(std::isfinite(x));
    assert(glGetError()==GL_NO_ERROR);return out;
}
bool same(const Frame &a,const Frame &b){
    for(unsigned k=0;k<2;k++)if(std::memcmp(a.color[k].data(),b.color[k].data(),a.color[k].size()*sizeof(Pixel)))return false;
    return std::memcmp(a.depth.data(),b.depth.data(),a.depth.size()*sizeof(float))==0;
}
float delta(const Frame &a,const Frame &b){float d=0;for(unsigned k=0;k<2;k++)for(unsigned at=0;at<a.color[k].size();at++)for(unsigned c=0;c<4;c++)d=std::max(d,std::abs(a.color[k][at][c]-b.color[k][at][c]));return d;}
void filter(GLenum target){glTexParameteri(target,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(target,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(target,GL_TEXTURE_WRAP_T,GL_REPEAT);}
}
int main(int argc,char **argv){
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Zero contribution parity",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);const auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Zero contribution renderer: %s\n",glGetString(GL_RENDERER));
    auto legacy_terrain=read("shaders/world_terrain.fs.glsl"),legacy_foliage=read("shaders/world_foliage.fs.glsl");
    const bool applied=legacy_foliage.find("    vec3 radiance=vec3(0.);\n")!=std::string::npos;
    assert(applied);
    const auto candidate_terrain=legacy_terrain;
    const auto candidate_foliage=legacy_foliage;
    legacy_foliage=world_shader_lossless_test::without_leaf_zero(world_shader_lossless_test::without_leaf_cloud_zero(world_shader_lossless_test::without_gather(legacy_foliage)));
    legacy_terrain=world_shader_lossless_test::without_gather(legacy_terrain);
    if(applied){
        legacy_terrain=replaced(legacy_terrain,
            "    // These ALU-only fields have exactly zero weight at the existing fade\n"
            "    // endpoints. Keep every positive-weight mix expression unchanged.\n"
            "    float meso = patch;\n"
            "    if(material_detail>0.)meso=mix(patch,noise(vPos.xz*.23+vec2(8.1,31.7)),material_detail);\n"
            "    float micro_detail=1.0-smoothstep(45.0,155.0,distance_to_eye);\n"
            "    float micro=.5;\n"
            "    if(micro_detail>0.)micro=mix(.5,noise(vPos.xz*1.17+vec2(71.4,9.2)),micro_detail);\n",
            "    float meso = mix(patch, noise(vPos.xz * .23 + vec2(8.1, 31.7)), material_detail);\n"
            "    float micro = mix(.5, noise(vPos.xz * 1.17 + vec2(71.4, 9.2)),\n"
            "                      1.0 - smoothstep(45.0, 155.0, distance_to_eye));\n");
        legacy_foliage=replaced(legacy_foliage,
            "    // A zero primary domain has no radiance contribution. Explicit LOD makes\n"
            "    // this branch independent of neighboring fragment texture derivatives.\n"
            "    vec3 radiance=vec3(0.);\n"
            "    if(weights.x>0.)radiance=textureLod(uLocalProbe0,local_reflection_direction(world,direction,0),rough*rough*uLocalProbeMip).rgb*weights.x;\n",
            "    vec3 radiance=textureLod(uLocalProbe0,local_reflection_direction(world,direction,0),rough*rough*uLocalProbeMip).rgb*weights.x;\n");
        legacy_foliage=replaced(legacy_foliage,
            "    // Opaque direct BRDF is exactly zero on the sun-opposed side. Thin\n"
            "    // leaves still need PCF for wrapped/transmitted light. Keep implicit-LOD\n"
            "    // four-tap sampling in its original control flow; nine-tap uses texelFetch.\n"
            "    float shadow=1.;\n"
            "    if(!uSoftShadows||leaf||!(dot(N,uSun)<=0.))shadow=visibility(geometricN);\n",
            "    float shadow=visibility(geometricN);\n");
    }
    // Before application the current files are legacy; afterward reconstruct
    // precisely the former expressions from actual production source.
    const GLuint terrain=program(candidate_terrain),terrain_old=program(legacy_terrain);
    const GLuint foliage=program(candidate_foliage),foliage_old=program(legacy_foliage);
    const GLuint wrong_noise=program(replaced(replaced(candidate_terrain,"if(material_detail>0.)","if(material_detail>.5)"),"if(micro_detail>0.)","if(micro_detail>.5)"));
    const GLuint wrong_probe=program(replaced(candidate_foliage,"if(weights.x>0.)radiance=","if(weights.x>.5)radiance="));
    const GLuint wrong_sun=program(replaced(candidate_foliage,"!(dot(N,uSun)<=0.)","!(dot(N,uSun)<=.2)"));
    const GLuint wrong_leaf=program(replaced(candidate_foliage,"bool leafSunZero=leaf&&dot(N,uSun)<=-.45&&dot(V,-uSun)<=0.;","bool leafSunZero=leaf;"));
    const GLuint wrong_through=program(replaced(candidate_foliage,"bool leafSunZero=leaf&&dot(N,uSun)<=-.45&&dot(V,-uSun)<=0.;","bool leafSunZero=leaf&&dot(N,uSun)<=-.45;"));
    const GLuint wrong_leaf_boundary=program(replaced(candidate_foliage,"dot(N,uSun)<=-.45&&","dot(N,uSun)<=-.44&&"));
    const GLuint wrong_cloud=program(replaced(candidate_foliage,"if(!leafSunZero)sunlight=uSunColor*cloud_transmittance();","if(!leaf)sunlight=uSunColor*cloud_transmittance();"));
    const GLuint wrong_opaque_cloud=program(replaced(candidate_foliage,"if(!leafSunZero)sunlight=uSunColor*cloud_transmittance();","if(!(dot(N,uSun)<=0.))sunlight=uSunColor*cloud_transmittance();"));
    const GLuint wrong_far_cloud=program(replaced(candidate_foliage,"        sunlight=uSunColor*cloud_transmittance();\n","        sunlight=uSunColor;\n"));
    for(GLuint p:{terrain,terrain_old,wrong_noise})defaults(p,true);
    for(GLuint p:{foliage,foliage_old,wrong_probe,wrong_sun,wrong_leaf,wrong_through,wrong_leaf_boundary,wrong_cloud,wrong_opaque_cloud,wrong_far_cloud})defaults(p,false);
    GLuint vao,fbo;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    std::array<GLuint,3> outputs={};glGenTextures(3,outputs.data());
    for(unsigned k=0;k<3;k++){glBindTexture(GL_TEXTURE_2D,outputs[k]);glTexImage2D(GL_TEXTURE_2D,0,k==2?GL_DEPTH_COMPONENT32F:GL_RGBA32F,side,side,0,k==2?GL_DEPTH_COMPONENT:GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glFramebufferTexture2D(GL_FRAMEBUFFER,k==2?GL_DEPTH_ATTACHMENT:GL_COLOR_ATTACHMENT0+k,GL_TEXTURE_2D,outputs[k],0);}
    const GLenum draw[2]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,draw);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    std::array<GLuint,9> maps={};glGenTextures(maps.size(),maps.data());
    for(unsigned k=0;k<6;k++){
        std::array<Pixel,64> data={};for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
            const float q=float((x+3*y)%8)/7;
            data[y*8+x]=k==0?Pixel{.2f+q*.5f,.15f+q*.2f,.1f+q*.2f,1}:k==1?Pixel{.1f+q*.8f,.1f+q*.7f,.7f,1}:k==2?Pixel{.6f,.3f+q*.6f,.15f,1}:k==3?Pixel{.55f,.6f,.2f,1}:k==4?Pixel{((x+y)&1)?.2f:.8f,0,0,1}:Pixel{25,0,0,1};
        }
        glBindTexture(GL_TEXTURE_2D,maps[k]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,8,8,0,GL_RGBA,GL_FLOAT,data.data());filter(GL_TEXTURE_2D);glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP,maps[6]);for(unsigned face=0;face<6;face++){std::array<Pixel,16> data;data.fill(Pixel{.6f+face*.03f,.1f,.05f,1});glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA32F,4,4,0,GL_RGBA,GL_FLOAT,data.data());}filter(GL_TEXTURE_CUBE_MAP);glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    // Separate secondary cube (rather than same-image fallback) proves weights.
    GLuint secondary;glGenTextures(1,&secondary);glBindTexture(GL_TEXTURE_CUBE_MAP,secondary);for(unsigned face=0;face<6;face++){std::array<Pixel,16> data;data.fill(Pixel{.03f,.13f,.7f+face*.02f,1});glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA32F,4,4,0,GL_RGBA,GL_FLOAT,data.data());}filter(GL_TEXTURE_CUBE_MAP);glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    std::array<Pixel,10> layers;layers.fill(Pixel{.55f,.65f,.8f,1});layers[3][3]=.45f;glBindTexture(GL_TEXTURE_2D_ARRAY,maps[7]);glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,1,1,10,0,GL_RGBA,GL_FLOAT,layers.data());filter(GL_TEXTURE_2D_ARRAY);glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    const Pixel white={1,1,1,1};glBindTexture(GL_TEXTURE_3D,maps[8]);glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA32F,1,1,1,0,GL_RGBA,GL_FLOAT,white.data());filter(GL_TEXTURE_3D);glGenerateMipmap(GL_TEXTURE_3D);
    for(unsigned unit=0;unit<16;unit++){glActiveTexture(GL_TEXTURE0+unit);if(unit==4||unit==5)glBindTexture(GL_TEXTURE_CUBE_MAP,unit==4?maps[6]:secondary);else if(unit==12)glBindTexture(GL_TEXTURE_2D_ARRAY,maps[7]);else if(unit==13||unit==14)glBindTexture(GL_TEXTURE_3D,maps[8]);else glBindTexture(GL_TEXTURE_2D,unit==1?maps[1]:unit==2?maps[2]:unit==6?maps[3]:unit==7?maps[4]:unit==11?maps[5]:maps[0]);}
    glViewport(0,0,side,side);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    unsigned terrain_frames=0,foliage_frames=0;
    const std::array<float,13> distances={.01f,45,110,std::nextafter(155.f,0.f),155,std::nextafter(155.f,INFINITY),245,std::nextafter(420.f,0.f),420,std::nextafter(420.f,INFINITY),650,1200,6000};
    for(float distance:distances)for(float span:{0.f,9.f})for(float wet:{0.f,.6f})for(unsigned shape=0;shape<2;shape++){
        for(GLuint p:{terrain,terrain_old}){glUseProgram(p);v3(p,"tRelative",distance,0,0);f(p,"tSpan",span);f(p,"uWetness",wet);v3(p,"tNormal",shape?.8f:0,shape?.6f:1,0);v3(p,"tWorld",1000,shape?900:20,900);}
        assert(same(frame(terrain),frame(terrain_old)));terrain_frames++;
    }
    // Includes constant and across-quad sign changes, normal-map hemisphere
    // disagreements, exact horizon and immediate finite neighboring normals.
    const std::array<float,7> facing={-.8f,-.1f,std::nextafter(0.f,-INFINITY),0,std::nextafter(0.f,INFINITY),.1f,.8f};
    for(float ny:facing)for(bool normal:{false,true})for(bool soft:{false,true})for(int leaf:{0,1,3,5})for(float span:{0.f,.8f}){
        for(GLuint p:{foliage,foliage_old}){glUseProgram(p);v3(p,"tRelative",20,-4,0);v3(p,"tWorld",1005,20,900);v3(p,"tNormal",.8f,ny,.2f);f(p,"tNormalSpan",span);i(p,"uHasNormal",normal);i(p,"uSoftShadows",soft);i(p,"uVegetation",leaf);f(p,"uAlphaCutoff",leaf?.4f:0);i(p,"uLocalProbeCount",2);}
        assert(same(frame(foliage),frame(foliage_old)));foliage_frames++;
    }
    // Full secondary-only domain, primary-only, overlap, both outside, and
    // explicit zero camera share. No black/identical cubes can mask mistakes.
    for(float wx:{990.f,998.f,1002.f,1007.f,1017.f,1022.f})for(float primary:{0.f,.3f})for(float rough:{.25f,1.f}){
        for(GLuint p:{foliage,foliage_old}){glUseProgram(p);v3(p,"tWorld",wx,20,900);v3(p,"tNormal",0,1,0);f(p,"tNormalSpan",0);i(p,"uHasNormal",0);i(p,"uVegetation",0);f(p,"uAlphaCutoff",0);v2(p,"uLocalProbeWeights",primary,.4f);f(p,"uRoughness",rough);}
        assert(same(frame(foliage),frame(foliage_old)));foliage_frames++;
    }
    // Exact horizon inputs: N=(0,-1,0), V=(0,0,1), so dot(N,sun)
    // and dot(V,-sun) are the supplied Y/Z values without normal-rounding
    // ambiguity. Adjacent representable values exercise both guard boundaries.
    auto leaf_setup=[&](GLuint p,float nd,float vd,int kind,bool soft,float sunlight){
        glUseProgram(p);v3(p,"tWorld",1005,20,900);v3(p,"tRelative",0,0,-20);
        v3(p,"tNormal",0,-1,0);f(p,"tNormalSpan",0);f(p,"tSpan",0);
        i(p,"uHasNormal",0);i(p,"uVegetation",kind==7?0:kind);i(p,"uGrass",kind==7);
        f(p,"uAlphaCutoff",.4f);i(p,"uSoftShadows",soft);i(p,"uLocalProbeCount",0);
        i(p,"uLodLevel",-1);i(p,"uVillageSkyEnabled",0);v3(p,"uVillageBounceAlbedo",0,0,0);
        f(p,"uRoughness",.7f);f(p,"uMetallic",0);f(p,"uWetness",0);f(p,"uPixelScale",600);
        v3(p,"uSun",std::sqrt(std::max(0.f,1-nd*nd-vd*vd)),-nd,-vd);
        v3(p,"uSunColor",sunlight,sunlight*.9f,sunlight*.8f);
    };
    unsigned leaf_boundary_frames=0,zero_sun_frames=0;
    const std::array<float,8> nds={-.9f,-.45001f,std::nextafter(-.45f,-INFINITY),-.45f,std::nextafter(-.45f,INFINITY),-.44999f,-.1f,.1f};
    const std::array<float,6> vds={-.3f,std::nextafter(0.f,-INFINITY),0,std::nextafter(0.f,INFINITY),.001f,.3f};
    for(float nd:nds)for(float vd:vds)for(int kind:{1,3,5,6,7})for(bool soft:{false,true}){
        for(GLuint p:{foliage,foliage_old})leaf_setup(p,nd,vd,kind,soft,.7f);
        const auto old=frame(foliage_old);assert(same(frame(foliage),old));leaf_boundary_frames++;
    }
    for(float sunlight:{0.f,std::nextafter(0.f,INFINITY),1e-8f,.0005f,.7f})for(int kind:{0,1,3,5,6,7})for(bool soft:{false,true}){
        for(GLuint p:{foliage,foliage_old})leaf_setup(p,.1f,.3f,kind,soft,sunlight);
        const auto old=frame(foliage_old);assert(same(frame(foliage),old));zero_sun_frames++;
    }
    for(GLuint p:{foliage,wrong_through})leaf_setup(p,-.8f,.3f,3,true,.7f);
    const float through_negative=delta(frame(foliage),frame(wrong_through));assert(through_negative>1e-6f);
    for(GLuint p:{foliage,wrong_leaf_boundary})leaf_setup(p,-.445f,-.3f,3,true,.7f);
    const float boundary_negative=delta(frame(foliage),frame(wrong_leaf_boundary));assert(boundary_negative>1e-6f);
    // Existing exact-zero guard also avoids only procedural cloud ALU. The
    // complete shader remains the oracle, including MRT bit patterns; positive
    // wrapped/transmitted light and opaque village bounce retain cloud shade.
    unsigned cloud_frames=0,far_card_frames=0,opaque_bounce_frames=0;
    for(float cloud:{0.f,.62f,.96f,1.f})for(float time:{0.f,7.3f,99.f})
        for(float wet:{0.f,.6f})for(float distance:{20.f,300.f,2000.f}){
            for(GLuint p:{foliage,foliage_old}){
                leaf_setup(p,-.8f,-.3f,4,true,.7f);f(p,"uCloud",cloud);f(p,"uTime",time);f(p,"uWetness",wet);
                v3(p,"tRelative",distance,-4,0);f(p,"uPixelScale",3000);i(p,"uLodLevel",distance>1450?3:-1);
            }
            assert(same(frame(foliage),frame(foliage_old)));far_card_frames++;
        }
    float cloud_negative=0,far_cloud_negative=0,opaque_cloud_negative=0;
    for(unsigned spot=0;spot<16;spot++){
        const float wx=1005+137.f*spot,wz=900+97.f*spot;
        for(float nd:{-.8f,-.45f,-.1f})for(float vd:{-.3f,0.f,.3f}){
            for(GLuint p:{foliage,foliage_old}){
                leaf_setup(p,nd,vd,3,true,.7f);v3(p,"tWorld",wx,20,wz);f(p,"uCloud",1);f(p,"uTime",0);i(p,"uLodLevel",-1);
            }
            assert(same(frame(foliage),frame(foliage_old)));cloud_frames++;
        }
        for(GLuint p:{foliage,wrong_cloud}){
            leaf_setup(p,-.1f,-.3f,3,true,.7f);v3(p,"tWorld",wx,20,wz);f(p,"uCloud",1);f(p,"uTime",0);i(p,"uLodLevel",-1);
        }
        cloud_negative=std::max(cloud_negative,delta(frame(foliage),frame(wrong_cloud)));
        for(GLuint p:{foliage,wrong_far_cloud}){
            leaf_setup(p,-.8f,-.3f,4,true,.7f);v3(p,"tWorld",wx,20,wz);f(p,"uCloud",1);f(p,"uTime",0);i(p,"uLodLevel",-1);
        }
        far_cloud_negative=std::max(far_cloud_negative,delta(frame(foliage),frame(wrong_far_cloud)));
        for(GLuint p:{foliage,foliage_old,wrong_opaque_cloud}){
            leaf_setup(p,-.8f,-.3f,0,true,.7f);v3(p,"tWorld",wx,20,wz);f(p,"uCloud",1);f(p,"uTime",0);i(p,"uLodLevel",-1);
            i(p,"uVillageSkyEnabled",1);v4(p,"uVillageSkyBounds",wx-30,wz-30,1.f/60,1.f/60);
            v2(p,"uVillageSkyHeights",20,20);v3(p,"uVillageBounceAlbedo",.2f,.15f,.1f);
        }
        const auto opaque=frame(foliage);assert(same(opaque,frame(foliage_old)));opaque_bounce_frames++;
        opaque_cloud_negative=std::max(opaque_cloud_negative,delta(opaque,frame(wrong_opaque_cloud)));
    }
    assert(cloud_negative>1e-6f&&far_cloud_negative>1e-6f&&opaque_cloud_negative>1e-6f);
    // Restore all fields changed by this extension before the older controls.
    for(GLuint p:{foliage,foliage_old,wrong_probe,wrong_sun,wrong_leaf}){glUseProgram(p);i(p,"uGrass",0);i(p,"uLodLevel",-1);i(p,"uVillageSkyEnabled",0);f(p,"uCloud",.96f);f(p,"uTime",7.3f);v3(p,"uSun",0,1,0);v3(p,"uSunColor",.7f,.65f,.6f);}
    // Active-feature controls: intentionally premature/no-primary/no-leaf
    // approximations must each visibly fail against the complete production FS.
    for(GLuint p:{terrain,wrong_noise}){glUseProgram(p);v3(p,"tRelative",300,0,0);f(p,"tSpan",0);f(p,"uWetness",0);v3(p,"tWorld",1000,20,900);v3(p,"tNormal",0,1,0);}
    const float noise_negative=delta(frame(terrain),frame(wrong_noise));assert(noise_negative>1e-6f);
    auto negative=[&](GLuint wrong,bool leaf){for(GLuint p:{foliage,wrong}){glUseProgram(p);v3(p,"tWorld",998,20,900);v3(p,"tRelative",20,-4,0);v3(p,"tNormal",.8f,leaf?-.1f:.1f,.2f);f(p,"tNormalSpan",0);i(p,"uHasNormal",0);i(p,"uVegetation",leaf?3:0);f(p,"uAlphaCutoff",leaf?.4f:0);i(p,"uSoftShadows",1);i(p,"uLocalProbeCount",wrong==wrong_probe?1:0);v2(p,"uLocalProbeWeights",.3f,.4f);f(p,"uRoughness",.7f);}return delta(frame(foliage),frame(wrong));};
    const float probe_negative=negative(wrong_probe,false),sun_negative=negative(wrong_sun,false),leaf_negative=negative(wrong_leaf,true);
    assert(probe_negative>1e-6f&&sun_negative>1e-6f&&leaf_negative>1e-6f);
    assert(glGetError()==GL_NO_ERROR);glDeleteTextures(1,&secondary);glDeleteTextures(maps.size(),maps.data());glDeleteTextures(outputs.size(),outputs.data());glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);
    for(GLuint p:{terrain,terrain_old,foliage,foliage_old,wrong_noise,wrong_probe,wrong_sun,wrong_leaf,wrong_through,wrong_leaf_boundary,wrong_cloud,wrong_opaque_cloud,wrong_far_cloud})glDeleteProgram(p);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS leaf boundary%u night/tiny-sun%u frames; wrapped-boundary/transmission negative deltas %.8f %.8f\n",leaf_boundary_frames,zero_sun_frames,boundary_negative,through_negative);
    std::printf("PASS exact-zero cloud work: leaf%u far-card%u opaque-bounce%u complete HDR/depth/MRT frames bitexact; wrong leaf/far-card/opaque-bounce cloud controls %.8f %.8f %.8f\n",cloud_frames,far_card_frames,opaque_bounce_frames,cloud_negative,far_cloud_negative,opaque_cloud_negative);
    std::printf("PASS complete production FS parity: terrain%u foliage%u HDR/depth/MRT frames bitexact; noise/probe/positive-sun/leaf negative deltas %.8f %.8f %.8f %.8f\n",terrain_frames,foliage_frames,noise_negative,probe_negative,sun_negative,leaf_negative);
}
