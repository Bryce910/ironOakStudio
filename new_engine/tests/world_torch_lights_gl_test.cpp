/* Exact production torch/BRDF functions in controlled probes. The complete
 *16-sampler programs are compiled separately by world_lighting_gl_test.
 * Default is software/offscreen; this fixture makes no native FPS claim. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_torch_lights.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
namespace {
using V=std::array<double,3>;
using Pixel=std::array<float,4>;
constexpr Pixel legacy={.03125f,.0625f,.125f,.8125f};
double dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V normal(V v){const double d=std::sqrt(dot(v,v));for(auto &a:v)a/=d;return v;}
std::string read(const char *path){std::ifstream f(path);assert(f);std::ostringstream out;out<<f.rdbuf();return out.str();}
std::string function(const std::string &text,const char *signature) {
    const auto begin=text.find(signature);assert(begin!=std::string::npos);
    std::size_t end=text.find('{',begin);assert(end!=std::string::npos);unsigned depth=1;
    for(++end;end<text.size()&&depth;end++){if(text[end]=='{')depth++;else if(text[end]=='}')depth--;}
    assert(!depth);return text.substr(begin,end-begin)+"\n";
}
GLuint shader(GLenum kind,const std::string &text) {
    GLuint s=glCreateShader(kind);const char *p=text.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);
    GLint okay=0;glGetShaderiv(s,GL_COMPILE_STATUS,&okay);if(!okay){char log[8192]={};glGetShaderInfoLog(s,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(okay);return s;
}
GLuint probe(const std::string &text,int kind) {
    std::string fs=R"GLSL(#version 330 core
uniform int uTorchCount,tMode;
uniform vec4 uTorchPositionRadius[8],uTorchColorIntensity[8],uTorchWallPlane[8],uTorchClipBounds[8];
uniform vec2 uTorchClipHeights[8],uWorldAnchor;
uniform vec3 uTorchClusterMin,uTorchClusterMax,tWorld,tNormal,tView,tBase;
uniform float tMetal,tRough,uWetness;
uniform bool tUseAlbedo;
uniform sampler2D tAlbedo;
const float PI=3.14159265;
vec3 vPos;
layout(location=0) out vec4 color;
)GLSL";
    fs+=function(text,kind==2?"float point_reflection(":"vec3 brdf(");
    fs+=function(text,"float torch_attenuation(");
    fs+=function(text,kind==2?"vec3 torch_water(":"vec3 torch_surface(");
    fs+="\nvoid main(){vPos=tWorld;";
    if(kind==0)fs+="vPos.xz-=uWorldAnchor;";
    fs+="vec3 base=tUseAlbedo?texture(tAlbedo,vec2(.5)).rgb:tBase;vec3 result;";
    fs+="if(tMode==2)result=base;else if(tMode==1)result=vec3(torch_attenuation(0,tWorld,uTorchPositionRadius[0].xyz-tWorld));else result=vec3(.03125,.0625,.125)+";
    fs+=kind==0?"torch_surface(base,tMetal,tRough,tNormal,tView)":kind==1?"torch_surface(base,tRough,tNormal,tView)":"torch_water(tWorld,tNormal,tView,tRough)";
    fs+=";color=vec4(result,.8125);}\n";
    const char *vs="#version 330 core\nvoid main(){vec2 q=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(q*2.-1.,0,1);}";
    GLuint vertex=shader(GL_VERTEX_SHADER,vs),fragment=shader(GL_FRAGMENT_SHADER,fs),p=glCreateProgram();
    glAttachShader(p,vertex);glAttachShader(p,fragment);glLinkProgram(p);GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);
    if(!okay){char log[8192]={};glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(okay);
    glDeleteShader(vertex);glDeleteShader(fragment);return p;
}
void integer(GLuint p,const char *name,int n){glUniform1i(glGetUniformLocation(p,name),n);}
void scalar(GLuint p,const char *name,float n){glUniform1f(glGetUniformLocation(p,name),n);}
void vector(GLuint p,const char *name,V v){glUniform3f(glGetUniformLocation(p,name),float(v[0]),float(v[1]),float(v[2]));}
void upload(GLuint p,const WorldTorchLights &lights) {
    glUseProgram(p);integer(p,"uTorchCount",int(lights.count));
    glUniform4fv(glGetUniformLocation(p,"uTorchPositionRadius[0]"),8,lights.position_radius[0].data());
    glUniform4fv(glGetUniformLocation(p,"uTorchColorIntensity[0]"),8,lights.color_intensity[0].data());
    glUniform4fv(glGetUniformLocation(p,"uTorchWallPlane[0]"),8,lights.plane[0].data());
    glUniform4fv(glGetUniformLocation(p,"uTorchClipBounds[0]"),8,lights.bounds[0].data());
    glUniform2fv(glGetUniformLocation(p,"uTorchClipHeights[0]"),8,lights.heights[0].data());
    glUniform3fv(glGetUniformLocation(p,"uTorchClusterMin"),1,lights.cluster_min.data());
    glUniform3fv(glGetUniformLocation(p,"uTorchClusterMax"),1,lights.cluster_max.data());
}
void cluster(WorldTorchLights &l) {
    for(unsigned axis=0;axis<3;axis++)for(unsigned i=0;i<l.count;i++) {
        const float low=l.position_radius[i][axis]-l.position_radius[i][3],high=l.position_radius[i][axis]+l.position_radius[i][3];
        l.cluster_min[axis]=i?std::min(l.cluster_min[axis],low):low;l.cluster_max[axis]=i?std::max(l.cluster_max[axis],high):high;
    }
}
Pixel draw(GLuint p,V world) {
    vector(p,"tWorld",world);glDrawArrays(GL_TRIANGLES,0,3);Pixel out;glReadPixels(8,8,1,1,GL_RGBA,GL_FLOAT,out.data());
    for(auto a:out)assert(std::isfinite(a));
    assert(glGetError()==GL_NO_ERROR);return out;
}
void close(Pixel p,V expected,double tolerance=2.e-5) {
    for(unsigned k=0;k<3;k++) {
        if(std::abs(p[k]-expected[k])>tolerance*(1+std::abs(expected[k])))std::fprintf(stderr,"channel%u actual%.9g expected%.9g\n",k,p[k],expected[k]);
        assert(std::abs(p[k]-expected[k])<=tolerance*(1+std::abs(expected[k])));
    }
    assert(p[3]==legacy[3]);
}
double smooth(double hi,double x){const double t=std::clamp(x/hi,0.,1.);return t*t*(3-2*t);}
double attenuation(const WorldTorchLights &l,V world) {
    const auto &p=l.position_radius[0];V delta={p[0]-world[0],p[1]-world[1],p[2]-world[2]};
    const double d2=dot(delta,delta),r=p[3];if(r<=0||d2>=r*r)return 0;
    const auto &n=l.plane[0],&b=l.bounds[0];
    const double plane=n[0]*world[0]+n[1]*world[1]+n[2]*world[2]+n[3];
    const double edge=std::min({world[0]-b[0],world[2]-b[1],b[2]-world[0],b[3]-world[2]});
    const double vertical=std::min(world[1]-l.heights[0][0],l.heights[0][1]-world[1]);
    if(plane<=0||edge<=0||vertical<=0)return 0;
    const double window=1-std::pow(d2/(r*r),2);
    return window*window/std::max(d2,.25)*smooth(.10,plane)*smooth(.15,edge)*smooth(.10,vertical);
}
V reflectance(int kind,V base,double metal,double rough,V N,V Vdir,V L) {
    V sum={Vdir[0]+L[0],Vdir[1]+L[1],Vdir[2]+L[2]};const double hnorm=std::sqrt(std::max(dot(sum,sum),.0001));for(auto &a:sum)a/=hnorm;
    const double nl=std::max(dot(N,L),0.),nv=std::max(dot(N,Vdir),.001),nh=std::max(dot(N,sum),0.),vh=std::max(dot(Vdir,sum),0.);
    const double a=rough*rough,a2=a*a,den=nh*nh*(a2-1)+1;
    const double D=a2/std::max(3.14159265*den*den,kind==2?.00000001:.0001);
    const double k=kind==2?rough*rough*.5:(rough+1)*(rough+1)*.125;
    const double G=nl/(nl*(1-k)+k)*nv/(nv*(1-k)+k);
    V result={};
    for(unsigned c=0;c<3;c++) {
        const double f0=kind==2?.0204:(kind==1?.04:.035*(1-metal)+base[c]*metal);
        const double F=f0+(1-f0)*std::pow(1-vh,5);
        if(kind==2)result[c]=std::min(18.,D*G*F/std::max(4*nv,.001));
        else result[c]=((1-F)*base[c]*(kind==1?1:1-metal)/3.14159265+
            (kind==1?.22:1)*D*G*F/std::max(4*nl*nv,.001))*nl;
    }
    return result;
}
V expected(int kind,const WorldTorchLights &l,V world,V base,double metal,double rough,V N,V view) {
    V out={legacy[0],legacy[1],legacy[2]};
    if(!l.count)return out;
    const double atten=attenuation(l,world);const auto &p=l.position_radius[0],&color=l.color_intensity[0];
    V delta={p[0]-world[0],p[1]-world[1],p[2]-world[2]};const double n=std::sqrt(std::max(dot(delta,delta),.000001));for(auto &a:delta)a/=n;
    const auto brdf=reflectance(kind,base,metal,rough,N,view,delta);
    for(unsigned c=0;c<3;c++)out[c]+=brdf[c]*color[c]*color[3]*atten;
    return out;
}
void check(GLuint p,int kind) {
    glUseProgram(p);integer(p,"tAlbedo",0);integer(p,"tUseAlbedo",0);integer(p,"tMode",0);
    scalar(p,"uWetness",0);scalar(p,"tMetal",0);scalar(p,"tRough",.6f);
    const V base={.2f,.25f,.3f},N={0,1,0},view={0,1,0};vector(p,"tBase",base);vector(p,"tNormal",N);vector(p,"tView",view);
    glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);
    VillageTorchEmitter source={0,26.8,-145,.4f,.13f,20,812,0,0,0,1,-11.6f,-153.6f,11.6f,-136.f,24.015f,31.f};
    WorldTorchLights l;assert(world_torch_lights_build(&source,1,0,0,0,25.75,-145,3.125,l));assert(l.count==1);
    const auto original=l;V world={0,25.75,-145};upload(p,l);const auto lit=draw(p,world);close(lit,expected(kind,l,world,base,0,.6f,N,view));
    assert(lit[0]>legacy[0]+.001&&lit[0]-legacy[0]>lit[1]-legacy[1]&&lit[1]-legacy[1]>lit[2]-legacy[2]);
    const V oblique=normal({.4,1,-.2});vector(p,"tView",oblique);l.position_radius[0][0]=2;l.position_radius[0][2]+=1;cluster(l);upload(p,l);
    close(draw(p,world),expected(kind,l,world,base,0,.6f,N,oblique));
    vector(p,"tView",view);vector(p,"tNormal",{0,-1,0});assert(draw(p,world)==legacy);
    vector(p,"tNormal",N);l=original;upload(p,l);
    // Every light color/intensity multiplier is already linear radiance.
    // A half-linear color must halve the return, never be decoded as sRGB.
    for(unsigned c=0;c<3;c++)l.color_intensity[0][c]*=.5f;
    upload(p,l);const auto half=draw(p,world);for(unsigned c=0;c<3;c++)assert(std::abs((half[c]-legacy[c])-(lit[c]-legacy[c])*.5)<1.e-5);
    l=original;l.color_intensity[0][3]*=.25f;upload(p,l);const auto quarter=draw(p,world);
    for(unsigned c=0;c<3;c++)assert(std::abs((quarter[c]-legacy[c])-(lit[c]-legacy[c])*.25)<1.e-5);
    l=original;l.count=8;
    for(unsigned i=0;i<8;i++) {
        l.position_radius[i]=original.position_radius[0];l.plane[i]=original.plane[0];l.bounds[i]=original.bounds[0];l.heights[i]=original.heights[0];
        l.color_intensity[i]=original.color_intensity[0];l.color_intensity[i][3]*=float(i+1)/8;
    }
    upload(p,l);const auto eight=draw(p,world);
    for(unsigned c=0;c<3;c++)assert(std::abs((eight[c]-legacy[c])-(lit[c]-legacy[c])*4.5)<.0001);
    // Model and terrain consume linear albedo. A real GL sRGB sample must
    // agree with independent IEC-decoded linear input, without a second decode.
    l=original;upload(p,l);
    if(kind<2) {
        integer(p,"tUseAlbedo",1);integer(p,"tMode",2);const auto decoded=draw(p,world);integer(p,"tMode",0);
        const auto sampled=draw(p,world);integer(p,"tUseAlbedo",0);
        const double srgb=128./255.,linear=std::pow((srgb+.055)/1.055,2.4);
        for(unsigned c=0;c<3;c++)assert(std::abs(decoded[c]-linear)<1./255.); // permitted8-bit sRGB table precision
        vector(p,"tBase",{decoded[0],decoded[1],decoded[2]});const auto linear_pixel=draw(p,world);
        for(unsigned c=0;c<3;c++)assert(std::abs(sampled[c]-linear_pixel[c])<1.e-5);
        vector(p,"tBase",{srgb,srgb,srgb});const auto unconverted=draw(p,world);assert(unconverted[0]>linear_pixel[0]*1.2f);vector(p,"tBase",base);
    } else {vector(p,"tBase",{.01,.8,.4});assert(draw(p,world)==lit);vector(p,"tBase",base);}
    // Metal and roughness take the original direct BRDF path, with analytic
    // CPU double checks rather than comparing against a captured wrong image.
    if(kind==0){scalar(p,"tMetal",1);close(draw(p,world),expected(kind,l,world,base,1,.6f,N,view));scalar(p,"tMetal",0);}
    scalar(p,"tRough",.25f);close(draw(p,world),expected(kind,l,world,base,0,.25,N,view),.0002);scalar(p,"tRough",.6f);
    // A new visual time/seed changes only shared light power.
    assert(world_torch_lights_build(&source,1,0,0,0,25.75,-145,3.25,l));upload(p,l);const auto flickered=draw(p,world);
    const double ratio=world_torch_flicker(3.25,source.seed)/world_torch_flicker(3.125,source.seed);
    for(unsigned c=0;c<3;c++)assert(std::abs((flickered[c]-legacy[c])-(lit[c]-legacy[c])*ratio)<2.e-5);
    source.seed++;assert(world_torch_lights_build(&source,1,0,0,0,25.75,-145,3.25,l));upload(p,l);assert(draw(p,world)!=flickered);source.seed--;
    l=original;
    // Any conservative global cluster rejection must preserve zero output.
    l.cluster_min[0]=.01f;upload(p,l);assert(draw(p,world)==legacy);l=original;
    if(kind==0){glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),32000,-32000);upload(p,l);assert(draw(p,world)==lit);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);}
    // Force source support above each tested surface so a zero cannot be
    // explained by its distance/normal; test wall, hall and height constraints.
    const V blocked[]={{-11.6f,25.75,-145},{-11.601f,25.75,-145},{11.6f,25.75,-145},
        {11.601f,25.75,-145},{0,25.75,-153.6f},{0,25.75,-153.601f},{0,25.75,-136.f},
        {0,25.75,-135.999f},{0,24.015f,-145},{0,24.014f,-145},{0,22.9f,-145},{0,31.f,-145},{0,31.001f,-145}};
    for(V q:blocked) {
        l=original;l.position_radius[0][0]=float(q[0]);l.position_radius[0][1]=float(q[1]+1);l.position_radius[0][2]=float(q[2]);cluster(l);
        upload(p,l);assert(draw(p,q)==legacy);
    }
    l=original;l.plane[0]={0,0,1,145};
    for(float z:{-145.1f,-145.f}){world={0,25.75,z};upload(p,l);assert(draw(p,world)==legacy);}
    // Inspect attenuation independently of the BRDF: exact radius, finite
    // source core, wall half-feather and the global/local clipping controls.
    integer(p,"tMode",1);world={0,25.75,-144.95f};l.position_radius[0]={0,26.75f,-144.95f,8};cluster(l);upload(p,l);
    const double atten=attenuation(l,world);const auto mid=draw(p,world);close(mid,{atten,atten,atten},.00005);
    assert(mid[0]>.45&&mid[0]<.55);
    l=original;l.bounds[0]={-1000,-1000,1000,1000};l.heights[0]={-1000,1000};l.plane[0]={0,0,0,1};l.position_radius[0]={0,25.75f,-145,8};cluster(l);world={0,25.75,-145};upload(p,l);
    const auto center=draw(p,world);close(center,{4,4,4});
    for(float d:{7.99f,8.f,8.01f}) {
        world={d,25.75,-145};const auto pixel=draw(p,world);
        if(d<8)assert(pixel[0]>0&&pixel[0]<.00001f);else assert(pixel[0]==0);
    }
    l.position_radius[0][3]=0;upload(p,l);close(draw(p,{0,25.75,-145}),{0,0,0});
    // Zero-distance direct direction is finite and contributes zero, rather
    // than normalizing0 and poisoning the HDR target with NaNs.
    l.position_radius[0][3]=8;upload(p,l);integer(p,"tMode",0);assert(draw(p,{0,25.75,-145})==legacy);
    // Count0 remains bit-identical even with stale/invalid array values.
    l.count=0;l.position_radius[0]={NAN,NAN,NAN,NAN};upload(p,l);assert(draw(p,{0,25.75,-145})==legacy);
    std::printf("PASS %s torch functions: analytic BRDF/range, warm linear/scaled radiance%s, metal/roughness, time/seed, wall/hall/height/cluster exact zeros, finite source center, count0 bit-exact\n",kind==0?"model":kind==1?"terrain":"water",kind<2?"/sRGB albedo":"/albedo-independent specular");
}
}
int main() {
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL")){setenv("SDL_VIDEODRIVER","offscreen",1);setenv("LIBGL_ALWAYS_SOFTWARE","1",1);}
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Torch lights regression",0,0,16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Renderer: %s\n",glGetString(GL_RENDERER));
    GLuint fbo=0,target=0,vao=0,albedo=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,16,16,0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glGenTextures(1,&albedo);glBindTexture(GL_TEXTURE_2D,albedo);const unsigned char rgb[]={128,128,128,255};glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,rgb);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glViewport(0,0,16,16);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_DITHER);glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    const char *paths[]={"shaders/world_foliage.fs.glsl","shaders/world_terrain.fs.glsl","shaders/world_water.fs.glsl"};std::array<std::string,3> texts;
    for(unsigned i=0;i<3;i++)texts[i]=read(paths[i]);
    assert(function(texts[0],"float torch_attenuation(")==function(texts[1],"float torch_attenuation(")&&function(texts[1],"float torch_attenuation(")==function(texts[2],"float torch_attenuation("));
    for(unsigned i=0;i<3;i++){const GLuint p=probe(texts[i],int(i));check(p,int(i));glDeleteProgram(p);}
    assert(glGetError()==GL_NO_ERROR);glDeleteVertexArrays(1,&vao);glDeleteTextures(1,&target);glDeleteTextures(1,&albedo);glDeleteFramebuffers(1,&fbo);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
