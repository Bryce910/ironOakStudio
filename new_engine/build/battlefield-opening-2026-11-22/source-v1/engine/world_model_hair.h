#ifndef VALWOOD_WORLD_MODEL_HAIR_H
#define VALWOOD_WORLD_MODEL_HAIR_H
#ifndef CGLTF_H_INCLUDED__
#include "cgltf.h"
#endif
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

namespace world_model_hair {
// Explicit owned-source contracts. A MASK material alone cannot distinguish
// hair from chainmail, helmet openings, feathers or arbitrary imported props.
enum Kind : unsigned {Ordinary=0,OrcMask=1,KnightBlend=2,ThomasCards=3,ThomasHdrpCards=4};
inline bool exact_factor(float value,float expected){return std::isfinite(value)&&std::abs(value-expected)<=.000001f;}
inline bool knight_id(std::string_view id){
 if(id.size()!=8||id.substr(0,6)!="knight"||id[6]<'0'||id[6]>'1'||id[7]<'0'||id[7]>'9')return false;
 const unsigned number=unsigned(id[6]-'0')*10+unsigned(id[7]-'0');return number>=1&&number<=18;
}
inline bool pose_frame(const std::filesystem::path &path){
 const std::string file=path.filename().string();constexpr std::string_view prefix="combat_body_";
 if(file.size()!=prefix.size()+7||file.compare(0,prefix.size(),prefix)||file.substr(prefix.size()+3)!=".glb")return false;
 unsigned slot=0;for(unsigned i=0;i<3;++i){const char c=file[prefix.size()+i];if(c<'0'||c>'9')return false;slot=slot*10+unsigned(c-'0');}
 return slot<172;
}
inline bool source_object(const cgltf_primitive &primitive,std::string_view expected){
 if(!primitive.extras.data)return false;
 std::string compact;compact.reserve(128);bool quoted=false;
 for(const char *p=primitive.extras.data;*p;++p){
  if(compact.size()>=256||*p=='\\')return false;
  if(*p=='"')quoted=!quoted;
  if(!quoted&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))continue;
  compact.push_back(*p);
 }
 return !quoted&&compact=="{\"source_object\":\""+std::string(expected)+"\",\"rigid_weapon\":false}";
}
inline bool pilosity(const cgltf_material *material){
 if(!material||!material->name||std::string_view(material->name)!="MedievalCharacter_Pilosity"||
    material->alpha_mode!=cgltf_alpha_mode_mask||!exact_factor(material->alpha_cutoff,.029f)||
    material->unlit||material->has_transmission||material->has_pbr_specular_glossiness)return false;
 const auto &pbr=material->pbr_metallic_roughness;
 if(!pbr.base_color_texture.texture||!pbr.metallic_roughness_texture.texture||
    !material->normal_texture.texture||!material->occlusion_texture.texture||
    !exact_factor(pbr.metallic_factor,1)||!exact_factor(pbr.roughness_factor,1))return false;
 // This exact Unity _Color belongs to the source _ALPHABLEND_ON material;
 // its alpha was preserved by the cooker despite the MASK downgrade.
 for(unsigned i=0;i<3;++i)if(!exact_factor(pbr.base_color_factor[i],.54245275f))return false;
 return exact_factor(pbr.base_color_factor[3],1);
}
inline bool thomas_cards(const cgltf_material *material){
 if(!material||!material->name||std::string_view(material->name)!="Thomas_Tom_Hair_M"||
    material->alpha_mode!=cgltf_alpha_mode_mask||!exact_factor(material->alpha_cutoff,.298f)||
    !material->double_sided||material->unlit||material->has_transmission||material->has_pbr_specular_glossiness)return false;
 const auto &pbr=material->pbr_metallic_roughness;
 constexpr std::array<float,4> tint={.6415094f,.4804795f,.30562478f,1.f};
 for(unsigned i=0;i<4;++i)if(!exact_factor(pbr.base_color_factor[i],tint[i]))return false;
 return pbr.base_color_texture.texture&&material->normal_texture.texture&&
        exact_factor(pbr.metallic_factor,0)&&exact_factor(pbr.roughness_factor,.799f);
}
inline bool thomas_hdrp_cards(const cgltf_material *material){
 if(!material||!material->name||
    (std::string_view(material->name)!="Thomas_HDRP_Hair_45fad7c5030668c4aa7772db60957c17"&&
     std::string_view(material->name)!="Thomas_Fiber_Hair_SourceBrunette")||
    material->alpha_mode!=cgltf_alpha_mode_mask||!exact_factor(material->alpha_cutoff,.5f)||
    !material->double_sided||material->unlit||material->has_transmission||material->has_pbr_specular_glossiness)return false;
 const auto &pbr=material->pbr_metallic_roughness;
 for(unsigned i=0;i<4;++i)if(!exact_factor(pbr.base_color_factor[i],1))return false;
 // Exact owned HDRP graph bake: source-linear opacity is in A, alphaPower
 // is applied once by the kind4 fragment contract. The normal graph's blend
 // and tangent-Y push are baked into its dedicated linear normal image.
 return pbr.base_color_texture.texture&&material->normal_texture.texture&&
        exact_factor(material->normal_texture.scale,1)&&
        exact_factor(pbr.metallic_factor,0)&&exact_factor(pbr.roughness_factor,1);
}
// These two authored outfits retain the same purchased head/hair and source
// materials as the roster. Keep that contract independent of the clothing:
// modern clothes must not fall back to the old hard-cut hair conversion.
inline bool story_body(const std::filesystem::path &model){
 const auto variant=model.parent_path();
 return model.filename()=="body.glb"&&variant.parent_path().filename()=="story_hero"&&
        (variant.filename()=="civilian"||variant.filename()=="armored");
}
inline unsigned skeletal(const char *path,const cgltf_material *material){
 if(!path)return Ordinary;
 const auto model=std::filesystem::path(path).lexically_normal();
 if(thomas_hdrp_cards(material))return story_body(model)?ThomasHdrpCards:Ordinary;
 if(thomas_cards(material))return story_body(model)?ThomasCards:Ordinary;
 if(!pilosity(material))return Ordinary;
 const auto character=model.parent_path();
 return (story_body(model)||(model.filename()=="body.glb"&&character.parent_path().filename()=="knights_roster"&&
        knight_id(character.filename().string())))?KnightBlend:Ordinary;
}
inline unsigned posed(const char *path,bool pose_asset,const cgltf_primitive &primitive){
 if(!pose_asset||!path)return Ordinary;
 const auto model=std::filesystem::path(path).lexically_normal();
 const auto body=model.parent_path(),character=body.parent_path(),roster=character.parent_path();
 if(body.filename()!="body"||!pose_frame(model))return Ordinary;
 const auto *material=primitive.material;
 if(roster.filename()=="npc_knights"&&knight_id(character.filename().string())&&pilosity(material))return KnightBlend;
 if(roster.filename()!="arena_orcs_roster"||!material||!material->name||
    material->alpha_mode!=cgltf_alpha_mode_mask||!material->pbr_metallic_roughness.base_color_texture.texture||
    !material->pbr_metallic_roughness.metallic_roughness_texture.texture)return Ordinary;
 struct Contract {std::string_view id,object,material;float cutoff;};
 constexpr std::array<Contract,5> contracts={{
  {"orc01","Orc_01_Beard","Owned Unity Orc Orc 01 Orc_01_Hair",.5f},
  {"orc04","Orc_04_Hair_Mesh","Owned Unity Orc Orc 04 Orc_04_Hair",.5f},
  {"orc05","Or_05_Beard_Mesh","Owned Unity Orc Orc 05 Haiir",.392f},
  {"orc05","Or_05_Hair_Mesh","Owned Unity Orc Orc 05 Haiir embedded hair",.392f},
  {"orc06","Orc_06_Hair_Mesh","Owned Unity Orc Orc 06 Orc_06_Hair",.5f}
 }};
 const std::string id=character.filename().string();
 for(const auto &contract:contracts)if(id==contract.id&&material->name==contract.material&&
     std::abs(material->alpha_cutoff-contract.cutoff)<.000001f&&source_object(primitive,contract.object))return OrcMask;
 return Ordinary;
}
}
#endif
