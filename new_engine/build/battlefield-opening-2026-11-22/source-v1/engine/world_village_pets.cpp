#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_village_pets.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
constexpr double pi=3.14159265358979323846;
struct Point {double x,z;};
struct Route {Point points[4];};
constexpr Route routes[]={
    {{{21.2,44.8},{21.2,48.2},{22.4,48.2},{22.4,44.8}}},
    {{{-20.8,45.5},{-20.8,48.5},{-22,48.5},{-22,45.5}}},
    {{{46,30},{46,40.5},{47.2,40.5},{47.2,30}}},
    {{{-46,30},{-46,40.5},{-47.6,40.5},{-47.6,30}}},
    {{{19,180},{19,192},{20.6,192},{20.6,180}}},
    {{{-19,180},{-19,192},{-20.6,192},{-20.6,180}}}
};
constexpr const char *names[]={"Copper","Mallow","Pip","Moss","Juniper","Pebble","Bramble","Barley","Cinder","Rook","Alder","Wren"};
struct Pet {
    unsigned species=0,route=0,group=0,a=0,b=0;Point point={};
    double offset=0;float y=24,yaw=0,speed=0,phase=0,blend=0;
    bool patrol=false,walking=false,seated=false,transitioning=false;
};
struct Group {WorldModelAsset *asset=nullptr;};
struct Blend {
    WorldModelPoseBlend *batch=nullptr;
    std::array<WorldModelInstance,world_village_pet_count> next={},uploaded={};
    std::array<float,world_village_pet_count> weights={},uploaded_weights={};
    std::size_t count=0,uploaded_count=0;
};
struct Uniforms {unsigned long long epoch=0;
    GLuint program=0;GLint offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,prop_lod=-1;
};
double length(Point a,Point b){return std::hypot(a.x-b.x,a.z-b.z);}
double smooth(double t){t=std::clamp(t,0.,1.);return t*t*t*(t*(t*6.-15.)+10.);}
double ramp(double d,double speed){return std::min(.55,d/(2*speed));}
double duration(double d,double speed){return d/speed+ramp(d,speed);}
double distance(double t,double d,double speed){
    const double r=ramp(d,speed),total=duration(d,speed);
    if(t<r)return speed*t*t/(2*r);
    if(t>total-r){const double left=total-t;return d-speed*left*left/(2*r);}
    return speed*(t-r*.5);
}
void cycle(Pet &p,double phase,bool walk){
    const unsigned count=walk?8:4,base=p.species*16+(walk?0:p.seated?12:8);
    phase-=std::floor(phase);const double f=phase*count;const unsigned index=unsigned(std::floor(f));
    p.group=p.a=base+index;p.b=base+(index+1)%count;p.blend=float(f-index);p.phase=float(phase);
}
void pose(Pet &p,double time){
    p.walking=p.transitioning=false;p.speed=0;
    if(!p.patrol){cycle(p,(time+p.offset)/(p.species?4.:8.),false);return;}
    const Route &route=routes[p.route];const double speed=p.species?.55:.32,dwell=p.species?2.:2.4;
    double period=0;for(unsigned i=0;i<4;i++)period+=duration(length(route.points[i],route.points[(i+1)%4]),speed)+dwell;
    double phase=std::fmod(time+p.offset,period);if(phase<0)phase+=period;
    for(unsigned i=0;i<4;i++){
        const Point a=route.points[i],b=route.points[(i+1)%4];const double d=length(a,b),total=duration(d,speed),r=ramp(d,speed),fade=.24;
        p.yaw=float(std::atan2(b.x-a.x,b.z-a.z));
        if(phase<total){
            const double travelled=distance(phase,d,speed),t=travelled/d,handover=distance(fade,d,speed),core=d-2*handover;
            const double stride=p.species?.64:.4,cycles=std::max(.5,std::round(core/stride*2)*.5);
            const bool opposite=(unsigned(std::llround(cycles*2))%2)!=0;
            p.point={a.x+(b.x-a.x)*t,a.z+(b.z-a.z)*t};p.walking=true;
            p.speed=float(speed*std::min({1.,phase/r,(total-phase)/r}));
            if(phase<fade||phase>total-fade){
                const bool to_idle=phase>total-fade;const unsigned walk=p.species*16,idle=walk+8;
                p.group=32+p.species*3+(to_idle?(opposite?2:1):0);
                p.a=to_idle?walk+(opposite?4:0):idle;p.b=to_idle?idle:walk;
                p.blend=float(smooth(to_idle?(phase-total+fade)/fade:phase/fade));p.phase=to_idle&&opposite?.5f:0;p.transitioning=true;
            }else cycle(p,std::clamp((travelled-handover)/core,0.,1.)*cycles,true);
            return;
        }
        phase-=total;
        if(phase<dwell){
            p.point=b;const Point next=route.points[(i+2)%4];const double angle=std::atan2(next.x-b.x,next.z-b.z);
            p.yaw=float(p.yaw+std::remainder(angle-p.yaw,2*pi)*smooth((phase-.15)/(dwell-.3)));
            cycle(p,smooth(phase/dwell),false);return;
        }
        phase-=dwell;
    }
}
}
struct WorldVillagePets {
    std::array<Pet,world_village_pet_count> pets={};
    std::array<Group,world_village_pet_pose_count> groups={};
    std::array<Blend,world_village_pet_blend_count> blends={};
    std::array<Uniforms,2> uniforms={};unsigned next_uniform=0;
    double x=0,z=0,anchor_x=0,anchor_z=0;WorldVillagePetsStats stats;
    Uniforms &locations(GLuint program){
        for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;
        auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
        u.offset=glGetUniformLocation(program,"uOffset");u.anchor=glGetUniformLocation(program,"uWorldAnchor");
        u.grass=glGetUniformLocation(program,"uGrass");u.vegetation=glGetUniformLocation(program,"uVegetation");
        u.lod=glGetUniformLocation(program,"uLodLevel");u.wind=glGetUniformLocation(program,"uWind");
        u.height=glGetUniformLocation(program,"uTreeHeight");u.reactive=glGetUniformLocation(program,"uActorReactive");
        u.prop_lod=glGetUniformLocation(program,"uPropLodRange");return u;
    }
};
WorldVillagePets *world_village_pets_load(const char *directory,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    if(!directory||!directory[0])return nullptr;
    auto *p=new(std::nothrow)WorldVillagePets;if(!p)return nullptr;
    world_atlas_village_origin(&p->x,&p->z);p->anchor_x=std::floor(p->x/128)*128;p->anchor_z=std::floor(p->z/128)*128;
    unsigned index=0;
    for(const char *species:{"cat","dog"})for(const char *mode:{"walk","idle","rest"})for(unsigned frame=0;frame<(!std::strcmp(mode,"walk")?8u:4u);frame++){
        char path[1024]={};const int n=std::snprintf(path,sizeof(path),"%s/village_pet_%s_%s_%02u.glb",directory,species,mode,frame);
        if(n<0||std::size_t(n)>=sizeof(path)||(p->groups[index++].asset=world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION))==nullptr){
            if(error&&capacity)std::snprintf(error,capacity,"Missing village pet pose: %s",path);
            world_village_pets_free(p);return nullptr;
        }
    }
    auto configure=[&](unsigned slot,unsigned a,unsigned b){
        auto &g=p->blends[slot];g.batch=world_model_pose_blend_create(world_village_pet_count,WORLD_MODEL_POSE_BLEND_VERSION);
        return g.batch&&world_model_pose_blend_set_targets(g.batch,p->groups[a].asset,p->groups[b].asset);
    };
    for(unsigned species=0;species<2;species++){
        for(unsigned mode=0;mode<3;mode++){
            const unsigned base=species*16+(mode==0?0:mode==1?8:12),count=mode==0?8:4;
            for(unsigned f=0;f<count;f++)if(!configure(base+f,base+f,base+(f+1)%count)){
                if(error&&capacity)std::snprintf(error,capacity,"Incompatible village pet pose cycle");
                world_village_pets_free(p);return nullptr;
            }
        }
        const unsigned base=species*16,slot=32+species*3;
        if(!configure(slot,base+8,base)||!configure(slot+1,base,base+8)||!configure(slot+2,base+4,base+8)){
            if(error&&capacity)std::snprintf(error,capacity,"Incompatible village pet idle transition");
            world_village_pets_free(p);return nullptr;
        }
    }
    constexpr Point stations[]={{20.8,42},{-20.8,40.8},{45.8,26},{-45.8,26},{23,186},{-23,186}};
    for(unsigned i=0;i<world_village_pet_count;i++){
        auto &a=p->pets[i];a.species=i/6;a.patrol=i%6>=3;a.seated=a.species&&!a.patrol;
        a.route=(i/6)*3+(i%6>=3?i%6-3:0);a.point=a.patrol?routes[a.route].points[0]:stations[(i/6)*3+i%6];
        a.offset=i*3.713;a.yaw=float(i%2?pi*.5:-pi*.5);a.y=world_atlas_height(p->x+a.point.x,p->z+a.point.z);
    }
    p->stats.pets=12;p->stats.cats=6;p->stats.dogs=6;p->stats.loaded_pose_assets=index;return p;
}
void world_village_pets_update(WorldVillagePets *p,double x,double y,double z,float time,float dt){
    if(!p||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(time)||time<0||time>1e7||!std::isfinite(dt)||dt<0||dt>1)return;
    p->stats.walking=p->stats.idling=p->stats.visible=p->stats.draw_groups=0;
    for(auto &g:p->blends)g.count=0;
    for(auto &a:p->pets){
        pose(a,time);if(a.walking)p->stats.walking++;else p->stats.idling++;
        const double wx=p->x+a.point.x,wz=p->z+a.point.z;if(std::hypot(wx-x,wz-z)>150)continue;
        auto &g=p->blends[a.group];g.weights[g.count]=a.blend;
        g.next[g.count++]={float(wx-p->anchor_x),a.y,float(wz-p->anchor_z),a.yaw,1};p->stats.visible++;
    }
    for(auto &g:p->blends){
        const bool changed=g.count!=g.uploaded_count||std::memcmp(g.next.data(),g.uploaded.data(),g.count*sizeof(WorldModelInstance))||std::memcmp(g.weights.data(),g.uploaded_weights.data(),g.count*sizeof(float));
        if(changed&&world_model_pose_blend_upload(g.batch,g.next.data(),g.weights.data(),g.count)){
            std::copy_n(g.next.begin(),g.count,g.uploaded.begin());std::copy_n(g.weights.begin(),g.count,g.uploaded_weights.begin());g.uploaded_count=g.count;p->stats.uploads++;
        }
        if(g.uploaded_count)p->stats.draw_groups++;
    }
    p->stats.updates++;
}
void world_village_pets_draw(WorldVillagePets *p,unsigned program,double x,double y,double z,bool shadow){
    if(!p||!program||!p->stats.draw_groups)return;
    auto &u=p->locations(program);glUseProgram(program);glUniform3f(u.offset,float(p->anchor_x-x),float(-y),float(p->anchor_z-z));
    glUniform2f(u.anchor,float(p->anchor_x),float(p->anchor_z));glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);
    glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,1);glUniform3f(u.prop_lod,0,0,0);
    glUniform1f(u.reactive,shadow?0.f:.85f);for(auto &g:p->blends)if(g.uploaded_count)world_model_pose_blend_draw(g.batch,program);glUniform1f(u.reactive,0);
}
WorldVillagePetsStats world_village_pets_stats(const WorldVillagePets *p){return p?p->stats:WorldVillagePetsStats{};}
bool world_village_pets_pet(const WorldVillagePets *p,std::size_t index,WorldVillagePet *out){
    if(!p||!out||index>=p->pets.size())return false;
    const auto &a=p->pets[index];out->name=names[index];out->species=a.species?"Dog":"Cat";
    out->x=p->x+a.point.x;out->z=p->z+a.point.z;out->y=a.y;out->yaw=a.yaw;out->speed=a.speed;out->cycle_phase=a.phase;
    out->pose_a=a.a;out->pose_b=a.b;out->pose_blend=a.blend;out->walking=a.walking;out->seated=a.seated;out->transitioning=a.transitioning;return true;
}
void world_village_pets_free(WorldVillagePets *p){
    if(!p)return;
    for(auto &g:p->blends)if(g.batch)world_model_pose_blend_free(g.batch);
    for(auto &g:p->groups)if(g.asset)world_model_free(g.asset);
    delete p;
}
