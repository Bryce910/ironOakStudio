#include "engine/world_river_network.h"
#include "engine/world_surface_field.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
using namespace world_surface;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static float ground(double,double,void *){return 100;}
static Ribbon river(std::vector<Point> points,float width=8){Ribbon r;r.kind=RibbonKind::River;r.name="Water";r.width=width;r.shoulder=4;r.depth=1.5;r.smooth=false;r.points=std::move(points);return r;}
static std::vector<BakedRibbon> bake(const Data &d){std::vector<BakedRibbon> out;std::string e;for(const auto &r:d.ribbons){BakedRibbon b;assert(bake_ribbon(r,b,e));out.push_back(b);}return out;}
static unsigned coverage(const RiverMesh &mesh,double x,double z,double &height){unsigned count=0;for(unsigned i=0;i+2<mesh.vertices.size();i+=3){const auto &a=mesh.vertices[i],&b=mesh.vertices[i+1],&c=mesh.vertices[i+2];double d=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);if(std::abs(d)<1.e-12)continue;double u=((b.z-c.z)*(x-c.x)+(c.x-b.x)*(z-c.z))/d,v=((c.z-a.z)*(x-c.x)+(a.x-c.x)*(z-c.z))/d,w=1-u-v;if(std::min({u,v,w})>1.e-7){height=u*a.y+v*b.y+w*c.y;++count;}}return count;}
int main(){
 std::string error;Data data;
 data.ribbons={river({{-30,100.06,0},{30,100,0}}),river({{0,100.05,-30},{0,100.03,0},{20,100.01,15}}),river({{-24,100.07,20},{-24,100.05,3},{-8,100.04,3},{-8,100.02,20}},6)};
 RiverNetwork network;CHECK(network.prepare(bake(data),error));RiverMesh mesh;CHECK(bake_river_mesh(network,mesh,error));CHECK(!mesh.vertices.empty()&&!mesh.ranges.empty());
 CHECK(mesh.vertices.size()%3==0&&sizeof(RiverVertex)==28);
 for(const auto &v:mesh.vertices){CHECK(std::isfinite(v.y)&&std::isfinite(v.flow_x)&&std::isfinite(v.flow_z));CHECK(std::hypot(v.flow_x,v.flow_z)<=1.000001);}
 TerrainField field;CHECK(bake_terrain(data,ground,nullptr,field,error));CHECK(bool(field.river_network));
 // Exact surface interpolation at independent points verifies the same
 // triangles queried by movement/terrain are drawn at sloping confluences.
 std::mt19937 random(28317);std::uniform_real_distribution<double> xy(-36,36);unsigned wet=0,dry=0;
 for(unsigned i=0;i<5000;++i){double x=xy(random),z=xy(random),height=0;const unsigned layers=coverage(mesh,x,z,height);CHECK(layers<=1);float water=0,shore=0;bool cpu=river(field,x,z,water,shore);CHECK(cpu==(layers==1));if(cpu){++wet;CHECK(std::abs(water-height)<.00002);CHECK(shore>=0&&shore<=1);}else ++dry;}
 CHECK(wet>700&&dry>2000);
 // The shared channel remains excavated at crossings; tributary shoulders
 // cannot introduce a dry berm inside an already carved main stream.
 for(int z=-30;z<=30;z+=2)for(int x=-34;x<=34;x+=2){const auto s=network.sample(x,z);if(s.inside){CHECK(std::abs(100+terrain_offset(field,x,z)-s.bed)<.00001);CHECK(s.bed<s.height+.151);}}
 auto reordered=data;std::reverse(reordered.ribbons.begin(),reordered.ribbons.end());TerrainField reordered_field;CHECK(bake_terrain(reordered,ground,nullptr,reordered_field,error));
 for(int z=-30;z<=30;z+=2)for(int x=-34;x<=34;x+=2)CHECK(std::abs(terrain_offset(field,x,z)-terrain_offset(reordered_field,x,z))<1.e-6);
 const auto east=network.surface(16.3,.1);CHECK(east.inside&&east.flow[0]>.8&&std::abs(east.flow[1])<.3);
 const auto south=network.surface(.1,-16.3);CHECK(south.inside&&south.flow[1]>.8&&std::abs(south.flow[0])<.3);
 const auto center=network.surface(.3,.2);CHECK(center.inside&&center.height>100.02&&center.height<100.04);
 CHECK(!network.surface(60,60).inside);
 // Negative world/tile seams use the same grid, with no crack or doubled cap.
 data.ribbons={river({{-144,100,-128},{-112,99.96,-128}},4)};CHECK(network.prepare(bake(data),error));CHECK(bake_river_mesh(network,mesh,error));for(double x:{-143.1,-128.1,-127.9,-112.1}){double h=0;CHECK(coverage(mesh,x,-128.21,h)==1);CHECK(std::abs(h-network.surface(x,-128.21).height)<.00002);}
 // Advanced numeric Y edits can reverse the authored point order. Flow must
 // still descend the actual surface, including two sides of a local valley.
 data.ribbons={river({{0,100,0},{32,100.32,0}})};CHECK(network.prepare(bake(data),error));
 const auto ascending=network.surface(16.3,.1);CHECK(ascending.inside&&ascending.flow[0]<-.99&&ascending.grade>.0099);
 CHECK(bake_river_mesh(network,mesh,error));double ascending_height=0;CHECK(coverage(mesh,16.3,.1,ascending_height)==1);CHECK(std::abs(ascending.height-ascending_height)<.00002);
 data.ribbons={river({{0,100.16,0},{16,100,0},{32,100.16,0}})};CHECK(network.prepare(bake(data),error));
 const auto left_fall=network.surface(8.3,.1),right_fall=network.surface(24.3,.1);
 CHECK(left_fall.flow[0]>.99&&right_fall.flow[0]<-.99);CHECK(left_fall.grade>.0099&&right_fall.grade>.0099);
 data.ribbons={river({{0,100,0},{32,100,0}})};CHECK(network.prepare(bake(data),error));CHECK(network.surface(16.3,.1).flow[0]>.99&&network.surface(16.3,.1).grade==0);
 // Real upper document budget: construction stays bounded and supports a full
 // 16-route designer map without requiring overlapping geometry or 4K maps.
 Data maximum;for(unsigned i=0;i<ribbon_limit;++i)maximum.ribbons.push_back(river({{0,100,double(i*64)},{256,99.9,double(i*64)},{512,99.8,double(i*64)},{768,99.7,double(i*64)},{1024,99.6,double(i*64)}},32));
 auto started=std::chrono::steady_clock::now();CHECK(network.prepare(bake(maximum),error));CHECK(bake_river_mesh(network,mesh,error));const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();CHECK(mesh.vertices.size()*sizeof(RiverVertex)<40u*1024*1024);CHECK(seconds<15);
 std::printf("PASS %u checks: 5000 union coverage/no-overlap + CPU/render heights, shared beds/order independence, tangent directions, negative tile seams; maximum 16x1024m width32m: %zu vertices %.2fMiB in %.3fs\n",checks,mesh.vertices.size(),mesh.vertices.size()*sizeof(RiverVertex)/1048576.,seconds);
}
