#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "world_sky.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <vector>

namespace {
struct Target { GLuint fbo=0,color=0; int w=0,h=0; };
void release(Target &t) {glDeleteFramebuffers(1,&t.fbo);glDeleteTextures(1,&t.color);t={};}
GLuint compile(GLenum kind,const std::string &source) {
    GLuint shader=glCreateShader(kind);const char *p=source.c_str();glShaderSource(shader,1,&p,nullptr);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096];glGetShaderInfoLog(shader,sizeof(log),nullptr,log);std::fprintf(stderr,"World sky shader: %s\n",log);glDeleteShader(shader);return 0;}
    return shader;
}
GLuint program(const std::string &v,const std::string &f) {
    GLuint vs=compile(GL_VERTEX_SHADER,v),fs=compile(GL_FRAGMENT_SHADER,f);
    if(!vs||!fs){glDeleteShader(vs);glDeleteShader(fs);return 0;}
    GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[4096];glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"World sky link: %s\n",log);glDeleteProgram(p);return 0;}
    return p;
}
std::string read(const char *path) {if(!path)return {};std::ifstream f(path);return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t hash(int x,int y,int z,unsigned salt) {
    std::uint32_t h=std::uint32_t(x)*0x8da6b343u^std::uint32_t(y)*0xd8163841u^std::uint32_t(z)*0xcb1ab31fu^salt;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);
}
float random(int x,int y,int z,unsigned salt) {return float(hash(x,y,z,salt)&0xffffffu)/16777215.f;}
int wrap(int x,int n) {x%=n;return x<0?x+n:x;}
float value(float x,float y,float z,int n,unsigned salt) {
    int ix=int(std::floor(x)),iy=int(std::floor(y)),iz=int(std::floor(z));
    float fx=x-ix,fy=y-iy,fz=z-iz;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);fz=fz*fz*(3-2*fz);
    float result=0;
    for(int dz=0;dz<2;++dz)for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)
        result+=random(wrap(ix+dx,n),wrap(iy+dy,n),wrap(iz+dz,n),salt)*(dx?fx:1-fx)*(dy?fy:1-fy)*(dz?fz:1-fz);
    return result;
}
float worley(float x,float y,float z,int n,unsigned salt) {
    int ix=int(std::floor(x)),iy=int(std::floor(y)),iz=int(std::floor(z));float best=3;
    for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
        int cx=ix+dx,cy=iy+dy,cz=iz+dz,wx=wrap(cx,n),wy=wrap(cy,n),wz=wrap(cz,n);
        float px=cx+random(wx,wy,wz,salt)-x,py=cy+random(wx,wy,wz,salt+1)-y,pz=cz+random(wx,wy,wz,salt+2)-z;
        best=std::min(best,px*px+py*py+pz*pz);
    }
    return 1-std::min(1.f,std::sqrt(best));
}
std::vector<unsigned char> volume() {
    constexpr int n=64;std::vector<unsigned char> data(n*n*n*4);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        float u=(x+.5f)/n,v=(y+.5f)/n,w=(z+.5f)/n;
        float a=value(u*4,v*4,w*4,4,21),b=worley(u*8,v*8,w*8,8,51),c=worley(u*16,v*16,w*16,16,81);
        const float channels[4]={std::clamp(a*.45f+b*.55f,0.f,1.f),b,c,value(u*16,v*16,w*16,16,121)};
        for(int k=0;k<4;++k)data[((z*n+y)*n+x)*4+k]=static_cast<unsigned char>(channels[k]*255+.5f);
    }
    return data;
}
bool target(Target &t,int w,int h) {
    glGenFramebuffers(1,&t.fbo);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);glGenTextures(1,&t.color);glBindTexture(GL_TEXTURE_2D,t.color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.color,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){release(t);return false;}
    t.w=w;t.h=h;return true;
}
constexpr const char *copy_fs=R"GLSL(#version 330 core
in vec2 vUV;out vec4 FragColor;uniform sampler2D uSky;
void main(){
    // Four bilinear samples form a small tent while upsampling. This filters
    // finite-step cloud variance without another pass or density evaluation.
    vec2 h=.5/vec2(textureSize(uSky,0));
    FragColor=.25*(texture(uSky,vUV+vec2(-h.x,-h.y))+texture(uSky,vUV+vec2(h.x,-h.y))
        +texture(uSky,vUV+vec2(-h.x,h.y))+texture(uSky,vUV+vec2(h.x,h.y)));
}
)GLSL";
}
struct WorldSky {
    Target targets[WORLD_SKY_VIEW_COUNT];GLuint program=0,copy=0,vao=0,noise=0;WorldSkyStats stats;
    GLint vector[12]={},scalar[4]={},jitter=-1,has_volume=-1,volumetric_clouds=-1,sun_disc=-1,environment=-1,environment_rotation=-1,environment_intensity=-1,environment_strength=-1;
};

WorldSky *world_sky_create(const char *vertex_path,const char *fragment_path) {
    WorldSky *s=new(std::nothrow) WorldSky;if(!s)return nullptr;
    const std::string vertex=read(vertex_path);std::string fragment=read(fragment_path);if(!fragment.empty())fragment.insert(fragment.find('\n')+1,"#define VALWOOD_HDR_ENVIRONMENT 1\n");
    if(vertex.empty()||fragment.empty()){delete s;return nullptr;}
    s->program=program(vertex,fragment);s->copy=program(vertex,copy_fs);
    if(!s->program||!s->copy){world_sky_free(s);return nullptr;}
    const char *vectors[]={"uRight","uUp","uForward","uEye","uSun","uSunColor","uFog","uAmbient","uSkyLight","uGroundLight","uSkyZenith"};
    for(unsigned i=0;i<11;++i)s->vector[i]=glGetUniformLocation(s->program,vectors[i]);
    const char *scalars[]={"uAspect","uTan","uCloud","uTime"};
    for(unsigned i=0;i<4;++i)s->scalar[i]=glGetUniformLocation(s->program,scalars[i]);
    s->jitter=glGetUniformLocation(s->program,"uJitterNDC");
    s->has_volume=glGetUniformLocation(s->program,"uHasVolume");
    s->volumetric_clouds=glGetUniformLocation(s->program,"uVolumetricClouds");glUseProgram(s->program);
    s->sun_disc=glGetUniformLocation(s->program,"uSunDisc");
    s->environment=glGetUniformLocation(s->program,"uEnvironmentSky");s->environment_rotation=glGetUniformLocation(s->program,"uEnvironmentRotation");s->environment_intensity=glGetUniformLocation(s->program,"uEnvironmentIntensity");s->environment_strength=glGetUniformLocation(s->program,"uEnvironmentStrength");glUniform1i(s->environment,1);
    glUniform1i(s->has_volume,1);glUniform1i(glGetUniformLocation(s->program,"uCloudNoise"),0);
    glUseProgram(s->copy);glUniform1i(glGetUniformLocation(s->copy,"uSky"),0);glGenVertexArrays(1,&s->vao);
    const auto data=volume();glActiveTexture(GL_TEXTURE0);glGenTextures(1,&s->noise);glBindTexture(GL_TEXTURE_3D,s->noise);
    glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA8,64,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,data.data());
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    for(GLenum axis:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_WRAP_R})glTexParameteri(GL_TEXTURE_3D,axis,GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_3D);
    GLint uploaded=0,last_mip=0;
    glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_WIDTH,&uploaded);
    glGetTexLevelParameteriv(GL_TEXTURE_3D,6,GL_TEXTURE_WIDTH,&last_mip);
    if(uploaded!=64||last_mip!=1){world_sky_free(s);return nullptr;}
    for(int n=64;n;n/=2)s->stats.noise_bytes+=std::uint64_t(n)*n*n*4;
    s->stats.noise_uploads=1;
    return s;
}
bool world_sky_draw(WorldSky *s,const WorldSkyFrame &f,unsigned destination,int width,int height,unsigned view) {
    if(!s||view>=WORLD_SKY_VIEW_COUNT||width<1||height<1||width>8192||height>8192||std::uint64_t(width)*height>16777216)return false;
    for(const auto &t:s->targets)if(t.fbo&&destination==t.fbo)return false;
    const float *vectors[]={f.right,f.up,f.forward,f.eye,f.sun,f.sun_color,f.fog,f.ambient,f.sky_light,f.ground_light,f.sky_zenith};
    for(const float *v:vectors)for(int i=0;i<3;++i)if(!std::isfinite(v[i]))return false;
    const float scalars[]={f.aspect,f.tan_half_fov,f.cloud,f.time,f.jitter_ndc[0],f.jitter_ndc[1],f.environment_rotation,f.environment_intensity,f.environment_strength};
    for(float v:scalars)if(!std::isfinite(v))return false;
    // Reject finite-but-overflowing caller input before the procedural hash,
    // normalization and half-float output. Normal game/world ranges are much
    // smaller than these generous bounds (10000km position, >3years time).
    for(unsigned v=0;v<11;++v)for(int i=0;i<3;++i)
        if(std::abs(vectors[v][i])>(v<5?1e7f:64.f))return false;
    if(f.environment_rotation< -180||f.environment_rotation>180||f.environment_intensity<0||f.environment_intensity>8||f.environment_strength<0||f.environment_strength>1)return false;
    if(f.aspect<=0||f.aspect>100||f.tan_half_fov<=0||f.tan_half_fov>100||std::abs(f.time)>1e8f
        ||std::abs(f.jitter_ndc[0])>10||std::abs(f.jitter_ndc[1])>10)return false;
    glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    Target &t=s->targets[view];const int w=(width+1)/2,h=(height+1)/2;
    if(t.w!=w||t.h!=h) {
        Target next;if(!target(next,w,h)){glBindFramebuffer(GL_FRAMEBUFFER,destination);glViewport(0,0,width,height);return false;}
        s->stats.target_allocations++;if(t.fbo)s->stats.target_resizes++;
        release(t);t=next;s->stats.width[view]=w;s->stats.height[view]=h;
        s->stats.target_bytes=0;for(const auto &q:s->targets)s->stats.target_bytes+=std::uint64_t(q.w)*q.h*8;
    }
    glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);glViewport(0,0,w,h);glBindVertexArray(s->vao);glUseProgram(s->program);
    const float angle=f.environment_rotation*.01745329252f;glUniform2f(s->environment_rotation,std::cos(angle),std::sin(angle));glUniform1f(s->environment_intensity,f.environment_intensity);glUniform1f(s->environment_strength,f.environment_texture?f.environment_strength:0);glActiveTexture(GL_TEXTURE1);glBindSampler(1,0);glBindTexture(GL_TEXTURE_CUBE_MAP,f.environment_texture);glActiveTexture(GL_TEXTURE0);
    for(unsigned i=0;i<11;++i)glUniform3fv(s->vector[i],1,vectors[i]);
    for(unsigned i=0;i<4;++i)glUniform1f(s->scalar[i],scalars[i]);
    glUniform2fv(s->jitter,1,f.jitter_ndc);
    glUniform1i(s->volumetric_clouds,f.volumetric_clouds?1:0);
    glUniform1i(s->sun_disc,f.sun_disc?1:0);
    glBindTexture(GL_TEXTURE_3D,s->noise);glDrawArrays(GL_TRIANGLES,0,3);
    glBindFramebuffer(GL_FRAMEBUFFER,destination);glViewport(0,0,width,height);glUseProgram(s->copy);glBindTexture(GL_TEXTURE_2D,t.color);
    glDrawArrays(GL_TRIANGLES,0,3);s->stats.draw_calls+=2;return true;
}
WorldSkyStats world_sky_stats(const WorldSky *s) {return s?s->stats:WorldSkyStats{};}
void world_sky_free(WorldSky *s) {
    if(!s)return;
    for(auto &t:s->targets)release(t);
    glDeleteTextures(1,&s->noise);glDeleteVertexArrays(1,&s->vao);
    glDeleteProgram(s->program);glDeleteProgram(s->copy);delete s;
}
