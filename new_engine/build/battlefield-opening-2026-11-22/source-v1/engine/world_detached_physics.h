#ifndef VALWOOD_WORLD_DETACHED_PHYSICS_H
#define VALWOOD_WORLD_DETACHED_PHYSICS_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
// Small render-independent rigid-piece solver. Shape points are the exact
// frozen, clipped surface, centred at the mass proxy, never a sampled envelope.
namespace world_detached_physics {
struct Vec3 {double x=0,y=0,z=0;};
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator-(Vec3 a){return {-a.x,-a.y,-a.z};}
inline Vec3 operator*(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
inline Vec3 operator/(Vec3 a,double s){return a*(1./s);}
inline double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double length(Vec3 a){return std::sqrt(dot(a,a));}
inline bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline Vec3 unit(Vec3 a,Vec3 fallback={0,1,0}){const double l=length(a);return l>1e-10&&std::isfinite(l)?a/l:fallback;}
struct Quat {double x=0,y=0,z=0,w=1;};
inline bool finite(Quat q){return std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&std::isfinite(q.w);}
inline Quat normalized(Quat q){const double n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return n>1e-12&&std::isfinite(n)?Quat{q.x/n,q.y/n,q.z/n,q.w/n}:Quat{};}
inline Quat multiply(Quat a,Quat b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline Vec3 rotate(Quat q,Vec3 v){const Vec3 u={q.x,q.y,q.z};return v+cross(u,v)*(2*q.w)+cross(u,cross(u,v))*2;}
inline Quat yaw(double a){return {0,std::sin(a*.5),0,std::cos(a*.5)};}
inline void clamp_length(Vec3 &v,double cap){const double l=length(v);if(l>cap)v=v*(cap/l);}
struct Shape {const Vec3 *points=nullptr;std::size_t count=0;};
struct Body {Vec3 position={},velocity={},omega={},inverse_inertia={1,1,1};Quat orientation={};double inverse_mass=1,accumulator=0,quiet=0;bool sleeping=false;};
struct Stats {std::uint64_t substeps=0,contacts=0,sleeps=0,clamps=0,invalid=0,floor_samples=0;double penetration=0;};
using FloorFn=double(*)(double x,double z,void *context);
struct Floor {Vec3 normal={0,1,0},point={};double height=0;};
inline double floor_height(FloorFn fn,void *ctx,double x,double z,double fallback,Stats &s){if(!fn)return fallback;++s.floor_samples;const double h=fn(x,z,ctx);return std::isfinite(h)?h:fallback;}
inline Floor floor_plane(FloorFn fn,void *ctx,Vec3 p,double fallback,Stats &s){
 Floor f;f.height=floor_height(fn,ctx,p.x,p.z,fallback,s);f.point={p.x,f.height,p.z};if(!fn)return f;
 constexpr double d=.18;const double px=floor_height(fn,ctx,p.x+d,p.z,f.height,s),nx=floor_height(fn,ctx,p.x-d,p.z,f.height,s),pz=floor_height(fn,ctx,p.x,p.z+d,f.height,s),nz=floor_height(fn,ctx,p.x,p.z-d,f.height,s);
 // Discontinuous floors remain horizontal; do not invent a wall normal.
 const double sx=(px-nx)/(2*d),sz=(pz-nz)/(2*d);if(std::abs(sx)<1.5&&std::abs(sz)<1.5)f.normal=unit({-sx,1,-sz});return f;
}
struct Support {Vec3 point={};std::array<Vec3,4> contacts={};unsigned count=0;double distance=0;};
inline Support support(const Body &b,Shape shape,Vec3 n={0,1,0}){
 Support out;out.distance=std::numeric_limits<double>::infinity();
 const Quat inverse={-b.orientation.x,-b.orientation.y,-b.orientation.z,b.orientation.w};const Vec3 local=rotate(inverse,n);
 for(std::size_t i=0;i<shape.count;++i){const double d=dot(local,shape.points[i]);if(d<out.distance){out.distance=d;out.point=shape.points[i];}}
 // Four extremal points of a small contact manifold stop artificial
 // corner rocking. The collision minimum still uses every exact vertex.
 std::array<std::size_t,4> ids={};double best[4]={1e99,-1e99,1e99,-1e99};
 const Vec3 ax=rotate(inverse,{1,0,0}),az=rotate(inverse,{0,0,1});
 for(std::size_t i=0;i<shape.count;++i)if(dot(local,shape.points[i])<=out.distance+.006){const double x=dot(ax,shape.points[i]),z=dot(az,shape.points[i]);if(x<best[0]){best[0]=x;ids[0]=i;}if(x>best[1]){best[1]=x;ids[1]=i;}if(z<best[2]){best[2]=z;ids[2]=i;}if(z>best[3]){best[3]=z;ids[3]=i;}}
 for(unsigned i=0;i<4;++i){bool duplicate=false;for(unsigned j=0;j<i;++j)duplicate|=ids[i]==ids[j];if(!duplicate)out.contacts[out.count++]=rotate(b.orientation,shape.points[ids[i]]);}
 out.point=rotate(b.orientation,out.point);return out;
}
inline Vec3 inverse_inertia(const Body &b,Vec3 v){const Quat inverse={-b.orientation.x,-b.orientation.y,-b.orientation.z,b.orientation.w};v=rotate(inverse,v);return rotate(b.orientation,{v.x*b.inverse_inertia.x,v.y*b.inverse_inertia.y,v.z*b.inverse_inertia.z});}
inline void impulse(Body &b,Vec3 j,Vec3 r){b.velocity=b.velocity+j*b.inverse_mass;b.omega=b.omega+inverse_inertia(b,cross(r,j));clamp_length(b.omega,18);}
inline void initialize(Body &b,Shape shape,double mass){
 mass=std::clamp(mass,.1,100.);b.inverse_mass=1/mass;Vec3 lo={1e9,1e9,1e9},hi={-1e9,-1e9,-1e9};
 for(std::size_t i=0;i<shape.count;++i){const auto &p=shape.points[i];lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}
 const Vec3 d=hi-lo;const double floor=.001; b.inverse_inertia={1/std::max(floor,mass*(d.y*d.y+d.z*d.z)/12),1/std::max(floor,mass*(d.x*d.x+d.z*d.z)/12),1/std::max(floor,mass*(d.x*d.x+d.y*d.y)/12)};
 b.orientation=normalized(b.orientation);
}
inline void update(Body &b,Shape shape,double dt,FloorFn fn,void *ctx,double &floor,Stats &s){
 if(!std::isfinite(dt)||dt<0||!shape.points||!shape.count||shape.count>240000||!finite(b.position)||!finite(b.velocity)||!finite(b.omega)||!finite(b.orientation)){++s.invalid;return;}
 if(dt==0||b.sleeping)return;if(dt>.05){dt=.05;++s.clamps;}b.accumulator+=dt;
 constexpr double step=1./120.;unsigned steps=0;
 while(b.accumulator+1e-12>=step&&steps<6){b.accumulator=std::max(0.,b.accumulator-step);++steps;++s.substeps;
  const Body previous=b;b.velocity.y-=9.81*step;b.position=b.position+b.velocity*step;
  const double speed=length(b.omega),half=speed*step*.5;const Vec3 axis=speed>1e-10?b.omega/speed:Vec3{};
  b.orientation=normalized(multiply({axis.x*std::sin(half),axis.y*std::sin(half),axis.z*std::sin(half),std::cos(half)},b.orientation));
  const Floor ground=floor_plane(fn,ctx,b.position,floor,s);floor=ground.height;const Vec3 n=ground.normal;bool touching=false;
  for(unsigned iteration=0;iteration<3;++iteration){const auto low=support(b,shape,n);const double gap=dot(b.position-ground.point,n)+low.distance;
   if(gap>.0008)break;touching=true;++s.contacts;if(gap<0)b.position=b.position+n*(-gap+1e-6);
   for(unsigned manifold=0;manifold<low.count;++manifold){const Vec3 r=low.contacts[manifold],contact_velocity=b.velocity+cross(b.omega,r);const double vn=dot(contact_velocity,n);
   const double denominator=b.inverse_mass+dot(n,cross(inverse_inertia(b,cross(r,n)),r));double normal_j=0;
   if(vn<0&&denominator>1e-10){normal_j=-(1+(vn<-.7?.14:0.))*vn/denominator;impulse(b,n*normal_j,r);}
   const Vec3 after=b.velocity+cross(b.omega,r),tangent=after-n*dot(after,n);const double vt=length(tangent);
   if(vt>1e-8){const Vec3 t=tangent/vt;const double divisor=b.inverse_mass+dot(t,cross(inverse_inertia(b,cross(r,t)),r));const double load=std::max(normal_j,9.81*step*n.y/(b.inverse_mass*std::max(1u,low.count)));if(divisor>1e-10)impulse(b,t*(-std::min(vt/divisor,.65*load)),r);}
   }
  }
  b.omega=b.omega*std::exp(-(touching?6.:.2)*step);if(touching){const double rolling=length(b.omega);if(rolling>1e-10)b.omega=b.omega*(std::max(0.,rolling-6.*step)/rolling);b.velocity.x*=std::exp(-.7*step);b.velocity.z*=std::exp(-.7*step);}
  // Exact lowest point is corrected again after all impulses. Rotation is
  // never derived from age and never frozen merely because time elapsed.
  const auto low=support(b,shape,n);const double gap=dot(b.position-ground.point,n)+low.distance;if(gap<0)b.position=b.position+n*(-gap+1e-6);
  s.penetration=std::max(s.penetration,std::max(0.,-(dot(b.position-ground.point,n)+low.distance)));
  if(gap<.006&&length(b.velocity)<.075&&length(b.omega)<.20)b.quiet+=step;else b.quiet=0;
  if(b.quiet>=.30){if(gap>0)b.position=b.position-n*gap;b.sleeping=true;b.velocity={};b.omega={};++s.sleeps;b.accumulator=0;break;}
  if(!finite(b.position)||!finite(b.velocity)||!finite(b.omega)||!finite(b.orientation)){b=previous;b.velocity={};b.omega={};b.sleeping=true;++s.invalid;break;}
 }
}
}
#endif
