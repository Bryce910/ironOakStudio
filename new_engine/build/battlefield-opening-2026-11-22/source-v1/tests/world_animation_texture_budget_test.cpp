#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "engine/world_animation_texture_budget_private.h"
#include <cassert>
#include <cstdio>
#include <iostream>

namespace budget=world_animation_texture_budget_private;
namespace bank=world_character_textures;
using namespace world_animation;
static std::vector<std::uint64_t> hashes(const Asset &asset){
 std::vector<std::uint64_t> result;
 for(const auto &image:asset.images)result.push_back(bank::fingerprint(image.encoded.data(),image.encoded.size()));
 return result;
}
static void accounting(){
 Asset a;a.images.resize(2);Material m;m.base.image=0;m.normal.image=0;m.metallic_roughness.image=1;m.occlusion.image=1;a.materials={m};
 std::array<budget::Size,128> sizes={};sizes[0]={8,4};sizes[1]={4,2};std::string error;std::uint64_t bytes=0;
 assert(budget::texture_budget(a,sizes,10000,bytes,error));assert(bytes==2*budget::mip_bytes(sizes[0])+budget::mip_bytes(sizes[1]));
 // A derived ARM is allocated per material, not merely once per image.
 a.materials[0].occlusion_strength=.5f;a.materials.push_back(a.materials[0]);
 assert(budget::texture_budget(a,sizes,10000,bytes,error));assert(bytes==2*budget::mip_bytes(sizes[0])+2*budget::mip_bytes(sizes[1]));
 a.materials[0].occlusion.wrap_s=33071;assert(!budget::texture_budget(a,sizes,10000,bytes,error));
 Asset neutral;Asset out;budget::Selection selection;assert(budget::prepare(neutral,2,0,4096,out,selection,error));assert(!selection.bytes&&!selection.downsampled_images);
 neutral.images.resize(129);assert(!budget::prepare(neutral,2,100,4096,out,selection,error));
}
int main(int argc,char **argv){
 assert(argc==2);accounting();const std::filesystem::path project=argv[1];std::string error;
 if(!bank::configure((project/"character-textures").c_str(),2,error)){std::cerr<<error<<'\n';return 1;}
 assert(bank::entries.size()==673);unsigned cases=0,adapted=0,protected_checks=0;bool saw_high12=false,saw_medium12=false;
 std::cout<<"{\"passed\":true,\"bank_identities\":"<<bank::entries.size()<<",\"cases\":[";
 for(unsigned knight=1;knight<=18;++knight){
  char id[16];std::snprintf(id,sizeof id,"knight%02u",knight);Asset source;
  if(!load_glb((project/"knights_roster"/id/"body.glb").c_str(),source,error)){std::cerr<<error<<'\n';return 1;}
  const auto original_hashes=hashes(source);
  for(unsigned profile=0;profile<4;++profile){
   const std::uint64_t ceiling=(profile>=2?768ull:256ull)*1024*1024;Asset selected;budget::Selection choice;
   if(!budget::prepare(source,profile,ceiling,4096,selected,choice,error)){std::cerr<<id<<" profile="<<profile<<": "<<error<<'\n';return 1;}
   assert(choice.bytes<=ceiling&&bank::quality==2&&bank::loading_quality==bank::inherit_quality);assert(original_hashes==hashes(source));
   unsigned lowered=0;
   for(unsigned image=0;image<source.images.size();++image){
    assert(choice.levels[image]<=profile);lowered+=choice.levels[image]<profile;
    Image expected;assert(budget::variant(source.images[image],choice.levels[image],expected,error));
    assert(selected.images[image].encoded==expected.encoded);
    if(choice.protected_image[image]){assert(choice.levels[image]==profile);++protected_checks;}
   }
   assert(lowered==choice.downsampled_images);if(choice.initial_bytes<=ceiling)assert(!lowered);
   adapted+=lowered>0;
   if(knight==12&&profile==2){
    saw_high12=lowered>0;Asset repeated;budget::Selection again;
    assert(budget::prepare(source,profile,ceiling,4096,repeated,again,error));
    assert(choice.levels==again.levels&&choice.bytes==again.bytes&&hashes(selected)==hashes(repeated));
   }
   if(knight==12&&profile==1)saw_medium12=true;
   if(cases++)std::cout<<',';
   std::cout<<"{\"knight\":\""<<id<<"\",\"profile\":"<<profile<<",\"images\":"<<source.images.size()<<",\"protected_images\":"<<choice.protected_images<<",\"initial_bytes\":"<<choice.initial_bytes<<",\"bytes\":"<<choice.bytes<<",\"budget\":"<<ceiling<<",\"downsampled_images\":"<<lowered<<",\"levels\":[";
   for(unsigned image=0;image<source.images.size();++image){if(image)std::cout<<',';std::cout<<choice.levels[image];}std::cout<<"]}";
  }
  if(knight==12){
   Asset selected;budget::Selection choice;
   // Exercise Medium -> Low with a tighter explicit caller budget, even
   // though this actual roster's default 256 MiB Medium already fits.
   assert(budget::prepare(source,1,192ull*1024*1024,4096,selected,choice,error));assert(choice.downsampled_images>0);
   for(unsigned i=0;i<source.images.size();++i)if(choice.protected_image[i])assert(choice.levels[i]==1);
   // Exhausting every unprotected registered choice cannot shrink the face.
   assert(!budget::prepare(source,2,1,4096,selected,choice,error));
   for(unsigned i=0;i<source.images.size();++i)if(choice.protected_image[i])assert(choice.levels[i]==2);
   // No registry means no fallback. Never resize an unknown image to fit.
   auto entries=std::move(bank::entries);assert(!budget::prepare(source,2,1,4096,selected,choice,error));assert(!choice.downsampled_images);bank::entries=std::move(entries);
   {bank::LoadingQuality outer(1);assert(budget::prepare(source,0,256ull*1024*1024,4096,selected,choice,error));assert(bank::loading_quality==1);}assert(bank::loading_quality==bank::inherit_quality);
   // Registered corruption must fail even when the requested budget is large.
   const auto &image=source.images.front().encoded;auto found=bank::entries.find({bank::fingerprint(image.data(),image.size()),image.size()});assert(found!=bank::entries.end());
   found->second.files[0].hash^=1;assert(!budget::prepare(source,0,768ull*1024*1024,4096,selected,choice,error));found->second.files[0].hash^=1;
  }
 }
 assert(cases==72&&saw_high12&&saw_medium12&&adapted>0&&protected_checks>0);
 std::cout<<"],\"actual_roster_profiles\":"<<cases<<",\"adapted_profiles\":"<<adapted<<",\"protected_image_checks\":"<<protected_checks<<",\"high_and_medium_knight12_loadable\":true,\"medium_to_low_under_tighter_explicit_budget\":true,\"unchanged_source_bytes\":true,\"exact_registered_variant_bytes\":true,\"deterministic_selection\":true,\"unknown_and_corrupt_fail_closed\":true,\"derived_arm_and_color_space_accounting\":true,\"neutral_unchanged\":true,\"loading_scope_restored\":true}\n";
}
