#include "engine/world_skeletal_hero.h"
#include "engine/world_knight_appearance.h"
#include "engine/world_attack_foot_contact.h"
#include "engine/world_skeletal_attack_arc_cache.h"
namespace world_attack_foot_contact {static bool deform_position(const world_animation::SkinVertex&,const world_animation::Pose&,world_animation::Vec3&,std::string&);}
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <filesystem>
#include <cstring>
namespace {
using namespace world_animation;
constexpr double tau=6.283185307179586;
Vec3 vadd(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 vsub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 vmul(Vec3 a,float f){return {a.x*f,a.y*f,a.z*f};}
float vdot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float vlength(Vec3 a){return std::sqrt(vdot(a,a));}
Vec3 vcross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 vunit(Vec3 a){const float length=vlength(a);return length>1.e-9f?vmul(a,1/length):Vec3{1,0,0};}
Quat qconjugate(Quat q){return {-q.x,-q.y,-q.z,q.w};}
Quat qproduct(Quat a,Quat b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
Vec3 qrotate(Quat q,Vec3 p){const Quat rotated=qproduct(qproduct(q,{p.x,p.y,p.z,0}),qconjugate(q));return {rotated.x,rotated.y,rotated.z};}
Quat qunit(Quat q){const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return {q.x/n,q.y/n,q.z/n,q.w/n};}
Quat world_rotation(const Skeleton&s,const Pose&p,unsigned joint){Quat q=p.local[joint].rotation;for(int n=s.joints[joint].parent;n>=0;n=s.joints[unsigned(n)].parent)q=qproduct(p.local[unsigned(n)].rotation,q);return qunit(q);}
Quat between(Vec3 a,Vec3 b){a=vunit(a);b=vunit(b);const double d=std::clamp(double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z,-1.,1.);const Vec3 cross=vcross(a,b);if(d<-.999999&&vlength(cross)<.00001f){const Vec3 axis=vunit(vcross(a,std::abs(a.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0}));return {axis.x,axis.y,axis.z,0};}return qunit({cross.x,cross.y,cross.z,float(1+d)});}
// Quaternion tangent vectors use radians, with one fixed hemisphere about
// the latest observation; this keeps the fit continuous across q/-q keys.
Vec3 qlog(Quat q){q=qunit(q);if(q.w<0){q.x=-q.x;q.y=-q.y;q.z=-q.z;q.w=-q.w;}const double n=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z);return n<1.e-9?Vec3{2*q.x,2*q.y,2*q.z}:vmul({q.x,q.y,q.z},float(2*std::atan2(n,double(q.w))/n));}
Quat qexp(Vec3 v){const double n=vlength(v);if(n<1.e-9)return qunit({v.x*.5f,v.y*.5f,v.z*.5f,1});const double scale=std::sin(n*.5)/n;return {float(v.x*scale),float(v.y*scale),float(v.z*scale),float(std::cos(n*.5))};}
Vec3 origin(const Mat4&m){return {m.m[12],m.m[13],m.m[14]};}
bool inverse_point(const Mat4&matrix,Vec3 point,Vec3&out){const auto&m=matrix.m;const double a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10],det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(!std::isfinite(det)||std::abs(det)<1.e-12)return false;point=vsub(point,origin(matrix));out={float(((e*i-f*h)*point.x+(c*h-b*i)*point.y+(b*f-c*e)*point.z)/det),float(((f*g-d*i)*point.x+(a*i-c*g)*point.y+(c*d-a*f)*point.z)/det),float(((d*h-e*g)*point.x+(b*g-a*h)*point.y+(a*e-b*d)*point.z)/det)};return true;}
constexpr double attack_frames[5][3]={{40,53,65},{38,50,61},{31,44,56},{13,21,32},{17,32,43}};
double attack_arc_frame(unsigned direction,double index){const auto&f=attack_frames[direction];return index<=440?f[0]+(f[1]-f[0])*index/440:index<=880?f[1]+(f[2]-f[1])*(index-440)/440:f[2]+8*(index-880)/440;}
double monotone_segment(double a,double b,double va,double vb,double duration,double f){
 const double f2=f*f,f3=f2*f;return (2*f3-3*f2+1)*a+(f3-2*f2+f)*duration*va+(-2*f3+3*f2)*b+(f3-f2)*duration*vb;
}
double harmonic(double a,double b){return a>0&&b>0?2*a*b/(a+b):0;}
float smooth(float f){f=std::clamp(f,0.f,1.f);return f*f*(3-2*f);}
Quat mixq(Quat a,Quat b,float f){float d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;if(d<0){b.x=-b.x;b.y=-b.y;b.z=-b.z;b.w=-b.w;d=-d;}float aa=1-f,bb=f;if(d<.9995f){const float t=std::acos(std::clamp(d,-1.f,1.f)),s=std::sin(t);aa=std::sin((1-f)*t)/s;bb=std::sin(f*t)/s;}Quat q{aa*a.x+bb*b.x,aa*a.y+bb*b.y,aa*a.z+bb*b.z,aa*a.w+bb*b.w};const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);q.x/=n;q.y/=n;q.z/=n;q.w/=n;return q;}
bool mixpose(const Skeleton &s,const Pose&a,const Pose&b,float f,Pose&out,std::string&e){out=a;for(unsigned i=0;i<out.nodes;++i){auto &o=out.local[i];const auto &v=b.local[i];o.translation={o.translation.x+(v.translation.x-o.translation.x)*f,o.translation.y+(v.translation.y-o.translation.y)*f,o.translation.z+(v.translation.z-o.translation.z)*f};o.scale={o.scale.x+(v.scale.x-o.scale.x)*f,o.scale.y+(v.scale.y-o.scale.y)*f,o.scale.z+(v.scale.z-o.scale.z)*f};o.rotation=mixq(o.rotation,v.rotation,f);}return evaluate(s,out,e);}
// Remove only the captured attack's world-horizontal advance, preserving its
// vertical hip action, angular motion and all joint rotations. Parent bases may
// contain FBX conversion rotation/centimetre scale: local X/Z are NOT world X/Z.
bool strip_attack(const Skeleton&s,unsigned pelvis,const Pose&first,Pose&p,std::string&e){const auto &a=p.global[pelvis].m,&b=first.global[pelvis].m;const Vec3 d{a[12]-b[12],0,a[14]-b[14]};const int parent=s.joints[pelvis].parent;if(parent<0){p.local[pelvis].translation.x-=d.x;p.local[pelvis].translation.z-=d.z;}else{const auto &m=p.global[unsigned(parent)].m;const float A=m[0],B=m[4],C=m[8],D=m[1],E=m[5],F=m[9],G=m[2],H=m[6],I=m[10];const float det=A*(E*I-F*H)-B*(D*I-F*G)+C*(D*H-E*G);if(!std::isfinite(det)||std::abs(det)<1e-12){e="Singular attack parent basis";return false;}auto &t=p.local[pelvis].translation;t.x-=((E*I-F*H)*d.x+(B*F-C*E)*d.z)/det;t.y-=((F*G-D*I)*d.x+(C*D-A*F)*d.z)/det;t.z-=((D*H-E*G)*d.x+(A*E-B*D)*d.z)/det;}return evaluate(s,p,e);}
}
WorldSkeletalHero::~WorldSkeletalHero(){release();}
bool WorldSkeletalHero::select_appearance(const world_animation::Asset&body,const std::vector<unsigned>&parts,std::string&e){
 if(!loaded_||gpu_||!first_person_primitives_.empty()){e="Knight appearances can only change on a fresh loading controller";return false;}
 world_knight_appearance::Config config;config.first_person=parts;
 world_animation::Asset next;std::vector<unsigned> view;
 if(!world_knight_appearance::compose(asset_,body,config,next,view,e))return false;
 auto previous=std::move(asset_);const auto feet=feet_;const auto support=support_phase_;
 asset_=std::move(next);feet_={};
 if(!initialize_feet(e)){asset_=std::move(previous);feet_=feet;support_phase_=support;return false;}
 first_person_primitives_=std::move(view);first_person_body_parts_=unsigned(parts.size());
 held_primitives_begin_=unsigned(body.mesh.primitives.size());
 have_pose_=false;quiet_pelvis_lowering_=0;reset_unarmed_motion();world_character_face::reset(face_);stats_.ready=false;return true;
}
void WorldSkeletalHero::release(){
 world_animation_render::destroy(gpu_);gpu_=nullptr;loaded_=have_pose_=false;stats_.ready=false;
 first_person_primitives_.clear();first_person_body_parts_=0;
 story_pose_.reset();story_have_entry_=story_hidden_=story_swim_clips_=story_death_clip_=false;
 relaxed_walk_clip_available_=have_relaxed_walk_rotations_=false;
 full_unarmed_clips_=full_unarmed_run_=full_unarmed_body_active_=false;full_unarmed_run_weight_=0;
 quiet_pelvis_lowering_=0;reset_unarmed_motion();unarmed_motion_available_=false;unarmed_motion_asset_={};world_character_face::reset(face_);
}
bool WorldSkeletalHero::load_cpu(const char*path,std::string&e,const char*clip_prefix,const WorldSkeletalHero*arc_source){
 if(loaded_||gpu_){e="Hero already loaded";return false;}
 using Clock=std::chrono::steady_clock;const auto load_begin=Clock::now();cpu_load_stats_={};
 world_animation::Asset next;if(!world_animation::load_glb(path,next,e))return false;
 const auto parse_end=Clock::now();cpu_load_stats_.parse_ms=std::chrono::duration<double,std::milli>(parse_end-load_begin).count();
 const std::string family=clip_prefix?clip_prefix:"";stationary_cut_adaptation_=family.empty();
 grip_profile_=family=="Shield_"?1u:(family=="Axe_"||family=="Staff_")?2u:0u;
 constexpr const char*names[]={"Idle","Walk_Fwd","Walk_FwdRight","Walk_Right","Walk_BwdRight","Walk_Bwd","Walk_BwdLeft","Walk_Left","Walk_FwdLeft","Run","Attack_Left","Attack_Right","Attack_High","Attack_Low","Attack_Thrust"};
 for(unsigned n=0;n<15;++n){const std::string wanted=family+names[n];unsigned count=0;for(unsigned c=0;c<next.clips.size();++c)if(next.clips[c].name==wanted){clips_[n]=c;++count;}
  if(count!=1||next.clips[clips_[n]].duration<=0){e=std::string("Missing or ambiguous hero clip: ")+wanted;return false;}}
 unsigned pc=0,hc=0,wc=0,spine=world_animation::node_limit;
 for(unsigned n=0;n<next.skeleton.joints.size();++n){const auto&name=next.skeleton.joints[n].name;
  if(name=="pelvis"){pelvis_=n;++pc;}if(name=="hand_r"){hand_=n;++hc;}if(name=="spine_01")spine=n;}
 if(pc!=1||hc!=1){e="Missing/ambiguous hero pelvis or hand_r";return false;}
 for(unsigned n=0;n<next.skeleton.joints.size();++n)if(next.skeleton.joints[n].name=="valwood_weapon_r"){hand_=n;++wc;}
 if(wc>1){e="Ambiguous hero valwood_weapon_r socket";return false;}
 spine_=spine;
 if(spine==world_animation::node_limit){e="Missing hero upper-body root spine_01";return false;}
 // Only spine descendants are overlaid for held guards and nonfatal reactions.
 // The pelvis and legs keep real locomotion, rather than freezing into a pose.
 for(unsigned n=0;n<next.skeleton.joints.size();++n){int ancestor=int(n);while(ancestor>=0&&unsigned(ancestor)!=spine)ancestor=next.skeleton.joints[unsigned(ancestor)].parent;upper_mask_[n]=ancestor>=0;}
 constexpr const char*directions[]={"Left","Right","High","Low","Thrust"};
 auto optional=[&](const char*prefix,std::array<unsigned,5>&indices,bool&available){unsigned found=0;for(unsigned n=0;n<5;++n){unsigned count=0;for(unsigned c=0;c<next.clips.size();++c)if(next.clips[c].name==std::string(prefix)+directions[n]){indices[n]=c;++count;}
   if(count>1){e=std::string("Ambiguous hero clip: ")+prefix+directions[n];return false;}if(count){if(next.clips[indices[n]].duration<=0){e="Empty optional hero clip";return false;}++found;}}
  if(found&&found!=5){e=std::string("Incomplete hero ")+prefix+" clip family";return false;}available=found==5;return true;};
 if(!optional((family+"Guard_").c_str(),guards_,guard_clips_)||!optional("Hit_",reactions_,reaction_clips_))return false;
 constexpr const char*equipment_names[]={"Equipment_Stow","Equipment_Relaxed","Equipment_Draw"};unsigned equipment_count=0;
 for(unsigned n=0;n<3;++n){unsigned count=0;for(unsigned c=0;c<next.clips.size();++c)if(next.clips[c].name==equipment_names[n]){equipment_clips_[n]=c;++count;}
  if(count>1||(count&&next.clips[equipment_clips_[n]].duration<=0)){e="Invalid equipment capture family";return false;}equipment_count+=count;}
 if(equipment_count&&equipment_count!=3){e="Incomplete equipment capture family";return false;}equipment_clips_available_=equipment_count==3;equipment_.reset();
 unsigned relaxed_count=0;
 for(unsigned c=0;c<next.clips.size();++c)if(next.clips[c].name=="Story_UnarmedWalk"){
  if(++relaxed_count>1||next.clips[c].duration<=0){e="Invalid relaxed walking capture";return false;}relaxed_walk_clip_=c;
 }
 if(relaxed_count&&!equipment_clips_available_){e="Relaxed walking requires the equipment capture family";return false;}
 relaxed_walk_clip_available_=relaxed_count!=0;
 constexpr const char*unarmed_names[]={"Story_UnarmedIdle","Story_UnarmedWalkFull","Story_UnarmedRunFull"};unsigned unarmed_found[3]={};
 for(unsigned n=0;n<3;++n)for(unsigned c=0;c<next.clips.size();++c)if(next.clips[c].name==unarmed_names[n]){
  if(++unarmed_found[n]>1||next.clips[c].duration<=0){e="Invalid full-body unarmed capture";return false;}full_unarmed_clip_[n]=c;
 }
 if(bool(unarmed_found[0])!=bool(unarmed_found[1])||(unarmed_found[2]&&!unarmed_found[0])){e="Incomplete full-body unarmed idle/walk family";return false;}
 full_unarmed_clips_=unarmed_found[0]&&unarmed_found[1];full_unarmed_run_=unarmed_found[2]!=0;
 if(full_unarmed_clips_&&!equipment_clips_available_){e="Full-body unarmed captures require equipment presentation";return false;}
 constexpr const char*story_names[]={"Story_SwimIdle","Story_SwimForward","Story_Death"};unsigned story_found[3]={};
 for(unsigned n=0;n<3;++n)for(unsigned c=0;c<next.clips.size();++c)if(next.clips[c].name==story_names[n]){
  if(++story_found[n]>1||next.clips[c].duration<=0){e="Invalid story body clip";return false;}story_clips_[n]=c;
 }
 if(bool(story_found[0])!=bool(story_found[1])){e="Incomplete story swimming clip pair";return false;}
 story_swim_clips_=story_found[0]&&story_found[1];story_death_clip_=story_found[2]!=0;
 if(!world_animation::rest_pose(next.skeleton,rest_,e))return false;
 constexpr const char*left_names[]={"clavicle_l","upperarm_l","lowerarm_l","hand_l","index_01_l","ring_01_l"};unsigned*left_indices[]={&left_clavicle_,&left_upper_,&left_lower_,&left_hand_,&left_index_,&left_ring_};
 for(unsigned k=0;k<6;++k){unsigned found=0;for(unsigned n=0;n<next.skeleton.joints.size();++n)if(next.skeleton.joints[n].name==left_names[k]){*left_indices[k]=n;++found;}if(found!=1){e=std::string("Missing/ambiguous left grip joint: ")+left_names[k];return false;}}
 if(next.skeleton.joints[left_upper_].parent!=int(left_clavicle_)||next.skeleton.joints[left_lower_].parent!=int(left_upper_)||next.skeleton.joints[left_hand_].parent!=int(left_lower_)){e="Unexpected left grip chain hierarchy";return false;}
 constexpr const char*right_names[]={"clavicle_r","upperarm_r","lowerarm_r","hand_r"};unsigned*right_indices[]={&right_clavicle_,&right_upper_,&right_lower_,&right_hand_};
 for(unsigned k=0;k<4;++k){unsigned found=0;for(unsigned n=0;n<next.skeleton.joints.size();++n)if(next.skeleton.joints[n].name==right_names[k]){*right_indices[k]=n;++found;}if(found!=1){e="Missing/ambiguous right grip chain";return false;}}
 if(next.skeleton.joints[right_upper_].parent!=int(right_clavicle_)||next.skeleton.joints[right_lower_].parent!=int(right_upper_)||next.skeleton.joints[right_hand_].parent!=int(right_lower_)||next.skeleton.joints[left_clavicle_].parent!=next.skeleton.joints[right_clavicle_].parent||next.skeleton.joints[left_clavicle_].parent<0){e="Unexpected common arm hierarchy";return false;}
 chest_=unsigned(next.skeleton.joints[left_clavicle_].parent);
 asset_=std::move(next);
 // Guard contacts include the actual owned steel crossguard/held sword,
 // plus the shield. Leather grips and costume armor are never steel surfaces.
 // Store offsets in held gear because appearance selection replaces body parts.
 guard_surface_held_offsets_.clear();held_primitives_begin_=19;
 unsigned steel_count=0,shield_count=0;
 for(unsigned p=held_primitives_begin_;p<asset_.mesh.primitives.size();++p){
  const auto&primitive=asset_.mesh.primitives[p];
  // Both audited canonical sword rigs store the owned steel at held offset0.
  // Its exact source triangle count and rigid socket weights fail closed.
  const bool sword_steel=(grip_profile_==0||grip_profile_==1)&&p==held_primitives_begin_&&primitive.index_count==2318u*3u;
  bool held_left=grip_profile_==1&&primitive.index_count>0,held_right=sword_steel&&primitive.index_count>0;
  for(std::size_t at=primitive.first_index;(held_left||held_right)&&at<primitive.first_index+primitive.index_count;++at){const auto&v=asset_.mesh.vertices[asset_.mesh.indices[at]];
   for(unsigned k=0;k<4;++k)if(v.weights[k]>.000001f){const unsigned joint=asset_.skeleton.palette[v.joints[k]];if(joint!=left_hand_)held_left=false;if(joint!=hand_)held_right=false;}
  }
  if(sword_steel&&!held_right){e="Owned sword steel is not rigid to its source weapon socket";return false;}
  if(held_right||held_left){guard_surface_held_offsets_.push_back(p-held_primitives_begin_);steel_count+=held_right;shield_count+=held_left;}
 }
 if((grip_profile_==0||grip_profile_==1)&&steel_count!=1){e="Equipped sword has no unambiguous owned steel surface";return false;}
 if(grip_profile_==1&&!shield_count){e="Equipped shield has no actual hand-bound surface";return false;}
 constexpr const char*momentum_names[]={"spine_01","spine_02","spine_03","clavicle_r","upperarm_r","lowerarm_r","hand_r","clavicle_l","upperarm_l","lowerarm_l","hand_l"};
 for(unsigned k=0;k<momentum_joint_count;++k){unsigned found=0;for(unsigned n=0;n<asset_.skeleton.joints.size();++n)if(asset_.skeleton.joints[n].name==momentum_names[k]){momentum_joints_[k]=n;++found;}if(found!=1){e="Missing/ambiguous upper-body momentum joint";return false;}
  // Triangle-inequality chain radii bound displacement for ALL poses, rather
  // than estimating reach from one convenient entry configuration.
  auto radius=[&](unsigned endpoint){float r=0;int n=int(endpoint);while(n>=0&&unsigned(n)!=momentum_joints_[k]){const int parent=asset_.skeleton.joints[unsigned(n)].parent;if(parent<0)return 0.f;r+=vlength(vsub(origin(rest_.global[unsigned(n)]),origin(rest_.global[unsigned(parent)])));n=parent;}return n<0?0.f:r;};
  wrist_radius_[k]=radius(right_hand_);elbow_radius_[k]=radius(right_lower_);
 }
 if(reaction_clips_){world_animation::Pose reference;if(!world_animation::sample(asset_.skeleton,asset_.clips[clips_[0]],0,false,reference,e))return false;
  for(unsigned n=0;n<asset_.skeleton.joints.size();++n){const auto&name=asset_.skeleton.joints[n].name;reaction_mask_[n]=name=="spine_01"||name=="spine_02"||name=="spine_03"||name=="neck_01"||name=="head";reaction_reference_[n]=reference.local[n].rotation;}
 }
 if(!load_unarmed_motion(path,e)||!initialize_feet(e)||!initialize_captures(e))return false;
 if(guard_clips_){world_animation::Pose reference;if(!world_animation::sample(asset_.skeleton,asset_.clips[guards_[0]],0,true,reference,e)||!grip_socket(reference,guard_socket_,e))return false;}
 const auto arc_begin=Clock::now();cpu_load_stats_.setup_ms=std::chrono::duration<double,std::milli>(arc_begin-parse_end).count();
 auto arc_inputs=[](const WorldSkeletalHero&h){return world_skeletal_attack_arc_cache::Inputs{&h.asset_,h.held_primitives_begin_,h.clips_,h.pelvis_,h.spine_,h.hand_,h.right_hand_,h.grip_profile_,h.stationary_cut_adaptation_};};
 // The source remains owned by the old loadout bank until the entire staged
 // appearance succeeds. Body geometry may differ; the exact comparison also
 // preserves all source animation and held-weapon transforms and topology.
 if(arc_source&&arc_source->loaded_&&world_skeletal_attack_arc_cache::compatible(arc_inputs(*this),arc_inputs(*arc_source))){
  attack_arcs_=arc_source->attack_arcs_;cpu_load_stats_.arc_cache_reused=true;
 }else for(unsigned direction=0;direction<5;++direction){Vec3 previous,previous_tip;auto&arc=attack_arcs_[direction];
  for(unsigned n=0;n<arc.size();++n){world_animation::Pose sampled;if(!sample_attack(direction,attack_arc_frame(direction,n),sampled,e))return false;
   ++cpu_load_stats_.generated_arc_samples;
   const Vec3 point=origin(sampled.global[right_hand_]);
   if(family.empty())arc[n]=n?arc[n-1]+vlength(vsub(point,previous)):0;
   else{
    // A shaft can rotate quickly around an almost stationary wrist. Include
    // the actual equipped edge so inverse arc sampling cannot compress that
    // visible sweep into a near-zero wrist-distance interval.
    double base[3],tip[3];if(!blade_pose(sampled,0,0,0,0,base,tip)){e="Nonfinite prefixed attack edge path";return false;}
    const Vec3 edge{float(tip[0]),float(tip[1]),float(tip[2])};
    arc[n]=n?arc[n-1]+std::max(vlength(vsub(point,previous)),.5f*vlength(vsub(edge,previous_tip))):0;previous_tip=edge;
   }
   previous=point;
  }
 }
 for(const auto&arc:attack_arcs_)if(!std::isfinite(arc.back())||arc[440]<=0||arc[880]<=arc[440]||arc.back()<=arc[880]){e="Degenerate attack wrist path";return false;}
 const auto load_end=Clock::now();cpu_load_stats_.arc_ms=std::chrono::duration<double,std::milli>(load_end-arc_begin).count();cpu_load_stats_.total_ms=std::chrono::duration<double,std::milli>(load_end-load_begin).count();
 loaded_=true;return true;
}
bool WorldSkeletalHero::initialize_captures(std::string&e){
 constexpr const char*names[]={"Start_Fwd","Start_Bwd","Start_Left","Start_Right","Stop_Fwd_L","Stop_Fwd_R","Stop_Bwd_L","Stop_Bwd_R","Stop_Left_L","Stop_Left_R","Stop_Right_L","Stop_Right_R"};
 constexpr double durations[]={0.9333333333333333,0.8,0.5,0.9333333333333333,1.2,1.1333333333333333,1.2666666666666666,0.9666666666666667,1.1666666666666667,1.2,1.1,0.9333333333333333};
 // Frozen V4 capture contract ef1a43b4...; curves are removed planar travel,
 // never applied to the gameplay root. Names and exact durations are checked.
 const std::array<std::vector<double>,12> distances={std::vector<double>{0,0.004821499473,0.01137253974,0.01962618263,0.03115458476,0.04649407424,0.06602762686,0.09041135505,0.1192540275,0.151750283,0.186655145,0.2220634809,0.2541916754,0.2846135844,0.3134149259,0.3431299539,0.3740466677,0.4052739424,0.4357540764,0.4675828656,0.5006146085,0.5342492105,0.5711306775,0.6110594168,0.6527607844,0.6981599082,0.745650206,0.7911851706,0.8353293738},std::vector<double>{0,0.002813393611,0.006119168418,0.01084028784,0.01746275391,0.02776440659,0.04255835272,0.06286649385,0.08853543718,0.1184063789,0.1481404358,0.1784653026,0.2089188324,0.2389550808,0.2703688626,0.302578715,0.3346432548,0.3672939236,0.3998666069,0.4318190156,0.4661563429,0.5031460394,0.543165406,0.58719742,0.6332500259},std::vector<double>{0,0.006441846818,0.01552576044,0.02638399136,0.03740102971,0.04877512627,0.06030226209,0.07194556706,0.08397353124,0.09667146802,0.1120299554,0.1299755273,0.1508628589,0.1793554097,0.2168595542,0.2613088867},std::vector<double>{0,0.001086478455,0.003044508645,0.006227089622,0.01064724093,0.01660027543,0.02474253335,0.03511199836,0.04901031134,0.06642055078,0.0866761085,0.1110559743,0.1378839404,0.1651540717,0.1917608741,0.2166895801,0.239406518,0.2629181059,0.2867440775,0.3103130745,0.3352717987,0.3616376364,0.38892051,0.4193676502,0.4514562977,0.4844399093,0.5216411261,0.5625025707,0.6048917894},std::vector<double>{0,0.05647942822,0.1135328144,0.1329477243,0.1503367792,0.1643564187,0.1736642064,0.1769179522,0.1810700363,0.1920650032,0.2083823311,0.2285026521,0.2509066447,0.2740735362,0.2964839794,0.3166179091,0.3329551448,0.3439756427,0.3481607152,0.3490579306,0.3520910426,0.3569066221,0.3631496259,0.3704657899,0.3784989284,0.3868951174,0.3952991221,0.4033555903,0.4107107286,0.4170084962,0.4218945689,0.4250139744,0.4260142895,0.4263552633,0.4269157053,0.4276667815,0.4285785193},std::vector<double>{0,0.04291714531,0.08486347209,0.1257330161,0.1665229947,0.1668131629,0.1673142464,0.1680227265,0.1689413986,0.1700829755,0.1714704033,0.1731357796,0.1751186429,0.1774645718,0.1802229057,0.1834457618,0.1894574739,0.1995327678,0.2130408253,0.229329941,0.2477436071,0.267623553,0.2883113007,0.3091475414,0.3294731059,0.34862866,0.3659551652,0.3807947476,0.3924909689,0.4013165154,0.4084401505,0.4140528222,0.4183447963,0.421506232,0.4237247838},std::vector<double>{0,0.04411377614,0.09193357546,0.142551234,0.1894554114,0.2009877914,0.2106952078,0.219034804,0.226463766,0.2334361055,0.2404064153,0.2478292236,0.2561611356,0.2665773584,0.2788943264,0.2914874975,0.3027271955,0.3110231457,0.3151046967,0.3186297079,0.3260207994,0.336869475,0.3503591199,0.3656150988,0.3817460669,0.3978545594,0.4130396966,0.4263997433,0.4370332117,0.4440411531,0.4465551536,0.4471545108,0.4477747965,0.4486871878,0.4501875279,0.4525287042,0.4559430837,0.4606549705,0.4668864679},std::vector<double>{0,0.04907946859,0.09154887826,0.1249786126,0.1469459693,0.1550857346,0.1569502839,0.1609917757,0.167142791,0.1750607362,0.184365035,0.1946654669,0.2055669335,0.2166737338,0.2275891412,0.2379162459,0.2472593388,0.2552242428,0.2614237937,0.2654930441,0.2672416673,0.2685680389,0.2704700165,0.2729782674,0.2760114483,0.279458666,0.2831973019,0.2870998877,0.2910362442,0.2948757165},std::vector<double>{0,0.04901414431,0.09960342437,0.1323317915,0.1677485301,0.2020228978,0.2313239664,0.2518211486,0.2596874055,0.2617749026,0.2678543065,0.277208437,0.2891122784,0.3028399709,0.3176654093,0.3328624346,0.3477048699,0.361466257,0.3734206564,0.3828414923,0.3890034131,0.3911829568,0.3915226252,0.3918647588,0.3922338332,0.3926652893,0.3932014293,0.3938854431,0.3947585711,0.3958597631,0.3972268447,0.398896401,0.4009041526,0.4032858514,0.4060767651,0.4093117472},std::vector<double>{0,0.03802177207,0.07612667011,0.1145087836,0.1275632556,0.1412432436,0.1551580211,0.1689168852,0.1821295713,0.1944053815,0.205353904,0.2145845658,0.2217073294,0.2263332511,0.2280803331,0.2292573963,0.2328114529,0.238414469,0.2457265487,0.2544075938,0.2641163417,0.274512293,0.2852540109,0.2960009342,0.3064117874,0.3161455557,0.3248614264,0.3322181216,0.3378746322,0.3414902536,0.3427245784,0.3428636344,0.3430151886,0.3432045916,0.3434643285,0.3438293401,0.3443323537},std::vector<double>{0,0.0429224018,0.05052432819,0.05598647419,0.05989479689,0.06336956429,0.06782437372,0.07413636805,0.08374894897,0.09707768859,0.1129538943,0.1301699146,0.1475158978,0.1638002378,0.177883743,0.1887865364,0.19618073,0.2031490072,0.2116113335,0.2214432969,0.2323444226,0.243942273,0.2558308623,0.267589617,0.2787913503,0.2890118981,0.2978404258,0.3049075701,0.3099817058,0.3134139313,0.3156284568,0.3169438752,0.3176960822,0.3182676668},std::vector<double>{0,0.05697244234,0.1137488472,0.1299927744,0.1477770058,0.1653098831,0.1805650287,0.1914190856,0.1957023133,0.1985865279,0.2067024337,0.2189019737,0.2340324358,0.2509407529,0.2684735871,0.2854776952,0.3008001107,0.3132893016,0.3217955388,0.3251945925,0.325989549,0.3270843486,0.3285022986,0.3301815349,0.3320359514,0.3339713659,0.3358898752,0.3376917778,0.3392765437}};
 unsigned found=0;
 for(unsigned k=0;k<12;++k){unsigned count=0;for(unsigned n=0;n<asset_.clips.size();++n)if(asset_.clips[n].name==names[k]){locomotion_captures_[k].clip=n;++count;}
  if(count>1){e="Ambiguous locomotion capture";return false;}if(count){if(std::abs(asset_.clips[locomotion_captures_[k].clip].duration-durations[k])>.00001){e="Locomotion capture differs from frozen distance contract";return false;}locomotion_captures_[k].distance=distances[k];++found;}}
 if(found&&found!=12){e="Incomplete locomotion transition capture family";return false;}capture_clips_=found==12;
 for(unsigned n=0;n<asset_.skeleton.joints.size();++n){const auto&name=asset_.skeleton.joints[n].name;leg_mask_[n]=name.rfind("thigh_",0)==0||name.rfind("calf_",0)==0||name.rfind("foot_",0)==0||name.rfind("ball_",0)==0;}
 constexpr const char*turn_names[]={"Turn_Left45","Turn_Right45","Turn_Left90","Turn_Right90"};unsigned turns=0;
 for(unsigned k=0;k<4;++k){unsigned count=0;for(unsigned n=0;n<asset_.clips.size();++n)if(asset_.clips[n].name==turn_names[k]){turn_clips_[k]=n;++count;}
  if(count>1){e="Ambiguous lower-body turn capture";return false;}if(count){world_animation::Pose first;if(asset_.clips[turn_clips_[k]].duration<=0||!world_animation::sample(asset_.skeleton,asset_.clips[turn_clips_[k]],0,false,first,e))return false;turn_reference_rotation_[k]=world_rotation(asset_.skeleton,first,pelvis_);++turns;}}
 if(turns&&turns!=4){e="Incomplete lower-body turn capture family";return false;}turn_clips_available_=turns==4;
 return true;
}
bool WorldSkeletalHero::copy_legs(const world_animation::Pose&from,float weight,world_animation::Pose&to,std::string&e)const{
 for(unsigned n=0;n<to.nodes;++n)if(leg_mask_[n]){auto&o=to.local[n];const auto&v=from.local[n];o.translation=vadd(o.translation,vmul(vsub(v.translation,o.translation),weight));o.scale=vadd(o.scale,vmul(vsub(v.scale,o.scale),weight));const int parent=asset_.skeleton.joints[n].parent;
  const Quat rotation=parent==int(pelvis_)?qunit(qproduct(qconjugate(world_rotation(asset_.skeleton,to,pelvis_)),world_rotation(asset_.skeleton,from,n))):v.rotation;
  o.rotation=mixq(o.rotation,rotation,weight);}
 return world_animation::evaluate(asset_.skeleton,to,e);
}
bool WorldSkeletalHero::lower_locomotion(world_animation::Pose&p,const Input&i,double actual,double dt,std::string&e){
 transition_contact_={0,0};if(!capture_clips_){if(dt>0)previous_actual_=actual;return true;}
 constexpr double strides[4]={1.58687,1.31857,1.38565,1.38528};constexpr unsigned walk_indices[4]={1,5,7,3};
 const double h=heading_<0?heading_+tau:heading_;const unsigned compass=unsigned(std::floor(h/(tau/4)+.5))%4;constexpr unsigned cardinal_map[4]={0,3,1,2};const unsigned direction=cardinal_map[compass];
 if(i.reset){locomotion_mode_=0;gait_offset_=0;previous_actual_=0;have_locomotion_pose_=false;}
 // A nearly straight source leg may not reach a raised ankle at the same
 // horizontal position. Bring the swing boot inward to keep the authored bone
 // lengths instead of silently skipping clearance and scraping the ground.
 auto clear_step=[&](unsigned side,float clearance){
  const auto&foot=feet_[side];Vec3 heel,toe;if(!sole_point(p,side,0,heel,e)||!sole_point(p,side,1,toe,e))return false;
  const float height=std::min(heel.y,toe.y);if(clearance<=height)return true;
  const Vec3 hip=origin(p.global[foot.hip]),knee=origin(p.global[foot.knee]),ankle=origin(p.global[foot.ankle]);
  const float reach=vlength(vsub(knee,hip))+vlength(vsub(ankle,knee))-.0001f;
  Vec3 target=ankle;target.y+=clearance-height;const float vertical=target.y-hip.y;
  if(std::abs(vertical)>=reach)return true;
  const float horizontal=std::hypot(target.x-hip.x,target.z-hip.z),allowed=std::sqrt(std::max(0.f,reach*reach-vertical*vertical));
  if(horizontal>allowed&&horizontal>.00001f){const float ratio=allowed/horizontal;target.x=hip.x+(target.x-hip.x)*ratio;target.z=hip.z+(target.z-hip.z)*ratio;}
  return solve_leg(p,foot,target,world_rotation(asset_.skeleton,p,foot.ankle),e);
 };
 auto leg_cost=[&](const world_animation::Pose&a,const world_animation::Pose&b){double cost=0;for(const auto&foot:feet_)for(unsigned n:{foot.knee,foot.ankle,foot.ball}){Vec3 x,y;if(!inverse_point(a.global[pelvis_],origin(a.global[n]),x)||!inverse_point(b.global[pelvis_],origin(b.global[n]),y))return std::numeric_limits<double>::infinity();cost+=vdot(vsub(x,y),vsub(x,y));}return cost;};
 // A sudden reversal must take a corrective step; blending forward through
 // both side loops at one arbitrary phase makes the free boot scrape a wide
 // ground-level arc. Match the resolved direction's nearest support phase and
 // bridge only the legs, leaving the gameplay heading and weapon untouched.
 if(dt>0&&i.grounded&&actual>.30&&previous_actual_>.30&&locomotion_mode_==0&&have_locomotion_pose_){
  const double distance=std::hypot(i.dx,i.dz),forward=(i.dx*std::sin(i.yaw)+i.dz*std::cos(i.yaw))/distance,right=(-i.dx*std::cos(i.yaw)+i.dz*std::sin(i.yaw))/distance,goal=std::atan2(right,forward);
  const double prior_gap=std::abs(std::remainder(goal-heading_,tau))/std::exp(-dt/.075);
  if(prior_gap>tau*.375){
   double resolved=goal;if(resolved<0)resolved+=tau;const unsigned chosen=cardinal_map[unsigned(std::floor(resolved/(tau/4)+.5))%4];
   locomotion_mode_=3;locomotion_capture_=chosen;locomotion_time_=0;locomotion_duration_=.24;locomotion_entry_=locomotion_previous_;
   locomotion_free_foot_=feet_[0].weight>feet_[1].weight?1:0;
   if(std::abs(forward)<.80){
    // The capsule has reversed its lateral travel immediately. Retire those
    // stale world anchors once, preserving the displayed leg pose as the step
    // entry instead of snapping back to its uncorrected source rotations.
    locomotion_entry_=pose_;
    // Keep the loaded boot anchored while the free boot takes the reversal
    // step. Retiring both anchors made the whole stance skate sideways.
    auto&foot=feet_[locomotion_free_foot_];if(foot.planted)++stats_.foot_releases;foot.planted=false;foot.releasing=true;foot.weight=0;foot.history=false;foot.cooldown=locomotion_duration_;
   }
   double best=std::numeric_limits<double>::infinity(),phase=0;
   for(unsigned n=0;n<128;++n){world_animation::Pose sample;if(!world_animation::sample(asset_.skeleton,asset_.clips[clips_[walk_indices[chosen]]],asset_.clips[clips_[walk_indices[chosen]]].duration*n/128,true,sample,e))return false;
    // Match the arriving stance as well as the entry silhouette. Matching
    // knees alone could finish the bridge with neither boot supporting the
    // new travel direction, producing a second sideways scrape.
    const double landing=n/128.+actual*locomotion_duration_/strides[chosen],f=(landing-std::floor(landing))*128;const unsigned k=unsigned(f)%128;
    const auto&support=support_phase_[walk_indices[chosen]][1-locomotion_free_foot_];const double contact=support[k]+(f-std::floor(f))*(support[(k+1)%128]-support[k]);
    const double cost=leg_cost(sample,locomotion_previous_)+.40*(1-contact)*(1-contact);if(cost<best){best=cost;phase=n/128.;}}
   locomotion_entry_time_=phase;locomotion_distance_=0;gait_offset_=phase-cycles_;
  }
 }
 if(dt>0&&i.grounded&&actual>.025&&previous_actual_<=.025){
  const double distance=std::hypot(i.dx,i.dz);double resolved=std::atan2((-i.dx*std::cos(i.yaw)+i.dz*std::sin(i.yaw))/distance,(i.dx*std::sin(i.yaw)+i.dz*std::cos(i.yaw))/distance);if(resolved<0)resolved+=tau;const unsigned start_direction=cardinal_map[unsigned(std::floor(resolved/(tau/4)+.5))%4];
  locomotion_mode_=1;locomotion_capture_=start_direction;locomotion_time_=0;locomotion_distance_=0;locomotion_entry_=have_locomotion_pose_?locomotion_previous_:p;locomotion_free_foot_=start_direction==0?1:0;
  const auto&capture=locomotion_captures_[start_direction];world_animation::Pose ending;if(!world_animation::sample(asset_.skeleton,asset_.clips[capture.clip],asset_.clips[capture.clip].duration,false,ending,e))return false;
  const bool running=actual>=3.&&start_direction==0;const unsigned loop=running?9:walk_indices[start_direction];const double period=running?2:1;double best=std::numeric_limits<double>::infinity();locomotion_entry_time_=0;
  for(unsigned n=0;n<128;++n){world_animation::Pose sample;if(!world_animation::sample(asset_.skeleton,asset_.clips[clips_[loop]],asset_.clips[clips_[loop]].duration*n/128,true,sample,e))return false;const double cost=leg_cost(sample,ending);if(cost<best){best=cost;locomotion_entry_time_=period*n/128;}}
  locomotion_duration_=running?2.45294:strides[start_direction];
 }
 if(dt>0&&i.grounded&&actual<.025&&previous_actual_>.025&&have_locomotion_pose_){
  locomotion_mode_=2;locomotion_time_=0;locomotion_entry_=locomotion_previous_;
  constexpr double latest_entry[8]={.15,.075,.10,.05,.18,.17,0,.05};double best=std::numeric_limits<double>::infinity();locomotion_entry_time_=0;
  // Match the actually loaded boot as well as the entry silhouette. Joint
  // similarity alone sometimes selected the opposite braking capture and
  // released the foot that was already carrying the character.
  for(unsigned loaded=0;loaded<2;++loaded){const unsigned index=4+direction*2+loaded;const auto&clip=asset_.clips[locomotion_captures_[index].clip];
   for(unsigned n=0;n<=unsigned(std::ceil(latest_entry[direction*2+loaded]*30));++n){world_animation::Pose sample;if(!world_animation::sample(asset_.skeleton,clip,n/30.,false,sample,e))return false;const double support_penalty=.5*(1-feet_[loaded].weight);const double cost=leg_cost(sample,locomotion_entry_)+support_penalty;if(cost<best){best=cost;locomotion_entry_time_=n/30.;locomotion_capture_=index;locomotion_free_foot_=1-loaded;}}}
  const auto&clip=asset_.clips[locomotion_captures_[locomotion_capture_].clip];locomotion_duration_=(clip.duration-locomotion_entry_time_)/1.8;
 }
 if(locomotion_mode_==1){locomotion_time_+=dt;if(dt>0)locomotion_distance_+=std::hypot(i.dx,i.dz);const auto&capture=locomotion_captures_[locomotion_capture_];const double total=capture.distance.back();const double overshoot=std::max(0.,locomotion_distance_-total),seconds=actual>.00001?overshoot/actual:0;
  double tail_phase=0;
  if(seconds>0){const double distance=std::hypot(i.dx,i.dz),forward=distance>.00001?(i.dx*std::sin(i.yaw)+i.dz*std::cos(i.yaw))/distance:1,goal=smooth(float((actual-1.8)/1.2))*smooth(float((forward-.65)/.30));const double run_tau=goal<run_?.045:.075,run_at_boundary=goal+(run_-goal)*std::exp(seconds/run_tau),integral=goal*seconds+(run_at_boundary-goal)*run_tau*(-std::expm1(-seconds/run_tau));tail_phase=actual*(seconds/strides[locomotion_capture_]+integral*(1/2.45294-1/strides[locomotion_capture_]));}
  gait_offset_=locomotion_entry_time_+(seconds>0?tail_phase:(locomotion_distance_-total)/locomotion_duration_)-cycles_;}
 // Keep the torso/weapon on its original gait clock. Only the legs receive a
 // support-matched loop phase; the final leg IK cannot move their ancestors.
 if(locomotion_mode_!=3&&std::abs(gait_offset_)>.000001){std::array<world_animation::Blend,4> blends={};unsigned count=0;const double phase=cycles_+gait_offset_;blends[count++]={clips_[0],idle_time_,1-locomotion_};for(unsigned d=0;d<8;++d)if(direction_[d]>0)blends[count++]={clips_[d+1],(phase-std::floor(phase))*asset_.clips[clips_[d+1]].duration,locomotion_*(1-run_)*direction_[d]};blends[count++]={clips_[9],(phase*.5-std::floor(phase*.5))*asset_.clips[clips_[9]].duration,locomotion_*run_};world_animation::Pose loop;if(!world_animation::blend(asset_,blends.data(),count,true,loop,e)||!copy_legs(loop,1,p,e))return false;}
 if(locomotion_mode_==3){
  locomotion_time_+=dt;if(dt>0)locomotion_distance_+=std::hypot(i.dx,i.dz);const double progress=std::min(1.,locomotion_time_/locomotion_duration_),phase=locomotion_entry_time_+locomotion_distance_/strides[locomotion_capture_];gait_offset_=phase-cycles_;world_animation::Pose target;
  const auto&clip=asset_.clips[clips_[walk_indices[locomotion_capture_]]];if(!world_animation::sample(asset_.skeleton,clip,(phase-std::floor(phase))*clip.duration,true,target,e))return false;
  world_animation::Pose bridge=locomotion_entry_;if(!copy_legs(target,smooth(float(progress)),bridge,e)||!copy_legs(bridge,1,p,e))return false;
  const float hold=1-smooth(float((progress-.70)/.30));transition_contact_[1-locomotion_free_foot_]=hold;transition_contact_[locomotion_free_foot_]=-hold;
  if(!clear_step(locomotion_free_foot_,float(.10*std::sin(progress*tau*.5))))return false;
  if(progress>=1)locomotion_mode_=0;
 }else if(locomotion_mode_){const auto&capture=locomotion_captures_[locomotion_capture_];const auto&clip=asset_.clips[capture.clip];world_animation::Pose captured;double time=0,progress=0;
  if(locomotion_mode_==1){const double distance=std::min(locomotion_distance_,capture.distance.back());auto it=std::lower_bound(capture.distance.begin(),capture.distance.end(),distance);const unsigned n=unsigned(it-capture.distance.begin());time=n==0?0:n>=capture.distance.size()?clip.duration:(n-1+(distance-capture.distance[n-1])/std::max(1.e-9,capture.distance[n]-capture.distance[n-1]))/30.;progress=locomotion_distance_/capture.distance.back();}
  else{locomotion_time_+=dt;time=std::min(clip.duration,locomotion_entry_time_+locomotion_time_*1.8);progress=locomotion_time_/std::max(.001,locomotion_duration_);}
  if(!world_animation::sample(asset_.skeleton,clip,time,false,captured,e))return false;
  world_animation::Pose legs=locomotion_entry_;const float entry=smooth(float(locomotion_time_/(locomotion_mode_==2?.16:.08)));if(!copy_legs(captured,entry,legs,e))return false;
  const float weight=locomotion_mode_==1?1-smooth(float((progress-.72)/.28)):1-smooth(float((progress-(locomotion_capture_>=6&&locomotion_capture_<=7?.15:.88))/(locomotion_capture_>=6&&locomotion_capture_<=7?.65:.12)));if(!copy_legs(legs,weight,p,e))return false;
  transition_contact_[1-locomotion_free_foot_]=weight;transition_contact_[locomotion_free_foot_]=-weight;
  if(locomotion_mode_==2&&progress>0&&progress<1&&!clear_step(locomotion_free_foot_,float(.08*std::sin(std::min(1.,progress)*tau*.5))))return false;
  if(progress>=1)locomotion_mode_=0;
 }
 locomotion_previous_=p;have_locomotion_pose_=true;if(dt>0)previous_actual_=actual;return true;
}

// Rotate only each thigh's world orientation. Its pelvis/torso/weapon ancestors
// stay bit-identical; the current authoritative actor yaw is never delayed.
bool WorldSkeletalHero::yaw_legs(world_animation::Pose&p,double radians,std::string&e)const{
 const double half=radians*.5;const Quat yaw{0,float(std::sin(half)),0,float(std::cos(half))};
 const auto parent=world_rotation(asset_.skeleton,p,pelvis_);
 for(unsigned n=0;n<p.nodes;++n)if(leg_mask_[n]&&asset_.skeleton.joints[n].parent==int(pelvis_))
  p.local[n].rotation=qunit(qproduct(qconjugate(parent),qproduct(yaw,world_rotation(asset_.skeleton,p,n))));
 return world_animation::evaluate(asset_.skeleton,p,e);
}
bool WorldSkeletalHero::turn_lower_body(world_animation::Pose&p,const Input&i,double dt,bool allowed,std::string&e){
 stats_.turn_weight=stats_.turn_progress=0;
 if(!turn_clips_available_)return true;
 if(i.reset||!have_pose_){turn_mode_=0;turn_reference_yaw_=turn_last_yaw_=i.yaw;turn_time_=0;stats_.turn_debt_radians=0;return true;}
 const double change=std::remainder(double(i.yaw)-turn_last_yaw_,tau),debt=std::remainder(double(i.yaw)-turn_reference_yaw_,tau);
 const bool stationary=std::hypot(i.dx,i.dz)<.00001;
 const bool eligible=allowed&&i.grounded&&stationary&&locomotion_mode_==0&&locomotion_<.12f;
 auto begin_exit=[&](){turn_entry_=pose_;turn_entry_yaw_=turn_last_yaw_;turn_mode_=2;turn_time_=0;turn_duration_=.12;};
 if(turn_mode_==1&&(!eligible||(debt*turn_goal_angle_<0&&std::abs(debt)>.08))){begin_exit();turn_reference_yaw_=i.yaw;}
 if(turn_mode_==0){
  if(!eligible)turn_reference_yaw_=i.yaw;
  else if(dt>0&&std::abs(debt)>.15){
   const double predicted=std::max(std::abs(debt),std::abs(change)/dt*.60);
   turn_capture_=(predicted>tau*.1875?2:0)+(debt<0?1:0);
   turn_entry_=pose_;turn_entry_yaw_=turn_last_yaw_;turn_mode_=1;turn_time_=turn_source_progress_=0;
   turn_duration_=asset_.clips[turn_clips_[turn_capture_]].duration*.65;turn_goal_angle_=debt;++stats_.turn_steps;
  }
 }
 if(turn_mode_==1){
  const double nominal=(turn_capture_<2?tau/8:tau/4)*(turn_capture_%2?-1:1);
  turn_goal_angle_=std::clamp(debt,-std::abs(nominal),std::abs(nominal));
  turn_time_=std::min(turn_duration_,turn_time_+dt);const double phase=turn_time_/turn_duration_;
  const auto&clip=asset_.clips[turn_clips_[turn_capture_]];world_animation::Pose captured;
  if(!world_animation::sample(asset_.skeleton,clip,phase*clip.duration,false,captured,e))return false;
  const auto delta=qunit(qproduct(world_rotation(asset_.skeleton,captured,pelvis_),qconjugate(turn_reference_rotation_[turn_capture_])));
  const double source_yaw=std::atan2(2.*(delta.w*delta.y+delta.x*delta.z),1.-2.*(delta.y*delta.y+delta.z*delta.z));
  turn_source_progress_=std::max(turn_source_progress_,std::clamp(source_yaw/nominal,0.,1.));
  // Captures have different initial combat stances (the left 90 starts with
  // an already opened boot). Transfer their joint rotation DELTAS onto this
  // frame's idle/guard legs; copying the absolute first stance would snap both
  // boots before the turn had even begun. Segment translations stay unchanged.
  world_animation::Pose first,warped=p;
  if(!world_animation::sample(asset_.skeleton,clip,0,false,first,e))return false;
  std::array<Quat,world_animation::node_limit> targets;
  for(unsigned joint=0;joint<p.nodes;++joint)if(leg_mask_[joint]){
   const auto motion=qproduct(world_rotation(asset_.skeleton,captured,joint),qconjugate(world_rotation(asset_.skeleton,first,joint)));
   targets[joint]=qunit(qproduct(motion,world_rotation(asset_.skeleton,p,joint)));
  }
  for(unsigned joint=0;joint<p.nodes;++joint)if(leg_mask_[joint]){
   const int parent=asset_.skeleton.joints[joint].parent;
   const auto parent_rotation=parent>=0&&leg_mask_[unsigned(parent)]?targets[unsigned(parent)]:world_rotation(asset_.skeleton,p,unsigned(parent));
   warped.local[joint].rotation=qunit(qproduct(qconjugate(parent_rotation),targets[joint]));
  }
  if(!world_animation::evaluate(asset_.skeleton,warped,e))return false;
  captured=std::move(warped);
  // Cancel the source's captured pelvis heading before warping its nominal
  // turn onto the actual actor heading. The owned stepping articulation stays.
  if(!yaw_legs(captured,turn_reference_yaw_-double(i.yaw)+turn_source_progress_*turn_goal_angle_-source_yaw,e))return false;
  world_animation::Pose entry=turn_entry_;
  if(!yaw_legs(entry,std::remainder(turn_entry_yaw_-double(i.yaw),tau),e))return false;
  const float weight=smooth(float(turn_time_/.10));
  if(!copy_legs(captured,weight,entry,e)||!copy_legs(entry,1,p,e))return false;
  // The actual captured sole lift releases its support, instead of clearing
  // both feet on every camera frame. A planted side keeps its world anchor.
  for(unsigned side=0;side<2;++side){Vec3 heel,toe;if(!sole_point(captured,side,0,heel,e)||!sole_point(captured,side,1,toe,e))return false;
   const float lift=std::min(heel.y,toe.y);transition_contact_[side]=lift>.018f?-1.f:lift<.008f?weight:0.f;}
  stats_.turn_weight=weight;stats_.turn_progress=float(phase);
  if(phase>=1){turn_reference_yaw_+=turn_goal_angle_;turn_entry_=p;turn_entry_yaw_=i.yaw;turn_mode_=2;turn_time_=0;turn_duration_=.12;}
 }else if(turn_mode_==2){
  turn_time_=std::min(turn_duration_,turn_time_+dt);const float weight=1-smooth(float(turn_time_/turn_duration_));
  world_animation::Pose entry=turn_entry_;if(!yaw_legs(entry,std::remainder(turn_entry_yaw_-double(i.yaw),tau),e)||!copy_legs(entry,weight,p,e))return false;
  stats_.turn_weight=weight;
  if(turn_time_>=turn_duration_)turn_mode_=0;
 }
 if(!stationary||!i.grounded)turn_reference_yaw_=i.yaw;
 turn_last_yaw_=i.yaw;stats_.turn_debt_radians=float(std::remainder(double(i.yaw)-turn_reference_yaw_,tau));return true;
}

bool WorldSkeletalHero::sole_point(const world_animation::Pose&p,unsigned side,unsigned marker,world_animation::Vec3&out,std::string&e)const{
 world_animation::Vec3 normal;if(!world_animation::deform_vertex(asset_.mesh.vertices[feet_[side].sole[marker]],p,out,normal,e))return false;
 out=world_animation::transform_point(p.global[asset_.skeleton.mesh_node],out);return true;
}
bool WorldSkeletalHero::initialize_feet(std::string&e){
 const auto&s=asset_.skeleton;
 auto point=[&](unsigned vertex,Vec3&v){Vec3 n;if(!world_animation::deform_vertex(asset_.mesh.vertices[vertex],rest_,v,n,e))return false;v=world_animation::transform_point(rest_.global[s.mesh_node],v);return true;};
 for(unsigned side=0;side<2;++side){auto&f=feet_[side];const char*suffix=side?"r":"l";const char*names[]={"thigh_","calf_","foot_","ball_"};unsigned*indices[]={&f.hip,&f.knee,&f.ankle,&f.ball};
  for(unsigned k=0;k<4;++k){unsigned count=0;for(unsigned n=0;n<s.joints.size();++n)if(s.joints[n].name==std::string(names[k])+suffix){*indices[k]=n;++count;}if(count!=1){e="Missing/ambiguous foot-support chain";return false;}}
  if(s.joints[f.knee].parent!=int(f.hip)||s.joints[f.ankle].parent!=int(f.knee)){e="Invalid foot-support leg chain";return false;}
  std::vector<unsigned> vertices,sole;float bottom=std::numeric_limits<float>::infinity();
  for(unsigned n=0;n<asset_.mesh.vertices.size();++n){const auto&v=asset_.mesh.vertices[n];float total=0;for(unsigned k=0;k<4;++k){const unsigned bone=s.palette[v.joints[k]];if(bone==f.ankle||bone==f.ball)total+=v.weights[k];}if(total>.95f){Vec3 p;if(!point(n,p))return false;vertices.push_back(n);bottom=std::min(bottom,p.y);}}
  if(vertices.empty()){e="No foot-weighted sole geometry";return false;}f.vertices=vertices;
  Vec3 forward=vsub(origin(rest_.global[f.ball]),origin(rest_.global[f.ankle]));forward.y=0;forward=vunit(forward);float back=std::numeric_limits<float>::infinity(),front=-back;
  for(unsigned n:vertices){Vec3 p;if(!point(n,p))return false;if(p.y<bottom+.015f){sole.push_back(n);const float distance=vdot(p,forward);back=std::min(back,distance);front=std::max(front,distance);}}
  if(sole.empty()){e="No foot sole band";return false;}
  for(unsigned marker=0;marker<2;++marker){Vec3 center={};std::vector<unsigned> region;for(unsigned n:sole){Vec3 p;if(!point(n,p))return false;const float distance=vdot(p,forward);if(marker?distance>front-(front-back)*.25f:distance<back+(front-back)*.25f){center=vadd(center,p);region.push_back(n);}}
   if(region.empty()){e="Empty foot sole region";return false;}center=vmul(center,1.f/region.size());float nearest=std::numeric_limits<float>::infinity();for(unsigned n:region){Vec3 p;if(!point(n,p))return false;const float distance=vlength(vsub(p,center));if(distance<nearest){nearest=distance;f.sole[marker]=n;}}
  }
 }
 constexpr double strides[8]={1.58687,1.43124,1.38528,1.10278,1.31857,1.24039,1.38565,1.17019};
 for(unsigned clip=0;clip<10;++clip){const auto&c=asset_.clips[clips_[clip]];const double cycles=clip==9?2:1,stride=clip==9?2.45294:clip?strides[clip-1]:0,heading=clip>0&&clip<9?(clip-1)*tau/8:0;const Vec3 motion{float(-std::sin(heading)),0,float(std::cos(heading))};
  for(unsigned n=0;n<128;++n){const double phase=double(n)/128,delta=1./2048;world_animation::Pose p,before,after;
   if(!world_animation::sample(s,c,phase*c.duration,true,p,e)||!world_animation::sample(s,c,std::fmod(phase-delta+1.,1.)*c.duration,true,before,e)||!world_animation::sample(s,c,std::fmod(phase+delta,1.)*c.duration,true,after,e))return false;
   for(unsigned side=0;side<2;++side){Vec3 a,b,low;unsigned marker=0;if(!sole_point(p,side,0,a,e)||!sole_point(p,side,1,b,e))return false;marker=b.y<a.y?1:0;low=marker?b:a;
    if(!sole_point(before,side,marker,a,e)||!sole_point(after,side,marker,b,e))return false;
    const Vec3 velocity=vmul(vsub(b,a),float(1/(2*delta*c.duration))),per_cycle=vmul(vsub(b,a),float(1/(2*delta*cycles)));
    const float low_score=smooth((.065f-low.y)/.040f),vertical=smooth((.50f-std::abs(velocity.y))/.30f);
    const float stance=clip?smooth(float((-vdot(per_cycle,motion)/stride-.15)/.25)):1.f;
    support_phase_[clip][side][n]=low_score*vertical*stance;
   }
  }
 }
 return initialize_unarmed_feet(e)&&initialize_unarmed_motion_feet(e);
}
bool WorldSkeletalHero::initialize_unarmed_feet(std::string&e){
 if(!full_unarmed_clips_)return true;
 // Each actual outfit supplies its own sole samples. Align a clip by one
 // constant height offset, preserving captured bob/sway rather than moving
 // the pelvis to the floor every frame. Moving stride comes from the median
 // backward velocity of low, vertically stable stance soles per full cycle.
 auto curves=full_unarmed_support_;std::array<double,3> offsets={};std::array<double,2> strides={1,1};
 constexpr double delta=1./512.;const unsigned count=full_unarmed_run_?3:2;
 for(unsigned k=0;k<count;++k){const auto&clip=asset_.clips[full_unarmed_clip_[k]];
  struct Sample{float low=0,vertical=0,backward=0;};std::array<std::array<Sample,128>,2> samples={};std::vector<double> floor;
  for(unsigned n=0;n<128;++n){const double phase=double(n)/128;world_animation::Pose p,before,after;
   if(!world_animation::sample(asset_.skeleton,clip,phase*clip.duration,true,p,e)||
      !world_animation::sample(asset_.skeleton,clip,(phase-delta+1)*clip.duration,true,before,e)||
      !world_animation::sample(asset_.skeleton,clip,(phase+delta)*clip.duration,true,after,e))return false;
   double low=1.e9;
   for(unsigned side=0;side<2;++side){Vec3 heel,toe,a,b;if(!sole_point(p,side,0,heel,e)||!sole_point(p,side,1,toe,e))return false;
    const unsigned marker=toe.y<heel.y?1:0;const auto sole=marker?toe:heel;
    if(!sole_point(before,side,marker,a,e)||!sole_point(after,side,marker,b,e))return false;
    const auto velocity=vmul(vsub(b,a),float(1/(2*delta*clip.duration)));
    samples[side][n]={sole.y,std::abs(velocity.y),float(-(b.z-a.z)/(2*delta))};low=std::min(low,double(sole.y));
   }
   if(k==0){
    // The stance markers sit inside the sole band. Aligning those to zero
    // buried the boot edges, so support_feet raised the ankles with the
    // pelvis fixed and introduced an artificial crouch. Calibrate the whole
    // idle against the same deformed foot envelope used by the floor solver.
    // Moving clips retain their captured marker/stride calibration.
    low=1.e9;
    for(const auto&foot:feet_)for(unsigned vertex:foot.vertices){Vec3 point;
     if(!world_attack_foot_contact::deform_position(asset_.mesh.vertices[vertex],p,point,e))return false;
     point=world_animation::transform_point(p.global[asset_.skeleton.mesh_node],point);
     low=std::min(low,double(point.y));
    }
   }
   floor.push_back(low);
  }
  std::sort(floor.begin(),floor.end());offsets[k]=-floor[floor.size()/20];
  if(!std::isfinite(offsets[k])||std::abs(offsets[k])>.30){e="Unarmed capture is outside the supported constant floor offset";return false;}
  if(k){std::vector<double> candidates;
   for(const auto&side:samples)for(const auto&s:side)if(s.low+offsets[k]<.055&&s.vertical<.65&&s.backward>.25)candidates.push_back(s.backward);
   if(candidates.size()<16){e="Unarmed walking capture has no measured stance-foot stride";return false;}
   std::sort(candidates.begin(),candidates.end());strides[k-1]=candidates[candidates.size()/2];
   if(!std::isfinite(strides[k-1])||strides[k-1]<.40||strides[k-1]>5.5){e="Unarmed stance stride is outside the accepted range";return false;}
  }
  for(unsigned side=0;side<2;++side)for(unsigned n=0;n<128;++n){const auto&s=samples[side][n];
   const float low=smooth(float((.065-(s.low+offsets[k]))/.040));
   const float vertical=smooth((.50f-s.vertical)/.30f);
   const float stance=k?smooth(float((s.backward/strides[k-1]-.15)/.25)):1.f;
   curves[k][side][n]=low*vertical*stance;
  }
 }
 if(!full_unarmed_run_){strides[1]=strides[0];offsets[2]=offsets[1];curves[2]=curves[1];}
 full_unarmed_support_=curves;full_unarmed_floor_=offsets;full_unarmed_stride_=strides;
 stats_.unarmed_walk_stride_m=strides[0];stats_.unarmed_run_stride_m=strides[1];return true;
}
void WorldSkeletalHero::release_stance_anchors(){
 for(auto&foot:feet_){if(foot.planted)++stats_.foot_releases;foot.planted=foot.releasing=foot.history=false;foot.weight=0;foot.contact_age=foot.cooldown=foot.toe_off=0;}
 stats_.foot_weight={};stats_.foot_planted={};stats_.foot_residual_m={};
}
bool WorldSkeletalHero::solve_leg(world_animation::Pose&p,const FootSupport&f,Vec3 target,Quat rotation,std::string&e)const{
 const auto&s=asset_.skeleton;const Vec3 hip=origin(p.global[f.hip]),knee=origin(p.global[f.knee]),ankle=origin(p.global[f.ankle]);const float upper=vlength(vsub(knee,hip)),lower=vlength(vsub(ankle,knee)),distance=vlength(vsub(target,hip));
 if(distance>upper+lower-.000005f||distance<std::abs(upper-lower)+.000005f){e="Foot-support target outside unchanged leg reach";return false;}
 const Vec3 axis=vunit(vsub(target,hip));Vec3 pole=vsub(knee,hip);pole=vsub(pole,vmul(axis,vdot(pole,axis)));if(vlength(pole)<.00001f)pole=vcross(axis,{1,0,0});pole=vunit(pole);
 const float along=(upper*upper+distance*distance-lower*lower)/(2*distance),height=std::sqrt(std::max(0.f,upper*upper-along*along));const Vec3 goal=vadd(hip,vadd(vmul(axis,along),vmul(pole,height)));
 const Quat hip_world=qproduct(between(vsub(knee,hip),vsub(goal,hip)),world_rotation(s,p,f.hip));const int parent=s.joints[f.hip].parent;p.local[f.hip].rotation=qunit(parent<0?hip_world:qproduct(qconjugate(world_rotation(s,p,unsigned(parent))),hip_world));if(!world_animation::evaluate(s,p,e))return false;
 const Vec3 knee_now=origin(p.global[f.knee]);const Quat knee_world=qproduct(between(vsub(origin(p.global[f.ankle]),knee_now),vsub(target,knee_now)),world_rotation(s,p,f.knee));p.local[f.knee].rotation=qunit(qproduct(qconjugate(world_rotation(s,p,f.hip)),knee_world));if(!world_animation::evaluate(s,p,e))return false;
 p.local[f.ankle].rotation=qunit(qproduct(qconjugate(world_rotation(s,p,f.knee)),rotation));return world_animation::evaluate(s,p,e);
}
bool WorldSkeletalHero::support_feet(world_animation::Pose&p,const Input&i,double dt,bool allowed,std::string&e,const float*attack_contact){
 const auto&s=asset_.skeleton;const double c=std::cos(i.yaw),sn=std::sin(i.yaw);const bool moving=std::hypot(i.dx,i.dz)>.00001&&dt>0;
 auto world=[&](Vec3 v){return std::array<double,3>{i.x+c*v.x+sn*v.z,i.y+v.y,i.z-sn*v.x+c*v.z};};
 auto local=[&](const std::array<double,3>&v){return Vec3{float(c*(v[0]-i.x)-sn*(v[2]-i.z)),float(v[1]-i.y),float(sn*(v[0]-i.x)+c*(v[2]-i.z))};};
 auto rotate=[&](Quat q,Vec3 v){const auto r=qproduct(qproduct(q,{v.x,v.y,v.z,0}),qconjugate(q));return Vec3{r.x,r.y,r.z};};
 auto sample_support=[&](unsigned clip,unsigned side,double phase){phase-=std::floor(phase);const double f=phase*128;const unsigned n=unsigned(f)%128;return support_phase_[clip][side][n]+float(f-std::floor(f))*(support_phase_[clip][side][(n+1)%128]-support_phase_[clip][side][n]);};
 auto unarmed_support=[&](unsigned clip,unsigned side,double phase){phase-=std::floor(phase);const double f=phase*128;const unsigned n=unsigned(f)%128;return full_unarmed_support_[clip][side][n]+float(f-std::floor(f))*(full_unarmed_support_[clip][side][(n+1)%128]-full_unarmed_support_[clip][side][n]);};
 const float quiet_weight=full_unarmed_settled()?1-smooth(locomotion_/.18f):0;
 auto captured_rock=[&](double gap){return gap>0?quiet_weight*gap*(1-smooth(float((gap-.008)/.004))):0.;};
 // On uneven ground, lower the quiet body's pelvis enough for its downhill
 // foot to reach support with a small knee bend. A fixed pelvis otherwise
 // stretches that leg straight, releases its plant and repeats the cycle.
 // The goal fades continuously through the filtered movement blend. A hard
 // idle/moving gate would add or remove up to3cm in one frame on a grade.
 double lowering_goal=0;
 if(quiet_weight>0&&i.grounded&&allowed){
  float lowering=0;
  for(unsigned side=0;side<2;++side){const auto&foot=feet_[side];Vec3 heel,toe;
   if(!sole_point(p,side,0,heel,e)||!sole_point(p,side,1,toe,e))return false;
   const Vec3 sole=foot.planted?(foot.marker?toe:heel):(toe.y<heel.y?toe:heel);
   const auto raw_world=world(sole);auto anchor=foot.planted?foot.anchor:raw_world;
   Support support;support.height=i.y;bool valid=true;
   if(i.ground_query){try{valid=i.ground_query(anchor[0],anchor[1],anchor[2],support,i.ground_context);}catch(...){e="Idle pelvis support callback failed";return false;}}
   const double length=std::sqrt(double(support.nx)*support.nx+double(support.ny)*support.ny+double(support.nz)*support.nz);
   if(!valid||!std::isfinite(support.height)||!std::isfinite(length)||length<.0001||support.ny/length<std::cos(25*tau/360)||std::abs(support.height-i.y)>=.50)continue;
   const Vec3 normal{float((c*support.nx-sn*support.nz)/length),float(support.ny/length),float((sn*support.nx+c*support.nz)/length)};
   const Quat tilt=between({0,1,0},normal);float edge=0;
   for(unsigned vertex:foot.vertices){Vec3 point;if(!world_attack_foot_contact::deform_position(asset_.mesh.vertices[vertex],p,point,e))return false;
    point=world_animation::transform_point(p.global[s.mesh_node],point);edge=std::max(edge,-vdot(rotate(tilt,vsub(point,sole)),normal));}
   anchor[1]=support.height+std::min(.06f,edge)/std::max(.9,double(normal.y));
   anchor[1]+=captured_rock(raw_world[1]-anchor[1]);
   const Vec3 hip=origin(p.global[foot.hip]),knee=origin(p.global[foot.knee]),ankle=origin(p.global[foot.ankle]);
   const Vec3 target=vsub(local(anchor),rotate(tilt,vsub(sole,ankle)));
   const float upper=vlength(vsub(knee,hip)),lower=vlength(vsub(ankle,knee));
   const float reach2=upper*upper+lower*lower-2*upper*lower*float(std::cos(175*tau/360));
   const float horizontal2=(target.x-hip.x)*(target.x-hip.x)+(target.z-hip.z)*(target.z-hip.z);
   if(horizontal2<reach2)lowering=std::max(lowering,hip.y-target.y-std::sqrt(reach2-horizontal2));
  }
  lowering_goal=std::clamp(double(lowering),0.,.03)*quiet_weight;
 }
 if(i.reset)quiet_pelvis_lowering_=lowering_goal;
 else {const double response=lowering_goal<quiet_pelvis_lowering_?.035:.10;quiet_pelvis_lowering_+=(lowering_goal-quiet_pelvis_lowering_)*(-std::expm1(-dt/response));}
 // Equipment entry already contains its preceding world pose. Its own blend
 // carries that offset; adding this state again would double the lowering.
 if(!full_unarmed_body_active_||equipment_.transitioning())quiet_pelvis_lowering_=0;
 if(quiet_pelvis_lowering_>.000001){Vec3 translation=vsub(origin(p.global[pelvis_]),{0,float(quiet_pelvis_lowering_),0});const int parent=s.joints[pelvis_].parent;
   if(parent>=0&&!inverse_point(p.global[unsigned(parent)],translation,translation)){e="Singular idle pelvis support parent";return false;}
   p.local[pelvis_].translation=translation;if(!world_animation::evaluate(s,p,e))return false;
 }
 for(unsigned side=0;side<2;++side){auto&f=feet_[side];f.cooldown=std::max(0.,f.cooldown-dt);f.toe_off=full_unarmed_body_active_?0:std::max(0.,f.toe_off-dt);Vec3 heel,toe;if(!sole_point(p,side,0,heel,e)||!sole_point(p,side,1,toe,e))return false;const unsigned lowest=toe.y<heel.y?1:0;
  float confidence=0;
  if(full_unarmed_body_active_){const float run=float(full_unarmed_run_weight_);
   confidence=unarmed_support(0,side,idle_time_/asset_.clips[full_unarmed_clip_[0]].duration)*(1-locomotion_)+
    locomotion_*((1-run)*unarmed_support(1,side,full_unarmed_cycles_)+run*unarmed_support(2,side,full_unarmed_cycles_));
  }else{confidence=sample_support(0,side,0)*(1-locomotion_);for(unsigned d=0;d<8;++d)confidence+=locomotion_*(1-run_)*direction_[d]*sample_support(d+1,side,cycles_+gait_offset_);confidence+=locomotion_*run_*sample_support(9,side,(cycles_+gait_offset_)*.5);}
  if(full_unarmed_body_active_&&motion_enabled_)confidence=confidence*(1-motion_support_override_)+motion_current_support_[side]*motion_support_override_;
  confidence=confidence*(1-std::abs(transition_contact_[side]))+std::max(0.f,transition_contact_[side]);
  if(f.toe_off>0)confidence=0;
  const bool attack_support=attack_contact!=nullptr;
  if(attack_support)confidence=world_attack_foot_contact::blend(confidence,attack_contact[side],attack_legs_weight_);
  const bool permitted=i.grounded&&allowed;const Vec3 raw=lowest?toe:heel;const auto raw_world=world(raw);double event_dt=dt;
  // During an interrupted crouched pivot the visual recovery lasts longer
  // than the stopping source's support window. Acquire a new plant only
  // once the blended visible boot is no longer sliding quickly.
  if(full_unarmed_body_active_&&motion_enabled_&&motion_mode_==3&&motion_blend_duration_>.32&&!f.planted&&f.weight<.001f){
   float stable=0;if(f.history&&dt>0){const auto marker=world(f.marker?toe:heel);const double velocity=std::hypot(marker[0]-f.previous.x,marker[2]-f.previous.z)/dt;stable=smooth(float((.65-velocity)/.20));}
   confidence*=stable;
  }
  if(!f.planted&&f.weight<.001f&&f.cooldown==0&&permitted&&confidence>.55f){
   const bool crossing=f.history&&f.marker==lowest&&f.contact_age<=.55&&confidence>f.contact_age&&dt>0;
   const double fraction=crossing?std::clamp((.55-f.contact_age)/(confidence-f.contact_age),0.,1.):1.;
   f.marker=lowest;f.anchor=raw_world;
   if(crossing){const double previous[]={f.previous.x,f.previous.y,f.previous.z};for(unsigned axis=0;axis<3;++axis)f.anchor[axis]=previous[axis]+fraction*(raw_world[axis]-previous[axis]);event_dt=dt*(1-fraction);}
   f.planted=true;f.releasing=false;++stats_.foot_plants;
  }
  // Transfer the same physical support from heel to toe (or back) as the
  // captured boot rolls. Keeping a rising heel welded down would drive its
  // forefoot through the floor even though the ankle solve remains exact.
  if(f.planted&&lowest!=f.marker&&((f.marker?toe:heel).y-raw.y)>.005f){const Vec3 old=f.marker?toe:heel;const auto a=world(old),b=world(raw);for(unsigned axis=0;axis<3;++axis)f.anchor[axis]+=b[axis]-a[axis];f.marker=lowest;}
  // At a coarse render interval a support window can be crossed entirely.
  // When the other boot is loaded, turn a fast low swing into a toe-off step
  // instead of waiting for a missed source contact to recover by itself.
  if(!full_unarmed_body_active_&&moving&&f.history&&!f.planted&&f.toe_off==0&&f.weight<.25f&&feet_[1-side].weight>.65f&&confidence<.20f&&raw.y<.04f&&dt>0){
   const double speed=std::hypot(raw_world[0]-f.previous.x,raw_world[2]-f.previous.z)/dt;
   if(speed>.45){f.toe_off=.18;f.cooldown=std::max(f.cooldown,.18);}
  }
  const bool captured_step=(full_unarmed_body_active_?(locomotion_>.01f||(motion_enabled_&&motion_mode_!=0&&motion_mode_!=2)):locomotion_mode_==2&&side==locomotion_free_foot_)&&raw_world[1]-f.anchor[1]>.035;
  if(f.planted&&(!permitted||captured_step||((moving||transition_contact_[side]<0||attack_support)&&confidence<.15f))){
   if(permitted&&transition_contact_[side]>=0&&f.history&&f.contact_age>=.15&&confidence<f.contact_age&&dt>0){const double fraction=std::clamp((f.contact_age-.15)/(f.contact_age-confidence),0.,1.);f.weight+=(1-f.weight)*float(-std::expm1(-dt*fraction/.020));event_dt=dt*(1-fraction);}
   f.planted=false;f.releasing=true;f.cooldown=.08;if(!full_unarmed_body_active_&&permitted&&moving&&raw.y<.03f&&transition_contact_[side]>=0){f.toe_off=.18;f.cooldown=.18;}++stats_.foot_releases;
  }
  Support support;support.height=i.y;bool valid=i.grounded;
  if(valid&&(f.planted||f.weight>.00001f)&&i.ground_query){try{valid=i.ground_query(f.anchor[0],f.anchor[1],f.anchor[2],support,i.ground_context);}catch(...){e="Foot support callback failed";return false;}}
  const double normal_length=std::sqrt(double(support.nx)*support.nx+double(support.ny)*support.ny+double(support.nz)*support.nz);
  valid=valid&&std::isfinite(support.height)&&std::isfinite(normal_length)&&normal_length>.0001&&support.ny/normal_length>=std::cos(25*tau/360)&&std::abs(support.height-i.y)<.50;
  if(!valid&&f.planted){f.planted=false;f.releasing=true;f.cooldown=.10;++stats_.foot_releases;}
  const float goal=f.planted&&valid?1.f:0.f;f.weight+=(goal-f.weight)*float(-std::expm1(-event_dt/(goal>f.weight?.020:.060)));if(f.weight<.00001f&&goal==0)f.weight=0;
  if(f.weight>0){Vec3 sole;if(!sole_point(p,side,f.marker,sole,e))return false;const Vec3 ankle=origin(p.global[f.ankle]),hip=origin(p.global[f.hip]),knee=origin(p.global[f.knee]),offset=vsub(sole,ankle);const Quat rotation=world_rotation(s,p,f.ankle);
   if(valid){
    const Vec3 normal_local{float((c*support.nx-sn*support.nz)/normal_length),float(support.ny/normal_length),float((sn*support.nx+c*support.nz)/normal_length)};const Quat full_tilt=between({0,1,0},normal_local);float edge_clearance=0;
    // The fixed diagnostic heel/toe is near the sole centroid. A rolling boot
    // can contact at its edge several centimetres below that marker. Ground
    // the actual weighted boot surface, retaining the same horizontal anchor.
    for(unsigned vertex_index:f.vertices){const auto&vertex=asset_.mesh.vertices[vertex_index];{Vec3 point;if(!world_attack_foot_contact::deform_position(vertex,p,point,e))return false;point=world_animation::transform_point(p.global[s.mesh_node],point);edge_clearance=std::max(edge_clearance,-vdot(rotate(full_tilt,vsub(point,sole)),normal_local));}}
    f.anchor[1]=support.height+std::min(.06f,edge_clearance)/std::max(.9,normal_local.y*1.);
    // A quiet capture can rock a sole a few millimetres above the floor.
    // Pulling that boot down stretches the straighter leg to its reach limit,
    // repeatedly releasing/replanting it. Preserve that small captured lift
    // while retaining the horizontal plant and the penetration guard below.
    f.anchor[1]+=captured_rock(raw_world[1]-f.anchor[1]);
   }
   const Vec3 anchor=local(f.anchor);const float correction=vlength(vsub(anchor,sole));stats_.maximum_foot_correction_m=std::max(stats_.maximum_foot_correction_m,correction);
   // Unchanged leg reach, rather than a horizontal-distance cutoff, decides
   // whether a world support is still physically possible.
   const Vec3 normal{float((c*support.nx-sn*support.nz)/std::max(.0001,normal_length)),float(support.ny/std::max(.0001,normal_length)),float((sn*support.nx+c*support.nz)/std::max(.0001,normal_length))};const Quat tilt=valid?between({0,1,0},normal):Quat{};
   auto target=[&](float weight){const Quat q=mixq({},tilt,weight);return vsub(vadd(sole,vmul(vsub(anchor,sole),weight)),rotate(q,offset));};
   const float upper=vlength(vsub(knee,hip)),lower=vlength(vsub(ankle,knee)),reach=upper+lower-.00001f,inner=std::abs(upper-lower)+.00001f;auto reachable=[&](float weight){const float length=vlength(vsub(target(weight),hip));return length<=reach&&length>=inner;};
   if(!reachable(f.weight)){float lo=0,hi=f.weight;if(reachable(0))for(unsigned n=0;n<20;++n){const float middle=(lo+hi)*.5f;if(reachable(middle))lo=middle;else hi=middle;}f.weight=lo;if(f.planted){f.planted=false;f.releasing=true;f.cooldown=.18;f.toe_off=full_unarmed_body_active_?0:.18;++stats_.foot_releases;++stats_.foot_reach_releases;}}
   if(f.weight>.00001f){const Quat q=mixq({},tilt,f.weight);if(!solve_leg(p,f,target(f.weight),qunit(qproduct(q,rotation)),e))return false;Vec3 after;if(!sole_point(p,side,f.marker,after,e))return false;const Vec3 desired=vadd(sole,vmul(vsub(anchor,sole),f.weight));const float residual=vlength(vsub(after,desired));stats_.foot_residual_m[side]=residual;stats_.maximum_foot_residual_m=std::max(stats_.maximum_foot_residual_m,residual);}
  }
  // An exhausted support needs a visible toe-off, rather than dragging the
  // released sole through the next stance. This changes only the swing leg.
  if(!full_unarmed_body_active_&&moving&&f.toe_off>0){
   Vec3 h,t;if(!sole_point(p,side,0,h,e)||!sole_point(p,side,1,t,e))return false;
   const float clearance=float(.085*std::sin((1-f.toe_off/.18)*tau*.5)),low=std::min(h.y,t.y);
   if(clearance>low){const Vec3 hip=origin(p.global[f.hip]),knee=origin(p.global[f.knee]),ankle=origin(p.global[f.ankle]);const float reach=vlength(vsub(knee,hip))+vlength(vsub(ankle,knee))-.0001f;Vec3 target=ankle;target.y+=clearance-low;const float vertical=target.y-hip.y;
    if(std::abs(vertical)<reach){const float horizontal=std::hypot(target.x-hip.x,target.z-hip.z),limit=std::sqrt(std::max(0.f,reach*reach-vertical*vertical));if(horizontal>limit&&horizontal>.00001f){const float k=limit/horizontal;target.x=hip.x+(target.x-hip.x)*k;target.z=hip.z+(target.z-hip.z)*k;}if(!solve_leg(p,f,target,world_rotation(s,p,f.ankle),e))return false;}
   }
  }
  if(i.grounded){
   Vec3 current_heel,current_toe;if(!sole_point(p,side,0,current_heel,e)||!sole_point(p,side,1,current_toe,e))return false;const auto floor_probe=world(current_toe.y<current_heel.y?current_toe:current_heel);
   Support floor;floor.height=i.y;bool floor_valid=true;
   if(i.ground_query){try{floor_valid=i.ground_query(floor_probe[0],floor_probe[1],floor_probe[2],floor,i.ground_context);}catch(...){e="Foot clearance callback failed";return false;}}
   const double length=std::sqrt(double(floor.nx)*floor.nx+double(floor.ny)*floor.ny+double(floor.nz)*floor.nz);
   floor_valid=floor_valid&&std::isfinite(floor.height)&&std::isfinite(length)&&length>.0001&&floor.ny/length>=std::cos(25*tau/360)&&std::abs(floor.height-i.y)<.5;
   if(floor_valid){const Vec3 normal{float((c*floor.nx-sn*floor.nz)/length),float(floor.ny/length),float((sn*floor.nx+c*floor.nz)/length)};const Vec3 surface=local({floor_probe[0],floor.height,floor_probe[2]});float depth=0;
    for(unsigned vertex_index:f.vertices){const auto&vertex=asset_.mesh.vertices[vertex_index];{Vec3 point;if(!world_attack_foot_contact::deform_position(vertex,p,point,e))return false;point=world_animation::transform_point(p.global[s.mesh_node],point);depth=std::max(depth,-vdot(vsub(point,surface),normal));}}
    if(depth>.0005f&&depth<.25f){const Vec3 hip=origin(p.global[f.hip]),knee=origin(p.global[f.knee]),ankle=origin(p.global[f.ankle]);Vec3 target=ankle;target.y+=(depth+.0005f)/normal.y;const float reach=vlength(vsub(knee,hip))+vlength(vsub(ankle,knee));if(vlength(vsub(target,hip))<reach-.00001f){if(!solve_leg(p,f,target,world_rotation(s,p,f.ankle),e))return false;if(f.planted){Vec3 supported;if(!sole_point(p,side,f.marker,supported,e))return false;f.anchor[1]=i.y+supported.y;}}}
   }
  }
  if(dt>0){const auto previous=world(f.marker?toe:heel);f.previous={float(previous[0]),float(previous[1]),float(previous[2])};f.contact_age=confidence;f.history=true;}
  stats_.foot_weight[side]=f.weight;stats_.foot_planted[side]=f.planted;
 }
 return true;
}

bool WorldSkeletalHero::grip_socket(const world_animation::Pose&p,GripSocket&out,std::string&e)const{
 if(grip_profile_==1){if(p.nodes!=asset_.skeleton.joints.size()){e="Invalid independent-shield pose";return false;}out={};out.held=0;return true;}
 if(p.nodes!=asset_.skeleton.joints.size()||!inverse_point(p.global[hand_],origin(p.global[left_hand_]),out.wrist)||!inverse_point(p.global[hand_],origin(p.global[left_lower_]),out.pole)){e="Invalid weapon-relative offhand socket";return false;}
 out.rotation=qunit(qproduct(qconjugate(world_rotation(asset_.skeleton,p,hand_)),world_rotation(asset_.skeleton,p,left_hand_)));
 Vec3 palm;if(!inverse_point(p.global[hand_],vmul(vadd(origin(p.global[left_index_]),origin(p.global[left_ring_])),.5f),palm)){e="Invalid offhand palm frame";return false;}
 const auto&m=p.global[hand_].m;const float scale_x=vlength({m[0],m[1],m[2]}),scale_y=vlength({m[4],m[5],m[6]}),scale_z=vlength({m[8],m[9],m[10]});
 const float radial=std::hypot(palm.x*scale_x,palm.y*scale_y),z=palm.z*scale_z;
 // Sword uses the owned capture's hilt envelope, preserving its released
 // Run hand. Authored Axe/Staff families keep both palms on the shaft; their
 // contact socket follows the cutting edge and is not the shaft's axis.
 out.held=grip_profile_==2?1.f:std::min({smooth((.12f-radial)/.04f),smooth((z+.23f)/.04f),smooth((.14f-z)/.04f)});return true;
}
WorldSkeletalHero::GripSocket WorldSkeletalHero::mix_grips(const GripSocket&a,const GripSocket&b,float weight){return {vadd(a.wrist,vmul(vsub(b.wrist,a.wrist),weight)),vadd(a.pole,vmul(vsub(b.pole,a.pole),weight)),mixq(a.rotation,b.rotation,weight),a.held+(b.held-a.held)*weight};}
bool WorldSkeletalHero::constrain_grip(world_animation::Pose&p,const GripSocket&socket,std::string&e){
 float correction=0,residual=0;
 if(!constrain_grip_pose(p,socket,correction,residual,e))return false;
 if(socket.held>.00001f){++stats_.grip_solves;stats_.maximum_grip_error_m=std::max(stats_.maximum_grip_error_m,residual);stats_.maximum_clavicle_degrees=std::max(stats_.maximum_clavicle_degrees,float(correction*360/tau));}
 return true;
}
bool WorldSkeletalHero::constrain_grip_pose(world_animation::Pose&p,const GripSocket&socket,float&correction,float&residual,std::string&e)const{
 correction=residual=0;
 if(socket.held<=.00001f)return true;
 const auto&s=asset_.skeleton;const Vec3 wrist=origin(p.global[left_hand_]),raw_target=world_animation::transform_point(p.global[hand_],socket.wrist),raw_pole=world_animation::transform_point(p.global[hand_],socket.pole);
 const Vec3 target=vadd(wrist,vmul(vsub(raw_target,wrist),socket.held)),pole=vadd(origin(p.global[left_lower_]),vmul(vsub(raw_pole,origin(p.global[left_lower_])),socket.held));
 const Quat current_rotation=world_rotation(s,p,left_hand_),target_rotation=mixq(current_rotation,qunit(qproduct(world_rotation(s,p,hand_),socket.rotation)),socket.held);
 return solve_arm(p,left_clavicle_,left_upper_,left_lower_,left_hand_,target,pole,target_rotation,correction,residual,e);
}
bool WorldSkeletalHero::solve_arm(world_animation::Pose&p,unsigned clavicle,unsigned upper_joint,unsigned lower_joint,unsigned hand_joint,Vec3 target,Vec3 pole,Quat target_rotation,float&correction,float&residual,std::string&e)const{
 const auto&s=asset_.skeleton;const Vec3 wrist=origin(p.global[hand_joint]);const Quat current_rotation=world_rotation(s,p,hand_joint);correction=0;residual=vlength(vsub(target,wrist));
 const float rotation_dot=std::abs(current_rotation.x*target_rotation.x+current_rotation.y*target_rotation.y+current_rotation.z*target_rotation.z+current_rotation.w*target_rotation.w);
 if(vlength(vsub(target,wrist))<.000001f&&rotation_dot>1-.0000001f)return true;
 const Vec3 anchor=origin(p.global[clavicle]),shoulder=origin(p.global[upper_joint]),elbow=origin(p.global[lower_joint]);
 const float upper=vlength(vsub(elbow,shoulder)),lower=vlength(vsub(wrist,elbow)),reach=(upper+lower)*.99f,direct=vlength(vsub(target,shoulder));
 correction=0;
 if(direct>reach){const Vec3 bone=vsub(shoulder,anchor),goal=vsub(target,anchor);const double radius=vlength(bone),distance=vlength(goal);
  if(radius<.00001||distance<.00001){e="Degenerate arm clavicle sphere";return false;}
  const double cosine=(distance*distance+radius*radius-reach*reach)/(2*distance*radius);
  if(cosine>1.000001){e="Arm goal outside original clavicle/arm reach";return false;}
  const double current=std::acos(std::clamp(double(vdot(bone,goal))/(radius*distance),-1.,1.)),permitted=std::acos(std::clamp(cosine,-1.,1.));
  correction=float(std::max(0.,current-permitted));
  if(correction>25*tau/360+.00001){e="Arm clavicle correction exceeds 25 degrees: "+std::to_string(correction*360/tau);return false;}
  const Quat delta=mixq({},between(bone,goal),float(correction/std::max(.000001,current))),world=qproduct(delta,world_rotation(s,p,clavicle));const int parent=s.joints[clavicle].parent;
  p.local[clavicle].rotation=qunit(parent>=0?qproduct(qconjugate(world_rotation(s,p,unsigned(parent))),world):world);
  if(!world_animation::evaluate(s,p,e))return false;
 }
 const Vec3 shoulder_after=origin(p.global[upper_joint]);const float distance=vlength(vsub(target,shoulder_after));
 if(distance>upper+lower-.000002f||distance<std::abs(upper-lower)+.000002f){e="Arm goal is outside unstretched two-bone reach";return false;}
 // Precision analytic arm solve. Do not use the general IK helper's
 // small-angle identity cutoff: that can leave a visible socket residual.
 const Vec3 axis=vunit(vsub(target,shoulder_after));Vec3 side=vsub(pole,shoulder_after);side=vsub(side,vmul(axis,vdot(side,axis)));
 if(vlength(side)<.000001f){side=vsub(origin(p.global[lower_joint]),shoulder_after);side=vsub(side,vmul(axis,vdot(side,axis)));}
 if(vlength(side)<.000001f)side=vcross(axis,std::abs(axis.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0});
 side=vunit(side);
 const float along=(upper*upper+distance*distance-lower*lower)/(2*distance),height=std::sqrt(std::max(0.f,upper*upper-along*along));
 const Vec3 elbow_goal=vadd(shoulder_after,vadd(vmul(axis,along),vmul(side,height)));
 const Quat upper_world=qproduct(between(vsub(origin(p.global[lower_joint]),shoulder_after),vsub(elbow_goal,shoulder_after)),world_rotation(s,p,upper_joint));
 p.local[upper_joint].rotation=qunit(qproduct(qconjugate(world_rotation(s,p,clavicle)),upper_world));if(!world_animation::evaluate(s,p,e))return false;
 const Vec3 lower_origin=origin(p.global[lower_joint]);const Quat lower_world=qproduct(between(vsub(origin(p.global[hand_joint]),lower_origin),vsub(target,lower_origin)),world_rotation(s,p,lower_joint));
 p.local[lower_joint].rotation=qunit(qproduct(qconjugate(world_rotation(s,p,upper_joint)),lower_world));if(!world_animation::evaluate(s,p,e))return false;
 p.local[hand_joint].rotation=qunit(qproduct(qconjugate(world_rotation(s,p,lower_joint)),target_rotation));
 if(!world_animation::evaluate(s,p,e))return false;
 residual=vlength(vsub(origin(p.global[hand_joint]),target));
 if(residual>.00005f){e="Arm grip solver missed its exact wrist target: "+std::to_string(residual);return false;}
 return true;
}
bool WorldSkeletalHero::cartesian_right(const world_animation::Pose&a,const world_animation::Pose&b,float weight,world_animation::Pose&out,std::string&e,float*correction_degrees)const{
 if(correction_degrees)*correction_degrees=0;
 if(weight<=0||weight>=1)return true;
 // Cartesian bridges preserve a two-handed hold. Captured one-handed Run
 // releases retain their FK arc: a straight wrist path can cross the elbow's
 // inner unreachable sphere, and the free offhand should not constrain it.
 GripSocket a_grip,b_grip;if(!grip_socket(a,a_grip,e)||!grip_socket(b,b_grip,e))return false;
 if(std::min(a_grip.held,b_grip.held)<.999f)return true;
 Vec3 ap,bp,ae,be;if(!inverse_point(a.global[chest_],origin(a.global[right_hand_]),ap)||!inverse_point(b.global[chest_],origin(b.global[right_hand_]),bp)||!inverse_point(a.global[chest_],origin(a.global[right_lower_]),ae)||!inverse_point(b.global[chest_],origin(b.global[right_lower_]),be)){e="Invalid Cartesian wrist blend frame";return false;}
 const Quat aq=qunit(qproduct(qconjugate(world_rotation(asset_.skeleton,a,chest_)),world_rotation(asset_.skeleton,a,right_hand_))),bq=qunit(qproduct(qconjugate(world_rotation(asset_.skeleton,b,chest_)),world_rotation(asset_.skeleton,b,right_hand_)));
 const Vec3 target=world_animation::transform_point(out.global[chest_],vadd(ap,vmul(vsub(bp,ap),weight))),pole=world_animation::transform_point(out.global[chest_],vadd(ae,vmul(vsub(be,ae),weight)));
 const Quat rotation=qunit(qproduct(world_rotation(asset_.skeleton,out,chest_),mixq(aq,bq,weight)));float correction=0,residual=0;
 if(!solve_arm(out,right_clavicle_,right_upper_,right_lower_,right_hand_,target,pole,rotation,correction,residual,e)){e="Right wrist transition: "+e;return false;}
 if(correction_degrees)*correction_degrees=float(correction*360/tau);
 return true;
}

bool WorldSkeletalHero::prepare(unsigned color,unsigned shadow,std::uint64_t epoch,std::string&e){
 if(!loaded_){e="Hero CPU asset not loaded";return false;}
 if(!gpu_){gpu_=world_animation_render::create(asset_,1,e,256ull*1024*1024);if(!gpu_)return false;}
 if(epoch_!=epoch||!stats_.ready){if(!world_animation_render::warm(gpu_,color,e)||!world_animation_render::warm(gpu_,shadow,e))return false;epoch_=epoch;stats_.ready=true;}return true;
}
bool WorldSkeletalHero::upper_body(const world_animation::Pose&a,const world_animation::Pose&b,float weight,world_animation::Pose&out,std::string&e)const{
 out=a;
 for(unsigned n=0;n<out.nodes;++n)if(upper_mask_[n]){auto&o=out.local[n];const auto&v=b.local[n];
  o.translation={o.translation.x+(v.translation.x-o.translation.x)*weight,o.translation.y+(v.translation.y-o.translation.y)*weight,o.translation.z+(v.translation.z-o.translation.z)*weight};
  o.scale={o.scale.x+(v.scale.x-o.scale.x)*weight,o.scale.y+(v.scale.y-o.scale.y)*weight,o.scale.z+(v.scale.z-o.scale.z)*weight};o.rotation=mixq(o.rotation,v.rotation,weight);
 }
 return world_animation::evaluate(asset_.skeleton,out,e);
}
bool WorldSkeletalHero::sample_attack(unsigned direction,double frame,world_animation::Pose&captured,std::string&e)const{
 if(!world_animation::sample(asset_.skeleton,asset_.clips[clips_[10+direction]],(frame-1)/30.,false,captured,e)||!strip_attack(asset_.skeleton,pelvis_,rest_,captured,e))return false;
 // Aim the visible High cut and thrust through the reticle's central lane.
 // Captured spine rotation receives the same small global-Y correction in
 // display and contact sampling; pelvis/legs and source relative grips stay.
 const double aim_degrees=direction==2?5.:direction==4?14.:0.;
 if(aim_degrees){const double half=aim_degrees*tau/720;const Quat delta{0,float(std::sin(half)),0,float(std::cos(half))};const Quat world=qproduct(delta,world_rotation(asset_.skeleton,captured,spine_));const int parent=asset_.skeleton.joints[spine_].parent;
  captured.local[spine_].rotation=qunit(parent>=0?qproduct(qconjugate(world_rotation(asset_.skeleton,captured,unsigned(parent))),world):world);
  if(!world_animation::evaluate(asset_.skeleton,captured,e))return false;
 }
 return true;
}
#include "engine/world_skeletal_hero_foot_contact.inc"
#include "engine/world_skeletal_hero_stationary_cut.inc"
void WorldSkeletalHero::remember_motion(const world_animation::Pose&p,double dt,double clock){
 if(dt<=0)return;
 if(motion_count_){const double interval=clock-motion_history_[(motion_next_+momentum_history_limit-1)%momentum_history_limit].clock;motion_clock_interval_=interval>0&&interval<=.100001?interval:0;}
 motion_time_+=dt;auto&sample=motion_history_[motion_next_];sample.time=motion_time_;sample.clock=clock;
 for(unsigned k=0;k<momentum_joint_count;++k)sample.rotations[k]=p.local[momentum_joints_[k]].rotation;
 motion_next_=(motion_next_+1)%momentum_history_limit;motion_count_=std::min(momentum_history_limit,motion_count_+1);
}
WorldSkeletalHero::Momentum WorldSkeletalHero::momentum_snapshot(double windup_seconds,double attack_commit_clock)const{
 Momentum out;if(!was_attacking_||motion_count_<3)return out;
 out.duration=std::min(.080,std::max(.001,windup_seconds)*.5);
 const auto&latest=motion_history_[(motion_next_+momentum_history_limit-1)%momentum_history_limit];
 // Reconstruct only travel in time from the last actually published pose to
 // the physical commit timestamp. Never extrapolate an old/stale history, a
 // normal Ready attack, or more than one valid simulation interval /35ms.
 const double elapsed=attack_commit_clock-latest.clock;
 if(attack_commit_clock>=0&&latest.clock>=0&&motion_clock_interval_>0&&elapsed>0&&elapsed<=motion_clock_interval_+.000001&&elapsed<=.0350001)out.entry_elapsed=elapsed;
 std::array<const MotionSample*,momentum_history_limit> samples={};unsigned count=0;double oldest=0;
 for(unsigned n=0;n<motion_count_;++n){const auto&sample=motion_history_[(motion_next_+momentum_history_limit-1-n)%momentum_history_limit];const double age=latest.time-sample.time;if(age>.0750001)break;samples[count++]=&sample;oldest=age;}
 if(count<3||oldest<.025)return Momentum{};
 // A quadratic time fit estimates the endpoint derivative over up to75ms.
 // It uses at least3 observations and a minimum25ms span at every frame rate.
 // Center/scale time before normal equations to avoid tiny-second pivots.
 double normal[3][3]={};std::array<std::array<Vec3,3>,momentum_joint_count> rhs={};
 for(unsigned n=0;n<count;++n){const double t=(samples[n]->time-latest.time)/oldest,v[3]={1,t,t*t};for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b)normal[a][b]+=v[a]*v[b];
  for(unsigned k=0;k<momentum_joint_count;++k){const Vec3 angle=qlog(qproduct(samples[n]->rotations[k],qconjugate(latest.rotations[k])));for(unsigned a=0;a<3;++a)rhs[k][a]=vadd(rhs[k][a],vmul(angle,float(v[a])));}
 }
 for(unsigned k=0;k<momentum_joint_count;++k){double a[3][6]={};for(unsigned r=0;r<3;++r){for(unsigned c=0;c<3;++c)a[r][c]=normal[r][c];a[r][3]=rhs[k][r].x;a[r][4]=rhs[k][r].y;a[r][5]=rhs[k][r].z;}
  for(unsigned c=0;c<3;++c){unsigned pivot=c;for(unsigned r=c+1;r<3;++r)if(std::abs(a[r][c])>std::abs(a[pivot][c]))pivot=r;if(std::abs(a[pivot][c])<1.e-9)return Momentum{};for(unsigned b=0;b<6;++b)std::swap(a[c][b],a[pivot][b]);const double d=a[c][c];for(unsigned b=c;b<6;++b)a[c][b]/=d;for(unsigned r=0;r<3;++r)if(r!=c){const double f=a[r][c];for(unsigned b=c;b<6;++b)a[r][b]-=f*a[c][b];}}
  out.angular[k]={float(a[1][3]/oldest),float(a[1][4]/oldest),float(a[1][5]/oldest)};
  const float speed=vlength(out.angular[k]);if(speed>40)out.angular[k]=vmul(out.angular[k],40/speed);
 }
 // Quintic envelope max is16/81*T. Bound the whole articulated chain, not
 // each limb independently: <=45mm wrist,40mm elbow,12deg wrist rotation,
 // <=2deg on each torso joint. Lengths remain exact via local rotations.
 const double peak=out.duration*16/81+out.entry_elapsed;double wrist=0,elbow=0,angle=0,scale=1;
 for(unsigned k=0;k<momentum_joint_count;++k){const double speed=vlength(out.angular[k]);wrist+=wrist_radius_[k]*speed*peak;elbow+=elbow_radius_[k]*speed*peak;if(k<7)angle+=speed*peak;if(k<3&&speed*peak>tau/180)scale=std::min(scale,(tau/180)/(speed*peak));}
 if(wrist>.045)scale=std::min(scale,.045/wrist);
 if(elbow>.040)scale=std::min(scale,.040/elbow);
 if(angle>tau/30)scale=std::min(scale,(tau/30)/angle);
 out.scale=float(scale);for(auto&v:out.angular)v=vmul(v,out.scale);return out;
}
bool WorldSkeletalHero::apply_momentum(world_animation::Pose&p,const Momentum&m,double seconds,std::string&e)const{
 if(m.duration<=0||seconds<0||seconds>=m.duration||(seconds==0&&m.entry_elapsed==0))return true;
 const double u=seconds/m.duration,u2=u*u,u3=u2*u,u4=u3*u,u5=u4*u;
 const float tail=float(m.entry_elapsed*(1-10*u3+15*u4-6*u5)+m.duration*(u-6*u3+8*u4-3*u5));
 for(unsigned k=0;k<momentum_joint_count;++k){auto&rotation=p.local[momentum_joints_[k]].rotation;rotation=qunit(qproduct(qexp(vmul(m.angular[k],tail)),rotation));}
 return world_animation::evaluate(asset_.skeleton,p,e);
}
bool WorldSkeletalHero::attack_pose(unsigned direction,double phase,world_animation::Pose&out,std::string&e,double windup_seconds,GripSocket*grip,float*right_correction,const world_animation::Pose*entry_override,double strike_seconds,double recovery_seconds,const Momentum*momentum_override)const{
 if(direction>=5||!std::isfinite(phase)||phase<0||phase>1){e="Invalid hero attack sample";return false;}
 double frame,local;float weight=1;const auto&frames=attack_frames[direction];
 // Match frame velocity across the gameplay phase boundaries. Linear remaps
 // used to jump from slow windup directly to a much faster strike, then brake
 // again at recovery. These monotone Hermite segments retain exact contacts.
 const double windup_duration=windup_seconds>0?windup_seconds:windup_duration_;
 const double strike_duration=strike_seconds>0?strike_seconds:strike_duration_,recovery_duration=recovery_seconds>0?recovery_seconds:recovery_duration_;
 const double windup_velocity=(frames[1]-frames[0])/windup_duration,strike_velocity=(frames[2]-frames[1])/strike_duration,recovery_velocity=8/recovery_duration;
 const double at_contact=harmonic(windup_velocity,strike_velocity),at_recovery=harmonic(strike_velocity,recovery_velocity);
 if(phase<.36){local=phase/.36;frame=monotone_segment(frames[0],frames[1],0,at_contact,windup_duration,local);}
 else if(phase<.60){local=(phase-.36)/.24;frame=monotone_segment(frames[1],frames[2],at_contact,at_recovery,strike_duration,local);}
 else{local=(phase-.60)/.40;frame=monotone_segment(frames[2],frames[2]+8,at_recovery,0,recovery_duration,local);weight=1-smooth(float(local));}
 // Native source frames have uneven wrist travel in every direction, not
 // just High. Retime distance along the exact captured path, retaining each
 // contact/recovery key and matching physical phase speeds at the seams.
 {const auto&arc=attack_arcs_[direction];const double a=arc[440],b=arc[880],c=arc.back();
  const double contact=harmonic(a/windup_duration,(b-a)/strike_duration),recover=harmonic((b-a)/strike_duration,(c-b)/recovery_duration);double distance;
  if(phase<.36)distance=monotone_segment(0,a,0,contact,windup_duration,local);
  else if(phase<.60)distance=monotone_segment(a,b,contact,recover,strike_duration,local);
  else distance=monotone_segment(b,c,recover,0,recovery_duration,local);
  const auto it=std::lower_bound(arc.begin(),arc.end(),distance);const unsigned n=unsigned(it-arc.begin());
  frame=n==0?frames[0]:n>=arc.size()?frames[2]+8:attack_arc_frame(direction,n-1+(distance-arc[n-1])/(arc[n]-arc[n-1]));
 }
 world_animation::Pose captured;if(!sample_attack(direction,frame,captured,e))return false;
 const world_animation::Pose*cartesian_from=&base_;float cartesian_weight=weight;
 if(phase<.36){const float entry_weight=smooth(float(std::min(1.,local*((direction<=2)?1.5:1.0))));const auto&entry=entry_override?*entry_override:entry_.nodes?entry_:base_;cartesian_from=&entry;cartesian_weight=entry_weight;
  if(entry_weight>=1)out=captured;else if(!mixpose(asset_.skeleton,entry,captured,entry_weight,out,e))return false;
  if(grip){GripSocket a,b;if(!grip_socket(entry,a,e)||!grip_socket(captured,b,e))return false;*grip=mix_grips(a,b,entry_weight);}
 }else{if(!mixpose(asset_.skeleton,base_,captured,weight,out,e))return false;
  if(grip){GripSocket a,b;if(!grip_socket(base_,a,e)||!grip_socket(captured,b,e))return false;*grip=mix_grips(a,b,weight);}
 }
 if(stationary_cut_adaptation_&&direction!=2&&(was_attacking_?attack_legs_weight_:locomotion_)<.99999f){const auto&entry=entry_override?*entry_override:entry_.nodes?entry_:base_;if(!stationary_cut_legs(direction,phase,entry,out,e))return false;}
 if(!travel_legs(out,was_attacking_?attack_legs_weight_:locomotion_,e)||!cartesian_right(*cartesian_from,captured,cartesian_weight,out,e,right_correction))return false;
 if(phase<.36&&cartesian_weight>0&&cartesian_weight<1){
  // Slerping ENTRY toward a moving source orientation independently on every
  // frame changes hemisphere when that source crosses180 degrees from ENTRY.
  // The resulting sword flip can span a metre while its wrist moves smoothly.
  // Follow the captured rotation and decay one fixed entry-space correction;
  // it has a single hemisphere choice and exact entry/contact endpoints.
  world_animation::Pose first;if(!sample_attack(direction,frames[0],first,e))return false;
  const auto local_hand=[&](const world_animation::Pose&p){return qunit(qproduct(qconjugate(world_rotation(asset_.skeleton,p,chest_)),world_rotation(asset_.skeleton,p,right_hand_)));};
  const auto correction=qunit(qproduct(qconjugate(local_hand(first)),local_hand(*cartesian_from)));
  const auto target=qunit(qproduct(world_rotation(asset_.skeleton,out,chest_),qproduct(local_hand(captured),mixq(correction,{},cartesian_weight))));
  out.local[right_hand_].rotation=qunit(qproduct(qconjugate(world_rotation(asset_.skeleton,out,right_lower_)),target));
  if(!world_animation::evaluate(asset_.skeleton,out,e))return false;
 }
 if(!stationary_cut_adaptation_&&phase>=.60){
  // Prefixed weapon captures can rotate through 180 degrees from Idle while
  // recovering. Keep one final-capture-to-Idle correction, rather than letting
  // independent per-frame slerps reselect a hemisphere and flip the edge.
  world_animation::Pose last;if(!sample_attack(direction,frames[2]+8,last,e))return false;
  const auto local_hand=[&](const world_animation::Pose&p){return qunit(qproduct(qconjugate(world_rotation(asset_.skeleton,p,chest_)),world_rotation(asset_.skeleton,p,right_hand_)));};
  const auto correction=qunit(qproduct(qconjugate(local_hand(last)),local_hand(base_)));
  const auto target=qunit(qproduct(world_rotation(asset_.skeleton,out,chest_),qproduct(local_hand(captured),mixq({},correction,1-weight))));
  out.local[right_hand_].rotation=qunit(qproduct(qconjugate(world_rotation(asset_.skeleton,out,right_lower_)),target));
  if(!world_animation::evaluate(asset_.skeleton,out,e))return false;
 }
 if(phase<.36&&!apply_momentum(out,momentum_override?*momentum_override:attack_momentum_,phase/.36*windup_duration,e))return false;
 return true;
}
bool WorldSkeletalHero::travel_legs(world_animation::Pose&out,float weight,std::string&e)const{
 // A physically moving capsule must not carry a stationary attack stance over
 // the ground. Keep the captured spine, arms, fingers and animated weapon;
 // let pelvis/legs follow actual locomotion. The mask rises with resolved
 // travel and stays latched through a moving attack/combo: stopping must not
 // pull the feet back into a distant stationary capture halfway through a cut.
 const float legs=weight>.99999f?1.f:weight;
 if(legs>0){for(unsigned n=0;n<out.nodes;++n)if(!upper_mask_[n]){
   if(legs==1){out.local[n]=base_.local[n];continue;}
   auto&o=out.local[n];const auto&v=base_.local[n];
   o.translation={o.translation.x+(v.translation.x-o.translation.x)*legs,o.translation.y+(v.translation.y-o.translation.y)*legs,o.translation.z+(v.translation.z-o.translation.z)*legs};
   o.scale={o.scale.x+(v.scale.x-o.scale.x)*legs,o.scale.y+(v.scale.y-o.scale.y)*legs,o.scale.z+(v.scale.z-o.scale.z)*legs};o.rotation=mixq(o.rotation,v.rotation,legs);
  }return world_animation::evaluate(asset_.skeleton,out,e);}
 return true;
}

bool WorldSkeletalHero::apply_reaction(world_animation::Pose&out,std::string&e,const std::array<world_animation::Quat,world_animation::node_limit>*offset)const{
 if(!reaction_clips_)return true;
 // Rotate only the captured torso chain. Arms, fingers, weapon sockets and
 // their relative grip remain intact, while the same visible impulse also
 // modifies authoritative contact samples.
 const auto&delta=offset?*offset:reaction_offset_;
 for(unsigned n=0;n<out.nodes;++n)if(reaction_mask_[n])out.local[n].rotation=qunit(qproduct(out.local[n].rotation,delta[n]));
 return world_animation::evaluate(asset_.skeleton,out,e);
}

bool WorldSkeletalHero::sample_steel_pose(double phase,world_animation::Pose&out,std::string&e,world_animation::Pose*unreacted)const{
 GripSocket grip;
 if(!attack_pose(steel_direction_,phase,out,e,steel_timing_[0],&grip,nullptr,&resolved_query_entry_,steel_timing_[1],steel_timing_[2],&resolved_query_momentum_))return false;
 float correction=0,residual=0;
 if(!constrain_grip_pose(out,grip,correction,residual,e))return false;
 if(unreacted)*unreacted=out;
 return apply_reaction(out,e,&resolved_query_offset_);
}
bool WorldSkeletalHero::begin_steel_recoil(unsigned direction,double phase,const WorldTrainingCombat&fight,std::string&e){
 if(direction>=5||!std::isfinite(phase)||phase<0||phase>1||!resolved_query_valid_){e="Invalid physical steel contact phase";return false;}
 steel_direction_=direction;steel_contact_phase_=phase;steel_source_phase_=phase;
 const auto&timing=fight.committed_timing();steel_timing_={timing.windup,timing.strike,timing.recovery};
 if(!sample_steel_pose(phase,steel_contact_,e))return false;
 double a[3],b[3],c[3],d[3];
 if(!blade_pose(steel_contact_,0,0,0,0,a,b)){e="Invalid physical steel contact blade";return false;}
 // Retreat along this exact capture's earlier path. A fixed .29 endpoint
 // moved FORWARD when contact happened before .29, and a late rendered
 // Strike pose already lay beyond the blocking weapon. Neither is authority.
 steel_retreat_phase_=phase;steel_retreat_=steel_contact_;
 for(unsigned n=1;n<=96;++n){const double candidate=phase*(1-double(n)/96);world_animation::Pose sampled;
  if(!sample_steel_pose(candidate,sampled,e)||!blade_pose(sampled,0,0,0,0,c,d))return false;
  const double travel=std::max(std::hypot(a[0]-c[0],a[1]-c[1],a[2]-c[2]),std::hypot(b[0]-d[0],b[1]-d[1],b[2]-d[2]));
  if(travel>.18)break;
  steel_retreat_phase_=candidate;steel_retreat_=std::move(sampled);
 }
 // Retiming canonical phase alone made the actual blade whip through an
 // uneven portion of its windup. Measure the exact constrained rewind once,
 // then spend the unchanged recovery time by distance along that same path.
 // Seventeen samples in fixed storage keep this off the per-frame hot path.
 steel_rewind_arc_.fill(0);double prior_base[3],prior_tip[3];
 if(!blade_pose(steel_retreat_,0,0,0,0,prior_base,prior_tip))return false;
 for(unsigned n=2;n<steel_rewind_arc_.size();n+=2){world_animation::Pose sampled;
  const double source=steel_retreat_phase_*(1-double(n)/(steel_rewind_arc_.size()-1));
  if(!sample_steel_pose(source,sampled,e)||!blade_pose(sampled,0,0,0,0,c,d))return false;
  const double travel=std::max(std::hypot(c[0]-prior_base[0],c[1]-prior_base[1],c[2]-prior_base[2]),std::hypot(d[0]-prior_tip[0],d[1]-prior_tip[1],d[2]-prior_tip[2]));
  steel_rewind_arc_[n]=steel_rewind_arc_[n-2]+travel;steel_rewind_arc_[n-1]=(steel_rewind_arc_[n-2]+steel_rewind_arc_[n])*.5;std::copy_n(c,3,prior_base);std::copy_n(d,3,prior_tip);
 }
 steel_time_=0;stats_.steel_contact_phase=float(phase);stats_.steel_source_phase=float(phase);stats_.steel_retreat_phase=float(steel_retreat_phase_);
 ++stats_.steel_rebounds;return true;
}
bool WorldSkeletalHero::steel_recoil_pose(world_animation::Pose&result,double dt,std::string&e){
 steel_time_+=dt;
 constexpr double reverse_seconds=.075,rewind_seconds=.170,settle_seconds=.075;
 if(steel_time_<reverse_seconds){
  steel_source_phase_=steel_contact_phase_+(steel_retreat_phase_-steel_contact_phase_)*smooth(float(steel_time_/reverse_seconds));
  if(!sample_steel_pose(steel_source_phase_,result,e,&steel_unreacted_))return false;
 }else if(steel_time_<reverse_seconds+rewind_seconds){
  const float weight=smooth(float((steel_time_-reverse_seconds)/rewind_seconds));
  const double distance=steel_rewind_arc_.back()*weight;
  const auto it=std::lower_bound(steel_rewind_arc_.begin(),steel_rewind_arc_.end(),distance);const unsigned n=unsigned(it-steel_rewind_arc_.begin());
  const double index=n==0?0:n>=steel_rewind_arc_.size()?double(steel_rewind_arc_.size()-1):n-1+(distance-steel_rewind_arc_[n-1])/std::max(1.e-12,steel_rewind_arc_[n]-steel_rewind_arc_[n-1]);
  steel_source_phase_=steel_retreat_phase_*(1-index/(steel_rewind_arc_.size()-1));
  if(!sample_steel_pose(steel_source_phase_,result,e,&steel_unreacted_))return false;
 }else{
  // The sword has retraced its actual windup back to the captured entry.
  // Only then bridge into the current ready/locomotion pose, rather than
  // resuming the abandoned cutting-through-models Strike/Recovery curve.
  steel_source_phase_=0;world_animation::Pose entry,reacted_entry,ready=base_;
  if(!sample_steel_pose(0,reacted_entry,e,&entry))return false;
  const float weight=smooth(float((steel_time_-reverse_seconds-rewind_seconds)/settle_seconds));
  GripSocket a,b;float correction=0;
  if(!grip_socket(entry,a,e)||!grip_socket(ready,b,e)||!mixpose(asset_.skeleton,entry,ready,weight,result,e)||
     !cartesian_right(entry,ready,weight,result,e,&correction)||!constrain_grip(result,mix_grips(a,b,weight),e))return false;
  stats_.maximum_clavicle_degrees=std::max(stats_.maximum_clavicle_degrees,correction);
  steel_unreacted_=result;
  if(!apply_reaction(result,e))return false;
 }
 stats_.steel_source_phase=float(steel_source_phase_);
 stats_.steel_recoil=1-smooth(float((steel_time_-reverse_seconds)/(rewind_seconds+settle_seconds)));
 return true;
}

bool WorldSkeletalHero::sample_unarmed_loop(world_animation::Pose&target,std::string&e)const{
 const auto&s=asset_.skeleton;unsigned head=unsigned(s.joints.size());for(unsigned n=0;n<s.joints.size();++n)if(s.joints[n].name=="head"){head=n;break;}
 const float run=float(full_unarmed_run_weight_);
   const double cycle=full_unarmed_cycles_-std::floor(full_unarmed_cycles_);
   std::array<world_animation::Blend,3> blend={};unsigned count=0;
   blend[count++]={full_unarmed_clip_[0],idle_time_,1-locomotion_};
   blend[count++]={full_unarmed_clip_[1],cycle*asset_.clips[full_unarmed_clip_[1]].duration,locomotion_*(1-run)};
   if(full_unarmed_run_)blend[count++]={full_unarmed_clip_[2],cycle*asset_.clips[full_unarmed_clip_[2]].duration,locomotion_*run};
   if(!world_animation::blend(asset_,blend.data(),count,true,target,e))return false;
   // The retargeted quiet capture cranes the neck well ahead of the body's
   // bind posture. Retract through cervical rotation, preserving bone length
   // and the captured head's world orientation instead of tilting the chin up
   // or sliding the head through the neck. Fade out fully during locomotion.
   const float neck_weight=1-smooth(locomotion_/.65f);
   if(neck_weight>0){
    if(head<s.joints.size()&&s.joints[head].parent>=0){const unsigned neck=unsigned(s.joints[head].parent);
     if(s.joints[neck].name=="neck_01"&&s.joints[neck].parent>=0){
      const Vec3 captured=vsub(origin(target.global[head]),origin(target.global[neck]));
      const Vec3 neutral=vsub(origin(rest_.global[head]),origin(rest_.global[neck]));
      const double excess=std::remainder(std::atan2(captured.z,captured.y)-std::atan2(neutral.z,neutral.y),tau);
      const float correction=float(std::clamp(excess,0.,22*tau/360))*neck_weight;
      if(correction>0){const Quat gaze=world_rotation(s,target,head);
       const Quat rotation=qproduct(qexp({-correction,0,0}),world_rotation(s,target,neck));
       target.local[neck].rotation=qunit(qproduct(qconjugate(world_rotation(s,target,unsigned(s.joints[neck].parent))),rotation));
       if(!world_animation::evaluate(s,target,e))return false;
       target.local[head].rotation=qunit(qproduct(qconjugate(world_rotation(s,target,neck)),gaze));
       if(!world_animation::evaluate(s,target,e))return false;
      }
     }
    }
   }
   const double lift=full_unarmed_floor_[0]*(1-locomotion_)+locomotion_*((1-run)*full_unarmed_floor_[1]+run*full_unarmed_floor_[2]);
   const int parent=asset_.skeleton.joints[pelvis_].parent;Vec3 translation=vadd(origin(target.global[pelvis_]),{0,float(lift),0});
   if(parent>=0&&!inverse_point(target.global[unsigned(parent)],translation,translation)){e="Singular full-body unarmed parent";return false;}
   target.local[pelvis_].translation=translation;if(!world_animation::evaluate(asset_.skeleton,target,e))return false;
 return true;
}

bool WorldSkeletalHero::equipment_pose(world_animation::Pose&result,const Input&i,double dt,std::string&e){
 stats_.relaxed_walk_weight=0;
 stats_.full_unarmed=false;stats_.full_unarmed_transition=false;stats_.full_unarmed_weight=stats_.full_unarmed_run_blend=0;
 const auto previous_phase=equipment_.phase();full_unarmed_body_active_=false;
 if(!equipment_clips_available_){have_relaxed_walk_rotations_=false;if(i.equipment_stowed){e="No owned equipment transition captures loaded";return false;}return true;}
 if(i.reset){equipment_.reset(i.equipment_stowed);have_relaxed_walk_rotations_=false;}
 const bool changed=equipment_.advance(i.equipment_stowed,dt,i.paused);
 if(changed){equipment_entry_=have_pose_?pose_:result;have_relaxed_walk_rotations_=false;quiet_pelvis_lowering_=0;motion_previous_dt_=0;}
 using Phase=world_equipment_presentation::Phase;const auto phase=equipment_.phase();
 stats_.equipment_transition=equipment_.transitioning();stats_.equipment_progress=float(equipment_.progress());stats_.equipment_weapon_visible=equipment_.weapon_visible();
 if(full_unarmed_clips_&&(i.reset||changed||(previous_phase!=phase&&!equipment_.transitioning())))release_stance_anchors();
 if(phase==Phase::Armed){have_relaxed_walk_rotations_=false;return true;}
 world_animation::Pose captured,target;const double progress=equipment_.progress();
 if(full_unarmed_clips_){
  full_unarmed_body_active_=true;have_relaxed_walk_rotations_=false;
  const auto&s=asset_.skeleton;unsigned head=unsigned(s.joints.size());
  for(unsigned n=0;n<s.joints.size();++n)if(s.joints[n].name=="head"){head=n;break;}
  if(changed&&phase==Phase::Lowering){full_unarmed_cycles_=0;full_unarmed_run_weight_=0;}
  const float run=float(full_unarmed_run_weight_);
  if(phase==Phase::Raising)target=result;
  else if(!sample_unarmed_loop(target,e))return false;
  if(phase==Phase::Stowed&&!unarmed_motion_pose(target,i,dt,e))return false;
  if(equipment_.transitioning()){
   const float weight=smooth(float(progress));
   if(!mixpose(s,equipment_entry_,target,weight,result,e))return false;
   // Interpolating the neck and its counterrotated child independently can
   // introduce a transient chin tilt. Interpolate the intended global gaze,
   // then express it beneath the already blended neck instead.
   if(head<s.joints.size()&&s.joints[head].parent>=0){
    const Quat gaze=mixq(world_rotation(s,equipment_entry_,head),world_rotation(s,target,head),weight);
    result.local[head].rotation=qunit(qproduct(qconjugate(world_rotation(s,result,unsigned(s.joints[head].parent))),gaze));
    if(!world_animation::evaluate(s,result,e))return false;
   }
  }else result=std::move(target);
  // This bridge changes the stance as well as the hands. Old wide combat
  // anchors cannot survive it: no anchor is allowed to plant until its end.
  transition_contact_={0,0};turn_mode_=0;turn_reference_yaw_=turn_last_yaw_=i.yaw;
  stats_.full_unarmed=true;stats_.full_unarmed_transition=equipment_.transitioning();
  stats_.full_unarmed_weight=phase==Phase::Stowed?1.f:phase==Phase::Lowering?smooth(float(progress)):1-smooth(float(progress));
  stats_.full_unarmed_cycles=full_unarmed_cycles_;stats_.full_unarmed_run_blend=run;
  if(phase!=Phase::Raising){
   if(locomotion_>.01f){stats_.relaxed_walk_weight=locomotion_;++stats_.relaxed_walk_frames;++stats_.full_unarmed_walk_frames;if(run>.05f)++stats_.full_unarmed_run_frames;}
   else ++stats_.full_unarmed_idle_frames;
  }
  return true;
 }
 // A bounded bridge to the genuine source relaxed pose. The complete source
 // transitions contain a flourish and are NOT time-compressed into0.4seconds.
 // This is a pose bridge, not a newly authored sheath/draw capture.
 if(phase==Phase::Raising)target=result;
 else{const auto&source=asset_.clips[equipment_clips_[1]];
  if(!world_animation::sample(asset_.skeleton,source,idle_time_,true,captured,e))return false;
  // The owned relaxed idle remains still when the resolved root is blocked.
  // Free arms follow the accepted foot-distance phase while walking, instead
  // of pinning a longsword idle over every moving body. Only spine descendants
  // change: the calibrated starts/stops, strides, support feet and pelvis stay
  // on the existing locomotion path. Running uses the same free-arm cycle per
  // stride; its accepted lower-body capture remains unchanged.
  if(relaxed_walk_clip_available_&&locomotion_>0){
   const auto&walk=asset_.clips[relaxed_walk_clip_];const double phase=cycles_+gait_offset_;
   world_animation::Pose moving,relaxed;
   if(!world_animation::sample(asset_.skeleton,walk,(phase-std::floor(phase))*walk.duration,true,moving,e))return false;
   // Foot capture starts/stops may rebase gait_offset_ to preserve a planted
   // sole. Their phase correction must not snap free hands to another point
   // in the human swing. Filter only this optional capture's upper rotations,
   // with actual animation dt; the current relaxed idle and foot solver are
   // untouched. Paused updates return before reaching this stateful layer.
   const float follow=float(-std::expm1(-dt/.065));const bool had_rotations=have_relaxed_walk_rotations_;
   for(unsigned n=0;n<moving.nodes;++n)if(upper_mask_[n]&&n!=spine_){
    auto&rotation=relaxed_walk_rotations_[n];const auto target=moving.local[n].rotation;
    if(!had_rotations)rotation=captured.local[n].rotation;
    rotation=mixq(rotation,target,follow);moving.local[n].rotation=rotation;
   }
   if(!upper_body(captured,moving,locomotion_,relaxed,e))return false;
   captured=std::move(relaxed);
   // A foot transition can also change the accepted pelvis orientation. Keep
   // the free torso's rotation continuous in world space, then derive only
   // spine_01's local rotation relative to that untouched current parent.
   // Other upper joints keep their local captured articulation. This is not
   // a root/leg correction and never runs on armed or full-body story poses.
   const int parent=asset_.skeleton.joints[spine_].parent;
   const auto parent_rotation=parent>=0?world_rotation(asset_.skeleton,result,unsigned(parent)):Quat{};
   const auto wanted=qunit(qproduct(parent_rotation,captured.local[spine_].rotation));
   auto&spine_rotation=relaxed_walk_rotations_[spine_];
   if(!had_rotations)spine_rotation=have_pose_&&!i.reset?world_rotation(asset_.skeleton,pose_,spine_):wanted;
   spine_rotation=mixq(spine_rotation,wanted,follow);
   captured.local[spine_].rotation=qunit(qproduct(qconjugate(parent_rotation),spine_rotation));
   have_relaxed_walk_rotations_=true;stats_.relaxed_walk_weight=locomotion_;++stats_.relaxed_walk_frames;
  }else have_relaxed_walk_rotations_=false;
  if(!upper_body(result,captured,1,target,e))return false;
 }
 if(equipment_.transitioning()){
  world_animation::Pose layered;if(!upper_body(result,equipment_entry_,1,layered,e)||!upper_body(layered,target,smooth(float(progress)),result,e))return false;
  // The offhand is allowed to release while equipment lowers; forcing the
  // armed two-handed socket onto a relaxed hand would stretch the real arm.
 }else result=std::move(target);
 return true;
}

bool WorldSkeletalHero::update_cpu(const Input&given,const WorldTrainingCombat&fight,std::string&e){
 Input resolved=given;const Input &i=resolved;
 if(!loaded_||!std::isfinite(i.dt)||i.dt<0||!std::isfinite(i.x)||!std::isfinite(i.y)||!std::isfinite(i.z)||!std::isfinite(i.dx)||!std::isfinite(i.dz)||!std::isfinite(i.yaw)||!std::isfinite(fight.impact_pause)||!std::isfinite(fight.clock)||!std::isfinite(fight.attack_commit_clock)||i.reaction_direction>=5){e="Invalid hero animation input";return false;}
 // Selected clothing remains on the exact accepted skeleton for genuine
 // swimming/death clips. These full-body poses never run combat grips/foot IK.
 const bool special_swimming=i.swimming&&story_swim_clips_,special_dead=i.dead&&story_death_clip_;
 const auto old_story=story_pose_.sample();
 const std::array<double,3> story_durations={story_swim_clips_?asset_.clips[story_clips_[0]].duration:1.,story_swim_clips_?asset_.clips[story_clips_[1]].duration:1.,story_death_clip_?asset_.clips[story_clips_[2]].duration:1.};
 world_story_body_pose::Input story_input;story_input.swimming=special_swimming;story_input.submerged=special_swimming&&i.submerged;
 story_input.dead=special_dead;story_input.moving=i.dt>0&&std::hypot(i.dx,i.dz)/i.dt>.12;
 story_input.dt=i.dt;story_input.paused=i.paused;story_input.reset=i.reset;
 if(!story_pose_.update(story_input,story_durations)){e="Invalid story body clock";return false;}
 const auto story=story_pose_.sample();story_hidden_=story.hidden;
 if(story.changed){
  quiet_pelvis_lowering_=0;
  story_have_entry_=have_pose_;if(have_pose_)story_entry_=pose_;
  if(!story.active){++stats_.special_exits;resolved.reset=true;}
  else if(!old_story.active)++stats_.special_entries;else ++stats_.special_mode_changes;
  for(auto&foot:feet_){foot.planted=foot.releasing=foot.history=false;foot.weight=0;}
  stats_.foot_planted={};stats_.foot_weight={};stats_.foot_residual_m={};
 }
 stats_.swimming=story.mode==world_story_body_pose::Mode::SwimIdle||story.mode==world_story_body_pose::Mode::SwimForward||story.mode==world_story_body_pose::Mode::SwimDive;
 stats_.submerged=story.mode==world_story_body_pose::Mode::SwimDive;stats_.dead=story.mode==world_story_body_pose::Mode::Death;
 stats_.special_mode=unsigned(story.mode);stats_.special_transition=story.transition;stats_.special_progress=story.blend;
 if(story.active){
  quiet_pelvis_lowering_=0;reset_unarmed_motion();
  stats_.relaxed_walk_weight=0;have_relaxed_walk_rotations_=false;
  full_unarmed_body_active_=false;full_unarmed_run_weight_=0;stats_.full_unarmed=false;stats_.full_unarmed_transition=false;stats_.full_unarmed_weight=stats_.full_unarmed_run_blend=0;
  x_=i.x;y_=i.y;z_=i.z;yaw_=i.yaw;feedback_clock_=fight.clock;
  const unsigned slot=stats_.dead?2u:story.mode==world_story_body_pose::Mode::SwimIdle?0u:1u;
  world_animation::Pose sampled;
  if(!world_animation::sample(asset_.skeleton,asset_.clips[story_clips_[slot]],story.seconds,!stats_.dead,sampled,e))return false;
  if(story.transition&&story_have_entry_){world_animation::Pose bridged;if(!mixpose(asset_.skeleton,story_entry_,sampled,story.blend,bridged,e))return false;sampled=std::move(bridged);}
  pose_=std::move(sampled);unreacted_=pose_;have_pose_=true;was_attacking_=false;have_locomotion_pose_=false;
  motion_count_=motion_next_=0;motion_time_=motion_clock_interval_=0;attack_momentum_={};
  guard_weight_=reaction_weight_=attack_legs_weight_=attack_legs_goal_=attack_legs_rate_=0;guard_target_valid_=false;
  reaction_offset_={};reaction_entry_={};steel_serial_=0;resolved_query_valid_=false;turn_mode_=0;
  contact_token_=std::uint64_t(fight.hits)+fight.guarded+fight.parries+fight.blocks+fight.enemy_hits;
  reaction_serial_=i.reaction_serial;clash_token_=std::uint64_t(fight.blocks)+fight.parries+fight.guarded;
  equipment_.reset(true);stats_.equipment_transition=false;stats_.equipment_weapon_visible=false;stats_.equipment_progress=1;
  stats_.speed=stats_.run=stats_.locomotion=stats_.guard=stats_.reaction=stats_.guard_recoil=stats_.attack_legs=0;
  ++stats_.updates;if(stats_.swimming)++stats_.swim_frames;if(stats_.submerged)++stats_.submerged_frames;if(stats_.dead)++stats_.death_frames;
  if(i.reset)world_character_face::reset(face_);
  world_character_face::update(asset_,face_,{i.paused?0:std::min(i.dt,.1),0,0,0,0,stats_.dead,stats_.swimming},pose_);
  return true;
 }
 if(!story.transition)story_have_entry_=false;
 // Keep locomotion/contact transitions below a 60Hz step even when the
 // renderer is slower. Resolved roots are interpolated only for these hidden
 // animation steps; physics and combat still advance once in their owner.
 // Authored attack/contact clocks retain their existing single-step path.
 if(have_pose_&&!story.transition&&!i.reset&&!i.paused&&fight.impact_pause<=0&&fight.player_phase==TrainingPlayerPhase::Ready&&i.dt>1./60.+1.e-7&&i.dt<=.1&&std::hypot(i.x-x_,i.z-z_)<1&&std::abs(i.y-y_)<.5){
  const unsigned count=unsigned(std::ceil(i.dt/(1./60.)));const double x=x_,y=y_,z=z_,yaw=yaw_,clock=feedback_clock_;
  for(unsigned step=1;step<=count;++step){const double t=double(step)/count;Input sample=i;sample.dt=i.dt/count;sample.dx=i.dx/count;sample.dz=i.dz/count;sample.x=x+(i.x-x)*t;sample.y=y+(i.y-y)*t;sample.z=z+(i.z-z)*t;sample.yaw=float(yaw+std::remainder(double(i.yaw)-yaw,tau)*t);
   auto state=fight;if(clock>=0&&fight.clock>=clock&&fight.clock-clock<=.100001)state.clock=clock+(fight.clock-clock)*t;
   if(!update_cpu(sample,state,e))return false;
  }return true;
 }
 if(i.guard_target_valid){if(i.guard_target_direction>=5){e="Invalid guard threat direction";return false;}for(double v:i.guard_target_world)if(!std::isfinite(v)||std::abs(v)>1.e7){e="Invalid guard threat point";return false;}}
 // A genuine pause freezes every visual layer. Impact hit-stop first commits
 // a newly published contact once, then holds it without ticking gait, guard
 // filters or reaction clocks. Gameplay/physical position remains authoritative.
 const std::uint64_t contact_token=std::uint64_t(fight.hits)+fight.guarded+fight.parries+fight.blocks+fight.enemy_hits;
 const bool new_outcome=fight.resolved_attack_serial&&fight.resolved_attack_serial!=resolved_seen_;
 const bool new_contact=contact_token!=contact_token_||i.reaction_serial!=reaction_serial_||new_outcome;
 const bool hitstop=fight.impact_pause>0;
 const double dt=i.paused||hitstop?0:std::min(i.dt,.1);
 if(dt==0||i.dt>.1||std::hypot(i.x-x_,i.z-z_)>=1||std::abs(i.y-y_)>=.5)motion_previous_dt_=0;
 if(i.reset){reset_unarmed_motion();world_character_face::reset(face_);}else commit_unarmed_motion(i,dt);
 const bool stationary_turn=!motion_enabled_&&have_pose_&&!i.reset&&dt>0&&std::hypot(i.dx,i.dz)<.000001&&std::abs(std::remainder(double(i.yaw)-yaw_,tau))>dt*.10;
 if(stationary_turn&&(!turn_clips_available_||(full_unarmed_clips_&&(i.equipment_stowed||full_unarmed_body_active_))))for(auto&foot:feet_){if(foot.planted)++stats_.foot_releases;foot.planted=false;foot.releasing=true;foot.weight=0;foot.history=false;foot.cooldown=.10;}
 x_=i.x;y_=i.y;z_=i.z;yaw_=i.yaw;
 if(hitstop)++stats_.hitstop_frames;
 if(i.reset||i.paused||hitstop||i.dt>.1||std::hypot(i.dx,i.dz)>=1){motion_count_=motion_next_=0;motion_time_=motion_clock_interval_=0;if(i.reset)attack_momentum_={};}
 if(have_pose_&&!i.reset&&(i.paused||(dt==0&&!new_contact))){++stats_.updates;return true;}
 const double dist=std::hypot(i.dx,i.dz);const bool traveled=dt>0&&dist<1&&!i.reset;
 const float actual=traveled?float(dist/dt):0;
 if(i.reset){quiet_pelvis_lowering_=0;speed_=run_=locomotion_=guard_weight_=reaction_weight_=attack_legs_weight_=attack_legs_goal_=attack_legs_rate_=0;cycles_=idle_time_=heading_=full_unarmed_cycles_=full_unarmed_run_weight_=0;direction_={1,0,0,0,0,0,0,0};have_pose_=was_attacking_=have_guard_pose_=false;exit_time_=reaction_time_=1;reaction_serial_=i.reaction_serial;clash_token_=std::uint64_t(fight.blocks)+fight.parries+fight.guarded;reaction_offset_={};reaction_entry_={};reaction_clash_=false;for(auto&foot:feet_){foot.weight=0;foot.planted=foot.releasing=foot.history=false;foot.contact_age=foot.cooldown=foot.toe_off=0;}}
 if(i.reset||(feedback_clock_>=0&&fight.clock+1.e-6<feedback_clock_)||fight.attack_start_serial<steel_serial_){resolved_seen_=i.reset?fight.resolved_attack_serial:0;steel_serial_=0;steel_time_=0;resolved_query_valid_=false;resolved_query_offset_={};stats_.steel_recoil=stats_.steel_contact_phase=stats_.steel_source_phase=stats_.steel_retreat_phase=0;}
 feedback_clock_=fight.clock;
 const float follow=float(-std::expm1(-dt/.075));speed_+=(actual-speed_)*follow;
 if(actual<.025f&&speed_<.035f)speed_=0;
 const float forward=dist>.00001?float((i.dx*std::sin(yaw_)+i.dz*std::cos(yaw_))/dist):1.f;
 const float run_goal=smooth((actual-1.8f)/1.2f)*smooth((forward-.65f)/.30f),old_run=run_;// Release Run a little faster as walking resumes; keep the distance
 // integral and the captured stop-tail prediction on the same filter.
 const double run_tau=run_goal<run_?.045:.075;run_+=(run_goal-run_)*float(-std::expm1(-dt/run_tau));
 const float run_average=dt>0?run_goal+(old_run-run_goal)*float(run_tau*(-std::expm1(-dt/run_tau))/dt):run_;
 // Human full-body captures have their own measured pace: the long run
 // stride was recorded much faster than the combat run. Blend it by actual
 // speed between the two captured paces, rather than inheriting the armed
 // threshold that reached full Run at only3m/s.
 double unarmed_phase_per_m=1/full_unarmed_stride_[0];
 if(full_unarmed_clips_){
  const double walk_pace=full_unarmed_stride_[0]/asset_.clips[full_unarmed_clip_[1]].duration;
  const double run_pace=full_unarmed_run_?full_unarmed_stride_[1]/asset_.clips[full_unarmed_clip_[2]].duration:walk_pace;
  const double goal=full_unarmed_run_?std::clamp((actual-walk_pace)/std::max(.01,run_pace-walk_pace),0.,1.)*smooth((forward-.65f)/.30f):0.;
  const double old=full_unarmed_run_weight_,response=goal<old?.045:.075;
  full_unarmed_run_weight_=goal+(old-goal)*std::exp(-dt/response);
  // Pose and support trajectories blend linearly in stride. Integrate the
  // reciprocal of that same stride through the short exponential response;
  // a harmonic mix of endpoint frequencies would make the soles slide.
  const double delta=full_unarmed_stride_[1]-full_unarmed_stride_[0],a=full_unarmed_stride_[0]+delta*goal,b=delta*(old-goal);
  unarmed_phase_per_m=dt>0?(1+response/dt*std::log1p(b*std::expm1(-dt/response)/(a+b)))/a:1/(a+b);
 }
 const float previous_locomotion=locomotion_;
 const float move_goal=smooth(actual/.24f);locomotion_+=(move_goal-locomotion_)*float(-std::expm1(-dt/(move_goal>locomotion_?.075:.14)));
 if(locomotion_<.00001f&&move_goal==0)locomotion_=0;
 // A walking contact is not the final quiet stance. Fade its support out once
 // the stop has nearly settled, then plant the actual neutral capture's feet.
 // Keeping that old horizontal anchor forever can pull a relaxed leg inward
 // or leave it spread at its last walking-step position.
 if(!motion_enabled_&&full_unarmed_clips_&&i.equipment_stowed&&actual<.025f&&previous_locomotion>.08f&&locomotion_<=.08f){
  for(auto&foot:feet_){if(foot.planted)++stats_.foot_releases;foot.planted=false;foot.releasing=true;foot.cooldown=.35;foot.history=false;foot.contact_age=0;}
 }
 if(dist>.00001&&actual>.025){
  const double f=(i.dx*std::sin(yaw_)+i.dz*std::cos(yaw_))/dist,r=(-i.dx*std::cos(yaw_)+i.dz*std::sin(yaw_))/dist;
  const double goal=std::atan2(r,f);heading_+=std::remainder(goal-heading_,tau)*double(follow);heading_=std::remainder(heading_,tau);
 }
 // Only the two neighbouring authored directions contribute. Filtering all
 // eight weights retained contradictory forward/back poses during quick turns.
 double direction=heading_;if(direction<0)direction+=tau;const double v=direction/(tau/8),vf=std::floor(v);const unsigned a=unsigned(vf)%8,b=(a+1)%8;
 direction_={};direction_[a]=1-float(v-vf);direction_[b]=float(v-vf);
 constexpr double stride[8]={1.58687,1.43124,1.38528,1.10278,1.31857,1.24039,1.38565,1.17019};
 double walk_stride=0;for(unsigned n=0;n<8;++n)walk_stride+=direction_[n]*stride[n];
 // No collision-resolved distance means no foot-cycle advance, even while the
 // posture filter settles. Keep Run's complete two-cycle capture and parity.
 if(traveled)cycles_+=dist*((1-run_average)/std::max(.5,walk_stride)+run_average/2.45294);
 if(traveled&&full_unarmed_clips_)full_unarmed_cycles_+=dist*unarmed_phase_per_m;
 idle_time_+=dt;
 std::array<world_animation::Blend,4> blends={};unsigned count=0;
 blends[count++]={clips_[0],idle_time_,1-locomotion_};
 for(unsigned n=0;n<8;++n)if(direction_[n]>0)blends[count++]={clips_[n+1],std::fmod(cycles_,1.)*asset_.clips[clips_[n+1]].duration,locomotion_*(1-run_)*direction_[n]};
 blends[count++]={clips_[9],std::fmod(cycles_,2.)*.5*asset_.clips[clips_[9]].duration,locomotion_*run_};
 if(!world_animation::blend(asset_,blends.data(),count,true,base_,e))return false;
 // A genuine relaxed body owns its legs and contact phase. Do not run the
 // armed start/stop/turn captures behind it: those can retire the wrong foot
 // anchor even when their final pose would subsequently be overwritten.
 const bool relaxed_lower=full_unarmed_clips_&&(i.equipment_stowed||equipment_.phase()==world_equipment_presentation::Phase::Lowering||equipment_.phase()==world_equipment_presentation::Phase::Stowed);
 if(relaxed_lower){locomotion_mode_=0;have_locomotion_pose_=false;previous_actual_=0;gait_offset_=0;transition_contact_={0,0};}
 else if(!lower_locomotion(base_,i,actual,dt,e))return false;
 if(i.reset)equipment_.reset(i.equipment_stowed);
 const bool attacking=fight.player_phase!=TrainingPlayerPhase::Ready;
 if((i.equipment_stowed||!equipment_.combat_ready())&&(attacking||fight.guard)){e="Stow/draw requires neutral combat controls";return false;}
 guard_target_valid_=i.guard_target_valid&&fight.guard&&!attacking;
 guard_target_direction_=i.guard_target_direction;guard_target_world_=i.guard_target_world;
 stats_.guard_adjustment_m=stats_.guard_target_residual_m=0;
 guard_base_=base_;
 const bool foot_entry_boundary=(attacking&&fight.player_time==0&&(!was_attacking_||attack_serial_!=fight.attack_start_serial))||(!attacking&&was_attacking_);
 const float guard_goal=guard_clips_&&fight.guard&&!attacking?1.f:0.f;
 guard_weight_+=(guard_goal-guard_weight_)*float(-std::expm1(-dt/.065));
 if(guard_clips_&&(guard_goal>0||guard_weight_>.0001f)){
  world_animation::Pose desired;
  if(!world_animation::sample(asset_.skeleton,asset_.clips[guards_[std::min(4u,unsigned(fight.selected))]],idle_time_,true,desired,e))return false;
  {world_animation::Pose layered;if(!upper_body(base_,desired,1,layered,e))return false;desired=std::move(layered);}
  if(!fit_guard_pose(desired,std::min(4u,unsigned(fight.selected)),e,&stats_.guard_adjustment_m,&stats_.guard_target_residual_m))return false;
  GripSocket desired_socket;if(!grip_socket(desired,desired_socket,e))return false;
  // Wrist/fingers share one captured hold; retain each direction's authored
  // elbow hint rather than forcing every elbow into the Left guard plane.
  desired_socket.wrist=guard_socket_.wrist;desired_socket.rotation=guard_socket_.rotation;
  if(!have_guard_pose_){guard_pose_=desired;guard_blend_socket_=desired_socket;have_guard_pose_=true;}
  else{const float weight=float(-std::expm1(-dt/.055));const auto previous=guard_pose_;float correction=0;
   if(!mixpose(asset_.skeleton,previous,desired,weight,guard_pose_,e)||!cartesian_right(previous,desired,weight,guard_pose_,e,&correction))return false;
   guard_blend_socket_=mix_grips(guard_blend_socket_,desired_socket,weight);
   if(!constrain_grip(guard_pose_,guard_blend_socket_,e))return false;
   stats_.maximum_clavicle_degrees=std::max(stats_.maximum_clavicle_degrees,correction);
  }
  GripSocket before;if(!grip_socket(base_,before,e))return false;const GripSocket goal=mix_grips(before,guard_blend_socket_,guard_weight_);
  if(!upper_body(base_,guard_pose_,guard_weight_,base_,e)||!constrain_grip(base_,goal,e))return false;
 }
 world_animation::Pose result=base_;
 if(attacking){
  const bool fresh=!was_attacking_||attack_serial_!=fight.attack_start_serial;
  if(fresh){entry_=have_pose_?unreacted_:base_;attack_serial_=fight.attack_start_serial;++stats_.attacks;exit_time_=1;
   if(!was_attacking_){attack_legs_weight_=attack_legs_goal_=previous_locomotion;attack_legs_rate_=0;}
   // Physical commit already chose this direction's complete timing. A new
   // serial may be queried at contact BEFORE its first visual Windup update;
   // neither the old serial's release nor recovery may shape that curve.
   const auto&timing=fight.committed_timing();
   windup_duration_=fight.player_phase==TrainingPlayerPhase::Windup?std::max(.001,fight.player_duration):timing.windup;
   strike_duration_=timing.strike;recovery_duration_=timing.recovery;
   attack_momentum_=momentum_snapshot(windup_duration_,fight.attack_commit_clock);
  }
  if(fight.player_phase==TrainingPlayerPhase::Strike)strike_duration_=std::max(.001,fight.player_duration);
  if(fight.player_phase==TrainingPlayerPhase::Recovery)recovery_duration_=std::max(.001,fight.player_duration);
  // Keep the largest resolved movement context through this attack/combo.
  // An exact critically damped rise avoids a first-frame leg pop when travel
  // begins during a stationary cut, and is independent of render frame rate.
  attack_legs_goal_=std::max(attack_legs_goal_,move_goal);
  if(dt>0&&attack_legs_weight_<attack_legs_goal_){constexpr double tau_legs=.075;const double decay=std::exp(-dt/tau_legs),delta=attack_legs_weight_-attack_legs_goal_,c=attack_legs_rate_+delta/tau_legs;
   const float next=float(attack_legs_goal_+(delta+c*dt)*decay);
   attack_legs_rate_=float((attack_legs_rate_-c*dt/tau_legs)*decay);
   attack_legs_weight_=std::clamp(next,attack_legs_weight_,attack_legs_goal_);
  }
  was_attacking_=true;
  const double f=std::clamp(fight.player_time/std::max(.001,fight.player_duration),0.,1.);
  const double phase=fight.player_phase==TrainingPlayerPhase::Windup?.36*f:fight.player_phase==TrainingPlayerPhase::Strike?.36+.24*f:.60+.40*f;
  GripSocket goal;float right_correction=0;if(!attack_pose(std::min(4u,unsigned(fight.attack_direction)),phase,result,e,0,&goal,&right_correction)||!constrain_grip(result,goal,e)){e="Attack direction "+std::to_string(unsigned(fight.attack_direction))+" phase "+std::to_string(phase)+": "+e;return false;}
  stats_.maximum_clavicle_degrees=std::max(stats_.maximum_clavicle_degrees,right_correction);
 }else{
  // Damage, guard, state changes or an interrupted combo can leave an attack
  // before its authored recovery. Bridge from the displayed pose, not rest.
  if(was_attacking_){entry_=unreacted_;exit_time_=0;}
  if(exit_time_<.12){const float weight=smooth(float(exit_time_/.12));GripSocket a,b;if(!grip_socket(entry_,a,e)||!grip_socket(base_,b,e))return false;
   float right_correction=0;if(!mixpose(asset_.skeleton,entry_,base_,weight,result,e)||!travel_legs(result,std::max(locomotion_,attack_legs_weight_),e)||!cartesian_right(entry_,base_,weight,result,e,&right_correction)||!constrain_grip(result,mix_grips(a,b,weight),e))return false;
   stats_.maximum_clavicle_degrees=std::max(stats_.maximum_clavicle_degrees,right_correction);exit_time_+=dt;}
  if(exit_time_>=.12)attack_legs_weight_=attack_legs_goal_=attack_legs_rate_=0;
 }
 bool steel_started=false;
 if(fight.resolved_attack_serial&&fight.resolved_attack_serial!=resolved_seen_){
  // Retrospective physical queries retain the pre-resolution torso impulse.
  // The newly resolved clang/rebound cannot bend the sweep that caused it.
  resolved_seen_=fight.resolved_attack_serial;resolved_query_offset_=reaction_offset_;
  resolved_query_entry_=entry_;resolved_query_momentum_=attack_momentum_;resolved_query_valid_=true;
  if(attacking&&fight.resolved_attack_serial==fight.attack_start_serial&&fight.resolved_attack_outcome==TrainingAttackOutcome::EnemyBlock){
   if(!begin_steel_recoil(std::min(4u,unsigned(fight.resolved_attack_direction)),fight.resolved_attack_phase,fight,e))return false;
   steel_serial_=fight.resolved_attack_serial;steel_started=true;
  }
 }
 stats_.steel_recoil=0;
 const bool steel_applied=steel_serial_&&steel_serial_==fight.attack_start_serial&&attacking;
 if(steel_applied){
  if(!steel_recoil_pose(result,steel_started?0:dt,e))return false;
 }else steel_serial_=0;
 if(!equipment_pose(result,i,dt,e))return false;
 // Buffered attacks and their bounded momentum start at the blade/upper body
 // that was actually shown after a steel rebound, not its abandoned follow-through.
 unreacted_=steel_applied?steel_unreacted_:result;remember_motion(unreacted_,dt,fight.clock);
 const std::uint64_t clash_token=std::uint64_t(fight.blocks)+fight.parries+fight.guarded;
 const bool body_impulse=reaction_clips_&&i.reaction_serial!=reaction_serial_;
 const bool clash_impulse=reaction_clips_&&clash_token>clash_token_&&!steel_started;
 if(body_impulse||clash_impulse){reaction_entry_=reaction_offset_;reaction_clash_=!body_impulse;reaction_direction_=body_impulse?i.reaction_direction:std::min(4u,unsigned(fight.guard&&!attacking?fight.selected:fight.attack_direction));
  reaction_strength_=body_impulse?1.f:.26f;
  reaction_duration_=body_impulse?std::min(.65,asset_.clips[reactions_[reaction_direction_]].duration):.24;
  reaction_time_=0;if(body_impulse)++stats_.body_reactions;else ++stats_.clash_reactions;
 }
 reaction_weight_=0;
 if(reaction_clips_){const auto&clip=asset_.clips[reactions_[reaction_direction_]];
  if(reaction_time_<reaction_duration_){
   // Fresh hit-stop retains the exact resolved contact. Subsequent unpaused
   // frames advance before sampling, and repeated impacts begin at the prior
   // additive offset instead of snapping the character back to neutral.
   reaction_time_=std::min(reaction_duration_,reaction_time_+dt);
   const float phase=float(reaction_time_/reaction_duration_),bridge=smooth(float(reaction_time_/.055));reaction_weight_=reaction_strength_*bridge*(1-smooth((phase-.55f)/.45f));
   if(!world_animation::sample(asset_.skeleton,clip,phase*clip.duration,false,reaction_pose_,e))return false;
   for(unsigned n=0;n<result.nodes;++n)if(reaction_mask_[n]){const auto delta=qunit(qproduct(qconjugate(reaction_reference_[n]),reaction_pose_.local[n].rotation));reaction_offset_[n]=mixq(reaction_entry_[n],mixq({},delta,reaction_weight_),bridge);}
  }else reaction_offset_={};
  if(!steel_applied&&!apply_reaction(result,e))return false;
 }
 if(!full_unarmed_body_active_&&!turn_lower_body(result,i,dt,!attacking,e))return false;
 // Contact intent blends with the very same lower-body weight as the pose.
 // Disabling support throughout the partial blend released both boots when
 // movement began during a cut, even if one sole was visibly bearing weight.
 std::array<float,2> attack_contact={};const bool captured_stance=attacking&&attack_legs_weight_<.99f;
 if(captured_stance){auto support_fight=fight;
  if(steel_applied){const double phase=steel_source_phase_;support_fight.player_phase=phase<.36?TrainingPlayerPhase::Windup:phase<.60?TrainingPlayerPhase::Strike:TrainingPlayerPhase::Recovery;
   support_fight.player_duration=phase<.36?steel_timing_[0]:phase<.60?steel_timing_[1]:steel_timing_[2];
   support_fight.player_time=(phase<.36?phase/.36:phase<.60?(phase-.36)/.24:(phase-.60)/.40)*support_fight.player_duration;
  }
  if(!attack_foot_contacts(result,support_fight,attack_contact,e))return false;
 }
 const bool support_allowed=full_unarmed_body_active_?!equipment_.transitioning()&&(!stationary_turn||motion_enabled_):!stationary_turn||turn_clips_available_;
 if(!support_feet(result,i,foot_entry_boundary&&dist<.0000001?0:dt,support_allowed,e,captured_stance?attack_contact.data():nullptr))return false;
 if(story.transition&&story_have_entry_){world_animation::Pose bridged;if(!mixpose(asset_.skeleton,story_entry_,result,story.blend,bridged,e))return false;result=std::move(bridged);}
 if(story_hidden_)stats_.equipment_weapon_visible=false;
 // Capture the final corrected outgoing velocity at the exact substep that
 // completes a source take. The next bridge continues this observation; a
 // frozen endpoint would suppress the limb velocity and then catch up.
 if(motion_bridge_pending_){if(!unarmed_pose_velocity(pose_,result,have_pose_&&!i.reset?dt:0,motion_pose_yaw_,i.yaw,motion_bridge_velocity_,e))return false;motion_bridge_pose_=result;motion_entry_yaw_=i.yaw;motion_bridge_pending_=false;}
 if(have_pose_&&!i.reset&&dt>0&&full_unarmed_body_active_&&!equipment_.transitioning()){motion_previous_pose_=pose_;motion_previous_dt_=dt;motion_previous_yaw_=motion_pose_yaw_;}else motion_previous_dt_=0;
 motion_pose_yaw_=i.yaw;
 pose_=std::move(result);world_character_face::update(asset_,face_,{dt,speed_,float(std::clamp(stats_.turn_debt_radians,-1.f,1.f)),std::max(run_,attacking?1.f:0.f),0,false,false},pose_);have_pose_=true;was_attacking_=attacking;contact_token_=contact_token;reaction_serial_=i.reaction_serial;clash_token_=clash_token;
 ++stats_.updates;stats_.gait_cycles=cycles_;stats_.speed=speed_;stats_.run=run_;stats_.locomotion=locomotion_;stats_.guard=guard_weight_;stats_.reaction=reaction_weight_;stats_.guard_recoil=reaction_clash_?reaction_weight_:0;stats_.attack_legs=attack_legs_weight_;stats_.momentum_scale=attacking?attack_momentum_.scale:0;stats_.momentum_seconds=attacking?float(attack_momentum_.duration):0;return true;
}
#include "engine/world_skeletal_unarmed_motion_private.inc"

bool WorldSkeletalHero::upload(std::string&e){if(!ready()){e="Hero not prepared";return false;}world_animation_render::Instance instance{0,float(y_),0,1,yaw_};world_animation_render::Bindings bindings;const bool ok=world_animation_render::upload(gpu_,&pose_,&instance,1,bindings,e);if(ok)++stats_.uploads;else ++stats_.failures;return ok;}
bool WorldSkeletalHero::warm_extra(unsigned program,std::string&e){return !gpu_||world_animation_render::warm(gpu_,program,e);}
bool WorldSkeletalHero::draw(unsigned program,std::string&e,const std::array<float,3>&wear){if(!ready())return true;world_animation_render::Bindings bindings;bindings.blood_wear=wear;if(!equipment_weapon_visible()){bindings.hidden_primitive_begin=held_primitives_begin_;bindings.hidden_primitive_end=unsigned(asset_.mesh.primitives.size());}const bool ok=world_animation_render::draw(gpu_,program,bindings,e);if(ok)++stats_.draws;else ++stats_.failures;return ok;}
bool WorldSkeletalHero::blade_pose(const world_animation::Pose&p,double x,double y,double z,float yaw,double base[3],double tip[3])const{
 const float m[16]={-.776209056f,.616602719f,.131508440f,0,-.627860665f,-.737032235f,-.250136852f,0,-.057308983f,-.276728392f,.959237695f,0,-5.626272678f,-4.092897892f,-3.749776840f,1};
 world_animation::Mat4 socket;std::copy(m,m+16,socket.m.begin());
 for(unsigned n=0;n<2;++n){world_animation::Vec3 point{0,0,n?107.9572f:10.54f};if(asset_.skeleton.joints[hand_].name!="valwood_weapon_r")point=world_animation::transform_point(socket,point);point=world_animation::transform_point(p.global[hand_],point);
  double*out=n?tip:base;out[0]=x+std::cos(yaw)*point.x+std::sin(yaw)*point.z;out[1]=y+point.y;out[2]=z-std::sin(yaw)*point.x+std::cos(yaw)*point.z;
  for(unsigned k=0;k<3;++k)if(!std::isfinite(out[k]))return false;
 }return true;
}
bool WorldSkeletalHero::blade(double base[3],double tip[3])const{return have_pose_&&equipment_combat_ready()&&blade_pose(pose_,x_,y_,z_,yaw_,base,tip);}
bool WorldSkeletalHero::fit_guard_pose(world_animation::Pose&pose,unsigned direction,std::string&e,float*adjustment,float*residual)const{
 if(adjustment)*adjustment=0;if(residual)*residual=0;
 if(!guard_target_valid_||guard_target_direction_!=direction)return true;
 return fit_guard_target(pose,direction,guard_target_world_,x_,y_,z_,yaw_,e,adjustment,residual);
}
bool WorldSkeletalHero::fit_guard_target(world_animation::Pose&pose,unsigned direction,const std::array<double,3>&world,double x,double y,double z,float yaw,std::string&e,float*adjustment,float*residual)const{
 const double c=std::cos(yaw),s=std::sin(yaw),dx=world[0]-x,dz=world[2]-z;
 const Vec3 target{float(c*dx-s*dz),float(world[1]-y),float(s*dx+c*dz)};
 double aa[3],bb[3];if(!blade_pose(pose,0,0,0,0,aa,bb)){e="Invalid source guard edge";return false;}
 const Vec3 source_a{float(aa[0]),float(aa[1]),float(aa[2])},source_b{float(bb[0]),float(bb[1]),float(bb[2])};
 const auto source=pose;const Vec3 wrist=origin(source.global[right_hand_]),pole=origin(source.global[right_lower_]);
 const auto rotation=world_rotation(asset_.skeleton,source,right_hand_);
 Quat turn={};Vec3 a=source_a,b=source_b;
 if(direction==4){
  // A thrust is parried by turning the real held blade across the incoming
  // line, rather than sliding an unchanged parallel edge towards that line.
  // The same bounded wrist turn is used by preview, prediction and display.
  const float angle=float(25*tau/360);
  turn=qexp({0,0,-angle});
  a=vadd(wrist,qrotate(turn,vsub(source_a,wrist)));b=vadd(wrist,qrotate(turn,vsub(source_b,wrist)));
 }
 const Vec3 edge=vsub(b,a);
 const float along=std::clamp(vdot(vsub(target,a),edge)/std::max(1.e-8f,vdot(edge,edge)),.05f,.95f);
 const Vec3 point=vadd(a,vmul(edge,along));Vec3 delta=vsub(target,point);
 const float requested=vlength(delta);if(requested>.45f)delta=vmul(delta,.45f/requested);
 const Quat turned_rotation=qproduct(turn,rotation);GripSocket offhand;
 if(!grip_socket(source,offhand,e))return false;
 offhand.wrist=guard_socket_.wrist;offhand.rotation=guard_socket_.rotation;
 auto solve=[&](float weight,world_animation::Pose&candidate){candidate=source;float correction=0,error=0;std::string rejected;
  return solve_arm(candidate,right_clavicle_,right_upper_,right_lower_,right_hand_,vadd(wrist,vmul(delta,weight)),vadd(pole,vmul(delta,weight*.5f)),mixq(rotation,turned_rotation,weight),correction,error,rejected)&&
         constrain_grip_pose(candidate,offhand,correction,error,rejected);
 };
 world_animation::Pose candidate;float weight=1;
 if(!solve(1,candidate)){float lo=0,hi=1;if(!solve(0,pose)){e="Original guard source grip is outside its own arm reach";return false;}
  for(unsigned n=0;n<12;++n){const float middle=(lo+hi)*.5f;if(solve(middle,candidate)){lo=middle;pose=candidate;}else hi=middle;}weight=lo;
 }else pose=std::move(candidate);
 if(adjustment)*adjustment=vlength(delta)*weight;
 if(residual){double base[3],tip[3];if(!blade_pose(pose,0,0,0,0,base,tip))return false;
  const Vec3 final_point{float(base[0]+(tip[0]-base[0])*along),float(base[1]+(tip[1]-base[1])*along),float(base[2]+(tip[2]-base[2])*along)};
  *residual=vlength(vsub(target,final_point));
 }
 return true;
}
bool WorldSkeletalHero::guard_blade(unsigned direction,double seconds,double x,double y,double z,float yaw,double base[3],double tip[3],std::string&e)const{
 if(!equipment_combat_ready()){e="Equipment is not ready for a physical weapon action";return false;}
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)){e="Invalid guard world transform";return false;}
 world_animation::Pose result;if(!sample_guard_pose(direction,seconds,result,e))return false;
 return blade_pose(result,x,y,z,yaw,base,tip);
}
bool WorldSkeletalHero::sample_guard_pose(unsigned direction,double seconds,world_animation::Pose&result,std::string&e)const{
 if(!loaded_||!have_pose_||!guard_clips_||direction>=5||!std::isfinite(seconds)||seconds<0||seconds>.100001){e="Invalid committed guard pose request";return false;}
 if(seconds==0){result=pose_;return true;}
 world_animation::Pose desired,filtered,lower;
 if(!upper_body(pose_,guard_base_,1,lower,e))return false;
 if(!world_animation::sample(asset_.skeleton,asset_.clips[guards_[direction]],idle_time_+seconds,true,desired,e))return false;
 {world_animation::Pose layered;if(!upper_body(lower,desired,1,layered,e))return false;desired=std::move(layered);}
 if(!fit_guard_pose(desired,direction,e))return false;
 GripSocket desired_socket;if(!grip_socket(desired,desired_socket,e))return false;
 desired_socket.wrist=guard_socket_.wrist;desired_socket.rotation=guard_socket_.rotation;
 GripSocket filtered_socket=desired_socket;
 if(have_guard_pose_){const float weight=float(-std::expm1(-seconds/.055));
  if(!mixpose(asset_.skeleton,guard_pose_,desired,weight,filtered,e)||!cartesian_right(guard_pose_,desired,weight,filtered,e))return false;
  filtered_socket=mix_grips(guard_blend_socket_,desired_socket,weight);
  float correction=0,residual=0;if(!constrain_grip_pose(filtered,filtered_socket,correction,residual,e))return false;
 }else filtered=desired;
 // Retain the actual supported pelvis/feet while removing only the previous
 // guard overlay. Re-applying it to an already guarded base would raise the
 // predicted weapon twice as fast as the displayed controller.
 const float hold=guard_weight_+(1-guard_weight_)*float(-std::expm1(-seconds/.065));
 GripSocket before;if(!grip_socket(lower,before,e)||!upper_body(lower,filtered,hold,result,e))return false;
 float correction=0,residual=0;
 if(!constrain_grip_pose(result,mix_grips(before,filtered_socket,hold),correction,residual,e)||!apply_reaction(result,e))return false;
 return true;
}
bool WorldSkeletalHero::guard_shield(unsigned direction,double seconds,double x,double y,double z,float yaw,std::vector<std::array<double,9>>&triangles,std::string&e)const{
 if(!equipment_combat_ready()){e="Equipment is not ready for a physical weapon action";return false;}
 triangles.clear();
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)){e="Invalid shield world transform";return false;}
 world_animation::Pose sampled;if(!sample_guard_pose(direction,seconds,sampled,e))return false;
 return pose_guard_surfaces(sampled,x,y,z,yaw,triangles,e);
}
bool WorldSkeletalHero::pose_guard_surfaces(const world_animation::Pose&sampled,double x,double y,double z,float yaw,std::vector<std::array<double,9>>&triangles,std::string&e)const{
 triangles.clear();
 std::size_t count=0;for(unsigned offset:guard_surface_held_offsets_){const unsigned index=held_primitives_begin_+offset;if(index>=asset_.mesh.primitives.size()){e="Held guard source primitive is unavailable";return false;}count+=asset_.mesh.primitives[index].index_count/3;}
 if(count>16384){e="Actual held guard surface triangle budget rejected";return false;}
 triangles.reserve(count);const double c=std::cos(yaw),s=std::sin(yaw);
 for(unsigned offset:guard_surface_held_offsets_){const auto&primitive=asset_.mesh.primitives[held_primitives_begin_+offset];
  for(std::size_t at=primitive.first_index;at<primitive.first_index+primitive.index_count;at+=3){std::array<double,9> triangle={};
   for(unsigned corner=0;corner<3;++corner){Vec3 point;
    if(!world_attack_foot_contact::deform_position(asset_.mesh.vertices[asset_.mesh.indices[at+corner]],sampled,point,e))return false;
    point=world_animation::transform_point(sampled.global[asset_.skeleton.mesh_node],point);
    triangle[corner*3]=x+c*point.x+s*point.z;triangle[corner*3+1]=y+point.y;triangle[corner*3+2]=z-s*point.x+c*point.z;
   }
   triangles.push_back(triangle);
  }
 }
 return true;
}
bool WorldSkeletalHero::body_capsules(double x,double y,double z,float yaw,std::vector<world_combat_anatomy::Capsule>&capsules,std::string&e,unsigned direction,double seconds)const{
 capsules.clear();
 if(!loaded_||!have_pose_||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)){e="Invalid posed player body request";return false;}
 world_animation::Pose sampled;
 if(direction<5){if(!equipment_combat_ready()){e="Equipment is not ready for guard prediction";return false;}if(!sample_guard_pose(direction,seconds,sampled,e))return false;}
 else if(direction!=~0u||!std::isfinite(seconds)||seconds!=0){e="Invalid player body prediction policy";return false;}
 const auto&body=direction<5?sampled:pose_;
 return pose_body_capsules(body,x,y,z,yaw,capsules,e);
}
bool WorldSkeletalHero::pose_body_capsules(const world_animation::Pose&body,double x,double y,double z,float yaw,std::vector<world_combat_anatomy::Capsule>&capsules,std::string&e)const{
 capsules.clear();const auto&skeleton=asset_.skeleton;
 auto joint=[&](const std::string&name,Vec3&point){for(unsigned n=0;n<skeleton.joints.size();++n)if(skeleton.joints[n].name==name){point=origin(body.global[n]);return true;}e="Missing actual player anatomy bone: "+name;return false;};
 const double c=std::cos(yaw),s=std::sin(yaw);
 auto world=[&](Vec3 p){return world_combat_anatomy::Vec3{x+c*p.x+s*p.z,y+p.y,z-s*p.x+c*p.z};};
 auto add=[&](TrainingBodyPart part,Vec3 a,Vec3 b,double radius){capsules.push_back({part,world(a),world(b),radius});};
 Vec3 pelvis,chest,neck,head,shoulder_l,shoulder_r;
 if(!joint("pelvis",pelvis)||!joint("spine_03",chest)||!joint("neck_01",neck)||!joint("head",head)||!joint("upperarm_l",shoulder_l)||!joint("upperarm_r",shoulder_r))return false;
 // Flesh proportions come from this source rig's actual shoulder/limb
 // lengths. Helmet plumes, weapons and costume bounds never widen the body.
 add(TrainingBodyPart::Torso,pelvis,chest,std::clamp(double(vlength(vsub(shoulder_l,shoulder_r)))*.42,.14,.28));
 const double neck_length=vlength(vsub(head,neck));
 add(TrainingBodyPart::Head,head,vadd(head,vmul(vsub(head,neck),.45f)),std::clamp(neck_length*.85,.075,.13));
 for(unsigned side=0;side<2;++side){const std::string suffix=side?"_r":"_l";Vec3 upper,elbow,wrist,index,ring,hip,knee,ankle;
  if(!joint("upperarm"+suffix,upper)||!joint("lowerarm"+suffix,elbow)||!joint("hand"+suffix,wrist)||!joint("index_01"+suffix,index)||!joint("ring_01"+suffix,ring)||!joint("thigh"+suffix,hip)||!joint("calf"+suffix,knee)||!joint("foot"+suffix,ankle))return false;
  const auto arm=side?TrainingBodyPart::RightArm:TrainingBodyPart::LeftArm;
  const auto hand=side?TrainingBodyPart::RightHand:TrainingBodyPart::LeftHand;
  const auto leg=side?TrainingBodyPart::RightLeg:TrainingBodyPart::LeftLeg;
  add(arm,upper,elbow,std::clamp(double(vlength(vsub(elbow,upper)))*.24,.045,.095));
  add(arm,elbow,wrist,std::clamp(double(vlength(vsub(wrist,elbow)))*.21,.035,.085));
  const Vec3 knuckles=vmul(vadd(index,ring),.5f);
  add(hand,wrist,knuckles,std::clamp(double(vlength(vsub(knuckles,wrist)))*.48,.035,.065));
  add(leg,hip,knee,std::clamp(double(vlength(vsub(knee,hip)))*.23,.065,.13));
  add(leg,knee,ankle,std::clamp(double(vlength(vsub(ankle,knee)))*.19,.05,.11));
 }
 for(const auto&cap:capsules)if(!world_combat_anatomy::finite(cap.a)||!world_combat_anatomy::finite(cap.b)||!std::isfinite(cap.radius)){e="Nonfinite source player anatomy";capsules.clear();return false;}
 return true;
}
bool WorldSkeletalHero::guard_target_preview(unsigned direction,const double*target,double x,double y,double z,float yaw,double base[3],double tip[3],std::vector<world_combat_anatomy::Capsule>&capsules,std::string&e,std::vector<std::array<double,9>>*surfaces)const{
 if(!equipment_combat_ready()){e="Equipment is not ready for a physical weapon action";return false;}
 capsules.clear();if(surfaces)surfaces->clear();
 if(!loaded_||!have_pose_||!guard_clips_||direction>=5||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)){e="Invalid guard target preview";return false;}
 if(target)for(unsigned i=0;i<3;++i)if(!std::isfinite(target[i])||std::abs(target[i])>1.e7){e="Invalid guard target preview point";return false;}
 world_animation::Pose source,layered;
 if(!world_animation::sample(asset_.skeleton,asset_.clips[guards_[direction]],idle_time_,true,source,e)||!upper_body(pose_,source,1,layered,e))return false;
 if(target&&!fit_guard_target(layered,direction,{target[0],target[1],target[2]},x,y,z,yaw,e,nullptr,nullptr))return false;
 GripSocket grip;if(!grip_socket(layered,grip,e))return false;grip.wrist=guard_socket_.wrist;grip.rotation=guard_socket_.rotation;float correction=0,residual=0;
 if(!constrain_grip_pose(layered,grip,correction,residual,e)||!apply_reaction(layered,e))return false;
 return blade_pose(layered,x,y,z,yaw,base,tip)&&pose_body_capsules(layered,x,y,z,yaw,capsules,e)&&(!surfaces||pose_guard_surfaces(layered,x,y,z,yaw,*surfaces,e));
}
bool WorldSkeletalHero::contact_blade(unsigned direction,double phase,double x,double y,double z,float yaw,double base[3],double tip[3],std::string&e,double windup_seconds,std::uint64_t attack_serial,double strike_seconds,double recovery_seconds,double attack_commit_clock)const{
 if(!equipment_combat_ready()){e="Equipment is not ready for a physical weapon action";return false;}
 if(!loaded_||!have_pose_||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)||!std::isfinite(windup_seconds)||windup_seconds<0||!std::isfinite(strike_seconds)||strike_seconds<0||!std::isfinite(recovery_seconds)||recovery_seconds<0||!std::isfinite(attack_commit_clock)){e="Invalid hero contact input";return false;}
 const bool resolved=resolved_query_valid_&&attack_serial&&attack_serial==resolved_seen_;
 const world_animation::Pose*entry=resolved?&resolved_query_entry_:(!was_attacking_||(attack_serial&&attack_serial!=attack_serial_))?&unreacted_:nullptr;
 const Momentum fresh_momentum=entry?momentum_snapshot(windup_seconds>0?windup_seconds:windup_duration_,attack_commit_clock):Momentum{};
 const auto*resolved_offset=resolved?&resolved_query_offset_:nullptr;
 world_animation::Pose sampled;if(!attack_pose(direction,phase,sampled,e,windup_seconds,nullptr,nullptr,entry,strike_seconds,recovery_seconds,resolved?&resolved_query_momentum_:entry?&fresh_momentum:nullptr)||!apply_reaction(sampled,e,resolved_offset))return false;
 if(!blade_pose(sampled,x,y,z,yaw,base,tip)){e="Nonfinite hero contact blade";return false;}return true;
}
