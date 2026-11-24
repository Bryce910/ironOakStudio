#include "engine/world_platform.h"
#include "engine/world_regions.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <unordered_set>
#include <unistd.h>

namespace world_regions {
#ifdef VALWOOD_QA_REGIONS_SHUTDOWN
// Linked only by the CPU shutdown regression executable. No exported runtime
// API or hook invocation exists in ordinary engine builds.
void shutdown_test_wait_predicate(bool ready);
void shutdown_test_stop_enter();
#endif
namespace {
bool range(double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;}
bool vec(Vec3 v){return range(v.x,-1e8,1e8)&&range(v.y,-1e8,1e8)&&range(v.z,-1e8,1e8);}
bool title(const std::string &s){return !s.empty()&&s.size()<=95&&s.find_first_of("\r\n\t")==std::string::npos&&s.find('\0')==std::string::npos;}
bool asset_id(const std::string &s){return !s.empty()&&s.size()<=95&&std::all_of(s.begin(),s.end(),[](unsigned char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c==':';});}
int region_index(const Document &d,Id id){if(id&&id<=d.regions.size()&&d.regions[std::size_t(id-1)].id==id)return int(id-1);for(unsigned i=0;i<d.regions.size();i++)if(d.regions[i].id==id)return int(i);return -1;}
int asset_index(const Document &d,const std::string &id){for(unsigned i=0;i<d.assets.size();i++)if(d.assets[i].id==id)return int(i);return -1;}
bool read_id(std::istream &in,Id &out){std::string s;if(!(in>>s)||s.empty()||s.size()>20||!std::all_of(s.begin(),s.end(),[](char c){return c>='0'&&c<='9';}))return false;try{out=std::stoull(s);return out!=0;}catch(...){return false;}}
std::uint64_t cpu_bytes(const CpuBlob &blob){return blob.cpu_bytes?blob.cpu_bytes:sizeof(CpuBlob)+blob.data.capacity();}
std::uint64_t transfer_bytes(const CpuBlob &blob){return blob.upload_bytes?blob.upload_bytes:blob.data.size();}
}

double distance(const Bounds &b,Vec3 p){double x=std::max({b.minimum.x-p.x,0.,p.x-b.maximum.x}),y=std::max({b.minimum.y-p.y,0.,p.y-b.maximum.y}),z=std::max({b.minimum.z-p.z,0.,p.z-b.maximum.z});return std::hypot(std::hypot(x,y),z);}
bool editable(const Document &d,Id region,Id layer){for(const auto &r:d.regions)if(r.id==region&&!r.locked)for(const auto &l:r.layers)if(l.id==layer)return !l.locked;return false;}
bool LoadRequest::cancelled()const{return !current_generation||!stopping||stopping->load(std::memory_order_acquire)||current_generation->load(std::memory_order_acquire)!=generation;}

bool validate(const Document &d,std::string &error){
    error.clear();auto fail=[&](const char *s){error=s;return false;};
    if(d.regions.size()>region_limit||d.assets.size()>asset_limit)return fail("Region/asset document budget exceeded");
    std::unordered_set<Id> regions,layers,entities;std::unordered_set<std::string> assets;
    for(const auto &a:d.assets)if(!asset_id(a.id)||!assets.insert(a.id).second||a.dependencies.size()>dependency_limit)return fail("Invalid/duplicate asset ID or dependency budget");
    for(const auto &r:d.regions){
        if(!r.id||!regions.insert(r.id).second||!title(r.name)||!vec(r.bounds.minimum)||!vec(r.bounds.maximum)||
            r.bounds.minimum.x>=r.bounds.maximum.x||r.bounds.minimum.y>=r.bounds.maximum.y||r.bounds.minimum.z>=r.bounds.maximum.z||
            r.priority< -1000||r.priority>1000||!range(r.load_distance,0,100000)||!range(r.preload_distance,r.load_distance,100000)||
            !range(r.unload_distance,r.preload_distance,100000)||r.dependencies.size()>dependency_limit||r.layers.size()>layer_limit)return fail("Invalid region bounds/ID/distance/layer budget");
        for(const auto &l:r.layers){
            if(!l.id||!layers.insert(l.id).second||!title(l.name)||l.priority< -1000||l.priority>1000||l.assets.size()>asset_limit)return fail("Invalid/duplicate layer ID");
            for(Id id:l.entities)if(!id||!entities.insert(id).second||entities.size()>entity_limit)return fail("Entity IDs must be uniquely owned by one region/layer");
            std::unordered_set<std::string> unique;for(const auto &a:l.assets)if(!assets.count(a)||!unique.insert(a).second)return fail("Missing/duplicate layer asset dependency");
        }
    }
    for(const auto &a:d.assets){std::unordered_set<std::string> unique;for(const auto &dep:a.dependencies)if(!assets.count(dep)||!unique.insert(dep).second)return fail("Missing/duplicate asset dependency");}
    for(const auto &r:d.regions){std::unordered_set<Id> unique;for(Id id:r.dependencies)if(!regions.count(id)||!unique.insert(id).second)return fail("Missing/duplicate region dependency");}
    std::array<unsigned char,asset_limit> ac={};auto visit_asset=[&](auto &&self,unsigned i)->bool{if(ac[i]==1)return false;if(ac[i]==2)return true;ac[i]=1;for(const auto &dep:d.assets[i].dependencies)if(!self(self,unsigned(asset_index(d,dep))))return false;ac[i]=2;return true;};
    for(unsigned i=0;i<d.assets.size();i++)if(!visit_asset(visit_asset,i))return fail("Cyclic asset dependencies");
    std::array<unsigned char,region_limit> rc={};auto visit_region=[&](auto &&self,unsigned i)->bool{if(rc[i]==1)return false;if(rc[i]==2)return true;rc[i]=1;for(Id id:d.regions[i].dependencies)if(!self(self,unsigned(region_index(d,id))))return false;rc[i]=2;return true;};
    for(unsigned i=0;i<d.regions.size();i++)if(!visit_region(visit_region,i))return fail("Cyclic region dependencies");
    return true;
}

struct Coordinator::Impl {
    struct Entry {State state;std::atomic<std::uint64_t> generation{1};std::shared_ptr<const CpuBlob> cpu;std::vector<std::string> assets;std::vector<unsigned> dependencies;int demand=0,priority=0,explicit_demand=-1;double explicit_distance=0;};
    std::thread::id owner=std::this_thread::get_id();Document document;Config config;Callbacks callbacks;
    std::array<Entry,region_limit> entries;std::vector<unsigned> topology;Stats totals;
    bool configured=false,have_camera=false;Vec3 camera;std::uint64_t frame=0,last_tick_frame=std::numeric_limits<std::uint64_t>::max();
    std::thread worker;std::mutex mutex;std::condition_variable wake;std::atomic<bool> stopping{false};
    bool queued=false,completed=false,outstanding=false;unsigned work_index=0;
    LoadRequest job;std::shared_ptr<const CpuBlob> result;std::array<char,192> result_error={};
    std::shared_ptr<const CpuBlob> retired,garbage;
    std::atomic<std::uint64_t> cleanup_bytes{0};
    std::uint64_t worker_limit=0;
    struct Ready {bool active=false,reserved=false;unsigned index=0;std::uint64_t generation=0,token=0,offset=0,gpu=0;std::shared_ptr<const CpuBlob> blob;} ready;
    bool owner_thread()const{return owner==std::this_thread::get_id();}
    void error(const char *text){std::snprintf(totals.last_error.data(),totals.last_error.size(),"%s",text?text:"Region operation failed");}
    void run(){
        for(;;){LoadRequest request;std::shared_ptr<const CpuBlob> discard;bool have_job=false;{
            std::unique_lock<std::mutex> lock(mutex);wake.wait(lock,[&]{
                const bool ready=stopping.load(std::memory_order_acquire)||queued||bool(garbage);
#ifdef VALWOOD_QA_REGIONS_SHUTDOWN
                shutdown_test_wait_predicate(ready);
#endif
                return ready;
            });
            if(stopping.load(std::memory_order_acquire))break;
            discard=std::move(garbage);if(queued){request=job;queued=false;have_job=true;}
        }
            if(discard){discard.reset();cleanup_bytes.store(0,std::memory_order_release);}
            if(!have_job)continue;
            std::array<char,192> message={};request.error=message.data();request.error_capacity=message.size();std::shared_ptr<const CpuBlob> blob;
            if(!request.cancelled()){try{blob=callbacks.load(request,callbacks.context);}catch(...){std::snprintf(message.data(),message.size(),"CPU region loader threw an exception");}}
            std::lock_guard<std::mutex> lock(mutex);result=std::move(blob);result_error=message;completed=true;
        }
    }
    void retire_blob(std::shared_ptr<const CpuBlob> blob){if(!blob)return;cleanup_bytes.store(cpu_bytes(*blob),std::memory_order_release);retired=std::move(blob);}
    void send_garbage(){if(!retired)return;std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(lock.owns_lock()&&!garbage){garbage=std::move(retired);wake.notify_one();}}
    std::uint64_t resident_cpu()const{std::uint64_t total=0;for(unsigned i=0;i<document.regions.size();i++)total+=entries[i].state.cpu_bytes;return total;}
    std::uint64_t resident_gpu()const{std::uint64_t total=0;for(unsigned i=0;i<document.regions.size();i++)total+=entries[i].state.gpu_bytes;return total;}
    void release_token(unsigned index,std::uint64_t token){if(token&&callbacks.release)callbacks.release(document.regions[index].id,token,callbacks.context);}
    void evict(unsigned index){auto &e=entries[index];release_token(index,e.state.token);e.cpu.reset();e.state.resident=false;e.state.token=e.state.cpu_bytes=e.state.gpu_bytes=e.state.resident_generation=0;totals.evictions++;}
    bool make_space(std::uint64_t cpu,std::uint64_t gpu){
        while(resident_cpu()+cpu>config.cpu_bytes||resident_gpu()+gpu>config.gpu_bytes){
            int candidate=-1;for(unsigned i=0;i<document.regions.size();i++){const auto &s=entries[i].state;if(!s.resident||s.wanted||s.pinned)continue;if(candidate<0||s.last_used<entries[unsigned(candidate)].state.last_used||(s.last_used==entries[unsigned(candidate)].state.last_used&&document.regions[i].id<document.regions[unsigned(candidate)].id))candidate=int(i);}
            if(candidate<0)return false;
            evict(unsigned(candidate));
        }return true;
    }
    void discard_ready(bool failure,const char *reason){
        if(!ready.active)return;
        auto &e=entries[ready.index];release_token(ready.index,ready.token);e.state.loading=false;
        if(failure){e.state.failed=true;e.state.dirty=true;totals.failures++;error(reason);}else totals.cancellations++;
        retire_blob(std::move(ready.blob));ready={};
    }
    bool stop(){
#ifdef VALWOOD_QA_REGIONS_SHUTDOWN
        if(worker.joinable())shutdown_test_stop_enter();
#endif
        // The condition predicate must change under the same mutex used by
        // wait(). Otherwise stop can notify between the worker's false
        // predicate check and its sleep, leaving join() waiting indefinitely.
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping.store(true,std::memory_order_release);
            for(auto &e:entries)e.generation.fetch_add(1,std::memory_order_acq_rel);
        }
        wake.notify_one();
        if(worker.joinable())worker.join();
        // Caller/destructor must be the owner. A wrong-thread destructor joins
        // CPU work but intentionally never calls a potential GL releaser there.
        if(owner_thread()){
            if(ready.active)release_token(ready.index,ready.token);
            for(unsigned i=0;i<document.regions.size();i++)if(entries[i].state.resident)release_token(i,entries[i].state.token);
        }
        ready={};result.reset();retired.reset();garbage.reset();cleanup_bytes.store(0,std::memory_order_release);for(auto &e:entries){e.cpu.reset();e.state={};e.assets.clear();e.dependencies.clear();e.explicit_demand=-1;e.explicit_distance=0;}
        outstanding=queued=completed=configured=false;worker_limit=0;document={};topology.clear();return owner_thread();
    }
};

Coordinator::Coordinator():impl_(new Impl){}
Coordinator::~Coordinator(){impl_->stop();}
bool Coordinator::configure(const Document &d,const Config &config,const Callbacks &callbacks,std::string &error){
    auto &p=*impl_;if(!p.owner_thread()){error="Region configuration requires the owner thread";return false;}
    if(!validate(d,error))return false;
    if(!callbacks.load||!callbacks.upload||!callbacks.release||!config.cpu_bytes||config.cpu_bytes>(1ull<<48)||config.gpu_bytes>(1ull<<48)||!config.upload_bytes_per_frame||!config.upload_jobs_per_frame||config.upload_jobs_per_frame>64){error="Invalid region callbacks/budgets";return false;}
    Document next=d;p.stop();p.document=std::move(next);p.config=config;p.callbacks=callbacks;p.totals={};p.frame=0;p.last_tick_frame=std::numeric_limits<std::uint64_t>::max();p.have_camera=false;
    for(unsigned i=0;i<p.document.regions.size();i++){
        auto &e=p.entries[i];e.generation.store(1,std::memory_order_release);e.state.generation=1;
        for(Id id:p.document.regions[i].dependencies)e.dependencies.push_back(unsigned(region_index(p.document,id)));
        std::array<bool,asset_limit> seen={};auto add=[&](auto &&self,unsigned n)->void{if(seen[n])return;seen[n]=true;for(const auto &id:p.document.assets[n].dependencies)self(self,unsigned(asset_index(p.document,id)));e.assets.push_back(p.document.assets[n].id);};
        for(const auto &layer:p.document.regions[i].layers)if(layer.visible)for(const auto &id:layer.assets)add(add,unsigned(asset_index(p.document,id)));
    }
    std::array<bool,region_limit> seen={};auto visit=[&](auto &&self,unsigned i)->void{if(seen[i])return;seen[i]=true;for(unsigned dep:p.entries[i].dependencies)self(self,dep);p.topology.push_back(i);};
    for(unsigned i=0;i<p.document.regions.size();i++)visit(visit,i);
    p.stopping.store(false,std::memory_order_release);p.configured=true;
    try{p.worker=std::thread([&p]{p.run();});}catch(...){p.configured=false;error="Cannot start the region CPU worker";return false;}
    error.clear();return true;
}

bool Coordinator::request(Vec3 camera,std::uint64_t frame){
    auto &p=*impl_;if(!p.owner_thread()||!p.configured||!vec(camera)||(p.have_camera&&frame<p.frame))return false;
    p.camera=camera;p.frame=frame;p.have_camera=true;
    for(unsigned i=0;i<p.document.regions.size();i++){
        auto &e=p.entries[i];const auto &r=p.document.regions[i];e.state.camera_distance=distance(r.bounds,camera);
        bool visible=false;int priority=-1000;for(const auto &l:r.layers)if(l.visible){visible=true;priority=std::max(priority,l.priority);}
        e.priority=r.priority+(visible?priority:0);e.demand=e.state.pinned?2:r.visible&&visible?(e.state.camera_distance<=r.load_distance?2:e.state.camera_distance<=r.preload_distance?1:e.state.wanted&&e.state.camera_distance<=r.unload_distance?1:0):0;
        if(e.explicit_demand>=0){if(!e.state.pinned)e.demand=e.explicit_demand;e.state.camera_distance=e.explicit_distance;}
    }
    for(auto at=p.topology.rbegin();at!=p.topology.rend();++at){auto &e=p.entries[*at];if(e.demand)for(unsigned dep:e.dependencies){auto &other=p.entries[dep];other.demand=std::max(other.demand,e.demand);other.priority=std::max(other.priority,e.priority+1);}}
    for(unsigned i=0;i<p.document.regions.size();i++){
        auto &e=p.entries[i];bool wanted=e.demand>0;if(e.state.wanted&&!wanted)e.generation.fetch_add(1,std::memory_order_acq_rel);e.state.wanted=wanted;e.state.generation=e.generation.load(std::memory_order_acquire);if(wanted)e.state.last_used=frame;
    }return true;
}

bool Coordinator::tick(FrameBudget budget){
    auto &p=*impl_;if(!p.owner_thread()||!p.configured)return false;
    p.send_garbage();
    if(p.frame!=p.last_tick_frame){p.totals.frame_upload_bytes=p.totals.frame_upload_jobs=0;p.last_tick_frame=p.frame;}
    const std::uint64_t bytes=budget.bytes?std::min(budget.bytes,p.config.upload_bytes_per_frame):p.config.upload_bytes_per_frame;
    const unsigned jobs=budget.jobs?std::min(budget.jobs,p.config.upload_jobs_per_frame):p.config.upload_jobs_per_frame;
    if(p.outstanding){std::unique_lock<std::mutex> lock(p.mutex,std::try_to_lock);if(lock.owns_lock()&&p.completed){
        auto blob=std::move(p.result);auto message=p.result_error;p.completed=false;p.outstanding=false;auto &e=p.entries[p.work_index];
        const auto generation=p.job.generation,limit=p.worker_limit;p.worker_limit=0;
        if(e.generation.load(std::memory_order_acquire)!=generation||!e.state.wanted){e.state.loading=false;p.totals.cancellations++;p.retire_blob(std::move(blob));}
        else if(!blob){e.state.loading=false;e.state.failed=true;e.state.dirty=true;p.totals.failures++;p.error(*message.data()?message.data():"CPU region load failed");}
        else if(cpu_bytes(*blob)<sizeof(CpuBlob)+blob->data.capacity()||cpu_bytes(*blob)>limit||blob->gpu_bytes>p.config.gpu_bytes||transfer_bytes(*blob)>p.config.cpu_bytes+p.config.gpu_bytes){e.state.loading=false;e.state.failed=true;e.state.dirty=true;p.totals.failures++;p.error("CPU blob exceeded its reserved CPU/GPU/transfer budget");}
        else {p.ready.active=true;p.ready.index=p.work_index;p.ready.generation=generation;p.ready.blob=std::move(blob);}
    }}
    if(p.ready.active){auto &e=p.entries[p.ready.index];if(!e.state.wanted||e.generation.load(std::memory_order_acquire)!=p.ready.generation)p.discard_ready(false,nullptr);}
    if(p.ready.active&&!p.ready.reserved){if(p.make_space(cpu_bytes(*p.ready.blob),p.ready.blob->gpu_bytes))p.ready.reserved=true;else p.totals.budget_waits++;}
    while(p.ready.active&&p.ready.reserved&&p.totals.frame_upload_jobs<jobs&&p.totals.frame_upload_bytes<bytes){
        auto &ready=p.ready;auto &entry=p.entries[ready.index];const auto &blob=*ready.blob;const auto allowance=bytes-p.totals.frame_upload_bytes;
        UploadRequest request{p.document.regions[ready.index].id,ready.generation,ready.token,ready.offset,allowance,&blob};UploadResult result;
        try{result=p.callbacks.upload(request,p.callbacks.context);}catch(...){result.success=false;}
        p.totals.frame_upload_jobs++;
        const bool token_valid=(!ready.token||result.token==ready.token)&&(!result.token||!entry.state.resident||result.token!=entry.state.token);
        const auto total=transfer_bytes(blob);
        const bool amounts=result.consumed_bytes<=allowance&&ready.offset<=total&&result.consumed_bytes<=total-ready.offset&&result.gpu_bytes>=ready.gpu&&result.gpu_bytes<=blob.gpu_bytes;
        if(!result.success||!token_valid||!amounts||(result.complete&&((!blob.upload_bytes_upper_bound&&ready.offset+result.consumed_bytes!=total)||(result.gpu_bytes&&!result.token)))){
            if(result.token&&result.token!=ready.token&&result.token!=entry.state.token)p.release_token(ready.index,result.token);
            p.discard_ready(true,"Owner region upload failed/violated token or byte budget");break;
        }
        ready.token=result.token;ready.offset+=result.consumed_bytes;ready.gpu=result.gpu_bytes;p.totals.frame_upload_bytes+=result.consumed_bytes;
        if(result.complete){
            p.release_token(ready.index,entry.state.token);entry.state.resident=true;entry.state.resident_generation=ready.generation;entry.state.token=ready.token;entry.state.gpu_bytes=ready.gpu;entry.state.cpu_bytes=p.config.keep_cpu?cpu_bytes(blob):0;entry.cpu=p.config.keep_cpu?ready.blob:nullptr;entry.state.loading=entry.state.failed=entry.state.dirty=false;p.totals.uploads++;if(!p.config.keep_cpu)p.retire_blob(std::move(ready.blob));ready={};
        }else if(!result.consumed_bytes)break;
    }
    if(!p.outstanding&&!p.ready.active&&!p.retired&&!p.cleanup_bytes.load(std::memory_order_acquire)){
        int best=-1;for(unsigned i=0;i<p.document.regions.size();i++){
            const auto &e=p.entries[i];if(!e.state.wanted||e.state.failed||(e.state.resident&&!e.state.dirty))continue;
            bool deps=true;for(unsigned dep:e.dependencies)if(!p.entries[dep].state.resident||p.entries[dep].state.dirty)deps=false;if(!deps)continue;
            if(best<0||e.demand>p.entries[unsigned(best)].demand||(e.demand==p.entries[unsigned(best)].demand&&(e.priority>p.entries[unsigned(best)].priority||(e.priority==p.entries[unsigned(best)].priority&&(e.state.camera_distance<p.entries[unsigned(best)].state.camera_distance||(e.state.camera_distance==p.entries[unsigned(best)].state.camera_distance&&p.document.regions[i].id<p.document.regions[unsigned(best)].id))))))best=int(i);
        }
        if(best>=0){
            if(!p.make_space(sizeof(CpuBlob)+1,0)){p.totals.budget_waits++;return true;}
            std::unique_lock<std::mutex> lock(p.mutex,std::try_to_lock);if(lock.owns_lock()){
                auto &e=p.entries[unsigned(best)];p.worker_limit=p.config.cpu_bytes-p.resident_cpu();p.work_index=unsigned(best);
                p.job={p.document.regions[unsigned(best)].id,e.generation.load(std::memory_order_acquire),p.worker_limit,&p.document.regions[unsigned(best)],&e.assets,&e.generation,&p.stopping,nullptr,0};
                e.state.loading=true;p.queued=p.outstanding=true;p.totals.loads++;p.wake.notify_one();
            }
        }
    }return true;
}

bool Coordinator::pin(Id id,bool pinned){auto &p=*impl_;if(!p.owner_thread()||!p.configured)return false;int i=region_index(p.document,id);if(i<0)return false;p.entries[unsigned(i)].state.pinned=pinned;return request(p.camera,p.frame);}
bool Coordinator::demand(Id id,int value,double nearest){auto &p=*impl_;if(!p.owner_thread()||!p.configured||value< -1||value>2||!std::isfinite(nearest)||nearest<0)return false;const int i=region_index(p.document,id);if(i<0)return false;auto &e=p.entries[unsigned(i)];e.explicit_demand=value;e.explicit_distance=nearest;return true;}
bool Coordinator::adopt(Id id,std::uint64_t token,std::uint64_t cpu,std::uint64_t gpu){auto &p=*impl_;if(!p.owner_thread()||!p.configured||!token||cpu>p.config.cpu_bytes||gpu>(1ull<<48))return false;const int i=region_index(p.document,id);if(i<0)return false;auto &e=p.entries[unsigned(i)];if(e.state.resident||e.state.loading||(p.ready.active&&p.ready.index==unsigned(i)))return false;e.state.resident=true;e.state.token=token;e.state.cpu_bytes=cpu;e.state.gpu_bytes=gpu;e.state.resident_generation=e.state.generation;e.state.last_used=p.frame;return true;}
bool Coordinator::retire(Id id){auto &p=*impl_;if(!p.owner_thread()||!p.configured)return false;const int i=region_index(p.document,id);if(i<0)return false;auto &e=p.entries[unsigned(i)];if(!e.state.resident||e.state.pinned||e.state.wanted||e.state.loading||(p.ready.active&&p.ready.index==unsigned(i)))return false;p.evict(unsigned(i));return true;}
bool Coordinator::reload(Id id){auto &p=*impl_;if(!p.owner_thread()||!p.configured)return false;int i=region_index(p.document,id);if(i<0)return false;auto &e=p.entries[unsigned(i)];e.state.generation=e.generation.fetch_add(1,std::memory_order_acq_rel)+1;e.state.failed=false;e.state.dirty=true;return true;}
State Coordinator::state(Id id)const{const auto &p=*impl_;if(!p.owner_thread())return {};int i=region_index(p.document,id);return i<0?State{}:p.entries[unsigned(i)].state;}
Stats Coordinator::stats()const{const auto &p=*impl_;if(!p.owner_thread())return {};auto out=p.totals;for(unsigned i=0;i<p.document.regions.size();i++){const auto &s=p.entries[i].state;out.resident+=s.resident;out.wanted+=s.wanted;out.failed+=s.failed;out.resident_cpu_bytes+=s.cpu_bytes;out.resident_gpu_bytes+=s.gpu_bytes;}out.loading=p.outstanding;out.worker_cpu_reservation=p.worker_limit+p.cleanup_bytes.load(std::memory_order_acquire);if(p.ready.active){out.ready=1;out.pending_cpu_bytes=cpu_bytes(*p.ready.blob);out.pending_gpu_reservation=p.ready.reserved?p.ready.blob->gpu_bytes:0;out.pending_gpu_bytes=p.ready.gpu;}return out;}
bool Coordinator::stop(){if(!impl_->owner_thread())return false;return impl_->stop();}

bool save(const char *path,const Document &d,std::string &error){
    if(!path||!*path){error="Choose a region file";return false;}if(!validate(d,error))return false;struct stat old={};if(lstat(path,&old)==0&&!S_ISREG(old.st_mode)){error="Region destination must be a regular file";return false;}
    std::ostringstream out;out<<std::setprecision(17)<<"VALWOOD_REGIONS 1\n";
    for(const auto &a:d.assets){out<<"asset "<<std::quoted(a.id)<<' '<<a.dependencies.size();for(const auto &id:a.dependencies)out<<' '<<std::quoted(id);out<<'\n';}
    for(const auto &r:d.regions){out<<"region "<<r.id<<' '<<std::quoted(r.name)<<' '<<r.bounds.minimum.x<<' '<<r.bounds.minimum.y<<' '<<r.bounds.minimum.z<<' '<<r.bounds.maximum.x<<' '<<r.bounds.maximum.y<<' '<<r.bounds.maximum.z<<' '<<r.priority<<' '<<r.visible<<' '<<r.locked<<' '<<r.load_distance<<' '<<r.preload_distance<<' '<<r.unload_distance<<' '<<r.dependencies.size();for(Id id:r.dependencies)out<<' '<<id;out<<'\n';
        for(const auto &l:r.layers){out<<"layer "<<r.id<<' '<<l.id<<' '<<std::quoted(l.name)<<' '<<l.priority<<' '<<l.visible<<' '<<l.locked<<' '<<l.entities.size();for(Id id:l.entities)out<<' '<<id;out<<' '<<l.assets.size();for(const auto &id:l.assets)out<<' '<<std::quoted(id);out<<'\n';}
    }
    const auto bytes=out.str();if(bytes.size()>document_byte_limit){error="Region document exceeds file budget";return false;}
    std::string pattern=std::string(path)+".tmp.XXXXXX";std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);int fd=mkstemp(temporary.data());if(fd<0){error="Cannot create region save transaction";return false;}
    size_t at=0;bool ok=true;while(at<bytes.size()){ssize_t n=write(fd,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=size_t(n);}if(ok)ok=fsync(fd)==0;int closed=close(fd);ok=ok&&closed==0;if(ok)ok=vw_replace(temporary.data(),path)==0;if(!ok){unlink(temporary.data());error="Region save failed; previous file preserved";return false;}
    const char *slash=std::strrchr(path,'/');std::string directory=slash?std::string(path,size_t(slash-path)):".";if(directory.empty())directory="/";fd=vw_open(directory.c_str(),O_RDONLY|O_DIRECTORY);if(fd>=0){(void)fsync(fd);close(fd);}error.clear();return true;
}

bool load(const char *path,Document &out,std::string &error){
    error.clear();if(!path||!*path){error="Choose a region file";return false;}int fd=vw_open(path,O_RDONLY|O_NOFOLLOW);if(fd<0){error="Cannot open region document";return false;}
    struct stat info={};if(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_size<0||std::uint64_t(info.st_size)>document_byte_limit){close(fd);error="Invalid/oversized region document";return false;}
    std::string bytes(size_t(info.st_size),'\0');size_t at=0;bool ok=true;while(at<bytes.size()){ssize_t n=read(fd,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=size_t(n);}close(fd);if(!ok||bytes.find('\0')!=std::string::npos){error="Incomplete/binary region document";return false;}
    std::istringstream file(bytes);std::string row;std::getline(file,row);if(row!="VALWOOD_REGIONS 1"){error="Unsupported region document";return false;}Document next;unsigned line=1,total_entities=0;
    while(std::getline(file,row)){line++;std::istringstream in(row);std::string kind;in>>kind;if(kind.empty()||kind[0]=='#')continue;bool parsed=false;
        if(kind=="asset"&&next.assets.size()<asset_limit){Asset a;unsigned count=0;parsed=bool(in>>std::quoted(a.id)>>count)&&count<=dependency_limit;for(unsigned i=0;i<count&&parsed;i++){std::string id;parsed=bool(in>>std::quoted(id));a.dependencies.push_back(std::move(id));}next.assets.push_back(std::move(a));}
        else if(kind=="region"&&next.regions.size()<region_limit){Region r;unsigned count=0;int visible=0,locked=0;parsed=read_id(in,r.id)&&bool(in>>std::quoted(r.name)>>r.bounds.minimum.x>>r.bounds.minimum.y>>r.bounds.minimum.z>>r.bounds.maximum.x>>r.bounds.maximum.y>>r.bounds.maximum.z>>r.priority>>visible>>locked>>r.load_distance>>r.preload_distance>>r.unload_distance>>count)&&visible>=0&&visible<=1&&locked>=0&&locked<=1&&count<=dependency_limit;r.visible=visible==1;r.locked=locked==1;for(unsigned i=0;i<count&&parsed;i++){Id id=0;parsed=read_id(in,id);r.dependencies.push_back(id);}next.regions.push_back(std::move(r));}
        else if(kind=="layer"){Id region=0;Layer l;unsigned count=0,assets=0;int visible=0,locked=0;parsed=read_id(in,region)&&read_id(in,l.id)&&bool(in>>std::quoted(l.name)>>l.priority>>visible>>locked>>count)&&visible>=0&&visible<=1&&locked>=0&&locked<=1&&count<=entity_limit-total_entities;l.visible=visible==1;l.locked=locked==1;if(parsed)total_entities+=count;for(unsigned i=0;i<count&&parsed;i++){Id id=0;parsed=read_id(in,id);l.entities.push_back(id);}parsed=parsed&&bool(in>>assets)&&assets<=asset_limit;for(unsigned i=0;i<assets&&parsed;i++){std::string id;parsed=bool(in>>std::quoted(id));l.assets.push_back(std::move(id));}int index=region_index(next,region);if(index<0||next.regions[unsigned(index)].layers.size()>=layer_limit)parsed=false;else next.regions[unsigned(index)].layers.push_back(std::move(l));}
        std::string extra;if(!parsed||(in>>extra)){error="Invalid region record at line "+std::to_string(line);return false;}
    }
    if(!validate(next,error))return false;
    out=std::move(next);return true;
}
}
