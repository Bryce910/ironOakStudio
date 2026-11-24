#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_castle_dragons.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include "engine/world_castle_visibility.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
namespace {
struct WorldDragonClip {const char *name;unsigned base,count;float seconds;bool loop;};
struct WorldDragonPoseDef {float mouth[3],direction[3],feet_y,minimum[3],maximum[3];};
#include "../projects/valwood_world/models/castle_dragons/dragon_metadata.inc"
constexpr unsigned poses=VALWOOD_DRAGON_POSE_COUNT,transitions=8,batches=poses+transitions;
static_assert(poses==50&&sizeof(world_dragon_clips)/sizeof(world_dragon_clips[0])==6,"Dragon pose-bank/controller contract changed");
constexpr double pi=3.14159265358979323846;
constexpr double flying_seconds=72.,approach_seconds=20.,departure_seconds=20.,cross_seconds=.65,idle_seconds=10.;
constexpr double landing_seconds=160./30.,breath_seconds=170./30.,takeoff_seconds=3.;
constexpr double cycle=flying_seconds+approach_seconds+landing_seconds+breath_seconds+takeoff_seconds+2*idle_seconds+departure_seconds+6*cross_seconds;
constexpr double angular_speed=(4*pi/3)/flying_seconds;
constexpr float fade_center=650.f,fade_half=50.f;
constexpr unsigned transition_from[transitions]={0,19,20,33,20,41,0,49};
constexpr unsigned transition_to[transitions]={10,20,24,20,34,0,43,0};
struct Selection {unsigned slot=0;float weight=0;};
struct Batch {WorldModelPoseBlend *gpu=nullptr;WorldModelInstance instance={};float weight=-1;bool uploaded=false;};
struct Bank {std::array<WorldModelAsset *,poses> assets={};std::array<Batch,batches> blend={};Selection selected;};
struct Uniforms {unsigned long long epoch=0;unsigned program=0;int offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,prop=-1;};
double smooth(double t){t=std::clamp(t,0.,1.);return t*t*(3.-2.*t);}
double smoother(double t){t=std::clamp(t,0.,1.);return t*t*t*(t*(t*6.-15.)+10.);}
double hermite(double a,double b,double velocity_a,double velocity_b,double seconds,double t){const double t2=t*t,t3=t2*t;return(2*t3-3*t2+1)*a+(t3-2*t2+t)*velocity_a*seconds+(-2*t3+3*t2)*b+(t3-t2)*velocity_b*seconds;}
Selection clip_selection(unsigned index,double t){
    const auto &c=world_dragon_clips[index];
    t=c.loop?t-std::floor(t):std::clamp(t,0.,1.);
    const double f=t*(c.loop?c.count:c.count-1);const unsigned key=std::min(c.count-1,unsigned(f));
    return{c.base+key,float(f-key)};
}
void targets(unsigned slot,unsigned &a,unsigned &b){
    if(slot>=poses){a=transition_from[slot-poses];b=transition_to[slot-poses];return;}
    for(const auto &clip:world_dragon_clips)if(slot>=clip.base&&slot<clip.base+clip.count){a=slot;b=clip.base+(clip.loop?(slot-clip.base+1)%clip.count:std::min(slot-clip.base+1,clip.count-1));return;}
    a=b=0;
}
double wing_clearance(const Selection &selection){
    unsigned a,b;targets(selection.slot,a,b);
    return std::max(0.,-double(world_dragon_pose_metadata[a].minimum[1])*(1-selection.weight)-double(world_dragon_pose_metadata[b].minimum[1])*selection.weight);
}
bool support(unsigned variant,double &floor){
    double ox,oz;world_atlas_village_origin(&ox,&oz);double lo=1.e20,hi=-1.e20;
    // Four feet occupy the compact centre, not the complete wing/tail AABB.
    // A single terrain floor is retained through every clip, never aligning
    // each pose's lowest wing vertex and making the whole animal bob.
    for(int z=-1;z<=1;z++)for(int x=-1;x<=1;x++){
        const double wx=ox+world_castle_dragon_roosts[variant][0]+x*2.,wz=oz+world_castle_dragon_roosts[variant][1]+z*2.;
        const float h=world_atlas_height(wx,wz);
        if(!std::isfinite(h)||h<=0||world_atlas_local_water(wx,wz,nullptr,nullptr))return false;
        lo=std::min(lo,double(h));hi=std::max(hi,double(h));
    }
    if(hi-lo>.45)return false;
    floor=hi+.015;return true;
}
bool sample(unsigned variant,double time,double floor,WorldCastleDragon &a,Selection &selection){
    if(variant>=WORLD_CASTLE_DRAGON_COUNT||!std::isfinite(time)||time<0||time>1.e9||!std::isfinite(floor))return false;
    a={};a.variant=variant;a.ground_y=float(floor);
    const double local=std::fmod(time+variant*cycle*.5,cycle);a.cycle_time=local;
    double t=local,theta=0,altitude=0;selection={};
    const double radius=std::abs(world_castle_dragon_roosts[variant][0]);
    auto transition=[&](unsigned index,double phase){selection={poses+index,float(smooth(phase))};a.transition=true;};
    auto fly_pose=[&](double clock){selection=clip_selection(0,clock/2.);};
    if(t<flying_seconds){
        a.stage=WorldCastleDragonStage::Flight;theta=pi/3+angular_speed*t;fly_pose(t);
        // Advance the native two-second flap clock through both cross-clip
        // segments: flight phase0 -> air phase.125, then air phase.875 ->
        // flight phase1. Their linear weights preserve phase velocity. The
        // sustained native exhale head is overlaid on each flying body pose.
        if(t>=20.&&t<24.){
            a.stage=WorldCastleDragonStage::AerialBreath;
            if(t<20.25){selection={poses+6,float((t-20.)/.25)};a.transition=true;}
            else if(t<23.75)selection=clip_selection(5,(t-20.)/2.);
            else{selection={poses+7,float((t-23.75)/.25)};a.transition=true;}
            a.breath=float(smooth((t-20.1)/.3)*(1-smooth((t-23.6)/.3)));
        }
        altitude=104.+8.*std::sin(2*theta)-floor;
    }else if((t-=flying_seconds)<approach_seconds){
        a.stage=WorldCastleDragonStage::Approach;const double u=t/approach_seconds;
        theta=hermite(5*pi/3,2*pi,angular_speed,0,approach_seconds,u);fly_pose(t);
        altitude=(104.+8.*std::sin(2*theta)-floor)*(1-smoother(u))+14.*smoother(u);
    }else if((t-=approach_seconds)<cross_seconds){
        a.stage=WorldCastleDragonStage::Landing;theta=2*pi;altitude=14.;transition(0,t/cross_seconds);
    }else if((t-=cross_seconds)<landing_seconds){
        a.stage=WorldCastleDragonStage::Landing;theta=2*pi;const double u=t/landing_seconds;selection=clip_selection(1,u);
        // Native fore/hind contact settles by44% of Landing; the remaining
        // source clip folds the wings while the planted root stays still.
        altitude=14.*(1-smoother(u/.44));a.grounded=u>=.44;
    }else if((t-=landing_seconds)<cross_seconds){
        a.stage=WorldCastleDragonStage::Idle;theta=2*pi;a.grounded=true;transition(1,t/cross_seconds);
    }else if((t-=cross_seconds)<idle_seconds){
        a.stage=WorldCastleDragonStage::Idle;theta=2*pi;a.grounded=true;selection=clip_selection(2,t/idle_seconds);
    }else if((t-=idle_seconds)<cross_seconds){
        a.stage=WorldCastleDragonStage::Breath;theta=2*pi;a.grounded=true;transition(2,t/cross_seconds);
    }else if((t-=cross_seconds)<breath_seconds){
        a.stage=WorldCastleDragonStage::Breath;theta=2*pi;a.grounded=true;const double u=t/breath_seconds;selection=clip_selection(3,u);
        a.breath=float(smooth((u-.15)/.15)*(1-smooth((u-.72)/.18)));
    }else if((t-=breath_seconds)<cross_seconds){
        a.stage=WorldCastleDragonStage::Idle;theta=2*pi;a.grounded=true;transition(3,t/cross_seconds);
    }else if((t-=cross_seconds)<idle_seconds){
        a.stage=WorldCastleDragonStage::Idle;theta=2*pi;a.grounded=true;selection=clip_selection(2,t/idle_seconds);
    }else if((t-=idle_seconds)<cross_seconds){
        a.stage=WorldCastleDragonStage::Takeoff;theta=2*pi;a.grounded=true;transition(4,t/cross_seconds);
    }else if((t-=cross_seconds)<takeoff_seconds){
        a.stage=WorldCastleDragonStage::Takeoff;theta=2*pi;const double u=t/takeoff_seconds;selection=clip_selection(4,u);
        // Preserve the complete downstroke: the carrier lifts early enough
        // that the interpolated native wing tips cannot pass through soil.
        // This does not shift individual pose origins or flatten wings.
        altitude=std::max(14.*smooth((u-.3)/.7),wing_clearance(selection));a.grounded=u<=.28;
    }else if((t-=takeoff_seconds)<cross_seconds){
        a.stage=WorldCastleDragonStage::Takeoff;theta=2*pi;altitude=14.;transition(5,t/cross_seconds);
    }else{
        t-=cross_seconds;a.stage=WorldCastleDragonStage::Departure;const double u=std::clamp(t/departure_seconds,0.,1.);
        theta=hermite(0,pi/3,0,angular_speed,departure_seconds,u);fly_pose(t);
        altitude=14.*(1-smoother(u))+(104.+8.*std::sin(2*theta)-floor)*smoother(u);
    }
    theta+=variant*pi;
    double ox,oz;world_atlas_village_origin(&ox,&oz);
    a.x=ox+radius*std::cos(theta);a.z=oz+60.+260.*std::sin(theta);a.y=float(floor+altitude);
    a.yaw=float(std::atan2(-radius*std::sin(theta),260.*std::cos(theta)));
    targets(selection.slot,a.pose_a,a.pose_b);a.blend=selection.weight;
    const auto &p=world_dragon_pose_metadata[a.pose_a],&q=world_dragon_pose_metadata[a.pose_b];
    const double c=std::cos(a.yaw),s=std::sin(a.yaw),w=a.blend;
    float mouth[3],direction[3];
    for(unsigned k=0;k<3;k++){mouth[k]=float(p.mouth[k]*(1-w)+q.mouth[k]*w);direction[k]=float(p.direction[k]*(1-w)+q.direction[k]*w);a.minimum[k]=std::min(p.minimum[k],q.minimum[k]);a.maximum[k]=std::max(p.maximum[k],q.maximum[k]);}
    a.mouth[0]=a.x+c*mouth[0]+s*mouth[2];a.mouth[1]=a.y+mouth[1];a.mouth[2]=a.z-s*mouth[0]+c*mouth[2];
    const double length=std::max(1.e-6,std::sqrt(double(direction[0])*direction[0]+double(direction[1])*direction[1]+double(direction[2])*direction[2]));
    a.direction[0]=float((c*direction[0]+s*direction[2])/length);a.direction[1]=float(direction[1]/length);a.direction[2]=float((-s*direction[0]+c*direction[2])/length);
    return true;
}
}
struct WorldCastleDragons {
    char directory[768]={};double anchor_x=0,anchor_z=0;
    std::array<double,WORLD_CASTLE_DRAGON_COUNT> floors={};
    std::array<Bank,WORLD_CASTLE_DRAGON_COUNT> banks={};
    std::array<WorldCastleDragon,WORLD_CASTLE_DRAGON_COUNT> actors={};
    std::array<Uniforms,3> uniforms={};unsigned next_uniform=0;
    WorldCastleDragonsStats stats;
    Uniforms &locations(unsigned program){
        for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;
        auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
        u.offset=glGetUniformLocation(program,"uOffset");u.anchor=glGetUniformLocation(program,"uWorldAnchor");u.grass=glGetUniformLocation(program,"uGrass");u.vegetation=glGetUniformLocation(program,"uVegetation");u.lod=glGetUniformLocation(program,"uLodLevel");u.wind=glGetUniformLocation(program,"uWind");u.height=glGetUniformLocation(program,"uTreeHeight");u.reactive=glGetUniformLocation(program,"uActorReactive");u.prop=glGetUniformLocation(program,"uPropLodRange");return u;
    }
    void unload(){
        for(auto &bank:banks){for(auto &b:bank.blend){if(b.gpu)world_model_pose_blend_free(b.gpu);b={};}for(auto &a:bank.assets){if(a)world_model_free(a);a=nullptr;}}
        if(stats.active)stats.unloads++;
        stats.active=false;stats.actors=stats.airborne=stats.grounded=stats.breathing=stats.visible=stats.reflection_visible=stats.shadow_visible=stats.pose_assets=stats.blend_batches=0;
    }
};
double world_castle_dragons_cycle_seconds(){return cycle;}
bool world_castle_dragons_sample(unsigned variant,double time,WorldCastleDragon *out){
    if(!out||variant>=WORLD_CASTLE_DRAGON_COUNT||!std::isfinite(time)||time<0||time>1.e9)return false;
    double floor=0;if(!support(variant,floor))return false;Selection selection;return sample(variant,time,floor,*out,selection);
}
WorldCastleDragons *world_castle_dragons_create(const char *directory){
    if(!directory||!*directory||std::strlen(directory)>=sizeof(WorldCastleDragons::directory))return nullptr;
    auto *d=new(std::nothrow)WorldCastleDragons;if(d){std::strcpy(d->directory,directory);world_atlas_village_origin(&d->anchor_x,&d->anchor_z);}return d;
}
bool world_castle_dragons_desired(const WorldCastleDragons *d,double x,double z){return d&&std::isfinite(x)&&std::isfinite(z)&&std::hypot(x-d->anchor_x,z-d->anchor_z)<(d->stats.active?980.:900.);}
bool world_castle_dragons_commit(WorldCastleDragons *d,bool active,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    if(!d)return false;
    if(d->stats.active==active)return true;
    d->unload();if(!active)return true;
    auto fail=[&](const char *message){if(error&&capacity)std::snprintf(error,capacity,"Castle dragons: %s",message);d->unload();d->stats.failures++;return false;};
    for(unsigned variant=0;variant<WORLD_CASTLE_DRAGON_COUNT;variant++){
        if(!support(variant,d->floors[variant]))return fail("unsafe or wet roost terrain");
        auto &bank=d->banks[variant];const char *stem=variant?"frostshade":"hellfire";
        for(const auto &clip:world_dragon_clips)for(unsigned i=0;i<clip.count;i++){
            char path[1100];const int n=std::snprintf(path,sizeof(path),"%s/%s_%s_%02u.glb",d->directory,stem,clip.name,i);
            if(n<0||std::size_t(n)>=sizeof(path))return fail("model path too long");
            bank.assets[clip.base+i]=world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION);if(!bank.assets[clip.base+i])return fail(path);
        }
        for(unsigned i=0;i<batches;i++){
            auto &b=bank.blend[i];unsigned a,next;targets(i,a,next);b.gpu=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!b.gpu||!world_model_pose_blend_set_targets(b.gpu,bank.assets[a],bank.assets[next]))return fail("pose topology or material correspondence failed");
        }
    }
    d->stats.active=true;d->stats.loads++;d->stats.pose_assets=poses*WORLD_CASTLE_DRAGON_COUNT;d->stats.blend_batches=batches*WORLD_CASTLE_DRAGON_COUNT;return true;
}
void world_castle_dragons_update(WorldCastleDragons *d,double time){
    if(!d||!d->stats.active||!std::isfinite(time)||time<0||time>1.e9)return;
    d->stats.updates++;d->stats.actors=d->stats.airborne=d->stats.grounded=d->stats.breathing=0;
    for(unsigned variant=0;variant<WORLD_CASTLE_DRAGON_COUNT;variant++){
        auto &bank=d->banks[variant];auto &a=d->actors[variant];if(!sample(variant,time,d->floors[variant],a,bank.selected))continue;
        auto &b=bank.blend[bank.selected.slot];const WorldModelInstance instance={float(a.x-d->anchor_x),a.y,float(a.z-d->anchor_z),a.yaw,1.f};
        if(!b.uploaded||std::memcmp(&instance,&b.instance,sizeof(instance))||b.weight!=bank.selected.weight){
            if(world_model_pose_blend_upload(b.gpu,&instance,&bank.selected.weight,1)){b.instance=instance;b.weight=bank.selected.weight;b.uploaded=true;d->stats.uploads++;}
            else{b.uploaded=false;d->stats.failures++;}
        }
        d->stats.actors++;d->stats.airborne+=!a.grounded;d->stats.grounded+=a.grounded;d->stats.breathing+=a.breath>0;
    }
}
void world_castle_dragons_draw(WorldCastleDragons *d,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass){
    if(!d||!d->stats.active||!program||pass>2||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(aspect)||aspect<=0)return;
    const auto view=world_vegetation_visibility_view(x,y,z,yaw,pitch,aspect);std::array<bool,WORLD_CASTLE_DRAGON_COUNT> selected={};unsigned visible=0;
    for(unsigned i=0;i<d->stats.actors;i++){
        const auto &a=d->actors[i];const auto &batch=d->banks[i].blend[d->banks[i].selected.slot];if(!batch.uploaded)continue;
        if(pass==2){if(std::hypot(a.x-x,a.z-z)>320.)continue;}
        else{
            const double cx=(a.minimum[0]+a.maximum[0])*.5,cy=(a.minimum[1]+a.maximum[1])*.5,cz=(a.minimum[2]+a.maximum[2])*.5;
            const double radius=std::sqrt(std::pow((a.maximum[0]-a.minimum[0])*.5,2)+std::pow((a.maximum[1]-a.minimum[1])*.5,2)+std::pow((a.maximum[2]-a.minimum[2])*.5,2));
            const double c=std::cos(a.yaw),s=std::sin(a.yaw);
            if(world_vegetation_sphere_outside(view,a.x+c*cx+s*cz,a.y+cy,a.z-s*cx+c*cz,radius,fade_center+fade_half))continue;
        }
        selected[i]=true;visible++;
    }
    if(pass==0)d->stats.visible=visible;else if(pass==1)d->stats.reflection_visible=visible;else d->stats.shadow_visible=visible;
    if(!visible)return;
    auto &u=d->locations(program);glUseProgram(program);glUniform3f(u.offset,float(d->anchor_x-x),float(-y),float(d->anchor_z-z));glUniform2f(u.anchor,float(d->anchor_x),float(d->anchor_z));glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,12);glUniform1f(u.reactive,pass==2?0.f:.85f);glUniform3f(u.prop,fade_center,fade_half,1);
    for(unsigned i=0;i<WORLD_CASTLE_DRAGON_COUNT;i++)if(selected[i]){world_model_pose_blend_draw(d->banks[i].blend[d->banks[i].selected.slot].gpu,program);d->stats.draws++;}
    glUniform1f(u.reactive,0);glUniform3f(u.prop,0,0,0);
}
WorldCastleDragonsStats world_castle_dragons_stats(const WorldCastleDragons *d){return d?d->stats:WorldCastleDragonsStats{};}
bool world_castle_dragons_actor(const WorldCastleDragons *d,unsigned index,WorldCastleDragon *out){if(!d||!out||!d->stats.active||index>=d->stats.actors)return false;*out=d->actors[index];return true;}
void world_castle_dragons_free(WorldCastleDragons *d){if(d){d->unload();delete d;}}
