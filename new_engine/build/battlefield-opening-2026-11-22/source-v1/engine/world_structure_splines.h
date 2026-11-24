#ifndef VALWOOD_WORLD_STRUCTURE_SPLINES_H
#define VALWOOD_WORLD_STRUCTURE_SPLINES_H
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

// Authored definitions own regenerated runtime geometry. Generated pieces are
// deliberately not Placement records: rebuilding cannot accumulate duplicates.
namespace world_structure {
using Id=std::uint64_t;
constexpr unsigned definition_limit=16,point_limit=32,segment_limit=512;
constexpr Id id_limit=0xffffffffull;
struct Point {double x=0,y=0,z=0;};
enum class Kind : unsigned {Wall=0,Bridge=1};
struct Definition {
    Id id=0;Kind kind=Kind::Wall;std::string alias;
    double spacing=3,width=4,height=3,thickness=.35,rail_height=1.1,support_depth=12;
    bool railings=true,supports=true,smooth=false,follow_ground=false;
    std::vector<Point> points;
};
struct Segment {
    Id owner_id=0;unsigned index=0;
    Point a,b,left_a,right_a,left_b,right_b;
    double length=0,horizontal_length=0,yaw=0,pitch=0;
};
struct Generated {Id owner_id=0;Kind kind=Kind::Wall;std::vector<Segment> segments;};
inline bool finite_range(double v,double a,double b){return std::isfinite(v)&&v>=a&&v<=b;}
inline bool valid_point(Point p){return finite_range(p.x,-50400,50400)&&finite_range(p.y,-2000,12000)&&finite_range(p.z,-28400,28400);}
inline bool valid_alias(const std::string &s){return !s.empty()&&s.size()<96&&std::all_of(s.begin(),s.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
inline Point mix(Point a,Point b,double t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
inline double distance(Point a,Point b){return std::hypot(std::hypot(a.x-b.x,a.z-b.z),a.y-b.y);}

// module_length is a measured, scaled asset AABB length, along its chosen local
// longitudinal axis. Each authored span is evenly filled and ends exactly at
// its control points; callers center/reorient the module from its measured AABB.
inline bool build(const Definition &d,Generated &out,std::string &error,double module_length=0){
    out={};error.clear();auto fail=[&](const char *s){error=s;return false;};
    if(!d.id||d.id>id_limit||unsigned(d.kind)>1||
       (!d.alias.empty()&&!valid_alias(d.alias))||
       !finite_range(d.spacing,.25,32)||!finite_range(d.width,.5,24)||
       !finite_range(d.height,.5,20)||!finite_range(d.thickness,.08,4)||
       !finite_range(d.rail_height,.5,2.5)||!finite_range(d.support_depth,0,64)||
       d.points.size()<2||d.points.size()>point_limit||!finite_range(module_length,0,256))return fail("Invalid wall/bridge definition");
    for(Point p:d.points)if(!valid_point(p)||p.y+(d.kind==Kind::Wall?d.height:(d.railings?d.rail_height:0))>12000)return fail("Structure point or top outside editable world");
    for(unsigned i=1;i<d.points.size();++i)if(std::hypot(d.points[i].x-d.points[i-1].x,d.points[i].z-d.points[i-1].z)<.1)return fail("Structure control points must be at least 10 cm apart horizontally");
    const double spacing=d.kind==Kind::Wall&&module_length>0?module_length:d.spacing;
    if(spacing<.1)return fail("Wall module is too short");
    std::vector<Point> points=d.points;
    if(d.smooth){
        points.clear();points.push_back(d.points.front());
        for(unsigned i=0;i+1<d.points.size();++i){
            const Point p1=d.points[i],p2=d.points[i+1];
            const Point p0=i?d.points[i-1]:Point{2*p1.x-p2.x,2*p1.y-p2.y,2*p1.z-p2.z};
            const Point p3=i+2<d.points.size()?d.points[i+2]:Point{2*p2.x-p1.x,2*p2.y-p1.y,2*p2.z-p1.z};
            const double raw_steps=std::max(4.,std::ceil(distance(p1,p2)/std::min(2.,spacing)));
            if(raw_steps>segment_limit||points.size()+unsigned(raw_steps)>segment_limit+1)return fail("Curved structure exceeds 512 sampled spans");
            const unsigned steps=unsigned(raw_steps);
            for(unsigned j=1;j<=steps;++j){
                const double t=double(j)/steps,t2=t*t,t3=t2*t;
                const auto catmull=[&](double a,double b,double c,double e){return .5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-e)*t2+(-a+3*b-3*c+e)*t3);};
                Point p=j==steps?p2:Point{catmull(p0.x,p1.x,p2.x,p3.x),catmull(p0.y,p1.y,p2.y,p3.y),catmull(p0.z,p1.z,p2.z,p3.z)};
                if(!valid_point(p)||p.y+(d.kind==Kind::Wall?d.height:(d.railings?d.rail_height:0))>12000)return fail("Curved structure overshoots editable world");
                points.push_back(p);
            }
        }
    }
    std::vector<Point> offsets(points.size());
    std::vector<double> lengths,horizontal;std::vector<unsigned> counts;
    unsigned total=0;
    for(unsigned i=1;i<points.size();++i){
        Point a=points[i-1],b=points[i];double h=std::hypot(b.x-a.x,b.z-a.z),len=distance(a,b);
        if(h<.1)return fail("Structure points must be at least 10 cm apart horizontally");
        if(std::abs(b.y-a.y)/h>(d.kind==Kind::Bridge?.65:2.))return fail("Structure span is too steep");
        const double pieces=std::ceil(len/spacing);
        if(pieces>segment_limit||total+unsigned(pieces)>segment_limit)return fail("Structure exceeds 512 generated segments");
        counts.push_back(unsigned(pieces));lengths.push_back(len);horizontal.push_back(h);total+=unsigned(pieces);
    }
    // Horizontal miter offsets form shared deck-edge vertices even on slopes.
    // Reject near reversals instead of generating unbounded spikes or cracks.
    for(unsigned i=0;i<points.size();++i){
        const unsigned before=i?i-1:0,after=std::min<unsigned>(i,points.size()-2);
        const auto a=points[before],b=points[before+1],c=points[after],e=points[after+1];
        const double ax=(b.x-a.x)/horizontal[before],az=(b.z-a.z)/horizontal[before];
        const double bx=(e.x-c.x)/horizontal[after],bz=(e.z-c.z)/horizontal[after];
        if(ax*bx+az*bz<-.5)return fail("Structure turns must be 120 degrees or less");
        const double nx=-az-bz,nz=ax+bx,n=std::hypot(nx,nz),den=(nx*(-bz)+nz*bx)/n;
        if(!std::isfinite(den)||den<.5)return fail("Invalid structure corner");
        const double half=(d.kind==Kind::Bridge?d.width:d.thickness)*.5;
        offsets[i]={nx/n*half/den,0,nz/n*half/den};
    }
    Generated next;next.owner_id=d.id;next.kind=d.kind;next.segments.reserve(total);
    constexpr double degrees=57.29577951308232;
    for(unsigned i=0;i<counts.size();++i)for(unsigned j=0;j<counts[i];++j){
        const double t0=double(j)/counts[i],t1=double(j+1)/counts[i];Segment s;
        s.owner_id=d.id;s.index=unsigned(next.segments.size());s.a=mix(points[i],points[i+1],t0);s.b=mix(points[i],points[i+1],t1);
        const auto oa=mix(offsets[i],offsets[i+1],t0),ob=mix(offsets[i],offsets[i+1],t1);
        s.left_a={s.a.x+oa.x,s.a.y,s.a.z+oa.z};s.right_a={s.a.x-oa.x,s.a.y,s.a.z-oa.z};
        s.left_b={s.b.x+ob.x,s.b.y,s.b.z+ob.z};s.right_b={s.b.x-ob.x,s.b.y,s.b.z-ob.z};
        s.length=lengths[i]/counts[i];s.horizontal_length=horizontal[i]/counts[i];
        s.yaw=std::atan2(s.b.x-s.a.x,s.b.z-s.a.z)*degrees;
        s.pitch=-std::atan2(s.b.y-s.a.y,s.horizontal_length)*degrees;
        if(!valid_point(s.left_a)||!valid_point(s.left_b)||!valid_point(s.right_a)||!valid_point(s.right_b))return fail("Structure width extends outside editable world");
        const auto area=[](Point a,Point b,Point c){return (b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z);};
        if(area(s.left_a,s.left_b,s.right_a)<1e-8||area(s.right_a,s.left_b,s.right_b)<1e-8)return fail("Structure is too wide for this corner or sampled span");
        next.segments.push_back(s);
    }
    out=std::move(next);return true;
}
inline bool validate(const Definition &d,std::string &e){Generated generated;return build(d,generated,e);}
inline bool validate(const std::vector<Definition> &defs,Id next_id,std::string &e){
    if(defs.size()>definition_limit||next_id<1||next_id>id_limit+1){e="Structure budget/identity exceeded";return false;}
    for(unsigned i=0;i<defs.size();++i){
        if(defs[i].id>=next_id){e="Structure identity high-water mark is invalid";return false;}
        for(unsigned j=0;j<i;++j)if(defs[i].id==defs[j].id){e="Duplicate structure identity";return false;}
        if(!validate(defs[i],e))return false;
    }e.clear();return true;
}
// Pure issuance: failed validation does not consume an ID or modify definitions.
inline bool append(std::vector<Definition> &defs,Id &next_id,Definition d,Id &issued,std::string &e){
    issued=0;if(!validate(defs,next_id,e))return false;
    if(defs.size()==definition_limit||next_id>id_limit){e="Structure budget/identity exhausted";return false;}
    d.id=next_id;if(!validate(d,e))return false;defs.push_back(std::move(d));issued=next_id++;return true;
}
inline Point snap_grid(Point p,double grid,bool vertical=false){
    if(!valid_point(p)||!finite_range(grid,.01,128))return p;
    const Point original=p;p.x=std::round(p.x/grid)*grid;p.z=std::round(p.z/grid)*grid;if(vertical)p.y=std::round(p.y/grid)*grid;return valid_point(p)?p:original;
}
struct SnapResult {Point point;Id owner_id=0;unsigned point_index=0;bool snapped=false;};
inline SnapResult snap_point(Point p,const std::vector<Definition> &defs,double radius,Id exclude_owner=0){
    SnapResult result;result.point=p;if(!valid_point(p)||!finite_range(radius,0,32))return result;
    double nearest=radius;
    for(const auto &d:defs)if(d.id!=exclude_owner)for(unsigned i=0;i<d.points.size();++i){
        if(!valid_point(d.points[i]))continue;
        double gap=distance(p,d.points[i]);if(gap<=nearest&&(!result.snapped||gap<nearest)){
            nearest=gap;result={d.points[i],d.id,i,true};
        }
    }return result;
}
// Same triangle diagonal as the procedural top deck: LA,LB,RA / RA,LB,RB.
// Runtime storey-aware collision must still filter this height by actor feet.
inline bool triangle_height(Point a,Point b,Point c,double x,double z,double &y){
    if(!valid_point(a)||!valid_point(b)||!valid_point(c))return false;
    const double det=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);
    if(!std::isfinite(x)||!std::isfinite(z)||std::abs(det)<1e-10)return false;
    double u=((b.z-c.z)*(x-c.x)+(c.x-b.x)*(z-c.z))/det;
    double v=((c.z-a.z)*(x-c.x)+(a.x-c.x)*(z-c.z))/det,w=1-u-v;
    if(u< -1e-8||v< -1e-8||w< -1e-8)return false;
    y=u*a.y+v*b.y+w*c.y;return true;
}
inline bool deck_height(const Segment &s,double x,double z,double &y){
    return triangle_height(s.left_a,s.left_b,s.right_a,x,z,y)||triangle_height(s.right_a,s.left_b,s.right_b,x,z,y);
}
}
#endif
