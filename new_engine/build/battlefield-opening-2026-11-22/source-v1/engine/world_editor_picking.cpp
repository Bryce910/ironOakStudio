#include "engine/world_transform_math.h"
#include "engine/world_editor_picking.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace world_editor_pick {
namespace {
bool finite(double value){return std::isfinite(value);}
bool valid(const Footprint &b){return finite(b.min_x)&&finite(b.max_x)&&finite(b.min_z)&&finite(b.max_z)&&b.min_x<=b.max_x&&b.min_z<=b.max_z;}
bool overlaps(const Footprint &b,const world_studio::Ray &r,double maximum){
    double lo=0,hi=maximum;
    const double origin[]={r.origin.x,r.origin.z},direction[]={r.direction.x,r.direction.z};
    const double minimum[]={b.min_x-1e-6,b.min_z-1e-6},maxima[]={b.max_x+1e-6,b.max_z+1e-6};
    for(unsigned axis=0;axis<2;axis++){
        if(direction[axis]==0){if(origin[axis]<minimum[axis]||origin[axis]>maxima[axis])return false;}
        else{double a=(minimum[axis]-origin[axis])/direction[axis],c=(maxima[axis]-origin[axis])/direction[axis];if(a>c)std::swap(a,c);lo=std::max(lo,a);hi=std::min(hi,c);if(lo>hi)return false;}
    }return true;
}
std::size_t cost(const world_assets::Report &r){
    std::size_t bytes=sizeof(r)+r.geometry.capacity()*sizeof(world_assets::Triangle)+r.images.capacity()*sizeof(world_assets::Image)+r.materials.capacity()*sizeof(world_assets::Material)+r.warnings.capacity()*sizeof(std::string)+r.error.capacity();
    for(const auto &m:r.materials)bytes+=m.name.capacity();
    for(const auto &s:r.warnings)bytes+=s.capacity();
    return bytes;
}
}
bool footprint(unsigned key,const world_studio::Pose &p,Footprint &out){
    if(!finite(p.root.x)||!finite(p.root.z)||!finite(p.minimum.x)||!finite(p.minimum.z)||!finite(p.maximum.x)||!finite(p.maximum.z)||!finite(p.yaw)||!finite(p.scale)||p.scale<=0||p.minimum.x>p.maximum.x||p.minimum.z>p.maximum.z)return false;
    const auto m=vw_transform::matrix(vw_transform::pose(p));Footprint next{key,1e30,1e30,-1e30,-1e30};
    for(unsigned k=0;k<8;k++){const auto q=vw_transform::point(m,{k&1?p.maximum.x:p.minimum.x,k&2?p.maximum.y:p.minimum.y,k&4?p.maximum.z:p.minimum.z});next.min_x=std::min(next.min_x,q[0]);next.max_x=std::max(next.max_x,q[0]);next.min_z=std::min(next.min_z,q[2]);next.max_z=std::max(next.max_z,q[2]);}
    if(!valid(next))return false;
    out=next;return true;
}
bool Index::rebuild(const std::vector<Footprint> &input){
    if(input.size()>200000||std::any_of(input.begin(),input.end(),[](const auto &p){return !valid(p);}))return false;
    entries_=input;nodes_.clear();nodes_.reserve(entries_.size()*2);
    if(!entries_.empty())build(0,unsigned(entries_.size()));
    return true;
}
unsigned Index::build(unsigned begin,unsigned end){
    const unsigned node=unsigned(nodes_.size());nodes_.push_back({});Footprint box=entries_[begin];
    for(unsigned i=begin+1;i<end;i++){box.min_x=std::min(box.min_x,entries_[i].min_x);box.min_z=std::min(box.min_z,entries_[i].min_z);box.max_x=std::max(box.max_x,entries_[i].max_x);box.max_z=std::max(box.max_z,entries_[i].max_z);}
    nodes_[node].box=box;
    if(end-begin<=8){nodes_[node].begin=begin;nodes_[node].count=end-begin;return node;}
    const bool x=box.max_x-box.min_x>=box.max_z-box.min_z;const unsigned middle=begin+(end-begin)/2;
    std::nth_element(entries_.begin()+begin,entries_.begin()+middle,entries_.begin()+end,[&](const auto &a,const auto &b){const double av=x?a.min_x+a.max_x:a.min_z+a.max_z,bv=x?b.min_x+b.max_x:b.min_z+b.max_z;return av==bv?a.key<b.key:av<bv;});
    const unsigned left=build(begin,middle),right=build(middle,end);nodes_[node].left=left;nodes_[node].right=right;return node;
}
void Index::query(const world_studio::Ray &ray,double maximum,std::vector<unsigned> &out,QueryStats *stats)const{
    out.clear();if(stats)*stats={};
    if(nodes_.empty()||!finite(maximum)||maximum<0||!finite(ray.origin.x)||!finite(ray.origin.y)||!finite(ray.origin.z)||!finite(ray.direction.x)||!finite(ray.direction.y)||!finite(ray.direction.z)||(ray.direction.x==0&&ray.direction.y==0&&ray.direction.z==0))return;
    std::array<unsigned,64> pending{};unsigned count=1;pending[0]=0;
    while(count){const auto &n=nodes_[pending[--count]];if(stats)stats->nodes++;if(!overlaps(n.box,ray,maximum))continue;
        if(n.count){for(unsigned i=n.begin;i<n.begin+n.count;i++){if(stats)stats->entries++;if(overlaps(entries_[i],ray,maximum))out.push_back(entries_[i].key);}}
        else{pending[count++]=n.left;pending[count++]=n.right;}
    }
}
bool operator==(Epoch a,Epoch b){return a.registry==b.registry&&a.inspection==b.inspection;}
std::shared_ptr<const world_assets::Report> MeshCache::get(const std::string &path,Epoch epoch,const Loader &loader){
    if(!initialized_||!(epoch_==epoch)){entries_.clear();bytes_=0;epoch_=epoch;initialized_=true;}
    for(auto &entry:entries_)if(entry.path==path){entry.used=++clock_;hits_++;return entry.report;}
    misses_++;auto report=std::make_shared<const world_assets::Report>(loader(path));const auto weight=cost(*report)+path.capacity()+sizeof(Entry);
    if(weight>budget_||!limit_)return report;
    while(!entries_.empty()&&(entries_.size()>=limit_||bytes_>budget_-weight)){
        auto at=std::min_element(entries_.begin(),entries_.end(),[](const auto &a,const auto &b){return a.used<b.used;});bytes_-=at->bytes;entries_.erase(at);evictions_++;
    }entries_.push_back({path,report,weight,++clock_});bytes_+=weight;return report;
}
CacheStats MeshCache::stats()const{return {hits_,misses_,evictions_,bytes_,entries_.size()};}
}
