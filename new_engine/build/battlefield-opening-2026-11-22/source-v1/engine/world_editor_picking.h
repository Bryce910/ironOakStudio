#ifndef VALWOOD_WORLD_EDITOR_PICKING_H
#define VALWOOD_WORLD_EDITOR_PICKING_H
#include "engine/world_assets.h"
#include <functional>
#include <memory>
#include <vector>
namespace world_editor_pick {
// XZ broad phase deliberately ignores height: sculpting/regrounding cannot
// invalidate this conservative filter. Precise 3D bounds/mesh tests follow.
struct Footprint {unsigned key=0;double min_x=0,min_z=0,max_x=0,max_z=0;};
bool footprint(unsigned,const world_studio::Pose &,Footprint &);
struct QueryStats {unsigned nodes=0,entries=0;};
class Index {
public:
    bool rebuild(const std::vector<Footprint> &);
    void query(const world_studio::Ray &,double maximum,std::vector<unsigned> &,QueryStats * =nullptr)const;
    std::size_t size()const{return entries_.size();}
private:
    struct Node {Footprint box;unsigned begin=0,count=0,left=0,right=0;};
    std::vector<Footprint> entries_;
    std::vector<Node> nodes_;
    unsigned build(unsigned,unsigned);
};
struct Epoch {std::uint64_t registry=0,inspection=0;};
bool operator==(Epoch,Epoch);
struct CacheStats {std::uint64_t hits=0,misses=0,evictions=0;std::size_t bytes=0,entries=0;};
// One picking worker owns this cache. Shared report handles may outlive an
// eviction; the byte budget bounds retained cache ownership, not active users.
class MeshCache {
public:
    using Loader=std::function<world_assets::Report(const std::string &)>;
    explicit MeshCache(std::size_t bytes=32*1024*1024,std::size_t entries=16):budget_(bytes),limit_(entries){}
    std::shared_ptr<const world_assets::Report> get(const std::string &,Epoch,const Loader &);
    CacheStats stats()const;
private:
    struct Entry {std::string path;std::shared_ptr<const world_assets::Report> report;std::size_t bytes=0;std::uint64_t used=0;};
    Epoch epoch_;bool initialized_=false;std::size_t budget_,limit_,bytes_=0;
    std::uint64_t clock_=0,hits_=0,misses_=0,evictions_=0;
    std::vector<Entry> entries_;
};
}
#endif
