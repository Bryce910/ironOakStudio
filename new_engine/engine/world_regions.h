#ifndef VALWOOD_WORLD_REGIONS_H
#define VALWOOD_WORLD_REGIONS_H
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Stable-ID authoring data and a CPU/owner-thread residency boundary. This module
// knows no GL, renderer, file asset decoder or audio API. Only the supplied owner
// callbacks may create/delete GPU resources; CPU loaders run on one worker.
namespace world_regions {
using Id=std::uint64_t;
constexpr unsigned region_limit=1024,layer_limit=32,asset_limit=1024;
constexpr unsigned entity_limit=16384,dependency_limit=64;
constexpr std::size_t document_byte_limit=4*1024*1024;
struct Vec3 {double x=0,y=0,z=0;};
struct Bounds {Vec3 minimum,maximum={128,128,128};};
struct Asset {std::string id;std::vector<std::string> dependencies;};
struct Layer {
    Id id=0;std::string name="Layer";int priority=0;bool visible=true,locked=false;
    std::vector<Id> entities;std::vector<std::string> assets;
};
struct Region {
    Id id=0;std::string name="Region";Bounds bounds;int priority=0;
    bool visible=true,locked=false;double load_distance=0,preload_distance=96,unload_distance=160;
    std::vector<Id> dependencies;std::vector<Layer> layers;
};
struct Document {std::vector<Asset> assets;std::vector<Region> regions;};
bool validate(const Document &,std::string &error);
bool save(const char *path,const Document &,std::string &error);
bool load(const char *path,Document &,std::string &error);
bool editable(const Document &,Id region,Id layer);
double distance(const Bounds &,Vec3);

struct CpuBlob {
    std::vector<std::uint8_t> data;
    std::shared_ptr<const void> payload; // optional decoded CPU structures only
    // cpu_bytes includes data capacity/descriptor/payload; zero derives descriptor
    // + data capacity. Loader must honor LoadRequest.cpu_limit before allocating.
    std::uint64_t cpu_bytes=0,gpu_bytes=0,upload_bytes=0;
    bool upload_bytes_upper_bound=false; // shared resident images can skip bytes
};
struct LoadRequest {
    Id region=0;std::uint64_t generation=0,cpu_limit=0;
    const Region *definition=nullptr;
    const std::vector<std::string> *asset_ids=nullptr; // visible layer dependency closure
    const std::atomic<std::uint64_t> *current_generation=nullptr;
    const std::atomic<bool> *stopping=nullptr;
    char *error=nullptr;std::size_t error_capacity=0;
    bool cancelled()const;
};
struct UploadRequest {
    Id region=0;std::uint64_t generation=0,token=0,offset=0,byte_budget=0;
    const CpuBlob *blob=nullptr;
};
struct UploadResult {
    bool success=true,complete=false;
    std::uint64_t token=0,consumed_bytes=0,gpu_bytes=0; // GPU bytes cumulative
};
using Loader=std::shared_ptr<const CpuBlob> (*)(const LoadRequest &,void *);
using Uploader=UploadResult (*)(const UploadRequest &,void *);
using Releaser=void (*)(Id region,std::uint64_t token,void *);
struct Callbacks {Loader load=nullptr;Uploader upload=nullptr;Releaser release=nullptr;void *context=nullptr;};
struct Config {
    std::uint64_t cpu_bytes=256ull*1024*1024,gpu_bytes=1024ull*1024*1024;
    std::uint64_t upload_bytes_per_frame=4ull*1024*1024;
    unsigned upload_jobs_per_frame=2;bool keep_cpu=false;
};
struct FrameBudget {std::uint64_t bytes=0;unsigned jobs=0;}; // zero uses Config
struct State {
    bool wanted=false,pinned=false,resident=false,loading=false,failed=false,dirty=false;
    std::uint64_t generation=0,resident_generation=0,token=0,cpu_bytes=0,gpu_bytes=0,last_used=0;
    double camera_distance=0;
};
struct Stats {
    unsigned resident=0,wanted=0,loading=0,ready=0,failed=0;
    std::uint64_t resident_cpu_bytes=0,pending_cpu_bytes=0,worker_cpu_reservation=0;
    std::uint64_t resident_gpu_bytes=0,pending_gpu_reservation=0,pending_gpu_bytes=0;
    std::uint64_t frame_upload_bytes=0,frame_upload_jobs=0;
    std::uint64_t loads=0,uploads=0,cancellations=0,failures=0,evictions=0,budget_waits=0;
    std::array<char,192> last_error={};
};
class Coordinator {
public:
    Coordinator();~Coordinator();
    Coordinator(const Coordinator &)=delete;Coordinator &operator=(const Coordinator &)=delete;
    // Owner thread, loading boundary: validate/copy document then join/reset any
    // previous worker/resources. Invalid drafts leave the current world intact.
    bool configure(const Document &,const Config &,const Callbacks &,std::string &error);
    bool request(Vec3 camera,std::uint64_t frame);
    // Nonblocking worker polling via try_lock; no joins/IO/heap allocations.
    // One bounded worker/result/upload pipeline. Uploader gets a strict remaining
    // frame byte allowance, may upload chunks, and must report actual bytes.
    bool tick(FrameBudget budget={});
    bool pin(Id,bool);
    // Explicit camera demand for a renderer asset shared by many disconnected
    // placements. -1 restores region bounds; 0/1/2 means none/preload/immediate.
    bool demand(Id,int,double nearest_distance);
    // Loading-boundary ownership transfer. Adoption may temporarily exceed the
    // configured geometry budget; retire unwanted entries before normal ticks.
    bool adopt(Id,std::uint64_t token,std::uint64_t cpu_bytes,std::uint64_t gpu_bytes);
    bool retire(Id); // only unpinned, unwanted, idle residents
    bool reload(Id); // retains the previous resident until replacement succeeds
    State state(Id)const;Stats stats()const;
    // Owner thread, loading/shutdown boundary. Cooperative cancellation + join;
    // arbitrary loader IO cannot be forcibly interrupted. Releases owned tokens.
    bool stop();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
#endif
