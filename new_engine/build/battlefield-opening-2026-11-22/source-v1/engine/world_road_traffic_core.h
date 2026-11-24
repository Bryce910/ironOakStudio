#ifndef VALWOOD_WORLD_ROAD_TRAFFIC_CORE_H
#define VALWOOD_WORLD_ROAD_TRAFFIC_CORE_H
#include "engine/world_road_traffic.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace world_road_traffic_core {
constexpr double pi=3.14159265358979323846;
struct Profile {double radius=.32,front=.40,rear=.40,height=1.85,stride=1.0,speed=1.15;};
struct Point {double x=0,y=0,z=0,dx=0,dz=1;};
struct Actor {
    WorldRoadTrafficActor view;double station=0,phase=0,idle=0,mix=0,mode_age=0,plant_from=0,plant_to=0,blend_from=0,breath_from=0,pace=1,start_delay=0;unsigned stance=0;
    enum Mode{Idle,Prepare,Start,Walk,Plant,Stop} mode=Idle;
};
inline double sq(double x){return x*x;}
inline double smooth(double t){t=std::clamp(t,0.,1.);return t*t*t*(t*(t*6-15)+10);}
inline double point_segment(double x,double z,double ax,double az,double bx,double bz){double dx=bx-ax,dz=bz-az,n=dx*dx+dz*dz,t=n>1e-18?std::clamp(((x-ax)*dx+(z-az)*dz)/n,0.,1.):0;return sq(x-ax-dx*t)+sq(z-az-dz*t);}
inline double cross(double ax,double az,double bx,double bz){return ax*bz-az*bx;}
inline double segment_distance(double ax,double az,double bx,double bz,double cx,double cz,double dx,double dz){
    double abx=bx-ax,abz=bz-az,cdx=dx-cx,cdz=dz-cz,den=cross(abx,abz,cdx,cdz);
    if(std::abs(den)>1e-15){double t=cross(cx-ax,cz-az,cdx,cdz)/den,u=cross(cx-ax,cz-az,abx,abz)/den;if(t>=0&&t<=1&&u>=0&&u<=1)return 0;}
    return std::min({point_segment(ax,az,cx,cz,dx,dz),point_segment(bx,bz,cx,cz,dx,dz),point_segment(cx,cz,ax,az,bx,bz),point_segment(dx,dz,ax,az,bx,bz)});
}
struct Capsule {double ax=0,az=0,bx=0,bz=0,r=.3,low=0,high=2;};
inline Capsule capsule(const Point &p,const Profile &f){double rear=std::max(0.,f.rear-f.radius),front=std::max(0.,f.front-f.radius);return {p.x-p.dx*rear,p.z-p.dz*rear,p.x+p.dx*front,p.z+p.dz*front,f.radius,p.y,p.y+f.height};}
inline bool separate(const Capsule &a,const Capsule &b,double margin=0){return a.high<=b.low+.05||a.low>=b.high-.05||segment_distance(a.ax,a.az,a.bx,a.bz,b.ax,b.az,b.bx,b.bz)>=sq(a.r+b.r+margin);}
// Camera lenses use the same fitted source envelope as movement: a horizontal
// capsule extruded through the complete human/horse/rider/cart height. Clip the
// swept eye to that vertical interval before testing its XZ path; a camera ray
// above a traveler must not be rejected by an unrelated ground-level footprint.
inline bool camera_capsule_clear(const Capsule &body,double ax,double ay,double az,
                                 double bx,double by,double bz,double radius,double half_height){
    constexpr double margin=.025;
    const double low=body.low-radius-half_height-margin,high=body.high+radius+half_height+margin;
    const double dy=by-ay;double begin=0,end=1;
    if(std::abs(dy)<1.e-12){if(ay<low||ay>high)return true;}
    else{double a=(low-ay)/dy,b=(high-ay)/dy;if(a>b)std::swap(a,b);begin=std::max(begin,a);end=std::min(end,b);if(begin>end)return true;}
    const double r=body.r+radius+margin;
    const double x0=ax+(bx-ax)*begin,z0=az+(bz-az)*begin,x1=ax+(bx-ax)*end,z1=az+(bz-az)*end;
    if(std::max(x0,x1)+r<std::min(body.ax,body.bx)||std::min(x0,x1)-r>std::max(body.ax,body.bx)||
       std::max(z0,z1)+r<std::min(body.az,body.bz)||std::min(z0,z1)-r>std::max(body.az,body.bz))return true;
    return segment_distance(x0,z0,x1,z1,body.ax,body.az,body.bx,body.bz)>=sq(r);
}
struct Route {
    std::array<Point,1536> points={};std::array<double,1536> distances={};unsigned count=0;double length=0,cycle=0;
    WorldRoadTrafficConfig config;
    bool push(Point p){if(count&&std::hypot(p.x-points[count-1].x,p.z-points[count-1].z)<1e-6)return true;if(count>=points.size())return false;if(count)length+=std::hypot(p.x-points[count-1].x,p.z-points[count-1].z);points[count]=p;distances[count++]=length;return true;}
    Point center(double s)const{if(!count)return {};s=std::clamp(s,0.,length);unsigned i=0;while(i+1<count&&distances[i+1]<s)++i;unsigned j=std::min(i+1,count-1);double t=j!=i?(s-distances[i])/(distances[j]-distances[i]):0;const auto&a=points[i];const auto&b=points[j];Point p{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.dx+(b.dx-a.dx)*t,a.dz+(b.dz-a.dz)*t};double n=std::hypot(p.dx,p.dz);if(n>1e-9){p.dx/=n;p.dz/=n;}return p;}
    Point sample(double t)const{
        t=std::fmod(t,cycle);if(t<0)t+=cycle;const double r=config.lane_offset,arc=pi*r;
        if(t<=length){auto p=center(t);p.x+=p.dz*r;p.z-=p.dx*r;return p;}
        if(t<length+arc){auto p=center(length);double a=(t-length)/r,c=std::cos(a),s=std::sin(a),dx=p.dx,dz=p.dz;p.x+=dz*r*c+dx*r*s;p.z+=-dx*r*c+dz*r*s;p.dx=dx*c-dz*s;p.dz=dz*c+dx*s;return p;}
        if(t<=2*length+arc){auto p=center(2*length+arc-t);p.x-=p.dz*r;p.z+=p.dx*r;p.dx=-p.dx;p.dz=-p.dz;return p;}
        auto p=center(0);double a=(t-2*length-arc)/r,c=std::cos(a),s=std::sin(a),dx=p.dx,dz=p.dz;p.x-=dz*r*c+dx*r*s;p.z-=(-dx*r*c+dz*r*s);p.dx=-dx*c+dz*s;p.dz=-dz*c-dx*s;return p;
    }
    bool build(const WorldRoadTrafficConfig &c){
        *this={};config=c;for(auto&p:c.points)for(double v:{p.x,p.y,p.z})if(!std::isfinite(v)||std::abs(v)>1e6)return false;
        for(double v:{c.start_trim,c.end_trim,c.lane_offset,c.road_half_width,c.camp.x,c.camp.y,c.camp.z,c.town.x,c.town.y,c.town.z,c.encounter.x,c.encounter.y,c.encounter.z,c.satchel.x,c.satchel.y,c.satchel.z})if(!std::isfinite(v)||std::abs(v)>1e6)return false;
        if(c.lane_offset<.7||c.lane_offset>1.7||c.road_half_width<2||c.road_half_width>5||c.start_trim<6||c.end_trim<6||c.start_trim>60||c.end_trim>60)return false;
        for(double v:{c.camp_radius,c.town_radius,c.encounter_radius,c.satchel_radius})if(!std::isfinite(v)||v<=0||v>30)return false;
        std::array<Point,6> p={};for(unsigned i=0;i<6;++i)p[i]={c.points[i].x,c.points[i].y,c.points[i].z};
        for(unsigned i=0;i<5;++i)if(std::hypot(p[i+1].x-p[i].x,p[i+1].z-p[i].z)<15)return false;
        auto move=[](Point a,Point b,double d){double n=std::hypot(b.x-a.x,b.z-a.z),t=d/n;return Point{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};};
        if(c.start_trim>=std::hypot(p[1].x-p[0].x,p[1].z-p[0].z)-3||c.end_trim>=std::hypot(p[5].x-p[4].x,p[5].z-p[4].z)-3)return false;
        p[0]=move(p[0],p[1],c.start_trim);p[5]=move(p[5],p[4],c.end_trim);
        auto line=[&](Point a,Point b){unsigned n=std::max(1u,unsigned(std::ceil(std::hypot(b.x-a.x,b.z-a.z)/.6)));for(unsigned i=1;i<=n;++i){double t=double(i)/n;if(!push({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t}))return false;}return true;};
        if(!push(p[0]))return false;Point previous=p[0];
        for(unsigned i=1;i<5;++i){Point a=move(p[i],p[i-1],2),b=move(p[i],p[i+1],2);if(!line(previous,a))return false;for(unsigned j=1;j<=16;++j){double t=double(j)/16,u=1-t;if(!push({u*u*a.x+2*u*t*p[i].x+t*t*b.x,u*u*a.y+2*u*t*p[i].y+t*t*b.y,u*u*a.z+2*u*t*p[i].z+t*t*b.z}))return false;}previous=b;}
        if(!line(previous,p[5]))return false;
        for(unsigned i=0;i<count;++i){unsigned a=i?i-1:0,b=std::min(i+1,count-1);double dx=points[b].x-points[a].x,dz=points[b].z-points[a].z,n=std::hypot(dx,dz);points[i].dx=dx/n;points[i].dz=dz/n;}
        cycle=2*length+2*pi*c.lane_offset;return length>100&&cycle<3000;
    }
};
struct Core {
    Route route;std::array<Actor,WORLD_ROAD_TRAFFIC_ACTORS> actors={};std::array<Profile,6> profiles={};WorldRoadTrafficStats stats;
    bool initialized=false,validated=false,enabled=false;double clock=0;unsigned order=0;
    const char *validation_reason="not checked";mutable const char *floor_reason="not checked";unsigned validation_bank=0;
    double validation_station=0;Point validation_point;
    const Profile &profile(const Actor&a)const{return profiles[a.view.kind==WorldRoadTrafficKind::Walker?a.view.appearance:a.view.kind==WorldRoadTrafficKind::Rider?4:5];}
    bool protected_point(const Point&p,const Profile&f,bool encounter)const{
        const auto&c=route.config;const auto cap=capsule(p,f);
        auto disk=[&](const WorldRoadTrafficPoint&q,double r){return point_segment(q.x,q.z,cap.ax,cap.az,cap.bx,cap.bz)<sq(r+f.radius);};
        return disk(c.camp,c.camp_radius)||disk(c.town,c.town_radius)||(encounter&&(disk(c.encounter,c.encounter_radius)||disk(c.satchel,c.satchel_radius)));
    }
    bool floor(Point &p,const Profile&f,const WorldRoadTrafficQuery&q,double &pitch)const{
        auto fail=[&](const char *why){floor_reason=why;return false;};
        if(!q.ground||!q.clear)return fail("missing host queries");
        double center=0;
        if(!q.ground(p.x,p.z,p.y,center,q.context))return fail("center ground query");
        if(!std::isfinite(center)||std::abs(center-p.y)>1.5)return fail("center height differs from authored route");
        const double fore=std::max(.2,f.front),back=std::max(.2,f.rear);double hf=0,hb=0,hl=0,hr=0;
        if(!q.ground(p.x+p.dx*fore,p.z+p.dz*fore,center,hf,q.context))return fail("front ground query");
        if(!q.ground(p.x-p.dx*back,p.z-p.dz*back,center,hb,q.context))return fail("rear ground query");
        if(!q.ground(p.x+p.dz*f.radius,p.z-p.dx*f.radius,center,hl,q.context))return fail("left ground query");
        if(!q.ground(p.x-p.dz*f.radius,p.z+p.dx*f.radius,center,hr,q.context))return fail("right ground query");
        for(double h:{hf,hb,hl,hr})if(!std::isfinite(h)||std::abs(h-center)>.38)return fail("footprint height delta");
        if(std::abs(hr-hl)>.16)return fail("cross slope");
        pitch=-std::atan2(hf-hb,fore+back);if(std::abs(pitch)>.16)return fail("longitudinal pitch");
        p.y=center+.015;auto cap=capsule(p,f);
        if(!q.clear(cap.ax,cap.az,cap.bx,cap.bz,p.y,f.height,f.radius,q.context))return fail("collision capsule");
        floor_reason="ready";return true;
    }
    bool validate(const WorldRoadTrafficQuery&q){
        validated=false;validation_reason="empty route";if(!route.count)return false;
        for(unsigned bank=0;bank<6;++bank){
            validation_bank=bank;const auto&f=profiles[bank];validation_reason="invalid profile or lane width";
            if(f.radius<=0||f.radius+route.config.lane_offset>route.config.road_half_width+.02||f.height<.5||f.height>5||f.front<0||f.rear<0||f.front+f.rear>9||f.stride<.1||f.stride>12)return false;
            Point old;bool have=false;
            for(double s=0;s<route.cycle;s+=.65){
                Point p=route.sample(s);validation_station=s;validation_point=p;double pitch;
                if(!floor(p,f,q,pitch)){validation_reason=floor_reason;return false;}
                validation_reason="protected camp or town envelope";if(protected_point(p,f,false))return false;
                validation_reason="swept movement collision";if(have&&!q.clear(old.x,old.z,p.x,p.z,std::min(old.y,p.y),f.height,f.radius,q.context))return false;
                old=p;have=true;
            }
        }
        validated=true;validation_reason="ready";return true;
    }
    bool spawn(const WorldRoadTrafficFrame&frame){
        // Reserve a mixed near-road composition before placing the smaller
        // walkers. IDs remain stable; these are initial stations, never a
        // timer-driven respawn or a shortcut through the encounter barrier.
        actors={};
        auto initial_station=[&](unsigned id){
            const double arc=pi*route.config.lane_offset;
            auto station=[&](double distance,bool returning){distance=std::clamp(distance,4.,route.length-8.);return returning?2*route.length+arc-distance:distance;};
            if(id>=28){unsigned n=id-28;return station(n==0?38.:n==1?22.:route.length*.83,n==1);}
            if(id>=24){unsigned n=id-24;return station(n==0?18.:n==1?46.:route.length*(n==2?.60:.76),n%2!=0);}
            const unsigned n=id/2;const double far[]={.40,.47,.54,.61,.68,.75,.82,.89,.96};
            const double distance=n==0?6.:n==1?62.:n==2?86.:route.length*far[n-3];
            return station(distance,id%2!=0);
        };
        for(unsigned n=0;n<actors.size();++n){const unsigned i=n<7?24+n:n-7;auto&a=actors[i];a.view.id=i;a.view.kind=i<24?WorldRoadTrafficKind::Walker:i<28?WorldRoadTrafficKind::Rider:WorldRoadTrafficKind::Pullcart;a.view.appearance=i<24?(i/2+i%2)%4:0;const char*names[]={"Road traveler","Village messenger","Market craftsman","Road wayfarer"};a.view.name=i<24?names[a.view.appearance]:i<28?"Mounted courier":"Cart porter";const auto&f=profile(a);double station=initial_station(i);bool placed=false;
            for(unsigned attempt=0;attempt<2400;++attempt,station+=.7){auto p=route.sample(station);double pitch;if(protected_point(p,f,!frame.encounter_cleared)||!floor(p,f,frame.query,pitch))continue;auto cap=capsule(p,f);if(frame.player_feet_y+1.75>cap.low&&frame.player_feet_y<cap.high&&point_segment(frame.player_x,frame.player_z,cap.ax,cap.az,cap.bx,cap.bz)<sq(cap.r+.85))continue;bool clear=true;for(unsigned j=0;j<actors.size()&&clear;++j){const auto&v=actors[j].view;if(j==i||!v.active)continue;Point other{v.x,v.y,v.z,std::sin(v.yaw),std::cos(v.yaw)};clear=separate(cap,capsule(other,profile(actors[j])),1.0);}if(!clear)continue;a.station=std::fmod(station,route.cycle);a.view.x=p.x;a.view.y=p.y;a.view.z=p.z;a.view.yaw=std::atan2(p.dx,p.dz);a.view.pitch=pitch;a.view.active=true;a.view.radius=f.radius;a.view.front=f.front;a.view.rear=f.rear;a.view.height=f.height;a.idle=double(i)*.173;unsigned seed=(i+1)*2654435761u;seed^=seed>>16;a.pace=1.+(int(seed%9)-4)*.01;a.stance=(seed>>4)&1u;a.start_delay=double((seed>>8)%650)/1000.;a.phase=a.stance*.5;placed=true;break;}
            if(!placed)return false;
        }initialized=true;return true;
    }
    // A traveler may finish moving out of the player's comfort buffer. Its
    // complete fitted capsule must stay beyond the current closest-point
    // supporting plane throughout the actual route interval; a clear endpoint
    // alone cannot authorize a curved sweep through the player.
    bool outward_from_player(const Actor &a,const Capsule &next,double station,const WorldRoadTrafficFrame &frame)const{
        const auto &v=a.view;const auto &f=profile(a);
        const auto old=capsule({v.x,v.y,v.z,std::sin(v.yaw),std::cos(v.yaw)},f);
        if(frame.player_feet_y+1.75<=old.low+.05||frame.player_feet_y>=old.high-.05)return false;
        const double ex=old.bx-old.ax,ez=old.bz-old.az,n=ex*ex+ez*ez;
        const double t=n>1.e-18?std::clamp(((frame.player_x-old.ax)*ex+(frame.player_z-old.az)*ez)/n,0.,1.):0.;
        double nx=old.ax+ex*t-frame.player_x,nz=old.az+ez*t-frame.player_z;
        const double distance=std::hypot(nx,nz);
        if(distance<1.e-8||point_segment(frame.player_x,frame.player_z,next.ax,next.az,next.bx,next.bz)<=distance*distance+1.e-10)return false;
        nx/=distance;nz/=distance;
        const double from=a.station,advance=station-from;
        if(!(advance>0&&advance<=2.))return false;
        // Split at every source polyline/semicircle boundary. On a polyline
        // segment its center is linear and normalized tangent follows the short
        // unit-circle arc; on a turnaround its center is constant. Independent
        // minima give a conservative lower bound for the whole capsule sweep.
        std::array<double,96> cuts={};unsigned count=2;cuts[0]=from;cuts[1]=station;
        auto add=[&](double at){if(at<=from)at+=route.cycle;if(at>from&&at<station){if(count>=cuts.size())return false;cuts[count++]=at;}return true;};
        const double arc=pi*route.config.lane_offset;
        for(unsigned i=0;i<route.count;++i)if(!add(route.distances[i])||!add(2*route.length+arc-route.distances[i]))return false;
        if(!add(route.length+arc)||!add(route.cycle))return false;
        std::sort(cuts.begin(),cuts.begin()+count);
        auto center=[&](double at){at=std::fmod(at,route.cycle);if(at<0)at+=route.cycle;return route.center(at<=route.length?at:at<route.length+arc?route.length:at<=2*route.length+arc?2*route.length+arc-at:0.);};
        for(unsigned i=1;i<count;++i){
            if(cuts[i]-cuts[i-1]<1.e-10)continue;
            const auto p=route.sample(cuts[i-1]),q=route.sample(cuts[i]),cp=center(cuts[i-1]),cq=center(cuts[i]);
            const double start=std::atan2(p.dz,p.dx),end=std::atan2(q.dz,q.dx),sweep=std::remainder(end-start,2*pi);
            const double center_min=std::min(nx*(cp.x-frame.player_x)+nz*(cp.z-frame.player_z),nx*(cq.x-frame.player_x)+nz*(cq.z-frame.player_z));
            for(double longitudinal:{-std::max(0.,f.rear-f.radius),std::max(0.,f.front-f.radius)}){
                const double hx=longitudinal*nx-route.config.lane_offset*nz,hz=longitudinal*nz+route.config.lane_offset*nx;
                double orient_min=std::min(hx*p.dx+hz*p.dz,hx*q.dx+hz*q.dz);
                const double bottom=std::remainder(std::atan2(hz,hx)+pi-start,2*pi);
                if((sweep>=0&&bottom>=0&&bottom<=sweep)||(sweep<0&&bottom<=0&&bottom>=sweep))orient_min=-std::hypot(hx,hz);
                if(center_min+orient_min<distance-1.e-9)return false;
            }
        }
        return true;
    }
    bool candidate(unsigned id,Point &p,double station,const WorldRoadTrafficFrame&frame,bool check_player,double &pitch,bool &barrier)const{
        const auto&a=actors[id];const auto&f=profile(a);p=route.sample(station);barrier=protected_point(p,f,!frame.encounter_cleared);if(barrier)return false;
        auto cap=capsule(p,f);
        if(check_player&&frame.player_feet_y+1.75>cap.low+.05&&frame.player_feet_y<cap.high-.05&&point_segment(frame.player_x,frame.player_z,cap.ax,cap.az,cap.bx,cap.bz)<sq(cap.r+.37+.30)&&!outward_from_player(a,cap,station,frame))return false;
        for(unsigned j=0;j<actors.size();++j)if(j!=id&&actors[j].view.active){const auto&v=actors[j].view;Point other{v.x,v.y,v.z,std::sin(v.yaw),std::cos(v.yaw)};if(!separate(cap,capsule(other,profile(actors[j])),.35))return false;}
        if(!floor(p,f,frame.query,pitch))return false;
        const auto&v=a.view;auto old=capsule({v.x,v.y,v.z,std::sin(v.yaw),std::cos(v.yaw)},f);
        return frame.query.clear(old.ax,old.az,cap.ax,cap.az,std::min(v.y,p.y),f.height,f.radius,frame.query.context)&&frame.query.clear(old.bx,old.bz,cap.bx,cap.bz,std::min(v.y,p.y),f.height,f.radius,frame.query.context);
    }
    void animate(Actor&a,double distance,double dt,bool requested){
        const auto&f=profile(a);const bool moving=a.view.speed>.025;
        auto begin=[&](Actor::Mode m){a.mode=m;a.mode_age=0;};
        if(requested&&a.mode==Actor::Idle){a.breath_from=std::pow(std::sin(pi*a.idle),2);begin(Actor::Prepare);}
        if(requested&&a.mode==Actor::Stop){a.blend_from=a.mix;begin(Actor::Start);}
        if(!requested&&a.mode==Actor::Prepare){a.idle=std::asin(std::sqrt(std::clamp(a.mix,0.,1.)))/pi;begin(Actor::Idle);}
        if(!requested&&a.mode==Actor::Start){a.blend_from=a.mix;begin(Actor::Stop);}
        if(!requested&&!moving&&a.mode==Actor::Walk){a.plant_from=a.phase;a.plant_to=std::round(a.phase*2)/2;begin(Actor::Plant);}
        a.mode_age+=dt;a.idle+=dt/4.7;
        if(a.mode==Actor::Walk){a.phase+=distance/f.stride;a.mix=1;}
        else if(a.mode==Actor::Prepare){a.mix=a.breath_from*(1-smooth(a.mode_age/.18));if(a.mode_age>=.18){a.phase=a.stance*.5;a.blend_from=0;begin(Actor::Start);a.mix=0;}}
        else if(a.mode==Actor::Start){a.mix=a.blend_from+(1-a.blend_from)*smooth(a.mode_age/.20);if(a.mode_age>=.20){begin(Actor::Walk);a.mix=1;}}
        else if(a.mode==Actor::Plant){a.phase=a.plant_from+(a.plant_to-a.plant_from)*smooth(a.mode_age/.18);a.mix=1;if(a.mode_age>=.18){a.phase=a.plant_to;a.stance=unsigned(std::llround(a.phase*2))%2;a.blend_from=1;begin(Actor::Stop);}}
        else if(a.mode==Actor::Stop){a.mix=a.blend_from*(1-smooth(a.mode_age/.20));if(a.mode_age>=.20){a.mix=0;a.idle=std::asin(std::sqrt(std::clamp(a.mix,0.,1.)))/pi;begin(Actor::Idle);}}
        else a.mix=0;
        a.view.cycle_phase=a.phase-std::floor(a.phase);a.view.moving=moving;a.view.transitioning=a.mode!=Actor::Walk&&a.mode!=Actor::Idle;
        unsigned count=a.view.kind==WorldRoadTrafficKind::Rider?16:8;double phase=a.view.cycle_phase;
        if(a.mode==Actor::Walk||a.mode==Actor::Plant){double frame=phase*count;a.view.pose_a=unsigned(frame)%count;a.view.pose_b=(a.view.pose_a+1)%count;a.view.pose_weight=frame-std::floor(frame);}
        else if(a.mode==Actor::Start){a.view.pose_a=a.stance?count/2:0;a.view.pose_b=count;a.view.pose_weight=1-a.mix;}
        else if(a.mode==Actor::Stop){a.view.pose_a=a.stance?count/2:0;a.view.pose_b=count;a.view.pose_weight=1-a.mix;}
        else{a.view.pose_a=count;a.view.pose_b=count+1;a.view.pose_weight=a.mode==Actor::Prepare?a.mix:std::pow(std::sin(pi*a.idle),2);}
    }
    void update(const WorldRoadTrafficFrame&frame){
        if(!std::isfinite(frame.dt)||frame.dt<0||frame.dt>.25||!std::isfinite(frame.player_x)||!std::isfinite(frame.player_feet_y)||!std::isfinite(frame.player_z)){++stats.failures;return;}
        ++stats.updates;enabled=frame.enabled;if(!enabled||!validated)return;if(!initialized&&!spawn(frame)){++stats.failures;return;}if(frame.paused||frame.dt==0)return;
        unsigned steps=std::max(1u,unsigned(std::ceil(frame.dt/(1./90.))));double dt=frame.dt/steps;
        for(unsigned step=0;step<steps;++step){clock+=dt;++stats.simulation_steps;
            for(unsigned n=0;n<actors.size();++n){unsigned i=(n+order)%actors.size();auto&a=actors[i];auto&v=a.view;const auto&f=profile(a);const double desired=f.speed*a.pace;double target=desired;a.start_delay=std::max(0.,a.start_delay-dt);if(a.mode==Actor::Idle&&a.start_delay>0)target=0;
                const double arc=pi*route.config.lane_offset,t=std::fmod(a.station,route.cycle);if((t>route.length&&t<route.length+arc)||t>2*route.length+arc)target=std::min(target,.50);
                // Stop under braking, before a protected quest area or another
                // occupied capsule; phase follows only accepted translation.
                double horizon=std::max(.6,v.speed*v.speed/(2*1.45)+.5),pitch=0;Point p;bool barrier=false;
                if(!candidate(i,p,a.station+horizon,frame,true,pitch,barrier)){double lo=0,hi=horizon;for(unsigned k=0;k<7;++k){double mid=(lo+hi)*.5;bool b=false;if(candidate(i,p,a.station+mid,frame,true,pitch,b))lo=mid;else hi=mid;}target=std::min(target,std::sqrt(2*1.45*std::max(0.,lo-.08)));}
                const bool requested=target>.03;
                if(a.mode==Actor::Idle||a.mode==Actor::Prepare||a.mode==Actor::Start||a.mode==Actor::Plant||a.mode==Actor::Stop)target=0;
                double rate=target>v.speed?.75:1.65;double speed=v.speed+std::clamp(target-v.speed,-rate*dt,rate*dt),distance=speed*dt;bool blocked=false;
                if(distance>0&&!candidate(i,p,a.station+distance,frame,true,pitch,barrier)){distance=speed=0;blocked=true;}
                v.yielding=blocked||target<desired*.50;v.barrier_wait=barrier&&v.yielding;if(blocked)++stats.blocked;
                if(distance>0){double oldx=v.x,oldz=v.z;a.station=std::fmod(a.station+distance,route.cycle);v.x=p.x;v.y=p.y;v.z=p.z;v.yaw=std::atan2(p.dx,p.dz);v.pitch=pitch;distance=std::hypot(v.x-oldx,v.z-oldz);v.travelled+=distance;stats.travelled+=distance;}v.speed=speed;if(v.yielding)++stats.actor_yields;auto cap=capsule({v.x,v.y,v.z,std::sin(v.yaw),std::cos(v.yaw)},f);if(frame.player_feet_y+1.75>cap.low&&frame.player_feet_y<cap.high){double clearance=std::sqrt(point_segment(frame.player_x,frame.player_z,cap.ax,cap.az,cap.bx,cap.bz))-cap.r-.37;stats.minimum_player_clearance=std::min(stats.minimum_player_clearance,clearance);if(v.yielding&&clearance<2)++stats.player_yields;}animate(a,distance,dt,requested&&!blocked);
            }order=(order+1)%actors.size();
        }
    }
    bool camera_clear(double ax,double ay,double az,double bx,double by,double bz,double radius,double half_height)const{
        if(!enabled||!validated||!initialized)return true;
        for(double value:{ax,ay,az,bx,by,bz,radius,half_height})if(!std::isfinite(value))return false;
        if(radius<=0||radius>2||half_height<0||half_height>2)return false;
        for(const auto &a:actors){const auto &v=a.view;if(!v.active)continue;
            const auto body=capsule({v.x,v.y,v.z,std::sin(v.yaw),std::cos(v.yaw)},profile(a));
            if(!camera_capsule_clear(body,ax,ay,az,bx,by,bz,radius,half_height))return false;
        }
        return true;
    }
    bool blocks(double ax,double az,double bx,double bz,double feet,double height,double radius)const{
        if(!enabled||!validated||!initialized)return false;for(double x:{ax,az,bx,bz,feet,height,radius})if(!std::isfinite(x))return true;if(height<=0||radius<=0)return true;
        for(const auto&a:actors){const auto&v=a.view;if(!v.active)continue;auto c=capsule({v.x,v.y,v.z,std::sin(v.yaw),std::cos(v.yaw)},profile(a));if(feet+height<=c.low+.05||feet>=c.high-.05)continue;double old=point_segment(ax,az,c.ax,c.az,c.bx,c.bz),next=point_segment(bx,bz,c.ax,c.az,c.bx,c.bz),r=sq(radius+c.r);if(old<r){double dx=c.bx-c.ax,dz=c.bz-c.az,n=dx*dx+dz*dz,t=n>1e-18?std::clamp(((ax-c.ax)*dx+(az-c.az)*dz)/n,0.,1.):0,ox=ax-c.ax-dx*t,oz=az-c.az-dz*t;if(next>old+1e-12&&ox*(bx-ax)+oz*(bz-az)>=-1e-12)continue;return true;}if(segment_distance(ax,az,bx,bz,c.ax,c.az,c.bx,c.bz)<r)return true;}return false;
    }
};
}
#endif
