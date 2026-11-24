#ifndef VALWOOD_WORLD_MODEL_ORC_ORGANIC_H
#define VALWOOD_WORLD_MODEL_ORC_ORGANIC_H
#ifndef CGLTF_H_INCLUDED__
#include "cgltf.h"
#endif
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace world_model_orc_organic {
// These exact source-object names come from the purchased roster's primitive
// correspondence receipts. An atlas/material can also own armor, hair or eyes;
// neither its name, its pixel colors nor nonmetal values identify organic body.
struct Contract {std::string_view id,body;};
inline constexpr std::array<Contract,14> contracts={{
 {"azrak","Azrak_Body_Mesh"},{"costalmed","Costalmed_Body_Mesh"},
 {"dardan","Dardan_Body_Mesh"},{"girra","Gira_Body_Mesh"},
 {"gnur","Gnur_Body_mesh"},{"kanit","Body_Mesh"},{"magur","Magur_Body_Mesh"},
 {"orc01","Orc_01_Body_Mesh"},{"orc02","Orc_02_Body_Mesh"},
 {"orc03","Orc_03_Body_Mesh"},{"orc04","Orc_04_Body_Mesh"},
 {"orc05","Orc_05_Body_Mesh"},{"orc06","Orc_06_Body_Mesh"},{"ursag","Body_Mesh"}
}};
inline bool classify(const char *path,bool pose_asset,const cgltf_primitive &primitive){
 if(!pose_asset||!path||!primitive.extras.data)return false;
 const std::filesystem::path model=std::filesystem::path(path).lexically_normal();
 const auto body=model.parent_path(),character=body.parent_path(),roster=character.parent_path();
 if(body.filename()!="body"||roster.filename()!="arena_orcs_roster")return false;
 const std::string filename=model.filename().string();
 constexpr std::string_view prefix="combat_body_";
 if(filename.size()!=prefix.size()+7||filename.compare(0,prefix.size(),prefix)||filename.substr(prefix.size()+3)!=".glb")return false;
 unsigned slot=0;for(unsigned i=0;i<3;++i){const char c=filename[prefix.size()+i];if(c<'0'||c>'9')return false;slot=slot*10+unsigned(c-'0');}
 if(slot>=172)return false;
 std::string_view expected;
 const std::string id=character.filename().string();
 for(const auto &contract:contracts)if(id==contract.id){expected=contract.body;break;}
 if(expected.empty())return false;
 // Conservatively accept only the bounded two-field provenance object emitted
 // by this owned cooker. Unknown metadata and escaped/duplicate fields remain
 // ordinary materials. Preserve quoted whitespace; never normalize names.
 std::string compact;compact.reserve(128);bool quoted=false;
 for(const char *p=primitive.extras.data;*p;++p){
  if(compact.size()>=256||*p=='\\')return false;
  if(*p=='"')quoted=!quoted;
  if(!quoted&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))continue;
  compact.push_back(*p);
 }
 return !quoted&&compact=="{\"source_object\":\""+std::string(expected)+"\",\"rigid_weapon\":false}";
}
}
#endif
