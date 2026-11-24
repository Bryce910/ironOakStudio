// Exercises the production admission method/Coordinator with a deterministic
// image-cache adapter. No OpenGL context and no replacement admission policy.
#include "engine/texture_cache.h"
static TextureCacheStats fake_cache={};
TextureCacheStats texture_cache_stats(void){return fake_cache;}
#include "engine/world_scenery.cpp"
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "#x"\n";std::abort();}}while(0)
static std::vector<unsigned> release_order;
static WorldModelAsset *release_primary=nullptr,*release_shadow=nullptr;
static unsigned release_shadow_refs=0;
extern "C" void world_model_free(WorldModelAsset *a){
    if(!a)return;
    if(a==release_primary){release_order.push_back(2);fake_cache.gpu_bytes-=40;}
    if(a==release_shadow){release_order.push_back(3);if(!--release_shadow_refs)fake_cache.gpu_bytes-=20;}
}
extern "C" void world_model_instance_batch_free(WorldModelInstanceBatch *b){if(b){release_order.push_back(1);world_model_free(release_shadow);}}
void world_model_stage_free(WorldModelStaged *){}
namespace policy=world_scenery_interior_residency;
static WorldModelAsset *marker(size_t i){return reinterpret_cast<WorldModelAsset *>(uintptr_t(64+i));}
struct Fixture {
    WorldScenery scene;SceneryStreaming stream;std::vector<size_t> released;
    std::array<uint64_t,6> bytes{{40,5,30,5,20,10}};
    unsigned incoming=4;bool invoke_from_upload=false;
    Fixture():stream(&scene,[]{WorldSceneryStreamingConfig c;c.texture_bytes=100;return c;}()){
        fake_cache={};fake_cache.gpu_bytes=100;
        scene.groups.resize(6);stream.definitions.resize(6);
        for(unsigned i=0;i<6;++i){auto &g=scene.groups[i];g.alias=i<2?0:i<4?1:i==4?2:3;g.source_lod=(i==1||i==3||i==4)?1:0;g.alias_name="test"+std::to_string(i);if(i!=4)g.asset=marker(i);stream.latest_distance[i]=i<2?400:i<4?80:i==4?10:600;}
        world_regions::Document d;for(unsigned i=0;i<6;++i){world_regions::Region r;r.id=i+1;r.name="test"+std::to_string(i);r.bounds={{0,-1,0},{1,1,1}};r.priority=scene.groups[i].source_lod?1:0;world_regions::Layer l;l.id=i+1;r.layers.push_back(l);d.regions.push_back(r);}
        world_regions::Config c;c.upload_jobs_per_frame=1;std::string error;
        CHECK(stream.coordinator.configure(d,c,{load,upload,release,this},error));
        for(unsigned i=0;i<6;++i){stream.coordinator.demand(i+1,2,stream.latest_distance[i]);if(i!=4)CHECK(stream.coordinator.adopt(i+1,i+1,0,4));if(scene.groups[i].source_lod&&i!=4)CHECK(stream.coordinator.pin(i+1,true));}
        stream.coordinator.request({-11968,0,5582},1);
    }
    static std::shared_ptr<const world_regions::CpuBlob> load(const world_regions::LoadRequest&,void*){auto b=std::make_shared<world_regions::CpuBlob>();b->data={1};b->cpu_bytes=sizeof(*b)+b->data.capacity();b->gpu_bytes=4;b->upload_bytes=1;return b;}
    static world_regions::UploadResult upload(const world_regions::UploadRequest&r,void*p){auto &f=*static_cast<Fixture*>(p);CHECK(r.region==f.incoming+1);CHECK(f.stream.make_texture_space(f.incoming,[&]{return f.bytes[f.incoming];}));CHECK(policy::texture_fits(fake_cache.gpu_bytes,f.bytes[f.incoming],100));fake_cache.gpu_bytes+=f.bytes[f.incoming];f.scene.groups[f.incoming].asset=marker(f.incoming);return {true,true,r.region,1,4};}
    static void release(world_regions::Id id,uint64_t,void*p){auto&f=*static_cast<Fixture*>(p);auto i=size_t(id-1);if(f.scene.groups[i].asset){CHECK(fake_cache.gpu_bytes>=f.bytes[i]);fake_cache.gpu_bytes-=f.bytes[i];f.scene.groups[i].asset=nullptr;f.released.push_back(i);}}
    ~Fixture(){stream.coordinator.stop();}
};
struct RetryWave {
    static constexpr unsigned count=65;
    WorldScenery scene;SceneryStreaming stream;unsigned uploads=0,cpu_fail=0,shared_zero=0;bool initial=true;
    std::array<std::uint64_t,count> need={};
    RetryWave():stream(&scene,[]{WorldSceneryStreamingConfig c;c.texture_bytes=100;return c;}()){
        fake_cache={};fake_cache.gpu_bytes=100;need.fill(20);scene.groups.resize(count);stream.definitions.resize(count);
        world_regions::Document d;
        for(unsigned i=0;i<count;++i){auto &g=scene.groups[i];g.alias=i<2?0:i;g.source_lod=i==0?0:1;g.alias_name="wave"+std::to_string(i);stream.latest_distance[i]=i<2?400:double(i+3);stream.latest_demand[i]=2;stream.wanted_before[i]=true;stream.definitions[i].pinned=i!=0;
            world_regions::Region r;r.id=i+1;r.name=g.alias_name;r.priority=i?1:0;r.bounds={{0,-1,0},{1,1,1}};world_regions::Layer l;l.id=i+1;r.layers.push_back(l);d.regions.push_back(r);}
        world_regions::Config c;c.upload_jobs_per_frame=1;std::string e;CHECK(stream.coordinator.configure(d,c,{load,upload,release,this},e));
        scene.groups[1].asset=marker(1);CHECK(stream.coordinator.adopt(2,2,0,4));
        for(unsigned i=1;i<count;++i){stream.coordinator.demand(i+1,2,stream.latest_distance[i]);CHECK(stream.coordinator.pin(i+1,true));}
        stream.coordinator.demand(1,0,400);stream.coordinator.request({-12115,0,5150},1);
        for(unsigned f=2;f<4000&&stream.coordinator.stats().failed<count-2;++f){stream.coordinator.request({-12115,0,5150},f);stream.coordinator.tick();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        CHECK(stream.coordinator.stats().failed==count-2);CHECK(uploads==count-2);initial=false;
    }
    static std::shared_ptr<const world_regions::CpuBlob> load(const world_regions::LoadRequest&r,void*p){auto &f=*static_cast<RetryWave*>(p);if(r.region==f.cpu_fail)return {};auto b=std::make_shared<world_regions::CpuBlob>();b->data={1};b->cpu_bytes=sizeof(*b)+b->data.capacity();b->gpu_bytes=4;b->upload_bytes=1;return b;}
    static world_regions::UploadResult upload(const world_regions::UploadRequest&r,void*p){auto&f=*static_cast<RetryWave*>(p);++f.uploads;auto i=size_t(r.region-1);const auto missing=r.region==f.shared_zero?0:f.need[i];if(f.initial||!f.stream.make_texture_space(i,[&]{return missing;})){
            f.stream.failure_texture_bytes[i]=fake_cache.gpu_bytes;f.stream.failure_missing_texture_bytes[i]=missing;world_regions::UploadResult out;out.success=false;return out;}
        fake_cache.gpu_bytes+=missing;f.scene.groups[i].asset=marker(i);return {true,true,r.region,1,4};}
    static void release(world_regions::Id id,uint64_t token,void*p){auto&f=*static_cast<RetryWave*>(p);if(f.stream.coordinator.state(id).token==token)f.scene.groups[id-1].asset=nullptr;/* images shared by another owner: zero cache bytes released */}
    void gate(){stream.begin_texture_retry();for(unsigned i=2;i<count;++i)stream.consider_texture_retry(i,stream.coordinator.state(i+1),stream.coordinator.state(i+1).wanted?2:0);stream.commit_texture_retry();}
    void finish(unsigned id){for(unsigned f=4000;f<6000;++f){auto st=stream.coordinator.state(id);if(st.failed||(st.resident&&!st.dirty))return;stream.coordinator.request({-12115,0,5150},f);stream.coordinator.tick();std::this_thread::sleep_for(std::chrono::milliseconds(1));}CHECK(false);}
    ~RetryWave(){stream.coordinator.stop();}
};
int main(){
    constexpr uint64_t G=1024ull*1024*1024;
    for(unsigned requested:{0u,512u,1024u,2048u,4096u})for(uint64_t pool:std::array<uint64_t,6>{0,1536ull*1024*1024,2*G,4*G,8*G,12*G}){
        CHECK(policy::texture_edge(requested,0,false,pool)==requested);
        unsigned cap=pool>=12*G?requested:pool>=8*G?2048:pool>=4*G?1024:512;
        unsigned expected=!cap?requested:requested&&requested<cap?requested:cap;
        CHECK(policy::texture_edge(requested,1,false,pool)==expected);
        CHECK(policy::texture_edge(requested,0,true,pool)==expected);
    }
    CHECK(policy::texture_fits(90,10,100));CHECK(!policy::texture_fits(101,0,100));CHECK(!policy::texture_fits(UINT64_MAX,1,100));
    CHECK(policy::refinement_reclaimable(true,400,10));CHECK(!policy::refinement_reclaimable(false,400,10));CHECK(!policy::refinement_reclaimable(false,10,18));CHECK(policy::refinement_reclaimable(false,10,18.01));
    CHECK(!policy::parked_refinement_retry(400,10,80,40,100));CHECK(policy::parked_refinement_retry(10,400,90,40,100));CHECK(policy::parked_refinement_retry(400,10,60,40,100));
    {
        Fixture f;CHECK(f.stream.usable_fallback(0));CHECK(!f.stream.usable_fallback(4));CHECK(!f.stream.usable_fallback(5));
        // Missing village coverage is admitted from a full image pool by
        // retiring the farthest optional HIGH. Its actual MID remains alive.
        CHECK(f.stream.make_texture_space(4,[]{return uint64_t(20);}));
        CHECK(f.released==std::vector<size_t>{0});CHECK(!f.scene.groups[0].asset);CHECK(f.scene.groups[1].asset);CHECK(f.stream.coordinator.state(2).resident);CHECK(f.scene.groups[5].asset);
        CHECK(fake_cache.gpu_bytes==60);CHECK(f.stream.pressure_parked[0]);CHECK(f.stream.texture_cost[0]==40);
        // Approaching the former refinement allows it to displace a farther
        // refinement, never either last representation or either MID.
        fake_cache.gpu_bytes+=20;f.scene.groups[4].asset=marker(4);CHECK(f.stream.coordinator.adopt(5,5,0,4));
        f.stream.latest_distance[0]=10;CHECK(policy::parked_refinement_retry(10,400,80,40,100));f.stream.pressure_parked[0]=false;
        CHECK(f.stream.make_texture_space(0,[]{return uint64_t(40);}));CHECK(f.released==std::vector<size_t>({0,2}));CHECK(fake_cache.gpu_bytes==50);CHECK(f.scene.groups[1].asset&&f.scene.groups[3].asset&&f.scene.groups[4].asset&&f.scene.groups[5].asset);
    }
    {
        // Real Coordinator invokes admission while its ready upload is live.
        // Reentrant demand/unpin/retire must preserve the ready generation and
        // token and keep exact coverage resident after the callback commits.
        Fixture f;
        for(unsigned frame=2;frame<2000&&!f.stream.coordinator.state(5).resident;++frame){f.stream.coordinator.request({-11968,0,5582},frame);CHECK(f.stream.coordinator.tick());std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        CHECK(f.stream.coordinator.state(5).resident);CHECK(f.stream.coordinator.state(5).token==5);CHECK(f.stream.coordinator.stats().failures==0);CHECK(f.released==std::vector<size_t>{0});CHECK(fake_cache.gpu_bytes==80);CHECK(f.stream.coordinator.state(2).resident&&f.stream.coordinator.state(4).resident);
    }
    {
        Fixture f;
        // An unchanged full pool cannot retry a far failed HIGH by deleting a
        // closer refinement. After travel swaps spatial priority, the actual
        // victim query becomes eligible even without a cache-byte decrease.
        CHECK(f.stream.coordinator.demand(1,0,400));CHECK(f.stream.coordinator.pin(1,false));CHECK(f.stream.coordinator.retire(1));fake_cache.gpu_bytes+=40;
        f.stream.latest_distance[2]=10;CHECK(f.stream.texture_victim(0)==f.scene.groups.size());
        f.stream.latest_distance[0]=10;f.stream.latest_distance[2]=80;CHECK(f.stream.texture_victim(0)==2);
        // A missing MID outranks optional HIGH even when that refinement is
        // nearer; coverage cannot be permanently starved behind color detail.
        f.stream.latest_distance[4]=400;CHECK(f.stream.texture_victim(4)==2);
    }
    {
        Fixture f;fake_cache.untracked_gpu_images=1;CHECK(!f.stream.make_texture_space(4,[]{return uint64_t(1);}));CHECK(f.released.empty());fake_cache.untracked_gpu_images=0;
        // No HIGH has a usable MID: admission must fail truthfully instead of
        // deleting the last visible building/ground/prop representation.
        f.scene.groups[1].asset=f.scene.groups[3].asset=nullptr;
        CHECK(!f.stream.make_texture_space(4,[]{return uint64_t(20);}));CHECK(f.released.empty());
    }
    {
        // Invoke the actual production release callback. The model/batch API
        // adapter retains a shadow reference exactly as the renderer does.
        WorldScenery scene;scene.groups.resize(1);WorldSceneryStreamingConfig c;SceneryStreaming stream(&scene,c);stream.configured=true;
        auto &g=scene.groups[0];g.asset=release_primary=marker(100);g.shadow_asset=release_shadow=marker(101);g.shadow_batch=reinterpret_cast<WorldModelInstanceBatch*>(uintptr_t(200));g.shadow_batch_target=g.shadow_asset;g.shadow_uploaded.push_back({});g.visible_count=1;
        auto &h=stream.handles[0];h.used=h.complete=true;h.group=0;h.primary=g.asset;h.shadow=g.shadow_asset;fake_cache.gpu_bytes=100;release_shadow_refs=2;release_order.clear();
        SceneryStreaming::release(1,1,&stream);
        CHECK(release_order==std::vector<unsigned>({1,3,2,3}));CHECK(release_shadow_refs==0);CHECK(fake_cache.gpu_bytes==40);CHECK(!g.asset&&!g.shadow_asset&&!g.shadow_batch&&!g.shadow_batch_target);CHECK(g.shadow_uploaded.empty());CHECK(g.visible_count==0);CHECK(!h.used);
        release_primary=release_shadow=nullptr;
    }
    {
        RetryWave f;f.scene.groups[0].asset=marker(0);CHECK(f.stream.coordinator.adopt(1,1,0,4));
        f.gate();CHECK(f.stream.texture_retry_active==3);CHECK(f.stream.coordinator.stats().failed==62);
        // reload() has not queued CPU work yet. Failed=false is not completion.
        for(unsigned n=0;n<32;++n){f.gate();CHECK(f.stream.texture_retry_active==3);CHECK(f.stream.coordinator.stats().failed==62);}
        f.finish(3);CHECK(f.uploads==64);CHECK(!f.scene.groups[0].asset);CHECK(fake_cache.gpu_bytes==100);
        f.gate();CHECK(!f.stream.texture_retry_active);CHECK(!f.stream.texture_retry_candidate);CHECK(f.stream.coordinator.stats().failed==63);CHECK(f.uploads==64);
    }
    {
        // Native d751 reproduction: a closer corpse HIGH shares all its image
        // bytes with retained owners. Missing banner coverage retires it but
        // frees zero bytes. The old distance/cost rule immediately reloaded
        // this very victim, making the same failed banner eligible forever.
        RetryWave f;f.shared_zero=1;f.scene.groups[0].asset=marker(0);
        CHECK(f.stream.coordinator.adopt(1,1,0,4));
        for(unsigned i=2;i<RetryWave::count;++i)f.stream.latest_distance[i]=100.+i;
        f.stream.latest_distance[0]=f.stream.latest_distance[1]=1.7;
        f.stream.latest_distance[2]=12.3;
        f.gate();CHECK(f.stream.texture_retry_active==3);f.finish(3);
        CHECK(f.stream.pressure_parked[0]);CHECK(f.stream.texture_cost[0]==1);
        CHECK(!f.scene.groups[0].asset);CHECK(f.scene.groups[1].asset);
        CHECK(!f.scene.groups[2].asset);CHECK(fake_cache.gpu_bytes==100);
        for(unsigned cycle=0;cycle<64;++cycle){
            if(f.stream.parked_retry(0,1.7,12.3)){
                f.stream.pressure_parked[0]=false;
                CHECK(f.stream.coordinator.demand(1,2,1.7));f.finish(1);
            }
            f.gate();if(f.stream.texture_retry_active)f.finish(unsigned(f.stream.texture_retry_active));
            CHECK(f.stream.pressure_parked[0]);CHECK(f.uploads==64);
            CHECK(f.stream.coordinator.state(3).failed);CHECK(!f.stream.texture_retry_active);
            CHECK(f.stream.coordinator.state(2).resident);CHECK(fake_cache.gpu_bytes==100);
        }
        // A small amount of cache space cannot resurrect a cheap shared HIGH
        // at the expense of the still-uncovered banner reservation.
        fake_cache.gpu_bytes=99;CHECK(!f.stream.parked_retry(0,1.7,12.3));
        f.stream.camera_x=-7.9;CHECK(!f.stream.parked_retry(0,6.2,20.2));
        f.stream.camera_x=-8.;CHECK(f.stream.parked_retry(0,6.3,20.3));
        f.stream.latest_distance[0]=6.3;f.stream.latest_distance[2]=20.3;
        f.stream.pressure_parked[0]=false;CHECK(f.stream.coordinator.demand(1,2,6.3));f.finish(1);
        f.gate();CHECK(f.stream.texture_retry_active==3);f.finish(3);f.gate();
        CHECK(f.stream.pressure_parked[0]);CHECK(f.stream.pressure_camera_x[0]==-8.);
        CHECK(!f.stream.parked_retry(0,6.3,20.3));CHECK(!f.stream.texture_retry_active);
        // Enough real room for both models is a valid stationary recovery.
        fake_cache.gpu_bytes=79;CHECK(f.stream.parked_retry(0,6.3,20.3));
        fake_cache.untracked_gpu_images=1;CHECK(!f.stream.parked_retry(0,1.7,12.3));fake_cache.untracked_gpu_images=0;
        // A queued retry has cleared its old failure reservation. Until it
        // commits coverage, absent metadata must not count as zero cost.
        f.stream.failure_missing_texture_bytes[2]=0;CHECK(!f.stream.parked_retry(0,1.7,12.3));
        f.stream.failure_missing_texture_bytes[2]=20;
        // The permanently non-fitting nearest request yields to a smaller
        // coverage bank which genuinely fits the available one-byte slot.
        fake_cache.gpu_bytes=99;f.need[3]=1;f.stream.failure_missing_texture_bytes[3]=1;
        f.gate();CHECK(f.stream.texture_retry_active==4);f.finish(4);
        CHECK(f.stream.coordinator.state(4).resident);CHECK(f.stream.coordinator.state(3).failed);
        CHECK(f.stream.pressure_parked[0]);CHECK(fake_cache.gpu_bytes==100);
        // Once the actual blocker has a resident representation, its parked
        // former victim can follow normal spatial refinement policy again.
        f.need[2]=0;CHECK(f.stream.coordinator.reload(3));f.finish(3);
        CHECK(f.stream.parked_retry(0,1.7,12.3));
        // An old resident token undergoing a dirty replacement still needs
        // admission; residency alone cannot release the parked refinement.
        CHECK(f.stream.coordinator.reload(3));CHECK(f.stream.coordinator.state(3).dirty);
        CHECK(!f.stream.parked_retry(0,1.7,12.3));
    }
    {
        // The same zero-byte loop is possible for optional HIGH admission even
        // with valid same-alias MID coverage: cost1 fits the remaining1 byte,
        // but the pending incoming HIGH needs20. Retain coverage and park once.
        RetryWave f;f.shared_zero=1;f.scene.groups[0].asset=marker(0);
        CHECK(f.stream.coordinator.adopt(1,1,0,4));
        f.scene.groups[2].alias=f.scene.groups[3].alias;
        f.scene.groups[2].source_lod=0;f.stream.definitions[2].pinned=false;
        f.need[3]=0;CHECK(f.stream.coordinator.reload(4));f.finish(4);
        for(unsigned i=2;i<RetryWave::count;++i)f.stream.latest_distance[i]=100.+i;
        for(unsigned i=4;i<RetryWave::count;++i){CHECK(f.stream.coordinator.demand(i+1,0,100.+i));CHECK(f.stream.coordinator.pin(i+1,false));}
        f.stream.latest_distance[0]=400;f.stream.latest_distance[2]=10;
        f.gate();CHECK(f.stream.texture_retry_active==3);f.finish(3);
        fake_cache.gpu_bytes=99;CHECK(f.stream.usable_fallback(2));CHECK(f.stream.texture_cost[0]==1);
        for(unsigned cycle=0;cycle<64;++cycle){
            if(f.stream.parked_retry(0,400,10)){
                f.stream.pressure_parked[0]=false;
                CHECK(f.stream.coordinator.demand(1,2,400));f.finish(1);
            }
            f.gate();if(f.stream.texture_retry_active)f.finish(unsigned(f.stream.texture_retry_active));
            CHECK(f.stream.pressure_parked[0]);CHECK(f.uploads==65);
            CHECK(f.stream.coordinator.state(3).failed);CHECK(!f.stream.texture_retry_active);
            CHECK(f.stream.usable_fallback(2));CHECK(fake_cache.gpu_bytes==99);
        }
    }
    {
        RetryWave f;fake_cache.gpu_bytes=60;f.cpu_fail=3;f.gate();CHECK(f.stream.texture_retry_active==3);f.finish(3);CHECK(f.stream.coordinator.state(3).failed);
        // The nearest retry now has a CPU failure and no recorded texture
        // pressure. It must yield to the next fitting coverage candidate.
        f.gate();CHECK(f.stream.texture_retry_active==4);f.finish(4);CHECK(f.stream.coordinator.state(4).resident);CHECK(fake_cache.gpu_bytes==80);CHECK(f.stream.coordinator.state(3).failed);
    }
    {
        RetryWave f;fake_cache.gpu_bytes=60;f.gate();CHECK(f.stream.texture_retry_active==3);
        CHECK(f.stream.coordinator.demand(3,0,5));CHECK(f.stream.coordinator.pin(3,false));f.gate();CHECK(f.stream.texture_retry_active==4);
    }
    {
        RetryWave f;fake_cache.gpu_bytes=60;f.scene.groups[2].asset=marker(2);CHECK(f.stream.coordinator.adopt(3,200,0,4));f.gate();auto st=f.stream.coordinator.state(3);CHECK(st.resident&&st.dirty&&!st.failed);f.gate();CHECK(f.stream.texture_retry_active==3);
    }
    {
        RetryWave f;fake_cache.untracked_gpu_images=1;f.gate();CHECK(!f.stream.texture_retry_active);fake_cache.untracked_gpu_images=0;
        // A zero-new-image attempt previously rejected solely because the
        // global cache was over cap can recover when that pressure disappears.
        f.stream.failure_texture_bytes[2]=120;f.stream.failure_missing_texture_bytes[2]=0;fake_cache.gpu_bytes=100;f.gate();CHECK(f.stream.texture_retry_active==3);
    }
    {
        WorldScenery scene;scene.groups.resize(1);WorldSceneryStreamingConfig c;SceneryStreaming stream(&scene,c);stream.configured=true;auto &g=scene.groups[0];g.asset=release_primary=marker(100);auto &h=stream.handles[0];h.used=true;h.group=0;h.primary=g.asset;fake_cache.gpu_bytes=100;stream.failure_texture_bytes[0]=100;stream.failure_missing_texture_bytes[0]=20;stream.texture_failure_rollback[0]=true;
        SceneryStreaming::release(1,1,&stream);CHECK(fake_cache.gpu_bytes==60);CHECK(stream.failure_texture_bytes[0]==60);CHECK(stream.failure_missing_texture_bytes[0]==60);CHECK(!policy::texture_retry(60,60,60,100));CHECK(!stream.texture_failure_rollback[0]);
        g.asset=release_primary=marker(101);h.used=true;h.primary=g.asset;h.group=0;fake_cache.gpu_bytes=100;stream.failure_missing_texture_bytes[0]=UINT64_MAX-5;stream.texture_failure_rollback[0]=true;SceneryStreaming::release(1,1,&stream);CHECK(stream.failure_missing_texture_bytes[0]==UINT64_MAX);release_primary=nullptr;
    }
    std::cout<<"PASS "<<checks<<" production scenery residency checks; bounded texture admission, coverage preservation, spatial refinement retry, pinned distance, reentrant Coordinator token commit\n";
}
