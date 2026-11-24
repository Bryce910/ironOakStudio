#ifndef VALWOOD_WORLD_STUDIO_SCENE_RECORD_H
#define VALWOOD_WORLD_STUDIO_SCENE_RECORD_H
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
// One scene transaction record. An unchanged source root may be deliberately
// elevated (the apartment is at600m); adding props must retain that root even
// when its rotation and scale are identity. New terrain placements retain the
// renderer's existing grounding policy unless they have an explicit transform.
namespace world_studio_scene_record {
struct Record {
    const char *name="";double u=0,v=0;
    // Keep degrees in double precision until the renderer parses radians.
    // A float degree intermediate cannot round-trip every float radian pose.
    double yaw=0;float scale=1,burial=0,elevation=0,offset_y=0;
    double pitch=0,roll=0;float sx=1,sy=1,sz=1;
    float pivot=std::numeric_limits<float>::quiet_NaN();
    bool preserve_source_root=false;
    float wind_sway=1,wind_flutter=1;
};
inline void write(std::ostream &out,const Record &r){
    out<<std::setprecision(17)<<r.name<<' '<<r.u<<' '<<r.v<<' '<<r.yaw<<' '<<r.scale<<' '<<r.burial<<' '<<r.elevation<<' '<<r.offset_y;
    const bool transformed=r.pitch!=0||r.roll!=0||r.sx!=1||r.sy!=1||r.sz!=1;
    const bool pivot=std::isfinite(r.pivot)&&(r.preserve_source_root||transformed);
    if(transformed||pivot)out<<' '<<r.pitch<<' '<<r.roll<<' '<<r.sx<<' '<<r.sy<<' '<<r.sz;
    if(pivot)out<<' '<<r.pivot;
    if(r.wind_sway!=1||r.wind_flutter!=1)out<<std::setprecision(std::numeric_limits<float>::max_digits10)<<" wind "<<r.wind_sway<<' '<<r.wind_flutter;
    out<<'\n';
}
}
#endif
