#ifndef VALWOOD_WORLD_JOUST_FALL_PRIVATE_H
#define VALWOOD_WORLD_JOUST_FALL_PRIVATE_H
#include "engine/world_joust_rider_private.h"
#include <fstream>
#include <filesystem>
#include <vector>
namespace world_joust_fall_private {
using V=world_joust::Vec3;
struct PNT {V p,n,t;};
inline PNT mix(PNT a,PNT b,float w){
 using namespace world_joust_arm_private;PNT q{lerp(a.p,b.p,w),unit(lerp(a.n,b.n,w),a.n),{}};
 q.t=unit(lerp(a.t,b.t,w)-q.n*world_joust::dot(lerp(a.t,b.t,w),q.n),a.t);return q;
}
inline V stage_point(const world_joust::Rider&r,V impact,const std::array<V,4>&p){
 auto air=[&](float age){if(age<.28f)return impact+(p[0]-impact)*world_joust::smooth_unit(age/.28f);
  if(age<.65f)return p[0]+(p[1]-p[0])*world_joust::smooth_unit((age-.28f)/.37f);
  return p[1]+(p[2]-p[1])*world_joust::smooth_unit((age-.65f)/.30f);};
 const V a=air(r.fall_grounded?std::max(0.f,r.fall_age-r.fall_ground_time):r.fall_age);
 return r.fall_grounded?a+(p[3]-a)*r.fall_settle_progress:a;
}
// Source-rig skinning: fixed hierarchy translations preserve bone lengths.
// Only local rotations vary; the first short release blends the exact captured
// impact geometry into this original-weight skeleton. No horse vertex is used.
struct Q {float w=1,x=0,y=0,z=0;};
inline Q unit(Q q){const float s=1.f/std::sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);return {q.w*s,q.x*s,q.y*s,q.z*s};}
inline Q mul(Q a,Q b){return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};}
inline Q slerp(Q a,Q b,float t){float d=a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z;if(d<0){b={-b.w,-b.x,-b.y,-b.z};d=-d;}if(d>.9995f)return unit(Q{a.w+(b.w-a.w)*t,a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t});const float angle=std::acos(std::clamp(d,-1.f,1.f)),den=std::sin(angle);const float x=std::sin((1-t)*angle)/den,y=std::sin(t*angle)/den;return {a.w*x+b.w*y,a.x*x+b.x*y,a.y*x+b.y*y,a.z*x+b.z*y};}
inline Q inverse(Q q){return {q.w,-q.x,-q.y,-q.z};}
inline Q axis_angle(V axis,float angle){const float h=angle*.5f;return {std::cos(h),axis.x*std::sin(h),axis.y*std::sin(h),axis.z*std::sin(h)};}
inline Q between(V a,V b){const float d=std::clamp(world_joust::dot(a,b),-1.f,1.f);const V c=world_joust_arm_private::cross(a,b);if(d<-.9999f){const V axis=world_joust_arm_private::unit(world_joust_arm_private::cross(a,{0,1,0}),{0,0,1});return {0,axis.x,axis.y,axis.z};}return unit(Q{1+d,c.x,c.y,c.z});}
inline V rotate(Q q,V p){const V u={q.x,q.y,q.z};return p*(q.w*q.w-world_joust::dot(u,u))+u*(2*world_joust::dot(u,p))+world_joust_arm_private::cross(u,p)*(2*q.w);}
struct Rigid {Q q;V t,x={1,0,0},y={0,1,0},z={0,0,1};
 Rigid()=default;Rigid(Q rotation,V translation):q(rotation),t(translation),x(rotate(q,{1,0,0})),y(rotate(q,{0,1,0})),z(rotate(q,{0,0,1})){}
 V point(V p)const{return vector(p)+t;}V vector(V p)const{return x*p.x+y*p.y+z*p.z;}};
class Bank {
 struct Bone {int parent=-1;unsigned group=0,role=0;V local;std::array<Q,5>keys;Q inverse_rest;V rest;};
 struct Skin {PNT bind;std::array<unsigned short,8>bone;std::array<float,8>weight;};
 std::vector<PNT> data_;std::vector<Bone>bones_;std::vector<Skin>skin_;
public:
 struct Frame {std::vector<Rigid>global,baseline,skin,normal;std::array<float,6>release={},ages={},settles={};std::array<float,4> limb_excursion={};float max_secondary=0,phase_span=0,max_segment_error=0,max_hinge=0;};
 bool load(const std::filesystem::path &root){
  constexpr auto count=world_joust_rider_private::vertices.size()*4;
  std::ifstream in(root/"fall.pnt",std::ios::binary|std::ios::ate);if(!in||in.tellg()!=std::streamoff(count*sizeof(PNT)))return false;
  data_.resize(count);in.seekg(0);in.read(reinterpret_cast<char*>(data_.data()),count*sizeof(PNT));if(!in)return false;
  for(const auto&p:data_)if(!world_joust_arm_private::finite(p.p)||!world_joust_arm_private::finite(p.n)||!world_joust_arm_private::finite(p.t)||std::abs(world_joust::length(p.n)-1)>.001f||std::abs(world_joust::length(p.t)-1)>.001f)return false;
  std::ifstream rig(root/"fall.rig",std::ios::binary);char magic[8];unsigned n=0,v=0,keys=0,slots=0;rig.read(magic,8);rig.read((char*)&n,4);rig.read((char*)&v,4);rig.read((char*)&keys,4);rig.read((char*)&slots,4);
  if(!rig||std::string(magic,8)!="JWTRIG01"||n<10||n>256||v!=world_joust_rider_private::vertices.size()||keys!=4||slots!=8)return false;
  bones_.resize(n);skin_.resize(v);
  for(unsigned i=0;i<n;++i){auto&b=bones_[i];rig.read((char*)&b.parent,4);rig.read((char*)&b.group,4);rig.read((char*)&b.role,4);rig.read((char*)&b.local,sizeof(V));rig.read((char*)b.keys.data(),sizeof(Q)*5);float rest[16];rig.read((char*)rest,sizeof rest);if(!rig||b.parent>=int(i)||b.parent< -1||b.group>5||b.role>9||!world_joust_arm_private::finite(b.local))return false;
   // Rest transform is row-major. Its native orthonormal basis is converted
   // once to a quaternion; source/cook validation independently checks it.
   const float tr=rest[0]+rest[5]+rest[10];Q q;
   if(tr>0){const float z=std::sqrt(tr+1.f)*2;q={z*.25f,(rest[9]-rest[6])/z,(rest[2]-rest[8])/z,(rest[4]-rest[1])/z};}
   else if(rest[0]>rest[5]&&rest[0]>rest[10]){const float z=std::sqrt(1.f+rest[0]-rest[5]-rest[10])*2;q={(rest[9]-rest[6])/z,z*.25f,(rest[1]+rest[4])/z,(rest[2]+rest[8])/z};}
   else if(rest[5]>rest[10]){const float z=std::sqrt(1.f+rest[5]-rest[0]-rest[10])*2;q={(rest[2]-rest[8])/z,(rest[1]+rest[4])/z,z*.25f,(rest[6]+rest[9])/z};}
   else{const float z=std::sqrt(1.f+rest[10]-rest[0]-rest[5])*2;q={(rest[4]-rest[1])/z,(rest[2]+rest[8])/z,(rest[6]+rest[9])/z,z*.25f};}
   q=unit(q);b.inverse_rest={q.w,-q.x,-q.y,-q.z};b.rest={rest[3],rest[7],rest[11]};for(const auto&k:b.keys)if(!std::isfinite(k.w)||std::abs(k.w*k.w+k.x*k.x+k.y*k.y+k.z*k.z-1)>.002f)return false;
  }
  for(auto&s:skin_){rig.read((char*)&s.bind,sizeof(PNT));rig.read((char*)s.bone.data(),sizeof(unsigned short)*8);rig.read((char*)s.weight.data(),sizeof(float)*8);float sum=0;for(unsigned i=0;i<8;++i){if(s.bone[i]>=n||!std::isfinite(s.weight[i])||s.weight[i]<0||s.weight[i]>1.001f)return false;sum+=s.weight[i];}if(std::abs(sum-1)>.001f||!world_joust_arm_private::finite(s.bind.p))return false;}
  return bool(rig)&&rig.peek()==std::char_traits<char>::eof();
 }
 void clear(){data_.clear();bones_.clear();skin_.clear();}
 unsigned joint_count()const{return unsigned(bones_.size());}
 void prepare(Frame&f,const world_joust::Rider&r)const{
  f.global.resize(bones_.size());f.baseline.resize(bones_.size());f.skin.resize(bones_.size());f.normal.resize(bones_.size());f.max_secondary=0;f.phase_span=0;f.max_segment_error=0;f.max_hinge=0;f.limb_excursion={};
  const float strength=std::clamp(r.fall_impact_strength,0.f,1.f),side=world_joust::dot(r.fall_impact_direction,world_joust::rider_rotate({1,0,0},r.fall_yaw));
  const float air=r.fall_grounded?std::max(0.f,r.fall_age-r.fall_ground_time):r.fall_age;
  float low=1,high=0;
  auto&ages=f.ages;auto&settles=f.settles;
  for(unsigned g=0;g<6;++g){const float delay=g?(.018f*float(g)+.018f*side*(g%2?1.f:-1.f)):0.f;ages[g]=std::max(0.f,air-delay);settles[g]=r.fall_grounded?world_joust::smooth_unit(r.fall_ground_time/(.49f+.032f*float(g))):0;f.release[g]=world_joust::smooth_unit(std::max(0.f,r.fall_age-delay)/(.11f+.012f*float(g)));low=std::min(low,f.release[g]);high=std::max(high,f.release[g]);}f.phase_span=high-low;
  for(unsigned i=0;i<bones_.size();++i){const auto&b=bones_[i];const float age=ages[b.group];Q q=age<.37f?slerp(b.keys[1],b.keys[2],world_joust::smooth_unit(age/.37f)):slerp(b.keys[2],b.keys[3],world_joust::smooth_unit((age-.37f)/.36f));if(r.fall_grounded)q=slerp(q,b.keys[4],settles[b.group]);
   const Q baseline_q=q;
   if(b.role){
    // Source bones run along local X. Rotating X only twists armor; use
    // transverse character-space axes for actual joint bending and splay.
    // Distinct damped impulses make hands/feet lag independently of the body.
    auto pulse=[](float t,float delay,float frequency){t=std::max(0.f,t-delay);return 1.6f*std::sin(t*frequency)*std::exp(-1.35f*t);};
    const float onset=world_joust::smooth_unit(r.fall_age/.11f);
    const float air_fade=1-world_joust::smooth_unit((air-.70f)/.36f);
    const float ground_fade=r.fall_grounded?1-world_joust::smooth_unit(r.fall_ground_time/(.32f+.025f*b.group)):1.f;
    const float response=(.75f+.25f*strength)*onset*air_fade*ground_fade;
    float flex=0,spread=0;
    switch(b.role){
     case 1:flex=-.80f*pulse(air,.00f,5.2f);spread=-.65f*pulse(air,.02f,4.6f);break;
     case 2:flex=1.00f*pulse(air,.055f,6.2f);break;
     case 3:flex=-1.00f*pulse(air,.075f,4.7f);spread=.60f*pulse(air,.09f,5.2f);break;
     case 4:flex=-.85f*pulse(air,.12f,5.5f);break;
     case 5:flex=.38f*pulse(air,.055f,5.3f);break;
     case 6:flex=1.25f*std::max(0.f,pulse(air,.08f,5.9f));break;
     case 7:flex=-.92f*pulse(air,.02f,4.6f);break;
     case 8:flex=1.35f*std::max(0.f,pulse(air,.13f,5.1f));break;
     case 9:flex=.12f*pulse(air,.02f,6.f);break;
    }
    flex*=response*(1.f+.08f*side*(b.group%2?1.f:-1.f));spread*=response;
    if(r.fall_grounded){const float t=r.fall_ground_time;const float landing=std::sin(t*(b.group%2?13.f:16.f))*std::exp(-8.f*t)*(.18f+.12f*strength)*ground_fade;
     flex+=(b.role==6||b.role==8)?std::max(0.f,landing):landing*(b.group%2?1.f:-1.f);}
    flex=std::clamp(flex,-1.35f,1.40f);spread=std::clamp(spread,-.8f,.8f);
    f.max_secondary=std::max(f.max_secondary,std::hypot(flex,spread));
    const Q parent=b.parent>=0?f.global[unsigned(b.parent)].q:Q{};
    Q candidate=unit(mul(mul(axis_angle({0,0,1},spread),axis_angle({1,0,0},flex)),mul(parent,q)));
    if(b.role<=4){
     // Let released arms trail towards world gravity instead of retaining the
     // seated guarding silhouette. Each chain has its own release/lag, with
     // a controlled return before ground contact rather than a frozen grip.
     const bool right=b.group==1;const float delay=right?.045f:.12f;
     const V gravity=world_joust_rider_private::inverse_rotation(r,{0,-1,0});
     const bool upper=b.role==1||b.role==3;
     const V bias=upper?V{right?-.35f:.50f,0,right?-.25f:.25f}:V{right?-.12f:.15f,0,right?.25f:.15f};
     const V desired=world_joust_arm_private::unit(gravity*(upper?.85f:1.f)+bias,{0,-1,0});
     const float sag=(.70f+.18f*strength)*(1-std::exp(-7.f*std::max(0.f,air-delay)))*(1-world_joust::smooth_unit((air-.58f)/.28f))*ground_fade;
     const V current=rotate(candidate,{1,0,0});candidate=unit(mul(slerp(Q{},between(current,desired),sag),candidate));
    }
    if((b.role==2||b.role==4||b.role==6||b.role==8)&&std::abs(flex)>.000001f){
     const V upper=rotate(parent,{1,0,0}),lower=rotate(candidate,{1,0,0});
     const float limit=b.role<=4?2.53f:2.27f,angle=std::acos(std::clamp(world_joust::dot(upper,lower),-1.f,1.f));
     if(angle>limit){const V across=world_joust_arm_private::unit(lower-upper*world_joust::dot(upper,lower),{0,0,1});const V target=upper*std::cos(limit)+across*std::sin(limit);candidate=unit(mul(between(lower,target),candidate));}
     f.max_hinge=std::max(f.max_hinge,std::min(angle,limit));
    }
    q=unit(mul(inverse(parent),candidate));
   }
   Rigid global{q,b.local},base{baseline_q,b.local};if(b.parent>=0){const auto&p=f.global[unsigned(b.parent)],&bp=f.baseline[unsigned(b.parent)];global={unit(mul(p.q,q)),p.point(b.local)};base={unit(mul(bp.q,baseline_q)),bp.point(b.local)};f.max_segment_error=std::max(f.max_segment_error,std::abs(world_joust::length(global.t-p.t)-world_joust::length(b.local)));}f.global[i]=global;f.baseline[i]=base;const Q skinq=unit(mul(global.q,b.inverse_rest));f.skin[i]={skinq,global.t-rotate(skinq,b.rest)};const Q inv={base.q.w,-base.q.x,-base.q.y,-base.q.z};f.normal[i]={unit(mul(global.q,inv)),{}};
   if(b.parent>=0){const auto role=bones_[unsigned(b.parent)].role;if(role==2||role==4||role==6||role==8){const unsigned limb=role/2-1;f.limb_excursion[limb]=world_joust::length(global.t-base.t);}}
  }
 }
 PNT sample(std::size_t index,PNT impact,const world_joust::Rider&,const Frame&f)const{
  const auto&s=skin_[index];V p={},n={},t={};float release=0;
  const unsigned group=bones_[s.bone[0]].group;const auto count=world_joust_rider_private::vertices.size();const float age=f.ages[group];
  PNT original=age<.37f?mix(data_[index],data_[count+index],world_joust::smooth_unit(age/.37f)):mix(data_[count+index],data_[2*count+index],world_joust::smooth_unit((age-.37f)/.36f));original=mix(original,data_[3*count+index],f.settles[group]);
  for(unsigned k=0;k<8;++k)if(s.weight[k]>0){const auto&b=f.skin[s.bone[k]];const float w=s.weight[k];p=p+b.point(s.bind.p)*w;n=n+f.normal[s.bone[k]].vector(original.n)*w;t=t+f.normal[s.bone[k]].vector(original.t)*w;release+=f.release[bones_[s.bone[k]].group]*w;}
  n=world_joust_arm_private::unit(n,impact.n);t=world_joust_arm_private::unit(t-n*world_joust::dot(t,n),impact.t);return mix(impact,PNT{p,n,t},release);
 }
};
// Source socket keys generated from the same posed publisher rig as fall.pnt.
inline constexpr std::array<V,4> right_palms={{
{-0.377758576f,2.19732425f,0.356076063f},
{-0.43581297f,2.39905521f,0.270030039f},
{-0.339133612f,2.21218026f,0.285083755f},
{0.0510269749f,1.94794239f,0.186987441f},
}};
inline constexpr std::array<V,4> left_palms={{
{0.367992877f,2.12704259f,0.347257588f},
{0.419616003f,2.29322377f,0.309371129f},
{0.30818303f,2.12755731f,0.290809876f},
{-0.0292457656f,1.91050014f,0.0487202336f},
}};
inline constexpr std::array<V,4> heads={{
{-0.00331134938f,2.49031941f,-0.0422120609f},
{-0.00331134938f,2.49031941f,-0.0422120609f},
{-0.00331134938f,2.49031941f,-0.0422120609f},
{-0.00331134938f,2.49031941f,-0.0422120609f},
}};
}
#endif
