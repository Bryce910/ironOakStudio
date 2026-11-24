#ifndef VALWOOD_WORLD_COVER_COLLISION_H
#define VALWOOD_WORLD_COVER_COLLISION_H
#include "engine/world_cover.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

// Owner-thread collision proxies for the exact published cover generations.
// Trees use narrow trunk cylinders; rocks use supplied near-mesh oriented
// bounds, including the renderer's burial offset. Leaves/grass never collide.
// This is a locomotion service, not a rigid-body or walkable-top simulator.
namespace world_cover_collision {
enum class Kind {Trunk,Rock};
struct Shape {
    Kind kind=Kind::Trunk;
    std::array<float,3> minimum={-.3f,0,-.3f},maximum={.3f,3,.3f};
    float trunk_radius=.3f,y_offset=0;
};
struct Obstacle {
    Kind kind=Kind::Trunk;size_t source_index=0;
    double x=0,z=0,ymin=0,ymax=0,radius=.3,cosine=1,sine=0;
    double xmin=0,xmax=0,zmin=0,zmax=0,extent=0;
};
namespace detail {
inline double point_segment(double px,double pz,double ax,double az,double bx,double bz){
    const double dx=bx-ax,dz=bz-az,length=dx*dx+dz*dz;
    const double t=length>0?std::clamp(((px-ax)*dx+(pz-az)*dz)/length,0.,1.):0.;
    const double x=px-ax-dx*t,z=pz-az-dz*t;return x*x+z*z;
}
inline double point_box(double x,double z,const Obstacle &o){const double dx=x-std::clamp(x,o.xmin,o.xmax),dz=z-std::clamp(z,o.zmin,o.zmax);return dx*dx+dz*dz;}
inline bool segment_box(double ax,double az,double bx,double bz,const Obstacle &o){
    double lo=0,hi=1;
    auto slab=[&](double start,double delta,double minimum,double maximum){
        if(std::abs(delta)<1.e-15)return start>=minimum&&start<=maximum;
        double a=(minimum-start)/delta,b=(maximum-start)/delta;if(a>b)std::swap(a,b);lo=std::max(lo,a);hi=std::min(hi,b);return lo<=hi;
    };
    return slab(ax,bx-ax,o.xmin,o.xmax)&&slab(az,bz-az,o.zmin,o.zmax);
}
inline double segment_segment(double ax,double az,double bx,double bz,double cx,double cz,double dx,double dz){
    // Rock edges are axis-aligned locally; the point/edge minima plus an
    // explicit crossing test are exact in the ground plane.
    const double ux=bx-ax,uz=bz-az,vx=dx-cx,vz=dz-cz,den=ux*vz-uz*vx;
    if(std::abs(den)>1.e-15){const double wx=cx-ax,wz=cz-az,t=(wx*vz-wz*vx)/den,s=(wx*uz-wz*ux)/den;if(t>=0&&t<=1&&s>=0&&s<=1)return 0;}
    return std::min({point_segment(ax,az,cx,cz,dx,dz),point_segment(bx,bz,cx,cz,dx,dz),point_segment(cx,cz,ax,az,bx,bz),point_segment(dx,dz,ax,az,bx,bz)});
}
inline double segment_box_distance(double ax,double az,double bx,double bz,const Obstacle &o){
    if(segment_box(ax,az,bx,bz,o))return 0;
    return std::min({segment_segment(ax,az,bx,bz,o.xmin,o.zmin,o.xmax,o.zmin),segment_segment(ax,az,bx,bz,o.xmax,o.zmin,o.xmax,o.zmax),segment_segment(ax,az,bx,bz,o.xmax,o.zmax,o.xmin,o.zmax),segment_segment(ax,az,bx,bz,o.xmin,o.zmax,o.xmin,o.zmin)});
}
}
class Field {
    struct Generation {std::vector<Obstacle> obstacles;std::vector<size_t> sorted;double extent=0;std::uint64_t generation=0;};
    std::array<Generation,2> generations_;
public:
    mutable size_t last_candidates=0;
    bool publish(const WorldCoverField &field,const Shape *shapes,size_t shape_count,
                 const unsigned char *variants,size_t variant_count,std::string &error){
        if((field.kind!=WORLD_COVER_TREES&&field.kind!=WORLD_COVER_ROCKS)||!shapes||!shape_count||
           (field.count&&(!field.instances||!variants))||variant_count!=field.count||
           field.count>(field.kind==WORLD_COVER_TREES?WORLD_COVER_TREE_CAP:WORLD_COVER_ROCK_CAP)||
           !std::isfinite(field.anchor_x)||!std::isfinite(field.anchor_z)){error="Invalid committed cover collision publication";return false;}
        Generation next;next.generation=field.generation;next.obstacles.reserve(field.count);next.sorted.reserve(field.count);
        for(size_t i=0;i<field.count;++i){
            if(variants[i]>=shape_count){error="Cover collision variant does not match its rendered model";return false;}
            const auto &s=shapes[variants[i]];const auto &a=field.instances[i];
            if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.z)||!std::isfinite(a.yaw)||!std::isfinite(a.scale)||a.scale<=0||a.scale>32||
               !std::isfinite(s.trunk_radius)||s.trunk_radius<=0||!std::isfinite(s.y_offset)){error="Invalid cover collision root or shape";return false;}
            for(unsigned axis=0;axis<3;++axis)if(!std::isfinite(s.minimum[axis])||!std::isfinite(s.maximum[axis])||s.minimum[axis]>s.maximum[axis]){error="Invalid near-model collision bounds";return false;}
            Obstacle o;o.kind=s.kind;o.source_index=i;o.x=field.anchor_x+a.x;o.z=field.anchor_z+a.z;o.ymin=a.y+(s.minimum[1]+s.y_offset)*a.scale;o.ymax=a.y+(s.maximum[1]+s.y_offset)*a.scale;
            o.radius=s.trunk_radius*a.scale;o.cosine=std::cos(a.yaw);o.sine=std::sin(a.yaw);o.xmin=s.minimum[0]*a.scale;o.xmax=s.maximum[0]*a.scale;o.zmin=s.minimum[2]*a.scale;o.zmax=s.maximum[2]*a.scale;
            o.extent=o.kind==Kind::Trunk?o.radius:std::hypot(std::max(std::abs(o.xmin),std::abs(o.xmax)),std::max(std::abs(o.zmin),std::abs(o.zmax)));
            if(!std::isfinite(o.x)||!std::isfinite(o.z)||!std::isfinite(o.extent)||o.extent>256){error="Cover collision footprint exceeds supported bounds";return false;}
            next.extent=std::max(next.extent,o.extent);next.sorted.push_back(i);next.obstacles.push_back(o);
        }
        std::sort(next.sorted.begin(),next.sorted.end(),[&](size_t a,size_t b){return next.obstacles[a].x==next.obstacles[b].x?a<b:next.obstacles[a].x<next.obstacles[b].x;});
        generations_[field.kind==WORLD_COVER_TREES?0:1]=std::move(next);error.clear();return true;
    }
    const std::vector<Obstacle> &obstacles(Kind kind)const{return generations_[kind==Kind::Trunk?0:1].obstacles;}
    std::uint64_t generation(Kind kind)const{return generations_[kind==Kind::Trunk?0:1].generation;}
    size_t size()const{return generations_[0].obstacles.size()+generations_[1].obstacles.size();}
    bool can_move(double from_x,double from_z,double to_x,double to_z,double feet,double height,double radius)const{
        last_candidates=0;
        if(!std::isfinite(from_x)||!std::isfinite(from_z)||!std::isfinite(to_x)||!std::isfinite(to_z)||!std::isfinite(feet)||!std::isfinite(height)||!std::isfinite(radius)||height<=0||radius<=0)return false;
        for(const auto &g:generations_){
            const double lo=std::min(from_x,to_x)-radius-g.extent,hi=std::max(from_x,to_x)+radius+g.extent;
            auto at=std::lower_bound(g.sorted.begin(),g.sorted.end(),lo,[&](size_t index,double x){return g.obstacles[index].x<x;});
            for(;at!=g.sorted.end()&&g.obstacles[*at].x<=hi;++at){const auto &o=g.obstacles[*at];
                if(o.z+o.extent<std::min(from_z,to_z)-radius||o.z-o.extent>std::max(from_z,to_z)+radius||feet>=o.ymax-.01||feet+height<=o.ymin+.01)continue;
                ++last_candidates;const double ax=from_x-o.x,az=from_z-o.z,bx=to_x-o.x,bz=to_z-o.z;
                if(o.kind==Kind::Trunk){
                    const double expanded=o.radius+radius,old=ax*ax+az*az,next=bx*bx+bz*bz;
                    if(old<expanded*expanded&&next>=old&&(ax*(bx-ax)+az*(bz-az))>=-1.e-8)continue;
                    if(detail::point_segment(0,0,ax,az,bx,bz)<expanded*expanded-1.e-8)return false;
                }else{
                    const double x0=o.cosine*ax-o.sine*az,z0=o.sine*ax+o.cosine*az,x1=o.cosine*bx-o.sine*bz,z1=o.sine*bx+o.cosine*bz;
                    const double old=detail::point_box(x0,z0,o),next=detail::point_box(x1,z1,o);
                    const double nx=x0-std::clamp(x0,o.xmin,o.xmax),nz=z0-std::clamp(z0,o.zmin,o.zmax);
                    if(old<radius*radius&&next>=old&&(nx*(x1-x0)+nz*(z1-z0))>=-1.e-8)continue;
                    if(detail::segment_box_distance(x0,z0,x1,z1,o)<radius*radius-1.e-8)return false;
                }
            }
        }
        return true;
    }
};
}
#endif
