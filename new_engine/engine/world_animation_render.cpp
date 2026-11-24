#define GL_GLEXT_PROTOTYPES
#include "engine/world_animation_render.h"
#include "engine/world_program_epoch.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include "stb_image.h"
#include "gl_helpers.h"
#include "engine/world_model_texture_blocks.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace world_animation_render {
using namespace world_animation;
namespace {
struct GPUInstance {float x,y,z,scale,yaw;std::uint32_t palette;};
struct GPUMaterial {Material values;std::array<GLuint,4> maps={};std::array<bool,4> owned={},present={};};
struct Locations {
 GLuint program=0;unsigned long long epoch=0;
 GLint palette=-1,joints=-1,base=-1,normal=-1,arm=-1,emissive=-1,has_base=-1,has_normal=-1,has_arm=-1,has_emissive=-1;
 GLint base_factor=-1,roughness=-1,metallic=-1,normal_scale=-1,cutoff=-1,emissive_factor=-1,reactive=-1;
 GLint grass=-1,vegetation=-1,wind=-1,instance_fade=-1,facade=-1,lod=-1,distance=-1,near_fade=-1,graph=-1;
};
void restore_bindings(const Bindings &b){
 glBindVertexArray(b.vao);glBindBuffer(GL_ARRAY_BUFFER,b.array_buffer);glBindBuffer(GL_TEXTURE_BUFFER,b.texture_buffer);
 for(unsigned i=0;i<4;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,b.textures[i]);glBindSampler(i,b.samplers[i]);}
 glActiveTexture(GL_TEXTURE0+palette_unit);glBindTexture(GL_TEXTURE_BUFFER,b.palette_texture);glBindSampler(palette_unit,b.palette_sampler);
 glActiveTexture(b.active_texture);if(b.cull_face)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
}
struct LoadingGuard {Bindings bindings;LoadingGuard():bindings(capture_bindings()){}~LoadingGuard(){restore_bindings(bindings);}};
void delete_texture(unsigned texture){const GLuint id=texture;glDeleteTextures(1,&id);}
void release(GLuint &texture){if(texture&&!texture_cache_release(texture))glDeleteTextures(1,&texture);texture=0;}
unsigned upload_encoded(const void *encoded,std::size_t length,const TextureCachePolicy *policy){
 world_model_texture_blocks::UnpackScope unpack;
 GLuint texture=(policy->mip_policy&world_model_texture_blocks::policy_bit)?world_model_texture_blocks::upload(encoded,length):0;
 if(!texture)texture=policy->srgb?GLH_texture_mem_srgb(encoded,int(length)):GLH_texture_mem(encoded,int(length));
 if(!texture)return 0;
 glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GLint(policy->min_filter));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GLint(policy->mag_filter));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GLint(policy->wrap_s));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GLint(policy->wrap_t));
 const auto anisotropy=(policy->mip_policy>>8)&255u;if(anisotropy>1)glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,float(anisotropy));
 if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture);return 0;}return texture;
}
unsigned upload_neutral(const void *encoded,std::size_t length,const TextureCachePolicy *policy){
 if(length!=8||std::memcmp(encoded,"VWN\1",4))return 0;
 world_model_texture_blocks::UnpackScope unpack;GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);const auto *bytes=static_cast<const unsigned char *>(encoded);
 glTexImage2D(GL_TEXTURE_2D,0,policy->srgb?GL_SRGB8_ALPHA8:GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,bytes+4);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GLint(policy->min_filter));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GLint(policy->mag_filter));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GLint(policy->wrap_s));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GLint(policy->wrap_t));
 if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture);return 0;}return texture;
}
bool matrix_valid(const Mat4 &m){return std::all_of(m.m.begin(),m.m.end(),[](float f){return std::isfinite(f)&&std::abs(f)<=1.e12f;})&&std::abs(m.m[3])<.001f&&std::abs(m.m[7])<.001f&&std::abs(m.m[11])<.001f&&std::abs(m.m[15]-1)<.001f;}
Mat4 product(const Mat4 &a,const Mat4 &b){Mat4 out;out.m.fill(0);for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++)for(unsigned k=0;k<4;k++)out.m[c*4+r]+=a.m[k*4+r]*b.m[c*4+k];return out;}
bool same_sampler(const TextureMap &a,const TextureMap &b){return a.min_filter==b.min_filter&&a.mag_filter==b.mag_filter&&a.wrap_s==b.wrap_s&&a.wrap_t==b.wrap_t;}
struct Size {int w=0,h=0;};
bool image_sizes(const Asset &asset,std::array<Size,128> &sizes,std::string &error){
 GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
 for(unsigned i=0;i<asset.images.size();i++){int channels=0;const auto &bytes=asset.images[i].encoded;auto &s=sizes[i];if(!stbi_info_from_memory(bytes.data(),int(bytes.size()),&s.w,&s.h,&channels)||s.w<=0||s.h<=0||s.w>maximum||s.h>maximum||s.w>4096||s.h>4096||std::uint64_t(s.w)*s.h>16*1024*1024){error="Invalid/oversized embedded skin image (4096 maximum side)";return false;}}
 return true;
}
std::uint64_t mip_bytes(Size size){std::uint64_t bytes=0;while(true){bytes+=std::uint64_t(size.w)*size.h*4;if(size.w==1&&size.h==1)break;size.w=std::max(1,size.w/2);size.h=std::max(1,size.h/2);}return bytes;}
bool derived_arm(const Material &m){return (m.metallic_roughness.image>=0||m.occlusion.image>=0)&&!(m.metallic_roughness.image>=0&&m.metallic_roughness.image==m.occlusion.image&&m.occlusion_strength==1);}
bool texture_budget(const Asset &asset,const std::array<Size,128> &sizes,std::uint64_t budget,std::uint64_t &bytes,std::string &error){
 std::array<std::array<bool,2>,128> used={};bytes=0;
 for(const auto &m:asset.materials){for(const auto *map:{&m.base,&m.normal,&m.emissive})if(map->image>=0)used[unsigned(map->image)][map==&m.normal?0:1]=true;
  if(m.metallic_roughness.image>=0&&m.occlusion.image>=0&&!same_sampler(m.metallic_roughness,m.occlusion)){error="AO and metallic-roughness need matching UV0 sampler policies";return false;}
  if(derived_arm(m)){const int image=m.metallic_roughness.image>=0?m.metallic_roughness.image:m.occlusion.image;bytes+=mip_bytes(sizes[unsigned(image)]);}else if(m.metallic_roughness.image>=0)used[unsigned(m.metallic_roughness.image)][0]=true;
 }
 for(unsigned i=0;i<asset.images.size();i++)for(unsigned space=0;space<2;space++)if(used[i][space])bytes+=mip_bytes(sizes[i]);
 if(bytes>budget){error="Skinned material texture estimate exceeds this import's budget";return false;}return true;
}
std::vector<unsigned char> pack_arm(const Asset &asset,const Material &m,std::string &error){
 const int mi=m.metallic_roughness.image,ai=m.occlusion.image;int mw=1,mh=1,aw=1,ah=1,c=0;using Pixels=std::unique_ptr<stbi_uc,void(*)(void *)>;Pixels mr(nullptr,stbi_image_free),ao(nullptr,stbi_image_free);
 if(mi>=0){const auto &image=asset.images[unsigned(mi)].encoded;mr.reset(stbi_load_from_memory(image.data(),int(image.size()),&mw,&mh,&c,4));if(!mr){error="Cannot decode metallic-roughness image";return {};}}
 if(ai>=0){const auto &image=asset.images[unsigned(ai)].encoded;ao.reset(stbi_load_from_memory(image.data(),int(image.size()),&aw,&ah,&c,4));if(!ao){error="Cannot decode occlusion image";return {};}}
 const int w=mi>=0?mw:aw,h=mi>=0?mh:ah;std::vector<unsigned char> tga(18+std::size_t(w)*h*4,0);tga[2]=2;tga[12]=static_cast<unsigned char>(w);tga[13]=static_cast<unsigned char>(w>>8);tga[14]=static_cast<unsigned char>(h);tga[15]=static_cast<unsigned char>(h>>8);tga[16]=32;tga[17]=0x28;
 auto address=[](int i,int count,unsigned wrap){if(wrap==GL_CLAMP_TO_EDGE)return std::clamp(i,0,count-1);const int period=wrap==GL_MIRRORED_REPEAT?count*2:count;int at=i%period;if(at<0)at+=period;return at>=count?period-1-at:at;};
 auto sample_ao=[&](int x,int y){
  const double sx=(x+.5)*aw/w-.5,sy=(y+.5)*ah/h-.5;
  auto red=[&](int px,int py){return double(ao.get()[(std::size_t(address(py,ah,m.occlusion.wrap_t))*aw+address(px,aw,m.occlusion.wrap_s))*4]);};
  if(m.occlusion.mag_filter==GL_NEAREST)return red(int(std::floor(sx+.5)),int(std::floor(sy+.5)));
  const int ix=int(std::floor(sx)),iy=int(std::floor(sy));const double dx=sx-ix,dy=sy-iy;return (red(ix,iy)*(1-dx)+red(ix+1,iy)*dx)*(1-dy)+(red(ix,iy+1)*(1-dx)+red(ix+1,iy+1)*dx)*dy;
 };
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){const auto out=18+(std::size_t(y)*w+x)*4,at=(std::size_t(y)*mw+x)*4;unsigned char a=255,g=255,b=255;if(mr){g=mr.get()[at+1];b=mr.get()[at+2];}if(ao)a=static_cast<unsigned char>(std::lround(255*(1-m.occlusion_strength)+sample_ao(x,y)*m.occlusion_strength));tga[out]=b;tga[out+1]=g;tga[out+2]=a;tga[out+3]=255;}
 return tga;
}
}

struct Renderer {
 GLuint vbo=0,ebo=0,vao[2]={},palette_buffer[2]={},palette_texture[2]={},instance_buffer[2]={},neutral[3]={};
 unsigned active=0,node_count=0,mesh_node=0,default_material=0;std::vector<GPUMaterial> materials;std::vector<Primitive> primitives;
 std::vector<GPUInstance> instance_staging;std::vector<float> palette_staging;std::array<Locations,8> programs={};Stats statistics;
 ~Renderer(){for(auto &m:materials)for(unsigned i=0;i<4;i++)if(m.owned[i])release(m.maps[i]);for(auto &id:neutral)release(id);glDeleteVertexArrays(2,vao);glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ebo);glDeleteBuffers(2,palette_buffer);glDeleteBuffers(2,instance_buffer);glDeleteTextures(2,palette_texture);}
};
Bindings capture_bindings(){
 Bindings b;GLint value=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&value);b.vao=unsigned(value);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&value);b.array_buffer=unsigned(value);glGetIntegerv(GL_TEXTURE_BUFFER_BINDING,&value);b.texture_buffer=unsigned(value);glGetIntegerv(GL_ACTIVE_TEXTURE,&value);b.active_texture=unsigned(value);b.cull_face=glIsEnabled(GL_CULL_FACE)!=0;
 for(unsigned i=0;i<4;i++){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&value);b.textures[i]=unsigned(value);glGetIntegerv(GL_SAMPLER_BINDING,&value);b.samplers[i]=unsigned(value);}glActiveTexture(GL_TEXTURE0+palette_unit);glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&value);b.palette_texture=unsigned(value);glGetIntegerv(GL_SAMPLER_BINDING,&value);b.palette_sampler=unsigned(value);glActiveTexture(b.active_texture);return b;
}
namespace {
TextureCachePolicy policy(const TextureMap &map,bool srgb,bool opaque){TextureCachePolicy p{srgb,map.min_filter,map.mag_filter,map.wrap_s,map.wrap_t,1};if(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")){float maximum=1;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&maximum);if(std::isfinite(maximum)&&maximum>=2)p.mip_policy|=unsigned(std::min(4.f,maximum))<<8;}if(world_model_texture_blocks::available(srgb,opaque))p.mip_policy|=world_model_texture_blocks::policy_bit;return p;}
bool acquire_material(Renderer &r,const Asset &asset,const Material &m,std::string &error){
 GPUMaterial gpu;gpu.values=m;gpu.maps={r.neutral[0],r.neutral[2],r.neutral[1],r.neutral[0]};const TextureMap *maps[]={&m.base,&m.normal,&m.metallic_roughness,&m.emissive};
 for(unsigned role=0;role<4;role++){
  const auto &map=*maps[role];std::vector<unsigned char> packed;const std::vector<unsigned char> *encoded=nullptr;TextureMap sampler=map;
  if(role==2&&derived_arm(m)){packed=pack_arm(asset,m,error);if(packed.empty())goto fail;encoded=&packed;if(sampler.image<0)sampler=m.occlusion;}
  else if(map.image>=0)encoded=&asset.images[unsigned(map.image)].encoded;
  if(!encoded)continue;
  gpu.present[role]=true;const auto p=policy(sampler,role==0||role==3,m.alpha_cutoff<=0&&role==0);gpu.maps[role]=texture_cache_acquire(encoded->data(),encoded->size(),&p,upload_encoded,delete_texture);if(!gpu.maps[role]){error="Cannot acquire skinned material image";goto fail;}gpu.owned[role]=true;
 }
 r.materials.push_back(gpu);return true;
 fail:for(unsigned i=0;i<4;i++)if(gpu.owned[i])release(gpu.maps[i]);return false;
}
}
Renderer *create(const Asset &asset,unsigned capacity,std::string &error,std::uint64_t budget){
 error.clear();if(!capacity||capacity>instance_limit||asset.skeleton.palette.empty()||asset.mesh.vertices.empty()||asset.mesh.indices.empty()||asset.mesh.primitives.empty()||!validate(asset,error)){if(error.empty())error="Invalid skinned renderer asset/capacity";return nullptr;}
 LoadingGuard guard;std::unique_ptr<Renderer> r;
 try{
  GLint texels=0,units=0;glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&texels);glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&units);if(texels<=0||std::uint64_t(capacity)*asset.skeleton.palette.size()*4>unsigned(texels)||units<=int(palette_unit)){error="GL3.3 joint texture-buffer/unit budget unavailable";return nullptr;}
  std::array<Size,128> sizes;std::uint64_t bytes=0;if(!image_sizes(asset,sizes,error)||!texture_budget(asset,sizes,budget,bytes,error))return nullptr;
  r=std::make_unique<Renderer>();r->node_count=unsigned(asset.skeleton.joints.size());r->mesh_node=asset.skeleton.mesh_node;r->statistics.capacity=capacity;r->statistics.joints=unsigned(asset.skeleton.palette.size());r->statistics.reference_texture_bytes=bytes;r->primitives=asset.mesh.primitives;r->materials.reserve(asset.materials.size()+1);r->instance_staging.resize(capacity);r->palette_staging.resize(std::size_t(capacity)*r->statistics.joints*16);
  glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);const unsigned char white[8]={'V','W','N',1,255,255,255,255},normal[8]={'V','W','N',1,128,128,255,255};for(unsigned i=0;i<3;i++){const TextureCachePolicy p{i==0,GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT,1};r->neutral[i]=texture_cache_acquire(i==2?normal:white,8,&p,upload_neutral,delete_texture);if(!r->neutral[i]){error="Cannot acquire skinned neutral sampler";return nullptr;}}
  world_model_texture_blocks::DirectoryScope directory(asset.source_path.c_str());for(const auto &m:asset.materials)if(!acquire_material(*r,asset,m,error))return nullptr;
  if(std::any_of(asset.mesh.primitives.begin(),asset.mesh.primitives.end(),[](const Primitive &p){return p.material<0;})){r->default_material=unsigned(r->materials.size());if(!acquire_material(*r,asset,Material{},error))return nullptr;}
  r->statistics.materials=unsigned(r->materials.size());
  glGenBuffers(1,&r->vbo);glGenBuffers(1,&r->ebo);glGenVertexArrays(2,r->vao);glGenBuffers(2,r->instance_buffer);glGenBuffers(2,r->palette_buffer);glGenTextures(2,r->palette_texture);glBindVertexArray(r->vao[0]);glBindBuffer(GL_ARRAY_BUFFER,r->vbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(asset.mesh.vertices.size()*sizeof(SkinVertex)),asset.mesh.vertices.data(),GL_STATIC_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,r->ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,GLsizeiptr(asset.mesh.indices.size()*sizeof(std::uint32_t)),asset.mesh.indices.data(),GL_STATIC_DRAW);
  r->statistics.geometry_bytes=asset.mesh.vertices.size()*sizeof(SkinVertex)+asset.mesh.indices.size()*sizeof(std::uint32_t);r->statistics.palette_bytes=2ull*capacity*r->statistics.joints*sizeof(Mat4);r->statistics.instance_bytes=2ull*capacity*sizeof(GPUInstance);
  for(unsigned bank=0;bank<2;bank++){
   glBindVertexArray(r->vao[bank]);glBindBuffer(GL_ARRAY_BUFFER,r->vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,r->ebo);
   const unsigned locations[]={0,1,2,3};const int components[]={3,3,2,4};const std::size_t offsets[]={offsetof(SkinVertex,position),offsetof(SkinVertex,normal),offsetof(SkinVertex,uv),offsetof(SkinVertex,tangent)};for(unsigned i=0;i<4;i++){glEnableVertexAttribArray(locations[i]);glVertexAttribPointer(locations[i],components[i],GL_FLOAT,GL_FALSE,sizeof(SkinVertex),reinterpret_cast<const void *>(offsets[i]));}
   glEnableVertexAttribArray(7);glVertexAttribIPointer(7,4,GL_UNSIGNED_INT,sizeof(SkinVertex),reinterpret_cast<const void *>(offsetof(SkinVertex,joints)));glEnableVertexAttribArray(8);glVertexAttribPointer(8,4,GL_FLOAT,GL_FALSE,sizeof(SkinVertex),reinterpret_cast<const void *>(offsetof(SkinVertex,weights)));
   glBindBuffer(GL_ARRAY_BUFFER,r->instance_buffer[bank]);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(capacity*sizeof(GPUInstance)),nullptr,GL_DYNAMIC_DRAW);glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),nullptr);glVertexAttribDivisor(4,1);glEnableVertexAttribArray(5);glVertexAttribPointer(5,1,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),reinterpret_cast<const void *>(offsetof(GPUInstance,yaw)));glVertexAttribDivisor(5,1);glEnableVertexAttribArray(6);glVertexAttribIPointer(6,1,GL_UNSIGNED_INT,sizeof(GPUInstance),reinterpret_cast<const void *>(offsetof(GPUInstance,palette)));glVertexAttribDivisor(6,1);
   glBindBuffer(GL_TEXTURE_BUFFER,r->palette_buffer[bank]);glBufferData(GL_TEXTURE_BUFFER,GLsizeiptr(std::size_t(capacity)*r->statistics.joints*sizeof(Mat4)),nullptr,GL_DYNAMIC_DRAW);glActiveTexture(GL_TEXTURE0+palette_unit);glBindTexture(GL_TEXTURE_BUFFER,r->palette_texture[bank]);glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA32F,r->palette_buffer[bank]);
  }
  if(glGetError()!=GL_NO_ERROR){error="Cannot allocate skinned mesh/batch GL stores";return nullptr;}return r.release();
 }catch(const std::exception &e){error="Skinned renderer allocation failed: "+std::string(e.what());return nullptr;}
}
void destroy(Renderer *r){if(!r)return;auto b=capture_bindings();delete r;if(b.vao&&!glIsVertexArray(b.vao))b.vao=0;if(b.array_buffer&&!glIsBuffer(b.array_buffer))b.array_buffer=0;if(b.texture_buffer&&!glIsBuffer(b.texture_buffer))b.texture_buffer=0;for(auto &id:b.textures)if(id&&!glIsTexture(id))id=0;if(b.palette_texture&&!glIsTexture(b.palette_texture))b.palette_texture=0;restore_bindings(b);}
bool warm(Renderer *r,unsigned program,std::string &error){
 error.clear();if(!r||!program||!glIsProgram(program)){error="Invalid skinned draw program";return false;}GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked){error="Unlinked skinned draw program";return false;}
 Locations *slot=nullptr;
 // Replacement invalidates every prior epoch, including cached locations for
 // recycled GL program IDs. Reuse stale storage only at this loading boundary.
 for(auto &p:r->programs)if(p.program==program&&p.epoch==world_program_epoch){slot=&p;break;}
 if(!slot)for(auto &p:r->programs)if(!p.program){slot=&p;break;}
 if(!slot)for(auto &p:r->programs)if(p.epoch!=world_program_epoch){slot=&p;break;}
 if(!slot){error="Skinned program cache capacity reached";return false;}
 Locations l;l.program=program;l.epoch=world_program_epoch;auto at=[&](const char *name){return glGetUniformLocation(program,name);};l.palette=at("uSkinPalette");l.joints=at("uJointCount");if(l.palette<0||l.joints<0){error="Program does not expose the skeletal vertex contract";return false;}
 l.base=at("uBase");l.normal=at("uNormal");l.arm=at("uARM");l.emissive=at("uEmissive");l.has_base=at("uHasBase");l.has_normal=at("uHasNormal");l.has_arm=at("uHasARM");l.has_emissive=at("uHasEmissive");l.base_factor=at("uBaseFactor");l.roughness=at("uRoughness");l.metallic=at("uMetallic");l.normal_scale=at("uNormalScale");l.cutoff=at("uAlphaCutoff");l.emissive_factor=at("uEmissiveFactor");l.reactive=at("uActorReactive");l.grass=at("uGrass");l.vegetation=at("uVegetation");l.wind=at("uWind");l.instance_fade=at("uInstanceFade");l.facade=at("uFacadeMode");l.lod=at("uPropLodRange");l.distance=at("uSceneryDistanceFade");l.near_fade=at("uTreeNearFade");l.graph=at("uStudioGraphIndex");*slot=l;return true;
}
bool upload(Renderer *r,const Pose *poses,const Instance *instances,std::size_t count,const Bindings &restore,std::string &error){
 error.clear();auto fail=[&](const char *message){if(r)r->statistics.failures++;error=message;return false;};if(!r||count>r->statistics.capacity||(count&&(!poses||!instances)))return fail("Invalid skinned batch capacity/pointers");
 for(std::size_t i=0;i<count;i++){
  const auto &p=poses[i];const auto &v=instances[i];if(p.nodes!=r->node_count||p.joints!=r->statistics.joints||!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.yaw)||!std::isfinite(v.scale)||std::abs(v.x)>1.e8||std::abs(v.y)>1.e8||std::abs(v.z)>1.e8||v.scale<=0||v.scale>100||!matrix_valid(p.global[r->mesh_node]))return fail("Invalid skinned instance/pose");
  r->instance_staging[i]={v.x,v.y,v.z,v.scale,v.yaw,std::uint32_t(i*r->statistics.joints*4)};for(unsigned j=0;j<r->statistics.joints;j++){if(!matrix_valid(p.skin[j]))return fail("Invalid skin palette matrix");const auto matrix=product(p.global[r->mesh_node],p.skin[j]);if(!matrix_valid(matrix))return fail("Composed skin palette overflow");std::copy(matrix.m.begin(),matrix.m.end(),r->palette_staging.begin()+std::ptrdiff_t((i*r->statistics.joints+j)*16));}
 }
 if(!count){r->statistics.instances=0;r->statistics.uploads++;return true;}
 const unsigned bank=1-r->active;glBindBuffer(GL_TEXTURE_BUFFER,r->palette_buffer[bank]);glBufferSubData(GL_TEXTURE_BUFFER,0,GLsizeiptr(count*r->statistics.joints*sizeof(Mat4)),r->palette_staging.data());glBindBuffer(GL_ARRAY_BUFFER,r->instance_buffer[bank]);glBufferSubData(GL_ARRAY_BUFFER,0,GLsizeiptr(count*sizeof(GPUInstance)),r->instance_staging.data());const bool ok=glGetError()==GL_NO_ERROR;restore_bindings(restore);if(!ok)return fail("Skinned batch upload failed; old committed bank retained");r->active=bank;r->statistics.instances=unsigned(count);r->statistics.uploads++;return true;
}
bool draw(Renderer *r,unsigned program,const Bindings &restore,std::string &error){
 error.clear();if(!r){error="Missing skinned renderer";return false;}
 if(restore.studio_graph_index< -1||restore.studio_graph_index>7){r->statistics.failures++;error="Skinned material graph index must be -1 through 7";return false;}
 const Locations *l=nullptr;for(const auto &p:r->programs)if(p.program==program&&p.epoch==world_program_epoch){l=&p;break;}if(!l){r->statistics.failures++;error="Warm the skinned program after loading/relink before drawing";return false;}
 if(!r->statistics.instances){glUniform1i(l->graph,-1);return true;}
 glUniform1i(l->palette,palette_unit);glUniform1i(l->joints,GLint(r->statistics.joints));glUniform1i(l->base,0);glUniform1i(l->normal,1);glUniform1i(l->arm,2);glUniform1i(l->emissive,3);glUniform1i(l->grass,0);glUniform1i(l->vegetation,0);glUniform1f(l->wind,0);glUniform1i(l->instance_fade,0);glUniform1i(l->facade,0);glUniform3f(l->lod,0,0,0);glUniform2f(l->distance,0,0);glUniform2f(l->near_fade,0,0);glUniform1i(l->graph,restore.studio_graph_index);glUniform1f(l->reactive,.85f);
 glActiveTexture(GL_TEXTURE0+palette_unit);glBindTexture(GL_TEXTURE_BUFFER,r->palette_texture[r->active]);glBindSampler(palette_unit,0);glBindVertexArray(r->vao[r->active]);
 for(const auto &primitive:r->primitives){const auto &m=r->materials[primitive.material>=0?unsigned(primitive.material):r->default_material];if(m.values.double_sided||!restore.cull_face)glDisable(GL_CULL_FACE);else glEnable(GL_CULL_FACE);for(unsigned i=0;i<4;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,m.maps[i]);glBindSampler(i,0);}glUniform1i(l->has_base,m.present[0]);glUniform1i(l->has_normal,m.present[1]);glUniform1i(l->has_arm,m.present[2]);glUniform1i(l->has_emissive,m.present[3]);glUniform4fv(l->base_factor,1,m.values.base_factor.data());glUniform1f(l->roughness,m.values.roughness);glUniform1f(l->metallic,m.values.metallic);glUniform1f(l->normal_scale,m.values.normal_scale);glUniform1f(l->cutoff,m.values.alpha_cutoff);glUniform3fv(l->emissive_factor,1,m.values.emissive_factor.data());glDrawElementsInstanced(GL_TRIANGLES,GLsizei(primitive.index_count),GL_UNSIGNED_INT,reinterpret_cast<const void *>(primitive.first_index*sizeof(std::uint32_t)),GLsizei(r->statistics.instances));r->statistics.draws++;}
 glUniform1i(l->graph,-1);glUniform1f(l->reactive,0);glUniform1f(l->normal_scale,1);const bool ok=glGetError()==GL_NO_ERROR;restore_bindings(restore);if(!ok){r->statistics.failures++;error="Skinned draw GL error";}return ok;
}
Stats stats(const Renderer *r){return r?r->statistics:Stats{};}
bool material_textures(const Renderer *r,unsigned material,MaterialTextures &out){if(!r||material>=r->materials.size())return false;out.maps=r->materials[material].maps;return true;}
}
