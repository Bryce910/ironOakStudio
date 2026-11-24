#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "engine/world_model_cpu.h"
#include "engine/world_model_image_bytes.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
namespace fs=std::filesystem;

#include "engine/world_character_textures.h"
#include "engine/world_model_pending_textures.h"
#include <thread>
#include <atomic>
#include <iomanip>
static unsigned checks=0;
static void require(bool value,const std::string&why){++checks;if(!value)throw std::runtime_error(why);}
static void u32(std::vector<unsigned char>&out,unsigned value){for(unsigned i=0;i<4;++i)out.push_back(static_cast<unsigned char>(value>>(8*i)));}
static void be32(std::vector<unsigned char>&out,unsigned value){for(int i=3;i>=0;--i)out.push_back(static_cast<unsigned char>(value>>(8*i)));}
static void chunk(std::vector<unsigned char>&out,const char*type,const std::vector<unsigned char>&data){
 be32(out,unsigned(data.size()));const auto start=out.size();out.insert(out.end(),type,type+4);out.insert(out.end(),data.begin(),data.end());unsigned crc=~0u;
 for(std::size_t i=start;i<out.size();++i){crc^=out[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
 be32(out,~crc);
}
// Tiny, valid RGBA PNGs with a stored zlib block; no test-only image dependency.
static std::vector<unsigned char> png(unsigned edge,unsigned color){
 std::vector<unsigned char> raw;for(unsigned y=0;y<edge;++y){raw.push_back(0);for(unsigned x=0;x<edge;++x){raw.push_back(static_cast<unsigned char>(color+x));raw.push_back(static_cast<unsigned char>(color+y));raw.push_back(static_cast<unsigned char>(220-color));raw.push_back(255);}}
 std::vector<unsigned char> compressed={0x78,0x01,0x01};const unsigned n=unsigned(raw.size());compressed.push_back(n&255);compressed.push_back(n>>8);compressed.push_back((~n)&255);compressed.push_back((~n)>>8);compressed.insert(compressed.end(),raw.begin(),raw.end());unsigned a=1,b=0;for(unsigned char v:raw){a=(a+v)%65521;b=(b+a)%65521;}be32(compressed,(b<<16)|a);
 std::vector<unsigned char> out={137,80,78,71,13,10,26,10},header;be32(header,edge);be32(header,edge);header.insert(header.end(),{8,6,0,0,0});chunk(out,"IHDR",header);chunk(out,"IDAT",compressed);chunk(out,"IEND",{});return out;
}
static void write(const fs::path&path,const std::vector<unsigned char>&data){fs::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));require(bool(f),"write fixture");}
static fs::path model(const fs::path&path,const std::vector<unsigned char>&image){
 const float vertices[]={0,0,0,1,0,0,0,1,0};std::vector<unsigned char> bin(sizeof(vertices));std::memcpy(bin.data(),vertices,sizeof(vertices));bin.insert(bin.end(),image.begin(),image.end());while(bin.size()%4)bin.push_back(0);
 std::ostringstream json;json<<R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":0}},"normalTexture":{"index":0}}],"textures":[{"source":0,"sampler":0}],"samplers":[{"minFilter":9984,"magFilter":9728,"wrapS":33071,"wrapT":33648}],"images":[{"bufferView":1,"mimeType":"image/png"}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":)"<<image.size()<<R"(}],"buffers":[{"byteLength":)"<<bin.size()<<"}]}";auto text=json.str();while(text.size()%4)text.push_back(' ');
 std::vector<unsigned char> data;u32(data,0x46546c67);u32(data,2);u32(data,unsigned(28+text.size()+bin.size()));u32(data,unsigned(text.size()));u32(data,0x4e4f534a);data.insert(data.end(),text.begin(),text.end());u32(data,unsigned(bin.size()));u32(data,0x004e4942);data.insert(data.end(),bin.begin(),bin.end());write(path,data);return path;
}
static unsigned uploads=0,destroys=0;
static unsigned upload(const void*,std::size_t,const TextureCachePolicy*){return ++uploads;}
static void destroy(unsigned){++destroys;}
static std::vector<unsigned> owners;
static std::vector<unsigned char> eager_bytes(const void*source,std::size_t length,unsigned quality,bool color){
 world_character_textures::LoadingQuality scope(world_character_textures::model_profile(quality,color));std::vector<unsigned char> selected;std::string error;
 require(world_character_textures::replace(source,length,selected,error),"eager replacement: "+error);
 if(selected.empty()){const auto*p=static_cast<const unsigned char*>(source);selected.assign(p,p+length);}return selected;
}
static void release(){for(unsigned id:owners)require(texture_cache_release(id),"release exact source cache owner");owners.clear();require(texture_cache_stats().references==0&&texture_cache_stats().entries==0,"cache ownership balances");texture_cache_shutdown();}
static void cache_eager_model(const char*path,unsigned quality,unsigned edge,bool bc7){
 cgltf_data*data=nullptr;cgltf_options parser={};require(cgltf_parse_file(&parser,path,&data)==cgltf_result_success,"parse actual eager source");
 struct Guard{cgltf_data*p;~Guard(){cgltf_free(p);}}guard{data};require(cgltf_load_buffers(&parser,data,path)==cgltf_result_success&&cgltf_validate(data)==cgltf_result_success,"validate eager source buffers");
 for(std::size_t mi=0;mi<data->meshes_count;++mi)for(std::size_t pi=0;pi<data->meshes[mi].primitives_count;++pi){const auto*m=data->meshes[mi].primitives[pi].material;if(!m)continue;const cgltf_texture_view*views[]={&m->pbr_metallic_roughness.base_color_texture,&m->normal_texture,&m->pbr_metallic_roughness.metallic_roughness_texture,&m->emissive_texture};
  for(unsigned role=0;role<4;++role){const auto&view=*views[role];if(!view.texture)continue;world_model_image_bytes::Image source;std::string error;require(world_model_image_bytes::read(view.texture->image,path,64ull*1024*1024,source,error),"read eager source image: "+error);const bool color=role==0||role==3;const auto selected=eager_bytes(source.data(),source.length,quality,color);int w=0,h=0,c=0;require(stbi_info_from_memory(selected.data(),int(selected.size()),&w,&h,&c)!=0,"selected eager image decodes");
   TextureCachePolicy policy={color,0x2703,0x2601,0x2901,0x2901,1u|(4u<<8)};if(const auto*s=view.texture->sampler){if(s->min_filter)policy.min_filter=s->min_filter;if(s->mag_filter)policy.mag_filter=s->mag_filter;if(s->wrap_s)policy.wrap_s=s->wrap_s;if(s->wrap_t)policy.wrap_t=s->wrap_t;}
   if(edge&&(unsigned(w)>edge||unsigned(h)>edge))policy.mip_policy|=world_model_texture_budget::flags(edge);
   if(bc7&&((color&&m->alpha_mode==cgltf_alpha_mode_opaque)||!color)){const unsigned format=color?world_texture_blocks::format_srgb_bc7:role==1?world_texture_blocks::format_normal_bc7:world_texture_blocks::format_linear_bc7;policy.mip_policy|=(1u<<16)|(format==world_texture_blocks::format_srgb_bc7?0:format<<17);}
   const auto id=texture_cache_acquire(selected.data(),selected.size(),&policy,upload,destroy);require(id!=0,"eager exact-byte cache owner");owners.push_back(id);
  }
 }
}
static void cache_cpu_asset(const world_model_cpu::Asset&asset){
 for(const auto&t:asset.textures){const auto policy=world_model_pending_textures::policy(t,4);const auto id=texture_cache_find_prepared(t.cache_key->prepared,&policy);require(id!=0,"CPU worker reuses exact eager encoded bytes and effective sampler/format");require(texture_cache_release(id),"release worker cache query");}
}
int main(int argc,char**argv){
 const auto directory=fs::temp_directory_path()/("valwood-cpu-character-parity-"+std::to_string(getpid()));
 try{
  namespace bank=world_character_textures;namespace cpu=world_model_cpu;std::string error;
  const auto source=png(64,70);const auto path=model(directory/"source.glb",source);std::array<std::vector<unsigned char>,4> variants;
  std::ostringstream manifest;manifest<<"VALWOOD_CHARACTER_TEXTURES 2\n"<<bank::fingerprint(source.data(),source.size())<<' '<<source.size();
  for(unsigned q=0;q<4;++q){variants[q]=png(4u<<q,20+q*35);const auto leaf=std::to_string(q)+".png";write(directory/leaf,variants[q]);manifest<<' '<<std::quoted(leaf)<<' '<<variants[q].size()<<' '<<bank::fingerprint(variants[q].data(),variants[q].size());}manifest<<'\n';const auto text=manifest.str();write(directory/"textures.manifest",{text.begin(),text.end()});
  unsigned budget_checks=0,parallel_cases=0;
  for(unsigned quality=0;quality<4;++quality){
   require(bank::configure(directory.c_str(),quality,error),error);cpu::Options options;options.max_texture_size=64;options.max_texture_edge=512;
   for(bool bc7:{false,true}){
    options.bc7=bc7;cache_eager_model(path.c_str(),quality,options.max_texture_edge,bc7);const auto before=uploads;const auto prepared=cpu::prepare(path.c_str(),options,error);require(bool(prepared),"CPU selected image preparation: "+error);require(prepared->textures.size()==(bc7?3u:2u),"dedup retains distinct color and compressed normal/ARM policies");cache_cpu_asset(*prepared);require(uploads==before,"no extra uploads after eager owner");
    for(unsigned role=0;role<3;++role){const auto&t=prepared->textures[unsigned(prepared->meshes[0].textures[role])];const unsigned selected=quality==3&&role!=0?2u:quality;require(t.encoded==variants[selected],"exact Low/Medium/High/Ultra bytes, Ultra surfaces stay High");require(t.levels[0].width==(4u<<selected),"dimensions and reduction follow selected image");require(t.min_filter==9984&&t.mag_filter==9728&&t.wrap_s==33071&&t.wrap_t==33648,"source sampler preserved");}
    require(bank::quality==quality&&bank::loading_quality==bank::inherit_quality,"CPU role scopes never mutate global quality or leak thread-local tier");release();
   }
   // The original image is 64px; a small selected tier must pass its own limit.
   options.bc7=false;options.max_texture_edge=0;options.max_texture_size=4u<<quality;
   require(bool(cpu::prepare(path.c_str(),options,error)),"selected dimensions checked after replacement");options.max_texture_size=(4u<<quality)-1;require(!cpu::prepare(path.c_str(),options,error)&&error.find("dimension")!=std::string::npos,"selected image dimension limit enforced");
   options.max_texture_size=64;std::uint64_t low=1024,high=4ull*1024*1024;while(low+1<high){const auto mid=(low+high)/2;options.cpu_bytes=mid;if(cpu::prepare(path.c_str(),options,error))high=mid;else low=mid;}
   options.cpu_bytes=high;require(bool(cpu::prepare(path.c_str(),options,error)),"exact selected-image CPU budget succeeds");options.cpu_bytes=high-1;require(!cpu::prepare(path.c_str(),options,error)&&error.find("budget")!=std::string::npos,"one-byte CPU budget rejection includes temporary selected image");++budget_checks;
  }
  // Configure once, then concurrent workers read immutable registry/files and
  // retain independent explicit requested tiers; the shared cache stays owner-only.
  require(bank::configure(directory.c_str(),1,error),error);std::array<std::thread,4> workers;std::array<bool,4> okay={};
  for(unsigned q=0;q<4;++q)workers[q]=std::thread([&,q]{bank::LoadingQuality scope(q);cpu::Options options;std::string message;const auto prepared=cpu::prepare(path.c_str(),options,message);if(!prepared)return;const auto&t=prepared->textures[unsigned(prepared->meshes[0].textures[0])];okay[q]=t.encoded==variants[q]&&bank::loading_quality==q;});
  for(auto&thread:workers)thread.join();for(bool value:okay){require(value,"concurrent worker selects its requested immutable variant");++parallel_cases;}require(bank::quality==1&&bank::loading_quality==bank::inherit_quality,"worker override leaves owner tier untouched");
  // Integrity failures reject instead of silently returning original images.
  auto corrupted=variants[1];corrupted.back()^=1;write(directory/"1.png",corrupted);cpu::Options options;require(!cpu::prepare(path.c_str(),options,error)&&error.find("integrity")!=std::string::npos,"changed registered image fails safely");write(directory/"1.png",variants[1]);
  if(argc>2)for(unsigned q=0;q<4;++q){require(bank::configure(argv[1],q,error),error);for(int i=2;i<argc;++i){cpu::Options actual;actual.cpu_bytes=768ull*1024*1024;actual.reuse_decoder_scratch=true;actual.bc7=true;cache_eager_model(argv[i],q,actual.max_texture_edge,true);const auto before=uploads;const auto prepared=cpu::prepare(argv[i],actual,error);require(bool(prepared),std::string("actual model ")+argv[i]+": "+error);cache_cpu_asset(*prepared);require(uploads==before,"actual CPU model introduces zero duplicate eager maps");std::cout<<"Actual profile="<<q<<" model="<<argv[i]<<" textures="<<prepared->textures.size()<<" texture_bytes="<<prepared->texture_bytes<<" cpu_bytes="<<prepared->cpu_bytes<<'\n';}release();}
  fs::remove_all(directory);std::cout<<"PASS CPU/eager character parity: "<<checks<<" checks; all 4 tiers, exact bytes/cache IDs/samplers, linear Ultra fallback, selected-image dimensions and "<<budget_checks<<" exact CPU budgets; "<<parallel_cases<<" concurrent immutable-profile workers\n";
 }catch(const std::exception&e){texture_cache_shutdown();fs::remove_all(directory);std::cerr<<e.what()<<'\n';return 1;}
}
