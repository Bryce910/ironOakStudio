#ifndef VALWOOD_WORLD_DRAGON_LIGHTS_H
#define VALWOOD_WORLD_DRAGON_LIGHTS_H

#include "engine/world_dragon_breath.h"
#include "engine/world_torch_lights.h"
#include <limits>

namespace world_dragon_lights_detail {
inline bool finite_position(double v) {return std::isfinite(v)&&std::abs(v)<=1.e8;}
inline bool valid_existing(const WorldTorchLights &lights) {
    if(lights.count>WorldTorchLights::capacity)return false;
    for(unsigned i=0;i<lights.count;i++) {
        const auto &p=lights.position_radius[i];const auto &c=lights.color_intensity[i];
        for(unsigned k=0;k<3;k++)if(!finite_position(p[k])||!std::isfinite(c[k])||c[k]<0||c[k]>64)return false;
        if(!std::isfinite(p[3])||p[3]<=0||p[3]>128||!std::isfinite(c[3])||c[3]<0||c[3]>1000)return false;
        double normal=0;
        for(unsigned k=0;k<4;k++){if(!std::isfinite(lights.plane[i][k]))return false;if(k<3)normal+=double(lights.plane[i][k])*lights.plane[i][k];}
        if(normal>1.001||std::abs(lights.plane[i][3])>3.e8)return false;
        for(float v:lights.bounds[i])if(!finite_position(v))return false;
        if(lights.bounds[i][0]>=lights.bounds[i][2]||lights.bounds[i][1]>=lights.bounds[i][3]||
           !finite_position(lights.heights[i][0])||!finite_position(lights.heights[i][1])||lights.heights[i][0]>=lights.heights[i][1])return false;
    }
    for(unsigned k=0;k<3;k++)if(!finite_position(lights.cluster_min[k])||!finite_position(lights.cluster_max[k])||
        (lights.count&&lights.cluster_min[k]>lights.cluster_max[k]))return false;
    return true;
}
inline double influence(const float *position,const float *color,double cx,double cy,double cz) {
    // A broad upper-envelope score also retains lamps lighting visible ground
    // when the camera itself is outside their radius. Never subtract two large
    // floats before converting: world coordinates are compared in double.
    const double x=double(position[0])-cx,y=double(position[1])-cy,z=double(position[2])-cz,r=position[3];
    const double energy=double(std::max({color[0],color[1],color[2]}))*color[3];
    return energy*r*r/(x*x+y*y+z*z+r*r);
}
inline float lower(double value) {
    return std::nextafter(float(value),-std::numeric_limits<float>::infinity());
}
inline float upper(double value) {
    return std::nextafter(float(value),std::numeric_limits<float>::infinity());
}
}

// Merge visual breath lighting into the existing8-point shader array. There
// are no new shader loops, allocations, caches, camera-relative float losses,
// or writes to retained torches' authored plane/box/height clipping geometry.
// All active inputs are validated before mutation. Invalid input returnsfalse
// atomically; no contributing breath leaves the WHOLE original byte-identical.
// The caller rebuilds the normal torch set each frame, then calls this helper.
inline bool world_dragon_lights_merge(WorldTorchLights &lights,const WorldDragonBreathLight *breath,
    std::size_t count,double cx,double cy,double cz) {
    using namespace world_dragon_lights_detail;
    if(count>WORLD_DRAGON_BREATH_EMITTER_CAPACITY||(count&&!breath)||!finite_position(cx)||!finite_position(cy)||!finite_position(cz)||!valid_existing(lights))return false;
    for(std::size_t i=0;i<count;i++) {
        const auto &b=breath[i];
        for(unsigned k=0;k<3;k++)if(!finite_position(b.position[k])||!std::isfinite(b.color[k])||b.color[k]<0||b.color[k]>64)return false;
        if(!std::isfinite(b.strength)||b.strength<0||b.strength>1000||!std::isfinite(b.radius)||b.radius<=0||b.radius>128)return false;
    }
    struct Candidate {std::array<float,4> position={},color={};double score=0;};
    std::array<Candidate,WORLD_DRAGON_BREATH_EMITTER_CAPACITY> candidates={};unsigned candidates_count=0;
    for(std::size_t i=0;i<count;i++) {
        const auto &b=breath[i];
        const double x=b.position[0]-cx,y=b.position[1]-cy,z=b.position[2]-cz,distance=std::sqrt(x*x+y*y+z*z);
        // A distant aerial jet cannot illuminate this receiver region. The
        //100–120m smooth fade avoids an abrupt cutoff on a moving flyby.
        if(distance>=120.)continue;
        const float fade=1.f-world_torch_smooth(100.f,120.f,float(distance));
        Candidate c;
        c.position={{float(b.position[0]),float(b.position[1]),float(b.position[2]),b.radius}};
        c.color={{b.color[0],b.color[1],b.color[2],b.strength*fade}};
        const double radiance=double(std::max({c.color[0],c.color[1],c.color[2]}))*c.color[3];
        c.score=influence(c.position.data(),c.color.data(),cx,cy,cz);
        // Negligible breath onset/tail cannot evict a meaningful street lamp.
        if(radiance<.08||c.score<=.012)continue;
        // Fade into the influence threshold instead of dropping a partly
        // visible distant point at a hard score boundary.
        const float influence_fade=world_torch_smooth(.012f,.040f,float(c.score));
        c.color[3]*=influence_fade;c.score*=influence_fade;
        if(c.score<=0)continue;
        candidates[candidates_count++]=c;
    }
    if(!candidates_count)return true;
    if(candidates_count==2&&candidates[1].score>candidates[0].score)std::swap(candidates[0],candidates[1]);
    WorldTorchLights result=lights;bool changed=false;
    for(unsigned n=0;n<candidates_count;n++) {
        const auto &candidate=candidates[n];unsigned slot=result.count;
        if(slot==WorldTorchLights::capacity) {
            unsigned weakest=0;double weakest_score=std::numeric_limits<double>::infinity();
            for(unsigned i=0;i<result.count;i++) {
                const double score=influence(result.position_radius[i].data(),result.color_intensity[i].data(),cx,cy,cz);
                if(score<weakest_score){weakest_score=score;weakest=i;}
            }
            if(candidate.score<=weakest_score*1.25+.025)continue;
            slot=weakest;
        } else result.count++;
        result.position_radius[slot]=candidate.position;result.color_intensity[slot]=candidate.color;
        // Existing torch shaders reject plane<=0. Zero normal with positive
        // offset expresses an unclipped point; vec4(0) would disable it.
        result.plane[slot]={{0,0,0,1}};
        const auto &p=candidate.position;
        result.bounds[slot]={{lower(double(p[0])-p[3]),lower(double(p[2])-p[3]),upper(double(p[0])+p[3]),upper(double(p[2])+p[3])}};
        result.heights[slot]={{lower(double(p[1])-p[3]),upper(double(p[1])+p[3])}};
        changed=true;
    }
    if(!changed)return true;
    for(unsigned axis=0;axis<3;axis++) {
        double minimum=std::numeric_limits<double>::infinity(),maximum=-minimum;
        for(unsigned i=0;i<result.count;i++) {
            const auto &p=result.position_radius[i];minimum=std::min(minimum,double(p[axis])-p[3]);maximum=std::max(maximum,double(p[axis])+p[3]);
        }
        result.cluster_min[axis]=lower(minimum);result.cluster_max[axis]=upper(maximum);
    }
    lights=result;return true;
}

#endif
