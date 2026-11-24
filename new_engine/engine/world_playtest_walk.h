#ifndef VALWOOD_WORLD_PLAYTEST_WALK_H
#define VALWOOD_WORLD_PLAYTEST_WALK_H
#include "engine/world_physics.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace world_playtest_walk {
constexpr float radius=.37f,half_height=.50f;
constexpr double eye_height=1.75,max_step=.6,max_drop=.8;
struct Bounds {double xmin=0,xmax=0,zmin=0,zmax=0,ymin=0,ymax=0;};
struct Spawn {double x=0,feet=0,z=0;float yaw=0,pitch=-.03f;};

// One explicitly bounded, source-triangle-supported walking sector. The
// reference feet select the current storey; roofs above it are never a floor.
class Support {
    std::unique_ptr<world_physics::World> world_;
    Bounds bounds_;
    Spawn spawn_;
    bool floor_at(double x,double z,double low,double high,float &height)const {
        if(!contains(x,z)||!std::isfinite(low)||!std::isfinite(high)||high<=low)return false;
        low=std::max(low,bounds_.ymin-.02);high=std::min(high,bounds_.ymax+.02);
        if(high<=low)return false;
        auto probe=[&](double px,double pz,float &value){
            if(!contains(px,pz))return false;
            double from=high;
            for(unsigned layer=0;layer<8&&from>low;layer++){
                world_physics::Hit hit;
                if(!world_->raycast({px,from,pz},{0,low-from,0},hit)||!hit.hit)return false;
                if(hit.normal.y>=.65&&hit.position.y>=bounds_.ymin-.001&&hit.position.y<=bounds_.ymax+.001){
                    value=float(hit.position.y);return true;
                }
                // Detailed stone risers have a narrow sloping bevel in front
                // of their tread. Continue below that source face to find the
                // actual tread/previous step instead of treating the bevel as
                // a missing floor. Body sweeps still retain it as an obstacle.
                from=std::min(from-.002,hit.position.y-.002);
            }
            return false;
        };
        if(probe(x,z,height))return true;
        // A ray exactly on a tread/riser seam can report the vertical face
        // instead of its coplanar tread. Resolve that 2mm ambiguity on actual
        // neighbouring source faces; never use a distant terrain fallback.
        float value=0;bool found=false;
        for(unsigned i=0;i<4;i++){
            const double dx=i==0?.002:i==1?-.002:0,dz=i==2?.002:i==3?-.002:0;
            if(probe(x+dx,z+dz,value)){height=found?std::max(height,value):value;found=true;}
        }
        return found;
    }
    bool blocking(world_physics::Vec3 center,world_physics::Vec3 delta)const {
        if(std::hypot(std::hypot(delta.x,delta.y),delta.z)<1.e-8)return false;
        world_physics::Hit hit;
        return !world_->cast_capsule_movement(center,radius,half_height,delta,hit)||
            (hit.hit&&hit.fraction<.999f&&hit.normal.y<.65f);
    }
public:
    bool loaded()const{return bool(world_);}
    const Bounds &bounds()const{return bounds_;}
    const Spawn &spawn()const{return spawn_;}
    world_physics::Stats stats()const{return world_?world_->stats():world_physics::Stats{};}
    bool contains(double x,double z)const {
        return world_&&std::isfinite(x)&&std::isfinite(z)&&x>=bounds_.xmin+radius&&
            x<=bounds_.xmax-radius&&z>=bounds_.zmin+radius&&z<=bounds_.zmax-radius;
    }
    bool sample(double x,double z,double referenceFeet,float &height,float *tread=nullptr)const {
        if(!std::isfinite(referenceFeet))return false;
        // Three metres below the reference keeps ordinary jumps supported;
        // horizontal movement separately rejects drops greater than .8m.
        if(!contains(x,z))return false;
        float center=0;const bool centered=floor_at(x,z,referenceFeet-3.0,referenceFeet+max_step+.01,center);
        // Use the actual rounded capsule footprint. The old eight floor rays
        // extended beyond its radius and took the whole neighbouring tread
        // height; a trim strip beside a clear doorway could therefore lift the
        // floor and make the controller reject its own current feet position.
        // A source tread at the centre or actual contact establishes the
        // current storey; the downcast anticipates only a genuine body contact,
        // including the rounded support height while crossing a stair edge.
        const double top=referenceFeet+max_step+.01,bottom=referenceFeet-3.0;
        world_physics::Hit contact;
        const bool capsule_contact=world_->cast_capsule_movement({x,top+radius+half_height,z},radius,half_height,{0,bottom-top,0},contact)&&
           contact.hit&&contact.normal.y>.01f&&contact.fraction>0;
        // Detailed stairs have genuine seams between their stones: a centre
        // ray can see a much lower floor through one. Prefer the real tread at
        // the capsule's contact, close to its source contact height, so that a
        // lower storey cannot replace the stone physically supporting us.
        float contact_tread=0;
        bool contact_anchor=false;
        if(capsule_contact&&contact.position.y<=referenceFeet+max_step+.002){
            auto contact_floor=[&](double px,double pz){
                return std::hypot(px-x,pz-z)<=radius+.001&&floor_at(px,pz,
                    std::max(bottom,contact.position.y-.10),std::min(top,contact.position.y+.10),contact_tread);
            };
            contact_anchor=contact_floor(contact.position.x,contact.position.z);
            // A contact exactly on a stone joint can have the same ray seam
            // ambiguity. Search locally inside the real capsule footprint;
            // these rays select a source tread, never the supported height.
            for(double offset:{.02,.05})for(unsigned i=0;i<8&&!contact_anchor;i++){
                const double angle=i*.7853981633974483;
                contact_anchor=contact_floor(contact.position.x+offset*std::cos(angle),contact.position.z+offset*std::sin(angle));
            }
        }
        if(!centered&&!contact_anchor)return false;
        const float anchor=contact_anchor?contact_tread:center;
        float supported=center;bool rounded_support=false;
        if(capsule_contact){
            const double candidate=top+(bottom-top)*contact.fraction;
            if(candidate>=anchor-(contact_anchor?radius+.10:.01)&&candidate-anchor<=(contact_anchor?.10:max_drop+.002)&&
               contact.position.y<=referenceFeet+max_step+.002&&candidate<=referenceFeet+max_step+.002){
                supported=contact_anchor?float(candidate):std::max(center,float(candidate));rounded_support=true;
            }
        }
        if(!centered&&!rounded_support)return false;
        height=supported;if(tread)*tread=rounded_support?anchor:center;return true;
    }
    bool cast_capsule(world_physics::Vec3 center,float r,float half,
                      world_physics::Vec3 delta,world_physics::Hit &hit)const {
        return world_&&world_->cast_capsule(center,r,half,delta,hit);
    }
    bool cast_capsule_movement(world_physics::Vec3 center,float r,float half,
                               world_physics::Vec3 delta,world_physics::Hit &hit)const {
        return world_&&world_->cast_capsule_movement(center,r,half,delta,hit);
    }
    bool raycast(world_physics::Vec3 origin,world_physics::Vec3 delta,world_physics::Hit &hit)const {
        return world_&&world_->raycast(origin,delta,hit);
    }
    bool can_move(double ax,double az,double bx,double bz,double feet,float &nextFloor)const {
        if(!contains(ax,az)||!contains(bx,bz)||!std::isfinite(feet))return false;
        const double distance=std::hypot(bx-ax,bz-az);
        if(!std::isfinite(distance)||distance>8)return false;
        float current=0,current_tread=0;
        if(!sample(ax,az,feet,current,&current_tread)||feet<current-.10||feet-current>2.8)return false;
        const bool grounded=std::abs(feet-current)<.15;
        const unsigned count=std::max(1u,unsigned(std::ceil(distance/.125)));
        double x=ax,z=az,bodyFeet=feet;
        for(unsigned step=1;step<=count;step++) {
            const double fraction=double(step)/count,nx=ax+(bx-ax)*fraction,nz=az+(bz-az)*fraction;
            float target=0,target_tread=0;if(!sample(nx,nz,bodyFeet,target,&target_tread))return false;
            const double rise=double(target)-current;
            if(rise>max_step+.002||rise< -max_drop-.002)return false;
            const double tread_rise=double(target_tread)-current_tread;
            if(grounded&&(tread_rise>max_step+.002||tread_rise< -max_drop-.002))return false;
            // A step-up/forward/down cast clears real stair risers while the
            // upward cast retains headroom. Side walls and low ceilings block.
            const double up=grounded&&rise>.002?rise+.025:0;
            if(up&&blocking({x,bodyFeet+.9,z},{0,up,0}))return false;
            if(blocking({x,bodyFeet+.9+up,z},{nx-x,0,nz-z}))return false;
            const double endFeet=grounded?target:bodyFeet;
            if(blocking({nx,bodyFeet+.9+up,nz},{0,endFeet-bodyFeet-up,0}))return false;
            x=nx;z=nz;current=target;current_tread=target_tread;bodyFeet=endFeet;
        }
        nextFloor=current;return true;
    }
    bool load(const char *path,std::string &error) {
        auto fail=[&](const char *message){error=message;return false;};
        if(!path||!*path)return fail("Choose a playtest walking sector");
        const auto settings_path=std::filesystem::u8path(path);
        std::ifstream file(settings_path,std::ios::binary);if(!file)return fail("Cannot open playtest walking settings");
        file.seekg(0,std::ios::end);const auto size=file.tellg();
        if(size<0||size>8192)return fail("Oversized playtest walking settings");
        file.seekg(0);std::string row;std::getline(file,row);if(!row.empty()&&row.back()=='\r')row.pop_back();
        if(row!="VALWOOD_PLAYTEST_WALK 1")return fail("Unsupported playtest walking settings");
        Bounds b;Spawn s;std::string physics;unsigned seen=0;
        while(std::getline(file,row)) {
            if(row.size()>1024||row.find('\0')!=std::string::npos)return fail("Invalid playtest walking record");
            std::istringstream in(row);std::string kind,extra;if(!(in>>kind)||kind[0]=='#')continue;
            bool ok=false;
            if(kind=="physics"&&!(seen&1)){ok=bool(in>>std::quoted(physics));seen|=1;}
            else if(kind=="bounds"&&!(seen&2)){ok=bool(in>>b.xmin>>b.xmax>>b.zmin>>b.zmax>>b.ymin>>b.ymax);seen|=2;}
            else if(kind=="spawn"&&!(seen&4)){ok=bool(in>>s.x>>s.feet>>s.z>>s.yaw>>s.pitch);seen|=4;}
            if(!ok||(in>>extra))return fail("Malformed or duplicate playtest walking record");
        }
        auto finite=[](double value){return std::isfinite(value)&&std::abs(value)<=1.e6;};
        if(seen!=7||physics.empty()||physics.size()>95||physics.find("..")!=std::string::npos||
           !std::all_of(physics.begin(),physics.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-'||c=='.';})||
           std::filesystem::path(physics).extension()!=".physics")return fail("Invalid relative playtest collision filename");
        for(double value:{b.xmin,b.xmax,b.zmin,b.zmax,b.ymin,b.ymax,s.x,s.feet,s.z,double(s.yaw),double(s.pitch)})
            if(!finite(value))return fail("Nonfinite/outside playtest walking coordinates");
        if(b.xmax-b.xmin<1||b.xmax-b.xmin>512||b.zmax-b.zmin<1||b.zmax-b.zmin>512||
           b.ymax<=b.ymin||b.ymax-b.ymin>128||std::abs(s.pitch)>1.570797)return fail("Invalid playtest walking bounds");
        const auto collision=settings_path.parent_path()/physics;
        world_physics::Document document;if(!world_physics::load(collision.u8string().c_str(),document,error))return false;
        for(const auto &body:document.bodies)if(body.motion!=world_physics::Motion::Static||body.sensor)
            return fail("Playtest collision must be static nonsensor source geometry");
        Support next;next.bounds_=b;next.spawn_=s;next.world_=std::make_unique<world_physics::World>();
        if(!next.world_->configure(document,error))return false;
        float floor=0;if(!next.sample(s.x,s.z,s.feet,floor)||std::abs(floor-s.feet)>.04||
           next.blocking({s.x,s.feet+.9,s.z},{0,.01,0}))return fail("Playtest spawn lacks clear authored floor/headroom");
        next.spawn_.feet=floor;
        world_=std::move(next.world_);bounds_=next.bounds_;spawn_=next.spawn_;error.clear();return true;
    }
};
}
#endif
