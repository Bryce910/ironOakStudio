/* Real production foliage MRT pixel equivalence and neutral sampler ownership.
 * Link with --wrap=glDrawElementsInstanced. Software GL by default; native opt-in
 * checks correctness only, never an FPS claim. No project asset is rewritten. */
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include "gl_helpers.h"
#include "engine/world_models.h"
#include "tests/fixtures/texture_cache_fixture.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
namespace {
constexpr int side=24;
constexpr unsigned units[7]={0,1,2,3,8,9,10};
using Image=std::array<std::vector<float>,2>;
struct Expected {bool maps[4]={};int facade=0;};
Expected expected;
bool legacy_zero=false,negative_control=false;
GLuint observed_program=0;
GLuint observed_shadow=0,shadow_sentinel=0;
std::vector<GLuint> observed_detail_samplers;
unsigned observed_draws=0;
unsigned observed_shadow_draws=0;
std::array<GLint,7> shadow_bindings={};
std::array<GLuint,3> neutral_ids={};
void empty(){const auto s=texture_cache_stats();assert(!s.entries&&!s.references&&!s.source_bytes&&!s.unmanaged_uploads);}
void i(GLuint p,const char *name,int value){glUniform1i(glGetUniformLocation(p,name),value);}
void f(GLuint p,const char *name,float value){glUniform1f(glGetUniformLocation(p,name),value);}
void v3(GLuint p,const char *name,float x,float y,float z){glUniform3f(glGetUniformLocation(p,name),x,y,z);}
int integer(GLuint p,const char *name){GLint value=-1;glGetUniformiv(p,glGetUniformLocation(p,name),&value);return value;}
std::array<GLint,7> bindings(){
    GLint active=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);std::array<GLint,7> result={};
    for(unsigned k=0;k<7;k++){glActiveTexture(GL_TEXTURE0+units[k]);glGetIntegerv(GL_TEXTURE_BINDING_2D,&result[k]);}
    glActiveTexture(GLenum(active));return result;
}
bool absent(unsigned k){return k<4?!expected.maps[k]:(expected.facade==0||(expected.facade==1&&k==6));}
unsigned neutral_role(unsigned k){return k==1||k==4?2:(k==2||k==5?1:0);}
void sampler_contract(){
    const char *flags[]={"uHasBase","uHasNormal","uHasARM","uHasEmissive"};
    for(unsigned k=0;k<4;k++)assert(integer(observed_program,flags[k])==int(expected.maps[k]));
    assert(integer(observed_program,"uFacadeMode")==expected.facade);
    const auto ids=bindings();
    for(unsigned k=0;k<7;k++){
        assert(ids[k]>0&&glIsTexture(GLuint(ids[k])));glActiveTexture(GL_TEXTURE0+units[k]);
        const bool srgb=k==0||k==3||k==6||((k==4||k==5)&&expected.facade==1);
        GLint sampler=0;glGetIntegeri_v(GL_SAMPLER_BINDING,units[k],&sampler);
        const bool supported=SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic");
        float maximum=1;if(supported)glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&maximum);
        const bool detailed=maximum>4&&expected.facade>0&&!absent(k)&&(k==0||(expected.facade==1&&k==4)||(expected.facade==2&&k==6));
        if(detailed){if(!sampler)std::fprintf(stderr,"Missing detail sampler at slot%u unit%u facade%d\n",k,units[k],expected.facade);assert(sampler>0&&glIsSampler(GLuint(sampler)));float quality=0;glGetSamplerParameterfv(GLuint(sampler),GL_TEXTURE_MAX_ANISOTROPY_EXT,&quality);assert(std::abs(quality-std::min(8.f,maximum))<.001f);
            for(GLenum parameter:{GL_TEXTURE_MIN_FILTER,GL_TEXTURE_MAG_FILTER,GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T}){GLint image=0,filtered=0;glGetTexParameteriv(GL_TEXTURE_2D,parameter,&image);glGetSamplerParameteriv(GLuint(sampler),parameter,&filtered);assert(image==filtered);}
            observed_detail_samplers.push_back(GLuint(sampler));
        }else assert(sampler==0);
        GLint format=0,w=0,h=0,filter=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
        glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&filter);
        // Existing imported linear images use the GL_RGBA request token;
        // verify actual channel precision/type too. New neutrals require RGBA8.
        const bool format_ok=srgb?format==GL_SRGB8_ALPHA8:
            (format==GL_RGBA8||(!absent(k)&&format==GL_RGBA));
        for(GLenum channel:{GL_TEXTURE_RED_SIZE,GL_TEXTURE_GREEN_SIZE,GL_TEXTURE_BLUE_SIZE,GL_TEXTURE_ALPHA_SIZE}){
            GLint bits=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,channel,&bits);assert(bits==8);
        }
        if(!format_ok||w<=0||h<=0||filter!=GL_LINEAR_MIPMAP_LINEAR)
            std::fprintf(stderr,"Sampler contract slot%u unit%u texture%d format0x%x expected0x%x size%dx%d filter0x%x facade%d missing%d\n",k,units[k],ids[k],format,srgb?GL_SRGB8_ALPHA8:GL_RGBA8,w,h,filter,expected.facade,absent(k));
        assert(format_ok&&w>0&&h>0&&filter==GL_LINEAR_MIPMAP_LINEAR);
        for(int level=0,x=w,y=h;;level++,x=std::max(1,x/2),y=std::max(1,y/2)){
            GLint mw=0,mh=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,level,GL_TEXTURE_WIDTH,&mw);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,level,GL_TEXTURE_HEIGHT,&mh);assert(mw==x&&mh==y);
            if(x==1&&y==1)break;
        }
        if(absent(k)){
            assert(w==1&&h==1);GLint last=-1;glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,&last);assert(last==0);
            const unsigned role=(expected.facade==1&&k>=4)?0:neutral_role(k);
            const std::array<unsigned char,4> wanted=role==2?std::array<unsigned char,4>{128,128,255,255}:std::array<unsigned char,4>{255,255,255,255};
            std::array<unsigned char,4> actual={};glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());assert(actual==wanted);
            if(neutral_ids[role])assert(neutral_ids[role]==GLuint(ids[k]));else neutral_ids[role]=GLuint(ids[k]);
        }
    }
    glActiveTexture(GL_TEXTURE0);
}
std::string source(const char *path){std::ifstream file(path);assert(file);std::ostringstream s;s<<file.rdbuf();return s.str();}
GLuint program(bool shadow=false){
    const char *vs=R"GLSL(#version 330 core
layout(location=0)in vec3 aPos;layout(location=7)in float aPaint;
uniform vec3 tCentre;uniform float tScale;
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;out vec2 vUV;
flat out float vSeed,vCoveragePhase,vScale,vRootHeight,vInstanceFade;
out float vFacadePaint;
flat out vec3 vRootRelative;
void main(){gl_Position=vec4((aPos-tCentre).xy*tScale,0,1);
 vPos=vec3(0,24,0);vNormal=vec3(0,0,1);vRelative=vec3(0,0,-3);
 vTangent=vec3(1,0,0);vBitangent=vec3(0,1,0);vUV=vec2(.37,.53);
 vSeed=.5;vCoveragePhase=0.;vScale=1.;vRootHeight=24.;vInstanceFade=1.;
 vFacadePaint=aPaint;vRootRelative=vec3(0,0,3);}
)GLSL";
    const auto fs=source(shadow?"shaders/world_foliage_shadow.fs.glsl":"shaders/world_foliage.fs.glsl");const GLuint p=GLH_program(vs,fs.c_str());
    GLint linked=0;glGetProgramiv(p,GL_LINK_STATUS,&linked);assert(linked);glUseProgram(p);
    i(p,"uLocalProbe0",4);i(p,"uLocalProbe1",5);i(p,"uShadow",7);i(p,"uMacroHeight",11);
    i(p,"uVillageSkyMask",12);i(p,"uVillageSkyDirectional",13);i(p,"uVillageSkyUp",14);
    v3(p,"uSkyLight",.35f,.45f,.55f);v3(p,"uGroundLight",.1f,.12f,.14f);
    v3(p,"uAmbient",.2f,.2f,.2f);v3(p,"uSun",0,0,1);v3(p,"uSunColor",.8f,.7f,.6f);
    v3(p,"uFog",.1f,.15f,.2f);f(p,"uPixelScale",12);f(p,"uTreeHeight",1);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,identity);
    return p;
}
std::vector<float> depth_pixels(WorldModelAsset *asset,GLuint p,bool old=false,bool wrong=false){
    glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
    glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);legacy_zero=old;negative_control=wrong;
    const auto before=observed_shadow_draws;world_model_draw(asset,p);assert(observed_shadow_draws==before+1);
    legacy_zero=negative_control=false;std::vector<float> pixels(side*side);
    glReadPixels(0,0,side,side,GL_DEPTH_COMPONENT,GL_FLOAT,pixels.data());
    for(float value:pixels)assert(std::isfinite(value)&&value>=0&&value<=1);
    const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);glReadBuffer(GL_COLOR_ATTACHMENT0);glDisable(GL_DEPTH_TEST);
    assert(glGetError()==GL_NO_ERROR);return pixels;
}
void depth_equivalence(WorldModelAsset *asset,GLuint p){
    assert(glGetUniformLocation(p,"uNormal")==-1&&glGetUniformLocation(p,"uARM")==-1);
    float lo[3],hi[3];assert(world_model_bounds(asset,lo,hi));glUseProgram(p);
    v3(p,"tCentre",(lo[0]+hi[0])*.5f,(lo[1]+hi[1])*.5f,(lo[2]+hi[2])*.5f);f(p,"tScale",1.5f/std::max(hi[0]-lo[0],hi[1]-lo[1]));
    for(unsigned k=1;k<7;k++){glActiveTexture(GL_TEXTURE0+units[k]);glBindTexture(GL_TEXTURE_2D,shadow_sentinel);}
    shadow_bindings=bindings();const auto actual=depth_pixels(asset,p),legacy=depth_pixels(asset,p,true);
    assert(actual==legacy);assert(std::count_if(actual.begin(),actual.end(),[](float d){return d<1;})>30);
    const auto after=bindings();for(unsigned k=1;k<7;k++)assert(after[k]==shadow_bindings[k]);
    const auto rejected=depth_pixels(asset,p,false,true);assert(rejected!=actual);
    for(float d:rejected)assert(d==1);
}
std::filesystem::path synthetic(const std::filesystem::path &dir,bool maps){
    const float positions[]={-.8f,0,0,.8f,0,0,0,1.2f,0},normals[]={0,0,1,0,0,1,0,0,1},uv[]={.37f,.53f,.37f,.53f,.37f,.53f};
    const std::uint16_t indices[]={0,1,2};std::vector<unsigned char> binary;
    auto append=[&](const void *p,std::size_t n){while(binary.size()%4)binary.push_back(0);const auto at=binary.size();const auto *b=static_cast<const unsigned char *>(p);binary.insert(binary.end(),b,b+n);return at;};
    const std::size_t offsets[]={append(positions,sizeof positions),append(normals,sizeof normals),append(uv,sizeof uv),append(indices,sizeof indices),maps?append(texture_fixture_png,sizeof texture_fixture_png):0};
    const std::size_t sizes[]={sizeof positions,sizeof normals,sizeof uv,sizeof indices,sizeof texture_fixture_png};
    while(binary.size()%4)binary.push_back(0);
    std::ostringstream j;
    j<<"{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],\"buffers\":[{\"byteLength\":"<<binary.size()<<"}],\"bufferViews\":[";
    for(unsigned k=0;k<(maps?5u:4u);k++){if(k)j<<',';j<<"{\"buffer\":0,\"byteOffset\":"<<offsets[k]<<",\"byteLength\":"<<sizes[k]<<'}';}
    j<<"],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[-0.8,0,0],\"max\":[0.8,1.2,0]},{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],";
    if(maps)j<<"\"images\":[{\"bufferView\":4,\"mimeType\":\"image/png\"}],\"textures\":[{\"source\":0}],";
    j<<"\"materials\":[{\"emissiveFactor\":[0.1,0.2,0.3],\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.7,0.6,0.5,1],\"metallicFactor\":0.4,\"roughnessFactor\":0.7";
    if(maps)j<<",\"baseColorTexture\":{\"index\":0},\"metallicRoughnessTexture\":{\"index\":0}";
    j<<'}';if(maps)j<<",\"normalTexture\":{\"index\":0},\"emissiveTexture\":{\"index\":0}";
    j<<"}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3,\"material\":0}]}]}";
    std::string text=j.str();while(text.size()%4)text.push_back(' ');const auto path=dir/(maps?"mapped.glb":"plain.glb");
    const std::uint32_t header[]={0x46546c67,2,std::uint32_t(28+text.size()+binary.size()),std::uint32_t(text.size()),0x4e4f534a},chunk[]={std::uint32_t(binary.size()),0x004e4942};
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char *>(header),sizeof header);file.write(text.data(),std::streamsize(text.size()));
    file.write(reinterpret_cast<const char *>(chunk),sizeof chunk);file.write(reinterpret_cast<const char *>(binary.data()),std::streamsize(binary.size()));assert(file);return path;
}
Image draw(WorldModelAsset *asset,GLuint p,bool old=false,bool wrong=false){
    float lo[3],hi[3];assert(world_model_bounds(asset,lo,hi));glUseProgram(p);
    v3(p,"tCentre",(lo[0]+hi[0])*.5f,(lo[1]+hi[1])*.5f,(lo[2]+hi[2])*.5f);
    f(p,"tScale",1.5f/std::max(hi[0]-lo[0],hi[1]-lo[1]));
    const float clear[4]={-.25f,-.25f,-.25f,-.25f};glClearBufferfv(GL_COLOR,0,clear);glClearBufferfv(GL_COLOR,1,clear);
    legacy_zero=old;negative_control=wrong;const auto prior=observed_draws;world_model_draw(asset,p);assert(observed_draws==prior+1);
    legacy_zero=negative_control=false;Image result;
    for(unsigned attachment=0;attachment<2;attachment++){
        result[attachment].resize(side*side*4);glReadBuffer(GL_COLOR_ATTACHMENT0+attachment);
        glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,result[attachment].data());
        for(float value:result[attachment])assert(std::isfinite(value));
    }
    glReadBuffer(GL_COLOR_ATTACHMENT0);assert(glGetError()==GL_NO_ERROR);return result;
}
void equivalence(WorldModelAsset *asset,GLuint p){
    const auto actual=draw(asset,p),before=draw(asset,p,true);
    for(unsigned k=0;k<2;k++)assert(std::memcmp(actual[k].data(),before[k].data(),actual[k].size()*sizeof(float))==0);
    std::size_t covered=0;for(std::size_t k=3;k<actual[0].size();k+=4)covered+=actual[0][k]>=0;assert(covered>30);
}
void cycle(const std::filesystem::path &dir){
    empty();neutral_ids={};observed_detail_samplers.clear();
    auto *window=SDL_CreateWindow("Material sampler contracts",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Material sampler renderer: %s\n",glGetString(GL_RENDERER));
    GLuint p=program(),shadow=program(true);observed_program=p;observed_shadow=shadow;
    GLuint framebuffer=0,color[2]={},depth=0;glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glGenTextures(2,color);for(unsigned k=0;k<2;k++){glBindTexture(GL_TEXTURE_2D,color[k]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,side,side,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+k,GL_TEXTURE_2D,color[k],0);}
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,side,side);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    glGenTextures(1,&shadow_sentinel);glBindTexture(GL_TEXTURE_2D,shadow_sentinel);const unsigned char white[4]={255,255,255,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glViewport(0,0,side,side);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_FRAMEBUFFER_SRGB);glPixelStorei(GL_PACK_ALIGNMENT,1);
    const auto plain=synthetic(dir,false),mapped=synthetic(dir,true);const WorldModelInstance instance={0,0,0,0,1};
    // Hostile unpack/PBO state is preserved by first-use neutral uploads.
    GLuint pbo=0;glGenBuffers(1,&pbo);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);glBufferData(GL_PIXEL_UNPACK_BUFFER,1,nullptr,GL_STATIC_DRAW);
    glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,17);glPixelStorei(GL_UNPACK_SKIP_PIXELS,3);glPixelStorei(GL_UNPACK_SKIP_ROWS,2);
    auto *a=world_model_load(plain.c_str()),*b=world_model_load(plain.c_str());assert(a&&b);
    const GLenum unpack_names[]={GL_PIXEL_UNPACK_BUFFER_BINDING,GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS};
    const GLint unpack_values[]={GLint(pbo),8,17,3,2};
    for(unsigned k=0;k<5;k++){GLint value=0;glGetIntegerv(unpack_names[k],&value);assert(value==unpack_values[k]);}
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glDeleteBuffers(1,&pbo);
    auto stats=texture_cache_stats();assert(stats.entries==WORLD_MODEL_NEUTRAL_TEXTURE_REFS&&stats.references==2*WORLD_MODEL_NEUTRAL_TEXTURE_REFS&&stats.source_bytes==WORLD_MODEL_NEUTRAL_SOURCE_BYTES&&stats.unmanaged_uploads==0);
    expected={};assert(world_model_upload_instances(a,&instance,1)&&world_model_upload_instances(b,&instance,1));equivalence(a,p);equivalence(b,p);
    depth_equivalence(a,shadow);depth_equivalence(b,shadow);
    const auto correct=draw(a,p),wrong=draw(a,p,true,true);bool changed=false;
    for(std::size_t k=0;k<correct[0].size();k++)changed=changed||std::abs(correct[0][k]-wrong[0][k])>.0001f;
    assert(changed);
    const auto owned=neutral_ids;for(auto id:owned)assert(id&&glIsTexture(id));
    world_model_free(a);stats=texture_cache_stats();assert(stats.entries==WORLD_MODEL_NEUTRAL_TEXTURE_REFS&&stats.references==WORLD_MODEL_NEUTRAL_TEXTURE_REFS);
    for(auto id:owned)assert(glIsTexture(id));
    equivalence(b,p);world_model_free(b);empty();for(auto id:owned)assert(!glIsTexture(id));
    neutral_ids={};
    const std::array<std::filesystem::path,4> paths={mapped,"tests/fixtures/source_facade/wood_zero.glb","tests/fixtures/source_facade/wood_worn.glb","tests/fixtures/source_facade/plaster.glb"};
    for(unsigned k=0;k<paths.size();k++){
        neutral_ids={};auto *asset=world_model_load(paths[k].c_str());assert(asset&&world_model_upload_instances(asset,&instance,1));
        expected={};expected.maps[0]=expected.maps[1]=expected.maps[2]=true;expected.maps[3]=k==0;expected.facade=k==0?0:(k==3?2:1);
        equivalence(asset,p);depth_equivalence(asset,shadow);world_model_free(asset);empty();
    }
    // Rejected load must release its already acquired neutral references too.
    assert(!world_model_load("tests/fixtures/source_facade/bad_paint.glb"));empty();
    for(GLuint sampler:observed_detail_samplers)assert(!glIsSampler(sampler));
    observed_program=observed_shadow=0;glDeleteProgram(p);glDeleteProgram(shadow);glDeleteTextures(2,color);glDeleteTextures(1,&shadow_sentinel);
    glDeleteRenderbuffers(1,&depth);glDeleteFramebuffers(1,&framebuffer);assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);
}
}
extern "C" void __real_glDrawElementsInstanced(GLenum,GLsizei,GLenum,const void *,GLsizei);
extern "C" void __wrap_glDrawElementsInstanced(GLenum mode,GLsizei count,GLenum type,const void *indices,GLsizei instances){
    GLint p=0;glGetIntegerv(GL_CURRENT_PROGRAM,&p);
    if(GLuint(p)==observed_shadow){
        const auto actual=bindings();for(unsigned k=1;k<7;k++)assert(actual[k]==shadow_bindings[k]);observed_shadow_draws++;
        if(legacy_zero)for(unsigned k=1;k<7;k++){glActiveTexture(GL_TEXTURE0+units[k]);glBindTexture(GL_TEXTURE_2D,0);}
        float cutoff=0;glGetUniformfv(observed_shadow,glGetUniformLocation(observed_shadow,"uAlphaCutoff"),&cutoff);
        if(negative_control)f(observed_shadow,"uAlphaCutoff",2);
        __real_glDrawElementsInstanced(mode,count,type,indices,instances);
        if(negative_control)f(observed_shadow,"uAlphaCutoff",cutoff);
        if(legacy_zero)for(unsigned k=1;k<7;k++){glActiveTexture(GL_TEXTURE0+units[k]);glBindTexture(GL_TEXTURE_2D,GLuint(actual[k]));}
        return;
    }
    if(GLuint(p)!=observed_program){__real_glDrawElementsInstanced(mode,count,type,indices,instances);return;}
    sampler_contract();observed_draws++;const auto ids=bindings();
    if(legacy_zero)for(unsigned k=0;k<7;k++)if(absent(k)){glActiveTexture(GL_TEXTURE0+units[k]);glBindTexture(GL_TEXTURE_2D,0);}
    if(negative_control)i(observed_program,"uHasBase",1);
    __real_glDrawElementsInstanced(mode,count,type,indices,instances);
    if(negative_control)i(observed_program,"uHasBase",int(expected.maps[0]));
    if(legacy_zero)for(unsigned k=0;k<7;k++)if(absent(k)){glActiveTexture(GL_TEXTURE0+units[k]);glBindTexture(GL_TEXTURE_2D,GLuint(ids[k]));}
}
int main(){
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL")){SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_setenv("LIBGL_ALWAYS_SOFTWARE","1",1);}
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    const auto dir=std::filesystem::temp_directory_path()/("valwood-neutral-samplers-"+std::to_string(getpid()));assert(std::filesystem::create_directory(dir));
    cycle(dir);cycle(dir);texture_cache_shutdown();std::filesystem::remove_all(dir);SDL_Quit();
    std::printf("PASS shared-image facade8x samplers (live filters/source addressing, reset and deletion) and complete neutral material samplers: production foliage twoRGBA32F MRT attachments bit-exact vs legacy absent-texture0; source flags unchanged; sRGB color/emission/height andlinear ARM/normal formats/fullmips; source facade modes0/1/2; shared3keys24sourcebytes12GPUbytes/3refsperasset; hostile unpack state; last-owner GL deletion andfreshcontext recreation; changed-hasBase negativecontrol; %u actualcolor draws; %u production shadow draws retain inactive normal/ARM/emissive/aux bindings with bit-exact depth anddiscard negativecontrol\n",observed_draws,observed_shadow_draws);
}
