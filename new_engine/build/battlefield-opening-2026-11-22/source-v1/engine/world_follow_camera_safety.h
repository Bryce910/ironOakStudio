#ifndef VALWOOD_WORLD_FOLLOW_CAMERA_SAFETY_H
#define VALWOOD_WORLD_FOLLOW_CAMERA_SAFETY_H
#include <algorithm>
#include <cmath>

struct WorldCameraPoint {double x=0,y=0,z=0;};
inline double world_camera_distance(WorldCameraPoint a,WorldCameraPoint b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
inline WorldCameraPoint world_camera_mix(WorldCameraPoint a,WorldCameraPoint b,double t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
// Enclose the actual near-plane corners, including wide displays. The ordinary
// 16:9 exploration lens retains the existing 22cm clearance.
inline float world_follow_camera_lens_radius(float near_plane,float fov,float aspect){
    const double near=std::isfinite(near_plane)?std::clamp(double(near_plane),.01,.4):.12;
    const double lens=std::isfinite(fov)?std::clamp(double(fov),.1,2.8):1.24;
    const double ratio=std::isfinite(aspect)?std::clamp(double(aspect),.1,8.):16./9.;
    const double half=near*std::tan(lens*.5);
    return float(std::max(.22,std::sqrt(near*near+half*half*(1+ratio*ratio))+.015));
}
struct WorldCameraSafetyFrame {WorldCameraPoint eye;bool line_blocked=false,motion_blocked=false,view_clear=true,recovering=false;};
// The boom and shoulder are safe component paths, not a proof that their final
// diagonal sees the player or that an orbit can move between consecutive eyes.
// Resolve the composed eye against both paths. This never moves the controller.
class WorldFollowCameraSafety {
    WorldCameraPoint previous_{},return_pivot_{},previous_pivot_{};bool initialized_=false,recovering_=false;
    template<class Clear> static WorldCameraPoint prefix(WorldCameraPoint a,WorldCameraPoint b,float radius,Clear &clear){
        if(clear(a,b,radius))return b;
        double lo=0,hi=1;
        for(unsigned i=0;i<12;++i){const double mid=(lo+hi)*.5;if(clear(a,world_camera_mix(a,b,mid),radius))lo=mid;else hi=mid;}
        return world_camera_mix(a,b,std::max(0.,lo-.001));
    }
public:
    void reset(){initialized_=recovering_=false;}
    template<class Clear> WorldCameraSafetyFrame update(WorldCameraPoint pivot,WorldCameraPoint desired,float radius,float dt,bool cut,Clear clear,bool component_blocked=false){
        WorldCameraSafetyFrame out{desired};
        if(!clear(pivot,desired,radius)){out.eye=prefix(pivot,desired,radius,clear);out.line_blocked=true;}
        // A moving body may enter the previously safe eye between frames.
        // Its now-invalid point is not a corridor to preserve: resolve this
        // frame from the clear player pivot, just as on the first frame.
        const bool history=initialized_&&!cut&&world_camera_distance(pivot,return_pivot_)<8.&&clear(previous_,previous_,radius);
        out.motion_blocked=history&&!clear(previous_,out.eye,radius);
        if(!history)recovering_=false;
        if(history&&(out.line_blocked||out.motion_blocked||component_blocked))recovering_=true;
        if(history&&recovering_&&clear(pivot,previous_,radius)){
            // If the old eye still sees the player, no instant zoom is needed.
            // Travel toward the new eye, or retract along the clear old boom
            // until there is room to round the post. Limit only collision
            // recovery; an ordinary clear-space orbit remains unchanged.
            const auto goal=out.eye;
            const bool direct=clear(previous_,goal,radius);
            const auto route=direct?goal:pivot;
            const double seconds=std::isfinite(dt)?std::clamp(double(dt),0.,.1):0.;
            const double distance=world_camera_distance(previous_,route);
            const double maximum=8*seconds+world_camera_distance(pivot,previous_pivot_);
            const double t=seconds>0?(distance>1.e-9?std::min(1-std::exp(-14*seconds),maximum/distance):1.):0.;
            auto candidate=world_camera_mix(previous_,route,t);
            if(!clear(pivot,candidate,radius)||!clear(previous_,candidate,radius)){
                const double retreat=world_camera_distance(previous_,pivot);
                const double weight=seconds>0?(retreat>1.e-9?std::min(1-std::exp(-14*seconds),maximum/retreat):1.):0.;
                candidate=world_camera_mix(previous_,pivot,weight);
            }
            out.eye=candidate;out.recovering=true;
            if(direct&&!out.line_blocked&&!out.motion_blocked&&!component_blocked&&seconds>0&&
               world_camera_distance(previous_,goal)<=maximum&&world_camera_distance(out.eye,goal)<.002){
                // The last two millimetres remain inside the same checked
                // segment; settling must not leave a permanent recovery latch.
                out.eye=goal;recovering_=false;
            }
        }else if(out.motion_blocked){
            // Find the longest current boom reachable without cutting across
            // the jamb. The retained distance feeds the ordinary eased boom.
            const auto goal=out.eye;bool found=false;
            for(unsigned i=1;i<=28;++i){
                const auto candidate=world_camera_mix(pivot,goal,1.-double(i)/28.);
                if(clear(pivot,candidate,radius)&&clear(previous_,candidate,radius)){out.eye=candidate;found=true;break;}
            }
            if(!found){
                // A convex corner can temporarily hide every current radial
                // candidate. Retract along the previous known-safe corridor;
                // never teleport the eye through the corner to the new pivot.
                const auto retreat=clear(previous_,pivot,radius)?pivot:return_pivot_;
                const double t=1-std::exp(-18*std::clamp(double(dt),0.,.1));
                out.eye=prefix(previous_,world_camera_mix(previous_,retreat,t),radius,clear);
            }
        }
        out.view_clear=clear(pivot,out.eye,radius);
        if(!initialized_||cut||out.view_clear)return_pivot_=pivot;
        previous_=out.eye;previous_pivot_=pivot;initialized_=true;return out;
    }
};
#endif
