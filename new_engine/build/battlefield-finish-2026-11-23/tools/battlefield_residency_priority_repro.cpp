#include "engine/world_regions.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
using namespace world_regions;
struct Trace{std::vector<Id> upload_order;};
static std::shared_ptr<const CpuBlob> load(const LoadRequest&,void*){auto b=std::make_shared<CpuBlob>();b->data={1};b->cpu_bytes=sizeof(CpuBlob)+b->data.capacity();b->gpu_bytes=4;b->upload_bytes=1;return b;}
static UploadResult upload(const UploadRequest&r,void*p){static_cast<Trace*>(p)->upload_order.push_back(r.region);return {true,true,r.region,1,4};}
static void release(Id,std::uint64_t,void*){}
static bool run(bool pins){Document d;for(unsigned i=1;i<=2;i++){Region r;r.id=i;r.name=i==1?"far fallback":"near destination";r.priority=1;r.bounds={{0,-1,0},{1,1,1}};Layer l;l.id=i;r.layers.push_back(l);d.regions.push_back(r);}Coordinator c;Trace t;Config cfg;cfg.upload_jobs_per_frame=1;std::string e;if(!c.configure(d,cfg,{load,upload,release,&t},e)){fprintf(stderr,"%s\n",e.c_str());return false;}c.demand(1,2,450);c.demand(2,2,10);c.pin(1,pins);c.pin(2,pins);c.request({-11968,0,5582},1);const auto a=c.state(1),b=c.state(2);for(unsigned i=2;i<2000&&t.upload_order.size()<2;i++){c.request({-11968,0,5582},i);c.tick();std::this_thread::sleep_for(std::chrono::milliseconds(1));}printf("%s camera_distances %.6f %.6f uploads",pins?"pinned":"unpinned",a.camera_distance,b.camera_distance);for(auto id:t.upload_order)printf(" %llu",(unsigned long long)id);puts("");return t.upload_order.size()==2&&(t.upload_order[0]==2&&a.camera_distance==450&&b.camera_distance==10);}
int main(){if(!run(true)||!run(false))return 1;puts("PASS: pinned and unpinned requests both honor actual nearest distance");}
