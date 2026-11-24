#ifndef VALWOOD_WORLD_MINIMAP_H
#define VALWOOD_WORLD_MINIMAP_H
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <istream>
#include <sstream>
#include <string>
#include <utility>

// CPU-only projection/configuration and bounded cached map geometry. The HUD
// does no texture loading, GPU readback, terrain/model traversal, or input IO.
namespace world_minimap {
constexpr unsigned landmark_limit=8,line_limit=768,primitive_limit=512;
constexpr double pi=3.14159265358979323846;
struct Point {double x=0,z=0;};
enum class MarkerKind {Walker,Rider,Cart};
struct Marker {Point point;MarkerKind kind=MarkerKind::Walker;};
enum class Kind {Camp,Town,Bridge};
struct Landmark {Point point;Kind kind=Kind::Camp;std::string label;};
struct Config {
    bool enabled=false;double radius=140;
    std::array<double,4> region{};
    std::array<Landmark,landmark_limit> landmarks{};unsigned count=0;
};
inline bool finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.z)&&std::abs(p.x)<=50400&&std::abs(p.z)<=28400;}
inline bool in_region(const Config &c,Point p){return finite(p)&&p.x>=c.region[0]&&p.z>=c.region[1]&&p.x<=c.region[2]&&p.z<=c.region[3];}
inline bool parse(std::istream &input,Config &out,std::string &error){
    Config next;error.clear();std::string line;unsigned lines=0,bytes=0;bool header=false,enabled=false,radius=false,region=false;
    auto fail=[&](const char *why){error=why;return false;};
    while(std::getline(input,line)){
        if(++lines>64||(bytes+=unsigned(line.size()+1))>4096||line.size()>160)return fail("Minimap config exceeds bounds");
        std::istringstream row(line);std::string tag,extra;if(!(row>>tag)||tag[0]=='#')continue;
        if(!header){int version=0;if(tag!="VALWOOD_MINIMAP"||!(row>>version)||version!=1||(row>>extra))return fail("Expected VALWOOD_MINIMAP 1");header=true;continue;}
        if(tag=="enabled"){int value=-1;if(enabled||!(row>>value)||(value!=0&&value!=1)||(row>>extra))return fail("Invalid minimap enabled");next.enabled=value==1;enabled=true;}
        else if(tag=="radius"){if(radius||!(row>>next.radius)||!std::isfinite(next.radius)||next.radius<80||next.radius>320||(row>>extra))return fail("Minimap radius must be 80 to 320 metres");radius=true;}
        else if(tag=="region"){
            if(region||!(row>>next.region[0]>>next.region[1]>>next.region[2]>>next.region[3])||(row>>extra)||
               !finite({next.region[0],next.region[1]})||!finite({next.region[2],next.region[3]})||
               next.region[2]-next.region[0]<100||next.region[3]-next.region[1]<100)return fail("Invalid minimap region");
            region=true;
        }else if(tag=="landmark"){
            if(next.count==landmark_limit)return fail("Too many minimap landmarks");
            auto &l=next.landmarks[next.count];std::string kind;
            if(!(row>>kind>>l.point.x>>l.point.z))return fail("Invalid minimap landmark");
            row>>std::ws;if(row.peek()!='"'||!(row>>std::quoted(l.label))||(row>>extra)||!finite(l.point)||l.label.empty()||l.label.size()>24)return fail("Invalid minimap landmark");
            if(kind=="camp")l.kind=Kind::Camp;else if(kind=="town")l.kind=Kind::Town;else if(kind=="bridge")l.kind=Kind::Bridge;else return fail("Unknown minimap landmark kind");
            for(unsigned char c:l.label)if(c<32||c>126)return fail("Minimap labels must be printable ASCII");
            for(unsigned i=0;i<next.count;++i)if(next.landmarks[i].label==l.label)return fail("Duplicate minimap landmark label");
            ++next.count;
        }else return fail("Unknown minimap config record");
    }
    if(input.bad()||!header||!enabled||!radius||!region)return fail("Incomplete minimap config");
    for(unsigned i=0;i<next.count;++i)if(!in_region(next,next.landmarks[i].point))return fail("Landmark outside minimap region");
    out=std::move(next);return true;
}
// North is negative Z. Values are normalized to the square map's half-size.
inline bool project(Point world,Point player,double radius,Point &out){
    if(!finite(world)||!finite(player)||!std::isfinite(radius)||radius<=0)return false;
    out={(world.x-player.x)/radius,(world.z-player.z)/radius};return true;
}
inline Point heading(double yaw){return std::isfinite(yaw)?Point{std::sin(yaw),-std::cos(yaw)}:Point{};}
inline bool inside(Point p,double edge=1){return std::isfinite(p.x)&&std::isfinite(p.z)&&std::abs(p.x)<=edge&&std::abs(p.z)<=edge;}
inline Point clamp_marker(Point p,double edge=.86){
    const double extent=std::max(std::abs(p.x),std::abs(p.z));
    if(!std::isfinite(extent))return {};
    return extent>edge?Point{p.x*edge/extent,p.z*edge/extent}:p;
}
// Liang-Barsky clipping; optional half-width margin preserves a broad river
// whose centre lies just outside the map. The drawlist clips its final stroke.
inline bool clip(Point &a,Point &b,double margin=0){
    if(!std::isfinite(a.x)||!std::isfinite(a.z)||!std::isfinite(b.x)||!std::isfinite(b.z)||!std::isfinite(margin)||margin<0||margin>2)return false;
    const Point start=a;const double dx=b.x-a.x,dz=b.z-a.z,edge=1+margin;double lo=0,hi=1;
    auto side=[&](double p,double q){if(std::abs(p)<1.e-12)return q>=0;double t=q/p;if(p<0)lo=std::max(lo,t);else hi=std::min(hi,t);return lo<=hi;};
    if(!side(-dx,start.x+edge)||!side(dx,edge-start.x)||!side(-dz,start.z+edge)||!side(dz,edge-start.z))return false;
    a={start.x+lo*dx,start.z+lo*dz};b={start.x+hi*dx,start.z+hi*dz};return true;
}
enum class LineKind {River,Road,Bridge};
struct Line {Point a,b;double width=1;LineKind kind=LineKind::Road;};
struct Geometry {
    std::array<Line,line_limit> lines{};unsigned count=0,truncated=0;
    void clear(){count=truncated=0;}
    bool append(Point a,Point b,double width,LineKind kind){
        if(!finite(a)||!finite(b)||!std::isfinite(width)||width<=0||width>128)return false;
        if(std::hypot(a.x-b.x,a.z-b.z)<.0001)return true;
        if(count==line_limit){++truncated;return false;}
        lines[count++]={a,b,width,kind};return true;
    }
};
struct Budget {unsigned used=0,dropped=0;bool take(unsigned n){if(n>primitive_limit-used){++dropped;return false;}used+=n;return true;}};
struct Visibility {
    bool hud=true,editor=false,modal=false,dialogue=false,atlas=false,graphics=false,loading=false,cinematic=false,flying=false;
};
inline bool visible(const Config &c,Point player,const Visibility &s){return c.enabled&&in_region(c,player)&&s.hud&&!s.editor&&!s.modal&&!s.dialogue&&!s.atlas&&!s.graphics&&!s.loading&&!s.cinematic&&!s.flying;}
}
#endif
