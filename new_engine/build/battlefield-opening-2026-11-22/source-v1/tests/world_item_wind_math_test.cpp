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


int main(){
    size_t samples=0,defaults=0,roots=0;float peak=0;
    const vec2 root(-1310.25f,-10206.5f);
    for(float height:{.2f,.71f,1.1f})for(float scale:{.16f,1.f,3.f})
    for(vec2 weather:{vec2(0),vec2(.6f,.4f),vec2(3,0),vec2(-5,3)})
    for(float sway:{0.f,.2f,.45f,.72f,.9f})for(float flutter:{0.f,.2f,.45f,.72f,.9f})
    for(unsigned tick=0;tick<40;tick++)for(float y:{0.f,height*.2f,height*.55f,height*.9f,height}) {
        const float time=tick*.217f,H=height*scale;
        const auto a=rootedGrassWindSplit(time,root,weather,height,H,y,sway,flutter);
        const float motion=length(a.displacement);assert(std::isfinite(motion));
        assert(a.displacement.y==0&&motion<=min(.18f,H*.22f)+.00001f);
        if(y==0){assert(motion==0&&length(a.derivative)==0&&a.speed_bound==0);++roots;}
        if(sway==0&&flutter==0)assert(motion==0&&a.speed_bound==0);
        if(sway==flutter){const auto b=rootedGrassWind(time,root,weather,height,H,y,sway);assert(!std::memcmp(&a,&b,sizeof(a)));++defaults;}
        constexpr float dt=.0005f;
        const auto b=rootedGrassWindSplit(time+dt,root,weather,height,H,y,sway,flutter);
        const float measured=length(b.displacement-a.displacement)/dt;
        assert(std::isfinite(a.speed_bound)&&a.speed_bound>=0&&measured<=a.speed_bound*1.006f+.0003f);
        peak=max(peak,motion);++samples;
    }
    const auto no_sway=rootedGrassWindSplit(1.7f,root,{2,1},.71f,.71f,.71f,0,.45f);
    const auto no_flutter=rootedGrassWindSplit(1.7f,root,{2,1},.71f,.71f,.71f,.45f,0);
    assert(length(no_sway.displacement)>.001f&&length(no_flutter.displacement)>.001f);
    assert(length(no_sway.displacement-no_flutter.displacement)>.001f);
    std::printf("PASS production split grass GLSL CPU math: %zu bounded/speed samples, %zu byte-exact equal/default samples, %zu fixed roots, independent channels and off; peak %.6fm\n",samples,defaults,roots,peak);
}
