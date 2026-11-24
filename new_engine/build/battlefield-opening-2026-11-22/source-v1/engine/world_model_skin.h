#ifndef VALWOOD_WORLD_MODEL_SKIN_H
#define VALWOOD_WORLD_MODEL_SKIN_H
#include "engine/world_model_hair.h"

namespace world_model_skin {
// The owned heads use independent skin materials. Eyes, wet mouth,
// Pilosity, hands, armor and combined arbitrary atlases are deliberately absent.
// Reuse the existing bounded organic response instead of clamping wet lips or
// changing the first-person hand material's established skin shader contract.
inline bool owned_head(const cgltf_material *material){
 if(!material||!material->name||material->alpha_mode!=cgltf_alpha_mode_opaque||
    material->unlit||material->has_transmission||material->has_pbr_specular_glossiness)return false;
 const std::string_view name=material->name;
 if(name!="KnightHead01"&&name!="KnightHead02"&&name!="KnightHead03"&&name!="KnightHead04"&&name!="Thomas_Thomas_Head"&&name!="CampHead01"&&name!="CampHead02"&&name!="CampHead04")return false;
 return material->pbr_metallic_roughness.base_color_texture.texture&&
        material->pbr_metallic_roughness.metallic_roughness_texture.texture&&
        material->normal_texture.texture&&material->occlusion_texture.texture;
}
inline bool skeletal(const char *path,const cgltf_material *material){
 if(!path||!owned_head(material))return false;
 const auto model=std::filesystem::path(path).lexically_normal(),character=model.parent_path();
 if(std::string_view(material->name)=="Thomas_Thomas_Head")return world_model_hair::story_body(model);
 if(character.filename()=="camp_people"){
  const auto name=std::string_view(material->name);const auto file=model.filename().string();
  return (file=="rowan.glb"&&name=="CampHead02")||(file=="bram.glb"&&name=="CampHead04")||(file=="ellis.glb"&&name=="CampHead01");
 }
 return world_model_hair::story_body(model)||(model.filename()=="body.glb"&&character.parent_path().filename()=="knights_roster"&&
        world_model_hair::knight_id(character.filename().string()));
}
inline bool posed(const char *path,bool pose_asset,const cgltf_primitive &primitive){
 if(!pose_asset||!path||!owned_head(primitive.material))return false;
 if(std::string_view(primitive.material->name)=="Thomas_Thomas_Head")return false;
 const auto model=std::filesystem::path(path).lexically_normal(),body=model.parent_path(),character=body.parent_path();
 return body.filename()=="body"&&character.parent_path().filename()=="npc_knights"&&
        world_model_hair::knight_id(character.filename().string())&&world_model_hair::pose_frame(model);
}
}
#endif
