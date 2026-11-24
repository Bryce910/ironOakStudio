// Read-only exact-source cache discovery; never creates a GL context or cache.
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "engine/world_texture_blocks_format.h"
#include "engine/world_character_textures.h"
#include "engine/world_model_image_bytes.h"
#include "engine/world_model_facade.h"
#include <set>
#include <map>
#include <fstream>
#include <iostream>
#include <iomanip>
namespace fs=std::filesystem;
struct Metadata {uint64_t original=0,selected=0,length=0;int w=0,h=0;bool changed=false,alpha=true;std::vector<unsigned char> selected_bytes;};
int main(int argc,char**argv){if(argc!=4)return 2;std::string error;if(!world_character_textures::configure(argv[3],2,error)){std::cerr<<error;return 3;}
 std::ifstream index(argv[2]),models(argv[1]);std::string path;std::map<std::string,std::vector<std::string>> caches;std::set<std::string> canonical;
 while(std::getline(index,path)){std::error_code ec;auto c=fs::canonical(path,ec).string();if(!ec&&canonical.insert(c).second)caches[fs::path(path).filename().string()].push_back(path);}
 std::set<std::string> seen;unsigned model_count=0;bool first=true;std::cout<<"[\n";
 while(std::getline(models,path)){cgltf_options options={};cgltf_data *data=nullptr;if(cgltf_parse_file(&options,path.c_str(),&data)!=cgltf_result_success||cgltf_load_buffers(&options,data,path.c_str())!=cgltf_result_success){std::cerr<<"Parse failed "<<path<<'\n';return 4;}++model_count;
 for(unsigned mi=0;mi<data->materials_count;++mi){auto &mat=data->materials[mi];auto facade=world_model_materials::facade_parameters(&mat,data->textures_count);bool packed=world_model_materials::facade_packed_layers(facade),opaque=mat.alpha_mode==cgltf_alpha_mode_opaque;
 struct Role {const char *name;cgltf_texture *texture;unsigned format;bool eligible;bool srgb;};std::vector<Role> roles={{"color",mat.pbr_metallic_roughness.base_color_texture.texture,1,opaque,true},{"normal",mat.normal_texture.texture,3,!packed,false},{"ARM",mat.pbr_metallic_roughness.metallic_roughness_texture.texture,packed?1u:2u,!packed||opaque,packed},{"emissive",mat.emissive_texture.texture,1,opaque,true}};
 for(unsigned j=0;j<3;++j)if(facade.aux[j]>=0){bool color=world_model_materials::facade_color_aux(facade,j);roles.push_back({j==0?"facade0":j==1?"facade1":"facade2",data->textures+facade.aux[j],1,opaque&&color,world_model_materials::facade_srgb_aux(facade,j)});}
 for(auto role:roles){auto *im=role.texture?role.texture->image:nullptr;if(!im)continue;world_model_image_bytes::Image source;if(!world_model_image_bytes::read(im,path.c_str(),64ull*1024*1024,source,error)){std::cerr<<error<<" "<<path<<'\n';continue;}
 uint64_t original=world_texture_blocks::fingerprint(source.data(),source.length);std::vector<unsigned char> selected;{world_character_textures::LoadingQuality q(world_character_textures::model_profile(2,role.srgb));if(!world_character_textures::replace(source.data(),source.length,selected,error)){std::cerr<<error;return 5;}}
 const unsigned char*bytes=selected.empty()?source.data():selected.data();size_t length=selected.empty()?source.length:selected.size();uint64_t hash=world_texture_blocks::fingerprint(bytes,length);char leaf[64],original_leaf[64];snprintf(leaf,sizeof leaf,role.format==1?"%016llx.vbc7":"%016llx.linear.vbc7",(unsigned long long)hash);snprintf(original_leaf,sizeof original_leaf,role.format==1?"%016llx.vbc7":"%016llx.linear.vbc7",(unsigned long long)original);
 auto parent=fs::path(path).parent_path();if(parent.filename()=="models")parent=parent.parent_path();auto cache=parent/"texture_blocks"/leaf;std::string key=cache.string()+":"+std::to_string(role.format);if(!seen.insert(key).second)continue;
 int w=0,h=0,ch=0;if(!stbi_info_from_memory(bytes,int(length),&w,&h,&ch)){std::cerr<<"Dimensions failed";return 6;}uint64_t rgba=0,block=0;for(unsigned x=w,y=h;;x=std::max(1u,x/2),y=std::max(1u,y/2)){rgba+=uint64_t(x)*y*4;block+=uint64_t((x+3)/4)*((y+3)/4)*16;if(x==1&&y==1)break;}
 world_texture_blocks::Image cooked;bool present=role.eligible&&world_texture_blocks::read(cache.c_str(),bytes,length,cooked,error,role.format);std::string elsewhere;
 if(role.eligible&&!present)for(auto &other:caches[leaf])if(world_texture_blocks::read(other.c_str(),bytes,length,cooked,error,role.format)){elsewhere=other;break;}
 bool alpha=true;if(role.eligible&&role.format!=1&&!present&&elsewhere.empty()){auto *pixels=stbi_load_from_memory(bytes,int(length),&w,&h,&ch,4);if(!pixels)return 7;for(uint64_t p=3;p<uint64_t(w)*h*4;p+=4)if(pixels[p]!=255){alpha=false;break;}stbi_image_free(pixels);}
 if(!first)std::cout<<",\n";first=false;std::cout<<"{\"model\":"<<std::quoted(path)<<",\"material\":"<<std::quoted(mat.name?mat.name:"unnamed")<<",\"role\":"<<std::quoted(role.name)<<",\"format\":"<<role.format<<",\"source_hash\":"<<original<<",\"selected_hash\":"<<hash<<",\"selected_changed\":"<<(hash!=original?"true":"false")<<",\"width\":"<<w<<",\"height\":"<<h<<",\"encoded_bytes\":"<<length<<",\"eligible\":"<<(role.eligible&&alpha?"true":"false")<<",\"linear_alpha_opaque\":"<<(alpha?"true":"false")<<",\"rgba_full_mips_bytes\":"<<rgba<<",\"bc7_full_mips_bytes\":"<<block<<",\"cache_leaf\":"<<std::quoted(leaf)<<",\"original_leaf\":"<<std::quoted(original_leaf)<<",\"cache_path\":"<<std::quoted(cache.string())<<",\"current_valid\":"<<(present?"true":"false")<<",\"other_valid\":"<<std::quoted(elsewhere)<<",\"old_fingerprint_cache_index_hits\":"<<caches[original_leaf].size()<<"}";
 }
 }cgltf_free(data);
 }std::cout<<"\n]\n";std::cerr<<"CPU read-only parsed "<<model_count<<" actual logged/source model representatives, "<<seen.size()<<" distinct prefix+image+format uses; no source/cache edits, noGL\n";}
