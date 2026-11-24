#include "engine/world_animation.h"
#include "engine/world_camp_skeletal_timing.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace world_animation;
namespace timing=world_camp_skeletal_timing;
static double distance(Vec3 a,Vec3 b){return std::hypot(std::hypot(double(a.x-b.x),double(a.y-b.y)),double(a.z-b.z));}
static Vec3 point(const Asset&a,const Pose&p,unsigned i){Vec3 v,n;std::string e;assert(deform_vertex(a.mesh.vertices[i],p,v,n,e));return transform_point(p.global[a.skeleton.mesh_node],v);}
int main(int argc,char**argv){
    assert(argc==4);
    // A render drop must not halve the time of a conversational reaction.
    for(unsigned actor=0;actor<3;++actor){
        std::array<float,4> reference={};
        for(unsigned fps:{10u,23u,30u,60u,144u}){
            std::array<float,4> w={1,0,0,0};
            for(unsigned frame=0;frame<fps;++frame)timing::approach(w,timing::target(true,false,false),1./fps);
            assert(w[2]>.899f);if(fps==10)reference=w;
            for(unsigned k=0;k<4;++k)assert(std::abs(w[k]-reference[k])<.000002f);
            for(unsigned k=0;k<4;++k){
                const double elapsed=double(fps*3)/fps;
                assert(std::abs(timing::seconds(k,actor,17.+elapsed)-timing::seconds(k,actor,20.))<1e-12);
            }
        }
        std::array<float,4> w={1,0,0,0};
        timing::approach(w,timing::target(true,false,false),.25);
        assert(w[2]>.80f); // The former fade only reached .58 in this interval.
    }
    for(unsigned actor=0;actor<3;++actor){
        Asset a;std::string error;assert(load_glb(argv[actor+1],a,error));
        assert(a.clips.size()==4);std::array<unsigned,4> clips={};
        const char *names[]={"Ambient","Listen","Talk","WarmHands"};
        for(unsigned k=0;k<4;++k){auto p=std::find_if(a.clips.begin(),a.clips.end(),[&](const Clip&c){return c.name==names[k];});assert(p!=a.clips.end());clips[k]=unsigned(p-a.clips.begin());}
        Pose neutral;assert(sample(a.skeleton,a.clips[clips[0]],0,true,neutral,error));
        std::vector<unsigned> soles,vertices;
        for(unsigned i=0;i<a.mesh.vertices.size();++i){if(point(a,neutral,i).y<.035f)soles.push_back(i);if(i%113==0)vertices.push_back(i);}
        assert(!soles.empty());
        double max_step=0,max_seam=0,sole_drift=0,hand_motion=0;
        unsigned hand=0;for(unsigned i=0;i<a.skeleton.joints.size();++i)if(a.skeleton.joints[i].name=="hand_r")hand=i;
        for(unsigned k=0;k<4;++k){
            const auto&clip=a.clips[clips[k]];const double rate=timing::seconds(k,actor,1)-timing::seconds(k,actor,0);
            const double duration=clip.duration/rate;
            assert(k!=2||(duration>3.4&&duration<3.9)); // One measured gesture, not8s.
            assert(k!=0||(duration>11.5&&duration<12.5)); // Breathing stays calm.
            Pose first,last;assert(sample(a.skeleton,clip,0,false,first,error));assert(sample(a.skeleton,clip,clip.duration,false,last,error));
            if(k==2){
                Pose old_second,new_second;
                assert(sample(a.skeleton,clip,.96+.027*actor,true,old_second,error));
                assert(sample(a.skeleton,clip,rate,true,new_second,error));
                const Vec3 start=transform_point(first.global[hand],{});
                const double old_motion=distance(start,transform_point(old_second.global[hand],{}));
                const double new_motion=distance(start,transform_point(new_second.global[hand],{}));
                std::printf("actor%u Talk hand displacement after1wall-second: before%.6fm after%.6fm\n",actor,old_motion,new_motion);
                assert(new_motion>.07&&new_motion>old_motion*1.6);
            }
            for(unsigned i:vertices)max_seam=std::max(max_seam,distance(point(a,first,i),point(a,last,i)));
            Pose prior=first;
            for(unsigned frame=0;frame<=unsigned(std::ceil(duration*60));++frame){
                Pose p;assert(sample(a.skeleton,clip,double(frame)/60*rate,true,p,error));
                for(unsigned i:vertices)max_step=std::max(max_step,distance(point(a,prior,i),point(a,p,i)));
                for(unsigned i:soles)sole_drift=std::max(sole_drift,distance(point(a,neutral,i),point(a,p,i)));
                hand_motion=std::max(hand_motion,distance(transform_point(first.global[hand],{}),transform_point(p.global[hand],{})));
                prior=p;
            }
            std::printf("actor%u %s cycle %.3fs (source%.1fs)\n",actor,names[k],duration,clip.duration);
        }
        std::printf("actor%u: max60HzStep=%.6fm loopSeam=%.6fm soleDrift=%.6fm handRange=%.6fm\n",actor,max_step,max_seam,sole_drift,hand_motion);
        assert(max_step<.04&&max_seam<.002&&sole_drift<.025&&hand_motion>.10);
    }
    std::puts("Camp acting: low-FPS timing, prompt response, distinct clip cadence, planted feet and continuous poses passed.");
}
