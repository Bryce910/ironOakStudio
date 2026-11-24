#ifndef VALWOOD_WORLD_ANIMATION_TEXTURE_BUDGET_PRIVATE_H
#define VALWOOD_WORLD_ANIMATION_TEXTURE_BUDGET_PRIVATE_H
// Loading-only implementation shared with the CPU roster test. No GL calls,
// runtime resampling, source mutations, or reads during drawing.
#include "engine/world_animation.h"
#include "engine/world_character_textures.h"
#include "stb_image.h"
#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace world_animation_texture_budget_private {
using namespace world_animation;
struct Size {int w=0,h=0;};
struct Selection {
 unsigned profile=0,downsampled_images=0,protected_images=0;
 std::uint64_t initial_bytes=0,bytes=0;
 std::array<unsigned,128> levels={};
 std::array<bool,128> protected_image={};
};
inline bool same_sampler(const TextureMap &a,const TextureMap &b){return a.min_filter==b.min_filter&&a.mag_filter==b.mag_filter&&a.wrap_s==b.wrap_s&&a.wrap_t==b.wrap_t;}
inline bool image_size(const std::vector<unsigned char> &bytes,int maximum,Size &s,std::string &error){
 int channels=0;
 if(bytes.size()>std::size_t(std::numeric_limits<int>::max())||!stbi_info_from_memory(bytes.data(),int(bytes.size()),&s.w,&s.h,&channels)||s.w<=0||s.h<=0||s.w>maximum||s.h>maximum||s.w>4096||s.h>4096||std::uint64_t(s.w)*s.h>16*1024*1024){error="Invalid/oversized embedded skin image (4096 maximum side)";return false;}
 return true;
}
inline bool image_sizes(const Asset &asset,int maximum,std::array<Size,128> &sizes,std::string &error){
 if(asset.images.size()>sizes.size()){error="Too many embedded skin images";return false;}
 for(unsigned i=0;i<asset.images.size();++i)if(!image_size(asset.images[i].encoded,maximum,sizes[i],error))return false;
 return true;
}
inline std::uint64_t mip_bytes(Size size){std::uint64_t bytes=0;while(true){bytes+=std::uint64_t(size.w)*size.h*4;if(size.w==1&&size.h==1)break;size.w=std::max(1,size.w/2);size.h=std::max(1,size.h/2);}return bytes;}
inline bool derived_arm(const Material &m){return (m.metallic_roughness.image>=0||m.occlusion.image>=0)&&!(m.metallic_roughness.image>=0&&m.metallic_roughness.image==m.occlusion.image&&m.occlusion_strength==1);}
inline bool texture_budget(const Asset &asset,const std::array<Size,128> &sizes,std::uint64_t budget,std::uint64_t &bytes,std::string &error){
 std::array<std::array<bool,2>,128> used={};bytes=0;
 if(asset.images.size()>used.size()){error="Too many embedded skin images";return false;}
 for(const auto &m:asset.materials){
  for(const auto *map:{&m.base,&m.normal,&m.emissive,&m.metallic_roughness,&m.occlusion})if(map->image< -1||map->image>=int(asset.images.size())){error="Invalid skinned material image index";return false;}
  for(const auto *map:{&m.base,&m.normal,&m.emissive})if(map->image>=0)used[unsigned(map->image)][map==&m.normal?0:1]=true;
  if(m.metallic_roughness.image>=0&&m.occlusion.image>=0&&!same_sampler(m.metallic_roughness,m.occlusion)){error="AO and metallic-roughness need matching UV0 sampler policies";return false;}
  if(derived_arm(m)){const int image=m.metallic_roughness.image>=0?m.metallic_roughness.image:m.occlusion.image;bytes+=mip_bytes(sizes[unsigned(image)]);}else if(m.metallic_roughness.image>=0)used[unsigned(m.metallic_roughness.image)][0]=true;
 }
 for(unsigned i=0;i<asset.images.size();++i)for(unsigned space=0;space<2;++space)if(used[i][space])bytes+=mip_bytes(sizes[i]);
 if(bytes>budget){error="Skinned material texture estimate exceeds this import's budget";return false;}return true;
}
inline bool variant(const Image &original,unsigned level,Image &out,std::string &error){
 world_character_textures::LoadingQuality scope(level);std::vector<unsigned char> replacement;
 if(!world_character_textures::replace(original.encoded.data(),original.encoded.size(),replacement,error))return false;
 out.encoded=replacement.empty()?original.encoded:std::move(replacement);return true;
}
inline bool prepare(const Asset &original,unsigned profile,std::uint64_t budget,int maximum,Asset &out,Selection &selection,std::string &error){
 error.clear();selection={};selection.profile=profile;
 if(profile>=world_character_textures::profile_count||original.images.size()>128||maximum<=0){error="Invalid skinned texture selection bounds";return false;}
 out.source_path=original.source_path;out.materials=original.materials;out.images.resize(original.images.size());
 std::array<Size,128> sizes={};
 std::array<std::pair<std::uint64_t,std::uint64_t>,128> identities={};
 std::array<bool,128> color_image={},data_image={};
 // Validate map indices before either protection or accounting accesses them.
 for(const auto &m:original.materials)for(const auto *map:{&m.base,&m.normal,&m.metallic_roughness,&m.occlusion,&m.emissive}){
  if(map->image< -1||map->image>=int(original.images.size())){error="Invalid skinned material image index";return false;}
  if(map->image>=0){
   const unsigned image=unsigned(map->image);
   const bool color=map==&m.base||map==&m.emissive;
   color_image[image]=color_image[image]||color;data_image[image]=data_image[image]||!color;
   // Old tiers preserve their exact protection contract. Ultra keeps face
   // and hair coverage color, but permits expensive surface-map fallback.
   if((m.organic_body||m.hair_material>0)&&(profile<3||map==&m.base))selection.protected_image[image]=true;
  }
 }
 for(unsigned i=0;i<original.images.size();++i){
  const auto &encoded=original.images[i].encoded;identities[i]={world_character_textures::fingerprint(encoded.data(),encoded.size()),encoded.size()};
  if(profile==3&&world_character_textures::known_hand_color(identities[i].first,identities[i].second))selection.protected_image[i]=true;
  selection.levels[i]=profile;selection.protected_images+=selection.protected_image[i];
  if(!variant(original.images[i],profile,out.images[i],error))return false;
 }
 if(!image_sizes(out,maximum,sizes,error)||!texture_budget(out,sizes,std::numeric_limits<std::uint64_t>::max(),selection.bytes,error))return false;
 selection.initial_bytes=selection.bytes;
 // Exhaust savings at High, then Medium, then Low for Ultra. Old tiers
 // retain their exact ordering. Ultra reduces data-only surface images
 // before other colors, preserving color detail without raising the ceiling.
 // Exhaust savings at Medium before considering Low. A stable original
 // content identity, then image index, breaks equal-saving ties. Every lookup
 // starts with the exact original bytes, never with a resized variant key.
 for(int target=int(profile)-1;selection.bytes>budget&&target>=0;--target){
  struct Candidate {unsigned image=0;Size size;std::uint64_t saving=0;};
  std::vector<Candidate> candidates;
  for(unsigned i=0;i<original.images.size();++i){
   if(selection.protected_image[i]||world_character_textures::entries.find(identities[i])==world_character_textures::entries.end())continue;
   Image lower;if(!variant(original.images[i],unsigned(target),lower,error))return false;
   Size lower_size;if(!image_size(lower.encoded,maximum,lower_size,error))return false;
   // Manifest variants cannot cause a lower tier to increase either side.
   if(lower_size.w>sizes[i].w||lower_size.h>sizes[i].h)continue;
   auto trial=sizes;trial[i]=lower_size;std::uint64_t estimate=0;
   if(!texture_budget(out,trial,std::numeric_limits<std::uint64_t>::max(),estimate,error))return false;
   if(estimate<selection.bytes)candidates.push_back({i,lower_size,selection.bytes-estimate});
  }
  std::sort(candidates.begin(),candidates.end(),[&](const Candidate &a,const Candidate &b){
   if(profile==3){const bool ad=data_image[a.image]&&!color_image[a.image],bd=data_image[b.image]&&!color_image[b.image];if(ad!=bd)return ad;}
   if(a.saving!=b.saving)return a.saving>b.saving;
   if(identities[a.image]!=identities[b.image])return identities[a.image]<identities[b.image];
   return a.image<b.image;
  });
  for(const auto &candidate:candidates){
   if(selection.bytes<=budget)break;
   const unsigned i=candidate.image;
   if(!variant(original.images[i],unsigned(target),out.images[i],error))return false;
   sizes[i]=candidate.size;selection.levels[i]=unsigned(target);
   if(!texture_budget(out,sizes,std::numeric_limits<std::uint64_t>::max(),selection.bytes,error))return false;
  }
 }
 for(unsigned i=0;i<original.images.size();++i)selection.downsampled_images+=selection.levels[i]<profile;
 // Unknown/unregistered images have no fallback; an asset that still exceeds
 // the ceiling fails closed, even if all registered choices were exhausted.
 return texture_budget(out,sizes,budget,selection.bytes,error);
}
}
#endif
