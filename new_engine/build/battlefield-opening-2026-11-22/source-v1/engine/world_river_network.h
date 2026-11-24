#ifndef VALWOOD_WORLD_RIVER_NETWORK_H
#define VALWOOD_WORLD_RIVER_NETWORK_H
#include "engine/world_surface_authoring.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

// One world-space channel union drives water queries, terrain beds and render
// vertices. A crossing is one surface, never two coplanar transparent ribbons.
// The 16m segment index keeps interactive rebuilds proportional to wet area.
namespace world_surface {
struct RiverSample {
    double distance=1.e30,height=0,bed=0,bank=0,grade=0;
    std::array<double,2> flow={1,0};bool inside=false;
};
struct RiverNetwork {
    struct Segment {Point a,b;std::array<float,2> ta,tb;double length=0,half=0,depth=0;unsigned ribbon=0;};
    std::vector<Segment> segments;
    std::map<std::pair<int,int>,std::vector<unsigned>> lookup;
    static constexpr double bucket_size=16,mesh_step=2;
    bool prepare(const std::vector<BakedRibbon> &ribbons,std::string &error) {
        RiverNetwork next;
        for(unsigned ri=0;ri<ribbons.size();++ri) {
            const auto &r=ribbons[ri];if(r.source.kind!=RibbonKind::River)continue;
            for(unsigned i=1;i<r.samples.size();++i) {
                const auto &a=r.samples[i-1],&b=r.samples[i];
                const float downhill=b.position.y>a.position.y?-1.f:1.f;
                Segment s{a.position,b.position,{-a.right[1]*downhill,a.right[0]*downhill},{-b.right[1]*downhill,b.right[0]*downhill},
                    std::hypot(b.position.x-a.position.x,b.position.z-a.position.z),r.source.width*.5,r.source.depth,ri};
                if(s.length<1.e-8)continue;
                const auto id=unsigned(next.segments.size());next.segments.push_back(s);
                const double margin=s.half+mesh_step*2;
                for(int z=int(std::floor((std::min(s.a.z,s.b.z)-margin)/bucket_size));z<=int(std::floor((std::max(s.a.z,s.b.z)+margin)/bucket_size));++z)
                    for(int x=int(std::floor((std::min(s.a.x,s.b.x)-margin)/bucket_size));x<=int(std::floor((std::max(s.a.x,s.b.x)+margin)/bucket_size));++x)
                        next.lookup[{x,z}].push_back(id);
            }
        }
        *this=std::move(next);error.clear();return true;
    }
    RiverSample sample(double x,double z) const {
        RiverSample out;if(!std::isfinite(x)||!std::isfinite(z))return out;
        auto bucket=lookup.find({int(std::floor(x/bucket_size)),int(std::floor(z/bucket_size))});if(bucket==lookup.end())return out;
        struct Hit {double distance=1.e30,t=0;const Segment *segment=nullptr;};
        std::array<Hit,ribbon_limit> hits={};
        for(unsigned id:bucket->second) {
            const auto &s=segments[id];const double dx=s.b.x-s.a.x,dz=s.b.z-s.a.z;
            const double t=std::clamp(((x-s.a.x)*dx+(z-s.a.z)*dz)/(s.length*s.length),0.,1.);
            const double distance=std::hypot(x-s.a.x-dx*t,z-s.a.z-dz*t);
            auto &hit=hits[s.ribbon];if(distance<hit.distance)hit={distance,t,&s};
        }
        double weight_sum=0,height=0,flow_x=0,flow_z=0,grade=0,bed=1.e30;
        const Hit *closest=nullptr;double closest_signed=1.e30;
        for(const auto &h:hits)if(h.segment) {
            const auto &s=*h.segment;const double signed_distance=h.distance-s.half;
            if(signed_distance<closest_signed){closest_signed=signed_distance;closest=&h;}
            out.distance=std::min(out.distance,signed_distance);
            if(signed_distance>=0)continue;
            const double q=h.distance/s.half,weight=(1-q)*(1-q);
            const double y=s.a.y+(s.b.y-s.a.y)*h.t;
            height+=y*weight;weight_sum+=weight;
            flow_x+=(s.ta[0]+(s.tb[0]-s.ta[0])*h.t)*weight;
            flow_z+=(s.ta[1]+(s.tb[1]-s.ta[1])*h.t)*weight;
            grade+=std::abs((s.a.y-s.b.y)/s.length)*weight;
            bed=std::min(bed,y-s.depth+(s.depth+.15)*q*q*q*q);
        }
        if(!closest)return out;
        const auto &s=*closest->segment;const double t=closest->t;
        if(weight_sum>1.e-14) {
            out.inside=true;out.height=height/weight_sum;out.grade=grade/weight_sum;out.bed=bed;
            out.bank=std::max(0.,-out.distance);
        } else {
            out.height=s.a.y+(s.b.y-s.a.y)*t;out.bed=out.height;
            flow_x=s.ta[0]+(s.tb[0]-s.ta[0])*t;flow_z=s.ta[1]+(s.tb[1]-s.ta[1])*t;
            out.grade=std::abs((s.a.y-s.b.y)/s.length);
        }
        double n=std::hypot(flow_x,flow_z);
        // Opposing authored streams meet in calm water instead of producing NaN.
        const double downhill=s.b.y>s.a.y?-1.:1.;
        if(n>1.e-10)out.flow={flow_x/n,flow_z/n};else out.flow={(s.b.x-s.a.x)*downhill/s.length,(s.b.z-s.a.z)*downhill/s.length};
        return out;
    }
    static RiverSample interpolate(const RiverSample &a,const RiverSample &b,double t) {
        RiverSample s;s.distance=a.distance+(b.distance-a.distance)*t;s.height=a.height+(b.height-a.height)*t;
        s.bed=a.bed+(b.bed-a.bed)*t;s.bank=a.bank+(b.bank-a.bank)*t;s.grade=a.grade+(b.grade-a.grade)*t;
        s.flow={a.flow[0]+(b.flow[0]-a.flow[0])*t,a.flow[1]+(b.flow[1]-a.flow[1])*t};s.inside=s.distance<0;return s;
    }
    // Match the render grid's diagonal and barycentric interpolation exactly.
    // In particular a sloping tributary cannot swim above its CPU water query.
    RiverSample surface(double x,double z) const {
        const double gx=std::floor(x/mesh_step)*mesh_step,gz=std::floor(z/mesh_step)*mesh_step;
        const double u=(x-gx)/mesh_step,v=(z-gz)/mesh_step;
        auto a=sample(gx,gz),b=sample(gx+mesh_step,gz),c=sample(gx,gz+mesh_step),d=sample(gx+mesh_step,gz+mesh_step);
        if(u+v<=1){const double sum=u+v;return sum>1.e-14?interpolate(a,interpolate(b,c,v/sum),sum):a;}
        const double sum=2-u-v;return sum>1.e-14?interpolate(d,interpolate(b,c,(1-u)/sum),sum):d;
    }
};
struct RiverVertex {float x=0,y=0,z=0,flow_x=1,flow_z=0,bank=0,grade=0;};
struct RiverMeshRange {int first=0,count=0;float level=0;std::array<double,4> bounds={};};
struct RiverMesh {std::vector<RiverVertex> vertices;std::vector<RiverMeshRange> ranges;};
inline bool bake_river_mesh(const RiverNetwork &network,RiverMesh &out,std::string &error) {
    constexpr unsigned vertex_limit=1500000;
    // Every cell belongs to exactly one chunk. End caps, tight bends, crossings
    // and tributaries all clip the same union; neither triangles nor caps overlap.
    std::map<std::pair<int,int>,std::set<std::pair<int,int>>> chunks;
    for(const auto &s:network.segments) {
        const double m=s.half+network.mesh_step;
        for(int z=int(std::floor((std::min(s.a.z,s.b.z)-m)/network.mesh_step));z<=int(std::floor((std::max(s.a.z,s.b.z)+m)/network.mesh_step));++z)
            for(int x=int(std::floor((std::min(s.a.x,s.b.x)-m)/network.mesh_step));x<=int(std::floor((std::max(s.a.x,s.b.x)+m)/network.mesh_step));++x)
                chunks[{int(std::floor(x/32.)),int(std::floor(z/32.))}].insert({x,z});
    }
    RiverMesh next;
    struct Corner {double x,z;RiverSample sample;};
    auto corner=[&](double x,double z){return Corner{x,z,network.sample(x,z)};};
    auto cut=[&](const Corner &a,const Corner &b){
        const double t=std::clamp(a.sample.distance/(a.sample.distance-b.sample.distance),0.,1.);
        return Corner{a.x+(b.x-a.x)*t,a.z+(b.z-a.z)*t,RiverNetwork::interpolate(a.sample,b.sample,t)};
    };
    auto vertex=[&](const Corner &p){const auto &v=p.sample;next.vertices.push_back({float(p.x),float(v.height),float(p.z),float(v.flow[0]),float(v.flow[1]),float(v.bank),float(v.grade)});};
    auto triangle=[&](Corner a,Corner b,Corner c){
        std::array<Corner,5> input={a,b,c},poly={};unsigned count=0;
        for(unsigned i=0;i<3;++i){const auto &p=input[i],&q=input[(i+1)%3];bool pin=p.sample.distance<0,qin=q.sample.distance<0;if(pin)poly[count++]=p;if(pin!=qin)poly[count++]=cut(p,q);}
        for(unsigned i=2;i<count;++i){const double area=(poly[i-1].x-poly[0].x)*(poly[i].z-poly[0].z)-(poly[i-1].z-poly[0].z)*(poly[i].x-poly[0].x);if(std::abs(area)<1.e-10)continue;vertex(poly[0]);vertex(poly[i-1]);vertex(poly[i]);}
    };
    for(const auto &[chunk,cells]:chunks) {
        RiverMeshRange range;range.first=int(next.vertices.size());range.bounds={chunk.first*64.,chunk.second*64.,chunk.first*64.+64,chunk.second*64.+64};
        for(const auto &cell:cells) {
            const double x=cell.first*network.mesh_step,z=cell.second*network.mesh_step;
            auto a=corner(x,z),b=corner(x+network.mesh_step,z),c=corner(x,z+network.mesh_step),d=corner(x+network.mesh_step,z+network.mesh_step);
            triangle(a,c,b);triangle(b,c,d);
            if(next.vertices.size()>vertex_limit){error="River water mesh exceeds 40MiB; split the design into smaller areas";return false;}
        }
        range.count=int(next.vertices.size())-range.first;if(range.count){range.level=next.vertices[unsigned(range.first)].y;next.ranges.push_back(range);}
    }
    out=std::move(next);error.clear();return true;
}
}
#endif
