#include "engine/world_animation.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>

// cgltf has no static-public-API switch in this bundled version. Prefix every
// public symbol in this translation unit, leaving main.c's implementation alone.
#define cgltf_parse wa_cgltf_parse
#define cgltf_parse_file wa_cgltf_parse_file
#define cgltf_load_buffers wa_cgltf_load_buffers
#define cgltf_load_buffer_base64 wa_cgltf_load_buffer_base64
#define cgltf_decode_string wa_cgltf_decode_string
#define cgltf_decode_uri wa_cgltf_decode_uri
#define cgltf_validate wa_cgltf_validate
#define cgltf_free wa_cgltf_free
#define cgltf_node_transform_local wa_cgltf_node_transform_local
#define cgltf_node_transform_world wa_cgltf_node_transform_world
#define cgltf_buffer_view_data wa_cgltf_buffer_view_data
#define cgltf_accessor_read_float wa_cgltf_accessor_read_float
#define cgltf_accessor_read_uint wa_cgltf_accessor_read_uint
#define cgltf_accessor_read_index wa_cgltf_accessor_read_index
#define cgltf_num_components wa_cgltf_num_components
#define cgltf_component_size wa_cgltf_component_size
#define cgltf_calc_size wa_cgltf_calc_size
#define cgltf_accessor_unpack_floats wa_cgltf_accessor_unpack_floats
#define cgltf_accessor_unpack_indices wa_cgltf_accessor_unpack_indices
#define cgltf_copy_extras_json wa_cgltf_copy_extras_json
#define cgltf_mesh_index wa_cgltf_mesh_index
#define cgltf_material_index wa_cgltf_material_index
#define cgltf_accessor_index wa_cgltf_accessor_index
#define cgltf_buffer_view_index wa_cgltf_buffer_view_index
#define cgltf_buffer_index wa_cgltf_buffer_index
#define cgltf_image_index wa_cgltf_image_index
#define cgltf_texture_index wa_cgltf_texture_index
#define cgltf_sampler_index wa_cgltf_sampler_index
#define cgltf_skin_index wa_cgltf_skin_index
#define cgltf_camera_index wa_cgltf_camera_index
#define cgltf_light_index wa_cgltf_light_index
#define cgltf_node_index wa_cgltf_node_index
#define cgltf_scene_index wa_cgltf_scene_index
#define cgltf_animation_index wa_cgltf_animation_index
#define cgltf_animation_sampler_index wa_cgltf_animation_sampler_index
#define cgltf_animation_channel_index wa_cgltf_animation_channel_index
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "engine/world_model_hair.h"
#include "engine/world_model_skin.h"

namespace world_animation {
namespace {
bool finite(float f){return std::isfinite(f)&&std::abs(f)<=1.e12f;}
bool finite(Vec3 v){return finite(v.x)&&finite(v.y)&&finite(v.z);}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 mul(Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float length(Vec3 a){return std::sqrt(dot(a,a));}
float &component(Vec3 &v,unsigned index){return index==0?v.x:(index==1?v.y:v.z);}
float component(const Vec3 &v,unsigned index){return index==0?v.x:(index==1?v.y:v.z);}
Vec3 unit(Vec3 a,Vec3 fallback={1,0,0}){float n=length(a);
 return n>1.e-8f?mul(a,1/n):fallback;}
float qdot(Quat a,Quat b){return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;}
bool normalized(Quat a,Quat &out){const float n=qdot(a,a);if(!finite(a.x)||!finite(a.y)||!finite(a.z)||!finite(a.w)||!std::isfinite(n)||n<1.e-16f)return false;const float f=1/std::sqrt(n);out={a.x*f,a.y*f,a.z*f,a.w*f};
 return true;}
Quat conjugate(Quat a){return {-a.x,-a.y,-a.z,a.w};}
Quat qmul(Quat a,Quat b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
Vec3 rotate(Quat q,Vec3 v){const auto t=mul(cross({q.x,q.y,q.z},v),2);
 return add(v,add(mul(t,q.w),cross({q.x,q.y,q.z},t)));}
Quat slerp(Quat a,Quat b,float weight){float d=qdot(a,b);if(d<0){b={-b.x,-b.y,-b.z,-b.w};d=-d;}float aa=1-weight,bb=weight;if(d<.9995f){const float theta=std::acos(std::clamp(d,-1.f,1.f)),s=std::sin(theta);aa=std::sin((1-weight)*theta)/s;bb=std::sin(weight*theta)/s;}Quat out;normalized({a.x*aa+b.x*bb,a.y*aa+b.y*bb,a.z*aa+b.z*bb,a.w*aa+b.w*bb},out);
 return out;}
Quat from_to(Vec3 a,Vec3 b){a=unit(a);b=unit(b);const float d=std::clamp(dot(a,b),-1.f,1.f);if(d>1-1.e-7f)return {};if(d< -1+1.e-6f){Vec3 axis=unit(cross(a,std::abs(a.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0}));
 return {axis.x,axis.y,axis.z,0};}const Vec3 c=cross(a,b);Quat out;normalized({c.x,c.y,c.z,1+d},out);
 return out;}
bool transform_valid(const Transform &t){Quat q;
 return finite(t.translation)&&finite(t.scale)&&normalized(t.rotation,q)&&std::abs(qdot(t.rotation,t.rotation)-1)<.005f;}
Mat4 matrix(const Transform &t){const auto &q=t.rotation;const float x=q.x,y=q.y,z=q.z,w=q.w;Mat4 m;m.m={
 (1-2*y*y-2*z*z)*t.scale.x,(2*x*y+2*z*w)*t.scale.x,(2*x*z-2*y*w)*t.scale.x,0,
 (2*x*y-2*z*w)*t.scale.y,(1-2*x*x-2*z*z)*t.scale.y,(2*y*z+2*x*w)*t.scale.y,0,
 (2*x*z+2*y*w)*t.scale.z,(2*y*z-2*x*w)*t.scale.z,(1-2*x*x-2*y*y)*t.scale.z,0,
 t.translation.x,t.translation.y,t.translation.z,1};
 return m;}
Mat4 product(const Mat4 &a,const Mat4 &b){Mat4 out;out.m.fill(0);for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++)for(unsigned k=0;k<4;k++)out.m[c*4+r]+=a.m[k*4+r]*b.m[c*4+k];
 return out;}
bool matrix_valid(const Mat4 &m){return std::all_of(m.m.begin(),m.m.end(),[](float f){return finite(f);})&&std::abs(m.m[3])<1.e-4f&&std::abs(m.m[7])<1.e-4f&&std::abs(m.m[11])<1.e-4f&&std::abs(m.m[15]-1)<1.e-4f;}
bool inverse(const Mat4 &input,Mat4 &output){double a[4][8]={};for(unsigned r=0;r<4;r++){for(unsigned c=0;c<4;c++)a[r][c]=input.m[c*4+r];a[r][r+4]=1;}
 for(unsigned k=0;k<4;k++){unsigned pivot=k;for(unsigned i=k+1;i<4;i++)if(std::abs(a[i][k])>std::abs(a[pivot][k]))pivot=i;if(std::abs(a[pivot][k])<1.e-12)return false;for(unsigned c=0;c<8;c++)std::swap(a[k][c],a[pivot][c]);double f=a[k][k];for(double &v:a[k])v/=f;for(unsigned r=0;r<4;r++)if(r!=k){f=a[r][k];for(unsigned c=0;c<8;c++)a[r][c]-=f*a[k][c];}}
 for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)output.m[c*4+r]=float(a[r][c+4]);
 return matrix_valid(output);}
bool skeleton_valid(const Skeleton &s,std::string &error){
 if(s.joints.empty()||s.joints.size()>node_limit||s.palette.size()>joint_limit||s.palette.size()!=s.inverse_bind.size()||s.mesh_node>=s.joints.size()){error="Invalid skeleton/palette/node budget";
 return false;}
 std::array<unsigned char,node_limit> mark={};
 auto walk=[&](auto &&self,unsigned i)->bool{if(mark[i]==2)return true;if(mark[i]==1){error="Skeleton hierarchy cycle";
 return false;}mark[i]=1;const auto &j=s.joints[i];if(j.parent< -1||j.parent>=int(s.joints.size())||!transform_valid(j.rest)||j.name.empty()||j.name.size()>128){error="Invalid joint parent/name/rest TRS";
 return false;}if(j.parent>=0&&!self(self,unsigned(j.parent)))return false;mark[i]=2;
 return true;};
 for(unsigned i=0;i<s.joints.size();i++)if(!walk(walk,i))return false;
 std::array<bool,node_limit> palette_seen={};
 for(unsigned i=0;i<s.palette.size();i++){if(s.palette[i]>=s.joints.size()||!matrix_valid(s.inverse_bind[i])){error="Invalid skin joint/inverse bind";
 return false;}if(palette_seen[s.palette[i]]){error="Duplicate skin joint";
 return false;}palette_seen[s.palette[i]]=true;}
 return true;
}
bool channel_shape(const Channel &c,unsigned nodes){return c.node<nodes&&unsigned(c.path)<=unsigned(Path::Scale)&&unsigned(c.interpolation)<=unsigned(Interpolation::CubicSpline)&&!c.times.empty()&&c.times.size()<=key_limit&&c.values.size()==c.times.size()*(c.interpolation==Interpolation::CubicSpline?3u:1u);}
bool clip_valid(const Skeleton &s,const Clip &clip,std::string &error){
 if(clip.name.size()>128||!std::isfinite(clip.duration)||clip.duration<0||clip.duration>1.e6||clip.channels.size()>channel_limit||clip.events.size()>event_limit){error="Invalid animation clip/budget";
 return false;}
 std::array<std::array<bool,3>,node_limit> seen={};std::size_t total=0;
 for(const auto &c:clip.channels){if(!channel_shape(c,unsigned(s.joints.size()))||seen[c.node][unsigned(c.path)]){error="Invalid/duplicate animation channel";
 return false;}seen[c.node][unsigned(c.path)]=true;total+=c.times.size();if(total>262144){error="Animation key budget exceeded";
 return false;}
 for(unsigned i=0;i<c.times.size();i++)if(!finite(c.times[i])||c.times[i]<0||c.times[i]>clip.duration||(i&&c.times[i]<=c.times[i-1])){error="Animation times must be finite and strictly increasing";
 return false;}
 for(unsigned i=0;i<c.values.size();i++){for(float f:c.values[i])if(!finite(f)){error="Non-finite animation value";
 return false;}if(c.path==Path::Rotation&&(c.interpolation!=Interpolation::CubicSpline||i%3==1)){const auto &v=c.values[i];if(std::abs(qdot({v[0],v[1],v[2],v[3]},{v[0],v[1],v[2],v[3]})-1)>.005f){error="Animation rotation key is not unit quaternion";
 return false;}}}
 }
 double previous=-1;for(const auto &e:clip.events){if(e.name.empty()||e.name.size()>64||!std::isfinite(e.time)||e.time<0||e.time>clip.duration||e.time<previous){error="Invalid/unsorted animation event";
 return false;}previous=e.time;}return true;
}
bool sampled_value(const Channel &c,double time,std::array<float,4> &out){
 const auto value=[&](unsigned key){return c.values[c.interpolation==Interpolation::CubicSpline?key*3+1:key];};
 if(time<=c.times.front()){out=value(0);
 return true;}if(time>=c.times.back()){out=value(unsigned(c.times.size()-1));
 return true;}
 const auto upper=std::upper_bound(c.times.begin(),c.times.end(),time,[](double value,float key){return value<double(key);});if(upper==c.times.begin()||upper==c.times.end())return false;const unsigned b=unsigned(upper-c.times.begin()),a=b-1;const double span=double(c.times[b])-c.times[a];if(!std::isfinite(span)||span<=0)return false;const float t=float((time-c.times[a])/span);
 if(c.interpolation==Interpolation::Step){out=value(a);
 return true;}
 const auto va=value(a),vb=value(b);
 if(c.interpolation==Interpolation::Linear){if(c.path==Path::Rotation){const auto q=slerp({va[0],va[1],va[2],va[3]},{vb[0],vb[1],vb[2],vb[3]},t);out={q.x,q.y,q.z,q.w};}else for(unsigned k=0;k<4;k++)out[k]=va[k]+(vb[k]-va[k])*t;
 return true;}
 const float t2=t*t,t3=t2*t,h00=2*t3-3*t2+1,h10=t3-2*t2+t,h01=-2*t3+3*t2,h11=t3-t2;
 for(unsigned k=0;k<4;k++)out[k]=h00*va[k]+h10*float(span)*c.values[a*3+2][k]+h01*vb[k]+h11*float(span)*c.values[b*3][k];
 if(c.path==Path::Rotation){Quat q;if(!normalized({out[0],out[1],out[2],out[3]},q))return false;out={q.x,q.y,q.z,q.w};}return true;
}
int find_name(const Skeleton &s,const std::string &name){int result=-1;for(unsigned i=0;i<s.joints.size();i++)if(s.joints[i].name==name){if(result>=0)return -2;result=int(i);}return result;}
bool global_rotations(const Skeleton &s,const std::array<Transform,node_limit> &local,std::array<Quat,node_limit> &out){std::array<bool,node_limit> done={};auto walk=[&](auto &&self,unsigned i)->void{if(done[i])return;const auto p=s.joints[i].parent;if(p>=0){self(self,unsigned(p));out[i]=qmul(out[unsigned(p)],local[i].rotation);}else out[i]=local[i].rotation;normalized(out[i],out[i]);done[i]=true;};for(unsigned i=0;i<s.joints.size();i++)walk(walk,i);
 return true;}
bool decompose(const Mat4 &m,Transform &t){
 if(!matrix_valid(m))return false;
 t.translation={m.m[12],m.m[13],m.m[14]};Vec3 x{m.m[0],m.m[1],m.m[2]},y{m.m[4],m.m[5],m.m[6]},z{m.m[8],m.m[9],m.m[10]};t.scale={length(x),length(y),length(z)};if(t.scale.x<1.e-8||t.scale.y<1.e-8||t.scale.z<1.e-8)return false;
 if(dot(cross(x,y),z)<0)t.scale.x=-t.scale.x;
 x=mul(x,1/t.scale.x);y=mul(y,1/t.scale.y);z=mul(z,1/t.scale.z);if(std::abs(dot(x,y))>1.e-4||std::abs(dot(x,z))>1.e-4||std::abs(dot(y,z))>1.e-4)return false;
 const float trace=x.x+y.y+z.z;Quat q;
 if(trace>0){float f=std::sqrt(trace+1)*2;q={(y.z-z.y)/f,(z.x-x.z)/f,(x.y-y.x)/f,.25f*f};}
 else if(x.x>y.y&&x.x>z.z){float f=std::sqrt(1+x.x-y.y-z.z)*2;q={.25f*f,(y.x+x.y)/f,(z.x+x.z)/f,(y.z-z.y)/f};}
 else if(y.y>z.z){float f=std::sqrt(1+y.y-x.x-z.z)*2;q={(y.x+x.y)/f,.25f*f,(z.y+y.z)/f,(z.x-x.z)/f};}
 else{float f=std::sqrt(1+z.z-x.x-y.y)*2;q={(z.x+x.z)/f,(z.y+y.z)/f,.25f*f,(x.y-y.x)/f};}
 return normalized(q,t.rotation)&&transform_valid(t);
}
}

Vec3 transform_point(const Mat4 &m,Vec3 v){return {m.m[0]*v.x+m.m[4]*v.y+m.m[8]*v.z+m.m[12],m.m[1]*v.x+m.m[5]*v.y+m.m[9]*v.z+m.m[13],m.m[2]*v.x+m.m[6]*v.y+m.m[10]*v.z+m.m[14]};}
bool evaluate(const Skeleton &s,Pose &pose,std::string &error){
 error.clear();if(!skeleton_valid(s,error)||pose.nodes!=s.joints.size()){if(error.empty())error="Pose/skeleton node count differs";
 return false;}Pose next=pose;std::array<bool,node_limit> done={};
 auto walk=[&](auto &&self,unsigned i)->bool{if(done[i])return true;if(!transform_valid(next.local[i])){error="Invalid sampled TRS";
 return false;}normalized(next.local[i].rotation,next.local[i].rotation);Mat4 m=matrix(next.local[i]);const auto p=s.joints[i].parent;if(p>=0){if(!self(self,unsigned(p)))return false;m=product(next.global[unsigned(p)],m);}if(!matrix_valid(m)){error="Pose matrix overflow";
 return false;}next.global[i]=m;done[i]=true;
 return true;};
 for(unsigned i=0;i<s.joints.size();i++)if(!walk(walk,i))return false;
 Mat4 mesh_inverse;if(!inverse(next.global[s.mesh_node],mesh_inverse)){error="Singular animated mesh transform";
 return false;}
 next.joints=unsigned(s.palette.size());for(unsigned i=0;i<s.palette.size();i++){next.skin[i]=product(product(mesh_inverse,next.global[s.palette[i]]),s.inverse_bind[i]);if(!matrix_valid(next.skin[i])){error="Skin matrix overflow";
 return false;}}
 pose=next;
 return true;
}
bool rest_pose(const Skeleton &s,Pose &pose,std::string &error){Pose next;if(s.joints.size()>node_limit){error="Skeleton node budget exceeded";
 return false;}next.nodes=unsigned(s.joints.size());for(unsigned i=0;i<s.joints.size();i++)next.local[i]=s.joints[i].rest;if(!evaluate(s,next,error))return false;
 pose=next;
 return true;}
bool sample(const Skeleton &s,const Clip &clip,double seconds,bool loop,Pose &pose,std::string &error){
 error.clear();if(!std::isfinite(seconds)||seconds<0||seconds>1.e12||!std::isfinite(clip.duration)||clip.duration<0||clip.duration>1.e6||clip.channels.size()>channel_limit||s.joints.size()>node_limit){error="Invalid animation sample time/budget";
 return false;}
 double time=loop&&clip.duration>0?std::fmod(seconds,clip.duration):std::min(seconds,clip.duration);Pose next;next.nodes=unsigned(s.joints.size());for(unsigned i=0;i<s.joints.size();i++)next.local[i]=s.joints[i].rest;
 for(const auto &c:clip.channels){if(!channel_shape(c,next.nodes)||!finite(c.times.front())||!finite(c.times.back())||c.times.front()>c.times.back()){error="Malformed animation channel";
 return false;}std::array<float,4> v={};if(!sampled_value(c,time,v)){error="Invalid animation interpolation";
 return false;}auto &t=next.local[c.node];if(c.path==Path::Translation)t.translation={v[0],v[1],v[2]};else if(c.path==Path::Scale)t.scale={v[0],v[1],v[2]};else if(!normalized({v[0],v[1],v[2],v[3]},t.rotation)){error="Invalid sampled quaternion";
 return false;}}
 if(!evaluate(s,next,error))return false;
 pose=next;
 return true;
}
bool sample_batch(const Asset &asset,unsigned clip,const double *times,std::size_t count,bool loop,Pose *poses,std::string &error){
 error.clear();if(count>batch_limit||clip>=asset.clips.size()||(count&&(!times||!poses))){error="Invalid animation batch";
 return false;}for(std::size_t i=0;i<count;i++)if(!std::isfinite(times[i])||times[i]<0||times[i]>1.e12){error="Invalid animation batch time";
 return false;}for(std::size_t i=0;i<count;i++)if(!sample(asset.skeleton,asset.clips[clip],times[i],loop,poses[i],error))return false;
 return true;
}
bool validate(const Asset &asset,std::string &error){
 error.clear();
 if(!skeleton_valid(asset.skeleton,error)||asset.clips.size()>clip_limit){if(error.empty())error="Animation clip budget exceeded";return false;}
 std::size_t keys=0;
 for(const auto &c:asset.clips){if(!clip_valid(asset.skeleton,c,error))return false;for(const auto &channel:c.channels){keys+=channel.times.size();if(keys>262144){error="Total asset animation key budget exceeded";return false;}}}
 if(asset.mesh.vertices.size()>200000||asset.mesh.indices.size()>600000||asset.mesh.primitives.size()>128||asset.images.size()>128||asset.materials.size()>128){error="Skinned mesh/image/material budget exceeded";return false;}
 std::size_t encoded_bytes=0;
 for(const auto &image:asset.images){if(image.encoded.size()<4||image.encoded.size()>32*1024*1024-encoded_bytes){error="Embedded skin image budget exceeded";return false;}encoded_bytes+=image.encoded.size();}
 for(const auto &m:asset.materials){
  if(m.hair_material>4){error="Invalid owned hair material contract";return false;}
  for(const auto *map:{&m.base,&m.normal,&m.metallic_roughness,&m.occlusion,&m.emissive}){
   if(map->image< -1||map->image>=int(asset.images.size())||(map->min_filter!=9728&&map->min_filter!=9729&&(map->min_filter<9984||map->min_filter>9987))||(map->mag_filter!=9728&&map->mag_filter!=9729)||(map->wrap_s!=33071&&map->wrap_s!=33648&&map->wrap_s!=10497)||(map->wrap_t!=33071&&map->wrap_t!=33648&&map->wrap_t!=10497)){error="Invalid skinned image/sampler";return false;}
  }
  for(float f:m.base_factor)if(!finite(f)||f<0||f>1){error="Invalid skinned base factor";return false;}
  for(float f:m.emissive_factor)if(!finite(f)||f<0||f>1000){error="Invalid skinned emissive factor";return false;}
  if(!finite(m.roughness)||m.roughness<0||m.roughness>1||!finite(m.metallic)||m.metallic<0||m.metallic>1||!finite(m.normal_scale)||m.normal_scale<0||m.normal_scale>16||!finite(m.occlusion_strength)||m.occlusion_strength<0||m.occlusion_strength>1||!finite(m.alpha_cutoff)||m.alpha_cutoff<0||m.alpha_cutoff>1){error="Invalid skinned PBR factors";return false;}
 }
 for(const auto &v:asset.mesh.vertices){
  float sum=0;
  if(!finite(v.position)||!finite(v.normal)||!finite(v.uv[0])||!finite(v.uv[1])||!finite(v.tangent[0])||!finite(v.tangent[1])||!finite(v.tangent[2])||!finite(v.tangent[3])||std::abs(std::abs(v.tangent[3])-1)>.001f){error="Invalid skinned vertex/basis";return false;}
  for(unsigned k=0;k<4;k++){if(v.joints[k]>=asset.skeleton.palette.size()||!std::isfinite(v.weights[k])||v.weights[k]<0||v.weights[k]>1){error="Invalid skin weights/joint palette index";return false;}sum+=v.weights[k];}
  if(std::abs(sum-1)>.002f){error="Skin weights must sum to one";return false;}
 }
 if(asset.morph_targets.size()>morph_limit){error="Facial morph target budget exceeded";return false;}
 std::vector<unsigned char> morphed(asset.mesh.vertices.size());std::size_t morph_entries=0;
 for(unsigned target=0;target<asset.morph_targets.size();++target){const auto &m=asset.morph_targets[target];
  if(m.name.empty()||m.name.size()>64||std::any_of(asset.morph_targets.begin(),asset.morph_targets.begin()+target,[&](const MorphTarget &other){return other.name==m.name;})){error="Invalid/duplicate morph target name";return false;}
  unsigned previous=0;bool first=true;
  for(const auto &d:m.deltas){if(d.vertex>=asset.mesh.vertices.size()||(!first&&d.vertex<=previous)||!finite(d.position)||!finite(d.normal)||!finite(d.tangent)||length(d.position)>.15f||length(d.normal)>2.01f||length(d.tangent)>2.01f){error="Invalid authored morph delta/order/bounds";return false;}first=false;previous=d.vertex;morphed[d.vertex]=1;}
  morph_entries+=m.deltas.size();if(morph_entries>std::size_t(morph_limit)*morph_vertex_limit){error="Morph sparse entry budget exceeded";return false;}
 }
 if(std::count(morphed.begin(),morphed.end(),1)>morph_vertex_limit){error="Morph vertex budget exceeded";return false;}
 for(auto i:asset.mesh.indices)if(i>=asset.mesh.vertices.size()){error="Skin index outside vertex range";return false;}
 for(const auto &p:asset.mesh.primitives)if(p.first_index>asset.mesh.indices.size()||p.index_count>asset.mesh.indices.size()-p.first_index||p.index_count%3||p.material< -1||p.material>=int(asset.materials.size())){error="Invalid skin primitive range/material";return false;}
 return true;
}
bool blend(const Asset &asset,const Blend *inputs,unsigned count,bool loop,Pose &pose,std::string &error){
 error.clear();if(!count||count>16||!inputs){error="Invalid blend input budget";
 return false;}double sum=0;for(unsigned i=0;i<count;i++){if(inputs[i].clip>=asset.clips.size()||!std::isfinite(inputs[i].weight)||inputs[i].weight<0){error="Invalid blend clip/weight";
 return false;}sum+=inputs[i].weight;}if(sum<1.e-8){error="Zero blend weight";
 return false;}
 Pose out,reference;out.nodes=unsigned(asset.skeleton.joints.size());bool have=false;for(unsigned n=0;n<count;n++){if(inputs[n].weight==0)continue;Pose sampled;if(!sample(asset.skeleton,asset.clips[inputs[n].clip],inputs[n].seconds,loop,sampled,error))return false;if(!have){reference=sampled;for(unsigned j=0;j<out.nodes;j++){out.local[j].translation={};out.local[j].scale={};out.local[j].rotation={0,0,0,0};}have=true;}
 const float w=float(inputs[n].weight/sum);for(unsigned j=0;j<out.nodes;j++){auto &o=out.local[j];const auto &s=sampled.local[j];o.translation=add(o.translation,mul(s.translation,w));o.scale=add(o.scale,mul(s.scale,w));const float qw=qdot(reference.local[j].rotation,s.rotation)<0?-w:w;o.rotation={o.rotation.x+s.rotation.x*qw,o.rotation.y+s.rotation.y*qw,o.rotation.z+s.rotation.z*qw,o.rotation.w+s.rotation.w*qw};}}
 for(unsigned j=0;j<out.nodes;j++)if(!normalized(out.local[j].rotation,out.local[j].rotation)){error="Degenerate quaternion blend";
 return false;}
 if(!evaluate(asset.skeleton,out,error))return false;
 pose=out;
 return true;
}
namespace {
bool points_valid(const std::vector<BlendPoint> &p,std::string &error){if(p.empty()||p.size()>16){error="Blend space requires1..16 points";
 return false;}for(const auto &v:p)if(!finite(v.x)||!finite(v.y)){error="Invalid blend space coordinate";
 return false;}return true;}
bool mix_pose(const Skeleton &s,const Pose &a,const Pose &b,float w,Pose &out,std::string &error){Pose p=a;for(unsigned i=0;i<s.joints.size();i++){p.local[i].translation=add(mul(a.local[i].translation,1-w),mul(b.local[i].translation,w));p.local[i].scale=add(mul(a.local[i].scale,1-w),mul(b.local[i].scale,w));p.local[i].rotation=slerp(a.local[i].rotation,b.local[i].rotation,w);}if(!evaluate(s,p,error))return false;out=p;
 return true;}
}
bool weights_1d(const std::vector<BlendPoint> &points,float x,std::array<float,16> &weights,std::string &error){
 error.clear();if(!points_valid(points,error)||!finite(x))return false;for(unsigned i=1;i<points.size();i++)if(points[i].x<=points[i-1].x){error="1D blend points must be strictly sorted";
 return false;}
 std::array<float,16> result={};if(x<=points.front().x)result[0]=1;else if(x>=points.back().x)result[points.size()-1]=1;else for(unsigned i=1;i<points.size();i++)if(x<=points[i].x){const float w=(x-points[i-1].x)/(points[i].x-points[i-1].x);result[i-1]=1-w;result[i]=w;break;}weights=result;
 return true;
}
bool weights_2d(const std::vector<BlendPoint> &p,const std::vector<Triangle> &triangles,float x,float y,std::array<float,16> &weights,std::string &error){
 error.clear();if(!points_valid(p,error)||!finite(x)||!finite(y)||triangles.empty()||triangles.size()>32){if(error.empty())error="Invalid triangulated blend space";
 return false;}
 for(unsigned i=0;i<p.size();i++)for(unsigned j=0;j<i;j++)if(std::hypot(p[i].x-p[j].x,p[i].y-p[j].y)<1.e-6){error="Duplicate blend points";
 return false;}
 std::array<float,16> best={};double distance=std::numeric_limits<double>::infinity();
 for(const auto &triangle:triangles){const auto &q=triangle.points;for(auto i:q)if(i>=p.size()){error="Blend triangle index out of range";
 return false;}const auto &a=p[q[0]],&b=p[q[1]],&c=p[q[2]];
 const double area=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);if(std::abs(area)<1.e-10){error="Degenerate blend triangle";
 return false;}
 const double wa=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/area,wb=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/area,wc=1-wa-wb;
 if(wa>=-1.e-7&&wb>=-1.e-7&&wc>=-1.e-7){if(distance>0){best.fill(0);best[q[0]]=float(std::max(0.,wa));best[q[1]]=float(std::max(0.,wb));best[q[2]]=float(std::max(0.,wc));float sum=best[q[0]]+best[q[1]]+best[q[2]];for(float &w:best)w/=sum;distance=0;}continue;}
 for(unsigned edge=0;edge<3;edge++){const auto i=q[edge],j=q[(edge+1)%3];const double dx=p[j].x-p[i].x,dy=p[j].y-p[i].y,t=std::clamp(((x-p[i].x)*dx+(y-p[i].y)*dy)/(dx*dx+dy*dy),0.,1.);const double ex=p[i].x+t*dx-x,ey=p[i].y+t*dy-y,d=ex*ex+ey*ey;if(d<distance){distance=d;best.fill(0);best[i]=float(1-t);best[j]=float(t);}}
 }
 weights=best;
 return true;
}
bool events_between(const Clip &clip,double previous,double current,bool loop,EventBuffer &output,std::string &error){
 error.clear();if(!std::isfinite(previous)||!std::isfinite(current)||previous<0||current<previous||current>1.e12||clip.events.size()>event_limit||!std::isfinite(clip.duration)||clip.duration<0){error="Invalid event interval";
 return false;}EventBuffer result;
 std::array<long double,event_limit> next={};std::array<std::uint64_t,event_limit> counts={};std::uint64_t total=0;
 for(unsigned i=0;i<clip.events.size();i++){const double t=clip.events[i].time;if(!std::isfinite(t)||t<0||t>clip.duration){error="Invalid event time";
 return false;}if(loop&&clip.duration>0){if(clip.duration<1.e-6){error="Loop events require duration >=1 microsecond";
 return false;}const long double d=clip.duration,first=std::max(0.L,std::floor((previous-t)/d)+1),last=std::floor((current-t)/d);if(last>=first){counts[i]=std::uint64_t(last-first+1);next[i]=t+first*d;}}
 else if(t>previous&&t<=current){counts[i]=1;next[i]=t;}
 total=counts[i]>std::numeric_limits<std::uint64_t>::max()-total?std::numeric_limits<std::uint64_t>::max():total+counts[i];}
 while(result.count<event_limit){int selected=-1;for(unsigned i=0;i<clip.events.size();i++)if(counts[i]&&(selected<0||next[i]<next[unsigned(selected)]))selected=int(i);if(selected<0)break;const unsigned i=unsigned(selected);result.hits[result.count++]={i,double(next[i])};counts[i]--;next[i]+=clip.duration;}
 result.dropped=total-result.count;output=result;
 return true;
}
bool transition(const Asset &asset,Playback &playback,unsigned target,double duration,std::string &error,bool restart){
 error.clear();if(target>=asset.clips.size()||playback.clip>=asset.clips.size()||!std::isfinite(duration)||duration<0||duration>60){error="Invalid animation transition";
 return false;}Playback next=playback;
 if(playback.fading){Pose a,b,current;if(playback.source_frozen){a.nodes=unsigned(asset.skeleton.joints.size());a.local=playback.frozen;if(!evaluate(asset.skeleton,a,error))return false;}else if(playback.from_clip>=asset.clips.size()||!sample(asset.skeleton,asset.clips[playback.from_clip],playback.from_time,playback.loop,a,error))return false;
 if(!sample(asset.skeleton,asset.clips[playback.clip],playback.time,playback.loop,b,error)||!mix_pose(asset.skeleton,a,b,float(std::clamp(playback.fade_elapsed/playback.fade_duration,0.,1.)),current,error))return false;
 next.frozen=current.local;next.source_frozen=true;
 }else{next.from_clip=playback.clip;next.from_time=playback.time;next.source_frozen=false;}
 next.clip=target;if(restart)next.time=0;next.fade_duration=duration;next.fade_elapsed=0;next.fading=duration>0;playback=std::move(next);
 return true;
}
bool advance(const Asset &asset,Playback &playback,double dt,Pose &pose,EventBuffer &events,std::string &error){
 error.clear();if(!std::isfinite(dt)||dt<0||playback.clip>=asset.clips.size()||playback.time+dt>1.e12||!std::isfinite(playback.time)||playback.time<0){error="Invalid animation playback interval";
 return false;}Playback next=playback;next.time+=dt;Pose target;EventBuffer hits;
 if(!sample(asset.skeleton,asset.clips[next.clip],next.time,next.loop,target,error)||!events_between(asset.clips[next.clip],playback.time,next.time,next.loop,hits,error))return false;
 if(next.fading){if(!std::isfinite(next.fade_duration)||next.fade_duration<=0||!std::isfinite(next.fade_elapsed)||next.fade_elapsed<0){error="Invalid crossfade state";
 return false;}next.fade_elapsed=std::min(next.fade_duration,next.fade_elapsed+dt);Pose source;
 if(next.source_frozen){source.nodes=unsigned(asset.skeleton.joints.size());source.local=next.frozen;if(!evaluate(asset.skeleton,source,error))return false;}
 else{next.from_time+=dt;if(next.from_clip>=asset.clips.size()||!sample(asset.skeleton,asset.clips[next.from_clip],next.from_time,next.loop,source,error))return false;}
 Pose mixed;if(!mix_pose(asset.skeleton,source,target,float(next.fade_elapsed/next.fade_duration),mixed,error))return false;target=mixed;if(next.fade_elapsed>=next.fade_duration)next.fading=next.source_frozen=false;}
 playback=std::move(next);pose=target;events=hits;
 return true;
}
namespace {
Rigid rigid_product(Rigid a,Rigid b){Quat q=qmul(a.rotation,b.rotation);normalized(q,q);
 return {add(a.translation,rotate(a.rotation,b.translation)),q};}
Rigid rigid_inverse(Rigid a){const auto q=conjugate(a.rotation);
 return {rotate(q,mul(a.translation,-1)),q};}
bool rigid_pose(const Skeleton &s,const Clip &c,unsigned node,double time,Rigid &out,std::string &error){Pose p;if(!sample(s,c,time,false,p,error))return false;std::array<Quat,node_limit> rotations;global_rotations(s,p.local,rotations);out={transform_point(p.global[node],{}),rotations[node]};
 return true;}
}
bool root_motion(const Skeleton &s,const Clip &clip,unsigned node,double previous,double current,bool loop,Rigid &out,std::string &error){
 error.clear();if(node>=s.joints.size()||!std::isfinite(previous)||!std::isfinite(current)||previous<0||current<previous||current>1.e12){error="Invalid root-motion interval";
 return false;}Rigid a,b;if(!loop||clip.duration==0){if(!rigid_pose(s,clip,node,previous,a,error)||!rigid_pose(s,clip,node,current,b,error))return false;out=rigid_product(rigid_inverse(a),b);
 return true;}
 if(!std::isfinite(clip.duration)||clip.duration<1.e-6){error="Invalid root-motion loop duration";
 return false;}const auto ca=std::uint64_t(std::floor(previous/clip.duration)),cb=std::uint64_t(std::floor(current/clip.duration));Rigid start,end;
 if(!rigid_pose(s,clip,node,0,start,error)||!rigid_pose(s,clip,node,clip.duration,end,error)||!rigid_pose(s,clip,node,std::fmod(previous,clip.duration),a,error)||!rigid_pose(s,clip,node,std::fmod(current,clip.duration),b,error))return false;
 Rigid cycle=rigid_product(rigid_inverse(start),end),power;std::uint64_t n=cb-ca;while(n){if(n&1)power=rigid_product(power,cycle);n>>=1;if(n)cycle=rigid_product(cycle,cycle);if(!finite(power.translation)||!finite(cycle.translation)){error="Root-motion accumulation overflow";
 return false;}}
 const auto relative_a=rigid_product(rigid_inverse(start),a),relative_b=rigid_product(rigid_inverse(start),b);Rigid result=rigid_product(rigid_product(rigid_inverse(relative_a),power),relative_b);if(!finite(result.translation)||!normalized(result.rotation,result.rotation)){error="Invalid accumulated root motion";
 return false;}out=result;
 return true;
}
bool strip_root_motion(const Skeleton &s,unsigned node,Pose &pose,std::string &error,bool keep_vertical){if(node>=s.joints.size()||pose.nodes!=s.joints.size()){error="Invalid root-motion node";
 return false;}Pose next=pose;const float y=next.local[node].translation.y;next.local[node].translation=s.joints[node].rest.translation;if(keep_vertical)next.local[node].translation.y=y;next.local[node].rotation=s.joints[node].rest.rotation;if(!evaluate(s,next,error))return false;
 pose=next;
 return true;}
bool socket_transform(const Skeleton &s,const Pose &pose,const Socket &socket,Mat4 &out,std::string &error){error.clear();const int joint=find_name(s,socket.joint);if(joint<0||pose.nodes!=s.joints.size()||!transform_valid(socket.local)){error="Missing/ambiguous socket joint or invalid transform";
 return false;}const auto result=product(pose.global[unsigned(joint)],matrix(socket.local));if(!matrix_valid(result)){error="Socket transform overflow";
 return false;}out=result;
 return true;}
bool retarget_map(const Skeleton &source,const Skeleton &target,RetargetMap &out,std::string &error,float scale){
 error.clear();if(!skeleton_valid(source,error)||!skeleton_valid(target,error)||!std::isfinite(scale)||scale<=0||scale>100){if(error.empty())error="Invalid retarget scale";
 return false;}RetargetMap map;map.target_nodes=unsigned(target.joints.size());map.translation_scale=scale;
 for(unsigned i=0;i<target.joints.size();i++){const auto from=find_name(source,target.joints[i].name);if(from== -2||find_name(target,target.joints[i].name)== -2){error="Ambiguous retarget joint name";
 return false;}map.source_for_target[i]=from;}
 for(auto i:target.palette)if(map.source_for_target[i]<0){error="Required target skin joint has no source name";
 return false;}
 for(unsigned i=0;i<target.joints.size();i++)if(map.source_for_target[i]>=0){int parent=target.joints[i].parent;while(parent>=0&&map.source_for_target[unsigned(parent)]<0)parent=target.joints[unsigned(parent)].parent;if(parent>=0){const int expected=map.source_for_target[unsigned(parent)];int ancestor=source.joints[unsigned(map.source_for_target[i])].parent;while(ancestor>=0&&ancestor!=expected)ancestor=source.joints[unsigned(ancestor)].parent;if(ancestor!=expected){error="Retarget hierarchy mismatch";
 return false;}}}
 out=map;
 return true;
}
bool retarget(const Skeleton &source,const Pose &input,const Skeleton &target,const RetargetMap &map,Pose &output,std::string &error){
 error.clear();if(!skeleton_valid(source,error)||!skeleton_valid(target,error)||input.nodes!=source.joints.size()||map.target_nodes!=target.joints.size()||!std::isfinite(map.translation_scale)||map.translation_scale<=0||map.translation_scale>100){if(error.empty())error="Invalid retarget input/map";
 return false;}
 for(unsigned i=0;i<target.joints.size();i++)if(map.source_for_target[i]< -1||map.source_for_target[i]>=int(source.joints.size())){error="Retarget map index out of range";
 return false;}
 for(auto joint:target.palette)if(map.source_for_target[joint]<0){error="Required retarget joint is unmapped";
 return false;}
 for(unsigned i=0;i<source.joints.size();i++)if(!transform_valid(input.local[i])){error="Invalid retarget source pose";
 return false;}
 for(unsigned i=0;i<target.joints.size();i++)if(map.source_for_target[i]>=0){
  for(unsigned j=0;j<i;j++)if(map.source_for_target[j]==map.source_for_target[i]){error="Duplicate retarget source joint";
 return false;}
  int parent=target.joints[i].parent;while(parent>=0&&map.source_for_target[unsigned(parent)]<0)parent=target.joints[unsigned(parent)].parent;
  if(parent>=0){const int expected=map.source_for_target[unsigned(parent)];int ancestor=source.joints[unsigned(map.source_for_target[i])].parent;while(ancestor>=0&&ancestor!=expected)ancestor=source.joints[unsigned(ancestor)].parent;if(ancestor!=expected){error="Retarget hierarchy mismatch";
 return false;}}
 }
 Pose source_rest,target_rest;if(!rest_pose(source,source_rest,error)||!rest_pose(target,target_rest,error))return false;std::array<Quat,node_limit> sr,tr,current,result;global_rotations(source,source_rest.local,sr);global_rotations(target,target_rest.local,tr);global_rotations(source,input.local,current);Pose next=target_rest;std::array<bool,node_limit> done={};
 auto walk=[&](auto &&self,unsigned i)->bool{if(done[i])return true;const int parent=target.joints[i].parent;if(parent>=0&&!self(self,unsigned(parent)))return false;const int mapped=map.source_for_target[i];if(mapped>=0){const unsigned j=unsigned(mapped);if(!transform_valid(input.local[j])){error="Invalid source retarget pose";
 return false;}const auto desired=qmul(qmul(tr[i],conjugate(sr[j])),current[j]);next.local[i].rotation=parent>=0?qmul(conjugate(result[unsigned(parent)]),desired):desired;
 const auto delta=sub(input.local[j].translation,source.joints[j].rest.translation);const float old_length=length(source.joints[j].rest.translation),new_length=length(target.joints[i].rest.translation);int mapped_parent=parent;while(mapped_parent>=0&&map.source_for_target[unsigned(mapped_parent)]<0)mapped_parent=target.joints[unsigned(mapped_parent)].parent;const float scale=mapped_parent<0?map.translation_scale:(old_length>1.e-6f?new_length/old_length:map.translation_scale);next.local[i].translation=add(target.joints[i].rest.translation,mul(delta,scale));
 for(unsigned k=0;k<3;k++){const float from=component(source.joints[j].rest.scale,k);if(std::abs(from)<1.e-8){error="Cannot retarget singular rest scale";
 return false;}component(next.local[i].scale,k)=component(target.joints[i].rest.scale,k)*component(input.local[j].scale,k)/from;}}
 result[i]=parent>=0?qmul(result[unsigned(parent)],next.local[i].rotation):next.local[i].rotation;done[i]=true;
 return true;};for(unsigned i=0;i<target.joints.size();i++)if(!walk(walk,i))return false;
 if(!evaluate(target,next,error))return false;
 output=next;
 return true;
}
bool two_bone_ik(const Skeleton &s,Pose &pose,const LegIK &ik,std::string &error,TerrainHeight terrain,void *userdata){
 error.clear();if(ik.hip>=s.joints.size()||ik.knee>=s.joints.size()||ik.ankle>=s.joints.size()||s.joints[ik.knee].parent!=int(ik.hip)||s.joints[ik.ankle].parent!=int(ik.knee)||!finite(ik.target)||!finite(ik.pole)||!finite(ik.weight)||ik.weight<0||ik.weight>1||!finite(ik.sole_offset)||std::abs(ik.sole_offset)>10){error="Invalid two-bone IK chain/target/weight";
 return false;}
 Pose next=pose;if(!evaluate(s,next,error))return false;
 for(int i=int(ik.ankle);i>=0;i=s.joints[unsigned(i)].parent){const auto v=next.local[unsigned(i)].scale;if(v.x<=0||std::abs(v.x-v.y)>1.e-4||std::abs(v.x-v.z)>1.e-4){error="Two-bone IK requires uniform positive chain scales";
 return false;}}
 if(ik.weight==0)return true;
 Vec3 target=ik.target;
 if(terrain){double height=target.y;bool accepted=false;try{accepted=terrain(target.x,target.z,height,userdata);}catch(...){error="Terrain IK callback failed";
 return false;}if(!accepted||!std::isfinite(height)||std::abs(height)>1.e8){error="Terrain IK height unavailable";
 return false;}target.y=float(height)+ik.sole_offset;}
 const Vec3 hip=transform_point(next.global[ik.hip],{}),knee=transform_point(next.global[ik.knee],{}),ankle=transform_point(next.global[ik.ankle],{});const float upper=length(sub(knee,hip)),lower=length(sub(ankle,knee));if(upper<1.e-5||lower<1.e-5){error="Degenerate IK bone length";
 return false;}
 const Vec3 axis=unit(sub(target,hip),unit(sub(ankle,hip),{0,-1,0}));const float epsilon=std::min(upper,lower)*1.e-4f,distance=std::clamp(length(sub(target,hip)),std::abs(upper-lower)+epsilon,upper+lower-epsilon);target=add(hip,mul(axis,distance));
 Vec3 pole=sub(ik.pole,hip);pole=sub(pole,mul(axis,dot(pole,axis)));if(length(pole)<1.e-6){pole=sub(knee,hip);pole=sub(pole,mul(axis,dot(pole,axis)));}if(length(pole)<1.e-6)pole=cross(axis,std::abs(axis.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0});pole=unit(pole);
 const float along=(upper*upper+distance*distance-lower*lower)/(2*distance),side=std::sqrt(std::max(0.f,upper*upper-along*along));const Vec3 desired_knee=add(hip,add(mul(axis,along),mul(pole,side)));
 std::array<Quat,node_limit> rotations;global_rotations(s,next.local,rotations);const Quat foot_rotation=rotations[ik.ankle];const auto parent=s.joints[ik.hip].parent;const Quat hip_world=qmul(from_to(sub(knee,hip),sub(desired_knee,hip)),rotations[ik.hip]);const Quat hip_local=parent>=0?qmul(conjugate(rotations[unsigned(parent)]),hip_world):hip_world;next.local[ik.hip].rotation=slerp(next.local[ik.hip].rotation,hip_local,ik.weight);
 if(!evaluate(s,next,error))return false;
 global_rotations(s,next.local,rotations);const Vec3 new_knee=transform_point(next.global[ik.knee],{}),new_ankle=transform_point(next.global[ik.ankle],{});const Quat knee_world=qmul(from_to(sub(new_ankle,new_knee),sub(target,new_knee)),rotations[ik.knee]);const Quat knee_local=qmul(conjugate(rotations[ik.hip]),knee_world);next.local[ik.knee].rotation=slerp(next.local[ik.knee].rotation,knee_local,ik.weight);
 if(!evaluate(s,next,error))return false;
 global_rotations(s,next.local,rotations);const auto ankle_local=qmul(conjugate(rotations[ik.knee]),foot_rotation);next.local[ik.ankle].rotation=slerp(next.local[ik.ankle].rotation,ankle_local,ik.weight);
 if(!evaluate(s,next,error))return false;
 pose=next;
 return true;
}
bool morph_vertex(const Asset &asset,unsigned vertex,const Pose &pose,SkinVertex &out,std::string &error){
 error.clear();if(vertex>=asset.mesh.vertices.size()||asset.morph_targets.size()>morph_limit){error="Invalid morph reference vertex/targets";return false;}
 SkinVertex next=asset.mesh.vertices[vertex];Vec3 tangent{next.tangent[0],next.tangent[1],next.tangent[2]};
 for(unsigned target=0;target<asset.morph_targets.size();++target){const float w=pose.morph_weights[target];if(!finite(w)||w<0||w>1){error="Invalid morph reference weight";return false;}if(w==0)continue;const auto &deltas=asset.morph_targets[target].deltas;auto at=std::lower_bound(deltas.begin(),deltas.end(),vertex,[](const MorphDelta &d,unsigned v){return d.vertex<v;});if(at==deltas.end()||at->vertex!=vertex)continue;next.position=add(next.position,mul(at->position,w));next.normal=add(next.normal,mul(at->normal,w));tangent=add(tangent,mul(at->tangent,w));}
 next.normal=unit(next.normal,asset.mesh.vertices[vertex].normal);tangent=sub(tangent,mul(next.normal,dot(next.normal,tangent)));tangent=unit(tangent,Vec3{next.tangent[0],next.tangent[1],next.tangent[2]});next.tangent={tangent.x,tangent.y,tangent.z,next.tangent[3]};out=next;return true;
}
bool deform_vertex(const SkinVertex &vertex,const Pose &pose,Vec3 &position,Vec3 &normal,std::string &error){
 error.clear();if(!finite(vertex.position)||!finite(vertex.normal)||pose.joints>joint_limit){error="Invalid CPU skinning input";
 return false;}Mat4 blended;blended.m.fill(0);float sum=0;
 for(unsigned k=0;k<4;k++){if(vertex.joints[k]>=pose.joints||!std::isfinite(vertex.weights[k])||vertex.weights[k]<0||vertex.weights[k]>1){error="Invalid CPU skin joint/weight";
 return false;}sum+=vertex.weights[k];for(unsigned m=0;m<16;m++)blended.m[m]+=pose.skin[vertex.joints[k]].m[m]*vertex.weights[k];}
 Mat4 inverted;if(std::abs(sum-1)>.002f||!inverse(blended,inverted)){error="Invalid/singular blended skin matrix";
 return false;}const Vec3 p=transform_point(blended,vertex.position);const auto &m=inverted.m;Vec3 n{m[0]*vertex.normal.x+m[1]*vertex.normal.y+m[2]*vertex.normal.z,m[4]*vertex.normal.x+m[5]*vertex.normal.y+m[6]*vertex.normal.z,m[8]*vertex.normal.x+m[9]*vertex.normal.y+m[10]*vertex.normal.z};if(!finite(p)||!finite(n)||length(n)<1.e-8){error="CPU skin deformation overflow/zero normal";
 return false;}position=p;normal=unit(n);
 return true;
}
namespace {
bool accessor(const cgltf_accessor *a,cgltf_type type,std::size_t count=0){return a&&a->type==type&&!a->is_sparse&&(!count||a->count==count)&&a->buffer_view&&!a->buffer_view->has_meshopt_compression;}
const cgltf_accessor *attribute(const cgltf_primitive &p,cgltf_attribute_type type,unsigned index=0){for(unsigned i=0;i<p.attributes_count;i++)if(p.attributes[i].type==type&&p.attributes[i].index==int(index))return p.attributes[i].data;
 return nullptr;}
bool floats(const cgltf_accessor *a,unsigned index,float *values,unsigned size){return cgltf_accessor_read_float(a,index,values,size)!=0&&std::all_of(values,values+size,[](float f){return finite(f);});}
bool material_map(const cgltf_data *data,const cgltf_texture_view &view,TextureMap &out,std::string &error){
 if(!view.texture)return true;
 if(!view.texture->image||!view.texture->image->buffer_view||view.texcoord!=0||view.has_transform){error="Skinned materials require embedded images and untransformed UV0";return false;}
 out.image=int(view.texture->image-data->images);
 const auto *sampler=view.texture->sampler;
 if(sampler){if(sampler->min_filter)out.min_filter=unsigned(sampler->min_filter);if(sampler->mag_filter)out.mag_filter=unsigned(sampler->mag_filter);if(sampler->wrap_s)out.wrap_s=unsigned(sampler->wrap_s);if(sampler->wrap_t)out.wrap_t=unsigned(sampler->wrap_t);}
 return true;
}
void complete_skin_basis(SkinMesh &mesh,const Primitive &primitive,std::size_t base,std::size_t count){
 std::vector<Vec3> tangents(count),bitangents(count);
 if(!primitive.has_normals)for(std::size_t i=base;i<base+count;i++)mesh.vertices[i].normal={};
 for(std::size_t i=primitive.first_index;i<primitive.first_index+primitive.index_count;i+=3){
  auto &a=mesh.vertices[mesh.indices[i]],&b=mesh.vertices[mesh.indices[i+1]],&c=mesh.vertices[mesh.indices[i+2]];
  const Vec3 e1=sub(b.position,a.position),e2=sub(c.position,a.position),n=cross(e1,e2);
  if(!primitive.has_normals)for(auto *vertex:{&a,&b,&c})vertex->normal=add(vertex->normal,n);
  if(!primitive.has_tangents){const float du1=b.uv[0]-a.uv[0],dv1=b.uv[1]-a.uv[1],du2=c.uv[0]-a.uv[0],dv2=c.uv[1]-a.uv[1],det=du1*dv2-dv1*du2;if(std::abs(det)>1.e-12f){const auto t=mul(sub(mul(e1,dv2),mul(e2,dv1)),1/det),bt=mul(sub(mul(e2,du1),mul(e1,du2)),1/det);for(unsigned corner=0;corner<3;corner++){const auto at=mesh.indices[i+corner]-base;tangents[at]=add(tangents[at],t);bitangents[at]=add(bitangents[at],bt);}}}
 }
 for(std::size_t i=base;i<base+count;i++){
  auto &v=mesh.vertices[i];v.normal=unit(v.normal,{0,1,0});Vec3 t=primitive.has_tangents?Vec3{v.tangent[0],v.tangent[1],v.tangent[2]}:tangents[i-base];t=sub(t,mul(v.normal,dot(t,v.normal)));t=unit(t,unit(cross(std::abs(v.normal.y)<.9f?Vec3{0,1,0}:Vec3{1,0,0},v.normal)));
  const float sign=primitive.has_tangents?v.tangent[3]:(dot(cross(v.normal,t),bitangents[i-base])<0?-1.f:1.f);v.tangent={t.x,t.y,t.z,sign};
 }
}
}
static bool load_glb_impl(const char *path,Asset &output,std::string &error){
 error.clear();auto fail=[&](const char *message){error=message;
 return false;};if(!path||!*path)return fail("Missing skeletal GLB path");
 // Detailed modular outfits can use the bounded32MiB image allowance plus
 // their real vertex/skin buffers. Preserve the independent image, vertex,
 // index and material caps; the container may be at most64MiB in total.
 std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file||file.tellg()<20||file.tellg()>64*1024*1024)return fail("Missing/oversized skeletal GLB (64MiB maximum)");const auto size=std::size_t(file.tellg());file.seekg(0);std::vector<char> bytes(size);if(!file.read(bytes.data(),std::streamsize(size)))return fail("Incomplete skeletal GLB read");
 cgltf_options options={};cgltf_data *raw=nullptr;if(cgltf_parse(&options,bytes.data(),bytes.size(),&raw)!=cgltf_result_success)return fail("Cannot parse skeletal GLB");std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
 if(data->file_type!=cgltf_file_type_glb||data->buffers_count!=1||data->buffers[0].uri||!data->bin||data->nodes_count==0||data->nodes_count>node_limit||data->skins_count==0||data->animations_count>clip_limit)return fail("Requires embedded GLB2,1buffer,<=256nodes,skin and<=96clips");
 if(cgltf_load_buffers(&options,data.get(),nullptr)!=cgltf_result_success||cgltf_validate(data.get())!=cgltf_result_success)return fail("Invalid embedded skeletal GLB buffers");
 Asset next;next.source_path=path;auto &s=next.skeleton;s.joints.resize(data->nodes_count);
 if(data->images_count>128||data->materials_count>128)return fail("Skinned image/material budget exceeded");
 std::size_t image_bytes=0;for(unsigned i=0;i<data->images_count;i++){const auto &image=data->images[i];if(!image.buffer_view||image.uri||image.buffer_view->has_meshopt_compression||image.buffer_view->size<4||image.buffer_view->size>32*1024*1024-image_bytes)return fail("Skinned textures must be bounded embedded image bytes");const auto *encoded=cgltf_buffer_view_data(image.buffer_view);if(!encoded)return fail("Missing embedded skin texture data");Image copy;copy.encoded.assign(encoded,encoded+image.buffer_view->size);image_bytes+=copy.encoded.size();next.images.push_back(std::move(copy));}
 for(unsigned i=0;i<data->materials_count;i++){
  const auto &m=data->materials[i];if(m.alpha_mode==cgltf_alpha_mode_blend||m.unlit||m.has_transmission||m.has_pbr_specular_glossiness)return fail("Skinned renderer supports opaque/MASK metallic-roughness PBR materials");Material copy;const auto &pbr=m.pbr_metallic_roughness;std::copy(pbr.base_color_factor,pbr.base_color_factor+4,copy.base_factor.begin());copy.roughness=pbr.roughness_factor;copy.metallic=pbr.metallic_factor;copy.normal_scale=m.normal_texture.scale;copy.occlusion_strength=m.occlusion_texture.scale;copy.alpha_cutoff=m.alpha_mode==cgltf_alpha_mode_mask?m.alpha_cutoff:0;copy.double_sided=m.double_sided;
  copy.hair_material=world_model_hair::skeletal(path,&m);
  copy.organic_body=world_model_skin::skeletal(path,&m);
  const float strength=m.has_emissive_strength?m.emissive_strength.emissive_strength:1;for(unsigned k=0;k<3;k++)copy.emissive_factor[k]=m.emissive_factor[k]*strength;
  if(!material_map(data.get(),pbr.base_color_texture,copy.base,error)||!material_map(data.get(),m.normal_texture,copy.normal,error)||!material_map(data.get(),pbr.metallic_roughness_texture,copy.metallic_roughness,error)||!material_map(data.get(),m.occlusion_texture,copy.occlusion,error)||!material_map(data.get(),m.emissive_texture,copy.emissive,error))return false;
  next.materials.push_back(copy);
 }
 for(unsigned i=0;i<data->nodes_count;i++){const auto &node=data->nodes[i];auto &joint=s.joints[i];joint.name=node.name&&*node.name?node.name:"node_"+std::to_string(i);joint.parent=node.parent?int(node.parent-data->nodes):-1;
 if(node.has_matrix){Mat4 matrix;std::copy(node.matrix,node.matrix+16,matrix.m.begin());if(!decompose(matrix,joint.rest))return fail("Sheared/singular node matrix cannot be represented as TRS");}
 else{joint.rest.translation={node.translation[0],node.translation[1],node.translation[2]};joint.rest.rotation={node.rotation[0],node.rotation[1],node.rotation[2],node.rotation[3]};joint.rest.scale={node.scale[0],node.scale[1],node.scale[2]};}
 }
 const auto &skin=data->skins[0];if(skin.joints_count==0||skin.joints_count>joint_limit)return fail("Skin must contain1..128 joints");s.palette.resize(skin.joints_count);s.inverse_bind.resize(skin.joints_count);
 if(skin.inverse_bind_matrices&&(!accessor(skin.inverse_bind_matrices,cgltf_type_mat4,skin.joints_count)||skin.inverse_bind_matrices->component_type!=cgltf_component_type_r_32f))return fail("Unsupported/sparse inverse-bind accessor");
 for(unsigned i=0;i<skin.joints_count;i++){s.palette[i]=unsigned(skin.joints[i]-data->nodes);if(skin.inverse_bind_matrices&&!floats(skin.inverse_bind_matrices,i,s.inverse_bind[i].m.data(),16))return fail("Invalid inverse-bind matrix");}
 const cgltf_node *mesh_node=nullptr;for(unsigned i=0;i<data->nodes_count;i++)if(data->nodes[i].skin==&skin&&data->nodes[i].mesh){if(mesh_node)return fail("First skin has multiple mesh nodes; split/cook separately");mesh_node=&data->nodes[i];s.mesh_node=i;}
 if(!mesh_node)return fail("First skin has no skinned mesh node");
 const auto &mesh=*mesh_node->mesh;if(mesh.primitives_count>128)return fail("Skinned primitive budget exceeded");
 if(mesh.target_names_count>morph_limit)return fail("Morph target count exceeds bounded contract");
 if(mesh.target_names_count){next.morph_targets.resize(mesh.target_names_count);for(unsigned t=0;t<mesh.target_names_count;++t){if(!mesh.target_names[t]||std::strlen(mesh.target_names[t])>64)return fail("Missing/oversized morph target name");next.morph_targets[t].name=mesh.target_names[t];}}
 if(mesh.weights_count){if(mesh.weights_count!=mesh.target_names_count)return fail("Invalid default morph weights");for(unsigned t=0;t<mesh.weights_count;++t)if(!finite(mesh.weights[t])||mesh.weights[t]!=0)return fail("Cook morph assets in neutral rest");}
 if(mesh_node->weights_count){if(mesh_node->weights_count!=mesh.target_names_count)return fail("Invalid node morph weight count");for(unsigned t=0;t<mesh_node->weights_count;++t)if(!finite(mesh_node->weights[t])||mesh_node->weights[t]!=0)return fail("Cook morph nodes in neutral rest");}
 for(unsigned pi=0;pi<mesh.primitives_count;pi++){
 const auto &p=mesh.primitives[pi];const auto *position=attribute(p,cgltf_attribute_type_position),*normal=attribute(p,cgltf_attribute_type_normal),*uv=attribute(p,cgltf_attribute_type_texcoord),*joints=attribute(p,cgltf_attribute_type_joints),*weights=attribute(p,cgltf_attribute_type_weights),*tangent=attribute(p,cgltf_attribute_type_tangent);
 if(p.type!=cgltf_primitive_type_triangles||p.has_draco_mesh_compression||!accessor(position,cgltf_type_vec3)||position->component_type!=cgltf_component_type_r_32f||!accessor(joints,cgltf_type_vec4,position->count)||!accessor(weights,cgltf_type_vec4,position->count)||attribute(p,cgltf_attribute_type_joints,1)||attribute(p,cgltf_attribute_type_weights,1))return fail("Requires uncompressed triangle POSITION/JOINTS0/WEIGHTS0 with4influences");
 if((joints->component_type!=cgltf_component_type_r_8u&&joints->component_type!=cgltf_component_type_r_16u)||joints->normalized||(weights->component_type!=cgltf_component_type_r_32f&&!(weights->normalized&&(weights->component_type==cgltf_component_type_r_8u||weights->component_type==cgltf_component_type_r_16u))))return fail("Invalid joint/weight accessor encoding");
 if((normal&&(!accessor(normal,cgltf_type_vec3,position->count)||normal->component_type!=cgltf_component_type_r_32f))||(uv&&!accessor(uv,cgltf_type_vec2,position->count)))return fail("Invalid/sparse normal/UV accessor");
 if(tangent&&(!accessor(tangent,cgltf_type_vec4,position->count)||tangent->component_type!=cgltf_component_type_r_32f))return fail("Invalid/sparse tangent accessor");
 if(!uv&&p.material){const auto &m=next.materials[unsigned(p.material-data->materials)];if(m.base.image>=0||m.normal.image>=0||m.metallic_roughness.image>=0||m.occlusion.image>=0||m.emissive.image>=0)return fail("Textured skinned primitive has no UV0 accessor");}
 if(position->count>200000-next.mesh.vertices.size())return fail("Skinned vertex budget exceeded");
 const auto base=std::uint32_t(next.mesh.vertices.size());
 if(p.targets_count!=mesh.target_names_count)return fail("Every primitive must match named morph count");

 for(unsigned vi=0;vi<position->count;vi++){SkinVertex v;float xyz[3];if(!floats(position,vi,xyz,3))return fail("Invalid position data");v.position={xyz[0],xyz[1],xyz[2]};if(normal){if(!floats(normal,vi,xyz,3))return fail("Invalid normal data");v.normal={xyz[0],xyz[1],xyz[2]};if(length(v.normal)<1.e-8)return fail("Zero skinned normal");v.normal=unit(v.normal);}if(uv&&!floats(uv,vi,v.uv.data(),2))return fail("Invalid UV data");cgltf_uint js[4];if(!cgltf_accessor_read_uint(joints,vi,js,4)||!floats(weights,vi,v.weights.data(),4))return fail("Invalid skin joint/weight data");float sum=0;for(unsigned k=0;k<4;k++){v.joints[k]=js[k];if(js[k]>=skin.joints_count||v.weights[k]<0||v.weights[k]>1)return fail("Skin joint/weight outside declared bounds");sum+=v.weights[k];}if(sum<1.e-8||std::abs(sum-1)>.02)return fail("Skin weights are not normalized");for(float &w:v.weights)w/=sum;next.mesh.vertices.push_back(v);}
 if(tangent)for(unsigned vi=0;vi<position->count;vi++)if(!floats(tangent,vi,next.mesh.vertices[base+vi].tangent.data(),4)||std::abs(std::abs(next.mesh.vertices[base+vi].tangent[3])-1)>.001f)return fail("Invalid tangent handedness/value");
 const auto count=p.indices?p.indices->count:position->count;if(count%3||count>600000-next.mesh.indices.size()||(p.indices&&(!accessor(p.indices,cgltf_type_scalar)||(p.indices->component_type!=cgltf_component_type_r_8u&&p.indices->component_type!=cgltf_component_type_r_16u&&p.indices->component_type!=cgltf_component_type_r_32u))))return fail("Invalid triangle indices/budget");Primitive primitive;primitive.first_index=next.mesh.indices.size();primitive.index_count=count;primitive.has_normals=normal!=nullptr;primitive.has_uv=uv!=nullptr;primitive.has_tangents=tangent!=nullptr;primitive.material=p.material?int(p.material-data->materials):-1;
 for(unsigned ii=0;ii<count;ii++){const auto idx=p.indices?cgltf_accessor_read_index(p.indices,ii):ii;if(idx>=position->count)return fail("Skinned triangle index outside vertices");next.mesh.indices.push_back(base+std::uint32_t(idx));}complete_skin_basis(next.mesh,primitive,base,position->count);next.mesh.primitives.push_back(primitive);
 if(p.targets_count){
  // Only target data may be sparse. cgltf_validate has checked byte ranges and
  // sparse index bounds before unpacking; reject duplicate/nonmonotonic indices.
  for(unsigned t=0;t<p.targets_count;++t){const auto &target=p.targets[t];if(!target.attributes_count)continue;if(target.attributes_count>3)return fail("Morph target supports POSITION/NORMAL/TANGENT only");std::array<std::vector<float>,3> values;
   for(unsigned ai=0;ai<target.attributes_count;++ai){const auto &attr=target.attributes[ai];unsigned role=0;if(attr.type==cgltf_attribute_type_position)role=0;else if(attr.type==cgltf_attribute_type_normal)role=1;else if(attr.type==cgltf_attribute_type_tangent)role=2;else return fail("Unknown morph target attribute");if(attr.index!=0||!values[role].empty())return fail("Duplicate morph attribute");const auto *a=attr.data;
    if(!a||a->type!=cgltf_type_vec3||a->component_type!=cgltf_component_type_r_32f||a->normalized||a->stride!=3*sizeof(float)||a->count!=position->count||(a->buffer_view&&a->buffer_view->has_meshopt_compression))return fail("Invalid morph accessor");
    if(a->is_sparse){const auto &sp=a->sparse;if(!sp.count||sp.count>a->count||sp.indices_buffer_view->stride||sp.values_buffer_view->stride||sp.indices_buffer_view->has_meshopt_compression||sp.values_buffer_view->has_meshopt_compression)return fail("Invalid sparse morph storage");const auto *bytes=cgltf_buffer_view_data(sp.indices_buffer_view);if(!bytes)return fail("Missing sparse indices");unsigned prior=0;for(std::size_t k=0;k<sp.count;++k){unsigned v=0;const auto *at=bytes+sp.indices_byte_offset+k*cgltf_component_size(sp.indices_component_type);if(sp.indices_component_type==cgltf_component_type_r_8u)v=*at;else if(sp.indices_component_type==cgltf_component_type_r_16u){std::uint16_t q;std::memcpy(&q,at,2);v=q;}else if(sp.indices_component_type==cgltf_component_type_r_32u)std::memcpy(&v,at,4);else return fail("Invalid sparse index type");if(v>=a->count||(k&&v<=prior))return fail("Sparse indices must increase strictly");prior=v;}}
    values[role].resize(position->count*3);if(cgltf_accessor_unpack_floats(a,values[role].data(),values[role].size())!=values[role].size())return fail("Cannot unpack authored morph");for(float value:values[role])if(!finite(value))return fail("Nonfinite morph value");
   }
   auto &deltas=next.morph_targets[t].deltas;for(unsigned vi=0;vi<position->count;++vi){MorphDelta delta;delta.vertex=base+vi;Vec3 *components[]={&delta.position,&delta.normal,&delta.tangent};bool nonzero=false;for(unsigned role=0;role<3;++role)if(!values[role].empty()){const auto *v=values[role].data()+vi*3;*components[role]={v[0],v[1],v[2]};nonzero|=v[0]!=0||v[1]!=0||v[2]!=0;}if(nonzero)deltas.push_back(delta);if(deltas.size()>morph_vertex_limit)return fail("Morph delta vertex budget exceeded");}
  }
 }
 }
 std::size_t total_keys=0;for(unsigned ci=0;ci<data->animations_count;ci++){
 const auto &input=data->animations[ci];if(input.channels_count>channel_limit)return fail("Animation channel budget exceeded");Clip clip;clip.name=input.name?input.name:"clip_"+std::to_string(ci);
 for(unsigned chi=0;chi<input.channels_count;chi++){const auto &channel=input.channels[chi];if(!channel.target_node||!channel.sampler||channel.target_path==cgltf_animation_path_type_weights||channel.target_node->has_matrix)return fail("Unsupported morph/matrix animation target");const auto &sampler=*channel.sampler;Channel c;c.node=unsigned(channel.target_node-data->nodes);
 if(channel.target_path==cgltf_animation_path_type_translation)c.path=Path::Translation;else if(channel.target_path==cgltf_animation_path_type_rotation)c.path=Path::Rotation;else if(channel.target_path==cgltf_animation_path_type_scale)c.path=Path::Scale;else return fail("Unknown animation path");
 if(sampler.interpolation==cgltf_interpolation_type_linear)c.interpolation=Interpolation::Linear;else if(sampler.interpolation==cgltf_interpolation_type_step)c.interpolation=Interpolation::Step;else if(sampler.interpolation==cgltf_interpolation_type_cubic_spline)c.interpolation=Interpolation::CubicSpline;else return fail("Unknown interpolation mode");
 const auto *times=sampler.input,*values=sampler.output;const unsigned factor=c.interpolation==Interpolation::CubicSpline?3:1;const auto type=c.path==Path::Rotation?cgltf_type_vec4:cgltf_type_vec3;
 if(!accessor(times,cgltf_type_scalar)||times->component_type!=cgltf_component_type_r_32f||times->count==0||times->count>key_limit||times->count>262144-total_keys||!accessor(values,type,times->count*factor)||values->component_type!=cgltf_component_type_r_32f)return fail("Invalid/sparse animation sampler shape/component type/key budget");
 total_keys+=times->count;c.times.resize(times->count);c.values.resize(values->count);
 for(unsigned k=0;k<times->count;k++){if(!floats(times,k,&c.times[k],1))return fail("Invalid animation input time");clip.duration=std::max(clip.duration,double(c.times[k]));}for(unsigned k=0;k<values->count;k++)if(!floats(values,k,c.values[k].data(),c.path==Path::Rotation?4:3))return fail("Invalid animation output value");clip.channels.push_back(std::move(c));
 }next.clips.push_back(std::move(clip));
 }
 if(!validate(next,error))return false;
 Pose rest;if(!rest_pose(s,rest,error))return false;output=std::move(next);
 return true;
}
bool load_glb(const char *path,Asset &output,std::string &error){
 try{return load_glb_impl(path,output,error);}catch(const std::exception &e){error="Skeletal GLB import allocation/parse failure: "+std::string(e.what());return false;}
}
}
