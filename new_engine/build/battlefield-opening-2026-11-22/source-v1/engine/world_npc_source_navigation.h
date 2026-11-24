#ifndef VALWOOD_WORLD_NPC_SOURCE_NAVIGATION_H
#define VALWOOD_WORLD_NPC_SOURCE_NAVIGATION_H
#include "engine/world_navigation.h"
#include "engine/world_playtest_walk.h"
#include "engine/world_walk.h"
#include <map>
#include <queue>
#include <tuple>
namespace world_npc_source_navigation {
using Point=world_navigation::Point;
inline bool endpoint(const world_playtest_walk::Support &support,Point &p) {
    float floor=0;
    if(!support.surface(p.x,p.z,p.y,floor,.20)||!support.sample(p.x,p.z,floor,floor))return false;
    p.y=floor;return support.standing_clear(p.x,p.z,p.y);
}
inline bool line(const world_playtest_walk::Support &support,const world_navigation::Query &query,
                 Point a,Point b,float radius,std::vector<Point> *dense=nullptr,bool reference_line=false) {
    const double distance=std::hypot(b.x-a.x,b.z-a.z);
    if(!std::isfinite(distance)||distance>2048)return false;
    const unsigned count=std::max(1u,unsigned(std::ceil(distance/.125)));
    if(dense){dense->clear();dense->push_back(a);}
    Point previous=a;
    for(unsigned i=1;i<=count;i++){
        const double t=double(i)/count;Point p{a.x+(b.x-a.x)*t,0,a.z+(b.z-a.z)*t};float floor=0;
        if(!support.can_move(previous.x,previous.z,p.x,p.z,previous.y,floor))return false;
        p.y=floor;
        if(reference_line&&std::abs(p.y-(a.y+(b.y-a.y)*t))>.20)return false;
        if(query.clear&&!query.clear(query.data,previous,p,radius))return false;
        previous=p;if(dense)dense->push_back(p);
    }
    return std::abs(previous.y-b.y)<=.15;
}
inline bool inside(const world_studio::NavigationArea &area,double x,double z,double inset=0) {
    return std::abs(x-area.x)<=area.span*.5-inset&&std::abs(z-area.z)<=area.span*.5-inset;
}
// A source path may revisit an XZ cell on a different storey. Height belongs in
// the search key; a flat terrain grid cannot represent stairwells correctly.
inline bool detour(const world_playtest_walk::Support &support,const world_navigation::Query &query,
                   const world_studio::NavigationArea &area,Point start,Point goal,
                   std::vector<Point> &out) {
    if(!inside(area,start.x,start.z,area.radius)||!inside(area,goal.x,goal.z,area.radius))return false;
    constexpr double cell=.25;
    struct Node {int x=0,z=0,parent=-1;Point p;double cost=0;};
    using Key=std::tuple<int,int,int>;using Entry=std::pair<double,unsigned>;
    std::map<Key,double> best;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> pending;
    std::vector<Node> nodes={{0,0,-1,start,0}};best[{0,0,int(std::lround(start.y*100))}]=0;
    auto h=[&](const Point &p){return std::hypot(goal.x-p.x,goal.z-p.z)+std::abs(goal.y-p.y);};
    pending.push({h(start),0});unsigned examined=0;int found=-1;
    while(!pending.empty()&&examined++<24000){
        const auto entry=pending.top();pending.pop();const auto a=nodes[entry.second];
        if(entry.first>a.cost+h(a.p)+.0001)continue;
        if(std::hypot(a.p.x-goal.x,a.p.z-goal.z)<.4&&std::abs(a.p.y-goal.y)<.5&&line(support,query,a.p,goal,area.radius)){
            found=int(entry.second);break;
        }
        for(int z=-1;z<=1;z++)for(int x=-1;x<=1;x++)if(x||z){
            const int ix=a.x+x,iz=a.z+z;Point p{start.x+ix*cell,0,start.z+iz*cell};
            if(!inside(area,p.x,p.z,area.radius))continue;
            float floor=0;if(!support.can_move(a.p.x,a.p.z,p.x,p.z,a.p.y,floor))continue;p.y=floor;
            if(query.clear&&!query.clear(query.data,a.p,p,area.radius))continue;
            const double cost=a.cost+std::hypot(x*cell,z*cell)+.05*std::abs(p.y-a.p.y);
            const Key key{ix,iz,int(std::lround(p.y*100))};auto previous=best.find(key);
            if(previous!=best.end()&&previous->second<=cost)continue;
            best[key]=cost;
            const auto index=unsigned(nodes.size());nodes.push_back({ix,iz,int(entry.second),p,cost});pending.push({cost+h(p),index});
        }
    }
    if(found<0)return false;
    std::vector<Point> reverse;
    for(int index=found;index>=0;index=nodes[unsigned(index)].parent)reverse.push_back(nodes[unsigned(index)].p);
    std::reverse(reverse.begin(),reverse.end());
    if(std::hypot(reverse.back().x-goal.x,reverse.back().z-goal.z)>.005)reverse.push_back(goal);else reverse.back()=goal;
    out.clear();out.push_back(start);
    // Keep enough local height references for runtime source-floor sampling,
    // but avoid pausing the actor at every individual stair tread.
    for(unsigned i=0;i+1<reverse.size();){
        unsigned end=unsigned(reverse.size()-1);
        while(end>i+1&&!line(support,query,reverse[i],reverse[end],area.radius,nullptr,true))--end;
        if(!line(support,query,reverse[i],reverse[end],area.radius))return false;
        out.push_back(reverse[end]);i=end;
    }
    return true;
}
inline bool route(const world_studio::Actor &actor,bool enabled,bool can_walk,
                  const world_playtest_walk::Support *support,const world_navigation::Query &query,
                  const world_studio::NavigationArea *area,std::vector<Point> &out,std::string &error) {
    out.clear();
    auto fail=[&](const char *why){error=why;return false;};
    if(actor.points.empty()||actor.points.size()>32||actor.floor_heights.size()!=actor.points.size())return fail("Source-floor waypoint heights are incomplete. Move each point in the scene.");
    if(!enabled){out.push_back({actor.points[0][0],actor.floor_heights[0],actor.points[0][1]});error.clear();return true;}
    if(!support||!support->loaded())return fail("Source-floor collision is still loading. Wait for the scene update.");
    if(actor.points.size()>1&&!can_walk)return fail("This appearance is stationary. Choose a walking appearance.");
    if(area&&!area->source_floor)return fail("Choose a navigation area placed on the same source floors.");
    const float radius=area?area->radius:.37f;
    if(radius>world_playtest_walk::radius+.001f)return fail("Source-floor paths currently require an agent radius of 0.37 m or smaller.");
    std::vector<Point> stops;
    for(unsigned i=0;i<actor.points.size();++i){
        Point p{actor.points[i][0],actor.floor_heights[i],actor.points[i][1]};
        if(!std::isfinite(p.y)||!endpoint(*support,p)||(query.clear&&!query.clear(query.data,p,p,radius)))return fail("Waypoint has no clear source floor/headroom at its saved height. Move it onto a clear tread or floor.");
        if(i&&std::hypot(p.x-stops.back().x,p.z-stops.back().z)<.1)return fail("Waypoints need at least 10 cm separation.");
        stops.push_back(p);
    }
    if(stops.size()==1){out=stops;error.clear();return true;}
    if(actor.loop){if(std::hypot(stops.front().x-stops.back().x,stops.front().z-stops.back().z)<.1)return fail("The closed patrol already returns to its first point. Remove the repeated endpoint.");stops.push_back(stops.front());}
    else for(int i=int(stops.size())-2;i>=0;--i)stops.push_back(stops[unsigned(i)]);
    out.push_back(stops.front());
    for(unsigned i=1;i<stops.size();++i){
        std::vector<Point> dense;
        if(line(*support,query,stops[i-1],stops[i],radius,&dense)){
            for(unsigned at=0;at+1<dense.size();){
                unsigned end=unsigned(dense.size()-1);
                while(end>at+1&&!line(*support,query,dense[at],dense[end],radius,nullptr,true))--end;
                out.push_back(dense[end]);at=end;
            }
        }else {
            if(!area)return fail("Walking path hits a wall, stairwell or a different storey. Add waypoints along the opening, or choose a source-floor navigation area for a detour.");
            std::vector<Point> leg;if(!detour(*support,query,*area,stops[i-1],stops[i],leg))return fail("No connected source-floor route inside this navigation area. Place waypoints through the open stairs/doorway or enlarge the area.");
            out.insert(out.end(),leg.begin()+1,leg.end());
        }
        if(out.size()>128)return fail("Source-floor route needs more than 128 corners. Shorten this patrol or split it between NPCs.");
    }
    error.clear();return true;
}
// The area preview is one flood-filled storey. Actual NPC routing above keeps
// multiple heights per cell, so stairs that revisit XZ remain distinct.
inline bool bake(const world_studio::NavigationArea &area,const world_playtest_walk::Support *support,
                 const world_navigation::Query &query,world_navigation::Grid &out,std::string &error) {
    if(!support||!support->loaded()){error="Source-floor collision is still loading.";return false;}
    world_navigation::Grid next;next.area=area;next.side=unsigned(std::ceil(area.span/area.cell));
    if(!next.side||next.side>96){error="Invalid source navigation area size.";return false;}
    next.cells.resize(next.side*next.side);
    auto point=[&](unsigned i,double y){return Point{area.x+(int(i%next.side)+.5-next.side*.5)*area.cell,y,area.z+(int(i/next.side)+.5-next.side*.5)*area.cell};};
    unsigned seed=0;double best=INFINITY;
    for(unsigned i=0;i<next.cells.size();++i){const auto p=point(i,area.floor_y);const double d=std::hypot(p.x-area.x,p.z-area.z);if(d<best){best=d;seed=i;}}
    Point p=point(seed,area.floor_y);if(!endpoint(*support,p)){error="Navigation area center has no clear source floor at its saved storey.";return false;}
    std::queue<unsigned> pending;next.cells[seed]={float(p.y),true,0};pending.push(seed);next.open=1;
    constexpr int dx[]={1,0,-1,0,1,-1,-1,1},dz[]={0,1,0,-1,1,1,-1,-1};
    while(!pending.empty()){
        const auto i=pending.front();pending.pop();const auto a=point(i,next.cells[i].y);const int x=int(i%next.side),z=int(i/next.side);
        for(unsigned k=0;k<8;k++){
            const int nx=x+dx[k],nz=z+dz[k];if(nx<0||nz<0||nx>=int(next.side)||nz>=int(next.side))continue;
            const unsigned j=unsigned(nz)*next.side+unsigned(nx);auto b=point(j,a.y);float floor=0;
            if(!support->can_move(a.x,a.z,b.x,b.z,a.y,floor))continue;
            b.y=floor;
            if(query.clear&&!query.clear(query.data,a,b,area.radius))continue;
            if(!next.cells[j].open){next.cells[j]={floor,true,0};++next.open;pending.push(j);}
            if(std::abs(next.cells[j].y-floor)<.15)next.cells[i].edges|=1u<<k;
        }
    }
    out=std::move(next);error.clear();return true;
}
// Terrain remains valid inside a broad source-scene AABB. Only a real floor
// sampled near the caller's feet can replace it; distant roofs never supply
// a storey. Bounded presentation sectors intentionally retain their hard edge.
inline bool terrain_floor(const world_playtest_walk::Support *support,bool complete,
                          double x,double z,double &height) {
    if(!support)return true;
    float source=0;
    if(support->sample(x,z,height,source))return std::abs(source-height)<=.15;
    return complete;
}
inline bool terrain_clear(const world_playtest_walk::Support &support,bool complete,
                          Point a,Point b,float radius,float (*terrain)(double,double,void *),void *context) {
    float old_floor=0,next_floor=0;
    const bool old_supported=support.sample(a.x,a.z,a.y,old_floor);
    const bool next_supported=support.sample(b.x,b.z,a.y,next_floor);
    if(!complete||(old_supported&&next_supported)){
        float approved=0;
        if(!support.can_move(a.x,a.z,b.x,b.z,a.y,approved)||std::abs(approved-b.y)>.15)return false;
    }else {
        if(!terrain)return false;
        const double old=old_supported?old_floor:terrain(a.x,a.z,context);
        const double next=next_supported?next_floor:terrain(b.x,b.z,context);
        if(!std::isfinite(old)||!std::isfinite(next)||std::abs(old-a.y)>.15||std::abs(next-b.y)>.15||
           next-a.y>world_playtest_walk::max_step+.002||a.y-next>world_playtest_walk::max_drop+.002)return false;
        if(!old_supported&&!next_supported&&!world_walk_can_move(a.x,a.z,b.x,b.z,terrain,context,1.2))return false;
        world_physics::Hit hit;
        if(!support.cast_capsule_movement({a.x,a.y+.9,a.z},radius,.50f,{b.x-a.x,b.y-a.y,b.z-a.z},hit)||
           (hit.hit&&hit.fraction<.999f&&hit.normal.y<.65f))return false;
    }
    // A zero-length navigation query still has to reject a body embedded in
    // source walls or under a low ceiling, including atlas-supported bodies.
    if(std::hypot(b.x-a.x,b.z-a.z)<.001){
        for(const auto &delta:{world_physics::Vec3{.01,0,0},{-.01,0,0},{0,0,.01},{0,0,-.01},{0,.01,0}}){
            world_physics::Hit hit;
            if(!support.cast_capsule_movement({a.x,a.y+.9,a.z},radius,.50f,delta,hit)||
               (hit.hit&&hit.fraction<.999f&&hit.normal.y<.65f))return false;
        }
    }
    return true;
}

}
#endif
