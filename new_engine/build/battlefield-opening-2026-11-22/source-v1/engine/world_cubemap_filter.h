#ifndef VALWOOD_WORLD_CUBEMAP_FILTER_H
#define VALWOOD_WORLD_CUBEMAP_FILTER_H
#include <SDL2/SDL_opengl.h>
// Startup-only 64-sample GGX prefilter shared by HDR and captured local radiance.
namespace world_cubemap_filter {
constexpr const char *vertex=R"GLSL(#version 330 core
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}
)GLSL";
constexpr const char *fragment=R"GLSL(#version 330 core
out vec4 color;uniform samplerCube uSource;uniform int uFace,uSide;uniform float uRough,uSourceSide;
const float PI=3.14159265359;
vec3 direction(vec2 p){if(uFace==0)return normalize(vec3(1.,-p.y,-p.x));if(uFace==1)return normalize(vec3(-1.,-p.y,p.x));if(uFace==2)return normalize(vec3(p.x,1.,p.y));if(uFace==3)return normalize(vec3(p.x,-1.,-p.y));if(uFace==4)return normalize(vec3(p.x,-p.y,1.));return normalize(vec3(-p.x,-p.y,-1.));}
float radical(uint b){b=(b<<16u)|(b>>16u);b=((b&0x55555555u)<<1u)|((b&0xAAAAAAAAu)>>1u);b=((b&0x33333333u)<<2u)|((b&0xCCCCCCCCu)>>2u);b=((b&0x0F0F0F0Fu)<<4u)|((b&0xF0F0F0F0u)>>4u);b=((b&0x00FF00FFu)<<8u)|((b&0xFF00FF00u)>>8u);return float(b)*2.3283064365386963e-10;}
void main(){vec3 N=direction(gl_FragCoord.xy/float(uSide)*2.-1.);if(uRough<.0001){color=vec4(textureLod(uSource,N,0.).rgb,1.);return;}vec3 up=abs(N.z)<.999?vec3(0,0,1):vec3(1,0,0),T=normalize(cross(up,N)),B=cross(N,T);float a=uRough*uRough,a2=a*a,total=0.;vec3 result=vec3(0.);
for(uint i=0u;i<64u;i++){vec2 xi=vec2(float(i)/64.,radical(i));float phi=2.*PI*xi.x,ct=sqrt((1.-xi.y)/(1.+(a2-1.)*xi.y)),st=sqrt(max(0.,1.-ct*ct));vec3 H=normalize(T*cos(phi)*st+B*sin(phi)*st+N*ct),L=normalize(2.*dot(N,H)*H-N);float nl=max(dot(N,L),0.);if(nl>0.){float nh=max(dot(N,H),0.),vh=max(dot(N,H),.00001),D=a2/(PI*pow(nh*nh*(a2-1.)+1.,2.)),pdf=D*nh/(4.*vh)+.0001,sa=1./(64.*pdf+.0001),texel=4.*PI/(6.*uSourceSide*uSourceSide),mip=max(0.,.5*log2(sa/texel));result+=textureLod(uSource,L,mip).rgb*nl;total+=nl;}}
color=vec4(result/max(total,.00001),1.);}
)GLSL";
inline GLuint program(){GLuint vs=glCreateShader(GL_VERTEX_SHADER),fs=glCreateShader(GL_FRAGMENT_SHADER),p=0;glShaderSource(vs,1,&vertex,nullptr);glCompileShader(vs);glShaderSource(fs,1,&fragment,nullptr);glCompileShader(fs);GLint a=0,b=0;glGetShaderiv(vs,GL_COMPILE_STATUS,&a);glGetShaderiv(fs,GL_COMPILE_STATUS,&b);if(a&&b){p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);glGetProgramiv(p,GL_LINK_STATUS,&a);if(!a){glDeleteProgram(p);p=0;}}glDeleteShader(vs);glDeleteShader(fs);return p;}
}
#endif
