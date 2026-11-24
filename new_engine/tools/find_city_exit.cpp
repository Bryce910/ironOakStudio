// CPU-only source-city route discovery using the same capsule and slope policy
// as check_city_collision. Does not launch or modify the game/project.
#include "engine/world_physics.h"
#include "engine/world_height_region.h"
#include "engine/world_walk.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <queue>
#include <string>
#include <vector>
struct Point{double x,z;};
struct Search{
    WorldHeightRegion height={};world_physics::World world;unsigned queries=0,blocked=0,slopes=0;world_physics::Hit last_block;
    static float sample(double x,double z,void *data){float y=0;return world_height_region_sample(&static_cast<Search*>(data)->height,x,z,&y)?y:NAN;}
    bool clear(Point a,Point b){
        const double length=std::hypot(b.x-a.x,b.z-a.z);const unsigned n=std::max(1u,unsigned(std::ceil(length/.125)));
        for(unsigned i=0;i<n;i++){
            Point p={a.x+(b.x-a.x)*i/n,a.z+(b.z-a.z)*i/n},q={a.x+(b.x-a.x)*(i+1)/n,a.z+(b.z-a.z)*(i+1)/n};
            float y=sample(p.x,p.z,this);if(!std::isfinite(y)||!world_walk_can_move(p.x,p.z,q.x,q.z,sample,this,1.2)){slopes++;return false;}
            world_physics::Hit hit;queries++;if(!world.cast_capsule({p.x,y+.9,p.z},.37f,.5f,{q.x-p.x,0,q.z-p.z},hit)){std::fprintf(stderr,"Jolt query failed\n");return false;}
            if(hit.hit&&hit.normal.y<.65&&hit.fraction<.999f){last_block=hit;blocked++;return false;}
        }return true;
    }
    ~Search(){world_height_region_clear(&height);}
};
int main(int argc,char **argv){
    if(argc!=9){std::fprintf(stderr,"Usage: find-city-exit city.physics showcase.height originX originZ spawnSourceX spawnSourceZ output.walk output.json\n");return 2;}
    Search s;char message[256];std::string error;world_physics::Document doc;
    if(world_height_region_load(&s.height,argv[2],message,sizeof(message))!=1||!world_physics::load(argv[1],doc,error)||!s.world.configure(doc,error)){std::fprintf(stderr,"Cannot prepare source search: %s %s\n",message,error.c_str());return 1;}
    const double ox=std::stod(argv[3]),oz=std::stod(argv[4]);Point spawn={ox+std::stod(argv[5]),oz+std::stod(argv[6])};
    constexpr double step=.25;
    const double xmin=ox+std::ceil((s.height.xmin-ox)/step)*step,zmin=oz+std::ceil((s.height.zmin-oz)/step)*step,xmax=s.height.xmin+(s.height.columns-1)*s.height.stepx,zmax=s.height.zmin+(s.height.rows-1)*s.height.stepz;
    const int cols=int(std::floor((xmax-xmin)/step))+1,rows=int(std::floor((zmax-zmin)/step))+1;
    if(cols<2||rows<2||cols>1024||rows>1024){std::fprintf(stderr,"Search grid exceeds1024-side bound\n");return 1;}
    auto point=[&](int id){return Point{xmin+(id%cols)*step,zmin+(id/cols)*step};};
    const int startx=int(std::lround((spawn.x-xmin)/step)),startz=int(std::lround((spawn.z-zmin)/step));
    if(startx<0||startx>=cols||startz<0||startz>=rows){std::fprintf(stderr,"Spawn outside source grid\n");return 1;}
    const int start=startz*cols+startx;std::vector<int> prev(cols*rows,-1),distance(cols*rows,0);std::queue<int> queue;prev[start]=start;queue.push(start);
    if(!s.clear(spawn,point(start))){std::fprintf(stderr,"Spawn cannot join search grid\n");return 1;}
    const std::array<std::array<int,2>,8> directions={{{-1,0},{0,-1},{0,1},{1,0},{-1,-1},{-1,1},{1,-1},{1,1}}};
    std::vector<int> candidates;unsigned visited=0;std::array<double,4> reached={spawn.x,spawn.z,spawn.x,spawn.z};
    while(!queue.empty()){
        const int id=queue.front();queue.pop();visited++;const int x=id%cols,z=id/cols;
        const auto p=point(id);reached={std::min(reached[0],p.x),std::min(reached[1],p.z),std::max(reached[2],p.x),std::max(reached[3],p.z)};
        if(x<=2||z<=2||x>=cols-3||z>=rows-3)candidates.push_back(id);
        for(auto d:directions){int nx=x+d[0],nz=z+d[1];if(nx<0||nx>=cols||nz<0||nz>=rows)continue;const int next=nz*cols+nx;if(prev[next]>=0)continue;
            if(s.clear(point(id),point(next))){prev[next]=id;distance[next]=distance[id]+1;queue.push(next);}
        }
    }
    std::sort(candidates.begin(),candidates.end(),[&](int a,int b){return distance[a]+(a%cols<=2?0:10000)<distance[b]+(b%cols<=2?0:10000);});
    int goal=-1;Point exit={};const char *edge="none";for(int id:candidates){Point p=point(id),q=p;const int x=id%cols,z=id/cols;const char *side;
        if(x<=2){q.x=s.height.xmin+.10;side="west";}else if(x>=cols-3){q.x=xmax-.10;side="east";}else if(z<=2){q.z=s.height.zmin+.10;side="north";}else{q.z=zmax-.10;side="south";}
        if(s.clear(p,q)){goal=id;exit=q;edge=side;break;}}
    const bool boundary_reached=goal>=0;world_physics::Hit blocker;
    if(!boundary_reached){
        for(int id=0;id<cols*rows;id++)if(prev[id]>=0&&(goal<0||point(id).x<point(goal).x||(point(id).x==point(goal).x&&distance[id]<distance[goal])))goal=id;
        exit=point(goal);edge="west-blocked";s.clear(exit,{exit.x-.5,exit.z});blocker=s.last_block;
        std::fprintf(stderr,"No source edge exit: visited%u, edge candidates%zu,Jolt blocked%u,slope rejected%u; reachable source bounds[%.2f,%.2f]..[%.2f,%.2f]; nearest west blocked by body%llu atY%.3f\n",visited,candidates.size(),s.blocked,s.slopes,reached[0]-ox,reached[1]-oz,reached[2]-ox,reached[3]-oz,(unsigned long long)blocker.body,blocker.position.y);
    }
    std::vector<Point> raw;for(int id=goal;;id=prev[id]){raw.push_back(point(id));if(id==start)break;}std::reverse(raw.begin(),raw.end());raw.insert(raw.begin(),spawn);raw.push_back(exit);
    std::vector<Point> smooth={raw.front()};for(size_t i=0;i+1<raw.size();){size_t j=raw.size()-1;while(j>i+1&&!s.clear(raw[i],raw[j]))j--;smooth.push_back(raw[j]);i=j;}
    const unsigned before=s.queries;double meters=0;for(size_t i=1;i<smooth.size();i++){if(!s.clear(smooth[i-1],smooth[i]))return 1;meters+=std::hypot(smooth[i].x-smooth[i-1].x,smooth[i].z-smooth[i-1].z);}
    std::ofstream route(argv[7]);route<<"VALWOOD_CITY_WALK 1\n"<<std::setprecision(14);for(size_t i=0;i<smooth.size();i++)route<<"p"<<i<<" "<<smooth[i].x<<" "<<smooth[i].z<<"\n";
    std::ofstream report(argv[8]);report<<std::setprecision(14)<<"{\n\"passed\":"<<(boundary_reached?"true":"false")<<",\"route_clear\":true,\"boundary_reached\":"<<(boundary_reached?"true":"false")<<",\"kind\":\"Actual Jolt source-UCX exit plus native1.2 slope policy,125mm sweeps\",\"edge\":\""<<edge<<"\",\"blocker\":{\"body\":"<<blocker.body<<",\"position\":["<<blocker.position.x<<","<<blocker.position.y<<","<<blocker.position.z<<"]},\"reachable_source_bounds\":["<<reached[0]-ox<<","<<reached[1]-oz<<","<<reached[2]-ox<<","<<reached[3]-oz<<"],\"visited\":"<<visited<<",\"search_capsule_queries\":"<<s.queries<<",\"validation_queries\":"<<s.queries-before<<",\"query_heap_fallbacks\":"<<s.world.stats().query_allocations<<",\"meters\":"<<meters<<",\"edge_inside_m\":0.1,\"points\":[\n";
    for(size_t i=0;i<smooth.size();i++){const auto p=smooth[i];report<<(i?",\n":"")<<"{\"source_xz\":["<<p.x-ox<<","<<p.z-oz<<"],\"world_xyz\":["<<p.x<<","<<Search::sample(p.x,p.z,&s)<<","<<p.z<<"]}";}
    report<<"\n]}\n";if(!route||!report||s.world.stats().query_allocations)return 1;std::printf("%s %s route: %zu waypoints,%.2fm,visited%u,%u validation sweeps; %s\n",boundary_reached?"PASS":"BLOCKED",edge,smooth.size(),meters,visited,s.queries-before,argv[8]);return boundary_reached?0:1;
}
