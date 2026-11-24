#ifndef VALWOOD_WORLD_SOURCE_REVIEW_ROUTE_H
#define VALWOOD_WORLD_SOURCE_REVIEW_ROUTE_H
// Explicit, bounded visual QA replay. It neither navigates nor changes saves.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace world_source_review {
struct Point {
    std::string label;
    unsigned transition=0,hold=0,capture=0;
    double x=0,y=0,z=0,yaw=0,pitch=0,fov=1.0471975512;
};
class Route {
    std::vector<Point> points_;
    std::string source_,report_,prefix_;
    FILE *csv_=nullptr;
    std::size_t current_=0;
    unsigned progress_=0,hold_=0;
    bool checkpoint_=false,finished_=false;
    std::uint64_t frames_=0,motion_=0,holds_=0,waiting_=0,cuts_=0,checkpoints_=0,captures_=0,failures_=0;
    double distance_=0,wall_sum_=0,wall_max_=0;
    std::vector<double> motion_ms_;
    bool moving_frame_=false,cut_frame_=false,captured_frame_=false;
    static std::string quote(const std::string &s){std::string out="\"";for(char c:s){if(c=='\\'||c=='\"')out+='\\';if(c=='\n')out+="\\n";else out+=c;}return out+'\"';}
    template<class Camera> static void apply(Camera &c,const Point &p){c.x=p.x;c.y=p.y;c.z=p.z;c.yaw=float(p.yaw);c.pitch=float(p.pitch);c.fov=float(p.fov);c.fly=true;c.grounded=false;c.vy=0;}
    void arrive(){hold_=points_[current_].hold;if(!hold_&&points_[current_].capture)checkpoint_=true;}
public:
    bool active=false,complete=false;
    ~Route(){if(csv_)std::fclose(csv_);}
    const Point &first()const{return points_.front();}
    const char *label()const{return points_[current_].label.c_str();}
    std::size_t point_count()const{return points_.size();}
    std::size_t current_point()const{return current_;}
    bool pending_checkpoint()const{return checkpoint_;}
    bool moving()const{return moving_frame_;}
    unsigned requested_frames()const{unsigned n=unsigned(points_.size());for(const auto &p:points_)n+=p.transition+p.hold;return n;}
    bool load(const char *path,const char *report,const char *capture_prefix,std::string &error){
        if(active){error="Route already loaded";return false;}
        std::ifstream input(path?path:"");if(!input){error="Cannot open source review route";return false;}
        source_=path;report_=report?report:"";prefix_=capture_prefix?capture_prefix:"";
        if(source_.size()>1200||report_.empty()||report_.size()>1200||prefix_.size()>1100){error="Invalid source review output path";return false;}
        std::string line;unsigned line_number=0,total=0;bool header=false;
        while(std::getline(input,line)){
            ++line_number;if(line.size()>512||line_number>16384){error="Source review route exceeds bounds";return false;}
            if(line.empty()||line[0]=='#')continue;
            std::istringstream row(line);std::string trailing;
            if(!header){std::string magic;int version=0,hz=0;if(!(row>>magic>>version>>hz)||magic!="VALWOOD_SOURCE_REVIEW"||version!=1||hz!=60||(row>>trailing)){error="Invalid source review header";return false;}header=true;continue;}
            Point p;
            if(!(row>>p.label>>p.transition>>p.hold>>p.capture>>p.x>>p.y>>p.z>>p.yaw>>p.pitch>>p.fov)||(row>>trailing)){error="Malformed source review point at line "+std::to_string(line_number);return false;}
            bool label_ok=!p.label.empty()&&p.label.size()<=80;for(char c:p.label)label_ok=label_ok&&((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-');
            const double values[]={p.x,p.y,p.z,p.yaw,p.pitch,p.fov};bool finite=true;for(double v:values)finite=finite&&std::isfinite(v);
            if(!label_ok||!finite||std::abs(p.x)>50400||std::abs(p.z)>28400||p.y< -2000||p.y>12000||std::abs(p.yaw)>100000||std::abs(p.pitch)>1.5||p.fov<.2||p.fov>2.8||p.transition>1200||p.hold>600||p.capture>1||points_.size()>=8192){error="Source review point outside bounds at line "+std::to_string(line_number);return false;}
            total+=p.transition+p.hold+1;if(total>216000){error="Source review route exceeds one hour";return false;}
            points_.push_back(p);
        }
        if(!header||points_.empty()||points_.front().transition){error="Source review needs an initial cut point";return false;}
        csv_=std::fopen((report_+".frames.csv").c_str(),"w");if(!csv_){error="Cannot write source review frame report";return false;}
        std::fprintf(csv_,"frame,phase,point,label,x,y,z,yaw,pitch,frame_ms,gpu_ms,capture_readback\n");
        active=true;arrive();return true;
    }
    template<class Camera> void begin(Camera &camera){apply(camera,points_.front());}
    // Return true only for a deliberate segment cut; the caller must use its
    // ordinary destination-loading/probe path before allowing another tick.
    template<class Camera> bool advance(Camera &camera,bool ready,std::uint64_t frame,unsigned warmup){
        moving_frame_=cut_frame_=captured_frame_=false;
        if(!active||complete||!ready||frame<=warmup)return false;
        if(checkpoint_)return false;
        if(hold_){--hold_;if(!hold_&&points_[current_].capture)checkpoint_=true;return false;}
        if(current_+1>=points_.size()){complete=true;return false;}
        const auto &a=points_[current_];const auto &b=points_[current_+1];
        if(!b.transition){++current_;apply(camera,b);arrive();cut_frame_=true;++cuts_;return true;}
        const double oldx=camera.x,oldy=camera.y,oldz=camera.z;
        const double t=double(++progress_)/b.transition;
        Point p=b;p.x=a.x+(b.x-a.x)*t;p.y=a.y+(b.y-a.y)*t;p.z=a.z+(b.z-a.z)*t;
        p.yaw=a.yaw+std::remainder(b.yaw-a.yaw,6.283185307179586)*t;p.pitch=a.pitch+(b.pitch-a.pitch)*t;p.fov=a.fov+(b.fov-a.fov)*t;apply(camera,p);
        const double step=std::sqrt((camera.x-oldx)*(camera.x-oldx)+(camera.y-oldy)*(camera.y-oldy)+(camera.z-oldz)*(camera.z-oldz));
        moving_frame_=step>1e-8||std::abs(b.yaw-a.yaw)>1e-8||std::abs(b.pitch-a.pitch)>1e-8;distance_+=step;
        if(progress_==b.transition){++current_;progress_=0;arrive();}
        return false;
    }
    std::string capture_path()const{if(!checkpoint_||prefix_.empty())return {};return prefix_+"-"+std::to_string(current_)+"-"+points_[current_].label+".bmp";}
    void acknowledge_checkpoint(bool ok,bool readback){if(!checkpoint_)return;++checkpoints_;if(readback){++captures_;captured_frame_=true;}if(!ok)++failures_;checkpoint_=false;}
    template<class Camera> void record(std::uint64_t frame,const Camera &camera,bool loading,double frame_ms,double gpu_ms){
        if(!active)return;
        const char *phase=loading?"loading":cut_frame_?"cut":moving_frame_?"motion":complete?"done":"hold";
        ++frames_;if(loading)++waiting_;else if(moving_frame_){++motion_;motion_ms_.push_back(frame_ms);wall_sum_+=frame_ms;wall_max_=std::max(wall_max_,frame_ms);}else if(!cut_frame_&&!complete)++holds_;
        if(csv_)std::fprintf(csv_,"%llu,%s,%zu,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.6f,%.6f,%d\n",(unsigned long long)frame,phase,current_,label(),camera.x,camera.y,camera.z,double(camera.yaw),double(camera.pitch),frame_ms,gpu_ms,captured_frame_?1:0);
    }
    bool finish(){
        if(!active||finished_)return true;
        finished_=true;if(csv_){std::fclose(csv_);csv_=nullptr;}
        std::sort(motion_ms_.begin(),motion_ms_.end());const std::size_t tail=std::max<std::size_t>(1,(motion_ms_.size()+99)/100);
        double worst=0;if(!motion_ms_.empty())for(std::size_t i=motion_ms_.size()-std::min(tail,motion_ms_.size());i<motion_ms_.size();++i)worst+=motion_ms_[i];
        FILE *out=std::fopen((report_+".json").c_str(),"w");if(!out)return false;
        std::fprintf(out,"{\n  \"kind\": \"deterministic 60Hz fly-through visual review; no walking collision claim\",\n  \"route\": %s,\n  \"complete\": %s,\n  \"points\": %zu,\n  \"last_point\": %zu,\n  \"frames\": %llu,\n  \"motion_frames\": %llu,\n  \"hold_frames\": %llu,\n  \"loading_or_warmup_frames\": %llu,\n  \"cuts\": %llu,\n  \"checkpoints\": %llu,\n  \"captures\": %llu,\n  \"failures\": %llu,\n  \"distance_m\": %.6f,\n  \"motion_mean_fps\": %.6f,\n  \"motion_one_percent_low_fps\": %.6f,\n  \"motion_max_ms\": %.6f,\n  \"capture_readbacks_contaminate_timing\": %s\n}\n",quote(source_).c_str(),complete?"true":"false",points_.size(),current_,(unsigned long long)frames_,(unsigned long long)motion_,(unsigned long long)holds_,(unsigned long long)waiting_,(unsigned long long)cuts_,(unsigned long long)checkpoints_,(unsigned long long)captures_,(unsigned long long)failures_,distance_,wall_sum_>0?1000.*motion_/wall_sum_:0.,worst>0?1000.*std::min(tail,motion_ms_.size())/worst:0.,wall_max_,captures_?"true":"false");
        return std::fclose(out)==0;
    }
};
}
#endif
