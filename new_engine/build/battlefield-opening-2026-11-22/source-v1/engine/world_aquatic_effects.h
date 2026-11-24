#ifndef VALWOOD_WORLD_AQUATIC_EFFECTS_H
#define VALWOOD_WORLD_AQUATIC_EFFECTS_H
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <SDL2/SDL_opengl.h>
#include "engine/world_aquatic_state.h"
#include <cstddef>
#include <cstdio>

namespace world_aquatic_effects {
struct Frame {
    // Column-major matrix transforms CAMERA-RELATIVE world positions.
    float vp[16]={};double eye[3]={};
    float right[3]={1,0,0},up[3]={0,1,0};
    float sun[3]={.3f,.8f,.4f},sun_color[3]={1.f,.92f,.8f},ambient[3]={.26f,.32f,.34f};
};
// Caller supplies current state; no synchronous GL state queries/readbacks.
// Framebuffer, viewport, textures, scissor, stencil and color-mask are untouched.
struct RestoreState {
    GLuint program=0,vao=0,array_buffer=0;
    bool depth_test=true,depth_write=true,blend=false,cull=false;
    GLenum depth_func=GL_LESS,blend_eq_rgb=GL_FUNC_ADD,blend_eq_alpha=GL_FUNC_ADD;
    GLenum blend_src_rgb=GL_ONE,blend_dst_rgb=GL_ZERO,blend_src_alpha=GL_ONE,blend_dst_alpha=GL_ZERO;
};
struct RenderStats { std::uint64_t fish_draws=0,surface_draws=0,uploads=0;unsigned fish_instances=0,effect_instances=0; };
class Renderer {
    struct Vertex { float x,y,z,nx,ny,nz,kind; };
    struct FishInstance {float pos_size[4],yaw_phase[4];};
    struct EffectInstance {float pos_size[4],color_kind[4],parameters[4];};
    GLuint fish_program_=0,effect_program_=0,fish_vao_=0,effect_vao_=0,mesh_=0,fish_buffer_=0,effect_buffer_=0;
    GLint fish_vp_=-1,fish_time_=-1,fish_sun_=-1,fish_color_=-1,fish_ambient_=-1;
    GLint effect_vp_=-1,effect_right_=-1,effect_up_=-1;
    GLsizei vertices_=0;
    std::array<FishInstance,fish_cap> fish_stage_{};
    std::array<EffectInstance,particle_cap+ripple_cap> effect_stage_{};
    static GLuint compile(GLenum type,const char* source) {
        const GLuint shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
        GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        if(!ok){char text[2048]={};glGetShaderInfoLog(shader,sizeof text,nullptr,text);std::fprintf(stderr,"[Aquatics] shader: %s\n",text);glDeleteShader(shader);return 0;}
        return shader;
    }
    static GLuint program(const char* vs,const char* fs) {
        const GLuint v=compile(GL_VERTEX_SHADER,vs),f=compile(GL_FRAGMENT_SHADER,fs);
        if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return 0;}
        const GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);
        GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
        if(!ok){char text[2048]={};glGetProgramInfoLog(p,sizeof text,nullptr,text);std::fprintf(stderr,"[Aquatics] link: %s\n",text);glDeleteProgram(p);return 0;}
        return p;
    }
    static void restore(const RestoreState& s) {
        if(s.depth_test)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        glDepthFunc(s.depth_func);glDepthMask(s.depth_write?GL_TRUE:GL_FALSE);
        if(s.blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
        glBlendEquationSeparate(s.blend_eq_rgb,s.blend_eq_alpha);
        glBlendFuncSeparate(s.blend_src_rgb,s.blend_dst_rgb,s.blend_src_alpha,s.blend_dst_alpha);
        if(s.cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
        glUseProgram(s.program);glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.array_buffer);
    }
    static bool valid(const Frame& f) {
        for(float x:f.vp)if(!std::isfinite(x))return false;
        for(double x:f.eye)if(!std::isfinite(x)||std::abs(x)>1e8)return false;
        for(const float* p:{f.right,f.up,f.sun,f.sun_color,f.ambient})for(unsigned i=0;i<3;i++)if(!std::isfinite(p[i]))return false;
        return true;
    }
public:
    RenderStats stats{};
    Renderer()=default;
    Renderer(const Renderer&)=delete;Renderer& operator=(const Renderer&)=delete;
    bool init(const RestoreState& state={}) {
        if(fish_program_||effect_program_)return false;
        static constexpr const char* fish_vs=R"GLSL(#version 330 core
layout(location=0)in vec3 aPosition;layout(location=1)in vec3 aNormal;layout(location=2)in float aKind;
layout(location=3)in vec4 iPosition;layout(location=4)in vec4 iMotion;
uniform mat4 uVP;uniform float uTime;
out vec3 vNormal;out float vKind;out float vBelly;out vec3 vEye;
void main(){
 vec3 p=aPosition;float tail=clamp((-p.x-.1)*2.8,0.,1.);
 p.z+=sin(uTime*9.+iMotion.y+p.x*5.)*.13*tail;
 if(aKind>1.5&&aKind<2.5)p.y+=sin(uTime*7.+iMotion.y)*.025;
 float c=cos(iMotion.x),s=sin(iMotion.x);mat3 turn=mat3(c,0.,s,0.,1.,0.,-s,0.,c);
 vec3 relative=iPosition.xyz+turn*p*iPosition.w;
 vNormal=turn*aNormal;vKind=aKind;vBelly=aPosition.y;vEye=-relative;
 gl_Position=uVP*vec4(relative,1.);
})GLSL";
        static constexpr const char* fish_fs=R"GLSL(#version 330 core
in vec3 vNormal;in float vKind;in float vBelly;in vec3 vEye;
uniform vec3 uSun,uSunColor,uAmbient;layout(location=0)out vec4 outColor;layout(location=1)out vec4 outIndirect;
void main(){
 vec3 n=normalize(gl_FrontFacing?vNormal:-vNormal),l=normalize(uSun),v=normalize(vEye);
 vec3 base=mix(vec3(.48,.49,.31),vec3(.09,.24,.22),smoothstep(-.08,.10,vBelly));
 if(vKind>1.5&&vKind<2.5)base=vec3(.29,.28,.12);
 if(vKind>2.5)base=vec3(.008,.012,.01);
 float light=max(dot(n,l),0.);float spec=pow(max(dot(n,normalize(l+v)),0.),40.);
 outColor=vec4(base*(uAmbient+uSunColor*light*.7)+uSunColor*spec*.30,1.);
 outIndirect=vec4(0.);
})GLSL";
        static constexpr const char* effect_vs=R"GLSL(#version 330 core
layout(location=0)in vec4 iPosition;layout(location=1)in vec4 iColorKind;layout(location=2)in vec4 iParameters;
uniform mat4 uVP;uniform vec3 uRight,uUp;out vec2 vUV;out vec3 vColor;out float vAlpha;flat out int vKind;
void main(){
 vec2 q=vec2((gl_VertexID==1||gl_VertexID==2||gl_VertexID==4)?1.:-1.,(gl_VertexID==2||gl_VertexID==4||gl_VertexID==5)?1.:-1.);
 vUV=q;vColor=iColorKind.rgb;vKind=int(iColorKind.a+.5);vAlpha=iParameters.x;
 vec3 offset=vKind==1?vec3(q.x,0.,q.y)*iPosition.w:(uRight*q.x+uUp*q.y)*iPosition.w;
 gl_Position=uVP*vec4(iPosition.xyz+offset,1.);
})GLSL";
        static constexpr const char* effect_fs=R"GLSL(#version 330 core
in vec2 vUV;in vec3 vColor;in float vAlpha;flat in int vKind;out vec4 outColor;
void main(){
 float r=length(vUV);if(r>1.)discard;
 float alpha;
 if(vKind==1){float ring=exp(-pow((r-.83)*24.,2.));float broken=.45+.55*smoothstep(-.65,.7,sin((r>.0001?atan(vUV.y,vUV.x):0.)*9.+r*13.));alpha=ring*broken*vAlpha;}
 else if(vKind==2){float edge=smoothstep(.65,.94,r)*(1.-smoothstep(.94,1.,r));float shine=exp(-dot(vUV-vec2(-.32,.35),vUV-vec2(-.32,.35))*55.);alpha=(edge*.65+shine)*vAlpha;}
 else{alpha=(1.-smoothstep(.25,1.,r))*vAlpha;}
 outColor=vec4(vColor*alpha,alpha);
})GLSL";
        fish_program_=program(fish_vs,fish_fs);effect_program_=program(effect_vs,effect_fs);
        if(!fish_program_||!effect_program_){free();restore(state);return false;}
        fish_vp_=glGetUniformLocation(fish_program_,"uVP");fish_time_=glGetUniformLocation(fish_program_,"uTime");
        fish_sun_=glGetUniformLocation(fish_program_,"uSun");fish_color_=glGetUniformLocation(fish_program_,"uSunColor");fish_ambient_=glGetUniformLocation(fish_program_,"uAmbient");
        effect_vp_=glGetUniformLocation(effect_program_,"uVP");effect_right_=glGetUniformLocation(effect_program_,"uRight");effect_up_=glGetUniformLocation(effect_program_,"uUp");
        std::array<Vertex,420> geometry{};unsigned count=0;
        auto vertex=[&](double x,double y,double z,double nx,double ny,double nz,float kind=0) {
            geometry[count++]={float(x),float(y),float(z),float(nx),float(ny),float(nz),kind};
        };
        // Ellipsoid body, thin forked tail, dorsal and two pectoral fins, eyes.
        auto body=[&](unsigned ring,unsigned side) {
            const double u=double(ring)/6*pi,a=double(side%10)/10*2*pi;
            const double x=.42*std::cos(u),y=.12*std::sin(u)*std::cos(a),z=.09*std::sin(u)*std::sin(a);
            const double nx=x/(.42*.42),ny=y/(.12*.12),nz=z/(.09*.09),len=std::sqrt(nx*nx+ny*ny+nz*nz);
            vertex(x,y,z,nx/len,ny/len,nz/len);
        };
        for(unsigned r=0;r<6;r++)for(unsigned a=0;a<10;a++) {
            body(r,a);body(r+1,a);body(r+1,a+1);body(r,a);body(r+1,a+1);body(r,a+1);
        }
        auto fin=[&](std::array<float,3>a,std::array<float,3>b,std::array<float,3>c,float kind) {
            double x=(b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]);
            double y=(b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]);
            double z=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);const double n=std::sqrt(x*x+y*y+z*z);
            for(auto p:{a,b,c})vertex(p[0],p[1],p[2],x/n,y/n,z/n,kind);
        };
        fin({-.36f,0,0},{-.65f,.21f,0},{-.57f,0,0},2);fin({-.36f,0,0},{-.57f,0,0},{-.65f,-.21f,0},2);
        fin({.10f,.10f,0},{-.12f,.27f,0},{-.23f,.06f,0},2);
        fin({.13f,-.015f,.075f},{-.10f,-.12f,.22f},{-.10f,-.06f,.05f},2);
        fin({.13f,-.015f,-.075f},{-.10f,-.06f,-.05f},{-.10f,-.12f,-.22f},2);
        for(float side:{-1.f,1.f})fin({.28f,.05f,side*.061f},{.24f,.06f,side*.075f},{.25f,.025f,side*.075f},3);
        vertices_=GLsizei(count);
        glGenVertexArrays(1,&fish_vao_);glGenVertexArrays(1,&effect_vao_);glGenBuffers(1,&mesh_);glGenBuffers(1,&fish_buffer_);glGenBuffers(1,&effect_buffer_);
        glBindVertexArray(fish_vao_);glBindBuffer(GL_ARRAY_BUFFER,mesh_);glBufferData(GL_ARRAY_BUFFER,count*sizeof(Vertex),geometry.data(),GL_STATIC_DRAW);
        for(unsigned i=0;i<3;i++){glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==2?1:3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(std::uintptr_t(i*3*sizeof(float))));}
        glBindBuffer(GL_ARRAY_BUFFER,fish_buffer_);glBufferData(GL_ARRAY_BUFFER,sizeof(fish_stage_),nullptr,GL_STREAM_DRAW);
        for(unsigned i=0;i<2;i++){glEnableVertexAttribArray(i+3);glVertexAttribPointer(i+3,4,GL_FLOAT,GL_FALSE,sizeof(FishInstance),reinterpret_cast<void*>(std::uintptr_t(i*4*sizeof(float))));glVertexAttribDivisor(i+3,1);}
        glBindVertexArray(effect_vao_);glBindBuffer(GL_ARRAY_BUFFER,effect_buffer_);glBufferData(GL_ARRAY_BUFFER,sizeof(effect_stage_),nullptr,GL_STREAM_DRAW);
        for(unsigned i=0;i<3;i++){glEnableVertexAttribArray(i);glVertexAttribPointer(i,4,GL_FLOAT,GL_FALSE,sizeof(EffectInstance),reinterpret_cast<void*>(std::uintptr_t(i*4*sizeof(float))));glVertexAttribDivisor(i,1);}
        const bool ok=fish_vao_&&effect_vao_&&mesh_&&fish_buffer_&&effect_buffer_;if(!ok)free();restore(state);return ok;
    }
    // Must run in the opaque scene HDR pass BEFORE water, with matching depth.
    // 48*381 vertices maximum, depth tested/written; no texture dependencies.
    void draw_fish(const System& system,const Frame& f,const RestoreState& state={}) {
        if(!fish_program_||!valid(f))return;
        unsigned count=0;
        for(const auto& fish:system.fish)if(fish.alive) {
            auto& g=fish_stage_[count++];g={{float(fish.x-f.eye[0]),float(fish.y-f.eye[1]),float(fish.z-f.eye[2]),float(fish.size)},
                                        {float(fish.yaw),float(fish.phase),0,0}};
        }
        stats.fish_instances=count;if(!count)return;
        glUseProgram(fish_program_);glUniformMatrix4fv(fish_vp_,1,GL_FALSE,f.vp);glUniform1f(fish_time_,float(std::fmod(system.time(),600.)));
        glUniform3fv(fish_sun_,1,f.sun);glUniform3fv(fish_color_,1,f.sun_color);glUniform3fv(fish_ambient_,1,f.ambient);
        glBindVertexArray(fish_vao_);glBindBuffer(GL_ARRAY_BUFFER,fish_buffer_);glBufferSubData(GL_ARRAY_BUFFER,0,count*sizeof(FishInstance),fish_stage_.data());
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
        glDrawArraysInstanced(GL_TRIANGLES,0,vertices_,count);++stats.fish_draws;++stats.uploads;restore(state);
    }
    // AFTER water in the same camera/depth target. Fixed 256 quads maximum.
    // Premultiplied alpha, depth test/no writes. Destination alpha preserved.
    void draw_surface(const System& system,const Frame& f,const RestoreState& state={}) {
        if(!effect_program_||!valid(f))return;
        unsigned count=0;
        for(const auto& r:system.ripples)if(r.alive) {
            const double t=r.age/r.life;auto& g=effect_stage_[count++];
            g={{float(r.x-f.eye[0]),float(r.y-f.eye[1]),float(r.z-f.eye[2]),float(r.size+t*.95)},
               {.55f,.72f,.74f,1},{float((1-t)*(1-t)*.16),0,0,0}};
        }
        for(const auto& p:system.particles)if(p.alive) {
            const double t=p.age/p.life,dx=p.x-f.eye[0],dy=p.y-f.eye[1],dz=p.z-f.eye[2];
            // Keep nearby exhalation bubbles from filling the first-person lens.
            // Small real-world radii plus a soft near fade apply to either camera.
            const double distance=std::sqrt(dx*dx+dy*dy+dz*dz);
            const double fade=std::clamp((distance-.22)/.48,0.,1.);
            const double size=p.size*(p.bubble?.22*(1.+t*.3):1.);
            auto& g=effect_stage_[count++];
            g={{float(dx),float(dy),float(dz),float(size)},
               {p.bubble?.5f:.75f,p.bubble?.75f:.9f,1.f,p.bubble?2.f:0.f},{float((1-t)*fade*(p.bubble?.4:.75)),0,0,0}};
        }
        stats.effect_instances=count;if(!count)return;
        glUseProgram(effect_program_);glUniformMatrix4fv(effect_vp_,1,GL_FALSE,f.vp);glUniform3fv(effect_right_,1,f.right);glUniform3fv(effect_up_,1,f.up);
        glBindVertexArray(effect_vao_);glBindBuffer(GL_ARRAY_BUFFER,effect_buffer_);glBufferSubData(GL_ARRAY_BUFFER,0,count*sizeof(EffectInstance),effect_stage_.data());
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);
        glDrawArraysInstanced(GL_TRIANGLES,0,6,count);++stats.surface_draws;++stats.uploads;restore(state);
    }
    // Explicit owner-context cleanup. No GL work in the C++ destructor.
    void free() {
        glDeleteBuffers(1,&mesh_);glDeleteBuffers(1,&fish_buffer_);glDeleteBuffers(1,&effect_buffer_);
        glDeleteVertexArrays(1,&fish_vao_);glDeleteVertexArrays(1,&effect_vao_);
        if(fish_program_)glDeleteProgram(fish_program_);
        if(effect_program_)glDeleteProgram(effect_program_);
        fish_program_=effect_program_=fish_vao_=effect_vao_=mesh_=fish_buffer_=effect_buffer_=0;vertices_=0;
    }
};
}
#endif
