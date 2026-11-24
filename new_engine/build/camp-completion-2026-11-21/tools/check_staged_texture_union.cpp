
#include "engine/world_model_pending_textures.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
namespace cpu=world_model_cpu;namespace pending=world_model_pending_textures;
static unsigned deletes=0;static void destroy(unsigned){deletes++;}
struct Owners {std::vector<unsigned> texture_owners;};
struct Stage {std::shared_ptr<cpu::Asset> cpu;Owners storage;Owners*asset=&storage;unsigned anisotropy=4,image=0;std::size_t texture=0,new_texture_bytes=0;~Stage(){for(auto id:storage.texture_owners)if(id)assert(texture_cache_release(id));}};
std::shared_ptr<cpu::Asset> make(const char*key){auto a=std::make_shared<cpu::Asset>();cpu::Texture t;t.srgb=true;t.min_filter=9987;t.mag_filter=9729;t.wrap_s=t.wrap_t=10497;t.fingerprint=19;t.encoded.assign(key,key+std::strlen(key));t.cache_key=std::make_shared<cpu::CacheKey>();t.cache_key->prepared=texture_cache_prepare(t.encoded.data(),t.encoded.size());assert(t.cache_key->prepared);t.levels.push_back({8,8,std::vector<std::uint8_t>(256)});t.levels.push_back({4,4,std::vector<std::uint8_t>(64)});a->textures.push_back(std::move(t));return a;}
void init(Stage&s,const char*key){s.cpu=make(key);s.storage.texture_owners.resize(1);s.new_texture_bytes=pending::bytes(s.cpu->textures[0]);}
unsigned adopt(Stage&s,unsigned id,std::size_t stored=320){auto&t=s.cpu->textures[0];auto p=pending::policy(t,s.anisotropy);auto image=texture_cache_adopt_prepared(t.cache_key->take(),&p,id,destroy);assert(image);assert(texture_cache_track_gpu_bytes(image,stored));s.storage.texture_owners[0]=image;s.new_texture_bytes=0;return image;}
int main(){
 {Stage primary,shadow;init(primary,"shared source A");init(shadow,"shared source A");assert(pending::missing_union(&primary,&shadow)==320);assert(primary.new_texture_bytes+shadow.new_texture_bytes==640);const auto image=adopt(primary,41);pending::refresh(&shadow);assert(shadow.storage.texture_owners[0]==image&&shadow.new_texture_bytes==0);assert(texture_cache_stats().references==2);pending::refresh(&shadow);assert(texture_cache_stats().references==2);assert(pending::missing_union(&primary,&shadow)==0);}
 assert(texture_cache_stats().references==0&&texture_cache_stats().entries==0&&deletes==1);
 {Stage a,b;init(a,"same hash one");init(b,"same hash two");assert(pending::missing_union(&a,&b)==640);b.cpu=make("same hash one");b.anisotropy=8;assert(pending::missing_union(&a,&b)==640);b.anisotropy=4;b.cpu->textures[0].srgb=false;assert(pending::missing_union(&a,&b)==640);b.cpu->textures[0].srgb=true;b.cpu->textures[0].texture_edge=512;assert(pending::missing_union(&a,&b)==640);b.cpu->textures[0].texture_edge=1024;assert(pending::missing_union(&a,&b)==640);b.cpu->textures[0].texture_edge=0;assert(pending::missing_union(&a,&b)==320);}
 {Stage a,b;init(a,"certified shared map");init(b,"certified shared map");for(auto*s:{&a,&b}){s->cpu->textures[0].bc7_policy=true;s->cpu->textures[0].bc7_format=world_texture_blocks::format_normal_bc7;}const auto image=adopt(a,42,16);b.image=99;assert(pending::missing_union(&a,&b)==320&&b.storage.texture_owners[0]==0);b.image=0;pending::refresh(&b);assert(b.storage.texture_owners[0]==image&&texture_cache_stats().gpu_bytes==16);assert(texture_cache_track_gpu_bytes(image,320)&&texture_cache_stats().gpu_bytes==16);auto&t=b.cpu->textures[0];assert(pending::policy(t,4).mip_policy==(1u|(4u<<8)|(1u<<16)|(world_texture_blocks::format_normal_bc7<<17)));}
 assert(texture_cache_stats().references==0&&texture_cache_stats().entries==0&&texture_cache_stats().gpu_bytes==0&&deletes==2);
 //55 independent model owners share three image banks. The first in each
 //family admits bytes once; all later models acquire references with no upload.
 {std::vector<std::unique_ptr<Stage>> owners;std::size_t admitted=0;for(unsigned i=0;i<55;i++){auto s=std::make_unique<Stage>();const auto key=i<13?"garage":i<52?"storage":"electrical";init(*s,key);auto missing=pending::missing_union(s.get(),static_cast<Stage*>(nullptr));admitted+=missing;if(missing)adopt(*s,100+i);owners.push_back(std::move(s));}assert(admitted==960&&texture_cache_stats().entries==3&&texture_cache_stats().references==55);}
 assert(texture_cache_stats().references==0&&texture_cache_stats().entries==0&&texture_cache_stats().gpu_bytes==0&&deletes==5);
 std::puts("Exact primary/shadow union, shared cache refresh, hash-collision byte check, sampler/role identity, partial image reservation,55 independent owners/3 banks, unchanged tracked storage and zero teardown refs PASS");
}
