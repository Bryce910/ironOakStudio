#include "engine/world_source_review_route.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
struct Camera {double x=0,y=0,z=0;float yaw=0,pitch=0,fov=1,vy=0;bool fly=false,grounded=true;};
int main(){
    const auto folder=std::filesystem::temp_directory_path()/"valwood-source-route-test";std::filesystem::create_directories(folder);
    auto path=folder/"test.route";std::ofstream(path)<<"VALWOOD_SOURCE_REVIEW 1 60\nstart 0 1 1 0 10 0 3.124139361 0 1\nmove 2 0 1 2 10 0 -3.124139361 0 1\ncut 0 1 1 50 20 10 0 0 1\n";
    world_source_review::Route r;std::string error;const std::string file=path.string(),report=(folder/"test").string();assert(r.load(file.c_str(),report.c_str(),nullptr,error));
    Camera c;r.begin(c);assert(c.x==0&&c.fly&&!c.grounded);
    r.advance(c,false,100,0);assert(c.x==0&&r.current_point()==0);
    r.advance(c,true,1,10);assert(c.x==0&&!r.pending_checkpoint());
    r.advance(c,true,11,10);assert(r.pending_checkpoint());r.acknowledge_checkpoint(true,false);
    r.advance(c,true,12,10);assert(c.x==1&&r.moving()&&std::abs(std::abs(c.yaw)-3.141592654)<1e-5);r.record(12,c,false,20,15);
    r.advance(c,false,13,10);assert(c.x==1);r.record(13,c,true,1000,0);
    r.advance(c,true,14,10);assert(c.x==2&&r.pending_checkpoint());r.acknowledge_checkpoint(true,false);r.record(14,c,false,20,15);
    assert(r.advance(c,true,15,10));assert(c.x==50&&c.y==20&&c.z==10&&!r.moving());
    r.advance(c,true,16,10);assert(r.pending_checkpoint());r.acknowledge_checkpoint(true,false);
    r.advance(c,true,17,10);assert(r.complete);assert(r.finish());
    std::ifstream data(report+".json");std::string text((std::istreambuf_iterator<char>(data)),{});assert(text.find("\"motion_mean_fps\": 50.000000")!=std::string::npos);assert(text.find("\"distance_m\": 2.000000")!=std::string::npos);assert(text.find("\"captures\": 0")!=std::string::npos);
    for(const std::string line:{"bad 0 0 0 999999 0 0 0 0 1","bad 0 0 0 0 0 0 0 2 1","bad 0 0 0 nan 0 0 0 0 1","../bad 0 0 0 0 0 0 0 0 1"}){
        std::ofstream(path)<<"VALWOOD_SOURCE_REVIEW 1 60\n"<<line<<"\n";world_source_review::Route invalid;assert(!invalid.load(file.c_str(),report.c_str(),nullptr,error));
    }
    std::cout<<"PASS source route bounds, loading/warmup pause, shortest yaw, explicit cuts, checkpoint acknowledgement, motion-only pacing\n";
}
