// Probe the exact production room-return function in linear floating-point
// render targets. Software/offscreen by default; this makes no FPS claim.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_tavern_lighting.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace {
using V=std::array<float,3>;
using Pixel=std::array<float,4>;
std::string read(const char *path) {
    std::ifstream file(path);assert(file);std::ostringstream text;text<<file.rdbuf();return text.str();
}
std::string function(const std::string &text,const char *signature) {
    const auto begin=text.find(signature);assert(begin!=std::string::npos);
    auto end=text.find('{',begin);assert(end!=std::string::npos);unsigned depth=1;
    for(++end;end<text.size()&&depth;end++){if(text[end]=='{')depth++;else if(text[end]=='}')depth--;}
    assert(!depth);return text.substr(begin,end-begin);
}
GLuint compile(GLenum kind,const std::string &source) {
    GLuint shader=glCreateShader(kind);const char *text=source.c_str();glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
    GLint okay=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&okay);
    if(!okay){char log[8192]={};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(okay);return shader;
}
GLuint program() {
    const std::string production=read("shaders/world_foliage.fs.glsl");
    // Confirm the complete shader sends this diffuse response through its
    // ordinary AO/MRT path, rather than treating it as unoccluded emission.
    assert(production.find("indirect+=base.rgb*(1.-metal)*ao*tavern_diffuse_return();")!=std::string::npos);
    assert(production.find("IndirectColor=vec4(indirect*(1.-fogAmount),0.);")!=std::string::npos);
    std::string fragment=R"GLSL(#version 330 core
uniform bool uTavernReturnEnabled;
uniform vec4 uTavernReturnBounds;
uniform vec2 uTavernReturnHeights,uWorldAnchor;
uniform vec3 uTavernReturnRoof,uTavernReturnIrradiance,tPosition,tBase,uSkyLight;
uniform float tMetal,tAO;
vec3 vPos;
layout(location=0) out vec4 light;
layout(location=1) out vec4 diffuse;
)GLSL";
    fragment+=function(production,"vec3 tavern_diffuse_return(");
    fragment+="\nvoid main(){vPos=tPosition;vec3 incident=tavern_diffuse_return();light=vec4(incident,.8125);diffuse=vec4(tBase*(1.-tMetal)*tAO*incident,.8125);}";
    const char *vertex="#version 330 core\nvoid main(){vec2 q=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(q*2.-1.,0,1);}";
    GLuint vs=compile(GL_VERTEX_SHADER,vertex),fs=compile(GL_FRAGMENT_SHADER,fragment),p=glCreateProgram();
    glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);
    if(!okay){char log[8192]={};glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(okay);
    glDeleteShader(vs);glDeleteShader(fs);return p;
}
void vector(GLuint p,const char *name,V value){glUniform3fv(glGetUniformLocation(p,name),1,value.data());}
void scalar(GLuint p,const char *name,float value){glUniform1f(glGetUniformLocation(p,name),value);}
void enabled(GLuint p,bool value){glUniform1i(glGetUniformLocation(p,"uTavernReturnEnabled"),value);}
void upload(GLuint p,const WorldTavernLighting &light,V anchor={0,0,0}) {
    glUseProgram(p);enabled(p,true);
    glUniform4fv(glGetUniformLocation(p,"uTavernReturnBounds"),1,light.bounds.data());
    glUniform2fv(glGetUniformLocation(p,"uTavernReturnHeights"),1,light.heights.data());
    vector(p,"uTavernReturnRoof",light.roof);vector(p,"uTavernReturnIrradiance",light.irradiance);
    glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),anchor[0],anchor[2]);
    vector(p,"uSkyLight",{0,0,0});
    vector(p,"tBase",{.5f,.25f,.125f});scalar(p,"tMetal",0);scalar(p,"tAO",1);
}
std::array<Pixel,2> draw(GLuint p,V position) {
    vector(p,"tPosition",position);glDrawArrays(GL_TRIANGLES,0,3);std::array<Pixel,2> result;
    for(unsigned i=0;i<2;i++){glReadBuffer(GL_COLOR_ATTACHMENT0+i);glReadPixels(4,4,1,1,GL_RGBA,GL_FLOAT,result[i].data());}
    for(const auto &pixel:result)for(float value:pixel)assert(std::isfinite(value));
    assert(glGetError()==GL_NO_ERROR);return result;
}
float smooth(float width,float distance) {
    const float t=std::clamp(distance/width,0.f,1.f);return t*t*(3.f-2.f*t);
}
float reference(const WorldTavernLighting &light,V world) {
    const float edge=std::min({world[0]-light.bounds[0],world[2]-light.bounds[1],light.bounds[2]-world[0],light.bounds[3]-world[2]});
    const float vertical=std::min(world[1]-light.heights[0],light.heights[1]-world[1]);
    const float roof=light.roof[1]-light.roof[2]*std::abs(world[2]-light.roof[0])-world[1];
    if(edge<=0||vertical<=0||roof<=0)return 0;
    const float edge_width=.15f+(.015f-.15f)*smooth(.4f,world[1]-28.f);
    return smooth(edge_width,edge)*smooth(.15f,vertical)*smooth(.05f,roof);
}
void close(const Pixel &pixel,V expected) {
    for(unsigned i=0;i<3;i++) {
        if(std::abs(pixel[i]-expected[i])>2.e-6f)std::fprintf(stderr,"channel%u actual%.9g expected%.9g\n",i,pixel[i],expected[i]);
        assert(std::abs(pixel[i]-expected[i])<=2.e-6f);
    }
    assert(pixel[3]==.8125f);
}
void check(GLuint p,double ox,double oz,V anchor) {
    WorldTavernLighting light;assert(world_tavern_lighting_build(ox,oz,light));upload(p,light,anchor);
    auto witness=[&](float x,float y,float z) {
        V position={float(ox+x)-anchor[0],y,float(oz+z)-anchor[2]};
        // Match the renderer's representable world reconstruction before the
        // independent CPU clipping/feather reference, including a10km origin.
        V world={position[0]+anchor[0],position[1],position[2]+anchor[2]};
        const float weight=reference(light,world);V expected=light.irradiance;
        for(auto &value:expected)value*=weight;
        auto result=draw(p,position);close(result[0],expected);
        for(unsigned i=0;i<3;i++)expected[i]*=std::array<float,3>{.5f,.25f,.125f}[i];
        close(result[1],expected);return weight;
    };
    assert(witness(-145,24.015f,-82)==1);
    assert(witness(-154.825f,26,-82)==1);assert(witness(-135.175f,26,-82)==1);
    assert(witness(-145,26,-89.825f)==1);assert(witness(-145,26,-74.175f)==1);
    assert(witness(-145,25.75f,-82)==1); // standing and seated residents
    // Actual encoded upper-gable triangle centroids, independently checked
    // against both source LODs; the inward panel is only20mm inside the box.
    assert(witness(-154.98f,29.6666667f,-82)==1);
    assert(witness(-135.02f,29.6666667f,-82)==1);
    for(unsigned i=0;i<=80;i++) {
        const float z=-90.f+float(i)*.2f;
        const float roof=32.2f-(3.8f/8.6f)*std::abs(z+82.f);
        if(i>1&&i<79)assert(witness(-145,roof-.16f,z)==1);
        assert(witness(-145,roof,z)==0); // actual exterior roof top
    }
    for(V point:std::array<V,8>{{{-155.175f,26,-82},{-134.825f,26,-82},
        {-145,26,-90.175f},{-145,26,-73.825f},{-145,23.8f,-82},
        {-145,33,-82},{0,25.75f,-145},{-145,25.75f,-70}}})
        assert(witness(point[0],point[1],point[2])==0);
    float previous=1;
    for(unsigned i=0;i<=2000;i++) {
        const float z=-74.3f+float(i)*.0003f;
        const float weight=witness(-145,25.75f,z);assert(weight<=previous+1.e-6f);previous=weight;
    }
    assert(previous==0);
    previous=0;
    for(unsigned i=0;i<=2000;i++) {
        const float height=27.8f+float(i)*.0004f;
        const float weight=witness(-154.98f,height,-82);
        assert(weight+1.e-6f>=previous);if(i)assert(weight-previous<.01f);previous=weight;
        assert(witness(-155.175f,height,-82)==0); // taper never reaches outside
    }
    assert(previous==1);
    const V center={float(ox-145)-anchor[0],25.75f,float(oz-82)-anchor[2]};
    scalar(p,"tAO",.25f);auto occluded=draw(p,center);
    V expected=light.irradiance;
    for(unsigned i=0;i<3;i++)expected[i]*=std::array<float,3>{.5f,.25f,.125f}[i]*.25f;
    close(occluded[1],expected);
    scalar(p,"tMetal",1);close(draw(p,center)[1],{0,0,0});
    scalar(p,"tMetal",0);scalar(p,"tAO",1);vector(p,"uSkyLight",{.2f,.3f,.4f});
    const V windows[]={{-6.5f,26.2f,8},{6.5f,26.2f,8},{.8f,26.2f,-8},
        {-10,26.2f,-3},{-10,26.2f,3},{10,26.2f,-3},{10,26.2f,3}};
    const V local={center[0]+anchor[0]-(light.bounds[0]+light.bounds[2])*.5f,
        center[1],center[2]+anchor[2]-(light.bounds[1]+light.bounds[3])*.5f};
    float window_sum=0;
    for(const auto &w:windows){float d2=0;for(unsigned j=0;j<3;j++){const float d=local[j]-w[j];d2+=d*d;}window_sum+=.08f/(1+.18f*d2);}
    expected=light.irradiance;for(unsigned j=0;j<3;j++)expected[j]+=std::array<float,3>{.2f,.3f,.4f}[j]*window_sum;
    close(draw(p,center)[0],expected);
    const V exterior={float(ox-155.175)-anchor[0],26,float(oz-82)-anchor[2]};
    close(draw(p,exterior)[0],{0,0,0}); // window return cannot leak outside
    enabled(p,false);const auto disabled=draw(p,center);close(disabled[0],{0,0,0});close(disabled[1],{0,0,0});
}
}

int main() {
    WorldTavernLighting light;assert(world_tavern_lighting_build(-1310.4,-10207.585,light));const auto before=light;
    for(double invalid:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),100800.01,-100800.01}) {
        assert(!world_tavern_lighting_build(invalid,0,light));assert(!std::memcmp(&light,&before,sizeof(light)));
        assert(!world_tavern_lighting_build(0,invalid,light));assert(!std::memcmp(&light,&before,sizeof(light)));
    }
    const V expected={float(4.*82./1044.*.38),float(4.*82./1044.*.32*.47),float(4.*82./1044.*.25*.12)};
    for(unsigned i=0;i<3;i++)assert(std::abs(light.irradiance[i]-expected[i])<1.e-8f);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL")){setenv("SDL_VIDEODRIVER","offscreen",1);setenv("LIBGL_ALWAYS_SOFTWARE","1",1);}
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Tavern diffuse return regression",0,0,8,8,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Renderer: %s\n",glGetString(GL_RENDERER));GLuint fbo=0,images[2]={},vao=0;
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(2,images);
    for(unsigned i=0;i<2;i++) {
        glBindTexture(GL_TEXTURE_2D,images[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,8,8,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,images[i],0);
    }
    const GLenum attachments[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,attachments);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glViewport(0,0,8,8);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_DITHER);
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);GLuint p=program();
    check(p,0,0,{0,0,0});check(p,-1310.4,-10207.585,{-1430.4f,0,-10287.585f});
    glDeleteProgram(p);glDeleteVertexArrays(1,&vao);glDeleteTextures(2,images);glDeleteFramebuffers(1,&fbo);assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS tavern return: finite/atomic config; linear single-bounce(%.8f,%.8f,%.8f); exact production function, floor/wall/gable/underside full, exterior roof/walls zero,2001-sample smooth door and height-feather crossings in local and10km rebased coordinates, diffuse AO/metal controls, disabled zero; no new images/passes in production\n",light.irradiance[0],light.irradiance[1],light.irradiance[2]);
}
