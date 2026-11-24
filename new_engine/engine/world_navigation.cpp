#include "engine/world_navigation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
namespace world_navigation {
namespace {
constexpr int dx[]={1,0,-1,0,1,-1,-1,1},dz[]={0,1,0,-1,1,1,-1,-1};
Point point(const Grid &g,unsigned index){return {g.area.x-g.side*g.area.cell*.5+(index%g.side+.5)*g.area.cell,g.cells[index].y,g.area.z-g.side*g.area.cell*.5+(index/g.side+.5)*g.area.cell};}
bool line(const Query &q,Point a,Point b,float radius,float slope=.65f){const double distance=std::hypot(b.x-a.x,b.z-a.z);if(distance>2048)return false;unsigned count=unsigned(std::max(1.,std::ceil(distance/.4)));Point previous=a;for(unsigned i=0;i<=count;i++){double t=double(i)/count;Point p{a.x+(b.x-a.x)*t,0,a.z+(b.z-a.z)*t};if(!q.ground(q.data,p.x,p.z,p.y)||std::abs(p.y-previous.y)>std::max(.35,distance/count*slope)||!q.clear(q.data,previous,p,radius))return false;previous=p;}return true;}
int cell(const Grid &g,Point p){int x=int(std::floor((p.x-g.area.x)/g.area.cell+g.side*.5)),z=int(std::floor((p.z-g.area.z)/g.area.cell+g.side*.5));return x<0||z<0||x>=int(g.side)||z>=int(g.side)?-1:z*int(g.side)+x;}
bool segment(const Grid *g,const Query &q,Point a,Point b,float radius,std::vector<Point> &out,std::string &e){
 if(line(q,a,b,radius,g?g->area.slope:.65f)){out={a,b};return true;}if(!g){e="Route crosses water, steep terrain or a solid. Bake a local navigation area for a detour.";return false;}
 int start=cell(*g,a),goal=cell(*g,b);if(start<0||goal<0||!g->cells[unsigned(start)].open||!g->cells[unsigned(goal)].open){e="Route endpoint is outside the baked area or obstructed";return false;}if(!line(q,a,point(*g,unsigned(start)),radius,g->area.slope)||!line(q,point(*g,unsigned(goal)),b,radius,g->area.slope)){e="Cannot join the exact route endpoint to the navigation grid";return false;}
 const unsigned n=unsigned(g->cells.size());std::vector<float> cost(n,std::numeric_limits<float>::infinity());std::vector<int> parent(n,-1);using Entry=std::pair<float,unsigned>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;auto h=[&](unsigned i){return float(std::hypot(int(i%g->side)-goal%int(g->side),int(i/g->side)-goal/int(g->side))*g->area.cell);};cost[unsigned(start)]=0;queue.push({h(unsigned(start)),unsigned(start)});
 unsigned visited=0;while(!queue.empty()&&visited<n*8){auto [estimate,i]=queue.top();queue.pop();if(estimate>cost[i]+h(i)+.0001f)continue;visited++;if(i==unsigned(goal))break;for(unsigned k=0;k<8;k++)if(g->cells[i].edges&(1u<<k)){unsigned next=unsigned(int(i)+dz[k]*int(g->side)+dx[k]);float value=cost[i]+g->area.cell*(k<4?1.f:1.41421356f);if(value<cost[next]){cost[next]=value;parent[next]=int(i);queue.push({value+h(next),next});}}}
 if(start!=goal&&parent[unsigned(goal)]<0){e="No connected navigation route";return false;}std::vector<Point> reverse;for(int i=goal;i!=start;i=parent[unsigned(i)]){if(i<0||reverse.size()>n){e="Broken navigation parent chain";return false;}reverse.push_back(point(*g,unsigned(i)));}reverse.push_back(point(*g,unsigned(start)));std::reverse(reverse.begin(),reverse.end());reverse.insert(reverse.begin(),a);reverse.push_back(b);
 out={a};for(unsigned i=0;i+1<reverse.size();){unsigned end=unsigned(reverse.size()-1);while(end>i+1&&!line(q,reverse[i],reverse[end],radius,g->area.slope))end--;if(std::hypot(out.back().x-reverse[end].x,out.back().z-reverse[end].z)>.02)out.push_back(reverse[end]);i=end;}return true;
}
}
bool bake(const world_studio::NavigationArea &area,const Query &q,Grid &output,std::string &error){
 if(!q.ground||!q.clear||!std::isfinite(area.span)||!std::isfinite(area.cell)||area.cell<1||area.span<8||area.span/area.cell>96){error="Invalid navigation query/budget";return false;}Grid g;g.area=area;g.side=unsigned(std::ceil(area.span/area.cell));g.cells.resize(g.side*g.side);
 for(unsigned i=0;i<g.cells.size();i++){Point p=point(g,i);bool open=q.ground(q.data,p.x,p.z,p.y);g.cells[i].y=float(p.y);g.cells[i].open=open&&q.clear(q.data,p,p,area.radius);g.open+=g.cells[i].open;}
 for(unsigned i=0;i<g.cells.size();i++)if(g.cells[i].open){Point a=point(g,i);int x=int(i%g.side),z=int(i/g.side);for(unsigned k=0;k<8;k++){int nx=x+dx[k],nz=z+dz[k];if(nx<0||nz<0||nx>=int(g.side)||nz>=int(g.side))continue;unsigned j=unsigned(nz)*g.side+unsigned(nx);if(!g.cells[j].open)continue;if(k>=4&&(!g.cells[unsigned(z)*g.side+unsigned(nx)].open||!g.cells[unsigned(nz)*g.side+unsigned(x)].open))continue;auto b=point(g,j);if(std::abs(b.y-a.y)<=std::max(.35f,area.cell*area.slope*(k<4?1.f:1.414214f))&&line(q,a,b,area.radius,area.slope))g.cells[i].edges|=1u<<k;}}
 output=std::move(g);error.clear();return true;
}
bool path(const Grid *grid,const Query &query,const std::vector<std::array<double,2>> &points,bool loop,float radius,std::vector<Point> &out,std::string &error){
 out.clear();if(points.empty()||points.size()>32||!query.ground||!query.clear){error="Choose one to32 waypoints";return false;}std::vector<Point> route;for(auto p:points){Point a{p[0],0,p[1]};if(!query.ground(query.data,a.x,a.z,a.y)||!query.clear(query.data,a,a,radius)){error="Actor endpoint is submerged or blocked";return false;}if(!route.empty()&&std::hypot(route.back().x-a.x,route.back().z-a.z)<.1){error="Waypoints need at least10cm separation";return false;}route.push_back(a);}
 if(route.size()==1){out=route;return true;}if(loop)route.push_back(route.front());else for(int i=int(route.size())-2;i>=0;i--)route.push_back(route[unsigned(i)]);
 std::vector<Point> result;for(unsigned i=1;i<route.size();i++){std::vector<Point> leg;if(!segment(grid,query,route[i-1],route[i],radius,leg,error))return false;if(!result.empty())leg.erase(leg.begin());result.insert(result.end(),leg.begin(),leg.end());if(result.size()>128){error="Route needs more than128 baked corners; simplify waypoints or change area";return false;}}out=std::move(result);return true;
}
}
