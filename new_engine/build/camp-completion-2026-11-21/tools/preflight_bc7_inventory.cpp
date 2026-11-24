
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_texture_blocks_format.h"
#include <filesystem>
#include <map>
#include <set>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
namespace fs=std::filesystem;
int main(int argc,char **argv){if(argc!=3)return 2;std::map<std::string,std::vector<std::string>>index;std::ifstream idx(argv[2]);std::string path;while(std::getline(idx,path))index[fs::path(path).filename().string()].push_back(path);std::ifstream inputs(argv[1]);unsigned models=0,images=0;std::set<std::string>checked;std::cout<<"[\n";bool first=true;
while(std::getline(inputs,path)){cgltf_options options={};cgltf_data *data=nullptr;if(cgltf_parse_file(&options,path.c_str(),&data)!=cgltf_result_success||cgltf_load_buffers(&options,data,path.c_str())!=cgltf_result_success){std::cerr<<"Parse "<<path<<'\n';return 1;}models++;
for(unsigned m=0;m<data->materials_count;m++){auto &mat=data->materials[m];struct Role{const char *name;cgltf_texture_view *view;unsigned format;bool eligible;};Role roles[]={{"color",&mat.pbr_metallic_roughness.base_color_texture,1,mat.alpha_mode==cgltf_alpha_mode_opaque},{"normal",&mat.normal_texture,3,true},{"ARM",&mat.pbr_metallic_roughness.metallic_roughness_texture,2,true},{"emissive",&mat.emissive_texture,1,mat.alpha_mode==cgltf_alpha_mode_opaque}};
for(auto role:roles){auto *tex=role.view->texture;if(!tex||!tex->image||!tex->image->buffer_view)continue;auto *v=tex->image->buffer_view;auto *bytes=cgltf_buffer_view_data(v);if(!bytes)continue;int w=0,h=0,ch=0;if(!stbi_info_from_memory(bytes,int(v->size),&w,&h,&ch))return 1;uint64_t rgba=0,blocks=0;for(unsigned x=w,y=h;;x=std::max(1u,x/2),y=std::max(1u,y/2)){rgba+=uint64_t(x)*y*4;blocks+=uint64_t((x+3)/4)*((y+3)/4)*16;if(x==1&&y==1)break;}
char leaf[40];snprintf(leaf,sizeof(leaf),role.format==1?"%016llx.vbc7":"%016llx.linear.vbc7",(unsigned long long)world_texture_blocks::fingerprint(bytes,v->size));auto parent=fs::path(path).parent_path();if(parent.filename()=="models")parent=parent.parent_path();auto cache=parent/"texture_blocks"/leaf;world_texture_blocks::Image cooked;std::string error,found;bool present=role.eligible&&world_texture_blocks::read(cache.c_str(),bytes,v->size,cooked,error,role.format);if(!present&&role.eligible){for(const auto &candidate:index[leaf]){std::error_code ec;if(fs::canonical(candidate,ec)==fs::canonical(cache,ec))continue;if(world_texture_blocks::read(candidate.c_str(),bytes,v->size,cooked,error,role.format)){found=candidate;break;}}}
bool alpha_opaque=true;if(role.eligible&&role.format!=1&&!present&&found.empty()){auto *pix=stbi_load_from_memory(bytes,int(v->size),&w,&h,&ch,4);if(!pix)return 1;for(uint64_t x=3;x<uint64_t(w)*h*4;x+=4)if(pix[x]!=255){alpha_opaque=false;break;}stbi_image_free(pix);}
if(!first)std::cout<<",\n";first=false;std::cout<<"{\"model\":"<<std::quoted(path)<<",\"material\":"<<std::quoted(mat.name?mat.name:"unnamed")<<",\"role\":"<<std::quoted(role.name)<<",\"alpha_mode\":"<<unsigned(mat.alpha_mode)<<",\"image\":"<<std::quoted(tex->image->name?tex->image->name:"")<<",\"image_index\":"<<(tex->image-data->images)<<",\"width\":"<<w<<",\"height\":"<<h<<",\"encoded_bytes\":"<<v->size<<",\"rgba_full_mips_bytes\":"<<rgba<<",\"bc7_full_mips_bytes\":"<<blocks<<",\"potential_saved_bytes\":"<<(role.eligible&&alpha_opaque?rgba-blocks:0)<<",\"eligible\":"<<(role.eligible&&alpha_opaque?"true":"false")<<",\"original_alpha_opaque\":"<<(alpha_opaque?"true":"false")<<",\"cache_leaf\":"<<std::quoted(leaf)<<",\"cache_path\":"<<std::quoted(cache.string())<<",\"current_cache_valid\":"<<(present?"true":"false")<<",\"other_validated_cache\":"<<std::quoted(found)<<"}";images++;}}
cgltf_free(data);}
std::cout<<"\n]\n";std::cerr<<"CPU only models"<<models<<" image uses"<<images<<'\n';}
