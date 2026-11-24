// CPU-only source-route planning through the same support/Jolt API as play.
#include "engine/world_playtest_walk.h"
#include "engine/world_playtest_slice.h"
#include "engine/world_json_string.h"
#include <cstdio>
#include <map>
#include <queue>
#include <tuple>
#include <vector>
struct Node{int ix=0,iz=0,parent=-1;double x=0,z=0,g=0;float feet=0;};
struct Resident{unsigned family=0;double x=0,y=0,z=0,yaw=0,scale=0,phase=0,ex=0,ey=0,ez=0;unsigned patrol=0;};
// Mirrors world_stronghold_can_move for the verified static source residents.
// Reject patrol rows rather than silently certifying only one animation time.
static bool residents_load(const char *filename,std::vector<Resident> &out){
    std::ifstream input(filename);if(!input)return false;std::string line;
    while(std::getline(input,line)){
        std::istringstream row(line);row>>std::ws;if(row.peek()=='#'||row.eof())continue;
        Resident r;if(!(row>>r.family>>r.x>>r.y>>r.z>>r.yaw>>r.scale>>r.phase>>r.ex>>r.ey>>r.ez>>r.patrol))return false;
        row>>std::ws;if(!row.eof()&&row.peek()!='#')return false;
        for(double v:{r.x,r.y,r.z,r.yaw,r.scale,r.phase,r.ex,r.ey,r.ez})if(!std::isfinite(v))return false;
        if(r.patrol||r.family>=5||r.scale<.5||r.scale>2||out.size()>=32)return false;
        if(r.family!=1)out.push_back(r);
    }
    return !input.bad()&&!out.empty();
}
static bool residents_clear(const std::vector<Resident> &residents,double ax,double az,double bx,double bz,double feet){
    for(const auto &r:residents){
        const float y=float(r.y),scale=float(r.scale);
        if(feet>y+1.5*scale||feet+1.75<y)continue;
        const double d=std::hypot(bx-r.x,bz-r.z),old=std::hypot(ax-r.x,az-r.z);
        if(d<double(world_playtest_walk::radius)+.4*scale&&d<old-.00001)return false;
    }return true;
}
static bool route(world_playtest_walk::Support &support,const std::vector<Resident> &residents,double gx,double gz,std::vector<Node> &out,unsigned &examined){
    const auto spawn=support.spawn();std::vector<Node> nodes={{0,0,-1,spawn.x,spawn.z,0,float(spawn.feet)}};
    using Key=std::tuple<int,int,int>;std::map<Key,double> best;
    using Entry=std::pair<double,unsigned>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> pending;
    pending.push({std::hypot(gx-spawn.x,gz-spawn.z),0});int found=-1;
    while(!pending.empty()&&examined<12000){
        const auto item=pending.top();pending.pop();const unsigned index=item.second;const auto a=nodes[index];examined++;
        if(std::hypot(gx-a.x,gz-a.z)<.65){found=int(index);break;}
        for(int iz=-1;iz<=1;iz++)for(int ix=-1;ix<=1;ix++){
            if(!ix&&!iz)continue;
            const int nx=a.ix+ix,nz=a.iz+iz;const double x=spawn.x+nx*.5,z=spawn.z+nz*.5;float floor=0;
            if(!support.can_move(a.x,a.z,x,z,a.feet,floor)||!residents_clear(residents,a.x,a.z,x,z,a.feet))continue;
            const double g=a.g+std::hypot(ix*.5,iz*.5);Key key={nx,nz,int(std::lround(floor*10))};
            const auto old=best.find(key);if(old!=best.end()&&old->second<=g)continue;best[key]=g;
            const unsigned next=unsigned(nodes.size());nodes.push_back({nx,nz,int(index),x,z,g,floor});
            pending.push({g+std::hypot(gx-x,gz-z),next});
        }
    }
    if(found<0)return false;
    for(int index=found;index>=0;index=nodes[unsigned(index)].parent)out.push_back(nodes[unsigned(index)]);
    std::reverse(out.begin(),out.end());return true;
}
int main(int argc,char **argv){
    if(argc!=4&&argc!=5){std::fprintf(stderr,"Usage: check-grandstone-playtest sector.settings output.route output.json [showcase.residents]\n");return 2;}
    world_playtest_walk::Support support;std::string error;
    if(!support.load(argv[1],error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    const char *residents_path=argc==5?argv[4]:"projects/valwood_showcase/showcase.residents";
    std::vector<Resident> residents;if(!residents_load(residents_path,residents)){std::fprintf(stderr,"Static source residents rejected: %s\n",residents_path);return 1;}
    const bool hall=std::filesystem::path(argv[1]).stem()=="hall";
    const double ox=-43844,oz=-23487.82415651827;
    // Stop before the actual hall guard, leaving its native body collision intact.
    const double gx=ox+(hall?239.:367.2),gz=hall?support.spawn().z-31.5:oz-535.5;
    unsigned examined=0;std::vector<Node> path;
    if(!route(support,residents,gx,gz,path,examined)){std::fprintf(stderr,"No supported source route after%u searches\n",examined);return 1;}
    // Compress only when the entire replacement segment passes real support
    // and capsule checks; retain connectors around stairs and source columns.
    std::vector<Node> compressed;size_t at=0;
    while(at<path.size()){
        compressed.push_back(path[at]);if(at+1==path.size())break;
        size_t next=at+1;
        for(size_t j=next;j<path.size()&&j<at+13;j++){
            float floor=0;if(support.can_move(path[at].x,path[at].z,path[j].x,path[j].z,path[at].feet,floor)&&residents_clear(residents,path[at].x,path[at].z,path[j].x,path[j].z,path[at].feet)&&std::abs(floor-path[j].feet)<.04)next=j;
        }
        at=next;
    }
    unsigned sweeps=0;double length=0,min_resident_margin=INFINITY;float feet=float(support.spawn().feet);
    for(size_t i=1;i<compressed.size();i++){
        const auto &a=compressed[i-1],&b=compressed[i];const double distance=std::hypot(b.x-a.x,b.z-a.z);length+=distance;
        const unsigned count=std::max(1u,unsigned(std::ceil(distance/.05)));
        for(unsigned k=1;k<=count;k++){
            float floor=0;const double f=double(k-1)/count,g=double(k)/count;
            const double ax=a.x+(b.x-a.x)*f,az=a.z+(b.z-a.z)*f,bx=a.x+(b.x-a.x)*g,bz=a.z+(b.z-a.z)*g;
            if(!support.can_move(ax,az,bx,bz,feet,floor)||!residents_clear(residents,ax,az,bx,bz,feet)){
                std::fprintf(stderr,"Dense route failed on connector%zu sample%u\n",i,k);return 1;
            }
            for(const auto &r:residents)if(feet<=float(r.y)+1.5*float(r.scale)&&feet+1.75>=float(r.y))
                min_resident_margin=std::min(min_resident_margin,std::hypot(bx-r.x,bz-r.z)-double(world_playtest_walk::radius)-.4*float(r.scale));
            feet=floor;sweeps++;
        }
    }
    bool old_resident_blocked=false,old_support_clear=false;
    if(hall){
        const auto &s=support.spawn();float next=0;
        old_resident_blocked=!residents_clear(residents,s.x,s.z-32.,s.x,s.z-32.05,s.feet);
        old_support_clear=support.can_move(s.x,s.z-32.,s.x,s.z-32.05,s.feet,next);
        if(!old_resident_blocked||!old_support_clear){std::fprintf(stderr,"Original native guard-collision regression was not reproduced\n");return 1;}
    }
    world_physics::Hit hit;const double fx=ox+(hall?240.:367.5),fz=oz-(hall?622.:535.5);
    float floor=0;const double reference=1130.8125+(hall?152.165:83.139);
    if(!support.sample(fx,fz,reference,floor)||!support.cast_capsule({fx,floor+.9,fz},.37f,.50f,{hall?6.:2.5,0,0},hit)||!hit.hit||hit.normal.y>=.65||hit.fraction>=.999){
        std::fprintf(stderr,"Source door/column negative capsule check failed\n");return 1;
    }
    float ignored=0;const bool bounds_reject=!support.sample(support.bounds().xmin-.5,support.spawn().z,support.spawn().feet,ignored);
    if(!bounds_reject)return 1;
    bool void_reject=false;double void_x=0,void_z=0;
    for(double z=support.bounds().zmin+1;z<support.bounds().zmax-1&&!void_reject;z+=1)
        for(double x=support.bounds().xmin+1;x<support.bounds().xmax-1;x+=1){
            world_physics::Hit probe;
            if(!support.sample(x,z,support.spawn().feet,ignored)&&
               support.raycast({x,support.spawn().feet+.61,z},{0,-3.61,0},probe)&&!probe.hit){
                void_reject=true;void_x=x;void_z=z;break;
            }
        }
    if(!void_reject){std::fprintf(stderr,"No actual source void negative was certified\n");return 1;}
    std::ofstream output(argv[2]);output<<std::setprecision(15)<<"VALWOOD_PLAYTEST 1\nname "<<std::quoted(hall?"Grandstone | First Temple hall":"Grandstone | Snowbound gate approach")<<"\nspawn "<<support.spawn().x<<' '<<support.spawn().feet+1.75<<' '<<support.spawn().z<<' '<<support.spawn().yaw<<' '<<support.spawn().pitch<<"\ncount "<<compressed.size()<<'\n';
    for(size_t i=0;i<compressed.size();i++){
        const bool checkpoint=i==0||i+1==compressed.size()||i==compressed.size()/2;
        const std::string label=i==0?(hall?"Temple entrance":"Snowbound approach"):i+1==compressed.size()?(hall?"Hall guard overlook":"Sealed gate | Enter hall"):(hall?"Pillar aisle":"Barbican gate");
        output<<"path "<<std::quoted(label)<<' '<<compressed[i].x<<' '<<compressed[i].feet<<' '<<compressed[i].z<<" .85"<<(checkpoint?" checkpoint":"")<<'\n';
    }
    output.close();if(!output)return 1;
    world_playtest_slice::Path decoded;
    if(!world_playtest_slice::load(argv[2],decoded,error)){std::fprintf(stderr,"Route codec: %s\n",error.c_str());return 1;}
    const auto stats=support.stats();if(stats.query_allocations)return 1;
    std::ofstream report(argv[3]);report<<std::setprecision(15)<<"{\"passed\":true,\"settings\":"<<world_json::quote(argv[1])<<",\"kind\":\"Actual source Jolt and reference-storey capsule route plus native static resident predicate\",\"residents\":"<<world_json::quote(residents_path)<<",\"static_residents\":"<<residents.size()<<",\"route_nodes\":"<<compressed.size()<<",\"astar_nodes\":"<<examined<<",\"dense_queries\":"<<sweeps<<",\"walked_m\":"<<length<<",\"resident_margin_m\":"<<(std::isfinite(min_resident_margin)?min_resident_margin:0)<<",\"old_hall_route_resident_blocked\":"<<(old_resident_blocked?"true":"false")<<",\"old_hall_source_support_clear\":"<<(old_support_clear?"true":"false")<<",\"source_obstacle_blocked\":true,\"obstacle_fraction\":"<<hit.fraction<<",\"outside_bounds_rejected\":true,\"actual_source_void_rejected\":true,\"source_void_world_xz\":["<<void_x<<','<<void_z<<"],\"query_heap_fallback\":"<<stats.query_allocations<<"}\n";
    std::printf("PASS %s: %.3fm, %zu nodes, %u dense steps; actual source obstacle blocks, bounds reject, query heap0\n",hall?"hall":"approach",length,compressed.size(),sweeps);
}
