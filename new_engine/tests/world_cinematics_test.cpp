#include "engine/world_cinematics.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

static std::atomic<unsigned long> allocations{0};
[[gnu::noinline]] void *operator new(std::size_t bytes){allocations++;if(void *p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t bytes){return ::operator new(bytes);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}

using namespace world_cinematics;
static unsigned checks=0;
#define CHECK(x) do{checks++;if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
static bool near(double a,double b,double eps=1e-5){return std::abs(a-b)<=eps;}
static double length(Vec3 v){return std::hypot(std::hypot(v.x,v.y),v.z);}
static std::string bytes(const std::string &path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static Document example(){
    Document d;d.name="Castle to rainforest";d.duration=10;
    CameraKey a;a.pose.position={0,2,0};a.pose.rotation=orientation(0,.1);a.pose.fov_degrees=60;a.curve=Curve::Linear;
    CameraKey b;b.time=10;b.pose.position={10,4,-20};b.pose.rotation=orientation(1,.3);b.pose.fov_degrees=80;
    d.camera={a,b};
    Zone z;z.id=4;z.asset="audio/demo/forest.wav";z.bus=1;z.extent={10,10,10};z.blend=2;z.dry=.8f;z.reverb=.3f;d.zones={z};
    Clip c;c.id=7;c.asset="audio/demo/creek.wav";c.bus=2;c.start=2;c.duration=4;c.fade_in=1;c.fade_out=1;d.clips={c};
    BusEnvelope e;e.bus=1;e.keys={{0,.5f,Curve::Linear},{10,1.5f,Curve::Linear}};d.envelopes={e};return d;
}
static float weight(const ZoneMix &mix,unsigned id){for(unsigned i=0;i<mix.count;i++)if(mix.voices[i].id==id)return mix.voices[i].weight;return 0;}

int main(){
    std::string error;Document d=example();CHECK(validate(d,error));
    CameraPose pose;CHECK(sample_camera(d,5,pose));CHECK(near(pose.position.x,5)&&near(pose.position.y,3)&&near(pose.position.z,-10));CHECK(near(pose.fov_degrees,70));
    CHECK(near(length(forward(pose.rotation)),1,1e-12));CHECK(near(length(up(pose.rotation)),1,1e-12));
    auto f=forward(orientation(.7,.3));CHECK(near(f.x,std::sin(.7)*std::cos(.3),1e-12)&&near(f.y,std::sin(.3),1e-12)&&near(f.z,-std::cos(.7)*std::cos(.3),1e-12));
    CHECK(sample_camera(d,-2,pose)&&near(pose.position.x,0));CHECK(sample_camera(d,50,pose)&&near(pose.position.x,10));
    d.camera[0].curve=Curve::Smooth;CHECK(sample_camera(d,2.5,pose)&&near(pose.position.x,1.5625));
    d.camera[0].curve=Curve::Hold;CHECK(sample_camera(d,9,pose)&&near(pose.position.x,0));CHECK(sample_camera(d,10,pose)&&near(pose.position.x,10));
    d.camera[0].curve=Curve::Linear;d.camera[0].pose.rotation=orientation(179*3.141592653589793/180,0);d.camera[1].pose.rotation=orientation(-179*3.141592653589793/180,0);
    CHECK(sample_camera(d,5,pose)&&forward(pose.rotation).z>.99999); // shortest orientation arc across +/-pi
    auto same=d.camera[0].pose.rotation;d.camera[1].pose.rotation={-same.x,-same.y,-same.z,-same.w};CHECK(sample_camera(d,5,pose));CHECK(near(forward(pose.rotation).x,forward(same).x,1e-12));
    CameraPose retained=pose;CHECK(!sample_camera(d,std::numeric_limits<double>::quiet_NaN(),pose));CHECK(near(retained.position.x,pose.position.x));
    d=example();CHECK(near(sample_bus(d,1,5),1));CHECK(near(sample_bus(d,0,3),1));CHECK(sample_bus(d,99,3)==0);
    d.loop=true;CHECK(sample_camera(d,15,pose)&&near(pose.position.x,5));CHECK(sample_camera(d,10,pose)&&near(pose.position.x,0));d.loop=false;

    ZoneMix zones;CHECK(sample_zones(d,{0,0,0},0,zones));CHECK(near(zones.outside_gain,0));CHECK(near(weight(zones,4),1));CHECK(near(zones.buses[1].dry_gain,.4)&&near(zones.buses[1].reverb_send,.15));
    CHECK(sample_zones(d,{10,0,0},5,zones));CHECK(near(weight(zones,4),std::sqrt(.5))&&near(zones.outside_gain,std::sqrt(.5)));
    CHECK(sample_zones(d,{12,0,0},5,zones)&&zones.outside_gain==1);CHECK(sample_zones(d,{0,20,0},5,zones)&&zones.outside_gain==1);
    auto high=d.zones[0];high.id=9;high.priority=10;high.asset="audio/demo/tavern.wav";d.zones.push_back(high);
    CHECK(sample_zones(d,{0,0,0},5,zones));CHECK(weight(zones,9)>.9999&&weight(zones,4)==0);
    d.zones[0].extent={100,100,100};CHECK(sample_zones(d,{10,0,0},5,zones));CHECK(near(weight(zones,4),std::sqrt(.5))&&near(weight(zones,9),std::sqrt(.5)));
    d.zones[1].priority=0;CHECK(sample_zones(d,{0,0,0},5,zones));CHECK(near(weight(zones,4),std::sqrt(.5))&&near(weight(zones,9),std::sqrt(.5)));
    float prior=0;for(int i=0;i<=10000;i++){
        CHECK(sample_zones(d,{-15+i*.003,0,0},5,zones));double energy=double(zones.outside_gain)*zones.outside_gain;
        for(unsigned k=0;k<zones.count;k++)energy+=double(zones.voices[k].weight)*zones.voices[k].weight;
        CHECK(near(energy,1,2e-7));float current=weight(zones,9);CHECK(std::abs(current-prior)<.003||i==0);prior=current;
    }
    d=example();d.zones[0].shape=Shape::Sphere;CHECK(sample_zones(d,{0,0,10},5,zones)&&near(weight(zones,4),std::sqrt(.5)));
    Mix mix;CHECK(sample_mix(d,{0,0,0},2.5,mix));CHECK(mix.clip_count==1&&near(mix.clips[0].gain,std::sqrt(.5)));CHECK(near(mix.clips[0].source_offset,.5));
    CHECK(sample_mix(d,{0,0,0},5.5,mix)&&near(mix.clips[0].gain,std::sqrt(.5)));CHECK(sample_mix(d,{0,0,0},6,mix)&&mix.clip_count==0);

    Transport player;Step step;CHECK(!player.advance(0,step));CHECK(player.configure(10,true));CHECK(player.advance(100,step)&&step.jumped&&near(step.time,0));
    CHECK(player.advance(109,step)&&near(step.time,9)&&!step.jumped);CHECK(player.advance(110.5,step)&&near(step.time,.5)&&step.wraps==1&&step.cycle==1);
    Step unchanged=step;CHECK(!player.advance(109,step));CHECK(near(step.time,unchanged.time)&&player.cycle()==1);
    player.pause(true);CHECK(player.advance(120,step)&&step.paused&&step.jumped&&near(step.time,.5));CHECK(player.advance(1000,step)&&near(step.time,.5));
    player.pause(false);CHECK(player.advance(1001,step)&&!step.paused&&step.jumped&&near(step.time,.5));CHECK(player.advance(1002,step)&&near(step.time,1.5));
    CHECK(player.seek(8));CHECK(player.advance(1003,step)&&step.jumped&&near(step.time,8)&&step.cycle==0);CHECK(player.advance(1025,step)&&step.wraps==3&&near(step.time,0));
    CHECK(player.seek(-5)&&near(player.time(),0));CHECK(player.seek(20)&&near(player.time(),0));CHECK(!player.seek(std::numeric_limits<double>::infinity()));
    CHECK(!player.advance(1000,step));CHECK(!player.configure(0,true));CHECK(player.configure(10,false));CHECK(player.advance(0,step));CHECK(player.advance(15,step)&&step.finished&&near(step.time,10)&&near(step.elapsed,10));
    player.looping(true);CHECK(player.advance(20,step)&&step.jumped&&near(step.time,0));
    Transport fine,coarse;CHECK(fine.configure(7,true)&&coarse.configure(7,true));CHECK(fine.advance(0,step));for(unsigned i=1;i<=10000;i++)CHECK(fine.advance(i*.01,step));CHECK(coarse.advance(0,step)&&coarse.advance(100,step));CHECK(near(fine.time(),coarse.time(),1e-9)&&fine.cycle()==coarse.cycle());

    Document cues;cues.duration=10;Clip a;a.id=1;a.asset="audio/first.wav";a.start=.25;a.duration=.25;a.fade_in=a.fade_out=0;Clip b=a;b.id=2;b.asset="audio/second.wav";b.start=.5;b.duration=.4;cues.clips={b,a};CHECK(validate(cues,error));
    Step interval;interval.previous=0;interval.time=1;interval.elapsed=1;interval.jumped=false;Schedule events;CHECK(schedule(cues,interval,events)&&events.count==4);CHECK(events.events[0].id==1&&events.events[0].kind==EventKind::Start);CHECK(events.events[1].id==1&&events.events[1].kind==EventKind::Stop);CHECK(events.events[2].id==2&&events.events[2].kind==EventKind::Start);CHECK(near(events.events[1].step_offset,.5)&&near(events.events[2].step_offset,.5));
    cues.loop=true;cues.clips.resize(1);cues.clips[0].start=0;cues.clips[0].duration=10;Transport transport;CHECK(transport.configure(10,true));CHECK(transport.advance(0,step)&&schedule(cues,step,events));CHECK(events.resync&&events.count==2&&events.events[0].kind==EventKind::StopAll);
    CHECK(transport.advance(9,step)&&schedule(cues,step,events)&&events.count==0);CHECK(transport.advance(10.5,step)&&schedule(cues,step,events));CHECK(events.count==2&&events.events[0].kind==EventKind::Stop&&events.events[1].kind==EventKind::Start&&events.events[1].cycle==1);CHECK(near(events.events[1].step_offset,1));
    CHECK(transport.seek(4)&&transport.advance(11,step)&&schedule(cues,step,events));CHECK(events.resync&&events.count==2&&near(events.events[1].source_offset,4));
    transport.pause(true);CHECK(transport.advance(12,step)&&schedule(cues,step,events)&&events.count==1&&events.events[0].kind==EventKind::StopAll);
    transport.pause(false);CHECK(transport.advance(13,step)&&transport.advance(100,step)&&schedule(cues,step,events));CHECK(events.resync&&events.count==2); // no burst after many loops
    Step invalid=step;invalid.wraps++;Schedule kept=events;CHECK(!schedule(cues,invalid,events));CHECK(events.count==kept.count);

    d=example();auto invalid_document=d;invalid_document.camera[1].time=0;CHECK(!validate(invalid_document,error));invalid_document=d;invalid_document.camera[0].pose.rotation={0,0,0,0};CHECK(!validate(invalid_document,error));
    invalid_document=d;invalid_document.clips[0].asset="../private.wav";CHECK(!validate(invalid_document,error));invalid_document=d;invalid_document.zones.push_back(invalid_document.zones[0]);CHECK(!validate(invalid_document,error));
    invalid_document=d;invalid_document.camera.resize(camera_limit+1);CHECK(!validate(invalid_document,error));invalid_document=d;invalid_document.envelopes[0].keys[0].gain=std::numeric_limits<float>::quiet_NaN();CHECK(!validate(invalid_document,error));

    char folder[]="/tmp/valwood-cinematics-test-XXXXXX";CHECK(mkdtemp(folder));const std::string file=std::string(folder)+"/timeline.vct",second=std::string(folder)+"/second.vct";
    CHECK(save(file.c_str(),d,error));Document loaded;CHECK(load(file.c_str(),loaded,error));CHECK(validate(loaded,error));CHECK(save(second.c_str(),loaded,error));CHECK(bytes(file)==bytes(second));
    auto original=bytes(file);invalid_document=d;invalid_document.duration=0;CHECK(!save(file.c_str(),invalid_document,error)&&bytes(file)==original);
    std::string link=std::string(folder)+"/link.vct";CHECK(symlink(file.c_str(),link.c_str())==0);CHECK(!save(link.c_str(),d,error)&&bytes(file)==original);CHECK(!load(link.c_str(),loaded,error));
    std::string broken=std::string(folder)+"/broken.vct";for(const std::string &contents:{std::string("BAD 1\n"),std::string("VALWOOD_CINEMATICS 1\ntimeline \"A\" 2 0\nunknown 1\n"),std::string("VALWOOD_CINEMATICS 1\ntimeline \"A\" 2 0\nclip 1 \"../bad.wav\" 0 0 1 0 1 0 0 0\n"),std::string("VALWOOD_CINEMATICS 1\ntimeline \"A\" 2 0\ncamera nan 0 0 0 0 0 0 1 60 0\n")}){
        std::ofstream out(broken);out<<contents;out.close();CHECK(!load(broken.c_str(),loaded,error));CHECK(loaded.name==d.name&&loaded.clips.size()==d.clips.size());
    }
    int fd=open(broken.c_str(),O_WRONLY|O_TRUNC);CHECK(fd>=0&&ftruncate(fd,document_byte_limit+1)==0);close(fd);CHECK(!load(broken.c_str(),loaded,error));
    CHECK(!save((std::string(folder)+"/missing/draft.vct").c_str(),d,error));CHECK(save(file.c_str(),loaded,error)&&bytes(file)==original);
    unlink(link.c_str());unlink(file.c_str());unlink(second.c_str());unlink(broken.c_str());CHECK(rmdir(folder)==0);

    // Every ordinary playback/frame function is a bounded no-allocation path.
    d=example();d.loop=true;CHECK(validate(d,error));Transport tick;CHECK(tick.configure(d.duration,true));CHECK(tick.advance(0,step));CHECK(sample_mix(d,{0,0,0},0,mix));CHECK(schedule(d,step,events));
    auto before=allocations.load();bool all=true;for(unsigned i=1;i<=50000;i++){
        const double t=i/60.;all=all&&tick.advance(t,step)&&sample_camera(d,step.time,pose)&&sample_mix(d,{std::sin(t)*12,0,0},step.time,mix)&&schedule(d,step,events);
    }CHECK(all);CHECK(allocations.load()==before);
    std::cout<<"world_cinematics: "<<checks<<" camera/zone/scheduling/codec contracts; 50,000 playback frames without allocations\n";return 0;
}
