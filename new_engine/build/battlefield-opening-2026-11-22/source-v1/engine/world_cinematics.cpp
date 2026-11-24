#include "engine/world_platform.h"
#include "engine/world_cinematics.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <limits>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace world_cinematics {
namespace {
constexpr double pi=3.14159265358979323846;
bool range(double value,double lo,double hi){return std::isfinite(value)&&value>=lo&&value<=hi;}
bool vector_ok(Vec3 p){return range(p.x,-1e8,1e8)&&range(p.y,-1e8,1e8)&&range(p.z,-1e8,1e8);}
bool title(const std::string &s){return !s.empty()&&s.size()<=95&&s.find_first_of("\r\n\t") == std::string::npos&&s.find('\0')==std::string::npos;}
bool asset_path(const std::string &s){
    if(s.empty()||s.size()>239||s[0]=='/'||s.find_first_of("\\:\r\n\t")!=std::string::npos||s.find('\0')!=std::string::npos)return false;
    size_t begin=0;while(begin<s.size()){
        auto end=s.find('/',begin);if(end==std::string::npos)end=s.size();
        auto part=s.substr(begin,end-begin);if(part.empty()||part[0]=='.')return false;
        begin=end+1;
    }return s.back()!='/';
}
double qlength(Quaternion q){return std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);}
Quaternion normal(Quaternion q){double d=qlength(q);if(!std::isfinite(d)||d<1e-12)return {};return {q.x/d,q.y/d,q.z/d,q.w/d};}
Quaternion multiply(Quaternion a,Quaternion b){return {
    a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
    a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
    a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
    a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
Vec3 rotate(Quaternion q,Vec3 v){q=normal(q);auto r=multiply(multiply(q,{v.x,v.y,v.z,0}),{-q.x,-q.y,-q.z,q.w});return {r.x,r.y,r.z};}
Quaternion slerp(Quaternion a,Quaternion b,double t){
    a=normal(a);b=normal(b);double d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(d<0){d=-d;b={-b.x,-b.y,-b.z,-b.w};}
    double aa=1-t,bb=t;
    if(d<.9995){double theta=std::acos(std::clamp(d,-1.,1.)),s=std::sin(theta);aa=std::sin((1-t)*theta)/s;bb=std::sin(t*theta)/s;}
    return normal({aa*a.x+bb*b.x,aa*a.y+bb*b.y,aa*a.z+bb*b.z,aa*a.w+bb*b.w});
}
double curve(Curve kind,double t){t=std::clamp(t,0.,1.);return kind==Curve::Hold?0:kind==Curve::Smooth?t*t*(3-2*t):t;}
double clock_time(const Document &d,double t){if(t<0)return 0;if(d.loop&&d.duration>0)return std::fmod(t,d.duration);return std::clamp(t,0.,d.duration);}
bool valid_curve(Curve c){return unsigned(c)<=unsigned(Curve::Hold);}
float clip_gain(const Clip &c,double t){
    const double age=t-c.start,left=c.duration-age;
    double in=c.fade_in>0?std::sin(std::clamp(age/c.fade_in,0.,1.)*pi*.5):1;
    double out=c.fade_out>0?std::sin(std::clamp(left/c.fade_out,0.,1.)*pi*.5):1;
    return float(c.gain*in*out);
}
bool active(const Clip &c,double time){return time>=c.start&&time<c.start+c.duration;}
double distance(const Zone &z,Vec3 p){
    double x=std::abs(p.x-z.center.x),y=std::abs(p.y-z.center.y),v=std::abs(p.z-z.center.z);
    if(z.shape==Shape::Sphere)return std::hypot(std::hypot(x,y),v)-z.extent.x;
    x-=z.extent.x;y-=z.extent.y;v-=z.extent.z;
    return std::hypot(std::hypot(std::max(x,0.),std::max(y,0.)),std::max(v,0.))+std::min(std::max({x,y,v}),0.);
}
}

Quaternion orientation(double yaw,double pitch,double roll){
    if(!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(roll))return {};
    return normal(multiply(multiply({0,-std::sin(yaw*.5),0,std::cos(yaw*.5)},
        {std::sin(pitch*.5),0,0,std::cos(pitch*.5)}),{0,0,-std::sin(roll*.5),std::cos(roll*.5)}));
}
Vec3 forward(Quaternion q){return rotate(q,{0,0,-1});}
Vec3 up(Quaternion q){return rotate(q,{0,1,0});}

bool validate(const Document &d,std::string &error){
    error.clear();auto fail=[&](const char *text){error=text;return false;};
    if(!title(d.name)||!range(d.duration,.001,86400))return fail("Invalid cinematic name/duration");
    if(d.camera.size()>camera_limit||d.zones.size()>zone_limit||d.clips.size()>clip_limit||d.envelopes.size()>bus_limit)return fail("Cinematic document budget exceeded");
    double previous=-1;
    for(const auto &key:d.camera){
        const auto &p=key.pose;const auto &q=p.rotation;
        if(!range(key.time,0,d.duration)||key.time<=previous||!vector_ok(p.position)||!range(p.fov_degrees,5,150)||!valid_curve(key.curve)||
            !range(q.x,-1,1)||!range(q.y,-1,1)||!range(q.z,-1,1)||!range(q.w,-1,1)||std::abs(qlength(q)-1)>.01)return fail("Invalid camera key/time/unit quaternion");
        previous=key.time;
    }
    for(size_t i=0;i<d.zones.size();i++){
        const auto &z=d.zones[i];
        if(!z.id||z.bus>=bus_limit||!title(z.name)||(!z.asset.empty()&&!asset_path(z.asset))||unsigned(z.shape)>unsigned(Shape::Box)||
            z.priority< -1000||z.priority>1000||!vector_ok(z.center)||!range(z.extent.x,.01,100000)||!range(z.extent.y,.01,100000)||!range(z.extent.z,.01,100000)||
            !range(z.blend,.01,10000)||!range(z.gain,0,2)||!range(z.dry,0,1)||!range(z.reverb,0,1))return fail("Invalid audio zone");
        for(size_t j=0;j<i;j++)if(d.zones[j].id==z.id)return fail("Duplicate audio zone ID");
    }
    for(size_t i=0;i<d.clips.size();i++){
        const auto &c=d.clips[i];
        if(!c.id||c.bus>=bus_limit||!asset_path(c.asset)||!range(c.start,0,d.duration)||!range(c.duration,.001,d.duration)||
            c.start+c.duration>d.duration+1e-9||!range(c.source_offset,0,86400)||!range(c.gain,0,2)||!range(c.fade_in,0,c.duration)||!range(c.fade_out,0,c.duration))return fail("Invalid clip/cue bounds");
        for(size_t j=0;j<i;j++)if(d.clips[j].id==c.id)return fail("Duplicate clip ID");
    }
    for(size_t i=0;i<d.envelopes.size();i++){
        const auto &e=d.envelopes[i];if(e.bus>=bus_limit||e.keys.empty()||e.keys.size()>envelope_key_limit)return fail("Invalid mix bus envelope");
        for(size_t j=0;j<i;j++)if(d.envelopes[j].bus==e.bus)return fail("Duplicate bus envelope");
        previous=-1;for(const auto &key:e.keys){if(!range(key.time,0,d.duration)||key.time<=previous||!range(key.gain,0,2)||!valid_curve(key.curve))return fail("Invalid bus gain key");previous=key.time;}
    }
    return true;
}

bool sample_camera(const Document &d,double time,CameraPose &out){
    if(d.camera.empty()||d.camera.size()>camera_limit||!std::isfinite(time)||!(d.duration>0))return false;
    time=clock_time(d,time);const auto &first=d.camera.front(),&last=d.camera.back();
    if(time<=first.time){out=first.pose;out.rotation=normal(out.rotation);return true;}
    if(time>=last.time){out=last.pose;out.rotation=normal(out.rotation);return true;}
    auto upper=std::upper_bound(d.camera.begin(),d.camera.end(),time,[](double t,const CameraKey &key){return t<key.time;});
    if(upper==d.camera.begin()||upper==d.camera.end())return false;
    const auto &a=upper[-1],&b=*upper;const double span=b.time-a.time;if(!(span>0))return false;
    const double t=curve(a.curve,(time-a.time)/span);CameraPose pose;
    pose.position={a.pose.position.x+(b.pose.position.x-a.pose.position.x)*t,a.pose.position.y+(b.pose.position.y-a.pose.position.y)*t,a.pose.position.z+(b.pose.position.z-a.pose.position.z)*t};
    pose.rotation=slerp(a.pose.rotation,b.pose.rotation,t);pose.fov_degrees=a.pose.fov_degrees+(b.pose.fov_degrees-a.pose.fov_degrees)*t;
    out=pose;return true;
}

float sample_bus(const Document &d,unsigned bus,double time){
    if(bus>=bus_limit||!std::isfinite(time)||!(d.duration>0))return 0;
    time=clock_time(d,time);
    for(const auto &e:d.envelopes)if(e.bus==bus&&!e.keys.empty()){
        if(time<=e.keys.front().time)return e.keys.front().gain;
        if(time>=e.keys.back().time)return e.keys.back().gain;
        auto b=std::upper_bound(e.keys.begin(),e.keys.end(),time,[](double t,const GainKey &key){return t<key.time;});
        const auto &a=b[-1];double t=curve(a.curve,(time-a.time)/(b->time-a.time));return float(a.gain+(b->gain-a.gain)*t);
    }return 1;
}

bool sample_zones(const Document &d,Vec3 listener,double time,ZoneMix &out){
    if(!vector_ok(listener)||!std::isfinite(time)||!(d.duration>0)||d.zones.size()>zone_limit)return false;
    ZoneMix next;std::array<unsigned,zone_limit> order={};for(unsigned i=0;i<d.zones.size();i++)order[i]=i;
    std::sort(order.begin(),order.begin()+d.zones.size(),[&](unsigned a,unsigned b){return d.zones[a].priority!=d.zones[b].priority?d.zones[a].priority>d.zones[b].priority:d.zones[a].id<d.zones[b].id;});
    std::array<double,bus_limit> dry={},wet={};double remaining=1;
    for(unsigned begin=0;begin<d.zones.size();){
        unsigned end=begin+1;while(end<d.zones.size()&&d.zones[order[end]].priority==d.zones[order[begin]].priority)end++;
        std::array<double,zone_limit> power={};double sum=0;
        for(unsigned j=begin;j<end;j++){const auto &z=d.zones[order[j]];if(z.bus>=bus_limit||!(z.blend>0))return false;double t=std::clamp(.5-.5*distance(z,listener)/z.blend,0.,1.);double w=std::sin(t*pi*.5);sum+=(power[j]=w*w);}
        if(sum>0){const double group=std::min(sum,1.),factor=remaining*group/sum;
            for(unsigned j=begin;j<end;j++)if(power[j]>0){const auto &z=d.zones[order[j]];auto &voice=next.voices[next.count++];voice.id=z.id;voice.bus=z.bus;voice.asset=z.asset.c_str();voice.weight=float(std::sqrt(power[j]*factor));voice.gain=voice.weight*z.gain*sample_bus(d,z.bus,time);voice.dry_gain=voice.gain*z.dry;voice.reverb_send=voice.gain*z.reverb;dry[z.bus]+=double(voice.dry_gain)*voice.dry_gain;wet[z.bus]+=double(voice.reverb_send)*voice.reverb_send;}
            remaining=std::max(0.,remaining*(1-group));
        }begin=end;
    }
    for(unsigned bus=0;bus<bus_limit;bus++)next.buses[bus]={float(std::sqrt(dry[bus])),float(std::sqrt(wet[bus]))};
    next.outside_gain=float(std::sqrt(remaining));out=next;return true;
}

bool sample_mix(const Document &d,Vec3 listener,double time,Mix &out){
    if(d.clips.size()>clip_limit||!std::isfinite(time)||!(d.duration>0))return false;
    Mix next;if(!sample_zones(d,listener,time,next.zones))return false;time=clock_time(d,time);
    for(unsigned i=0;i<bus_limit;i++)next.buses[i]=sample_bus(d,i,time);
    for(const auto &c:d.clips)if(active(c,time)){
        if(c.bus>=bus_limit)return false;
        next.clips[next.clip_count++]={c.id,c.bus,c.asset.c_str(),c.source_offset+time-c.start,clip_gain(c,time)*next.buses[c.bus],c.loop};
    }out=next;return true;
}

bool Transport::configure(double duration,bool loop){
    if(!range(duration,.001,86400))return false;
    duration_=duration;loop_=loop;time_=last_clock_=0;cycle_=0;paused_=seen_clock_=anchor_=false;jumped_=true;return true;
}
bool Transport::seek(double time){
    if(!(duration_>0)||!std::isfinite(time))return false;
    time_=std::clamp(time,0.,duration_);if(loop_&&time_==duration_)time_=0;cycle_=0;anchor_=false;jumped_=true;return true;
}
void Transport::pause(bool paused){if(paused_!=paused){paused_=paused;anchor_=false;jumped_=true;}}
void Transport::looping(bool loop){if(loop_!=loop){loop_=loop;if(loop_&&time_==duration_)time_=0;anchor_=false;jumped_=true;}}
bool Transport::advance(double now,Step &out){
    if(!(duration_>0)||!std::isfinite(now)||now<0||(seen_clock_&&now<last_clock_))return false;
    Step next;next.previous=time_;next.time=time_;next.previous_cycle=next.cycle=cycle_;next.paused=paused_;next.jumped=jumped_;
    const double delta=anchor_&&!paused_?now-last_clock_:0;
    if(loop_&&delta>0){long double total=static_cast<long double>(time_)+delta;long double turns=std::floor(total/duration_);
        if(turns>static_cast<long double>(std::numeric_limits<std::uint64_t>::max()-cycle_))return false;
        next.wraps=std::uint64_t(turns);next.cycle+=next.wraps;next.time=double(std::fmod(total,static_cast<long double>(duration_)));next.elapsed=delta;
    }else if(!loop_&&delta>0){next.time=std::min(duration_,time_+delta);next.elapsed=next.time-time_;}
    next.finished=!loop_&&next.time>=duration_;time_=next.time;cycle_=next.cycle;last_clock_=now;seen_clock_=anchor_=true;jumped_=false;out=next;return true;
}

bool schedule(const Document &d,const Step &step,Schedule &out){
    if(d.clips.size()>clip_limit||!range(step.time,0,d.duration)||!range(step.previous,0,d.duration)||
        step.cycle<step.previous_cycle||step.wraps!=step.cycle-step.previous_cycle||(!step.jumped&&!step.wraps&&step.time<step.previous))return false;
    Schedule next;next.resync=step.jumped||step.wraps>1;
    auto start=[&](const Clip &c,double at,double offset,std::uint64_t cycle,bool resume){
        if(next.count==event_limit)return false;
        next.events[next.count++]={EventKind::Start,c.id,c.bus,c.asset.c_str(),at,c.source_offset+(resume?at-c.start:0),offset,cycle,clip_gain(c,at)*sample_bus(d,c.bus,at),c.loop};return true;
    };
    if(next.resync){
        next.events[next.count++]={};
        if(!step.paused)for(const auto &c:d.clips)if(active(c,step.time))if(!start(c,step.time,0,step.cycle,true))return false;
    }else if(!step.paused){
        auto segment=[&](double begin,double end,double offset,std::uint64_t cycle,bool include_begin){
            for(const auto &c:d.clips){
                if((c.start>begin||(include_begin&&c.start==begin))&&c.start<=end)if(!start(c,c.start,offset+c.start-begin,cycle,false))return false;
                const double stop=c.start+c.duration;
                if(stop>begin&&stop<=end){if(next.count==event_limit)return false;next.events[next.count++]={EventKind::Stop,c.id,c.bus,c.asset.c_str(),stop,0,offset+stop-begin,cycle,0,c.loop};}
            }return true;
        };
        if(step.wraps){if(!segment(step.previous,d.duration,0,step.previous_cycle,false)||!segment(0,step.time,d.duration-step.previous,step.cycle,true))return false;}
        else if(!segment(step.previous,step.time,0,step.cycle,false))return false;
        std::sort(next.events.begin(),next.events.begin()+next.count,[](const Event &a,const Event &b){if(a.step_offset!=b.step_offset)return a.step_offset<b.step_offset;if(a.kind!=b.kind)return unsigned(a.kind)<unsigned(b.kind);return a.id<b.id;});
    }out=next;return true;
}

bool save(const char *path,const Document &d,std::string &error){
    if(!path||!*path){error="Choose a cinematic file";return false;}if(!validate(d,error))return false;
    struct stat old={};if(lstat(path,&old)==0&&!S_ISREG(old.st_mode)){error="Cinematic destination must be a regular file";return false;}
    std::ostringstream text;text<<std::setprecision(17)<<"VALWOOD_CINEMATICS 1\n"<<"timeline "<<std::quoted(d.name)<<' '<<d.duration<<' '<<d.loop<<'\n';
    for(const auto &k:d.camera){const auto &p=k.pose;const auto &q=p.rotation;text<<"camera "<<k.time<<' '<<p.position.x<<' '<<p.position.y<<' '<<p.position.z<<' '<<q.x<<' '<<q.y<<' '<<q.z<<' '<<q.w<<' '<<p.fov_degrees<<' '<<unsigned(k.curve)<<'\n';}
    for(const auto &z:d.zones)text<<"zone "<<z.id<<' '<<std::quoted(z.name)<<' '<<std::quoted(z.asset)<<' '<<z.bus<<' '<<unsigned(z.shape)<<' '<<z.priority<<' '<<z.center.x<<' '<<z.center.y<<' '<<z.center.z<<' '<<z.extent.x<<' '<<z.extent.y<<' '<<z.extent.z<<' '<<z.blend<<' '<<z.gain<<' '<<z.dry<<' '<<z.reverb<<'\n';
    for(const auto &c:d.clips)text<<"clip "<<c.id<<' '<<std::quoted(c.asset)<<' '<<c.bus<<' '<<c.start<<' '<<c.duration<<' '<<c.source_offset<<' '<<c.gain<<' '<<c.fade_in<<' '<<c.fade_out<<' '<<c.loop<<'\n';
    for(const auto &e:d.envelopes){text<<"envelope "<<e.bus<<'\n';for(const auto &k:e.keys)text<<"gain "<<e.bus<<' '<<k.time<<' '<<k.gain<<' '<<unsigned(k.curve)<<'\n';}
    const auto bytes=text.str();if(bytes.size()>document_byte_limit){error="Cinematic document exceeds file budget";return false;}
    std::string pattern=std::string(path)+".tmp.XXXXXX";std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);
    int fd=mkstemp(temporary.data());if(fd<0){error="Cannot create cinematic save transaction";return false;}
    bool ok=true;size_t at=0;while(at<bytes.size()){ssize_t n=write(fd,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=size_t(n);}if(ok)ok=fsync(fd)==0;int closed=close(fd);ok=ok&&closed==0;
    if(ok)ok=vw_replace(temporary.data(),path)==0;
    if(!ok){unlink(temporary.data());error="Cinematic save failed; previous file preserved";return false;}
    const char *slash=std::strrchr(path,'/');std::string directory=slash?std::string(path,size_t(slash-path)):".";if(directory.empty())directory="/";
    fd=vw_open(directory.c_str(),O_RDONLY|O_DIRECTORY);if(fd>=0){(void)fsync(fd);close(fd);}error.clear();return true;
}

bool load(const char *path,Document &out,std::string &error){
    error.clear();if(!path||!*path){error="Choose a cinematic file";return false;}
    int fd=vw_open(path,O_RDONLY|O_NOFOLLOW);if(fd<0){error="Cannot open cinematic document";return false;}
    struct stat info={};if(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_size<0||std::uint64_t(info.st_size)>document_byte_limit){close(fd);error="Invalid/oversized cinematic file";return false;}
    std::string bytes(size_t(info.st_size),'\0');size_t at=0;bool ok=true;while(at<bytes.size()){ssize_t n=read(fd,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=size_t(n);}close(fd);if(!ok||bytes.find('\0')!=std::string::npos){error="Incomplete/binary cinematic document";return false;}
    std::istringstream file(bytes);std::string row;std::getline(file,row);if(row!="VALWOOD_CINEMATICS 1"){error="Unsupported cinematic document";return false;}
    Document next;bool timeline=false;unsigned line=1;
    while(std::getline(file,row)){line++;if(row.size()>4096){error="Oversized cinematic record";return false;}std::istringstream in(row);std::string kind;in>>kind;if(kind.empty()||kind[0]=='#')continue;
        bool parsed=false;
        if(kind=="timeline"){int loop=0;parsed=!timeline&&bool(in>>std::quoted(next.name)>>next.duration>>loop)&&loop>=0&&loop<=1;next.loop=loop==1;timeline=true;}
        else if(kind=="camera"&&next.camera.size()<camera_limit){CameraKey k;auto &p=k.pose;auto &q=p.rotation;unsigned c=0;parsed=bool(in>>k.time>>p.position.x>>p.position.y>>p.position.z>>q.x>>q.y>>q.z>>q.w>>p.fov_degrees>>c);k.curve=Curve(c);next.camera.push_back(k);}
        else if(kind=="zone"&&next.zones.size()<zone_limit){Zone z;unsigned shape=0;parsed=bool(in>>z.id>>std::quoted(z.name)>>std::quoted(z.asset)>>z.bus>>shape>>z.priority>>z.center.x>>z.center.y>>z.center.z>>z.extent.x>>z.extent.y>>z.extent.z>>z.blend>>z.gain>>z.dry>>z.reverb);z.shape=Shape(shape);next.zones.push_back(std::move(z));}
        else if(kind=="clip"&&next.clips.size()<clip_limit){Clip c;int loop=0;parsed=bool(in>>c.id>>std::quoted(c.asset)>>c.bus>>c.start>>c.duration>>c.source_offset>>c.gain>>c.fade_in>>c.fade_out>>loop)&&loop>=0&&loop<=1;c.loop=loop==1;next.clips.push_back(std::move(c));}
        else if(kind=="envelope"&&next.envelopes.size()<bus_limit){BusEnvelope e;parsed=bool(in>>e.bus);for(const auto &old:next.envelopes)if(old.bus==e.bus)parsed=false;next.envelopes.push_back(std::move(e));}
        else if(kind=="gain"){unsigned bus=0,c=0;GainKey key;parsed=bool(in>>bus>>key.time>>key.gain>>c);key.curve=Curve(c);auto entry=std::find_if(next.envelopes.begin(),next.envelopes.end(),[&](const BusEnvelope &e){return e.bus==bus;});if(entry==next.envelopes.end()||entry->keys.size()>=envelope_key_limit)parsed=false;else entry->keys.push_back(key);}
        std::string extra;if(!parsed||(in>>extra)){error="Invalid cinematic record at line "+std::to_string(line);return false;}
    }
    if(!timeline){error="Missing cinematic timeline";return false;}if(!validate(next,error))return false;out=std::move(next);return true;
}
}
