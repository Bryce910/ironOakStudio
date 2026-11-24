#ifndef VALWOOD_WORLD_ARENA_PLAYER_MOVEMENT_H
#define VALWOOD_WORLD_ARENA_PLAYER_MOVEMENT_H

#include <algorithm>
#include <cmath>
#include <cstddef>

// Arena character separation is a movement constraint, not a source of player
// displacement. Sweep against the requested path, then spend the remaining
// distance along a contact. Existing overlap may be escaped without an idle
// teleport or accumulated velocity against a blocked opponent.
namespace world_arena_player_movement {
struct Point { double x=0,z=0; };
struct Circle { double x=0,z=0,radius=0; };
struct Result { Point position; unsigned contacts=0; bool valid=false; };
inline double dot(Point a,Point b){return a.x*b.x+a.z*b.z;}
inline Point add(Point a,Point b){return {a.x+b.x,a.z+b.z};}
inline Point sub(Point a,Point b){return {a.x-b.x,a.z-b.z};}
inline Point scale(Point a,double t){return {a.x*t,a.z*t};}
inline bool finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.z);}

inline Result move(Point from,Point requested,const Circle *circles,std::size_t count,double limit){
    Result result;result.position=from;
    if(!finite(from)||!finite(requested)||!std::isfinite(limit)||limit<=0||limit>1.e6||
       std::abs(from.x)>1.e7||std::abs(from.z)>1.e7||count>32||(count&&!circles))return result;
    for(std::size_t i=0;i<count;++i)if(!std::isfinite(circles[i].x)||!std::isfinite(circles[i].z)||
        !std::isfinite(circles[i].radius)||circles[i].radius<=0||circles[i].radius>1.e6||
        std::abs(circles[i].x)>1.e7||std::abs(circles[i].z)>1.e7)return result;
    result.valid=true;
    requested.x=std::clamp(requested.x,-limit,limit);requested.z=std::clamp(requested.z,-limit,limit);
    Point remaining=sub(requested,from);
    constexpr double skin=1.e-6;
    for(unsigned pass=0;pass<8&&dot(remaining,remaining)>1.e-20;++pass){
        const double distance2=dot(remaining,remaining);double earliest=1.;Point normal;bool hit=false;
        auto boundary=[&](double position,double delta,Point inward){
            if((delta>0&&position+delta>limit)||(delta<0&&position+delta<-limit)){
                const double at=((delta>0?limit:-limit)-position)/delta;
                if(at>=0&&at<=earliest){earliest=at;normal=inward;hit=true;}
            }
        };
        boundary(result.position.x,remaining.x,{remaining.x>0?-1.:1.,0});
        boundary(result.position.z,remaining.z,{0,remaining.z>0?-1.:1.});
        for(std::size_t i=0;i<count;++i){
            const auto &circle=circles[i];const Point offset=sub(result.position,{circle.x,circle.z});
            const double radial2=dot(offset,offset),toward=dot(offset,remaining);
            const double outside=radial2-circle.radius*circle.radius;
            double at=1.;
            if(outside<=1.e-10){
                // A newly arrived opponent may already overlap this frame's
                // player root. Permit escape, never push the controller away.
                if(toward>=-1.e-12||radial2<1.e-18)continue;
                at=0;
            }else{
                if(toward>=0)continue;
                const double discriminant=toward*toward-distance2*outside;
                if(discriminant<=0)continue;
                at=(-toward-std::sqrt(discriminant))/distance2;
                if(at<0||at>earliest)continue;
            }
            if(at<=earliest){
                const auto contact=add(offset,scale(remaining,at));const double length=std::hypot(contact.x,contact.z);
                if(length<=1.e-12)continue;
                earliest=at;normal=scale(contact,1/length);hit=true;
            }
        }
        if(!hit){result.position=add(result.position,remaining);break;}
        const double travel=std::max(0.,earliest-skin/std::sqrt(distance2));
        result.position=add(result.position,scale(remaining,travel));
        remaining=scale(remaining,1-travel);
        const double inward=dot(remaining,normal);
        if(inward<0)remaining=sub(remaining,scale(normal,inward));
        ++result.contacts;
    }
    return result;
}
}
#endif
