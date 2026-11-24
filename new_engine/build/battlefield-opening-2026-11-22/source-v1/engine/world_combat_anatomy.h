#ifndef VALWOOD_WORLD_COMBAT_ANATOMY_H
#define VALWOOD_WORLD_COMBAT_ANATOMY_H
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

enum class TrainingBodyPart : unsigned {None,Head,Torso,LeftArm,RightArm,LeftHand,RightHand,LeftLeg,RightLeg};
enum class TrainingWeaponHand : unsigned {None,Right,Left};
namespace world_combat_anatomy {
inline constexpr std::uint16_t bit(TrainingBodyPart p){return unsigned(p)>0&&unsigned(p)<=8?std::uint16_t(1u<<(unsigned(p)-1)):0;}
inline constexpr bool valid(TrainingBodyPart p){return unsigned(p)>0&&unsigned(p)<=8;}
inline constexpr bool removed(std::uint16_t mask,TrainingBodyPart p){
 return (mask&bit(p))||((mask&bit(TrainingBodyPart::Torso))&&(p==TrainingBodyPart::Head||p==TrainingBodyPart::LeftArm||p==TrainingBodyPart::RightArm||p==TrainingBodyPart::LeftHand||p==TrainingBodyPart::RightHand))||
  (p==TrainingBodyPart::LeftHand&&(mask&bit(TrainingBodyPart::LeftArm)))||(p==TrainingBodyPart::RightHand&&(mask&bit(TrainingBodyPart::RightArm)));
}
inline constexpr TrainingWeaponHand surviving_hand(std::uint16_t mask){return !removed(mask,TrainingBodyPart::RightHand)?TrainingWeaponHand::Right:!removed(mask,TrainingBodyPart::LeftHand)?TrainingWeaponHand::Left:TrainingWeaponHand::None;}
inline constexpr bool limb(TrainingBodyPart p){return unsigned(p)>=unsigned(TrainingBodyPart::LeftArm)&&unsigned(p)<=unsigned(TrainingBodyPart::RightLeg);}
inline constexpr bool surviving_limb(TrainingBodyPart p){return p==TrainingBodyPart::LeftArm||p==TrainingBodyPart::RightArm||p==TrainingBodyPart::LeftHand||p==TrainingBodyPart::RightHand;}
inline constexpr bool can_move(std::uint16_t m){return !(m&(bit(TrainingBodyPart::LeftLeg)|bit(TrainingBodyPart::RightLeg)|bit(TrainingBodyPart::Torso)|bit(TrainingBodyPart::Head)));}
inline constexpr bool can_fight(std::uint16_t m){return surviving_hand(m)!=TrainingWeaponHand::None&&!(m&(bit(TrainingBodyPart::Head)|bit(TrainingBodyPart::Torso)|bit(TrainingBodyPart::LeftLeg)|bit(TrainingBodyPart::RightLeg)));}

// All points use one common WORLD frame in double metres. Capsules are posed
// at the authoritative impact time; a Sweep is one short interval of the baked
// visible cutting edge, not a camera ray or an attack-direction proxy.
struct Vec3 {double x=0,y=0,z=0;};
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
inline double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline double length(Vec3 a){return std::sqrt(dot(a,a));}
inline bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&std::abs(a.x)<=1e7&&std::abs(a.y)<=1e7&&std::abs(a.z)<=1e7;}
struct Edge {Vec3 base={},tip={};};
struct Sweep {Edge from={},to={};double radius=.012;};
struct Capsule {TrainingBodyPart part=TrainingBodyPart::None;Vec3 a={},b={};double radius=.1;};
struct Contact {bool hit=false;TrainingBodyPart part=TrainingBodyPart::None;Vec3 point={};double time=1;};
struct Closest {Vec3 edge={},body={};double distance=0;};
inline Closest closest(Vec3 p,Vec3 q,Vec3 a,Vec3 b){
 const Vec3 u=q-p,v=b-a,w=p-a;const double uu=dot(u,u),vv=dot(v,v),uv=dot(u,v),uw=dot(u,w),vw=dot(v,w);
 double s=0,t=0;
 if(uu<=1e-20&&vv<=1e-20)return {p,a,length(p-a)};
 if(uu<=1e-20)t=std::clamp(vw/vv,0.,1.);
 else if(vv<=1e-20)s=std::clamp(-uw/uu,0.,1.);
 else{
  const double denom=uu*vv-uv*uv;s=denom>1e-20?std::clamp((uv*vw-uw*vv)/denom,0.,1.):0.;t=(uv*s+vw)/vv;
  if(t<0){t=0;s=std::clamp(-uw/uu,0.,1.);}else if(t>1){t=1;s=std::clamp((uv-uw)/uu,0.,1.);}
 }
 const Vec3 x=p+u*s,y=a+v*t;return {x,y,length(x-y)};
}
inline Contact sweep_contact(const Sweep &s,const Capsule *capsules,std::size_t count,std::uint16_t severed=0){
 Contact result;
 if(!capsules||!count||count>24||!finite(s.from.base)||!finite(s.from.tip)||!finite(s.to.base)||!finite(s.to.tip)||!std::isfinite(s.radius)||s.radius<0||s.radius>.12||length(s.from.tip-s.from.base)>4||length(s.to.tip-s.to.base)>4)return result;
 const Vec3 db=s.to.base-s.from.base,dt=s.to.tip-s.from.tip;const double speed=std::max(length(db),length(dt));
 // Segment distance is Lipschitz in time with this endpoint-speed bound.
 // Conservative advancement therefore cannot skip a thin hand/neck capsule.
 constexpr double epsilon=1e-5;
 for(std::size_t i=0;i<count;++i){const auto &c=capsules[i];
  // None is reserved for a caller's actual guard-weapon capsule; it can
  // intercept an edge but can never acquire a severable body-part bit.
  if(unsigned(c.part)>8||removed(severed,c.part)||!finite(c.a)||!finite(c.b)||length(c.b-c.a)>3||!std::isfinite(c.radius)||c.radius<.015||c.radius>.65)continue;
  double time=0;const double radius=c.radius+s.radius;
  for(unsigned iteration=0;iteration<512;++iteration){
   const auto d=closest(s.from.base+db*time,s.from.tip+dt*time,c.a,c.b);
   const double gap=d.distance-radius;
   if(gap<=epsilon){
    // Stable ties prefer the existing capsule order, supplied by anatomy.
    if(!result.hit||time<result.time-1e-9){Vec3 n=d.edge-d.body;const double l=length(n);n=l>1e-12?n*(1./l):Vec3{0,1,0};result={true,c.part,d.body+n*c.radius,time};}
    break;
   }
   if(speed<=1e-12)break;
   const double advance=gap/speed;if(time+advance>1.+1e-12)break;
   const double next=std::min(1.,time+advance*.95);if(next<=time+1e-14)break;time=next;
  }
 }
 return result;
}
}
#endif
