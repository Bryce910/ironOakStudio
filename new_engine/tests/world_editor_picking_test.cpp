#include "engine/world_editor_picking.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
using namespace world_editor_pick;
using namespace world_studio;
static bool box_hit(const Footprint &b,const Ray &r,double maximum){
    double lo=0,hi=maximum;const double o[]={r.origin.x,r.origin.z},d[]={r.direction.x,r.direction.z};
    const double mn[]={b.min_x-1e-6,b.min_z-1e-6},mx[]={b.max_x+1e-6,b.max_z+1e-6};
    for(unsigned i=0;i<2;i++){if(d[i]==0){if(o[i]<mn[i]||o[i]>mx[i])return false;}else{double a=(mn[i]-o[i])/d[i],c=(mx[i]-o[i])/d[i];if(a>c)std::swap(a,c);lo=std::max(lo,a);hi=std::min(hi,c);if(lo>hi)return false;}}return true;
}
int main(){
    std::mt19937 random(481);std::uniform_real_distribution<double> coordinate(-1000,1000),angle(-3.14,3.14),size(.1,20),scale(.15,4);
    std::vector<Pose> poses;std::vector<Footprint> entries;
    for(unsigned i=0;i<8000;i++){Pose p;p.root={coordinate(random),coordinate(random),coordinate(random)};p.minimum={-size(random),0,-size(random)};p.maximum={size(random),size(random),size(random)};p.yaw=angle(random);p.scale=scale(random);Footprint b;assert(footprint(i,p,b));poses.push_back(p);entries.push_back(b);}
    Index index;assert(index.rebuild(entries)&&index.size()==8000);std::vector<unsigned> actual;
    std::vector<Ray> rays;for(unsigned i=0;i<350;i++){const double a=angle(random);rays.push_back({{coordinate(random),coordinate(random),coordinate(random)},{std::cos(a),-.1,std::sin(a)}});}
    rays.push_back({poses[0].root,{0,-1,0}});rays.push_back({poses[1].root,{0,1,0}});rays.push_back({poses[2].root,{1e-14,-1,0}});
    std::uint64_t broad_tests=0,linear_tests=0;
    for(const auto &ray:rays){QueryStats stats;index.query(ray,700,actual,&stats);broad_tests+=stats.entries;std::sort(actual.begin(),actual.end());std::vector<unsigned> expected;
        for(const auto &b:entries){linear_tests++;if(box_hit(b,ray,700))expected.push_back(b.key);}assert(actual==expected);
        // The XZ index must conservatively include every precise oriented hit,
        // even after arbitrary vertical regrounding, including a vertical ray.
        for(unsigned i=0;i<poses.size();i++){double hit=0;auto p=poses[i];p.root.y+=std::sin(double(i))*100;if(ray_bounds(ray,p,hit)&&hit<=700)assert(std::binary_search(actual.begin(),actual.end(),i));}
    }
    const auto count=index.size();auto invalid=entries;invalid[0].min_x=std::numeric_limits<double>::quiet_NaN();assert(!index.rebuild(invalid)&&index.size()==count);
    index.query({{},{}},700,actual);assert(actual.empty());index.query(rays[0],-1,actual);assert(actual.empty());
    index.query(rays[0],std::numeric_limits<double>::infinity(),actual);assert(actual.empty());
    Pose malformed=poses[0];malformed.scale=-1;Footprint untouched{99,1,2,3,4};assert(!footprint(0,malformed,untouched)&&untouched.key==99);
    assert(index.rebuild({}));index.query(rays[0],700,actual);assert(actual.empty());
    assert(broad_tests<linear_tests/10);
    unsigned loads=0;auto loader=[&](const std::string &name){loads++;world_assets::Report report;report.supported=true;report.error=name;report.geometry.resize(100);return report;};
    MeshCache cache(20000,2);auto a=cache.get("a",{1,1},loader);assert(loads==1);assert(cache.get("a",{1,1},loader)==a&&loads==1);
    auto b=cache.get("b",{1,1},loader);assert(loads==2);assert(cache.get("a",{1,1},loader)==a);
    cache.get("c",{1,1},loader);assert(cache.stats().entries==2&&cache.stats().evictions==1&&cache.stats().bytes<=20000);
    assert(a->error=="a"&&b->error=="b");cache.get("b",{1,1},loader);assert(loads==4);
    auto new_a=cache.get("a",{2,1},loader);assert(new_a!=a&&cache.stats().entries==1);cache.get("a",{2,2},loader);assert(loads==6);
    MeshCache tiny(1,16);tiny.get("oversize",{1,1},loader);tiny.get("oversize",{1,1},loader);assert(tiny.stats().entries==0&&tiny.stats().bytes==0);
    MeshCache disabled(100000,0);disabled.get("disabled",{1,1},loader);assert(disabled.stats().entries==0);
    // Reserved metadata also consumes the retention budget even when no
    // warning has been appended. Oversized reports remain usable by callers.
    MeshCache metadata_budget(20000,2);auto reserved=metadata_budget.get("metadata",{1,1},[](const std::string &){world_assets::Report r;r.warnings.reserve(2000);return r;});
    assert(reserved&&metadata_budget.stats().entries==0&&metadata_budget.stats().bytes==0);
    // Paired same-data benchmark: all broad-phase results remain identical.
    volatile std::uint64_t checksum=0;auto start=std::chrono::steady_clock::now();
    for(unsigned repeat=0;repeat<5;repeat++)for(const auto &ray:rays)for(const auto &entry:entries)if(box_hit(entry,ray,700))checksum+=entry.key;
    const auto before=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();start=std::chrono::steady_clock::now();
    assert(index.rebuild(entries));for(unsigned repeat=0;repeat<5;repeat++)for(const auto &ray:rays){index.query(ray,700,actual);for(unsigned key:actual)checksum+=key;}
    const auto after=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"{\"test\":\"editor_picking\",\"passed\":true,\"sources\":8000,\"rays\":"<<rays.size()<<",\"linear_entry_tests\":"<<linear_tests<<",\"indexed_entry_tests\":"<<broad_tests<<",\"before_ms\":"<<before<<",\"after_ms_including_build\":"<<after<<",\"checksum\":"<<checksum<<"}\n";
}
