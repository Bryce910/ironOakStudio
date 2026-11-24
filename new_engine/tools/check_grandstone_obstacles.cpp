// Actual purchased-source stair and doorway regression. CPU only; same Jolt
// Support movement and capsule dimensions as the running playtest.
#include "engine/world_playtest_walk.h"
#include "engine/world_json_string.h"
#include "engine/world_grandstone_qa_cases.h"
#include <map>
#include <queue>
#include <tuple>
#include <cstdio>
#include <vector>
#include <chrono>
using world_grandstone_qa::Point;
using world_grandstone_qa::Case;
struct Node {int ix=0,iz=0,parent=-1;double x=0,z=0,cost=0;float feet=0;};
static bool plan(world_playtest_walk::Support &s,Point start,Point goal,std::vector<Node> &path,unsigned &examined){
 float floor=0;if(!s.sample(start.x,start.z,start.feet,floor)||std::abs(floor-start.feet)>.7)return false;
 constexpr double grid=.25;std::vector<Node> nodes={{0,0,-1,start.x,start.z,0,floor}};
 using Key=std::tuple<int,int,int>;std::map<Key,double> best;best[{0,0,int(std::lround(floor*100))}]=0;
 using Entry=std::pair<double,unsigned>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> todo;
 todo.push({std::hypot(goal.x-start.x,goal.z-start.z)+std::abs(goal.feet-floor),0});int found=-1;
 while(!todo.empty()&&examined<20000){auto item=todo.top();todo.pop();auto index=item.second;auto a=nodes[index];examined++;
  if(std::hypot(goal.x-a.x,goal.z-a.z)<.38&&std::abs(goal.feet-a.feet)<.30){found=int(index);break;}
  if(std::hypot(a.x-start.x,a.z-start.z)>90)continue;
  for(int iz=-1;iz<=1;iz++)for(int ix=-1;ix<=1;ix++){if(!ix&&!iz)continue;
   int nx=a.ix+ix,nz=a.iz+iz;double x=start.x+nx*grid,z=start.z+nz*grid;float next=0;
   if(!s.can_move(a.x,a.z,x,z,a.feet,next))continue;
   double cost=a.cost+std::hypot(ix*grid,iz*grid)+.05*std::abs(next-a.feet);
   Key key={nx,nz,int(std::lround(next*100))};auto old=best.find(key);if(old!=best.end()&&old->second<=cost)continue;best[key]=cost;
   unsigned n=unsigned(nodes.size());nodes.push_back({nx,nz,int(index),x,z,cost,next});
   todo.push({cost+std::hypot(goal.x-x,goal.z-z)+std::abs(goal.feet-next),n});
  }
 }
 if(found<0)return false;for(int i=found;i>=0;i=nodes[unsigned(i)].parent)path.push_back(nodes[unsigned(i)]);
 std::reverse(path.begin(),path.end());return true;
}
static bool exterior(world_playtest_walk::Support &s,Point start,const std::filesystem::path &file,std::vector<Node> &path,unsigned &examined){
 float feet=0;if(!s.sample(start.x,start.z,start.feet,feet))return false;
 path.push_back({0,0,-1,start.x,start.z,0,feet});unsigned pieces=0;
 for(auto point:world_grandstone_qa::exterior_centers){
  const double x=point.x,z=point.z;
  float floor=0;auto a=path.back();if(!s.sample(x,z,a.feet,floor))return false;
  std::vector<Node> part;unsigned count=0;if(!plan(s,{a.x,a.feet,a.z},{x,floor,z},part,count))return false;
  examined+=count;path.insert(path.end(),part.begin()+1,part.end());pieces++;
 }
 return pieces==67&&path.front().feet-path.back().feet>50;
}
static bool dense(world_playtest_walk::Support &s,const std::vector<Node> &path,bool reverse,unsigned &steps,double &meters,Point &failure){
 float feet=reverse?path.back().feet:path.front().feet;
 for(size_t j=1;j<path.size();j++){
  const auto &a=reverse?path[path.size()-j]:path[j-1],&b=reverse?path[path.size()-j-1]:path[j];
  double length=std::hypot(b.x-a.x,b.z-a.z);unsigned count=std::max(1u,unsigned(std::ceil(length/.05)));
  for(unsigned i=1;i<=count;i++){
   double f=double(i-1)/count,g=double(i)/count,ax=a.x+(b.x-a.x)*f,az=a.z+(b.z-a.z)*f,bx=a.x+(b.x-a.x)*g,bz=a.z+(b.z-a.z)*g;
   float next=0;if(!s.can_move(ax,az,bx,bz,feet,next)){failure={bx,feet,bz};return false;}
   float again=0,twice=0;
   if(!s.sample(bx,bz,next,again)||std::abs(again-next)>.005||!s.sample(bx,bz,again,twice)||std::abs(twice-again)>.005){
    failure={bx,next,bz};std::fprintf(stderr,"Non-idempotent actual source support at %.12f %.9f %.12f: %.9f %.9f\n",bx,next,bz,again,twice);return false;
   }
   feet=next;steps++;
  }meters+=length;
 }return true;
}
int main(int argc,char **argv){
 if(argc!=3&&argc!=4){std::fprintf(stderr,"Usage: check-grandstone-obstacles runtime-directory output-json [case-name]\n");return 2;}
 const auto &cases=world_grandstone_qa::cases;
 std::ofstream out(argv[2]);out<<std::setprecision(15)<<"{\n\"kind\":\"Purchased source stair/doorway capsule traversal; forward and reverse 5cm steps\",\n\"cases\":[\n";
 unsigned failures=0,index=0;std::string last;world_playtest_walk::Support support;
 for(const auto &c:cases){
  if(argc==4&&std::string(argv[3])!=c.name&&std::string(argv[3])!="all-short"&&std::string(argv[3])!="verified")continue;
  if(argc==4&&std::string(argv[3])=="all-short"&&std::string(c.name)=="exterior_complete_descent")continue;
  std::string error;if(last!=world_grandstone_qa::sector_name(c.section)){last=world_grandstone_qa::sector_name(c.section);auto file=std::filesystem::path(argv[1])/(last+".settings");if(!support.load(file.u8string().c_str(),error)){std::fprintf(stderr,"Load %s: %s\n",world_grandstone_qa::sector_name(c.section),error.c_str());return 1;}}
  auto a=c.start,b=c.goal;std::vector<Node> path;unsigned examined=0,steps=0;double length=0;Point failure{};
  auto begin=std::chrono::steady_clock::now();bool planned=false,forward=false,reverse=false;
  if(argc==4&&std::string(argv[3])=="verified"){
   const auto &frozen=world_grandstone_qa::paths[&c-cases];
   for(std::size_t i=0;i<frozen.count;i++){const auto &p=frozen.points[i];path.push_back({0,0,-1,p.x,p.z,0,float(p.feet)});}
   planned=path.size()>=2;
  }else planned=std::string(c.name)=="exterior_complete_descent"?exterior(support,a,std::filesystem::path(argv[1]).parent_path()/"exterior-source-centers.route",path,examined):plan(support,a,b,path,examined);
  if(planned){forward=dense(support,path,false,steps,length,failure);if(forward)reverse=dense(support,path,true,steps,length,failure);}
  bool passed=planned&&forward&&reverse&&!support.stats().query_allocations;failures+=!passed;
  double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
  if(index++)out<<",\n";out<<"{\"name\":"<<world_json::quote(c.name)<<",\"sector\":"<<world_json::quote(world_grandstone_qa::sector_name(c.section))<<",\"passed\":"<<(passed?"true":"false")<<",\"planned\":"<<(planned?"true":"false")<<",\"forward\":"<<(forward?"true":"false")<<",\"reverse\":"<<(reverse?"true":"false")<<",\"examined\":"<<examined<<",\"path_nodes\":"<<path.size()<<",\"dense_steps\":"<<steps<<",\"walked_m_both_directions\":"<<length<<",\"seconds\":"<<seconds<<",\"start_world\":["<<a.x<<','<<a.feet<<','<<a.z<<"],\"target_world\":["<<b.x<<','<<b.feet<<','<<b.z<<"],\"failure_world\":["<<failure.x<<','<<failure.feet<<','<<failure.z<<"],\"query_heap_fallback\":"<<support.stats().query_allocations<<",\"path\":[";
  for(size_t i=0;i<path.size();i++){if(i)out<<',';out<<'['<<path[i].x<<','<<path[i].feet<<','<<path[i].z<<']';}out<<"]}";out.flush();
  std::printf("%s %s: planned%d forward%d reverse%d examined%u steps%u %.3fm %.3fs\n",passed?"PASS":"FAIL",c.name,planned,forward,reverse,examined,steps,length,seconds);std::fflush(stdout);
 }
 out<<"\n],\"closed_gates\":[";
 unsigned gate_index=0;
 for(const auto &gate:world_grandstone_qa::gates){
  std::string error;auto file=std::filesystem::path(argv[1])/(std::string(world_grandstone_qa::sector_name(gate.section))+".settings");
  bool loaded=support.load(file.u8string().c_str(),error);auto a=gate.start,b=gate.goal;world_physics::Hit hit;
  bool blocks=loaded&&support.cast_capsule({a.x,a.feet+.9,a.z},world_playtest_walk::radius,world_playtest_walk::half_height,{b.x-a.x,0,b.z-a.z},hit)&&hit.hit&&hit.fraction<.999&&hit.normal.y<.65;
  float ignored=0;bool bounds=loaded&&!support.sample(support.bounds().xmax+1,support.spawn().z,support.spawn().feet,ignored);
  bool passed=blocks&&bounds&&support.stats().query_allocations==0;failures+=!passed;
  if(gate_index++)out<<',';
  out<<"{\"name\":"<<world_json::quote(gate.name)<<",\"passed\":"<<(passed?"true":"false")<<",\"solid_leaf_blocks_capsule\":"<<(blocks?"true":"false")<<",\"fraction\":"<<hit.fraction<<",\"outside_bounds_rejected\":"<<(bounds?"true":"false")<<",\"query_heap_fallback\":"<<support.stats().query_allocations<<'}';
  std::printf("%s %s: solid leaf blocks%d outsidebounds%d\n",passed?"PASS":"FAIL",gate.name,blocks,bounds);
 }
 out<<"],\"passed\":"<<(failures?"false":"true")<<",\"failures\":"<<failures<<"}\n";return failures?1:0;
}
