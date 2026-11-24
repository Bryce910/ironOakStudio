#ifndef VALWOOD_WORLD_GRASS_INTERACTION_H
#define VALWOOD_WORLD_GRASS_INTERACTION_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Render-only footprints of the real gameplay controller. No terrain mutation,
// allocation, camera-follow coordinates, or global vegetation-density changes.
namespace world_grass_interaction {
constexpr float radius=.95f, lifetime=.78f, sample_interval=.24f;
struct Contact { double x=0,y=0,z=0,time=0; bool valid=false; };
struct State {
    std::array<Contact,3> trail{};
    std::array<float,4> head{},bounds{};
    std::array<std::array<float,4>,3> history{};
    double clock=0,last_x=0,last_y=0,last_z=0,last_sample=0;
    bool previous=false;
    uint64_t active_frames=0,moving_frames=0,history_samples=0,recovery_frames=0,clears=0;
    unsigned peak_contacts=0,active_history=0;
    double travel=0,max_step=0;
    void clear() {
        if(previous||active_history)++clears;
        trail={};head={};history={};bounds={};previous=false;active_history=0;
    }
    void update(double dt,double x,double y,double z,bool gameplay,bool grounded) {
        if(!std::isfinite(dt)||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!gameplay) { clear();return; }
        clock+=std::clamp(dt,0.,.1);
        const double step=previous?std::hypot(x-last_x,z-last_z):0.;
        // Teleports, scene transitions and vertical layer changes cannot leave
        // stale bends in an unrelated location or storey.
        if(previous&&(step>2.5||std::abs(y-last_y)>1.1))clear();
        const bool moved=previous&&grounded&&step>.0001;
        bool inserted=false;
        if(grounded) {
            ++active_frames;
            if(moved){++moving_frames;travel+=step;max_step=std::max(max_step,step);}
            if(moved&&clock-last_sample>=sample_interval) {
                trail[2]=trail[1];trail[1]=trail[0];trail[0]={last_x,last_y,last_z,clock,true};
                ++history_samples;last_sample=clock;
                inserted=true;
            }
            head={float(x),float(z),float(y),radius};
        } else head={};
        active_history=0;history={};
        double minx=x,maxx=x,minz=z,maxz=z;
        bool any=grounded;
        for(unsigned i=0;i<trail.size();++i) {
            auto &c=trail[i];const double age=clock-c.time;
            if(!c.valid||age>=lifetime){c.valid=false;continue;}
            const float t=float(std::clamp(age/lifetime,0.,1.));
            const float strength=1.f-t*t*(3.f-2.f*t);
            history[i]={float(c.x),float(c.z),float(c.y),strength};++active_history;
            if(!any){minx=maxx=c.x;minz=maxz=c.z;any=true;}
            else {minx=std::min(minx,c.x);maxx=std::max(maxx,c.x);minz=std::min(minz,c.z);maxz=std::max(maxz,c.z);}
        }
        if(active_history&&!moved)++recovery_frames;
        peak_contacts=std::max(peak_contacts,active_history+unsigned(grounded));
        if(any) {
            const double cx=(minx+maxx)*.5,cz=(minz+maxz)*.5;
            // .02m covers float packing error at this atlas's world extents.
            const double reach=std::hypot(maxx-minx,maxz-minz)*.5+radius+.02;
            const double change_speed=dt>0?std::max(step/dt,inserted?1./dt:0.):0.;
            bounds={float(cx),float(cz),float(reach*reach),float(change_speed)};
        } else bounds={};
        last_x=x;last_y=y;last_z=z;previous=grounded;
        if(!grounded)last_sample=clock;
    }
};
}
#endif
