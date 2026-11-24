#include "engine/world_regions.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <new>
#include <thread>
#include <unistd.h>

static std::atomic<bool> track{false};static std::thread::id tracked_owner;static std::atomic<unsigned> owner_allocations{0};
[[gnu::noinline]] void *operator new(std::size_t n){if(track.load()&&std::this_thread::get_id()==tracked_owner)owner_allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
using namespace world_regions;
static unsigned checks=0;
#define CHECK(x) do{checks++;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
static Document document(){
    Document d;d.assets={{"texture",{}},{"tree",{"texture"}}};
    for(unsigned i=0;i<3;i++){Region r;r.id=i+1;r.name="District "+std::to_string(i);r.bounds={{double(i*200),-10,-10},{double(i*200+20),10,10}};r.load_distance=0;r.preload_distance=30;r.unload_distance=60;Layer l;l.id=100+i;l.entities={1000+i};l.assets={"tree"};r.layers={l};d.regions.push_back(r);}return d;
}
struct Fake {
    std::thread::id owner=std::this_thread::get_id();std::atomic<bool> hold{false},started{false},fail_load{false},ignore_limit{false};
    std::atomic<unsigned> concurrent{0},peak_concurrent{0};std::atomic<bool> bad_thread{false};
    bool fail_upload=false,overshoot=false;std::uint64_t next_token=10,gpu=80,payload=100;
    std::vector<Id> loads,releases,uploads;std::vector<std::uint64_t> live;std::mutex record;
    std::atomic<bool> track_cleanup{false},owner_cleanup{false};
    struct CheckedCpu {Fake *fake;~CheckedCpu(){if(fake->track_cleanup.load()&&std::this_thread::get_id()==fake->owner)fake->owner_cleanup=true;}};
    Fake(){loads.reserve(1024);releases.reserve(1024);uploads.reserve(1024);live.reserve(1024);}
    static std::shared_ptr<const CpuBlob> load(const LoadRequest &request,void *context){
        auto &f=*static_cast<Fake*>(context);if(std::this_thread::get_id()==f.owner)f.bad_thread=true;
        unsigned n=f.concurrent.fetch_add(1)+1;f.peak_concurrent.store(std::max(f.peak_concurrent.load(),n));f.started=true;
        {std::lock_guard<std::mutex> lock(f.record);f.loads.push_back(request.region);}
        while(f.hold.load()&&!request.cancelled())std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if(request.cancelled()||f.fail_load.load()){f.concurrent--;return {};}
        if(request.asset_ids->size()!=2||(*request.asset_ids)[0]!="texture"||(*request.asset_ids)[1]!="tree")f.bad_thread=true;
        if(!f.ignore_limit&&f.payload+sizeof(CpuBlob)>request.cpu_limit){f.concurrent--;return {};}
        auto blob=std::make_shared<CpuBlob>();blob->data.resize(size_t(f.payload),42);blob->payload=std::shared_ptr<CheckedCpu>(new CheckedCpu{&f});blob->gpu_bytes=f.gpu;f.concurrent--;return blob;
    }
    static UploadResult upload(const UploadRequest &request,void *context){
        auto &f=*static_cast<Fake*>(context);if(std::this_thread::get_id()!=f.owner)f.bad_thread=true;
        std::uint64_t token=request.token;if(!token){token=f.next_token++;f.live.push_back(token);}
        auto count=std::min(request.byte_budget,request.blob->data.size()-request.offset);if(f.overshoot)count=request.byte_budget+1;
        f.uploads.push_back(request.region);return {!f.fail_upload,request.offset+count==request.blob->data.size(),token,count,f.gpu};
    }
    static void release(Id id,std::uint64_t token,void *context){auto &f=*static_cast<Fake*>(context);if(std::this_thread::get_id()!=f.owner)f.bad_thread=true;f.releases.push_back(id);auto at=std::find(f.live.begin(),f.live.end(),token);if(at==f.live.end())f.bad_thread=true;else f.live.erase(at);}
    Callbacks callbacks(){return {load,upload,release,this};}
};
static bool pump(Coordinator &c,Vec3 camera,std::uint64_t &frame,Id id,bool fail=false){
    for(unsigned i=0;i<2000;i++){
        if(!c.request(camera,++frame)||!c.tick())return false;
        const auto s=c.stats();if(s.frame_upload_bytes>30||s.frame_upload_jobs>1||s.resident_gpu_bytes+s.pending_gpu_reservation>160||s.resident_cpu_bytes+s.pending_cpu_bytes+s.worker_cpu_reservation>512)return false;
        auto state=c.state(id);if(fail?state.failed:state.resident&&!state.dirty)return true;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }return false;
}
static std::string bytes(const std::string &path){std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}

int main(){
    auto d=document();std::string error;CHECK(validate(d,error));CHECK(distance(d.regions[0].bounds,{10,0,0})==0);CHECK(std::abs(distance(d.regions[0].bounds,{23,14,10})-5)<1e-12);CHECK(editable(d,1,100));d.regions[0].locked=true;CHECK(!editable(d,1,100));d.regions[0].locked=false;d.regions[0].layers[0].locked=true;CHECK(!editable(d,1,100));d.regions[0].layers[0].locked=false;
    auto bad=d;bad.regions[1].id=1;CHECK(!validate(bad,error));bad=d;bad.regions[0].dependencies={2};bad.regions[1].dependencies={1};CHECK(!validate(bad,error));bad=d;bad.assets[0].dependencies={"tree"};CHECK(!validate(bad,error));bad=d;bad.regions[1].layers[0].entities={1000};CHECK(!validate(bad,error));bad=d;bad.regions[0].layers[0].assets={"missing"};CHECK(!validate(bad,error));bad=d;bad.regions[1].bounds=d.regions[0].bounds;CHECK(validate(bad,error)); // overlaps allowed

    Config config;config.cpu_bytes=512;config.gpu_bytes=160;config.upload_bytes_per_frame=30;config.upload_jobs_per_frame=1;Fake fake;Coordinator c;std::uint64_t frame=0;
    fake.track_cleanup=true;
    CHECK(c.configure(d,config,fake.callbacks(),error));CHECK(c.request({10,0,0},++frame)&&c.tick());CHECK(pump(c,{10,0,0},frame,1));CHECK(c.state(1).resident&&fake.peak_concurrent==1&&!fake.bad_thread);CHECK(fake.loads.size()==1);CHECK(fake.uploads.size()==4);
    for(unsigned i=0;i<2000&&c.stats().worker_cpu_reservation;i++){CHECK(c.tick());std::this_thread::sleep_for(std::chrono::microseconds(100));}
    CHECK(!c.stats().worker_cpu_reservation&&!fake.owner_cleanup);fake.track_cleanup=false;
    auto stat=c.stats();CHECK(stat.resident_gpu_bytes==80&&stat.resident_cpu_bytes==0);auto oldtoken=c.state(1).token;
    // Calling tick twice cannot multiply a frame's upload allowance.
    CHECK(c.reload(1));CHECK(c.request({10,0,0},++frame)&&c.tick());
    for(unsigned i=0;i<2000&&!c.stats().ready;i++){CHECK(c.tick());std::this_thread::sleep_for(std::chrono::microseconds(200));}
    auto used=c.stats().frame_upload_bytes;CHECK(used==30);for(unsigned i=0;i<50;i++)CHECK(c.tick());CHECK(c.stats().frame_upload_bytes==used);CHECK(c.state(1).token==oldtoken);
    CHECK(pump(c,{10,0,0},frame,1));CHECK(c.state(1).token!=oldtoken&&fake.releases.size()==1);
    // Hysteresis holds an already desired region beyond its preload boundary.
    CHECK(c.request({65,0,0},++frame)&&c.state(1).wanted);CHECK(c.request({85,0,0},++frame)&&!c.state(1).wanted);CHECK(c.state(1).resident);
    CHECK(pump(c,{210,0,0},frame,2));CHECK(c.state(1).resident&&c.state(2).resident);
    CHECK(pump(c,{410,0,0},frame,3));CHECK(!c.state(1).resident&&c.state(2).resident&&c.state(3).resident);CHECK(c.stats().evictions==1);
    CHECK(c.pin(2,true));CHECK(c.request({10000,0,0},++frame)&&c.state(2).wanted&&c.state(2).pinned);CHECK(c.pin(2,false));
    // Failed reloads keep the original GPU resident; retry is explicit.
    CHECK(c.request({410,0,0},++frame));oldtoken=c.state(3).token;fake.fail_load=true;CHECK(c.reload(3));CHECK(pump(c,{410,0,0},frame,3,true));CHECK(c.state(3).resident&&c.state(3).token==oldtoken);unsigned attempts=unsigned(fake.loads.size());for(unsigned i=0;i<30;i++){CHECK(c.request({410,0,0},++frame)&&c.tick());}CHECK(fake.loads.size()==attempts);
    fake.fail_load=false;CHECK(c.reload(3));fake.fail_upload=true;CHECK(pump(c,{410,0,0},frame,3,true));CHECK(c.state(3).token==oldtoken);fake.fail_upload=false;CHECK(c.reload(3));CHECK(pump(c,{410,0,0},frame,3));CHECK(c.state(3).token!=oldtoken);
    CHECK(c.stop()&&fake.live.empty()&&!fake.bad_thread);

    // A blocked CPU loader never blocks owner polling; generations cancel it.
    fake.hold=true;fake.started=false;CHECK(c.configure(d,config,fake.callbacks(),error));CHECK(c.request({10,0,0},++frame)&&c.tick());for(unsigned i=0;i<1000&&!fake.started;i++){CHECK(c.tick());std::this_thread::sleep_for(std::chrono::microseconds(100));}CHECK(fake.started);
    auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<1000;i++)CHECK(c.tick());CHECK(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(100));CHECK(c.request({210,0,0},++frame));fake.hold=false;CHECK(pump(c,{210,0,0},frame,2));CHECK(!c.state(1).resident&&c.stats().cancellations>=1);
    fake.hold=true;fake.started=false;CHECK(c.reload(2)&&c.request({210,0,0},++frame)&&c.tick());for(unsigned i=0;i<1000&&!fake.started;i++){CHECK(c.tick());std::this_thread::sleep_for(std::chrono::microseconds(100));}CHECK(fake.started);start=std::chrono::steady_clock::now();CHECK(c.stop());CHECK(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(250));CHECK(fake.live.empty());fake.hold=false;

    // Required region dependencies load first even when outside camera bounds.
    d=document();d.regions[0].dependencies={2};fake.loads.clear();CHECK(c.configure(d,config,fake.callbacks(),error));CHECK(pump(c,{10,0,0},frame,1));CHECK(fake.loads.size()==2&&fake.loads[0]==2&&fake.loads[1]==1);CHECK(c.state(2).wanted);CHECK(c.stop());
    // Priority wins distance among requests of the same demand class.
    d=document();d.regions[1].bounds=d.regions[0].bounds;d.regions[1].priority=10;fake.loads.clear();CHECK(c.configure(d,config,fake.callbacks(),error));CHECK(pump(c,{10,0,0},frame,1));CHECK(fake.loads[0]==2);CHECK(c.stop());
    // Enforced upload-report and loader memory reservations retain old content.
    d=document();CHECK(c.configure(d,config,fake.callbacks(),error));CHECK(pump(c,{10,0,0},frame,1));oldtoken=c.state(1).token;fake.overshoot=true;CHECK(c.reload(1));CHECK(pump(c,{10,0,0},frame,1,true));CHECK(c.state(1).token==oldtoken);fake.overshoot=false;fake.payload=1000;fake.ignore_limit=true;CHECK(c.reload(1));CHECK(pump(c,{10,0,0},frame,1,true));CHECK(c.state(1).token==oldtoken);fake.payload=100;fake.ignore_limit=false;CHECK(c.stop());
    // Invalid reconfiguration preserves the running coordinator and its handles.
    CHECK(c.configure(d,config,fake.callbacks(),error));CHECK(pump(c,{10,0,0},frame,1));oldtoken=c.state(1).token;bad=d;bad.regions[0].id=0;CHECK(!c.configure(bad,config,fake.callbacks(),error));CHECK(c.state(1).token==oldtoken);
    bool foreign_tick=true;std::thread foreign([&]{foreign_tick=c.tick();});foreign.join();CHECK(!foreign_tick);
    tracked_owner=std::this_thread::get_id();owner_allocations=0;track=true;bool hot=true;for(unsigned i=0;i<10000;i++)hot=hot&&c.request({10,0,0},++frame)&&c.tick();track=false;CHECK(hot&&owner_allocations==0);CHECK(c.stop()&&fake.live.empty());

    // A renderer can transfer eager handles, then use exact nearest-placement
    // demands instead of a giant AABB covering disconnected world locations.
    d=document();CHECK(c.configure(d,config,fake.callbacks(),error));
    fake.live.push_back(90001);CHECK(c.adopt(1,90001,0,80));CHECK(!c.adopt(1,90002,0,80));
    CHECK(c.demand(1,0,99999)&&c.request({10,0,0},++frame));CHECK(!c.state(1).wanted&&c.state(1).resident);
    CHECK(c.retire(1)&&std::find(fake.live.begin(),fake.live.end(),90001)==fake.live.end()&&!c.retire(1));
    CHECK(c.demand(1,2,3)&&c.request({99999,0,0},++frame));CHECK(c.state(1).wanted&&c.state(1).camera_distance==3);
    CHECK(pump(c,{99999,0,0},frame,1));CHECK(!c.retire(1));
    CHECK(c.pin(1,true)&&c.demand(1,0,9999)&&c.request({99999,0,0},++frame));CHECK(c.state(1).wanted&&!c.retire(1));
    CHECK(c.pin(1,false)&&c.request({99999,0,0},++frame)&&c.retire(1));CHECK(c.stop()&&fake.live.empty());

    char folder[]="/tmp/valwood-regions-test-XXXXXX";CHECK(mkdtemp(folder));std::string a=std::string(folder)+"/world.regions",b=std::string(folder)+"/copy.regions",broken=std::string(folder)+"/bad.regions";CHECK(save(a.c_str(),d,error));Document loaded;CHECK(load(a.c_str(),loaded,error)&&save(b.c_str(),loaded,error));CHECK(bytes(a)==bytes(b));auto original=bytes(a);bad=d;bad.assets[0].dependencies={"tree"};CHECK(!save(a.c_str(),bad,error)&&bytes(a)==original);
    for(const std::string &text:{std::string("VALWOOD_REGIONS 9\n"),std::string("VALWOOD_REGIONS 1\nregion -1 \"bad\" 0 0 0 1 1 1 0 1 0 0 1 2 0\n"),std::string("VALWOOD_REGIONS 1\nasset \"a\" 1 \"b\"\n"),std::string("VALWOOD_REGIONS 1\nunknown 5\n")}){std::ofstream file(broken);file<<text;file.close();CHECK(!load(broken.c_str(),loaded,error));CHECK(loaded.regions.size()==d.regions.size());}
    std::string link=std::string(folder)+"/link.regions";CHECK(symlink(a.c_str(),link.c_str())==0);CHECK(!save(link.c_str(),d,error)&&!load(link.c_str(),loaded,error));unlink(link.c_str());unlink(a.c_str());unlink(b.c_str());unlink(broken.c_str());CHECK(rmdir(folder)==0);
    std::cout<<"world_regions: "<<checks<<" authoring/residency/codec contracts; single worker, owner-only budgeted uploads, no allocation in 10,000 steady ticks\n";return 0;
}
