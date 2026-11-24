#ifndef VALWOOD_WORLD_TRANSFORM_MATH_H
#define VALWOOD_WORLD_TRANSFORM_MATH_H
#include "engine/world_entity_graph.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
// Column-vector convention: translation * yaw(Y) * pitch(X) * roll(Z) * scale.
// Reflections/zero scale are deliberately excluded. TRS hierarchies cannot
// represent shear; decomposition rejects it instead of silently moving a mesh.
namespace vw_transform {
using T=world_entity::Transform;
using V=std::array<double,3>;
struct M {double a[3][3]={{1,0,0},{0,1,0},{0,0,1}};V p={};};
inline bool simple(const T &t){return t.pitch==0&&t.roll==0&&t.sx==1&&t.sy==1&&t.sz==1;}
inline bool identity(const T &t){return simple(t)&&t.x==0&&t.y==0&&t.z==0&&t.yaw==0&&t.scale==1;}
inline M matrix(const T &t){
    constexpr double d=.017453292519943295;const double cy=std::cos(t.yaw*d),sy=std::sin(t.yaw*d),cx=std::cos(t.pitch*d),sx=std::sin(t.pitch*d),cz=std::cos(t.roll*d),sz=std::sin(t.roll*d);
    M m;const double r[3][3]={{cy*cz+sy*sx*sz,-cy*sz+sy*sx*cz,sy*cx},{cx*sz,cx*cz,-sx},{-sy*cz+cy*sx*sz,sy*sz+cy*sx*cz,cy*cx}};
    const double s[]={t.scale*t.sx,t.scale*t.sy,t.scale*t.sz};for(unsigned i=0;i<3;i++)for(unsigned j=0;j<3;j++)m.a[i][j]=r[i][j]*s[j];m.p={t.x,t.y,t.z};return m;
}
inline V vector(const M &m,V v){V o={};for(unsigned i=0;i<3;i++)for(unsigned j=0;j<3;j++)o[i]+=m.a[i][j]*v[j];return o;}
inline V point(const M &m,V v){auto o=vector(m,v);for(unsigned i=0;i<3;i++)o[i]+=m.p[i];return o;}
inline M multiply(const M &p,const M &l){M o;for(unsigned i=0;i<3;i++)for(unsigned j=0;j<3;j++){o.a[i][j]=0;for(unsigned k=0;k<3;k++)o.a[i][j]+=p.a[i][k]*l.a[k][j];}o.p=point(p,l.p);return o;}
inline M inverse(const M &m){
    M o;double det=0;for(unsigned j=0;j<3;j++)det+=m.a[0][j]*(m.a[1][(j+1)%3]*m.a[2][(j+2)%3]-m.a[1][(j+2)%3]*m.a[2][(j+1)%3]);
    for(unsigned i=0;i<3;i++)for(unsigned j=0;j<3;j++)o.a[j][i]=(m.a[(i+1)%3][(j+1)%3]*m.a[(i+2)%3][(j+2)%3]-m.a[(i+1)%3][(j+2)%3]*m.a[(i+2)%3][(j+1)%3])/det;
    o.p=vector(o,{-m.p[0],-m.p[1],-m.p[2]});return o;
}
inline T decompose(const M &m){
    T t;t.x=m.p[0];t.y=m.p[1];t.z=m.p[2];double s[3]={};double r[3][3];
    for(unsigned j=0;j<3;j++){for(unsigned i=0;i<3;i++)s[j]+=m.a[i][j]*m.a[i][j];s[j]=std::sqrt(s[j]);for(unsigned i=0;i<3;i++)r[i][j]=m.a[i][j]/s[j];}
    for(unsigned j=0;j<3;j++)for(unsigned k=j+1;k<3;k++){double dot=0;for(unsigned i=0;i<3;i++)dot+=r[i][j]*r[i][k];if(!std::isfinite(dot)||std::abs(dot)>1e-7){t.scale=std::numeric_limits<double>::quiet_NaN();return t;}}
    constexpr double degrees=57.29577951308232;t.pitch=std::asin(std::clamp(-r[1][2],-1.,1.))*degrees;
    if(std::abs(std::cos(t.pitch/degrees))>1e-8){t.yaw=std::atan2(r[0][2],r[2][2])*degrees;t.roll=std::atan2(r[1][0],r[1][1])*degrees;}
    else{t.yaw=std::atan2(-r[2][0],r[0][0])*degrees;t.roll=0;}
    t.scale=std::cbrt(s[0]*s[1]*s[2]);t.sx=s[0]/t.scale;t.sy=s[1]/t.scale;t.sz=s[2]/t.scale;return t;
}
template<class P> inline T pose(const P &p){return {p.root.x,p.root.y,p.root.z,p.yaw*57.29577951308232,p.scale,p.pitch*57.29577951308232,p.roll*57.29577951308232,p.sx,p.sy,p.sz};}
template<class P,class Q> inline void extra(P &p,const Q &t){p.pitch=float(t.pitch);p.roll=float(t.roll);p.sx=float(t.sx);p.sy=float(t.sy);p.sz=float(t.sz);}
inline bool extra_valid(double pitch,double roll,double sx,double sy,double sz){return std::isfinite(pitch)&&std::abs(pitch)<=360&&std::isfinite(roll)&&std::abs(roll)<=360&&std::isfinite(sx)&&sx>=.1&&sx<=8&&std::isfinite(sy)&&sy>=.1&&sy<=8&&std::isfinite(sz)&&sz>=.1&&sz<=8;}
}
#endif
