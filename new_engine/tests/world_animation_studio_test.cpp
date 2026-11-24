// Standalone CPU/ImGui inspector integration test; no native GL context.
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
}
#include "engine/world_studio.h"
#include "engine/world_studio_tools.h"
#include "third_party/imgui/imgui.h"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <memory>
struct Camera {double x=0,y=0,z=0;};
struct Preview {
 std::string project_directory,message;double x=100,y=24,z=100;bool target=true;int tab=0,requested_tab=21;
 bool project_point(const Camera &,world_studio::Vec p,ImVec2 &out)const{out={float(p.x),float(p.y)};return true;}
 #include "engine/world_studio_animation_view.inc"
};
int main(){
 Files files;GLB glb;glb.save(files.path("rig.glb"));
 Preview p;p.project_directory=files.directory;std::snprintf(p.animation_file,sizeof p.animation_file,"rig.glb");p.animation_load();
 assert(p.animation_ready&&p.animation_asset.clips.size()==4&&p.animation_pose.nodes==3&&p.animation_pose.joints==2);
 assert(p.animation_total_channels==6&&p.animation_total_keys==13);assert(p.animation_origin_ready);
 vector_near(p.animation_mesh_min,{0,1,0});vector_near(p.animation_mesh_max,{1,2,0});
 assert(p.animation_sample_preview(1));assert(near(p.animation_pose.local[0].translation.x,6));
 p.animation_measure_bounds();assert(p.animation_measured_bounds&&p.animation_bounds_time==1);
 p.animation_mode=1;p.animation_target=0;p.animation_secondary=2;p.animation_playback.time=1;
 assert(p.animation_sample_preview(0));assert(near(p.animation_pose.local[0].translation.x,6.25));
 p.animation_mode=0;assert(transition(p.animation_asset,p.animation_playback,1,.25,p.animation_error));assert(p.animation_sample_preview(.25));assert(!p.animation_playback.fading);
 p.animation_socket.joint="Tip";p.animation_socket_ready=socket_transform(p.animation_asset.skeleton,p.animation_pose,p.animation_socket,p.animation_socket_matrix,p.animation_socket_error);assert(p.animation_socket_ready);
 const auto good_pose=p.animation_pose.local;std::snprintf(p.animation_file,sizeof p.animation_file,"../rig.glb");p.animation_load();assert(!p.animation_error.empty()&&p.animation_ready);for(unsigned i=0;i<p.animation_pose.nodes;i++)vector_near(p.animation_pose.local[i].translation,good_pose[i].translation);
 std::snprintf(p.animation_file,sizeof p.animation_file,"/etc/passwd");std::filesystem::path resolved;assert(!p.animation_resolve_file(resolved,p.animation_error));
 std::filesystem::create_symlink("/etc/passwd",files.path("outside.glb"));std::snprintf(p.animation_file,sizeof p.animation_file,"outside.glb");assert(!p.animation_resolve_file(resolved,p.animation_error));
 {std::ofstream bad(files.path("bad.glb"));bad<<"invalid";}std::snprintf(p.animation_file,sizeof p.animation_file,"bad.glb");p.animation_load();assert(p.animation_ready&&p.animation_asset.clips.size()==4&&!p.animation_error.empty());
 p.animation_error.clear();allocation_count=0;count_allocations=true;for(unsigned i=0;i<10000;i++)assert(p.animation_sample_preview(1./60.));count_allocations=false;assert(allocation_count==0);
 auto *context=ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1280,720};io.DeltaTime=1.f/60;unsigned char *pixels=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
 for(unsigned i=0;i<5;i++){ImGui::NewFrame();ImGui::SetNextWindowSize({500,650});ImGui::Begin("Animation harness");assert(ImGui::BeginTabBar("Tools"));p.animation_panel({});ImGui::EndTabBar();ImGui::End();ImGui::Render();assert(p.tab==21);}
 ImGui::DestroyContext(context);
 Preview real;real.project_directory=std::filesystem::canonical("projects/valwood_world").string();real.animation_load();
 assert(real.animation_ready&&real.animation_asset.skeleton.joints.size()==26&&real.animation_asset.skeleton.palette.size()==24&&real.animation_asset.clips.size()==3);
 for(unsigned clip=0;clip<real.animation_asset.clips.size();clip++){
  real.animation_playback={};real.animation_playback.clip=clip;
  assert(real.animation_sample_preview(real.animation_asset.clips[clip].duration*.5));
  real.animation_measure_bounds();assert(real.animation_measured_bounds);
  for(unsigned joint=0;joint<real.animation_pose.joints;joint++)for(float v:real.animation_pose.skin[joint].m)assert(std::isfinite(v));
 }
 std::puts("Existing knight inspector:26 nodes,24 joints,3 clips, actual pose matrices and deformed bounds passed");
 std::puts("Animation inspector: real GLB load, pose/blend/crossfade/bounds, path rejection, previous asset retention,10k allocation-free samples and ImGui bone/socket panel passed");
}
