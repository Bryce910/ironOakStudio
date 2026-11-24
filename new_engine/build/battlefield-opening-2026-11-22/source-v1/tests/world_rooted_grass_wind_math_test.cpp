// Compile the production GLSL deformation directly as C++ and verify its
// geometric invariants against finite-difference positions; no GL/GPU needed.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
struct vec2 {
    float x,y;
    vec2(float a=0):x(a),y(a){} vec2(float a,float b):x(a),y(b){}
    vec2 &operator*=(float s){x*=s;y*=s;return *this;}
};
struct vec3 {
    float x,y,z;
    vec3(float a=0):x(a),y(a),z(a){} vec3(float a,float b,float c):x(a),y(b),z(c){}
};
vec2 operator+(vec2 a,vec2 b){return {a.x+b.x,a.y+b.y};}
vec2 operator*(vec2 a,float b){return {a.x*b,a.y*b};}
vec2 operator*(float b,vec2 a){return a*b;}
vec2 operator/(vec2 a,float b){return {a.x/b,a.y/b};}
vec3 operator+(vec3 a,vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
vec3 operator-(vec3 a,vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
vec3 operator*(vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
vec3 operator/(vec3 a,float b){return {a.x/b,a.y/b,a.z/b};}
float dot(vec2 a,vec2 b){return a.x*b.x+a.y*b.y;}
float dot(vec3 a,vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float length(vec2 v){return std::sqrt(dot(v,v));}
float length(vec3 v){return std::sqrt(dot(v,v));}
vec2 normalize(vec2 v){return v/length(v);}
vec3 normalize(vec3 v){return v/length(v);}
vec3 cross(vec3 a,vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float min(float a,float b){return std::min(a,b);}
float max(float a,float b){return std::max(a,b);}
float clamp(float v,float a,float b){return std::clamp(v,a,b);}
#include "shaders/world_rooted_grass_wind.glsl"

vec3 transform(vec3 v,float yaw,float pitch,float roll){
    const float cz=std::cos(roll),sz=std::sin(roll),cx=std::cos(pitch),sx=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
    v={cz*v.x-sz*v.y,sz*v.x+cz*v.y,v.z};v={v.x,cx*v.y-sx*v.z,sx*v.y+cx*v.z};return {cy*v.x+sy*v.z,v.y,-sy*v.x+cy*v.z};
}
bool same(vec3 a,vec3 b){return !std::memcmp(&a,&b,sizeof(a));}

int main(){
    size_t roots=0,samples=0,jacobians=0;float peak=0,worst=0;
    const vec2 root(-1310.25f,-10206.5f);
    for(float height:{.2f,.71f,1.1f})for(float scale:{.16f,.72f,1.f,1.35f,3.f})for(float axisY:{.4f,1.f,2.f})
    for(float pitch:{-.60f,0.f,.60f})for(float roll:{-.4f,0.f,.4f}) {
        const float H=height*scale*axisY;const vec3 up=transform({0,1,0},.73f,pitch,roll);
        for(vec2 weather:{vec2(0),vec2(.6f,.4f),vec2(3,0),vec2(-5,3)})for(float wind:{0.f,.45f,1.5f}) {
            for(unsigned tick=0;tick<60;tick++){
                const float time=tick*.217f;
                for(vec3 local:{vec3(-1.5f,0,-1.f),vec3(1.2f,0,1.1f)}){
                    const vec3 posed=transform(local*scale,.73f,pitch,roll);auto warp=rootedGrassWind(time,root,weather,height,H,0,wind);
                    assert(same(posed,posed+warp.displacement));assert(length(warp.derivative)==0);roots++;
                }
                for(float y:{height*.2f,height*.55f,height*.9f,height}){
                    auto warp=rootedGrassWind(time,root,weather,height,H,y,wind);const float motion=length(warp.displacement);
                    assert(std::isfinite(motion)&&warp.displacement.y==0&&motion<=min(.18f,H*.22f)+.00001f);
                    // Independent time differences check the actual GLSL
                    // temporal speed bound, including transitions onto its cap.
                    constexpr float dt=.0005f;
                    const auto next=rootedGrassWind(time+dt,root,weather,height,H,y,wind);
                    const float measured=length(next.displacement-warp.displacement)/dt;
                    assert(std::isfinite(warp.speed_bound)&&warp.speed_bound>=0.f);
                    assert(measured<=warp.speed_bound*1.006f+.0003f);
                    assert(1+dot(up,warp.derivative)>.55f);if(wind==0)assert(motion==0);peak=max(peak,motion);samples++;
                }
            }
        }
        // Independent positional finite differences validate inverse-transpose
        // normals/tangents under full yaw/pitch/roll and nonuniform Y scales.
        const vec3 normal=normalize(vec3(.37f,.31f,.87f));const vec3 tangent=normalize(cross(normal,{0,1,0})),bitangent=cross(normal,tangent);
        const vec3 p=up*(H*.55f);const float epsilon=H*.002f;
        auto deform=[&](vec3 q){const float y=dot(q,up)*height/H;return q+rootedGrassWind(2.31f,root,{1.4f,.7f},height,H,y,.45f).displacement;};
        auto derivative=[&](vec3 direction){return (deform(p+direction*epsilon)-deform(p-direction*epsilon))/(2*epsilon);};
        const auto warp=rootedGrassWind(2.31f,root,{1.4f,.7f},height,H,height*.55f,.45f);
        const vec3 n=normalize(rootedGrassNormal(normal,warp.derivative,up));const vec3 t=rootedGrassTangent(tangent,warp.derivative,up);
        const vec3 finite_t=derivative(tangent),finite_b=derivative(bitangent);const vec3 finite_n=normalize(cross(finite_t,finite_b));
        const float error=length(n-finite_n);worst=max(worst,error);assert(error<.0003f);assert(length(t-finite_t)<.0003f);assert(std::abs(dot(n,t))<.00001f);jacobians++;
    }
    // A fixed camera must see measurable tip motion even in clear weather.
    float tipMin=100,tipMax=-100;vec3 first=rootedGrassWind(0,root,vec2(0),.71f,.71f,.71f,.45f).displacement;float span=0;
    for(unsigned k=0;k<480;k++){const auto d=rootedGrassWind(k/60.f,root,vec2(0),.71f,.71f,.71f,.45f).displacement;tipMin=min(tipMin,d.x);tipMax=max(tipMax,d.x);span=max(span,length(d-first));}
    assert(tipMax-tipMin>.045f&&span>.045f&&peak>.10f);
    // Wind phase uses stable world root, independent of floating-origin128m
    // rebasing. Repeat evaluation is bit-identical.
    vec2 a(-1408+97.75f,-10368+161.5f),b(-1280-30.25f,-10240+33.5f);
    const auto wa=rootedGrassWind(3.4f,a,{1,.5f},.71f,.71f,.5f,.45f),wb=rootedGrassWind(3.4f,b,{1,.5f},.71f,.71f,.5f,.45f);
    assert(same(wa.displacement,wb.displacement)&&same(wa.derivative,wb.derivative));
    std::printf("PASS production grass GLSL CPU math: %zu fixed root samples, %zu bounded poses and finite-difference time-speed bounds, %zu finite-difference TRS bases; peak %.6fm, clear-weather tip Xspan %.6fm, max normal error %.8f; neutral wind0 and128m rebase exact\n",roots,samples,jacobians,peak,tipMax-tipMin,worst);
}
