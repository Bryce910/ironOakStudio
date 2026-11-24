// Compile the production GLSL deformation directly as C++ and verify its
// geometric invariants against finite-difference positions; no GL/GPU needed.
#include <algorithm>
#include <cstdlib>
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
struct vec4 {float x,y,z,w;vec4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){}};
vec2 operator-(vec2 a,vec2 b){return {a.x-b.x,a.y-b.y};}
float smoothstep(float a,float b,float x){float t=clamp((x-a)/(b-a),0.f,1.f);return t*t*(3.f-2.f*t);}
#include "shaders/world_rooted_grass_wind.glsl"
#include "shaders/world_grass_interaction.glsl"
#include "engine/world_grass_interaction.h"

vec3 transform(vec3 v,float yaw,float pitch,float roll){
    const float cz=std::cos(roll),sz=std::sin(roll),cx=std::cos(pitch),sx=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
    v={cz*v.x-sz*v.y,sz*v.x+cz*v.y,v.z};v={v.x,cx*v.y-sx*v.z,sx*v.y+cx*v.z};return {cy*v.x+sy*v.z,v.y,-sy*v.x+cy*v.z};
}
bool same(vec3 a,vec3 b){return !std::memcmp(&a,&b,sizeof(a));}


int main(){
    size_t checks=0;float maxNormalError=0,maxContact=0;
    #define check(yes) do {++checks;if(!(yes)){std::fprintf(stderr,"FAIL line %d check %zu: %s\n",__LINE__,checks,#yes);std::abort();}} while(false)
    using world_grass_interaction::State;
    State s;
    for(unsigned frame=0;frame<180;++frame)s.update(1./60.,-11991.8,11.8,5181.9+frame*.02,true,true);
    check(s.history_samples>=10&&s.peak_contacts==4&&s.moving_frames==179);
    check(s.travel>3.57&&s.travel<3.59);
    // Every precise radial contact lies inside the cheap packed broadphase,
    // including negative world coordinates, the radius boundary and float error.
    for(unsigned k=0;k<6000;++k){
        vec2 point(float(-11996.+(k%100)*.08),float(5179.+(k/100)*.12));
        vec2 total=grassContactPush(point,11.8,vec4(s.head[0],s.head[1],s.head[2],1),.95);
        for(const auto &h:s.history)total=total+grassContactPush(point,11.8,vec4(h[0],h[1],h[2],h[3]),.95);
        vec2 delta=point-vec2(s.bounds[0],s.bounds[1]);
        if(length(total)>0)check(dot(delta,delta)<s.bounds[2]);
    }
    float previous=1;
    for(unsigned frame=0;frame<60;++frame){
        s.update(1./60.,s.last_x,s.last_y,s.last_z,true,true);
        check(s.history[0][3]<=previous);previous=s.history[0][3];
    }
    check(s.active_history==0&&s.recovery_frames>0&&s.head[3]==.95f);
    auto sample=[&](double x,double y,double z){s.update(1./60.,x,y,z,true,true);};
    for(unsigned frame=0;frame<90;++frame)sample(s.last_x+.01,s.last_y,s.last_z);
    sample(s.last_x+100,s.last_y,s.last_z);check(s.active_history==0);
    sample(s.last_x,s.last_y+5,s.last_z);check(s.active_history==0);
    s.update(1./60.,0,0,0,false,true);check(s.head[3]==0&&s.bounds[2]==0&&s.active_history==0);
    s.update(1./60.,0,0,0,true,false);check(s.head[3]==0);
    for(int kind=0;kind<7;++kind){
        check(grassContactEligible(kind,.8,true)==(kind==2||kind==3||kind==5));
        check(!grassContactEligible(kind,.8,false));
    }
    check(grassContactEligible(3,1.1,true)&&!grassContactEligible(3,1.1001,true));
    check(grassContactEligible(3,2.f*.5f,true)); // actual world height, not model height
    // Soil anchors are unaffected by contact or complete player overlap.
    for(float height:{.18f,.44f,.95f})for(float scale:{.15f,.7f,1.f,2.f})for(float axisY:{.5f,1.f,1.8f})
    for(float pitch:{-.55f,0.f,.55f})for(float roll:{-.4f,.4f})for(float strength:{0.f,.3f,1.f,3.f}){
        float H=height*scale*axisY;
        vec3 up=transform({0,1,0},.72f,pitch,roll);
        vec2 push(.7f*strength,-.5f*strength);
        auto warp=[&](float y){return grassContactCombine(rootedGrassWindSplit(2.3f,{-11990,5183},{1.4f,.7f},height,H,y,.4f,.8f),grassContactWarp(push,height,H,y,3.f),height,H,y);};
        auto root=warp(0);
        check(same(root.displacement,vec3(0))&&same(root.derivative,vec3(0)));
        for(float ratio:{.15f,.55f,.9f,1.f}){
            auto w=warp(height*ratio);
            check(std::isfinite(length(w.displacement))&&w.displacement.y==0);
            check(length(w.displacement)<=min(.30f,H*.40f)*ratio*ratio+.00001f);
            check(length(w.derivative)<=.80001f&&1+dot(up,w.derivative)>=.19999f);
            check(w.speed_bound>=0&&std::isfinite(w.speed_bound));
            maxContact=max(maxContact,length(w.displacement));
        }
        // Positions are evaluated independently along two arbitrary mesh
        // directions; compare the production normal/tangent to their cross.
        vec3 n=normalize(vec3(.37f,.31f,.87f));vec3 t=normalize(cross(n,{0,1,0})),b=cross(n,t);
        vec3 p=up*(H*.55f);float eps=H*.002f;
        auto deform=[&](vec3 q){return q+warp(dot(q,up)*height/H).displacement;};
        vec3 ft=(deform(p+t*eps)-deform(p-t*eps))/(2*eps),fb=(deform(p+b*eps)-deform(p-b*eps))/(2*eps);
        auto w=warp(height*.55f);
        vec3 actualN=normalize(rootedGrassNormal(n,w.derivative,up));
        vec3 actualT=rootedGrassTangent(t,w.derivative,up);
        float error=length(actualN-normalize(cross(ft,fb)));maxNormalError=max(maxNormalError,error);
        check(error<.0005f&&length(actualT-ft)<.0005f&&std::abs(dot(actualN,actualT))<.00002f);
        if(strength==0){auto original=rootedGrassWindSplit(2.3f,{-11990,5183},{1.4f,.7f},height,H,height*.55f,.4f,.8f);check(same(w.displacement,original.displacement)&&same(w.derivative,original.derivative)&&w.speed_bound==original.speed_bound);}
        // Low fern/shrub contact retains its legacy world-Y wind and adds
        // the actual slope-up contact Jacobian rather than world-Y normals.
        vec3 windTip(.012f,0,-.008f);auto contact=grassContactWarp(push,height,H,height*.55f,3.f);
        auto fern=[&](vec3 q){float h=clamp(q.y/H,0,1);return q+windTip*(h*h)+grassContactWarp(push,height,H,dot(q,up)*height/H,3.f).displacement;};
        ft=(fern(p+t*eps)-fern(p-t*eps))/(2*eps);fb=(fern(p+b*eps)-fern(p-b*eps))/(2*eps);
        vec3 wd=windTip*(2*p.y/(H*H));
        actualN=normalize(grassContactNormal(n,wd,contact.derivative,up));actualT=grassContactTangent(t,wd,contact.derivative,up);
        // Extremely tiny plants get proportionally smaller legacy wind here.
        if(H>.15f)check(length(actualN-normalize(cross(ft,fb)))<.0005f&&length(actualT-ft)<.0005f);
    }
    for(float x:{-.951f,-.3f,0.f,.3f,.951f}){
        auto push=grassContactPush({x,0},0,vec4(0,0,0,1),.95);
        check(std::isfinite(length(push))&&length(push)<=1.0001f);
        if(std::abs(x)>=.95)check(length(push)==0);
        check(length(grassContactPush({x,0},1,vec4(0,0,0,1),.95))==0); // other storey
        check(length(grassContactPush({x,0},0,vec4(0,0,0,0),.95))==0);
    }
    check(maxContact>.15f);
    std::printf("PASS actual production interaction GLSL+controller history: %zu checks, root/contact/wind TRS finite differences maxNormalError %.9f, peak bend %.6fm; four-contact bound, finite center, exact recovery/motion-off/other-storey and no-contact preservation\n",checks,maxNormalError,maxContact);
}
