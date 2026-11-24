#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_village_population.h"
#include "engine/world_castle_creek.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
#include "engine/world_village_contact_data.inc"
static_assert(std::size(world_village_contact_curves)==world_village_pose_blend_group_count);
float contact_offset(unsigned group,float blend){
    const float position=std::clamp(blend,0.f,1.f)*32.f;
    const unsigned i=std::min(31u,unsigned(position));const float t=position-float(i);
    return -(world_village_contact_curves[group][i]*(1-t)+world_village_contact_curves[group][i+1]*t);
}
constexpr std::size_t resident_count=world_village_resident_count,group_count=world_village_pose_group_count,blend_count=world_village_pose_blend_group_count;
constexpr std::size_t actor_capacity=resident_count+WORLD_EDITED_ACTOR_CAPACITY;
constexpr double pi=3.14159265358979323846;
enum Role {Knight,Archer,Trader,Traveler};
const char *roles[]={"Knight","Archer","Trader","Traveler"};
const char *names[]={"Crown watch","Bow sentinel","Market keeper","Road traveler"};
struct Point {double x,z;};
struct Route {Point points[4];};
constexpr Route routes[]={
    {{{-18,-75},{18,-75},{18,-85},{-18,-85}}},
    {{{-1,-42},{-1,178},{1,178},{1,-42}}},
    {{{2.4,55},{93,55},{93,125},{2.4,125}}},
    {{{-2.4,125},{-93,125},{-93,55},{-2.4,55}}},
    // A short loop stays on the western pavement rather than duplicating
    // the center avenue's lanes and walking straight through other residents.
    {{{-4.7,-42},{-4.7,44},{-3.3,44},{-3.3,-42}}},
    // Merchants Lane stays inside the independently measured street corridor.
    // Separate 1.6 m return lanes keep opposing residents apart; turns and
    // dwells remain well short of both shared cross streets.
    {{{60.95,-25},{60.95,47},{62.55,47},{62.55,-25}}},
    // Existing travelers browse the open aisle between the market columns.
    // Return strips clear both counters and their stationary traders.
    {{{17,-12},{17,32},{18.8,32},{18.8,-12}}},
    // The demo quarter receives existing craftspeople, on separate 1.6 m
    // return strips entirely inside the six-metre public carriageways.
    // Both stop lines stay clear of the shared 55/125 cross-street crowd.
    {{{-36.8,68},{-36.8,115},{-35.2,115},{-35.2,68}}},
    {{{25,150.2},{65,150.2},{65,151.8},{25,151.8}}},
    {{{-130,-28},{-130,46},{-132,46},{-132,-28}}},
    // The bartender works along the open aisle behind the counter. Existing
    // distance-driven gait and eased arrivals keep the short route unhurried.
    {{{-147.5,-87.5},{-144.2,-87.5},{-147.5,-87.5},{-144.2,-87.5}}}
};
struct VillageLifeDef {const char *name,*role,*district;unsigned archetype;double x,z;float yaw,y_offset;unsigned route;double phase;bool walker,seated;};
#include "engine/world_village_life_data.inc"
static_assert(std::size(village_life_defs)+world_village_original_resident_count==resident_count);
struct Resident {Role role;unsigned archetype=0,route=0;double offset=0,speed=1.35;Point point={};float yaw=0,y=24,current_speed=0,cycle_phase=0,blend=0;unsigned group=0,pose_a=0,pose_b=0;bool walking=false,transitioning=false;
    bool stationary=false,seated=false;float y_offset=0,stride_scale=1;const VillageLifeDef *life=nullptr;};
struct ActorRuntime {
    enum Mode {Idle,Walk,Plant,Blend};
    WorldVillageActorRuntime pose;bool overridden=false,initialized=false;
    Mode mode=Idle;double last_x=0,last_y=0,last_z=0,phase=0,idle_phase=0,plant_from=0,plant_to=0;
    float elapsed=0,blend_from=0,blend_to=0,blend_value=0;
    bool pair_to_idle=false,opposite=false;
};
struct Bounds {
    std::array<double,3> minimum={},maximum={};bool valid=false;
    void include(const std::array<double,3> &lo,const std::array<double,3> &hi){
        if(!valid){minimum=lo;maximum=hi;valid=true;return;}
        for(unsigned i=0;i<3;i++){minimum[i]=std::min(minimum[i],lo[i]);maximum[i]=std::max(maximum[i],hi[i]);}
    }
};
struct Group {
    WorldModelAsset *asset=nullptr;
    Bounds bounds;
};
struct BlendGroup {
    WorldModelPoseBlend *batch=nullptr;
    std::array<WorldModelInstance,actor_capacity> next={},uploaded={};
    std::array<float,actor_capacity> weights={},uploaded_weights={};
    std::array<WorldModelCharacterLook,actor_capacity> looks={},uploaded_looks={};
    std::size_t count=0,uploaded_count=0;
    unsigned a=0,b=0;int graph=-1;
    Bounds source_bounds,committed_bounds;
};
struct Uniforms {unsigned long long epoch=0;
    GLuint program=0;GLint offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,tree_height=-1,reactive=-1,prop_lod=-1,graph=-1;
};
/* Both affine source poses and every instance are enclosed. Compute only after
 * a successful GPU upload: a failed upload must keep the old drawable bounds. */
Bounds instance_bounds(const BlendGroup &group){
    Bounds result;
    std::array<double,3> center={},extent={};
    for(unsigned axis=0;axis<3;axis++){
        center[axis]=(group.source_bounds.minimum[axis]+group.source_bounds.maximum[axis])*.5;
        extent[axis]=(group.source_bounds.maximum[axis]-group.source_bounds.minimum[axis])*.5;
    }
    for(std::size_t i=0;i<group.uploaded_count;i++){
        const auto &instance=group.uploaded[i];const double c=std::cos(double(instance.yaw)),s=std::sin(double(instance.yaw)),scale=instance.scale;
        const double width=group.uploaded_looks[i].width;
        const std::array<double,3> position={instance.x+scale*width*(c*center[0]+s*center[2]),instance.y+scale*center[1],instance.z+scale*width*(-s*center[0]+c*center[2])};
        const std::array<double,3> radius={scale*width*(std::abs(c)*extent[0]+std::abs(s)*extent[2]),scale*extent[1],scale*width*(std::abs(s)*extent[0]+std::abs(c)*extent[2])};
        std::array<double,3> lo={},hi={};for(unsigned axis=0;axis<3;axis++){lo[axis]=position[axis]-radius[axis];hi[axis]=position[axis]+radius[axis];}
        result.include(lo,hi);
    }
    return result;
}
struct Frustum {
    double planes[6][4]={};bool valid=false;
    explicit Frustum(const float *matrix){
        if(!matrix)return;
        // Reject singular/invalid matrices rather than interpreting an invalid
        // clip volume as proof that a drawable batch is invisible.
        double rows[4][4]={};
        for(unsigned row=0;row<4;row++){
            double maximum=0;
            for(unsigned column=0;column<4;column++){
                const double value=matrix[column*4+row];if(!std::isfinite(value))return;
                rows[row][column]=value;maximum=std::max(maximum,std::abs(value));
            }
            if(maximum==0)return;
            for(double &value:rows[row])value/=maximum;
        }
        for(unsigned column=0;column<4;column++){
            unsigned pivot=column;for(unsigned row=column+1;row<4;row++)if(std::abs(rows[row][column])>std::abs(rows[pivot][column]))pivot=row;
            if(std::abs(rows[pivot][column])<1e-12)return;
            if(pivot!=column)for(unsigned j=column;j<4;j++)std::swap(rows[pivot][j],rows[column][j]);
            for(unsigned row=column+1;row<4;row++){
                const double factor=rows[row][column]/rows[column][column];
                for(unsigned j=column+1;j<4;j++)rows[row][j]-=factor*rows[column][j];
            }
        }
        for(unsigned axis=0;axis<3;axis++)for(unsigned side=0;side<2;side++){
            auto &plane=planes[axis*2+side];const double sign=side?1.:-1.;
            for(unsigned j=0;j<4;j++)plane[j]=double(matrix[j*4+3])+sign*double(matrix[j*4+axis]);
            if(plane[0]==0&&plane[1]==0&&plane[2]==0)return;
        }
        valid=true;
    }
    bool visible(const Bounds &bounds,const std::array<double,3> &offset) const{
        if(!valid||!bounds.valid)return true;
        for(const auto &plane:planes){
            double maximum=plane[3],normal=0;
            for(unsigned axis=0;axis<3;axis++){
                maximum+=plane[axis]*((plane[axis]>=0?bounds.maximum[axis]:bounds.minimum[axis])+offset[axis]);
                normal+=std::abs(plane[axis]);
            }
            // 5 cm covers float shader trig/anchor arithmetic across this
            // 100.8 km world, and retains touching/near-plane geometry.
            if(maximum < -.05*normal)return false;
        }
        return true;
    }
};
double length(Point a,Point b){return std::hypot(b.x-a.x,b.z-a.z);}
double smooth(double t){t=std::clamp(t,0.,1.);return t*t*t*(t*(t*6.-15.)+10.);}
double ramp(double distance,double speed){return std::min(.35,distance/(speed*2.));}
double travel_time(double distance,double speed){return distance/speed+ramp(distance,speed);}
double travel_distance(double t,double distance,double speed){
    const double r=ramp(distance,speed),duration=travel_time(distance,speed);
    if(t<r)return speed*t*t/(2*r);
    if(t>duration-r){const double left=duration-t;return distance-speed*left*left/(2*r);}
    return speed*(t-r*.5);
}
double period(const Route &route,double speed,double dwell){
    double seconds=0;for(std::size_t i=0;i<4;i++)seconds+=travel_time(length(route.points[i],route.points[(i+1)%4]),speed)+dwell;
    return seconds;
}
unsigned transition_group(unsigned archetype,bool to_idle,bool opposite=false){
    unsigned group=world_village_original_pose_group_count;
    for(unsigned i=0;i<archetype;i++)if(world_village_pose_archetypes[i].walk_count)group+=3;
    return group+(to_idle?(opposite?2u:1u):0u);
}
void cycle_pose(Resident &a,double phase,bool walk){
    const auto &appearance=world_village_pose_archetypes[a.archetype];
    const unsigned count=walk?appearance.walk_count:appearance.idle_count,base=walk?appearance.walk_group:appearance.idle_group;
    phase-=std::floor(phase);const double frame=phase*count;const unsigned index=unsigned(std::floor(frame));
    a.group=base>=world_village_original_pose_group_count?unsigned(world_village_original_blend_group_count)+base-unsigned(world_village_original_pose_group_count)+index:base+index;
    a.pose_a=base+index;a.pose_b=base+(index+1)%count;a.blend=float(frame-index);a.cycle_phase=float(phase);
}
void pose(Resident &a,double time){
    a.transitioning=false;a.current_speed=0;
    if(a.seated){a.walking=false;cycle_pose(a,(time+a.offset)/(194./24.),false);return;}
    if(a.stationary){
        // Authored neighbors breathe at source-like, individually varied
        // tempos. Keep roots and facing fixed: a body-wide swivel would drag
        // planted feet across the floor. Original street population is intact.
        const double seconds=a.archetype==7?4.0:a.archetype==6?4.7:5.4;
        const double variation=1.+.06*std::sin(a.offset*1.71);
        a.walking=false;cycle_pose(a,(time+a.offset)/(seconds*variation),false);return;
    }
    if(a.role==Trader||a.role==Archer){a.walking=false;cycle_pose(a,(time+a.offset*.173)/2.4,false);return;}
    const Route &route=routes[a.route];
    // Close-up neighbors take a measured breath and turn before departing;
    // preserve the original128 actors' absolute phases and existing lanes.
    const double dwell=a.role==Knight?2.0:a.life?(a.life->route==10?2.0:1.65):1.15;
    const double cycle=period(route,a.speed,dwell);
    double phase=std::fmod(time+a.offset,cycle);if(phase<0)phase+=cycle;
    for(std::size_t i=0;i<4;i++){
        Point p=route.points[i],q=route.points[(i+1)%4];const double distance=length(p,q),duration=travel_time(distance,a.speed),r=ramp(distance,a.speed),fade=std::min(.18,r*.6);
        a.yaw=float(std::atan2(q.x-p.x,q.z-p.z));
        if(phase<duration){
            const double d=travel_distance(phase,distance,a.speed),t=d/distance;
            const double handover=travel_distance(fade,distance,a.speed),core=distance-2*handover;
            const double cycles=std::max(.5,std::round(core/(world_village_walk_stride_m[a.archetype]*a.stride_scale)*2.)*.5);
            const bool opposite=(static_cast<unsigned>(std::llround(cycles*2))%2)!=0;
            a.point={p.x+(q.x-p.x)*t,p.z+(q.z-p.z)*t};a.walking=true;
            a.current_speed=float(a.speed*std::min({1.,phase/r,(duration-phase)/r}));
            if(phase<fade||phase>duration-fade){
                const bool to_idle=phase>duration-fade;const auto &appearance=world_village_pose_archetypes[a.archetype];
                a.group=transition_group(a.archetype,to_idle,opposite);a.pose_a=to_idle?appearance.walk_group+(opposite?appearance.walk_count/2:0):appearance.idle_group;a.pose_b=to_idle?appearance.idle_group:appearance.walk_group;
                a.blend=float(smooth(to_idle?(phase-duration+fade)/fade:phase/fade));a.cycle_phase=to_idle&&opposite?.5f:0.f;a.transitioning=true;
            }else{
                // End each leg on a planted source frame. The tiny handover
                // distance belongs to the idle/walk blend; the remaining gait
                // phase follows real translation, never the render frame rate.
                cycle_pose(a,std::clamp((d-handover)/core,0.,1.)*cycles,true);
            }
            return;
        }
        phase-=duration;
        if(phase<dwell){
            a.point=q;a.walking=false;const Point next=route.points[(i+2)%4];
            const double outgoing=std::atan2(next.x-q.x,next.z-q.z),delta=std::remainder(outgoing-double(a.yaw),2*pi);
            a.yaw=float(double(a.yaw)+delta*smooth((phase-.12)/(dwell-.24)));
            if(a.life&&a.life->route==10){
                // Turn toward the serving counter, pause with guests, then
                // leave on the next leg. Both joins have zero angular speed.
                const double incoming=std::atan2(q.x-p.x,q.z-p.z);
                if(phase<.70)a.yaw=float(incoming+std::remainder(-incoming,2*pi)*smooth(phase/.70));
                else if(phase<dwell-.70)a.yaw=0;
                else a.yaw=float(outgoing*smooth((phase-dwell+.70)/.70));
            }
            // Appended close-up residents make a small neutral-to-breath-to-
            // neutral gesture instead of rushing an entire multi-second idle
            // clip into a short stop. Both joins remain exactly pose zero.
            const double idle_phase=a.life ? .16*std::pow(std::sin(pi*phase/dwell),2.) : smooth(phase/dwell);
            cycle_pose(a,idle_phase,false);return;
        }
        phase-=dwell;
    }
}
}
struct WorldVillagePopulation {
    struct Appearance {float scale=1;WorldModelCharacterLook look={1,0,1,0,{1,1,1},0};};
    std::array<Appearance,actor_capacity> appearances={};
    std::array<WorldVillageAppearanceInfo,std::size(world_village_pose_archetypes)> appearance_info={};
    std::array<bool,resident_count> runtime_active=[](){std::array<bool,resident_count> a;a.fill(true);return a;}();
    std::array<WorldVillageActorConfig,WORLD_EDITED_ACTOR_CAPACITY> edits={};
    std::array<Resident,WORLD_EDITED_ACTOR_CAPACITY> edited_residents={};
    std::array<ActorRuntime,WORLD_EDITED_ACTOR_CAPACITY> actor_runtime={};
    std::array<int,resident_count> source_edits=[](){std::array<int,resident_count> a;a.fill(-1);return a;}();
    std::array<unsigned,WORLD_EDITED_ACTOR_CAPACITY> added_edits={};unsigned edit_count=0,added_count=0;
    std::array<Resident,resident_count> residents={};
    std::array<Group,group_count> groups={};
    std::array<BlendGroup,blend_count> blends={};
    std::array<WorldModelPoseBlend *,12> player_batches={};unsigned player_slot=0;bool player_ready=false;
    double player_x=0,player_z=0;
    // Crowd and player now use two distinct color/depth program pairs.
    // Retain all four so the separate character variant never churns queries.
    std::array<Uniforms,4> uniforms={};unsigned next_uniform=0;
    double x=0,z=0,anchor_x=0,anchor_z=0;
    WorldVillagePopulationStats stats;
    Uniforms &locations(GLuint program){
        for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;
        auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
        u.offset=glGetUniformLocation(program,"uOffset");u.anchor=glGetUniformLocation(program,"uWorldAnchor");
        u.grass=glGetUniformLocation(program,"uGrass");u.vegetation=glGetUniformLocation(program,"uVegetation");
        u.lod=glGetUniformLocation(program,"uLodLevel");u.wind=glGetUniformLocation(program,"uWind");
        u.tree_height=glGetUniformLocation(program,"uTreeHeight");u.reactive=glGetUniformLocation(program,"uActorReactive");
        u.prop_lod=glGetUniformLocation(program,"uPropLodRange");u.graph=glGetUniformLocation(program,"uStudioGraphIndex");return u;
    }
};
WorldVillagePopulation *world_village_population_load(const char *directory,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    if(!directory||!directory[0])return nullptr;
    auto *p=new(std::nothrow)WorldVillagePopulation;if(!p)return nullptr;
    world_atlas_village_origin(&p->x,&p->z);p->anchor_x=std::floor(p->x/128)*128;p->anchor_z=std::floor(p->z/128)*128;
    unsigned group=0;
    for(const auto &appearance:world_village_pose_archetypes)for(unsigned mode=0;mode<2;mode++){
        unsigned count=mode?appearance.idle_count:appearance.walk_count;
        for(unsigned frame=0;frame<count;frame++){
            char path[1024];int size=std::snprintf(path,sizeof(path),"%s/%s_%s_%s_%02u.glb",directory,appearance.file_prefix,appearance.name,mode?"idle":"walk",frame);
            if(size<0||std::size_t(size)>=sizeof(path)||(p->groups[group].asset=world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION))==nullptr){
                if(error&&capacity)std::snprintf(error,capacity,"Missing village character pose: %s",path);
                std::fprintf(stderr,"[VillagePopulation] Required character pose failed: %s\n",path);
                world_village_population_free(p);return nullptr;
            }
            float minimum[3],maximum[3];bool valid=world_model_bounds(p->groups[group].asset,minimum,maximum);
            for(unsigned axis=0;axis<3&&valid;axis++)valid=std::isfinite(minimum[axis])&&std::isfinite(maximum[axis])&&minimum[axis]<=maximum[axis];
            if(!valid){
                if(error&&capacity)std::snprintf(error,capacity,"Invalid village character pose bounds: %s",path);
                world_village_population_free(p);return nullptr;
            }
            p->groups[group].bounds.include({minimum[0],minimum[1],minimum[2]},{maximum[0],maximum[1],maximum[2]});
            group++;
        }
    }
    unsigned blend=0;
    for(unsigned i=0;i<std::size(world_village_pose_archetypes);i++){
        const auto &g=p->groups[world_village_pose_archetypes[i].idle_group];
        p->appearance_info[i]={float(g.bounds.maximum[1]-g.bounds.minimum[1]),world_model_character_capabilities(g.asset)};
    }
    auto configure=[&](unsigned slot,unsigned a,unsigned b){
        auto &g=p->blends[slot];g.a=a;g.b=b;g.batch=world_model_pose_blend_create(actor_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
        g.source_bounds=p->groups[a].bounds;g.source_bounds.include(p->groups[b].bounds.minimum,p->groups[b].bounds.maximum);
        return g.batch&&world_model_pose_blend_set_targets(g.batch,p->groups[a].asset,p->groups[b].asset)&&world_model_pose_blend_enable_character(g.batch);
    };
    for(const auto &appearance:world_village_pose_archetypes)for(unsigned mode=0;mode<2;mode++){
        const unsigned count=mode?appearance.idle_count:appearance.walk_count,base=mode?appearance.idle_group:appearance.walk_group;
        if(base>=world_village_original_pose_group_count)continue;
        for(unsigned frame=0;frame<count;frame++)if(!configure(blend++,base+frame,base+(frame+1)%count)){
            if(error&&capacity)std::snprintf(error,capacity,"Incompatible adjacent village character poses");
            world_village_population_free(p);return nullptr;
        }
    }
    for(const auto &appearance:world_village_pose_archetypes)if(appearance.walk_count){
        if(!configure(blend++,appearance.idle_group,appearance.walk_group)||!configure(blend++,appearance.walk_group,appearance.idle_group)||!configure(blend++,appearance.walk_group+appearance.walk_count/2,appearance.idle_group)){
            if(error&&capacity)std::snprintf(error,capacity,"Incompatible village walk/idle transition");
            world_village_population_free(p);return nullptr;
        }
    }
    for(const auto &appearance:world_village_pose_archetypes)if(appearance.idle_group>=world_village_original_pose_group_count){
        for(unsigned frame=0;frame<appearance.idle_count;frame++)if(!configure(blend++,appearance.idle_group+frame,appearance.idle_group+(frame+1)%appearance.idle_count)){
            if(error&&capacity)std::snprintf(error,capacity,"Incompatible village seated source poses");
            world_village_population_free(p);return nullptr;
        }
    }
    for(unsigned i=0;i<12;i++){
        const unsigned next=i<8?(i+1)%8:8+(i-8+1)%4;
        auto *&b=p->player_batches[i];b=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!b||!world_model_pose_blend_set_targets(b,p->groups[i].asset,p->groups[next].asset)){
            if(error&&capacity)std::snprintf(error,capacity,"Player pose batch allocation failed");
            world_village_population_free(p);return nullptr;
        }
    }
    std::size_t index=0;
    for(unsigned i=0;i<8;i++){auto &a=p->residents[index++];a.role=Knight;a.archetype=0;a.route=0;}
    for(unsigned i=0;i<8;i++){
        auto &a=p->residents[index++];a.role=Archer;a.archetype=1;a.point={i<4?-35.:35.,-72.-(i%4)*5.};
        a.yaw=i<4?float(pi*.5):float(-pi*.5);a.offset=i*.8;
    }
    for(unsigned i=0;i<6;i++){
        auto &a=p->residents[index++];a.role=Trader;a.archetype=i%2?4:2;a.point={i<3?-25.35:25.35,-6.+(i%3)*15.};
        a.yaw=i<3?float(pi*.5):float(-pi*.5);a.offset=i*.9;
    }
    constexpr unsigned male_travelers[]={3,8,7};
    for(unsigned i=0;i<32;i++){
        auto &a=p->residents[index++];a.role=Traveler;a.archetype=i%2?5:male_travelers[(i/2)%3];
        a.route=i<4?5:(i<12?6:1+i%4);
    }
    // Existing inner stalls gain keepers; leave both main avenue lanes and
    // the village's 806m walking route clear. No character overlaps a counter.
    for(unsigned i=0;i<4;i++){
        auto &a=p->residents[index++];a.role=Trader;a.archetype=6;
        a.point={i<2?-13.35:13.35,i%2?24.:-6.};
        a.yaw=i<2?float(pi*.5):float(-pi*.5);a.offset=7.1+i*1.7;
    }
    for(unsigned i=0;i<6;i++){
        auto &a=p->residents[index++];a.role=Traveler;a.archetype=i%2?7:8;
        // Stable actors 60/61 enliven Southgate and 62/63 Coopers Return.
        // The original alley and market groups, including 26/27, keep their
        // route populations/phase offsets and all appearance indices.
        a.route=i<2?1+i%4:(i<4?7:8);
    }
    // Route time is absolute, so keep formation deterministic even after a
    // teleport or a skipped update. Equal speed and uniform cycle spacing
    // prevent same-lane overtaking/intersections without an O(n²) frame solver.
    std::array<unsigned,std::size(routes)> route_counts={},route_ordinals={};
    for(std::size_t i=0;i<index;i++){const auto &a=p->residents[i];if(a.role==Knight||a.role==Traveler)route_counts[a.route]++;}
    for(std::size_t i=0;i<index;i++){auto &a=p->residents[i];if(a.role!=Knight&&a.role!=Traveler)continue;
        a.speed=1.35;
        const double cycle=period(routes[a.route],a.speed,a.role==Knight?2.:1.15);
        a.offset=a.route*13.7+route_ordinals[a.route]++*cycle/route_counts[a.route];
    }
    // Preserve every original resident's identity, lane and absolute phase.
    // New walkers interleave in the free phase slots of those same safe loops;
    // no per-frame crowd solver or extra pose/model owners are required.
    auto add_walker=[&](Role role,unsigned archetype,unsigned route,double phase){
        auto &a=p->residents[index++];a.role=role;a.archetype=archetype;a.route=route;a.speed=1.35;
        a.offset=route*13.7+phase*period(routes[route],a.speed,role==Knight?2.:1.15);
    };
    for(unsigned i=0;i<8;i++)add_walker(Knight,0,0,(i+.5)/8.);
    for(unsigned i=0;i<8;i++){
        auto &a=p->residents[index++];a.role=Archer;a.archetype=1;
        a.point={i<4?-30.:30.,-64.-(i%4)*5.};a.yaw=i<4?float(pi*.5):float(-pi*.5);a.offset=13.+i*.8;
    }
    constexpr Point new_trader_points[]={
        {-13.8,1.5},{13.8,1.5},{-13.8,16.5},{13.8,16.5},
        {-25.8,1.5},{-25.8,16.5},{-33.7,.5},{-33.7,15.5},
        {-25.8,40.5},{27.8,40.5}
    };
    for(unsigned i=0;i<std::size(new_trader_points);i++){
        auto &a=p->residents[index++];a.role=Trader;a.archetype=i%3==0?6:(i%2?4:2);
        a.point=new_trader_points[i];a.yaw=a.point.x<0?float(pi*.5):float(-pi*.5);a.offset=23.+i*1.37;
    }
    unsigned new_traveler=0;
    auto add_traveler=[&](unsigned route,double phase){
        const unsigned i=new_traveler++;add_walker(Traveler,i%2?5:male_travelers[(i/2)%3],route,phase);
    };
    // The busy market gains eight browsers, Merchants Lane four, and each
    // newer residential street six. All lanes retain their 1.6/1.8m width.
    for(unsigned i=0;i<4;i++)add_traveler(5,(i+.5)/4.);
    for(unsigned i=0;i<8;i++)add_traveler(6,(i+.5)/8.);
    for(unsigned route:{7u,8u})for(unsigned i=1;i<8;i++)if(i!=4)add_traveler(route,i/8.);
    for(unsigned route:{1u,2u,3u,4u}){
        const unsigned count=route<3?4:3;
        for(unsigned i=0;i<count;i++)add_traveler(route,(i+.5)/route_counts[route]);
    }
    for(const auto &def:village_life_defs){
        auto &a=p->residents[index++];a.role=Traveler;a.archetype=def.archetype;a.point={def.x,def.z};a.yaw=def.yaw;
        a.y_offset=def.y_offset;a.stationary=!def.walker;a.seated=def.seated;a.life=&def;a.offset=def.phase;a.route=def.route;
        if(def.walker&&def.route==10)a.speed=.8;
        if(def.walker)a.offset=def.route*13.7+def.phase*period(routes[def.route],a.speed,def.route==10?2.0:1.65);
    }
    p->stats.residents=index;p->stats.knights=16;p->stats.archers=16;p->stats.traders=20;p->stats.travelers=76;
    p->stats.sheltered_residents=4;p->stats.tavern_residents=6;p->stats.street_residents=2;p->stats.seated=4;
    p->stats.loaded_pose_assets=group;return p;
}
bool world_village_population_configuration_valid(const WorldVillageActorConfig *edits,std::size_t count){
 if(count>WORLD_EDITED_ACTOR_CAPACITY||(count&&!edits))return false;
 std::array<int,resident_count> sources;sources.fill(-1);std::array<unsigned,WORLD_EDITED_ACTOR_CAPACITY> added={};unsigned n=0;
 for(unsigned i=0;i<count;i++){const auto &c=edits[i];if(c.source< -1||c.source>=int(resident_count)||c.appearance>=std::size(world_village_pose_archetypes)||!c.count||c.count>128||!std::isfinite(c.speed)||c.speed<.3||c.speed>2.5||!std::isfinite(c.dwell)||c.dwell<.4||c.dwell>15||!std::isfinite(c.yaw)||c.name.back()!=0||(c.count>1&&!world_village_pose_archetypes[c.appearance].walk_count))return false;for(unsigned j=0;j<c.count;j++){const auto &a=c.points[j];if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.z)||std::abs(a.x)>50400||std::abs(a.z)>28400||std::abs(a.y)>12000)return false;if(j&&std::hypot(a.x-c.points[j-1].x,a.z-c.points[j-1].z)<.005)return false;}if(c.source>=0){if(sources[unsigned(c.source)]>=0)return false;sources[unsigned(c.source)]=int(i);}else added[n++]=i;}
 return true;
}
bool world_village_population_configure(WorldVillagePopulation *p,const WorldVillageActorConfig *edits,std::size_t count){
 if(!p||!world_village_population_configuration_valid(edits,count)||count>WORLD_EDITED_ACTOR_CAPACITY||(count&&!edits))return false;
 std::array<int,resident_count> sources;sources.fill(-1);std::array<unsigned,WORLD_EDITED_ACTOR_CAPACITY> added={};unsigned n=0;
 for(unsigned i=0;i<count;i++){if(edits[i].source>=0)sources[unsigned(edits[i].source)]=int(i);else added[n++]=i;}
 p->edit_count=unsigned(count);p->added_count=n;p->source_edits=sources;p->added_edits=added;world_village_population_clear_runtime(p);for(unsigned i=0;i<count;i++){p->edits[i]=edits[i];auto &a=p->edited_residents[i];a={};a.archetype=edits[i].appearance;a.stationary=edits[i].count==1;a.seated=a.archetype==9;a.role=a.archetype==0?Knight:a.archetype==1?Archer:Traveler;a.speed=edits[i].speed;a.yaw=edits[i].yaw;}
 if(!count){p->anchor_x=std::floor(p->x/128)*128;p->anchor_z=std::floor(p->z/128)*128;}p->stats.residents=resident_count+n;for(unsigned i=0;i<count;i++)if(!edits[i].enabled)p->stats.residents--;return true;
}
namespace {
void runtime_actor_pose(Resident &a,ActorRuntime &runtime,double time,float dt,double origin_x,double origin_z){
    const auto &p=runtime.pose;const auto &appearance=world_village_pose_archetypes[a.archetype];
    a.point={p.x-origin_x,p.z-origin_z};a.y=float(p.y);a.yaw=p.yaw;a.current_speed=p.speed;a.transitioning=false;
    double distance=0;
    if(runtime.initialized){const double dx=p.x-runtime.last_x,dy=p.y-runtime.last_y,dz=p.z-runtime.last_z;distance=std::sqrt(dx*dx+dy*dy+dz*dz);}
    // A changed world location/spawn does not turn a teleport into thousands of
    // gait cycles. Ordinary navigation advances phase by actual travelled metres.
    if(!runtime.initialized||distance>std::max(4.,double(p.speed)*dt*4+.1)){
        runtime.mode=ActorRuntime::Idle;runtime.phase=runtime.idle_phase=0;runtime.elapsed=0;distance=0;runtime.initialized=true;
    }
    runtime.last_x=p.x;runtime.last_y=p.y;runtime.last_z=p.z;
    const bool moving=appearance.walk_count&&p.speed>.01f;a.walking=moving;
    if(!appearance.walk_count){a.current_speed=0;cycle_pose(a,time/(a.seated?8.08:5.4),false);return;}
    const double stride=world_village_walk_stride_m[a.archetype]*a.stride_scale;
    auto begin_blend=[&](bool to_idle,bool opposite,float from,float target){runtime.mode=ActorRuntime::Blend;runtime.pair_to_idle=to_idle;runtime.opposite=opposite;runtime.blend_from=runtime.blend_value=from;runtime.blend_to=target;runtime.elapsed=0;};
    if(runtime.mode==ActorRuntime::Idle){
        if(moving)begin_blend(false,false,0,1);
        else{runtime.idle_phase+=dt/5.4;cycle_pose(a,runtime.idle_phase,false);return;}
    }
    if(runtime.mode==ActorRuntime::Walk){
        runtime.phase+=distance/stride;
        if(!moving){runtime.mode=ActorRuntime::Plant;runtime.plant_from=runtime.phase;runtime.plant_to=std::round(runtime.phase*2)*.5;runtime.elapsed=0;}
        else{cycle_pose(a,runtime.phase,true);return;}
    }
    if(runtime.mode==ActorRuntime::Plant){
        if(moving){runtime.mode=ActorRuntime::Walk;runtime.phase+=distance/stride;cycle_pose(a,runtime.phase,true);return;}
        runtime.elapsed+=dt;runtime.phase=runtime.plant_from+(runtime.plant_to-runtime.plant_from)*smooth(runtime.elapsed/.10);
        if(runtime.elapsed<.10f){cycle_pose(a,runtime.phase,true);a.transitioning=true;return;}
        const bool opposite=std::llround(runtime.plant_to*2)%2!=0;
        begin_blend(true,opposite,0,1);
    }
    if(runtime.mode==ActorRuntime::Blend){
        const float target=runtime.pair_to_idle?(moving?0.f:1.f):(moving?1.f:0.f);
        if(target!=runtime.blend_to){runtime.blend_from=runtime.blend_value;runtime.blend_to=target;runtime.elapsed=0;}
        runtime.elapsed+=dt;runtime.blend_value=runtime.blend_from+(runtime.blend_to-runtime.blend_from)*float(smooth(runtime.elapsed/.18));
        a.group=transition_group(a.archetype,runtime.pair_to_idle,runtime.opposite);
        a.pose_a=runtime.pair_to_idle?appearance.walk_group+(runtime.opposite?appearance.walk_count/2:0):appearance.idle_group;
        a.pose_b=runtime.pair_to_idle?appearance.idle_group:appearance.walk_group;
        a.blend=runtime.blend_value;a.cycle_phase=runtime.pair_to_idle&&runtime.opposite?.5f:0.f;a.transitioning=true;
        if(runtime.elapsed>=.18f){
            if(moving){runtime.mode=ActorRuntime::Walk;runtime.phase=runtime.pair_to_idle&&runtime.opposite?.5:0;cycle_pose(a,runtime.phase,true);}
            else{runtime.mode=ActorRuntime::Idle;runtime.idle_phase=0;cycle_pose(a,0,false);}
            a.transitioning=false;
        }
    }
}
void edited_pose(Resident &a,const WorldVillageActorConfig &c,double time,double origin_x,double origin_z){
 a.transitioning=false;a.current_speed=0;auto setpoint=[&](world_navigation::Point p){a.point={p.x-origin_x,p.z-origin_z};a.y=float(p.y);};if(c.count==1){setpoint(c.points[0]);a.walking=false;a.yaw=c.yaw;cycle_pose(a,time/(a.seated?8.08:5.4),false);return;}
 double total=0;for(unsigned i=1;i<c.count;i++)total+=travel_time(std::hypot(c.points[i].x-c.points[i-1].x,c.points[i].z-c.points[i-1].z),c.speed)+c.dwell;double phase=std::fmod(time,total);
 for(unsigned i=1;i<c.count;i++){const auto &p=c.points[i-1],&q=c.points[i];double distance=std::hypot(q.x-p.x,q.z-p.z),duration=travel_time(distance,c.speed),r=ramp(distance,c.speed),fade=std::min(.18,r*.6);a.yaw=float(std::atan2(q.x-p.x,q.z-p.z));
  if(phase<duration){double d=travel_distance(phase,distance,c.speed),t=d/distance;setpoint({p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,p.z+(q.z-p.z)*t});a.walking=true;a.current_speed=float(c.speed*std::min({1.,phase/r,(duration-phase)/r}));double hand=travel_distance(fade,distance,c.speed),core=std::max(.001,distance-2*hand),cycles=std::max(.5,std::round(core/(world_village_walk_stride_m[a.archetype]*a.stride_scale)*2)*.5);bool opposite=unsigned(std::llround(cycles*2))%2;
   if(phase<fade||phase>duration-fade){bool to_idle=phase>duration-fade;auto appearance=world_village_pose_archetypes[a.archetype];a.group=transition_group(a.archetype,to_idle,opposite);a.pose_a=to_idle?appearance.walk_group+(opposite?appearance.walk_count/2:0):appearance.idle_group;a.pose_b=to_idle?appearance.idle_group:appearance.walk_group;a.blend=float(smooth(to_idle?(phase-duration+fade)/fade:phase/fade));a.transitioning=true;a.cycle_phase=to_idle&&opposite?.5f:0;}else cycle_pose(a,std::clamp((d-hand)/core,0.,1.)*cycles,true);return;
  }phase-=duration;if(phase<c.dwell){setpoint(q);a.walking=false;const auto &next=c.points[i+1<c.count?i+1:1];double outgoing=std::atan2(next.x-q.x,next.z-q.z);a.yaw+=float(std::remainder(outgoing-a.yaw,2*pi)*smooth((phase-.12)/(c.dwell-.24)));cycle_pose(a,.16*std::pow(std::sin(pi*phase/c.dwell),2),false);return;}phase-=c.dwell;
 }
}
}
void world_village_population_update(WorldVillagePopulation *p,double x,double y,double z,float time,float dt){
    if(!p||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(time)||time<0||time>1e7||!std::isfinite(dt)||dt<0||dt>1)return;
    p->stats.walking=p->stats.idling=p->stats.visible=p->stats.draw_groups=0;
    for(auto &g:p->blends)g.count=0;
    if(p->edit_count&&std::hypot(x-p->anchor_x,z-p->anchor_z)>256){p->anchor_x=std::floor(x/128)*128;p->anchor_z=std::floor(z/128)*128;}
    for(unsigned index=0;index<resident_count+p->added_count;index++){
        if(index<resident_count&&!p->runtime_active[index])continue;
        const int edited=index<resident_count?p->source_edits[index]:int(p->added_edits[index-resident_count]);
        if(edited>=0&&!p->edits[unsigned(edited)].enabled)continue;
        const bool runtime=edited>=0&&p->actor_runtime[unsigned(edited)].overridden;
        if(runtime&&!p->actor_runtime[unsigned(edited)].pose.active)continue;
        auto &a=edited>=0?p->edited_residents[unsigned(edited)]:p->residents[index];
        const auto &appearance=p->appearances[index];a.stride_scale=appearance.scale*appearance.look.width;
        if(runtime)runtime_actor_pose(a,p->actor_runtime[unsigned(edited)],time,dt,p->x,p->z);else if(edited>=0)edited_pose(a,p->edits[unsigned(edited)],time,p->x,p->z);else pose(a,time);if(a.walking)p->stats.walking++;else p->stats.idling++;
        double wx=p->x+a.point.x,wz=p->z+a.point.z;
        // Scripted streets retain the proved 24 m terrace; only the lowered
        // bridge adds a 2 cm walking deck. Avoid128 full atlas queries/frame.
        if(edited<0)a.y=float(world_castle_creek_floor_local(a.point.x,a.point.z,24.))+a.y_offset;
        if(std::hypot(wx-x,wz-z)>220)continue;
        if(edited>=0&&!runtime)a.y=world_atlas_walk_height(wx,wz);
        auto &g=p->blends[a.group];g.weights[g.count]=a.blend;g.looks[g.count]=appearance.look;
        g.next[g.count++]={float(wx-p->anchor_x),a.y+contact_offset(a.group,a.blend)*appearance.scale,float(wz-p->anchor_z),a.yaw,appearance.scale};p->stats.visible++;
    }
    for(auto &g:p->blends){
        bool changed=g.count!=g.uploaded_count||std::memcmp(g.next.data(),g.uploaded.data(),g.count*sizeof(WorldModelInstance))!=0||std::memcmp(g.weights.data(),g.uploaded_weights.data(),g.count*sizeof(float))!=0||std::memcmp(g.looks.data(),g.uploaded_looks.data(),g.count*sizeof(WorldModelCharacterLook))!=0;
        if(changed&&world_model_pose_blend_upload_character(g.batch,g.next.data(),g.weights.data(),g.looks.data(),g.count)){
            std::copy_n(g.next.begin(),g.count,g.uploaded.begin());std::copy_n(g.weights.begin(),g.count,g.uploaded_weights.begin());g.uploaded_count=g.count;p->stats.uploads++;
            std::copy_n(g.looks.begin(),g.count,g.uploaded_looks.begin());
            g.committed_bounds=instance_bounds(g);
        }
        if(g.uploaded_count)p->stats.draw_groups++;
    }
    p->stats.updates++;
}
namespace {
void draw_population(WorldVillagePopulation *p,unsigned program,double x,double y,double z,bool shadow,bool cull,const float *matrix){
    if(!p||!program||!p->stats.draw_groups)return;
    const Frustum frustum(cull?matrix:nullptr);
    // Match the actual float offset sent to the shader before testing planes.
    const std::array<double,3> offset={float(p->anchor_x-x),float(-y),float(p->anchor_z-z)};
    if(cull)p->stats.frustum_draw_calls++;
    auto &u=p->locations(program);glUseProgram(program);
    glUniform3f(u.offset,float(p->anchor_x-x),float(-y),float(p->anchor_z-z));
    glUniform2f(u.anchor,float(p->anchor_x),float(p->anchor_z));
    glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.tree_height,2);
    glUniform3f(u.prop_lod,0,0,0);
    glUniform1f(u.reactive,shadow?0.f:.85f);
    for(auto &g:p->blends)if(g.uploaded_count){
        if(cull&&!frustum.visible(g.committed_bounds,offset)){p->stats.frustum_culled_batches++;continue;}
        if(cull)p->stats.frustum_submitted_batches++;
        glUniform1i(u.graph,g.graph);world_model_pose_blend_draw(g.batch,program);
    }
    glUniform1i(u.graph,-1);glUniform1f(u.reactive,0);
}
}
void world_village_population_draw(WorldVillagePopulation *p,unsigned program,double x,double y,double z,bool shadow){
    draw_population(p,program,x,y,z,shadow,false,nullptr);
}
void world_village_population_draw_frustum(WorldVillagePopulation *p,unsigned program,double x,double y,double z,const float *matrix){
    draw_population(p,program,x,y,z,false,true,matrix);
}
void world_village_population_graphs(WorldVillagePopulation *p,const char *const *names,std::size_t count){
 if(!p||count>8||(count&&!names))return;
 for(auto &g:p->blends){g.graph=-1;for(unsigned a=0;a<std::size(world_village_pose_archetypes);a++){
  const auto &v=world_village_pose_archetypes[a];if(!((g.a>=v.walk_group&&g.a<v.walk_group+v.walk_count)||(g.a>=v.idle_group&&g.a<v.idle_group+v.idle_count)))continue;
  char target[32];std::snprintf(target,sizeof target,"__npc_%u__",a);for(unsigned i=0;i<count;i++)if(names[i]&&std::strcmp(names[i],target)==0)g.graph=int(i);break;
 }}
}
bool world_village_population_runtime_active(WorldVillagePopulation *p,std::size_t index,bool active){if(!p||index>=resident_count)return false;p->runtime_active[index]=active;return true;}
void world_village_population_clear_runtime(WorldVillagePopulation *p){if(p)p->actor_runtime={};}
bool world_village_population_actor_runtime(WorldVillagePopulation *p,unsigned index,const WorldVillageActorRuntime *pose){
    if(!p||index>=p->edit_count||index>=WORLD_EDITED_ACTOR_CAPACITY)return false;
    if(!pose){p->actor_runtime[index]={};return true;}
    if(!std::isfinite(pose->x)||!std::isfinite(pose->y)||!std::isfinite(pose->z)||!std::isfinite(pose->yaw)||!std::isfinite(pose->speed)||std::abs(pose->x)>50400||std::abs(pose->z)>28400||std::abs(pose->y)>12000||pose->speed<0||pose->speed>12||(pose->speed>0&&!world_village_pose_archetypes[p->edits[index].appearance].walk_count))return false;
    auto &runtime=p->actor_runtime[index];runtime.pose=*pose;runtime.overridden=true;return true;
}
WorldVillagePopulationStats world_village_population_stats(const WorldVillagePopulation *p){return p?p->stats:WorldVillagePopulationStats{};}
bool world_village_population_resident(const WorldVillagePopulation *p,std::size_t index,WorldVillageResident *out){
    if(!p||!out||index>=resident_count+p->added_count)return false;
    int edited=index<resident_count?p->source_edits[index]:int(p->added_edits[index-resident_count]);if(edited>=0&&!p->edits[unsigned(edited)].enabled)return false;
    const ActorRuntime *runtime=edited>=0&&p->actor_runtime[unsigned(edited)].overridden?&p->actor_runtime[unsigned(edited)]:nullptr;
    if(runtime&&!runtime->pose.active)return false;
    const auto &a=edited>=0?p->edited_residents[unsigned(edited)]:p->residents[index];out->name=names[a.role];out->role=roles[a.role];
    out->archetype=world_village_pose_archetypes[a.archetype].name;
    if(a.life){out->name=a.life->name;out->role=a.life->role;}
    out->seated=a.seated;out->district=a.life?a.life->district:"Established village";
    out->x=p->x+a.point.x;out->z=p->z+a.point.z;out->y=a.y;out->yaw=a.yaw;out->walking=a.walking;
    if(runtime){out->x=runtime->pose.x;out->z=runtime->pose.z;out->y=float(runtime->pose.y);out->yaw=runtime->pose.yaw;}
    if(edited>=0){out->name=p->edits[unsigned(edited)].name.data();out->role="Authored resident";out->district="Studio";}
    out->appearance_scale=p->appearances[index].scale;out->body_width=p->appearances[index].look.width;
    out->visual_y_offset=contact_offset(a.group,a.blend)*out->appearance_scale;
    out->speed=runtime?runtime->pose.speed:a.current_speed;if(runtime)out->walking=runtime->pose.speed>.01f;out->cycle_phase=a.cycle_phase;out->pose_blend=a.blend;out->pose_a=a.pose_a;out->pose_b=a.pose_b;out->transitioning=a.transitioning;return true;
}
bool world_village_population_appearance_info(const WorldVillagePopulation *p,std::size_t index,WorldVillageAppearanceInfo *out){
    if(!p||!out||index>=resident_count+p->added_count)return false;
    const int edited=index<resident_count?p->source_edits[index]:int(p->added_edits[index-resident_count]);
    const auto &a=edited>=0?p->edited_residents[unsigned(edited)]:p->residents[index];
    *out=p->appearance_info[a.archetype];return true;
}
bool world_village_population_appearance(WorldVillagePopulation *p,std::size_t index,float scale,const WorldModelCharacterLook *look){
    if(!p||index>=resident_count+p->added_count)return false;
    if(!look){p->appearances[index]={};return true;}
    if(!std::isfinite(scale)||scale<.001f||scale>100||!std::isfinite(look->width)||look->width<.65f||look->width>1.45f||!std::isfinite(look->cutoff_offset)||std::abs(look->cutoff_offset)>.3f||!std::isfinite(look->alpha_exponent)||look->alpha_exponent<.5f||look->alpha_exponent>2||!std::isfinite(look->color_strength)||look->color_strength<0||look->color_strength>1||!std::isfinite(look->hide_hair)||look->hide_hair<0||look->hide_hair>1)return false;
    for(float c:look->hair_tint)if(!std::isfinite(c)||c<0||c>1)return false;
    p->appearances[index]={scale,*look};return true;
}
bool world_village_population_apply_appearances(WorldVillagePopulation *p,const WorldVillageAppearanceChange *changes,std::size_t count){
    if(!p||count>64||(count&&!changes))return false;
    const auto previous=p->appearances;
    for(std::size_t i=0;i<count;i++){const auto &c=changes[i];if(!world_village_population_appearance(p,c.resident,c.scale,c.clear?nullptr:&c.look)){p->appearances=previous;return false;}}
    return true;
}
bool world_village_population_closest(const WorldVillagePopulation *p,double x,double z,double radius,WorldVillageResident *out){
    if(!p||!out||!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(radius)||radius<0)return false;
    bool found=false;double best=radius;
    for(std::size_t i=0;i<resident_count+p->added_count;i++){
        WorldVillageResident a;if(!world_village_population_resident(p,i,&a))continue;double d=std::hypot(a.x-x,a.z-z);
        if(d<=best){best=d;*out=a;found=true;}
    }
    return found;
}

bool world_village_population_player_update(WorldVillagePopulation *p,double x,double feet,double z,float yaw,float phase,bool walking){
    if(!p||!std::isfinite(x)||!std::isfinite(feet)||!std::isfinite(z)||!std::isfinite(yaw)||!std::isfinite(phase))return false;
    phase-=std::floor(phase);
    const float f=phase*(walking?8.f:4.f);
    const unsigned slot=(walking?0:8)+unsigned(f);const float weight=f-std::floor(f);
    (void)walking;
    // The player owns a local origin, independent of the village's position.
    p->player_x=std::floor(x/128)*128;p->player_z=std::floor(z/128)*128;
    WorldModelInstance instance={float(x-p->player_x),float(feet + contact_offset(slot,weight)),float(z-p->player_z),yaw,1};
    p->player_slot=slot;p->player_ready=world_model_pose_blend_upload(p->player_batches[slot],&instance,&weight,1);return p->player_ready;
}
void world_village_population_player_draw(WorldVillagePopulation *p,unsigned program,double x,double y,double z,bool shadow){
    if(!p||!program||!p->player_ready)return;
    auto &u=p->locations(program);glUseProgram(program);
    glUniform3f(u.offset,float(p->player_x-x),float(-y),float(p->player_z-z));glUniform2f(u.anchor,float(p->player_x),float(p->player_z));
    glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);
    glUniform1f(u.tree_height,2);glUniform3f(u.prop_lod,0,0,0);glUniform1f(u.reactive,shadow?0.f:.85f);
    glUniform1i(u.graph,-1);world_model_pose_blend_draw(p->player_batches[p->player_slot],program);glUniform1f(u.reactive,0);
}

void world_village_population_free(WorldVillagePopulation *p){
    if(!p)return;
    for(auto *b:p->player_batches)if(b)world_model_pose_blend_free(b);
    for(auto &g:p->blends)if(g.batch)world_model_pose_blend_free(g.batch);
    for(auto &g:p->groups)if(g.asset)world_model_free(g.asset);
    delete p;
}
