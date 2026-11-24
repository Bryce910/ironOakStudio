
#include "engine/world_scenery_interior_residency.h"
#include "engine/world_regions.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <thread>
namespace p=world_scenery_interior_residency;namespace r=world_regions;
struct Cache {std::uint64_t bytes=1342031548,cap=1342177280;static constexpr std::uint64_t bank=4194300,remote=32ull*1024*1024;bool owned[7]={};unsigned guard_failures=0,released_remote=0;};
std::shared_ptr<const r::CpuBlob> load(const r::LoadRequest&,void*){auto b=std::make_shared<r::CpuBlob>();b->data={1};b->cpu_bytes=sizeof(r::CpuBlob)+b->data.capacity();b->gpu_bytes=1;b->upload_bytes=1;return b;}
r::UploadResult upload(const r::UploadRequest&q,void*context){auto&c=*static_cast<Cache*>(context);r::UploadResult x;if(c.bytes>c.cap||Cache::bank>c.cap-c.bytes){x.success=false;c.guard_failures++;return x;}c.bytes+=Cache::bank;c.owned[q.region]=true;x.complete=true;x.token=q.region;x.consumed_bytes=1;x.gpu_bytes=1;return x;}
void release(r::Id id,std::uint64_t,void*context){auto&c=*static_cast<Cache*>(context);if(id==1){c.bytes-=Cache::remote;c.released_remote++;}else if(id>=4&&c.owned[id]){c.bytes-=Cache::bank;c.owned[id]=false;}}
int main(){
 const double nan=std::numeric_limits<double>::quiet_NaN();
 assert(!p::release_remote_pin(false,false,18000,240,96));
 assert(!p::release_remote_pin(true,true,18000,240,96));
 assert(!p::release_remote_pin(true,false,0,240,96));
 assert(!p::release_remote_pin(true,false,336,240,96));
 assert(p::release_remote_pin(true,false,336.001,240,96));
 assert(!p::release_remote_pin(true,false,nan,240,96));
 assert(!p::release_remote_pin(true,false,500,240,nan));
 assert(!p::texture_retry(1342031548,1342031548,4194300,1342177280));
 assert(!p::texture_retry(1342031547,1342031548,4194300,1342177280));
 assert(p::texture_retry(1342177280-4194300,1342031548,4194300,1342177280));
 assert(!p::texture_retry(1342177280+1,1342177280+2,1,1342177280));
 Cache cache;r::Coordinator coordinator;r::Document document;for(unsigned i=1;i<=6;i++){r::Region region;region.id=i;r::Layer layer;layer.id=i;region.layers.push_back(layer);document.regions.push_back(region);}r::Config limits;limits.cpu_bytes=1024*1024;limits.gpu_bytes=1024;limits.upload_bytes_per_frame=1024;std::string error;assert(coordinator.configure(document,limits,{load,upload,release,&cache},error));
 for(unsigned i=1;i<=3;i++){assert(coordinator.adopt(i,i,0,1));assert(coordinator.pin(i,true));assert(coordinator.demand(i,2,0));}
 for(unsigned i=4;i<=6;i++)assert(coordinator.demand(i,2,0));
 std::uint64_t frame=0;auto pump=[&](auto done){for(unsigned i=0;i<3000&&!done();i++){assert(coordinator.request({},++frame));assert(coordinator.tick());std::this_thread::sleep_for(std::chrono::milliseconds(1));}assert(done());};
 pump([&]{return coordinator.stats().failed==3;});assert(cache.guard_failures==3&&cache.bytes==1342031548);
 // Actual Coordinator pin/demand/retire path: only the remote outdoor family
 // releases; a nearby reused prop and apartment MID keep their original pins.
 const bool remote_release=p::release_remote_pin(true,false,18000,240,96);
 const bool nearby_release=p::release_remote_pin(true,false,2,240,96);
 const bool apartment_release=p::release_remote_pin(true,true,2,240,96);
 assert(remote_release&&!nearby_release&&!apartment_release);
 assert(coordinator.pin(1,!remote_release));assert(coordinator.demand(1,0,18000));assert(coordinator.request({},++frame));assert(coordinator.retire(1));
 assert(!coordinator.state(1).resident&&coordinator.state(2).pinned&&coordinator.state(3).pinned&&cache.released_remote==1);
 for(unsigned i=4;i<=6;i++){assert(p::texture_retry(cache.bytes,1342031548,Cache::bank,cache.cap));assert(coordinator.reload(i));}
 pump([&]{return coordinator.state(4).resident&&coordinator.state(5).resident&&coordinator.state(6).resident;});
 assert(cache.guard_failures==3&&cache.bytes<=cache.cap);assert(!coordinator.state(4).failed&&!coordinator.state(5).failed&&!coordinator.state(6).failed);
 std::printf("Same cap=%llu: baseline three shared HIGH banks rejected; remote32MiB retirement admits all12,582,900B atresident=%llu; nearby and apartment pins retained; guard unchanged PASS\n",(unsigned long long)cache.cap,(unsigned long long)cache.bytes);
 assert(coordinator.stop());std::puts("Interior unload boundary, opt-out policy preservation and symmetric remote release, invalid-input retention, exact-space retry and real asynchronous Coordinator integration PASS");
}
