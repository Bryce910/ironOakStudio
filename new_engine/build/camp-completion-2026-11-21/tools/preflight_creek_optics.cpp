#include "engine/world_studio.h"
#include "engine/world_river_network.h"
#include "engine/world_pool_reflection_visibility.h"
#include <cstdio>
#include <cstring>
#include <cmath>
struct V3{float x,y,z;};
    V3 add(V3 a, V3 b) {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    V3 mul(V3 a, float b) {
        return {a.x * b, a.y * b, a.z * b};
    }

    float dot(V3 a, V3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    V3 cross(V3 a, V3 b) {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }

    V3 norm(V3 a) {
        return mul(a, 1.f / std::max(std::sqrt(dot(a, a)), .00001f));
    }

    struct M4 {
        float m[16];
    };

    M4 identity() {
        M4 r = {};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
        return r;
    }

    M4 product(M4 a, M4 b) {
        M4 r = {};
        for (int c = 0; c < 4; c++)
            for (int row = 0; row < 4; row++)
                for (int k = 0; k < 4; k++)
                    r.m[c * 4 + row] += a.m[k * 4 + row] * b.m[c * 4 + k];
        return r;
    }

    M4 perspective(float fov, float aspect, float near, float far) {
        M4 r = {};
        float f = 1 / std::tan(fov * .5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (far + near) / (near - far);
        r.m[11] = -1;
        r.m[14] = 2 * far * near / (near - far);
        return r;
    }

    M4 look(V3 eye, V3 target, V3 up) {
        V3 f = norm(add(target, mul(eye, -1))), s = norm(cross(f, up)), u = cross(s, f);
        M4 r = identity();
        r.m[0] = s.x;
        r.m[4] = s.y;
        r.m[8] = s.z;
        r.m[1] = u.x;
        r.m[5] = u.y;
        r.m[9] = u.z;
        r.m[2] = -f.x;
        r.m[6] = -f.y;
        r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] = dot(f, eye);
        return r;
    }
double smooth(double a,double b,double x){x=std::clamp((x-a)/(b-a),0.,1.);return x*x*(3-2*x);}
int main(int argc,char**argv){world_studio::Document d;std::string error;if(!world_studio::load(argv[1],d,error)){fprintf(stderr,"%s",error.c_str());return 1;}
std::vector<world_surface::BakedRibbon>r;for(auto&a:d.surfaces.ribbons){world_surface::BakedRibbon b;if(!world_surface::bake_ribbon(a,b,error))return 1;r.push_back(b);}world_surface::RiverNetwork net;if(!net.prepare(r,error))return 1;
double cx=-12008,cy=14,cz=5184;float yaw=-1.249,pitch=-.30;V3 f={sin(yaw)*cos(pitch),sin(pitch),-cos(yaw)*cos(pitch)};M4 vp=product(perspective(1.0471975512,16.f/9,.4,15000),look({0,0,0},f,{0,1,0}));
double nearest=1e30,level=0,bx=0,bz=0;unsigned considered=0;for(const auto&s:net.segments){double dx=s.b.x-s.a.x,dz=s.b.z-s.a.z,t=std::clamp(((cx-s.a.x)*dx+(cz-s.a.z)*dz)/(s.length*s.length),0.,1.),x=s.a.x+dx*t,z=s.a.z+dz*t,distance=std::max(0.,std::hypot(cx-x,cz-z)-s.half);if(distance>=nearest||distance>120)continue;auto hit=net.surface(x,z);if(!hit.inside||hit.grade>.02||cy<=hit.height+.04)continue;WorldPoolMainClipInput in;memcpy(in.vp,vp.m,sizeof(vp.m));in.pool_bounds[0]=float(x);in.pool_bounds[1]=float(z);in.pool_bounds[2]=in.pool_bounds[3]=1.f/float(std::max(2.,s.half));in.origin[0]=cx;in.origin[1]=cz;in.offset_y=-cy;in.water_level=hit.height;if(world_pool_main_quad_outside_clip(in))continue;considered++;nearest=distance;level=hit.height;bx=x;bz=z;}
printf("{\"selected_level\":%.9g,\"nearest_distance\":%.9g,\"selected_x\":%.9g,\"selected_z\":%.9g,\"valid_candidates\":%u,\"samples\":[",level,nearest,bx,bz,considered);unsigned n=0;for(double z:{5168.,5174.,5180.,5184.,5190.,5196.,5202.}){auto h=net.surface(-12018,z);double coverage=(1-smooth(.1,.4,abs(h.height-level)))*(1-smooth(.02,.06,h.grade));printf("%s{\"x\":-12018,\"z\":%.9g,\"height\":%.9g,\"grade\":%.9g,\"inside\":%s,\"planar_max_coverage\":%.9g}",n++?",":"",z,h.height,h.grade,h.inside?"true":"false",coverage);}puts("]}");}
