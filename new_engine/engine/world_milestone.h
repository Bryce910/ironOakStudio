#ifndef VALWOOD_WORLD_MILESTONE_H
#define VALWOOD_WORLD_MILESTONE_H
#include <algorithm>
#include <cmath>
namespace world_milestone {
enum class View {None=0,Courtyard=1,Jungle=2,Hero=3};
struct Review {
    View view=View::None;float orbit=.24f,distance=1.95f,pitch=.04f;
    unsigned long long portrait_frames=0;
    bool active()const{return view!=View::None;}
    bool portrait()const{return view==View::Hero;}
    void drag(float x,float y){if(!std::isfinite(x)||!std::isfinite(y))return;orbit=std::remainder(orbit+x*.004f,6.283185307f);pitch=std::clamp(pitch-y*.002f,-.25f,.3f);}
    void zoom(float delta){if(std::isfinite(delta))distance=std::clamp(distance-delta*.12f,1.45f,3.2f);}
};
struct Portrait {double x,y,z;float yaw,pitch,fov,focus;};
inline Portrait portrait(double x,double feet,double z,float player_yaw,const Review &r){
    const double a=player_yaw+r.orbit,dx=std::sin(a)*r.distance,dz=-std::cos(a)*r.distance;
    const double y=feet+1.48+std::sin(r.pitch)*r.distance;
    return{x+dx,y,z+dz,float(std::atan2(-dx,dz)),float(std::atan2(feet+1.48-y,r.distance)),.48f,float(std::hypot(r.distance,y-feet-1.48))};
}
inline const char *scene(View view){return view==View::Jungle?"rainforest-milestone":view==View::Hero?"castle-hero-portrait":"castle-milestone";}
}
#endif
