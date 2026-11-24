#ifndef VALWOOD_WORLD_RIGID_WEAPON_POSE_H
#define VALWOOD_WORLD_RIGID_WEAPON_POSE_H
#include <algorithm>
#include <array>
#include <cmath>
// Rendering-only rigid interpolation. The authored hand/grip follows the same
// linear path as the skinned hand; the weapon rotates about that moving grip.
// A normal vertex morph shrinks a rigid blade by cos(angle/2) at its midpoint.
namespace world_rigid_weapon_pose {
using Point=std::array<float,3>;
inline Point sub(const Point&a,const Point&b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline Point cross(const Point&a,const Point&b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline float dot(const Point&a,const Point&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline bool unit(Point &p){const float n=std::sqrt(dot(p,p));if(!(n>1.e-7f)||!std::isfinite(n))return false;for(float&v:p)v/=n;return true;}
inline bool rotation(const std::array<Point,3>&a,const std::array<Point,3>&b,float q[4]){
    Point ax=sub(a[1],a[0]),ay=sub(a[2],a[0]),bx=sub(b[1],b[0]),by=sub(b[2],b[0]);
    if(!unit(ax)||!unit(bx))return false;
    Point az=cross(ax,ay),bz=cross(bx,by);if(!unit(az)||!unit(bz))return false;
    ay=cross(az,ax);by=cross(bz,bx);
    float m[3][3];for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)m[r][c]=bx[r]*ax[c]+by[r]*ay[c]+bz[r]*az[c];
    const float trace=m[0][0]+m[1][1]+m[2][2];
    if(trace>0){const float k=std::sqrt(trace+1.f)*2;q[3]=.25f*k;q[0]=(m[2][1]-m[1][2])/k;q[1]=(m[0][2]-m[2][0])/k;q[2]=(m[1][0]-m[0][1])/k;}
    else{unsigned i=m[1][1]>m[0][0]?1:0;if(m[2][2]>m[i][i])i=2;const unsigned j=(i+1)%3,k=(i+2)%3;
        const float s=std::sqrt(std::max(0.f,1.f+m[i][i]-m[j][j]-m[k][k]))*2;if(!(s>1.e-7f))return false;
        q[i]=.25f*s;q[j]=(m[j][i]+m[i][j])/s;q[k]=(m[k][i]+m[i][k])/s;q[3]=(m[k][j]-m[j][k])/s;}
    float length=0;for(unsigned i=0;i<4;++i){if(!std::isfinite(q[i]))return false;length+=q[i]*q[i];}
    length=std::sqrt(length);if(!(length>.0001f))return false;
    const float sign=q[3]<0?-1.f:1.f;for(unsigned i=0;i<4;++i)q[i]*=sign/length;return true;
}
inline Point rotate(const Point&p,const float q[4]){const Point v={q[0],q[1],q[2]},t=cross(v,p),u=cross(v,t);return {p[0]+2*(q[3]*t[0]+u[0]),p[1]+2*(q[3]*t[1]+u[1]),p[2]+2*(q[3]*t[2]+u[2])};}
inline Point interpolate(const Point&p,const float pivot_a[3],const float pivot_b[3],const float rotation[4],float weight){
    weight=std::clamp(weight,0.f,1.f);float q[4]={rotation[0]*weight,rotation[1]*weight,rotation[2]*weight,1+(rotation[3]-1)*weight};
    float n=0;for(float v:q)n+=v*v;n=std::sqrt(n);for(float&v:q)v/=std::max(n,1.e-7f);
    const auto out=rotate({p[0]-pivot_a[0],p[1]-pivot_a[1],p[2]-pivot_a[2]},q);
    return {out[0]+pivot_a[0]+(pivot_b[0]-pivot_a[0])*weight,out[1]+pivot_a[1]+(pivot_b[1]-pivot_a[1])*weight,out[2]+pivot_a[2]+(pivot_b[2]-pivot_a[2])*weight};
}
}
#endif
