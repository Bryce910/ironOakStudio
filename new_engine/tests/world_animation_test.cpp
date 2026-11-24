#include "engine/world_animation.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <vector>
#include <unistd.h>
using namespace world_animation;
static bool count_allocations=false;static std::size_t allocation_count=0;
[[gnu::noinline]] void *operator new(std::size_t size){if(count_allocations)allocation_count++;if(void *p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
namespace {
bool near(float a,float b,float e=2.e-4f){return std::abs(a-b)<=e;}
void vector_near(Vec3 a,Vec3 b,float e=2.e-4f){assert(near(a.x,b.x,e)&&near(a.y,b.y,e)&&near(a.z,b.z,e));}
Quat z_rotation(float radians){return {0,0,std::sin(radians*.5f),std::cos(radians*.5f)};}
struct Files {std::string directory;Files(){char t[]="/tmp/valwood-animation-test-XXXXXX";const auto *p=mkdtemp(t);assert(p);directory=p;}~Files(){std::filesystem::remove_all(directory);}std::string path(const char *p){return directory+"/"+p;}};
std::vector<unsigned char> png_rgba(unsigned width,unsigned height,const std::vector<unsigned char> &rgba){
 assert(rgba.size()==std::size_t(width)*height*4);std::vector<unsigned char> png={137,80,78,71,13,10,26,10};
 auto be=[](std::vector<unsigned char> &v,std::uint32_t word){for(int i=3;i>=0;i--)v.push_back(static_cast<unsigned char>(word>>(8*i)));};
 auto chunk=[&](const char *type,const std::vector<unsigned char> &data){be(png,std::uint32_t(data.size()));const auto begin=png.size();png.insert(png.end(),type,type+4);png.insert(png.end(),data.begin(),data.end());std::uint32_t crc=~0u;for(std::size_t i=begin;i<png.size();i++){crc^=png[i];for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}be(png,~crc);};
 std::vector<unsigned char> ihdr;be(ihdr,width);be(ihdr,height);ihdr.insert(ihdr.end(),{8,6,0,0,0});chunk("IHDR",ihdr);
 std::vector<unsigned char> raw;for(unsigned y=0;y<height;y++){raw.push_back(0);raw.insert(raw.end(),rgba.begin()+std::ptrdiff_t(y*width*4),rgba.begin()+std::ptrdiff_t((y+1)*width*4));}assert(raw.size()<65536);const auto n=unsigned(raw.size());std::vector<unsigned char> compressed={0x78,0x01,1,static_cast<unsigned char>(n),static_cast<unsigned char>(n>>8),static_cast<unsigned char>(~n),static_cast<unsigned char>((~n)>>8)};compressed.insert(compressed.end(),raw.begin(),raw.end());std::uint32_t a=1,b=0;for(auto byte:raw){a=(a+byte)%65521;b=(b+a)%65521;}be(compressed,(b<<16)|a);chunk("IDAT",compressed);chunk("IEND",{});return png;
}
struct GLB {
 std::vector<unsigned char> bin;std::vector<std::string> views,accessors;std::size_t joint_offset=0;
 template<class T> unsigned data(const std::vector<T> &v,unsigned component,const char *type,unsigned count,const char *range=""){
  while(bin.size()%4)bin.push_back(0);
  const auto offset=bin.size();const auto *bytes=reinterpret_cast<const unsigned char *>(v.data());bin.insert(bin.end(),bytes,bytes+v.size()*sizeof(T));std::ostringstream view;view<<"{\"buffer\":0,\"byteOffset\":"<<offset<<",\"byteLength\":"<<v.size()*sizeof(T)<<'}';views.push_back(view.str());std::ostringstream accessor;accessor<<"{\"bufferView\":"<<views.size()-1<<",\"componentType\":"<<component<<",\"count\":"<<count<<",\"type\":\""<<type<<"\""<<range<<'}';accessors.push_back(accessor.str());return unsigned(accessors.size()-1);
 }
 std::string join(const std::vector<std::string> &v){std::string s;for(unsigned i=0;i<v.size();i++){if(i)s+=',';s+=v[i];}return s;}
 static void u32(std::vector<unsigned char> &out,std::uint32_t v){for(unsigned k=0;k<4;k++)out.push_back(static_cast<unsigned char>(v>>(k*8)));}
 void save(const std::string &path,bool bad_joint=false,bool external=false,bool quantized=false,bool textured=false){
  const auto pos=data<float>({0,1,0,1,1,0,0,2,0},5126,"VEC3",3,",\"min\":[0,1,0],\"max\":[1,2,0]");
  const auto normal=data<float>({1,0,0,1,0,0,1,0,0},5126,"VEC3",3);
  while(bin.size()%4)bin.push_back(0);
  joint_offset=bin.size();const auto joints=data<std::uint16_t>({std::uint16_t(bad_joint?9:1),0,0,0,1,0,0,0,1,0,0,0},5123,"VEC4",3);
  const auto weights=quantized?data<std::uint8_t>({255,0,0,0,255,0,0,0,255,0,0,0},5121,"VEC4",3,",\"normalized\":true"):data<float>({1,0,0,0,1,0,0,0,1,0,0,0},5126,"VEC4",3);const auto indices=data<std::uint16_t>({0,1,2},5123,"SCALAR",3);
  auto identity=Mat4{}.m;std::vector<float> binds(identity.begin(),identity.end());identity[13]=-1;binds.insert(binds.end(),identity.begin(),identity.end());const auto ibm=data<float>(binds,5126,"MAT4",2);
  const auto times=data<float>({0,2},5126,"SCALAR",2,",\"min\":[0],\"max\":[2]"),steps=data<float>({0,1,2},5126,"SCALAR",3,",\"min\":[0],\"max\":[2]");
  const auto translation=data<float>({5,0,0,7,0,0},5126,"VEC3",2),rotation=data<float>({0,0,0,1,0,0,1,0},5126,"VEC4",2),scale=data<float>({1,1,1,2,2,2,3,3,3},5126,"VEC3",3);
  const auto cubic=data<float>({0,0,0,5,0,0,2,0,0,0,0,0,7,0,0,0,0,0},5126,"VEC3",6);
  const auto cubic_rotation=data<float>({0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},5126,"VEC4",6);
  const auto nonuniform=data<float>({1,1,1,2,1,1},5126,"VEC3",2);
  std::string extra,uv_attribute,material_attribute;
  if(textured){const auto uv=data<float>({0,0,1,0,0,1},5126,"VEC2",3);uv_attribute=",\"TEXCOORD_0\":"+std::to_string(uv);std::vector<unsigned> image_views;
   for(const auto &png:{png_rgba(2,1,{128,64,32,0,128,64,32,255}),png_rgba(1,1,{128,128,255,255}),png_rgba(1,1,{200,128,64,255})}){data<unsigned char>(png,5121,"SCALAR",unsigned(png.size()));image_views.push_back(unsigned(views.size()-1));}
   std::ostringstream properties;properties<<",\"images\":[";for(unsigned i=0;i<3;i++){if(i)properties<<',';properties<<"{\"bufferView\":"<<image_views[i]<<",\"mimeType\":\"image/png\"}";}properties<<"],\"samplers\":[{\"magFilter\":9728,\"minFilter\":9728,\"wrapS\":33071,\"wrapT\":33071}],\"textures\":[{\"source\":0,\"sampler\":0},{\"source\":1,\"sampler\":0},{\"source\":2,\"sampler\":0}],\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},\"metallicRoughnessTexture\":{\"index\":2},\"roughnessFactor\":0.6,\"metallicFactor\":0.2},\"normalTexture\":{\"index\":1,\"scale\":0.7},\"occlusionTexture\":{\"index\":2},\"emissiveTexture\":{\"index\":0},\"emissiveFactor\":[0.2,0.1,0],\"alphaMode\":\"MASK\",\"alphaCutoff\":0.5,\"doubleSided\":true}]";extra=properties.str();material_attribute=",\"material\":0";
  }
  std::ostringstream json;json<<"{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":"<<bin.size()<<(external?",\"uri\":\"must-not-read.bin\"":"")<<"}],\"bufferViews\":["<<join(views)<<"],\"accessors\":["<<join(accessors)<<"]"<<extra<<",\"nodes\":[{\"name\":\"Root\",\"translation\":[5,0,0],\"children\":[1]},{\"name\":\"Tip\",\"translation\":[0,1,0]},{\"name\":\"Mesh\",\"translation\":[5,0,0],\"mesh\":0,\"skin\":0}],\"skins\":[{\"joints\":[0,1],\"inverseBindMatrices\":"<<ibm<<"}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":"<<pos<<",\"NORMAL\":"<<normal<<",\"JOINTS_0\":"<<joints<<",\"WEIGHTS_0\":"<<weights<<uv_attribute<<"},\"indices\":"<<indices<<material_attribute<<"}]}],\"animations\":[";
  json<<"{\"name\":\"Walk\",\"samplers\":[{\"input\":"<<times<<",\"output\":"<<translation<<",\"interpolation\":\"LINEAR\"},{\"input\":"<<times<<",\"output\":"<<rotation<<",\"interpolation\":\"LINEAR\"}],\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}},{\"sampler\":1,\"target\":{\"node\":1,\"path\":\"rotation\"}}]},";
  json<<"{\"name\":\"Step\",\"samplers\":[{\"input\":"<<steps<<",\"output\":"<<scale<<",\"interpolation\":\"STEP\"}],\"channels\":[{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"scale\"}}]},";
  json<<"{\"name\":\"Cubic\",\"samplers\":[{\"input\":"<<times<<",\"output\":"<<cubic<<",\"interpolation\":\"CUBICSPLINE\"},{\"input\":"<<times<<",\"output\":"<<cubic_rotation<<",\"interpolation\":\"CUBICSPLINE\"}],\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}},{\"sampler\":1,\"target\":{\"node\":1,\"path\":\"rotation\"}}]},";
  json<<"{\"name\":\"Scale\",\"samplers\":[{\"input\":"<<times<<",\"output\":"<<nonuniform<<"}],\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"scale\"}}]}],\"scenes\":[{\"nodes\":[0,2]}],\"scene\":0}";
  auto text=json.str();while(text.size()%4)text+=' ';while(bin.size()%4)bin.push_back(0);std::vector<unsigned char> out;u32(out,0x46546c67);u32(out,2);u32(out,std::uint32_t(12+8+text.size()+8+bin.size()));u32(out,std::uint32_t(text.size()));u32(out,0x4e4f534a);out.insert(out.end(),text.begin(),text.end());u32(out,std::uint32_t(bin.size()));u32(out,0x004e4942);out.insert(out.end(),bin.begin(),bin.end());std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char *>(out.data()),std::streamsize(out.size()));assert(f.good());
 }
};
Asset imported(){Files files;GLB fixture;fixture.save(files.path("rig.glb"));Asset asset;std::string error;assert(load_glb(files.path("rig.glb").c_str(),asset,error));return asset;}
void import_and_skinning(){
 Files files;GLB glb;glb.save(files.path("rig.glb"));Asset asset;std::string error;assert(load_glb(files.path("rig.glb").c_str(),asset,error));assert(asset.skeleton.palette.size()==2&&asset.mesh.vertices.size()==3&&asset.clips.size()==4);assert(validate(asset,error));Pose pose;
 assert(rest_pose(asset.skeleton,pose,error));for(const auto &vertex:asset.mesh.vertices){Vec3 p,n;assert(deform_vertex(vertex,pose,p,n,error));vector_near(p,vertex.position);vector_near(n,{1,0,0});}
 assert(sample(asset.skeleton,asset.clips[0],1,false,pose,error));assert(near(pose.local[0].translation.x,6));vector_near(transform_point(pose.global[1],{}),{6,1,0});Vec3 p,n;assert(deform_vertex(asset.mesh.vertices[1],pose,p,n,error));vector_near(p,{1,2,0});vector_near(n,{0,1,0});
 auto mixed=asset.mesh.vertices[1];mixed.joints={0,1,0,0};mixed.weights={.5f,.5f,0,0};assert(deform_vertex(mixed,pose,p,n,error));vector_near(p,{1.5f,1.5f,0});vector_near(n,{std::sqrt(.5f),std::sqrt(.5f),0});
 assert(sample(asset.skeleton,asset.clips[0],1.999999999,false,pose,error)); // double time must not round past the final float key
 assert(sample(asset.skeleton,asset.clips[0],3,true,pose,error));assert(near(pose.local[0].translation.x,6));assert(sample(asset.skeleton,asset.clips[0],3,false,pose,error));assert(near(pose.local[0].translation.x,7));
 assert(sample(asset.skeleton,asset.clips[1],.999,false,pose,error));assert(near(pose.local[1].scale.x,1));assert(sample(asset.skeleton,asset.clips[1],1,false,pose,error));assert(near(pose.local[1].scale.x,2));
 assert(sample(asset.skeleton,asset.clips[2],1,false,pose,error));assert(near(pose.local[0].translation.x,6.5));assert(near(pose.local[1].rotation.z,std::sqrt(.5f))&&near(pose.local[1].rotation.w,std::sqrt(.5f)));
 assert(sample(asset.skeleton,asset.clips[3],2,false,pose,error));auto vertex=asset.mesh.vertices[0];vertex.normal={std::sqrt(.5f),std::sqrt(.5f),0};assert(deform_vertex(vertex,pose,p,n,error));vector_near(n,{1/std::sqrt(5.f),2/std::sqrt(5.f),0});
 GLB bad;bad.save(files.path("bad.glb"),true);assert(!load_glb(files.path("bad.glb").c_str(),asset,error));assert(asset.clips.size()==4&&asset.mesh.vertices.size()==3);GLB external;external.save(files.path("external.glb"),false,true);assert(!load_glb(files.path("external.glb").c_str(),asset,error));
 GLB quantized;quantized.save(files.path("quantized.glb"),false,false,true);Asset normalized;assert(load_glb(files.path("quantized.glb").c_str(),normalized,error));assert(near(normalized.mesh.vertices[0].weights[0],1));
 GLB textured;textured.save(files.path("textured.glb"),false,false,false,true);Asset material;assert(load_glb(files.path("textured.glb").c_str(),material,error));assert(material.images.size()==3&&material.materials.size()==1&&material.materials[0].base.image==0&&material.materials[0].normal.image==1&&material.materials[0].metallic_roughness.image==2&&material.materials[0].occlusion.image==2&&material.materials[0].emissive.image==0);assert(near(material.materials[0].normal_scale,.7f)&&near(material.materials[0].alpha_cutoff,.5f)&&material.mesh.primitives[0].has_uv);
 for(const auto &v:material.mesh.vertices){const float squared=v.tangent[0]*v.tangent[0]+v.tangent[1]*v.tangent[1]+v.tangent[2]*v.tangent[2],dot=v.normal.x*v.tangent[0]+v.normal.y*v.tangent[1]+v.normal.z*v.tangent[2];assert(near(squared,1)&&near(dot,0));}
 auto invalid=asset;invalid.clips[0].channels[0].times[1]=0;assert(!validate(invalid,error));invalid=asset;invalid.skeleton.joints[0].parent=1;assert(!validate(invalid,error));invalid=asset;invalid.mesh.vertices[0].joints[0]=99;assert(!validate(invalid,error));
 invalid=asset;invalid.mesh.vertices[0].tangent[3]=std::numeric_limits<float>::quiet_NaN();assert(!validate(invalid,error));invalid=asset;invalid.skeleton.palette[1]=invalid.skeleton.palette[0];assert(!validate(invalid,error));
 double times[]={0,.5,1,1.5};std::array<Pose,4> batch;assert(sample_batch(asset,0,times,4,false,batch.data(),error));assert(near(batch[3].local[0].translation.x,6.5));assert(!sample_batch(asset,0,times,batch_limit+1,false,batch.data(),error));
}
void blends_events_and_rootmotion(){
 auto asset=imported();std::string error;asset.clips[0].events={{"start",0},{"hit",.5},{"finish",2}};EventBuffer hits;assert(events_between(asset.clips[0],1.75,2.75,true,hits,error));assert(hits.count==3&&hits.hits[0].absolute_time==2&&hits.hits[1].absolute_time==2&&hits.hits[2].absolute_time==2.5);assert(events_between(asset.clips[0],0,10000,true,hits,error));assert(hits.count==event_limit&&hits.dropped>1000);for(unsigned i=1;i<hits.count;i++)assert(hits.hits[i].absolute_time>=hits.hits[i-1].absolute_time);
 assert(events_between(asset.clips[0],0,1.e12,true,hits,error));assert(hits.count==64&&hits.dropped==1500000000000ull-64);
 std::vector<BlendPoint> points={{0,0,0},{1,0,1},{0,1,2}};std::array<float,16> weights;assert(weights_1d({points[0],points[1]},.25,weights,error));assert(near(weights[0],.75)&&near(weights[1],.25));std::vector<Triangle> triangles={{{0,1,2}}};assert(weights_2d(points,triangles,.25,.25,weights,error));assert(near(weights[0],.5)&&near(weights[1],.25)&&near(weights[2],.25));assert(weights_2d(points,triangles,2,0,weights,error));assert(near(weights[1],1));assert(!weights_2d(points,{{{0,0,2}}},0,0,weights,error));
 Blend input[]={{0,1,.5f},{2,1,.5f}};Pose pose;assert(blend(asset,input,2,false,pose,error));assert(near(pose.local[0].translation.x,6.25));
 Playback playback;playback.time=1;assert(transition(asset,playback,2,1,error));assert(advance(asset,playback,.5,pose,hits,error));assert(playback.fading&&near(float(playback.fade_elapsed),.5));const auto before=pose.local;assert(transition(asset,playback,1,1,error));assert(playback.source_frozen);assert(advance(asset,playback,0,pose,hits,error));for(unsigned i=0;i<pose.nodes;i++)vector_near(pose.local[i].translation,before[i].translation);assert(advance(asset,playback,1,pose,hits,error));assert(!playback.fading&&near(pose.local[1].scale.x,2));
 Rigid motion;assert(root_motion(asset.skeleton,asset.clips[0],0,1.5,2.5,true,motion,error));vector_near(motion.translation,{1,0,0});assert(root_motion(asset.skeleton,asset.clips[0],0,0,10,true,motion,error));vector_near(motion.translation,{10,0,0});
 auto turn=asset.clips[0];Channel rotation;rotation.node=0;rotation.path=Path::Rotation;rotation.times={0,2};rotation.values={{0,0,0,1},{0,0,std::sqrt(.5f),std::sqrt(.5f)}};turn.channels.push_back(rotation);assert(root_motion(asset.skeleton,turn,0,0,4,true,motion,error));vector_near(motion.translation,{2,2,0},.001f);assert(near(std::abs(motion.rotation.z),1));
 assert(sample(asset.skeleton,asset.clips[0],1,false,pose,error));assert(strip_root_motion(asset.skeleton,0,pose,error));assert(near(pose.local[0].translation.x,5));Socket socket;socket.joint="Tip";socket.local.translation={1,0,0};Mat4 socket_world;assert(socket_transform(asset.skeleton,pose,socket,socket_world,error));vector_near(transform_point(socket_world,{}),{5,2,0});socket.joint="missing";assert(!socket_transform(asset.skeleton,pose,socket,socket_world,error));
}
void retarget_and_hierarchy(){
 auto asset=imported();auto source=asset.skeleton,target=source;target.joints[0].rest.rotation=z_rotation(.5f);target.joints[1].rest.translation.y=2;std::string error;RetargetMap map;assert(retarget_map(source,target,map,error,2));Pose input,output;assert(rest_pose(source,input,error));assert(retarget(source,input,target,map,output,error));assert(near(output.local[0].rotation.z,target.joints[0].rest.rotation.z));vector_near(output.local[1].translation,{0,2,0});assert(sample(source,asset.clips[0],1,false,input,error));assert(retarget(source,input,target,map,output,error));assert(near(output.local[0].translation.x,7));
 auto helper_target=target;helper_target.joints.push_back({"Unmapped scene helper",-1,{}});helper_target.joints[0].parent=3;RetargetMap helper_map;assert(retarget_map(source,helper_target,helper_map,error,2));assert(retarget(source,input,helper_target,helper_map,output,error));assert(near(output.local[0].translation.x,7));
 auto wrong=target;wrong.joints[1].name="NoMatchingBone";const auto previous=map;assert(!retarget_map(source,wrong,map,error));assert(map.source_for_target==previous.source_for_target);wrong=target;wrong.joints[0].parent=1;wrong.joints[1].parent=-1;assert(!retarget_map(source,wrong,map,error));
 auto invalid_map=map;invalid_map.source_for_target[1]=-1;const auto old=output.local;assert(!retarget(source,input,target,invalid_map,output,error));for(unsigned i=0;i<output.nodes;i++)vector_near(output.local[i].translation,old[i].translation);
 // Arbitrary source node ordering: parent does not need to precede child.
 Skeleton unordered;unordered.joints={{"Child",1,{{0,2,0},{},{1,1,1}}},{"Parent",-1,{{3,0,0},{},{1,1,1}}}};unordered.mesh_node=1;assert(rest_pose(unordered,output,error));vector_near(transform_point(output.global[0],{}),{3,2,0});
}
bool ground(double,double,double &height,void *){height=.5;return true;}
bool unavailable(double,double,double &,void *){return false;}
void analytic_leg_ik(){
 Skeleton s;s.joints={{"Hip",-1,{{0,2,0},{},{1,1,1}}},{"Knee",0,{{0,-1,0},{},{1,1,1}}},{"Ankle",1,{{0,-1,0},{},{1,1,1}}},{"Mesh",-1,{}}};s.mesh_node=3;std::string error;Pose p;assert(rest_pose(s,p,error));LegIK ik;ik.hip=0;ik.knee=1;ik.ankle=2;ik.target={.5f,0,0};ik.pole={0,1,1};assert(two_bone_ik(s,p,ik,error,ground));vector_near(transform_point(p.global[2],{}),{.5f,.5f,0},.001f);auto h=transform_point(p.global[0],{}),k=transform_point(p.global[1],{}),a=transform_point(p.global[2],{});assert(near(std::hypot(std::hypot(h.x-k.x,h.y-k.y),h.z-k.z),1));assert(near(std::hypot(std::hypot(a.x-k.x,a.y-k.y),a.z-k.z),1));
 auto before=p.local;assert(!two_bone_ik(s,p,ik,error,unavailable));for(unsigned i=0;i<p.nodes;i++)vector_near(p.local[i].translation,before[i].translation);ik.target={100,0,0};assert(two_bone_ik(s,p,ik,error));a=transform_point(p.global[2],{});assert(std::hypot(std::hypot(h.x-a.x,h.y-a.y),h.z-a.z)<=2.001f);
}
void allocation_and_quaternion_contracts(){
 auto asset=imported();std::string error;Pose pose;auto antipodal=asset.clips[0];auto &rotation=antipodal.channels[1];rotation.values={{0,0,0,1},{0,0,0,-1}};assert(sample(asset.skeleton,antipodal,1,false,pose,error));assert(near(std::abs(pose.local[1].rotation.w),1));
 Playback playback;EventBuffer events;Socket socket;socket.joint="Tip";Mat4 transform;assert(rest_pose(asset.skeleton,pose,error));allocation_count=0;count_allocations=true;
 for(unsigned i=0;i<120;i++){assert(sample(asset.skeleton,asset.clips[0],i/60.,true,pose,error));assert(advance(asset,playback,1./60.,pose,events,error));assert(socket_transform(asset.skeleton,pose,socket,transform,error));}
 count_allocations=false;assert(allocation_count==0);
}
}
int main(){import_and_skinning();blends_events_and_rootmotion();retarget_and_hierarchy();analytic_leg_ik();allocation_and_quaternion_contracts();std::puts("world_animation: real embedded GLB TRS/skin/PBR import, LINEAR/STEP/CUBICSPLINE, hierarchy/mesh inverse binds, CPU skin deformation/normals, blendspaces/crossfade/events/root motion/sockets, retarget rejection, analytic terrain IK and allocation-free playback passed");return 0;}
