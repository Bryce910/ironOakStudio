// CPU only: genuine owned GLBs plus hostile/mismatched contract boundaries.
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "engine/world_model_hair.h"
#include "engine/world_model_skin.h"
#include "engine/world_animation.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <iostream>
#include <limits>

using Data=std::unique_ptr<cgltf_data,decltype(&cgltf_free)>;
static Data parse(const std::filesystem::path &path){
 cgltf_options options={};cgltf_data *data=nullptr;
 if(cgltf_parse_file(&options,path.c_str(),&data)!=cgltf_result_success){std::cerr<<"parse failed: "<<path<<"\n";std::abort();}
 return Data(data,cgltf_free);
}
static void boundaries(){
 using namespace world_model_hair;
 assert(knight_id("knight01")&&knight_id("knight18")&&!knight_id("knight00")&&!knight_id("knight19")&&!knight_id("knight011"));
 assert(pose_frame("combat_body_000.glb")&&pose_frame("combat_body_171.glb")&&!pose_frame("combat_body_172.glb")&&!pose_frame("combat_body_001.glb.bak"));
 cgltf_texture texture={};cgltf_material m={};m.name=const_cast<char*>("MedievalCharacter_Pilosity");m.alpha_mode=cgltf_alpha_mode_mask;m.alpha_cutoff=.029f;
 auto &pbr=m.pbr_metallic_roughness;pbr.base_color_texture.texture=&texture;pbr.metallic_roughness_texture.texture=&texture;
 pbr.metallic_factor=1;pbr.roughness_factor=1;for(unsigned i=0;i<3;++i)pbr.base_color_factor[i]=.54245275f;pbr.base_color_factor[3]=1;
 m.normal_texture.texture=&texture;m.occlusion_texture.texture=&texture;
 cgltf_primitive p={};p.material=&m;
 assert(skeletal("/owned/knights_roster/knight12/body.glb",&m)==KnightBlend);
 for(const char *variant:{"civilian","armored"}){
  const std::string path=std::string("/owned/story_hero/")+variant+"/body.glb";
  assert(skeletal(path.c_str(),&m)==KnightBlend);
 }
 for(const char *path:{"/props/civilian/body.glb","/owned/story_hero/other/body.glb","/owned/story_hero/civilian/prop.glb"})assert(!skeletal(path,&m));
 assert(posed("/owned/npc_knights/knight12/body/combat_body_000.glb",true,p)==KnightBlend);
 assert(!skeletal("/props/knight12/body.glb",&m)&&!skeletal("/owned/knights_roster/knight19/body.glb",&m));
 assert(!posed("/owned/npc_knights/knight12/body/combat_body_000.glb",false,p));
 assert(!posed("/owned/npc_knights/knight12/body/combat_body_172.glb",true,p));
 m.alpha_cutoff=.5f;assert(!skeletal("/owned/knights_roster/knight12/body.glb",&m));m.alpha_cutoff=.029f;
 m.alpha_cutoff=std::numeric_limits<float>::quiet_NaN();assert(!skeletal("/owned/knights_roster/knight12/body.glb",&m));m.alpha_cutoff=.029f;
 cgltf_material head=m;head.name=const_cast<char*>("KnightHead01");head.alpha_mode=cgltf_alpha_mode_opaque;cgltf_primitive head_primitive={};head_primitive.material=&head;
 assert(world_model_skin::skeletal("/owned/knights_roster/knight12/body.glb",&head));
 assert(world_model_skin::posed("/owned/npc_knights/knight12/body/combat_body_000.glb",true,head_primitive));
 assert(!world_model_skin::skeletal("/props/knight12/body.glb",&head));
 assert(world_model_skin::skeletal("/owned/story_hero/civilian/body.glb",&head));
 assert(world_model_skin::skeletal("/owned/story_hero/armored/body.glb",&head));
 assert(!world_model_skin::skeletal("/owned/story_hero/other/body.glb",&head));
 assert(!world_model_skin::posed("/owned/arena_orcs_roster/knight12/body/combat_body_000.glb",true,head_primitive));
 for(const char *name:{"Knight Eyes","Knight Eyes_Blue","KnightHMouth","MedievalCharacter_Pilosity","VALWOOD_EQUIPMENT:32 NPC_Characters_04_Hands","KnightHead01_wrong"}){
  head.name=const_cast<char*>(name);assert(!world_model_skin::skeletal("/owned/knights_roster/knight12/body.glb",&head));
 }
 head.name=const_cast<char*>("KnightHead01");head.alpha_mode=cgltf_alpha_mode_mask;assert(!world_model_skin::skeletal("/owned/knights_roster/knight12/body.glb",&head));
 m.name=const_cast<char*>("Armor01_Bascinet01_ChainMail");assert(!skeletal("/owned/knights_roster/knight12/body.glb",&m));
 m.name=const_cast<char*>("Owned Unity Orc Orc 01 Orc_01_Hair");m.alpha_cutoff=.5f;
 p.extras.data=const_cast<char*>("{\"source_object\":\"Orc_01_Beard\",\"rigid_weapon\":false}");
 assert(posed("/owned/arena_orcs_roster/orc01/body/combat_body_000.glb",true,p)==OrcMask);
 assert(!posed("/props/orc01/body/combat_body_000.glb",true,p));
 assert(!posed("/owned/arena_orcs_roster/orc02/body/combat_body_000.glb",true,p));
 p.extras.data=const_cast<char*>("{\"source_object\":\"Orc_01_Armor_01_Mesh\",\"rigid_weapon\":false}");assert(!posed("/owned/arena_orcs_roster/orc01/body/combat_body_000.glb",true,p));
 p.extras.data=const_cast<char*>("{\"source_object\":\"Orc_01_Beard\",\"rigid_weapon\":true}");assert(!posed("/owned/arena_orcs_roster/orc01/body/combat_body_000.glb",true,p));
 p.extras.data=const_cast<char*>("{\"source_object\":\"Orc_01_Beard\",\"source_object\":\"Orc_01_Beard\",\"rigid_weapon\":false}");assert(!posed("/owned/arena_orcs_roster/orc01/body/combat_body_000.glb",true,p));
 p.extras.data=const_cast<char*>("{\"source_object\":\"Orc_01_\\u0042eard\",\"rigid_weapon\":false}");assert(!posed("/owned/arena_orcs_roster/orc01/body/combat_body_000.glb",true,p));
}
int main(int argc,char **argv){
 assert(argc==2);boundaries();const std::filesystem::path project=argv[1];
 unsigned body_files=0,posed_files=0,material_count=0,knight_hair_materials=0,posed_knight_hair=0,posed_orc_hair=0,ordinary=0,organic_head_materials=0,posed_organic_heads=0;
 auto head_name=[](const char *name){const std::string_view s=name?name:"";return s=="KnightHead01"||s=="KnightHead02"||s=="KnightHead03"||s=="KnightHead04";};
 // Exercise actual skeletal loading, not only fabricated material structs.
 for(unsigned i=1;i<=18;++i){
  char id[16];std::snprintf(id,sizeof id,"knight%02u",i);const auto path=project/"knights_roster"/id/"body.glb";
  auto document=parse(path);world_animation::Asset asset;std::string error;
  if(!world_animation::load_glb(path.c_str(),asset,error)){std::cerr<<path<<": "<<error<<"\n";return 1;}
  assert(asset.materials.size()==document->materials_count);++body_files;
  for(unsigned m=0;m<document->materials_count;++m){
   const bool expected=document->materials[m].name&&std::string_view(document->materials[m].name)=="MedievalCharacter_Pilosity";
   assert(asset.materials[m].hair_material==(expected?2u:0u));
   const bool head=head_name(document->materials[m].name);assert(asset.materials[m].organic_body==head);organic_head_materials+=head;
   knight_hair_materials+=expected;++material_count;
   if(expected){auto invalid=asset;invalid.materials[m].hair_material=3;assert(!world_animation::validate(invalid,error));}
  }
  for(unsigned frame:{0u,171u}){
   char filename[40];std::snprintf(filename,sizeof filename,"combat_body_%03u.glb",frame);
   const auto pose=project/"npc_knights"/id/"body"/filename;auto data=parse(pose);++posed_files;
   for(cgltf_size mi=0;mi<data->meshes_count;++mi)for(cgltf_size pi=0;pi<data->meshes[mi].primitives_count;++pi){
    const auto &p=data->meshes[mi].primitives[pi];const bool expected=p.material&&p.material->name&&std::string_view(p.material->name)=="MedievalCharacter_Pilosity";
    const unsigned kind=world_model_hair::posed(pose.c_str(),true,p);assert(kind==(expected?2u:0u));posed_knight_hair+=expected;ordinary+=!expected;
    const bool head=p.material&&head_name(p.material->name);assert(world_model_skin::posed(pose.c_str(),true,p)==head);posed_organic_heads+=head;
   }
  }
 }
 const char *ids[]={"orc01","orc02","orc03","orc04","orc05","orc06","azrak","costalmed","dardan","girra","gnur","kanit","magur","ursag"};
 for(const char *id:ids)for(unsigned frame:{0u,171u}){
  char filename[40];std::snprintf(filename,sizeof filename,"combat_body_%03u.glb",frame);const auto pose=project/"arena_orcs_roster"/id/"body"/filename;
  auto data=parse(pose);++posed_files;
  for(cgltf_size mi=0;mi<data->meshes_count;++mi)for(cgltf_size pi=0;pi<data->meshes[mi].primitives_count;++pi){
   const auto &p=data->meshes[mi].primitives[pi];const unsigned kind=world_model_hair::posed(pose.c_str(),true,p);
   const std::string_view name=p.material&&p.material->name?p.material->name:"";
   const bool expected=name=="Owned Unity Orc Orc 01 Orc_01_Hair"||name=="Owned Unity Orc Orc 04 Orc_04_Hair"||name=="Owned Unity Orc Orc 05 Haiir"||name=="Owned Unity Orc Orc 05 Haiir embedded hair"||name=="Owned Unity Orc Orc 06 Orc_06_Hair";
   assert(kind==(expected?1u:0u));posed_orc_hair+=expected;ordinary+=!expected;
   assert(!world_model_skin::posed(pose.c_str(),true,p));
  }
 }
 assert(body_files==18&&posed_files==64&&knight_hair_materials==12&&posed_orc_hair==10&&posed_knight_hair==24&&organic_head_materials==12&&posed_organic_heads==24);
 std::cout<<"{\"passed\":true,\"actual_skeletal_bodies_loaded\":"<<body_files<<",\"actual_skeletal_materials\":"<<material_count
          <<",\"source_blend_knight_materials\":"<<knight_hair_materials<<",\"actual_pose_files\":"<<posed_files
          <<",\"source_blend_knight_primitives\":"<<posed_knight_hair<<",\"source_mask_orc_primitives\":"<<posed_orc_hair
          <<",\"ordinary_primitives_unchanged\":"<<ordinary<<",\"organic_head_materials\":"<<organic_head_materials<<",\"posed_organic_head_primitives\":"<<posed_organic_heads<<",\"separate_eyes_mouth_hair_hands_excluded\":true,\"negative_contract_boundaries_passed\":true,\"invalid_material_kind_rejected\":true}\n";
}
