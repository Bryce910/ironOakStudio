#include "engine/world_battlefield_actors.h"
#include "engine/world_knight_appearance.h"
#include "engine/world_character_face.h"
#include "engine/world_combat_anatomy.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <vector>

namespace {
using namespace world_animation;
constexpr double pi=3.14159265358979323846;
float ease(double t){t=std::clamp(t,0.,1.);return float(t*t*(3-2*t));}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 mul(Vec3 a,float n){return {a.x*n,a.y*n,a.z*n};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float length(Vec3 p){return std::sqrt(dot(p,p));}
Vec3 unit(Vec3 p){return mul(p,1/std::max(1.e-7f,length(p)));}
Vec3 mix(Vec3 a,Vec3 b,float t){return add(a,mul(sub(b,a),t));}
Quat qm(Quat a,Quat b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
Quat qi(Quat q){return {-q.x,-q.y,-q.z,q.w};}
Quat axis(Vec3 v,float angle){v=unit(v);const float s=std::sin(angle*.5f);return {v.x*s,v.y*s,v.z*s,std::cos(angle*.5f)};}
Quat slerp(Quat a,Quat b,float t){float d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;if(d<0){b={-b.x,-b.y,-b.z,-b.w};d=-d;}float aa=1-t,bb=t;if(d<.9995f){float r=std::acos(std::clamp(d,-1.f,1.f)),s=std::sin(r);aa=std::sin((1-t)*r)/s;bb=std::sin(t*r)/s;}Quat q{aa*a.x+bb*b.x,aa*a.y+bb*b.y,aa*a.z+bb*b.z,aa*a.w+bb*b.w};float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return {q.x/n,q.y/n,q.z/n,q.w/n};}
Quat from_to(Vec3 a,Vec3 b){a=unit(a);b=unit(b);float d=std::clamp(dot(a,b),-1.f,1.f);if(d<-.9999f)return axis(cross(a,std::abs(a.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0}),float(pi));Vec3 c=cross(a,b);Quat q{c.x,c.y,c.z,1+d};float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return {q.x/n,q.y/n,q.z/n,q.w/n};}
Quat rotation(const Skeleton&s,const Pose&p,unsigned j){Quat q=p.local[j].rotation;for(int n=s.joints[j].parent;n>=0;n=s.joints[unsigned(n)].parent)q=qm(p.local[unsigned(n)].rotation,q);return q;}
Vec3 point(const Pose&p,unsigned n){return {p.global[n].m[12],p.global[n].m[13],p.global[n].m[14]};}
bool inverse_point(const Mat4&m,Vec3 p,Vec3&out){const auto&a=m.m;p=sub(p,{a[12],a[13],a[14]});double A=a[0],B=a[4],C=a[8],D=a[1],E=a[5],F=a[9],G=a[2],H=a[6],I=a[10],det=A*(E*I-F*H)-B*(D*I-F*G)+C*(D*H-E*G);if(std::abs(det)<1.e-15)return false;out={float(((E*I-F*H)*p.x+(C*H-B*I)*p.y+(B*F-C*E)*p.z)/det),float(((F*G-D*I)*p.x+(A*I-C*G)*p.y+(C*D-A*F)*p.z)/det),float(((D*H-E*G)*p.x+(B*G-A*H)*p.y+(A*E-B*D)*p.z)/det)};return true;}
bool blend_pose(const Skeleton&s,const Pose&a,const Pose&b,float t,Pose&p,std::string&e){p=a;for(unsigned n=0;n<p.nodes;++n){p.local[n].translation=mix(a.local[n].translation,b.local[n].translation,t);p.local[n].scale=mix(a.local[n].scale,b.local[n].scale,t);p.local[n].rotation=slerp(a.local[n].rotation,b.local[n].rotation,t);}return evaluate(s,p,e);}
}

struct WorldBattlefieldActors::Impl {
 struct Body {world_character_face::State face;Asset asset;Pose pose,grounded_idle;world_animation_render::Renderer*gpu=nullptr;world_animation_render::Instance instance;Anchors anchors;unsigned body_parts=19;bool visible=false,weapon=false;std::map<std::string,unsigned> clips,joints;std::vector<unsigned> floor_samples;std::array<std::vector<unsigned>,2> soles;std::vector<std::pair<unsigned,Quat>> fingers;Vec3 offhand_socket;Quat offhand_rotation;~Body(){world_animation_render::destroy(gpu);}};
 std::array<Body,2> pair;std::unique_ptr<Body> background,archers;
 std::array<Pose,32> background_pose;std::array<world_animation_render::Instance,32> background_instance;
 std::array<Pose,4> archer_pose;std::array<world_animation_render::Instance,4> archer_instance;
 std::array<Anchors,32> background_anchors;unsigned archer_count=0;
 std::array<double,3> background_origin={};bool background_origin_ready=false;
 unsigned background_count=0;Stats stats;std::uint64_t epoch=0;bool loaded=false;
 Beat previous_beat=Beat::Hidden;bool death_from_pair=false,death_attacker_visible=false,death_attacker_weapon=false;
 Pose death_start,death_attacker_pose;world_animation_render::Instance death_instance,death_attacker_instance;
 bool tackle_entry_valid=false;Pose tackle_player_entry,tackle_attacker_entry;
 double previous_progress=0,struggle_clock=0;float death_attacker_time=1.34f;Vec3 stab_entry_hilt={0,.57f,1.79f};
 static unsigned joint(const Body&b,const char*n){const auto it=b.joints.find(n);return it==b.joints.end()?~0u:it->second;}
 static void encode_root(Pose&p,world_animation_render::Instance&instance,const std::array<double,3>&origin){
  const float c=std::cos(instance.yaw)*instance.scale,s=std::sin(instance.yaw)*instance.scale,k=instance.scale;
  Mat4 root;root.m={c,0,-s,0,0,k,0,0,s,0,c,0,float(instance.x-origin[0]),float(instance.y-origin[1]),float(instance.z-origin[2]),1};
  // Upload composes global[mesh_node] * skin. A common left transform on all
  // global joints leaves mesh-relative skin matrices unchanged. This retains
  // source skin/morph geometry while previous palette captures every root.
  for(unsigned j=0;j<p.nodes;++j){const Mat4 before=p.global[j];auto&out=p.global[j].m;out.fill(0);for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)for(unsigned n=0;n<4;++n)out[col*4+row]+=root.m[n*4+row]*before.m[col*4+n];}
  instance={};
 }
 static bool sever(Body&b,Pose&p,std::uint16_t mask,std::string&e){
  if(!mask)return true;
  constexpr const char*roots[]={"neck_01","spine_01","upperarm_l","upperarm_r","hand_l","hand_r","thigh_l","thigh_r"};
  for(unsigned part=0;part<8;++part)if(mask&(1u<<part)){
   const unsigned root=joint(b,roots[part]);if(root==~0u){e="Missing battlefield severing joint";return false;}
   // Scaling the root propagates to every descendant/twist/finger and the
   // original bound helmet/hand-held mesh. Other actors retain their own pose.
   p.local[root].scale={0,0,0};
  }
  return evaluate(b.asset.skeleton,p,e);
 }
 static bool load_body(const Asset&base,const char*path,Body&b,std::string&e){
  b.asset=base;
  if(path&&*path){Asset body;if(!load_glb(path,body,e))return false;world_knight_appearance::Config cfg;std::vector<unsigned> fp;Asset composed;cfg.first_person={0};if(!world_knight_appearance::compose(base,body,cfg,composed,fp,e))return false;b.body_parts=unsigned(body.mesh.primitives.size());b.asset=std::move(composed);}
  for(unsigned n=0;n<b.asset.skeleton.joints.size();++n)b.joints[b.asset.skeleton.joints[n].name]=n;
  for(unsigned c=0;c<b.asset.clips.size();++c)b.clips[b.asset.clips[c].name]=c;
  for(const char*n:{"pelvis","head","spine_03","hand_l","hand_r","upperarm_l","lowerarm_l","upperarm_r","lowerarm_r","foot_l","foot_r","valwood_weapon_r"})if(joint(b,n)==~0u){e=std::string("Battlefield actor missing canonical joint ")+n;return false;}
  std::vector<bool> used(b.asset.mesh.vertices.size(),false);for(unsigned p=0;p<std::min(b.body_parts,unsigned(b.asset.mesh.primitives.size()));++p){auto&pr=b.asset.mesh.primitives[p];for(std::size_t n=pr.first_index;n<pr.first_index+pr.index_count;++n)used[b.asset.mesh.indices[n]]=true;}
  Pose bind;if(!rest_pose(b.asset.skeleton,bind,e))return false;
  for(unsigned n=0;n<used.size();n+=7)if(used[n]){const auto&v=b.asset.mesh.vertices[n];const Vec3 at=transform_point(bind.global[b.asset.skeleton.mesh_node],v.position);unsigned dominant=0;for(unsigned k=1;k<4;++k)if(v.weights[k]>v.weights[dominant])dominant=k;const unsigned bone=b.asset.skeleton.palette[v.joints[dominant]];const auto&name=b.asset.skeleton.joints[bone].name;if(name=="root"||name=="valwood_weapon_r")continue;const Vec3 origin=point(bind,bone);float distance=length(sub(at,origin));for(unsigned child=0;child<b.asset.skeleton.joints.size();++child)if(b.asset.skeleton.joints[child].parent==int(bone)){Vec3 edge=sub(point(bind,child),origin);float t=std::clamp(dot(sub(at,origin),edge)/std::max(1.e-8f,dot(edge,edge)),0.f,1.f);distance=std::min(distance,length(sub(at,add(origin,mul(edge,t)))));}if(distance<.23f)b.floor_samples.push_back(n);}
  // Purchased modular bodies contain collapsed hidden slots and soft helmet
  // feathers. Neither is a rigid floor support: admitting them levitates a
  // prone soldier. Restrict support to the actual anatomical envelope.
  if(b.floor_samples.empty()){e="Battlefield body has no physical floor samples";return false;}
  for(unsigned n=0;n<b.asset.mesh.vertices.size();++n)if(used[n]){const auto&v=b.asset.mesh.vertices[n];std::array<float,2> weight={};for(unsigned k=0;k<4;++k){const auto&name=b.asset.skeleton.joints[b.asset.skeleton.palette[v.joints[k]]].name;if(name=="foot_l"||name=="ball_l")weight[0]+=v.weights[k];if(name=="foot_r"||name=="ball_r")weight[1]+=v.weights[k];}for(unsigned side=0;side<2;++side)if(weight[side]>.25f)b.soles[side].push_back(n);}if(b.soles[0].empty()||b.soles[1].empty()){e="Battlefield shoe supports unavailable";return false;}
  Pose idle;if(!world_animation::sample(b.asset.skeleton,b.asset.clips[b.clips.at("Idle")],0,false,idle,e))return false;if(!inverse_point(idle.global[joint(b,"valwood_weapon_r")],point(idle,joint(b,"hand_l")),b.offhand_socket)){e="Invalid battlefield offhand bind";return false;}b.offhand_rotation=qm(qi(rotation(b.asset.skeleton,idle,joint(b,"valwood_weapon_r"))),rotation(b.asset.skeleton,idle,joint(b,"hand_l")));for(unsigned n=0;n<b.asset.skeleton.joints.size();++n){const auto&name=b.asset.skeleton.joints[n].name;for(const char*prefix:{"thumb_","index_","middle_","ring_","pinky_"})if(name.rfind(prefix,0)==0)b.fingers.push_back({n,idle.local[n].rotation});}
  if(!standing(b,idle,e))return false;
  return true;
 }
 static bool add_bow(Body&b,std::string&e){
  Pose rest;if(!rest_pose(b.asset.skeleton,rest,e))return false;
  // Replace held steel with a curved wood bow; rigid limbs follow the bow
  // hand, the real string nock follows the drawing hand through skin weights.
  b.asset.mesh.primitives.resize(b.body_parts);
  Material wood;wood.base_factor={.16f,.062f,.023f,1};wood.roughness=.56f;wood.metallic=0;
  Material string;string.base_factor={.30f,.25f,.16f,1};string.roughness=.9f;string.metallic=0;
  const unsigned wm=unsigned(b.asset.materials.size());b.asset.materials.push_back(wood);b.asset.materials.push_back(string);
  unsigned lp=~0u,rp=~0u;for(unsigned n=0;n<b.asset.skeleton.palette.size();++n){if(b.asset.skeleton.palette[n]==joint(b,"hand_l"))lp=n;if(b.asset.skeleton.palette[n]==joint(b,"hand_r"))rp=n;}if(lp==~0u||rp==~0u){e="Bow hand palette unavailable";return false;}
  const Vec3 hand=point(rest,joint(b,"hand_l")),draw=point(rest,joint(b,"hand_r"));const auto&mesh=rest.global[b.asset.skeleton.mesh_node];
  auto vertex=[&](Vec3 p,Vec3 n,unsigned j0,unsigned j1,float w){SkinVertex v;Vec3 a,c;if(!inverse_point(mesh,p,a)||!inverse_point(mesh,add(p,n),c))return false;v.position=a;v.normal=unit(sub(c,a));v.tangent={1,0,0,1};v.joints={j0,j1,0,0};v.weights={1-w,w,0,0};v.uv={p.y,p.z};b.asset.mesh.vertices.push_back(v);return true;};
  auto tube=[&](Vec3 from,Vec3 to,float radius,unsigned material,bool bending){const unsigned begin=unsigned(b.asset.mesh.indices.size()),count=bending?12u:1u,sides=6u;Vec3 along=unit(sub(to,from)),side=unit(cross(along,std::abs(along.x)<.9f?Vec3{1,0,0}:Vec3{0,0,1})),up=unit(cross(along,side));const unsigned first=unsigned(b.asset.mesh.vertices.size());for(unsigned k=0;k<=count;++k){float t=float(k)/count;Vec3 c=mix(from,to,t);for(unsigned j=0;j<sides;++j){float a=float(2*pi*j/sides);Vec3 normal=add(mul(side,std::cos(a)),mul(up,std::sin(a)));if(!vertex(add(c,mul(normal,radius)),normal,lp,bending?rp:lp,bending?t:0))return false;}}for(unsigned k=0;k<count;++k)for(unsigned j=0;j<sides;++j){unsigned a=first+k*sides+j,c=first+k*sides+(j+1)%sides,d=c+sides,bb=a+sides;b.asset.mesh.indices.insert(b.asset.mesh.indices.end(),{a,c,d,a,d,bb});}b.asset.mesh.primitives.push_back({begin,b.asset.mesh.indices.size()-begin,int(material),true,true,false});return true;};
  const unsigned bow_index_begin=unsigned(b.asset.mesh.indices.size());Vec3 prior{};for(unsigned k=0;k<=36;++k){float y=-.67f+1.34f*k/36;float z=.11f*std::cos(float(pi*y/1.34f))-.11f-.16f*std::pow(std::abs(y)/.67f,4);Vec3 at=add(hand,{0,y,z});if(k&&!tube(prior,at,.016f*(1-.48f*std::abs(y)/.67f),wm,false))return false;prior=at;}
  b.asset.mesh.primitives.resize(b.body_parts);b.asset.mesh.primitives.push_back({bow_index_begin,b.asset.mesh.indices.size()-bow_index_begin,int(wm),true,true,false});const unsigned string_begin=unsigned(b.asset.mesh.indices.size());for(float sign:{-1.f,1.f})if(!tube(add(hand,{0,.67f*sign,-.27f}),draw,.0017f,wm+1,true))return false;b.asset.mesh.primitives.resize(b.body_parts+1);b.asset.mesh.primitives.push_back({string_begin,b.asset.mesh.indices.size()-string_begin,int(wm+1),true,true,false});
  return validate(b.asset,e);
 }
 static bool archer(Body&b,Pose&p,double phase,std::string&e){
  if(!sample(b,"Idle",phase*duration(b,"Idle"),p,e,true)||!center(b,p,0,0,e))return false;
  Vec3 left{.03f,1.39f,.69f},right{-.16f,1.46f,.16f};
  if(phase>.10&&phase<.42){float w=ease((phase-.10)/.10),lower=ease((phase-.25)/.17);right=mix(right,{-.10f,1.43f,.46f},w);left=mix(left,{.18f,1.12f,.45f},lower);right=mix(right,{-.24f,1.03f,.38f},lower);}
  else if(phase>=.42&&phase<.66){float w=ease((phase-.42)/.24);left={.18f,1.12f,.45f};right=mix({-.24f,1.03f,.38f},{-.31f,1.56f,-.17f},w);}
  else if(phase>=.66&&phase<.84){float w=ease((phase-.66)/.18);left=mix({.18f,1.12f,.45f},{.03f,1.39f,.69f},w);right=mix({-.31f,1.56f,-.17f},{-.07f,1.37f,.61f},w);}
  else if(phase>=.84)right=mix({-.07f,1.37f,.61f},right,ease((phase-.84)/.16));
  if(!arm(b,p,false,left,{.45f,1.12f,.30f},1,e)||!arm(b,p,true,right,{-.58f,1.47f,-.16f},1,e))return false;
  Pose rest;if(!rest_pose(b.asset.skeleton,rest,e))return false;
  if(!rotate_world(b,p,joint(b,"hand_l"),rotation(b.asset.skeleton,rest,joint(b,"hand_l")),e))return false;
  return floor(b,p,e);
 }
 static float sole(Body&b,const Pose&p,unsigned side){float low=10;for(unsigned n:b.soles[side]){const auto&v=b.asset.mesh.vertices[n];Vec3 q{};for(unsigned k=0;k<4;++k)q=add(q,mul(transform_point(p.skin[v.joints[k]],v.position),v.weights[k]));low=std::min(low,transform_point(p.global[b.asset.skeleton.mesh_node],q).y);}return low;}
 static bool standing(Body&b,const Pose&source,std::string&e){b.grounded_idle=source;auto&p=b.grounded_idle;float lowering=std::max({0.f,sole(b,p,0),sole(b,p,1)})+.01f;unsigned pelvis=joint(b,"pelvis");Vec3 target=point(p,pelvis),local;target.y-=lowering;int parent=b.asset.skeleton.joints[pelvis].parent;if(parent<0)local=target;else if(!inverse_point(p.global[unsigned(parent)],target,local))return false;p.local[pelvis].translation=local;if(!evaluate(b.asset.skeleton,p,e))return false;for(unsigned pass=0;pass<2;++pass)for(unsigned side=0;side<2;++side){bool right=side==1;unsigned hip=joint(b,right?"thigh_r":"thigh_l"),knee=joint(b,right?"calf_r":"calf_l"),foot=joint(b,right?"foot_r":"foot_l");Vec3 at=point(p,foot),pole=point(p,knee);at.y-=sole(b,p,side);pole.z+=.3f;if(!two_bone_ik(b.asset.skeleton,p,{hip,knee,foot,at,pole,1,0},e))return false;}return floor(b,p,e);}
 static bool sample(Body&b,const char*name,double time,Pose&p,std::string&e,bool loop=false){auto it=b.clips.find(name);if(it==b.clips.end()){e=std::string("Missing battlefield capture ")+name;return false;}return world_animation::sample(b.asset.skeleton,b.asset.clips[it->second],time,loop,p,e);}
 static double duration(const Body&b,const char*name){return b.asset.clips[b.clips.at(name)].duration;}
 static bool center(Body&b,Pose&p,float x,float z,std::string&e){unsigned j=joint(b,"pelvis");Vec3 at=point(p,j),target{x,at.y,z};const int parent=b.asset.skeleton.joints[j].parent;Vec3 local;if(parent<0)local=target;else if(!inverse_point(p.global[unsigned(parent)],target,local)){e="Singular cinematic pelvis parent";return false;}p.local[j].translation=local;return evaluate(b.asset.skeleton,p,e);}
 static bool floor(Body&b,Pose&p,std::string&e){float low=10;for(unsigned n:b.floor_samples){const auto&v=b.asset.mesh.vertices[n];Vec3 out{};for(unsigned k=0;k<4;++k)out=add(out,mul(transform_point(p.skin[v.joints[k]],v.position),v.weights[k]));out=transform_point(p.global[b.asset.skeleton.mesh_node],out);low=std::min(low,out.y);}if(!std::isfinite(low)||low>2||low< -2){e="Invalid battlefield body floor "+std::to_string(low);return false;}unsigned root=joint(b,"pelvis");Vec3 at=point(p,root),local;at.y-=low;const int parent=b.asset.skeleton.joints[root].parent;if(parent<0)local=at;else if(!inverse_point(p.global[unsigned(parent)],at,local))return false;p.local[root].translation=local;return evaluate(b.asset.skeleton,p,e);}
 static bool rotate_world(Body&b,Pose&p,unsigned n,Quat desired,std::string&e){const int parent=b.asset.skeleton.joints[n].parent;p.local[n].rotation=parent<0?desired:qm(qi(rotation(b.asset.skeleton,p,unsigned(parent))),desired);return evaluate(b.asset.skeleton,p,e);}
 static bool arm(Body&b,Pose&p,bool right,Vec3 target,Vec3 pole,float weight,std::string&e){unsigned a=joint(b,right?"upperarm_r":"upperarm_l"),c=joint(b,right?"lowerarm_r":"lowerarm_l"),h=joint(b,right?"hand_r":"hand_l");return two_bone_ik(b.asset.skeleton,p,{a,c,h,target,pole,weight,0},e);}
 static Vec3 to_local(const Body&b,Vec3 world){const float c=std::cos(b.instance.yaw),s=std::sin(b.instance.yaw);world.x-=b.instance.x;world.y-=b.instance.y;world.z-=b.instance.z;return {(c*world.x-s*world.z)/b.instance.scale,world.y/b.instance.scale,(s*world.x+c*world.z)/b.instance.scale};}
 static Vec3 to_world(const Body&b,Vec3 p){float c=std::cos(b.instance.yaw),s=std::sin(b.instance.yaw),k=b.instance.scale;return {b.instance.x+k*(c*p.x+s*p.z),b.instance.y+k*p.y,b.instance.z+k*(-s*p.x+c*p.z)};}
 static bool kneel(Body&b,Pose&p,double t,std::string&e){
  if(!sample(b,"BF_Victim",3.8,p,e)||!center(b,p,0,0,e))return false;
  const unsigned chest=joint(b,"spine_03");Quat r=rotation(b.asset.skeleton,p,chest);if(!rotate_world(b,p,chest,qm(axis({1,0,0},.42f+float(.025*std::sin(t*4.1))),r),e))return false;
  return floor(b,p,e);
 }
 static void set_instance(Body&b,const Input&i,float x,float z,float yaw=0){const double c=std::cos(i.yaw),s=std::sin(i.yaw);b.instance={float(i.x+c*x+s*z),float(i.y),float(i.z-s*x+c*z),1,i.yaw+yaw};}
 static bool skin_anchors(Body&b,std::string&e){
  auto world=[&](const char*n){Vec3 p=to_world(b,point(b.pose,joint(b,n)));return std::array<double,3>{p.x,p.y,p.z};};
  b.anchors.head=world("head");b.anchors.left_hand=world("hand_l");b.anchors.right_hand=world("hand_r");b.anchors.chest=world("spine_03");b.anchors.pelvis=world("pelvis");
  unsigned w=joint(b,"valwood_weapon_r");Vec3 p=to_world(b,transform_point(b.pose.global[w],{0,0,10.54f})),q=to_world(b,transform_point(b.pose.global[w],{0,0,107.9572f}));b.anchors.blade_base={p.x,p.y,p.z};b.anchors.blade_tip={q.x,q.y,q.z};b.anchors.visible=b.visible;b.anchors.weapon_visible=b.weapon;
  for(double v:b.anchors.head)if(!std::isfinite(v)){e="Nonfinite battlefield anchor";return false;}return true;
 }
 // Draw placement uses per-actor XZ uniforms in the host; upload XZ remains0
 // to retain precision in the world's large authored coordinates.
 static world_animation_render::Instance upload_instance(const Body&b){auto x=b.instance;x.x=x.z=0;return x;}
};
WorldBattlefieldActors::WorldBattlefieldActors():impl_(std::make_unique<Impl>()){}
WorldBattlefieldActors::~WorldBattlefieldActors()=default;
bool WorldBattlefieldActors::load_cpu(const char*rig,const char*capture,const char*player,const char*knight,std::string&e){
 auto&x=*impl_;if(x.loaded){e="Battlefield actors already loaded";return false;}Asset base,bank;if(!load_glb(rig,base,e)||!load_glb(capture,bank,e))return false;
 if(base.skeleton.joints.size()!=bank.skeleton.joints.size()){e="Battlefield capture skeleton mismatch";return false;}
 for(unsigned n=0;n<base.skeleton.joints.size();++n)if(base.skeleton.joints[n].name!=bank.skeleton.joints[n].name||base.skeleton.joints[n].parent!=bank.skeleton.joints[n].parent){e="Battlefield capture hierarchy mismatch";return false;}
 // This isolated performance needs only these canonical gameplay loops; do
 // not duplicate the full combat controller's 50-clip bank for every extra.
 const char*keep[]={"Idle","Walk_Fwd","Run","Attack_Left","Attack_Right","Guard_High","Story_UnarmedIdle"};
 std::vector<Clip> selected;for(const auto&c:base.clips)for(const char*n:keep)if(c.name==n){selected.push_back(c);break;}base.clips=std::move(selected);
 if(base.clips.size()+bank.clips.size()>clip_limit){e="Battlefield clip budget exceeded";return false;}base.clips.insert(base.clips.end(),bank.clips.begin(),bank.clips.end());
 if(!Impl::load_body(base,player,x.pair[0],e)||!Impl::load_body(base,knight,x.pair[1],e))return false;x.background=std::make_unique<Impl::Body>();if(!Impl::load_body(base,knight,*x.background,e))return false;
 x.archers=std::make_unique<Impl::Body>();if(!Impl::load_body(base,knight,*x.archers,e)||!Impl::add_bow(*x.archers,e))return false;
 x.loaded=true;return true;
}
bool WorldBattlefieldActors::prepare(unsigned color,unsigned shadow,std::uint64_t epoch,std::string&e){auto&x=*impl_;if(!x.loaded){e="Battlefield CPU data not loaded";return false;}
 for(auto*b:{&x.pair[0],&x.pair[1],x.background.get(),x.archers.get()}){if(!b->gpu)b->gpu=world_animation_render::create(b->asset,b==x.background.get()?32:b==x.archers.get()?4:1,e,128ull*1024*1024);if(!b->gpu||!world_animation_render::warm(b->gpu,color,e)||!world_animation_render::warm(b->gpu,shadow,e))return false;}x.epoch=epoch;x.stats.ready=true;return true;}
bool WorldBattlefieldActors::warm_extra(unsigned program,std::string&e){if(!program)return true;for(auto*b:{&impl_->pair[0],&impl_->pair[1],impl_->background.get(),impl_->archers.get()})if(b&&b->gpu&&!world_animation_render::warm(b->gpu,program,e))return false;return true;}

bool WorldBattlefieldActors::update_cpu(const Input&i,std::string&e){
 auto&x=*impl_;if(!x.loaded||!std::isfinite(i.progress)||!std::isfinite(i.seconds)||!std::isfinite(i.x)||!std::isfinite(i.y)||!std::isfinite(i.z)||!std::isfinite(i.yaw)||!std::isfinite(i.struggle)||!std::isfinite(i.dt)||i.dt<0||i.dt>1||unsigned(i.beat)>unsigned(Beat::Aftermath)||std::abs(i.x)>1e6||std::abs(i.y)>1e5||std::abs(i.z)>1e6){e="Invalid battlefield choreography input";return false;}
 auto&p=x.pair[0];auto&a=x.pair[1];if(i.reset){world_character_face::reset(p.face);world_character_face::reset(a.face);x.previous_beat=Beat::Hidden;x.death_from_pair=false;x.tackle_entry_valid=false;x.stab_entry_hilt={0,.57f,1.79f};}
 if(i.beat==Beat::Tackle&&x.previous_beat!=Beat::Tackle){x.tackle_entry_valid=p.visible&&a.visible&&(x.previous_beat==Beat::Ambush||x.previous_beat==Beat::Duck);if(x.tackle_entry_valid){x.tackle_player_entry=p.pose;x.tackle_attacker_entry=a.pose;}}
 if(i.beat==Beat::Stab&&x.previous_beat!=Beat::Stab){x.stab_entry_hilt={0,.57f,1.79f};if(x.previous_beat==Beat::Struggle||x.previous_beat==Beat::Disarm){const auto&hand=a.anchors.right_hand;const double dx=hand[0]-i.x,dz=hand[2]-i.z;x.stab_entry_hilt={float(std::cos(i.yaw)*dx-std::sin(i.yaw)*dz),float(hand[1]-i.y),float(std::sin(i.yaw)*dx+std::cos(i.yaw)*dz)};}}
 if(i.beat==Beat::Death&&x.previous_beat!=Beat::Death){
  x.death_from_pair=p.visible&&x.previous_beat>=Beat::Tackle&&x.previous_beat<=Beat::Recover;
  x.death_attacker_visible=a.visible;x.death_attacker_weapon=a.weapon;x.death_attacker_pose=a.pose;x.death_attacker_instance=a.instance;
  x.death_attacker_time=x.previous_beat==Beat::Duck?float(1.20+x.previous_progress*.48):float(.56+x.previous_progress*.78);
  if(x.death_from_pair){x.death_start=p.pose;x.death_instance=p.instance;}
 }else if(i.beat!=Beat::Death)x.death_from_pair=false;
 double f=std::clamp(i.progress,0.,1.);p.visible=i.beat!=Beat::Hidden;a.visible=i.beat>=Beat::Ambush&&i.beat<=Beat::Recover;p.weapon=false;a.weapon=a.visible;Impl::set_instance(p,i,0,0);Impl::set_instance(a,i,0,1.30f,float(pi));
 bool ok=true;auto sample=[&](Impl::Body&b,const char*n,double t,bool loop=false){return Impl::sample(b,n,t,b.pose,e,loop);};
 switch(i.beat){
 case Beat::Arrival:case Beat::Dazed:ok=sample(p,"BF_Rise",f*2.6)&&Impl::center(p,p.pose,0,0,e);break;
 case Beat::Rise:{ok=sample(p,"BF_Rise",2.6+f*(Impl::duration(p,"BF_Rise")-2.6))&&Impl::center(p,p.pose,0,0,e);if(ok&&f>.82){Pose idle;ok=Impl::sample(p,"Story_UnarmedIdle",i.seconds,idle,e,true)&&Impl::center(p,idle,0,0,e)&&blend_pose(p.asset.skeleton,p.pose,idle,ease((f-.82)/.18),p.pose,e);}break;}
 case Beat::Ambush:{ok=sample(p,"Story_UnarmedIdle",i.seconds,true)&&sample(a,"BF_Slash",(.56+f*.78))&&Impl::center(p,p.pose,0,0,e)&&Impl::center(a,a.pose,0,0,e);break;}
 case Beat::Duck:ok=sample(p,"BF_Ram",.08+f*.35)&&sample(a,"BF_Slash",1.20+f*.48)&&Impl::center(p,p.pose,0,0,e)&&Impl::center(a,a.pose,0,0,e);Impl::set_instance(p,i,0,float(f*.35));break;
 case Beat::Tackle:{
  // The shoulder closes the standing gap before the receiver starts falling.
  // QTE acceptance bridges the actually displayed idle/duck, rather than
  // teleporting straight into the source ram's wide-legged opening frame.
  const float lower=ease(f/.42),impact=ease((f-.44)/.56);ok=sample(p,"BF_Ram",.08+std::min(f/.65,1.)*.65)&&sample(a,"BF_Fall",impact*2.80)&&Impl::center(p,p.pose,0,0,e)&&Impl::center(a,a.pose,0,0,e);Impl::set_instance(p,i,0,1.02f*lower);Impl::set_instance(a,i,.08f*impact,1.30f+.35f*impact,float(pi-.95*impact));
  if(ok&&x.tackle_entry_valid&&f<.22){const float entry=ease(f/.22);ok=blend_pose(p.asset.skeleton,x.tackle_player_entry,p.pose,entry,p.pose,e)&&blend_pose(a.asset.skeleton,x.tackle_attacker_entry,a.pose,entry,a.pose,e);}
  // Keep the ramming shoulder on the receiver while carrying the head
  // outside the armor silhouette, rather than through the receiver's waist.
  if(ok){const float contact=ease((f-.16)/.18)*(1-ease((f-.64)/.20));const unsigned chest=Impl::joint(p,"spine_01");ok=Impl::rotate_world(p,p.pose,chest,qm(axis({0,0,1},.50f*contact),rotation(p.asset.skeleton,p.pose,chest)),e);}
  if(ok&&f>.68){Pose kneel;const float settle=ease((f-.68)/.32);ok=Impl::kneel(p,kneel,i.seconds,e)&&blend_pose(p.asset.skeleton,p.pose,kneel,settle,p.pose,e);Impl::set_instance(p,i,-.10f*settle,1.02f+.44f*settle);}
  break;}
 case Beat::Struggle:case Beat::Disarm:case Beat::Stab:case Beat::Recover:{
  ok=Impl::kneel(p,p.pose,i.seconds,e)&&sample(a,"BF_Fall",2.80+.015*std::sin(i.seconds*5),false)&&Impl::center(a,a.pose,0,0,e)&&Impl::floor(a,a.pose,e);Impl::set_instance(p,i,-.10f,1.46f);Impl::set_instance(a,i,.08f,1.65f,float(pi-.95));
  if(!ok)break;
  const double contest_clock=i.beat==Beat::Struggle?i.seconds:x.struggle_clock;const float contest=i.beat==Beat::Struggle?1.f:i.beat==Beat::Stab?1-ease(f/.18):0.f;
  if(i.beat==Beat::Struggle)x.struggle_clock=i.seconds;
  if(contest>0){
   const float pulse=float(std::sin(contest_clock*5.4)),effort=float(std::sin(contest_clock*5.4+.8));
   const unsigned pc=Impl::joint(p,"spine_03"),ac=Impl::joint(a,"spine_03");
   ok=Impl::rotate_world(p,p.pose,pc,qm(axis({0,1,0},contest*.085f*pulse),qm(axis({1,0,0},contest*.065f*effort),rotation(p.asset.skeleton,p.pose,pc))),e)&&Impl::rotate_world(a,a.pose,ac,qm(axis({0,0,1},contest*.095f*pulse),qm(axis({1,0,0},contest*.075f*effort),rotation(a.asset.skeleton,a.pose,ac))),e)&&Impl::floor(p,p.pose,e)&&Impl::floor(a,a.pose,e);
   if(!ok)break;
  }
  // The shared hilt is a single world-space constraint. Both actors' hands
  // solve to it; no hand or weapon swaps to a new independent transform.
  const float pressure=float(std::clamp(i.struggle,0.,1.));Vec3 hilt_local{.02f-.02f*pressure,.57f+.012f*float(std::sin(i.seconds*6.5)),1.80f-.01f*pressure};
  if(i.beat==Beat::Struggle)hilt_local={.02f+.035f*float(std::sin(i.seconds*5.4)),.60f+.045f*float(std::sin(i.seconds*5.4)),1.82f-.12f*pressure+.035f*float(std::sin(i.seconds*4.1))};
  // The kneeling chest is ahead of the pelvis. Retracting the grip behind
  // encounter Z1.46 pulled both arms through the back and aimed steel through
  // the jacket. Lift and withdraw in front of the chest instead; the original
  // contact waypoint and all gameplay timing remain unchanged.
  if(i.beat==Beat::Recover)hilt_local={-.20f,1.15f,1.96f};
  if(i.beat==Beat::Disarm)hilt_local=mix(hilt_local,{-.18f,.90f,1.86f},ease(f));
  if(i.beat==Beat::Stab){const float draw=ease(f/.30),strike=ease((f-.30)/.14),withdraw=ease((f-.68)/.25);hilt_local=mix(x.stab_entry_hilt,{-.18f,.90f,1.86f},ease(f/.18));hilt_local=mix(hilt_local,{-.13f,1.25f,1.86f},draw);hilt_local=mix(hilt_local,{-.13f,.98f,1.75f},strike);hilt_local=mix(hilt_local,{-.20f,1.15f,1.96f},withdraw);}
  const float c=std::cos(i.yaw),s=std::sin(i.yaw);Vec3 hilt{float(i.x+c*hilt_local.x+s*hilt_local.z),float(i.y+hilt_local.y),float(i.z-s*hilt_local.x+c*hilt_local.z)};
  Vec3 ph=Impl::to_local(p,hilt),ah=Impl::to_local(a,hilt);const float release=i.beat==Beat::Struggle?1.f:i.beat==Beat::Disarm?1-ease((f-.35)/.5):i.beat==Beat::Stab?1-ease(f/.18):0.f;
  ok=Impl::arm(p,p.pose,true,ph,add(ph,{-.4f,.10f,-.35f}),1,e)&&Impl::arm(p,p.pose,false,add(ph,{.06f,.015f,.035f}),add(ph,{.4f,.15f,-.3f}),1,e)&&Impl::arm(a,a.pose,true,ah,add(ah,{-.4f,.2f,-.1f}),release,e);
  if(!ok)break;
  if(release>.99f){
   // The two different shoulder frames can put an energetic tug just outside
   // one arm's reach. Reconcile the shared grip, rather than letting that hand
   // hover away from the rendered gauntlet as the opponent resists.
   for(unsigned pass=0;ok&&pass<2;++pass){const Vec3 shared=mix(Impl::to_world(p,point(p.pose,Impl::joint(p,"hand_r"))),Impl::to_world(a,point(a.pose,Impl::joint(a,"hand_r"))),.5f);ph=Impl::to_local(p,shared);ah=Impl::to_local(a,shared);ok=Impl::arm(p,p.pose,true,ph,add(ph,{-.4f,.10f,-.35f}),1,e)&&Impl::arm(p,p.pose,false,add(ph,{.06f,.015f,.035f}),add(ph,{.4f,.15f,-.3f}),1,e)&&Impl::arm(a,a.pose,true,ah,add(ah,{-.4f,.2f,-.1f}),1,e);}
   if(!ok)break;
  }
  // Aim the same held blade down toward the victim, retaining the hilt socket
  // through the struggle/disarm. Finger capture remains inherited and closed.
  p.weapon=i.beat==Beat::Stab||i.beat==Beat::Recover||(i.beat==Beat::Disarm&&f>=.55);a.weapon=!p.weapon;
  Impl::Body&owner=p.weapon?p:a;unsigned w=Impl::joint(owner,"valwood_weapon_r");Vec3 start=point(owner.pose,w);Vec3 desired_world_dir{float(std::sin(i.yaw)*.92),-.392f,float(std::cos(i.yaw)*.92)};Vec3 dir{std::cos(owner.instance.yaw)*desired_world_dir.x-std::sin(owner.instance.yaw)*desired_world_dir.z,desired_world_dir.y,std::sin(owner.instance.yaw)*desired_world_dir.x+std::cos(owner.instance.yaw)*desired_world_dir.z};
  if(i.beat==Beat::Stab||i.beat==Beat::Recover){const auto chest=Impl::to_local(owner,Impl::to_world(a,point(a.pose,Impl::joint(a,"spine_03"))));dir=unit(mix(dir,unit(sub(chest,point(owner.pose,Impl::joint(owner,"hand_r")))),(i.beat==Beat::Recover?1.f:ease((f-.18)/.12))));}
  // Same physical hilt on either wrist. A handedness-dependent imported
  // socket offset cannot survive this transfer: pin its grip centre to the
  // actual solved right wrist, then orient the blade about that same pivot.
  const Vec3 wrist=point(owner.pose,Impl::joint(owner,"hand_r"));const Vec3 origin=sub(wrist,mul(dir,.035f));const int parent=owner.asset.skeleton.joints[w].parent;Vec3 local_origin;if(parent<0)local_origin=origin;else if(!inverse_point(owner.pose.global[unsigned(parent)],origin,local_origin)){e="Invalid paired weapon parent";return false;}owner.pose.local[w].translation=local_origin;if(!evaluate(owner.asset.skeleton,owner.pose,e))return false;
  Quat current=rotation(owner.asset.skeleton,owner.pose,w);Vec3 z=unit({owner.pose.global[w].m[8],owner.pose.global[w].m[9],owner.pose.global[w].m[10]});ok=Impl::rotate_world(owner,owner.pose,w,qm(from_to(z,dir),current),e);
  if(i.beat==Beat::Recover){
   const Pose kneel_start=p.pose;Pose rise,idle;
   ok=ok&&Impl::sample(p,"BF_Rise",3.9+f*(Impl::duration(p,"BF_Rise")-3.9),rise,e)&&Impl::center(p,rise,0,0,e)&&((idle=p.grounded_idle),true)&&Impl::center(p,idle,0,0,e)&&blend_pose(p.asset.skeleton,p.pose,rise,ease(f),p.pose,e);
   if(ok&&f>.8)ok=blend_pose(p.asset.skeleton,p.pose,idle,ease((f-.8)/.2),p.pose,e);
   const float root_right=-.10f-.65f*ease(f);Impl::set_instance(p,i,root_right,1.46f);
   // Step beside the casualty: the right foot leads, plants, then the left
   // follows with enough toe height to clear the armor. Foot targets are in
   // the fixed encounter frame, not sliding along with the moving pelvis.
   for(unsigned side=0;ok&&side<2;++side){const bool right=side==0;const unsigned hip=Impl::joint(p,right?"thigh_r":"thigh_l"),knee=Impl::joint(p,right?"calf_r":"calf_l"),foot=Impl::joint(p,right?"foot_r":"foot_l");const float phase=std::clamp(float((f-(right?.02:.10))/(right?.53:.65)),0.f,1.f),travel=ease(phase);Vec3 from=add(point(kneel_start,foot),{-.10f,0,1.46f}),to=add(point(idle,foot),{-.75f,0,1.46f});Vec3 target=mix(from,to,travel);target.y+=(right?.07f:.11f)*std::sin(float(pi)*phase);target.x-=root_right;target.z-=1.46f;Vec3 pole=point(p.pose,knee);pole.z+=.35f;if(!two_bone_ik(p.asset.skeleton,p.pose,{hip,knee,foot,target,pole,1,0},e))ok=false;}
   p.weapon=true;a.weapon=false;
  }
  break;}
 case Beat::Death:
  if(x.death_from_pair){
   // A failed grapple must begin at the last displayed kneeling performance,
   // not at the camera's standing root or frame zero of a standing death.
   Pose fallen;ok=Impl::sample(p,"BF_DeathSide",Impl::duration(p,"BF_DeathSide"),fallen,e)&&Impl::center(p,fallen,0,0,e)&&Impl::floor(p,fallen,e)&&blend_pose(p.asset.skeleton,x.death_start,fallen,ease(std::max(0.,i.seconds)/.25),p.pose,e);
   p.instance=x.death_instance;a.visible=x.death_attacker_visible;a.pose=x.death_attacker_pose;a.instance=x.death_attacker_instance;a.weapon=x.death_attacker_weapon;
  }else {
   ok=sample(p,"BF_DeathSide",std::min(Impl::duration(p,"BF_DeathSide"),std::max(0.,i.seconds)*1.6))&&Impl::center(p,p.pose,0,0,e);
   a.visible=x.death_attacker_visible;a.weapon=x.death_attacker_weapon;a.instance=x.death_attacker_instance;
   if(a.visible){Pose settle;ok=ok&&sample(a,"BF_Slash",x.death_attacker_time+std::min(std::max(0.,i.seconds),.45)*.70)&&Impl::center(a,a.pose,0,0,e);if(ok&&i.seconds>.45)ok=Impl::sample(a,"Idle",0,settle,e)&&Impl::center(a,settle,0,0,e)&&blend_pose(a.asset.skeleton,a.pose,settle,ease((i.seconds-.45)/.4),a.pose,e);}
  }
  break;
 case Beat::Hidden:p.visible=a.visible=false;p.anchors.visible=a.anchors.visible=false;x.previous_beat=Beat::Hidden;return true;
 case Beat::Aftermath:p.visible=false;a.visible=true;a.weapon=false;ok=sample(a,"BF_Fall",2.80)&&Impl::center(a,a.pose,0,0,e);Impl::set_instance(a,i,.08f,1.65f,float(pi-.95));break;
 }
 if(ok&&i.beat==Beat::Tackle&&f>.66){const float blend=ease((f-.66)/.34);Vec3 target_local{.02f,.57f,1.8f};const float c=std::cos(i.yaw),s=std::sin(i.yaw);Vec3 target{float(i.x+c*target_local.x+s*target_local.z),float(i.y+target_local.y),float(i.z-s*target_local.x+c*target_local.z)};Vec3 ph=Impl::to_local(p,target),ah=Impl::to_local(a,target);ok=Impl::arm(p,p.pose,true,ph,add(ph,{-.4f,.1f,-.35f}),blend,e)&&Impl::arm(p,p.pose,false,add(ph,{.06f,.015f,.035f}),add(ph,{.4f,.15f,-.3f}),blend,e)&&Impl::arm(a,a.pose,true,ah,add(ah,{-.4f,.2f,-.1f}),blend,e);}
 if(ok&&i.beat>=Beat::Tackle&&i.beat<=Beat::Recover){const float grip=i.beat==Beat::Tackle?ease((f-.66)/.34):1.f;for(const auto&j:p.fingers)p.pose.local[j.first].rotation=slerp(p.pose.local[j.first].rotation,j.second,grip);for(const auto&j:a.fingers)if(a.asset.skeleton.joints[j.first].name.back()=='r')a.pose.local[j.first].rotation=slerp(a.pose.local[j.first].rotation,j.second,grip);ok=evaluate(p.asset.skeleton,p.pose,e)&&evaluate(a.asset.skeleton,a.pose,e);}
 if(!ok){++x.stats.failures;return false;}
 if(p.visible&&!Impl::floor(p,p.pose,e))return false;world_character_face::update(p.asset,p.face,{i.dt,0,0,(i.beat==Beat::Dazed||i.beat==Beat::Arrival)?.35f:.9f,0,i.beat==Beat::Death,false},p.pose);if(a.visible&&!Impl::floor(a,a.pose,e))return false;
 p.anchors.visible=p.visible;a.anchors.visible=a.visible;
 if((p.visible&&!Impl::skin_anchors(p,e))||(a.visible&&!Impl::skin_anchors(a,e)))return false;if(i.beat==Beat::Struggle){double sum=0;for(unsigned k=0;k<3;++k){const double d=p.anchors.right_hand[k]-a.anchors.right_hand[k];sum+=d*d;}x.stats.max_hand_error_m=std::max(x.stats.max_hand_error_m,float(std::sqrt(sum)));}x.previous_beat=i.beat;x.previous_progress=f;++x.stats.samples;return true;
}

bool WorldBattlefieldActors::update_background(const Background*in,unsigned count,std::string&e){auto&x=*impl_;if(!x.loaded||count>32||(!in&&count)){e="Invalid battlefield background actor count";return false;}x.background_count=x.archer_count=0;for(unsigned n=0;n<count;++n){const auto&i=in[n];if(!std::isfinite(i.x)||!std::isfinite(i.y)||!std::isfinite(i.z)||!std::isfinite(i.progress)||!std::isfinite(i.yaw)||!std::isfinite(i.scale)||i.scale<.5||i.scale>2||i.severed_parts>255){e="Invalid battlefield extra placement";return false;}
 if(!x.background_origin_ready){x.background_origin={std::floor(i.x/128.)*128.,0,std::floor(i.z/128.)*128.};x.background_origin_ready=true;}
 const bool archer=i.motion==Motion::Archer;if(archer&&x.archer_count>=4){e="Battlefield archery group exceeds four actors";return false;}auto&b=archer?*x.archers:*x.background;auto&p=archer?x.archer_pose[x.archer_count]:x.background_pose[x.background_count];const char*name="Idle";bool loop=true;switch(i.motion){case Motion::Walk:name="Walk_Fwd";break;case Motion::Run:name="Run";break;case Motion::AttackLeft:name="Attack_Left";break;case Motion::AttackRight:name="Attack_Right";break;case Motion::Guard:name="Guard_High";break;case Motion::Death:case Motion::Dead:name="BF_Fall";loop=false;break;default:break;}double t=i.motion==Motion::Dead?Impl::duration(b,name):std::clamp(i.progress,0.,1.)*Impl::duration(b,name);if(i.motion==Motion::AttackLeft)t=(39.+33.*std::clamp(i.progress,0.,1.))/30.;if(i.motion==Motion::AttackRight)t=(37.+33.*std::clamp(i.progress,0.,1.))/30.;if(archer){if(!Impl::archer(b,p,std::clamp(i.progress,0.,1.),e))return false;}else if(!Impl::sample(b,name,t,p,e,loop)||!Impl::center(b,p,0,0,e)||!Impl::floor(b,p,e))return false;
 if(!archer&&(i.motion==Motion::AttackLeft||i.motion==Motion::AttackRight||i.motion==Motion::Death)){const float blend=i.motion==Motion::Death?ease(i.progress/.08):std::min(ease(i.progress/.12),ease((1-i.progress)/.12));if(blend<1){Pose guard;if(!Impl::sample(b,"Guard_High",0,guard,e)||!Impl::center(b,guard,0,0,e)||!blend_pose(b.asset.skeleton,guard,p,blend,p,e)||!Impl::floor(b,p,e))return false;}}
 if(!archer&&i.motion!=Motion::Death&&i.motion!=Motion::Dead){Vec3 target=transform_point(p.global[Impl::joint(b,"valwood_weapon_r")],b.offhand_socket);if(!Impl::arm(b,p,false,target,add(target,{.45f,.05f,-.20f}),1,e)||!Impl::rotate_world(b,p,Impl::joint(b,"hand_l"),qm(rotation(b.asset.skeleton,p,Impl::joint(b,"valwood_weapon_r")),b.offhand_rotation),e))return false;}
 if(!Impl::sever(b,p,i.severed_parts,e))return false;
 b.pose=p;b.instance={float(i.x),float(i.y),float(i.z),i.scale,i.yaw};b.visible=true;b.weapon=!world_combat_anatomy::removed(i.severed_parts,TrainingBodyPart::RightHand);if(!Impl::skin_anchors(b,e))return false;x.background_anchors[n]=b.anchors;
 auto instance=b.instance;Impl::encode_root(p,instance,x.background_origin);if(archer)x.archer_instance[x.archer_count++]=instance;else x.background_instance[x.background_count++]=instance;
 }x.stats.background_count=count;return true;}
bool WorldBattlefieldActors::upload(const world_animation_render::Bindings&bindings,std::string&e){auto&x=*impl_;if(!x.stats.ready){e="Battlefield actors not prepared";return false;}for(auto&b:x.pair)if(b.visible){auto inst=Impl::upload_instance(b);if(!world_animation_render::upload(b.gpu,&b.pose,&inst,1,bindings,e))return false;++x.stats.uploads;}if(x.background_count){if(!world_animation_render::upload(x.background->gpu,x.background_pose.data(),x.background_instance.data(),x.background_count,bindings,e))return false;++x.stats.uploads;}if(x.archer_count){if(!world_animation_render::upload(x.archers->gpu,x.archer_pose.data(),x.archer_instance.data(),x.archer_count,bindings,e))return false;++x.stats.uploads;}return true;}
bool WorldBattlefieldActors::draw(Role role,unsigned program,const world_animation_render::Bindings&bindings,std::string&e){auto&x=*impl_;auto&b=x.pair[unsigned(role)];if(!x.stats.ready||!b.visible)return true;auto bind=bindings;if(!b.weapon){bind.hidden_primitive_begin=b.body_parts;bind.hidden_primitive_end=unsigned(b.asset.mesh.primitives.size());}bind.blood_wear={role==Role::Player?.10f:.35f,role==Role::Player?.37f:.71f,.65f};if(!world_animation_render::draw(b.gpu,program,bind,e)){++x.stats.failures;return false;}++x.stats.draws;return true;}
bool WorldBattlefieldActors::draw_background(unsigned program,const world_animation_render::Bindings&bindings,std::string&e){auto&x=*impl_;if(!x.stats.ready)return true;auto bind=bindings;bind.blood_wear={.42f,.61f,.60f};if(x.background_count&&!world_animation_render::draw(x.background->gpu,program,bind,e)){++x.stats.failures;return false;}++x.stats.draws;return true;}
bool WorldBattlefieldActors::draw_archers(unsigned program,const world_animation_render::Bindings&bindings,std::string&e){auto&x=*impl_;if(!x.stats.ready||!x.archer_count)return true;if(!world_animation_render::draw(x.archers->gpu,program,bindings,e)){++x.stats.failures;return false;}++x.stats.draws;return true;}
const world_animation::Pose&WorldBattlefieldActors::pose(Role r)const{return impl_->pair[unsigned(r)].pose;}
const world_animation::Asset&WorldBattlefieldActors::asset(Role r)const{return impl_->pair[unsigned(r)].asset;}
const world_animation_render::Instance&WorldBattlefieldActors::instance(Role r)const{return impl_->pair[unsigned(r)].instance;}
const WorldBattlefieldActors::Anchors&WorldBattlefieldActors::anchors(Role r)const{return impl_->pair[unsigned(r)].anchors;}
const WorldBattlefieldActors::Stats&WorldBattlefieldActors::stats()const{return impl_->stats;}
unsigned WorldBattlefieldActors::previous_palette(Role r,const void*&id,std::uint64_t&uploads,int*offset)const{auto*b=impl_->pair[unsigned(r)].gpu;id=b;uploads=b?world_animation_render::stats(b).uploads:0;if(offset)*offset=b?world_animation_render::previous_palette_offset(b):0;return b?world_animation_render::previous_palette(b):0;}
unsigned WorldBattlefieldActors::background_previous_palette(const void*&id,std::uint64_t&uploads,int*offset)const{auto*b=impl_->background?impl_->background->gpu:nullptr;id=b;uploads=b?world_animation_render::stats(b).uploads:0;if(offset)*offset=b?world_animation_render::previous_palette_offset(b):0;return b?world_animation_render::previous_palette(b):0;}
void WorldBattlefieldActors::release(){impl_=std::make_unique<Impl>();}

const WorldBattlefieldActors::Anchors&WorldBattlefieldActors::background_anchors(unsigned n)const{static const Anchors empty;return n<impl_->stats.background_count?impl_->background_anchors[n]:empty;}
const std::array<double,3>&WorldBattlefieldActors::background_origin()const{return impl_->background_origin;}

unsigned WorldBattlefieldActors::archer_previous_palette(const void*&id,std::uint64_t&uploads,int*offset)const{auto*b=impl_->archers?impl_->archers->gpu:nullptr;id=b;uploads=b?world_animation_render::stats(b).uploads:0;if(offset)*offset=b?world_animation_render::previous_palette_offset(b):0;return b?world_animation_render::previous_palette(b):0;}
