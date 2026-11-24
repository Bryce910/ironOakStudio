#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "engine/world_model_cpu.h"
#include "engine/world_model_pending_textures.h"
#include "engine/world_texture_blocks_format.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
namespace fs=std::filesystem;
namespace cpu=world_model_cpu;
namespace pending=world_model_pending_textures;
static unsigned checks=0;
static void check(bool v,const std::string &s){++checks;if(!v)throw std::runtime_error(s);}
static std::vector<unsigned char> read(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void write(const fs::path&p,const std::vector<unsigned char>&v){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write((const char*)v.data(),v.size());check(bool(f),"write fixture");}
static void u32(std::vector<unsigned char>&out,unsigned v){for(unsigned i=0;i<4;++i)out.push_back((unsigned char)(v>>(i*8)));}
static fs::path model(const fs::path&dir,const char *name,const std::vector<unsigned char>&png,bool mask=false){
 float pos[]={0,0,0,1,0,0,0,1,0};std::vector<unsigned char> bin(sizeof(pos));std::memcpy(bin.data(),pos,sizeof(pos));bin.insert(bin.end(),png.begin(),png.end());while(bin.size()%4)bin.push_back(0);
 std::ostringstream j;j<<R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],"materials":[{)";
 if(mask)j<<R"("alphaMode":"MASK","alphaCutoff":0.4,)";
 j<<R"("pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicFactor":0,"roughnessFactor":0.7}}],"textures":[{"source":0}],"images":[{"bufferView":1,"mimeType":"image/png"}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":)"<<png.size()<<R"(}],"buffers":[{"byteLength":)"<<bin.size()<<"}]}";
 auto js=j.str();while(js.size()%4)js.push_back(' ');std::vector<unsigned char>out;u32(out,0x46546c67);u32(out,2);u32(out,unsigned(28+js.size()+bin.size()));u32(out,unsigned(js.size()));u32(out,0x4e4f534a);out.insert(out.end(),js.begin(),js.end());u32(out,unsigned(bin.size()));u32(out,0x004e4942);out.insert(out.end(),bin.begin(),bin.end());const auto path=dir/"models"/name;write(path,out);return path;
}
static std::shared_ptr<const cpu::Asset> load(const fs::path &p,unsigned cap,bool bc7=false){cpu::Options o;o.max_texture_edge=cap;o.bc7=bc7;o.reuse_decoder_scratch=true;std::string e;auto a=cpu::prepare(p.c_str(),o,e);check(bool(a),"prepare "+p.string()+" cap "+std::to_string(cap)+": "+e);check(a->textures.size()==1,"one source texture");return a;}
static std::uint64_t rgba_bytes(unsigned w,unsigned h){std::uint64_t n=0;for(;;w=std::max(1u,w/2),h=std::max(1u,h/2)){n+=std::uint64_t(w)*h*4;if(w==1&&h==1)return n;}}
static std::uint64_t actual_bytes(const cpu::Texture&t){std::uint64_t n=0;for(const auto&l:t.levels)n+=l.rgba.size();return n;}
static void levels_equal(const cpu::Texture&a,const cpu::Texture&b){check(a.levels.size()==b.levels.size(),"unchanged mip count");for(unsigned i=0;i<a.levels.size();++i)check(a.levels[i].width==b.levels[i].width&&a.levels[i].height==b.levels[i].height&&a.levels[i].rgba==b.levels[i].rgba,"unchanged mip pixels");}
static void noop(unsigned){}
int main(int argc,char**argv){try{
 check(argc==2,"fixture directory argument");const fs::path dir=argv[1];
 const auto wide=model(dir,"wide.glb",read(dir/"wide.png"));auto original=load(wide,0);const auto&full=original->textures[0];check(full.levels[0].width==4096&&full.levels[0].height==16,"wide source dimensions");
 for(unsigned cap:{512u,1024u,2048u,4096u}){auto a=load(wide,cap);const auto&t=a->textures[0];unsigned w=4096,h=16;world_model_texture_budget::dimensions(w,h,cap);check(t.levels[0].width==w&&t.levels[0].height==h,"quality-selected dimensions");check(t.texture_edge==(cap<4096?cap:0),"effective cap only if smaller");check(t.encoded==full.encoded&&t.fingerprint==full.fingerprint,"original encoded identity preserved");check(a->texture_bytes==rgba_bytes(w,h)&&actual_bytes(t)==a->texture_bytes,"exact retained/upload mip bytes");check(a->gpu_bytes==a->geometry_bytes+a->texture_bytes&&a->upload_bytes==a->gpu_bytes,"GPU admission bytes include exact reduced chain");const auto policy=pending::policy(t,4);check(world_model_texture_budget::edge(policy.mip_policy)==t.texture_edge,"pending cache cap matches eager policy bits");if(cap==4096){levels_equal(t,full);check(pending::same(t,4,full,4),"source within cap shares original cache identity");}else{check(!pending::same(t,4,full,4),"different dimensions never share cache identity");check(t.levels[0].rgba[0]==188,"sRGB reduction averages in linear light");}}
 const auto small=model(dir,"small.glb",read(dir/"small.png"));auto sm0=load(small,0);for(unsigned cap:{512u,1024u,2048u,4096u}){auto a=load(small,cap);check(!a->textures[0].texture_edge,"small source cap is zero");levels_equal(a->textures[0],sm0->textures[0]);check(pending::same(a->textures[0],4,sm0->textures[0],4),"all fitting caps share exact cache");}
 const auto odd=model(dir,"odd.glb",read(dir/"odd.png"));auto od=load(odd,512);check(od->textures[0].levels[0].width==384&&od->textures[0].levels[0].height==192,"odd dimensions select exact source mip aspect");check(od->texture_bytes==rgba_bytes(384,192),"odd exact byte count");check(od->textures[0].levels[0].rgba[0]>0,"odd final column contributes to filtered output");
 const auto mask=model(dir,"mask.glb",read(dir/"mask.png"),true);auto ma0=load(mask,0,true),ma=load(mask,512,true);const auto &mt=ma->textures[0];check(!mt.compressed&&!mt.bc7_policy,"masked color stays uncompressed");check(mt.levels[0].width==512&&mt.levels[0].height==4,"masked dimensions reduced");check(mt.levels[0].rgba[0]==255&&mt.levels[0].rgba[1]==64&&mt.levels[0].rgba[2]==32&&mt.levels[0].rgba[3]==128,"alpha-zero edge RGB bleed, source opacity averaging");check(ma->meshes[0].cutoff==.4f&&ma0->meshes[0].cutoff==.4f,"declared alpha cutoff unchanged");check(ma0->textures[0].levels[0].rgba[0]==0&&ma0->textures[0].levels[0].rgba[3]==0,"uncapped source pixels unchanged");
 // A mathematically constant-white BC7 mode6 fixture has zero RGB/alpha error
 // at every mip; use the real serializer/parser certificate and source matching.
 auto white_png=read(dir/"white.png");const auto white=model(dir,"white.glb",white_png);world_texture_blocks::Image image;image.width=2048;image.height=8;
 for(unsigned w=image.width,h=image.height;;w=std::max(1u,w/2),h=std::max(1u,h/2)){world_texture_blocks::Level l;l.width=w;l.height=h;l.blocks.resize(std::size_t((w+3)/4)*((h+3)/4)*16);for(std::size_t i=0;i<l.blocks.size();i+=16){l.blocks[i]=0xc0;for(unsigned k=1;k<8;++k)l.blocks[i+k]=255;l.blocks[i+8]=1;}image.levels.push_back(std::move(l));if(w==1&&h==1)break;}
 std::vector<unsigned char> cooked;std::string error;check(world_texture_blocks::serialize(white_png.data(),white_png.size(),image,cooked,error),"valid constant-white BC7 fixture: "+error);char leaf[40];std::snprintf(leaf,sizeof(leaf),"%016llx.vbc7",(unsigned long long)world_texture_blocks::fingerprint(white_png.data(),white_png.size()));const auto cache=dir/"texture_blocks"/leaf;write(cache,cooked);
 auto bcfull=load(white,0,true),bclow=load(white,512,true);check(bcfull->textures[0].compressed&&bclow->textures[0].compressed,"certified sidecar selected");const auto&bl=bclow->textures[0];check(bl.texture_edge==512&&bl.levels[0].width==512&&bl.levels[0].height==2,"compressed cap starts at certified existing mip");check(bl.levels.size()==bcfull->textures[0].levels.size()-2,"compressed complete tail retained");for(unsigned i=0;i<bl.levels.size();++i)check(bl.levels[i].rgba==bcfull->textures[0].levels[i+2].rgba,"compressed original blocks unchanged");check(bclow->texture_bytes==actual_bytes(bl)&&bclow->texture_bytes<bcfull->texture_bytes,"compressed exact retained byte accounting");check((pending::policy(bl,4).mip_policy&(1u<<16))&&world_model_texture_budget::edge(pending::policy(bl,4).mip_policy)==512,"compression+cap combined identity");
 cooked.back()^=1;write(cache,cooked);auto fallback=load(white,512,true);check(!fallback->textures[0].compressed&&fallback->textures[0].texture_edge==512,"bad certificate/checksum uses reduced original fallback");check(fallback->texture_bytes==rgba_bytes(512,2),"fallback reservation is exact RGBA chain");check(pending::same(bl,4,fallback->textures[0],4),"certified/fallback same source and semantic policy");
 auto *key=full.cache_key->prepared;auto cap512=load(wide,512);auto original_policy=pending::policy(full,4),low_policy=pending::policy(cap512->textures[0],4);check(texture_cache_adopt_prepared(texture_cache_prepare(full.encoded.data(),full.encoded.size()),&original_policy,9001,noop)==9001,"original cache adopt");check(texture_cache_find_prepared(key,&low_policy)==0,"cap does not reuse full GPU texture");check(texture_cache_adopt_prepared(texture_cache_prepare(full.encoded.data(),full.encoded.size()),&low_policy,9002,noop)==9002,"reduced cache adopt");check(texture_cache_find_prepared(key,&low_policy)==9002,"reduced key reuses exact GPU policy");check(texture_cache_release(9002)&&texture_cache_release(9002)&&texture_cache_release(9001),"all GPU cache references released");
 cpu::Options invalid;invalid.max_texture_edge=768;check(!cpu::prepare(wide.c_str(),invalid,error)&&error.find("options")!=std::string::npos,"invalid cap fails atomic model load");invalid.max_texture_edge=512;invalid.max_texture_size=2048;check(!cpu::prepare(wide.c_str(),invalid,error)&&error.find("dimension")!=std::string::npos,"hardware source limit remains independent of quality cap");
 // Exercise conservative scratch threshold on an actually reduced masked map.
 std::uint64_t lo=1024,hi=32ull*1024*1024;cpu::Options bounded;bounded.max_texture_edge=512;bounded.reuse_decoder_scratch=true;while(lo+1<hi){auto mid=(lo+hi)/2;bounded.cpu_bytes=mid;if(cpu::prepare(mask.c_str(),bounded,error))hi=mid;else lo=mid;}bounded.cpu_bytes=hi;check(bool(cpu::prepare(mask.c_str(),bounded,error)),"exact conservative decode/reduction budget succeeds");bounded.cpu_bytes=hi-1;check(!cpu::prepare(mask.c_str(),bounded,error)&&error.find("budget")!=std::string::npos,"one byte below reduced CPU scratch budget fails");
 check(!texture_cache_stats().references&&!texture_cache_stats().entries,"no cache ownership leaks");texture_cache_shutdown();std::cout<<"CPU environment texture PASS: "<<checks<<" checks; RGBA tiers512/1024/2048/4096, untouched fitting/originals, certified mip tails/fallback, alpha-safe sRGB filtering, policy identity, hardware and scratch budgets\n";return 0;
 }catch(const std::exception&e){texture_cache_shutdown();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
