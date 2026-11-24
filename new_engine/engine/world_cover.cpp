#include "engine/world_cover.h"
#include "engine/world_atlas.h"
#include "engine/world_castle_dragons_layout.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double grass_radius=108.0; // 90m visible + nearest24m-anchor guard.
constexpr double tree_radius=1750.0;
constexpr double tree_step_m=18.0;
constexpr size_t capacities[]={WORLD_COVER_GRASS_CAP,WORLD_COVER_TREE_CAP,WORLD_COVER_ROCK_CAP,WORLD_COVER_FERN_CAP};
uint64_t hash(int64_t x,int64_t z,uint64_t salt){
    uint64_t h=uint64_t(x)*0x9e3779b97f4a7c15ull^uint64_t(z)*0xbf58476d1ce4e5b9ull^salt*0x94d049bb133111ebull;
    h^=h>>30;h*=0xbf58476d1ce4e5b9ull;h^=h>>27;h*=0x94d049bb133111ebull;return h^(h>>31);
}
double random01(int64_t x,int64_t z,uint64_t salt){return double(hash(x,z,salt)>>11)*(1.0/9007199254740992.0);}
double roost_clearance(double x,double z){double ox,oz;world_atlas_village_origin(&ox,&oz);return world_castle_dragons_ground_exclusion_local(x-ox,z-oz);}
double smooth(double a,double b,double value){double t=std::max(0.0,std::min(1.0,(value-a)/(b-a)));return t*t*(3-2*t);}
struct Field {
    std::vector<WorldModelInstance> live,pending;
    std::vector<uint64_t> priorities;
    std::vector<size_t> priority_heap;
    bool ready=false;
    double x=0,z=0;
    uint64_t generation=0;
};
struct Stage {
    bool initialized=false,active=false;
    double x=0,z=0,step=3,radius=grass_radius;
    int phase=0;
    int64_t minx=0,maxx=0,minz=0,maxz=0,cx=0,cz=0;
    int64_t tuft_x=0,tuft_z=0;
    unsigned tuft_next=0,tuft_count=0;
    size_t done=0,total=0;
    uint64_t generation=0;
};
size_t cell_count(double x,double z,double radius,double step){
    return size_t(std::ceil((x+radius)/step)-std::floor((x-radius)/step)+1)
          *size_t(std::ceil((z+radius)/step)-std::floor((z-radius)/step)+1);
}
void set_grid(Stage &s,double step,double radius){
    s.step=step;s.radius=radius;
    s.minx=int64_t(std::floor((s.x-radius)/step));s.maxx=int64_t(std::ceil((s.x+radius)/step));
    s.minz=int64_t(std::floor((s.z-radius)/step));s.maxz=int64_t(std::ceil((s.z+radius)/step));
    s.cx=s.minx;s.cz=s.minz;
}
void advance_cell(Stage &s){s.done++;if(++s.cx>s.maxx){s.cx=s.minx;++s.cz;}}
bool dry(const WorldAtlasSample &a,double x,double z){return !world_atlas_local_water(x,z,nullptr,nullptr)&& std::isfinite(a.height)&&a.height>2&&a.shore_distance>8&&a.land_weight>.75f;}
void add(Field &field,WorldCoverKind kind,const WorldModelInstance &instance,int64_t x,int64_t z,uint64_t salt,double dressing_protected_radius=0){
    double protected_radius=kind==WORLD_COVER_GRASS?WORLD_COVER_GRASS_PROTECTED_RADIUS:
                            kind==WORLD_COVER_TREES?WORLD_COVER_TREE_PROTECTED_RADIUS:dressing_protected_radius;
    double radius2=double(instance.x)*instance.x+double(instance.z)*instance.z;
    bool protected_root=protected_radius>0&&radius2<=protected_radius*protected_radius;
    uint64_t priority=hash(x,z,salt^0x6375727461696eull)&0x7fffffffffffffffull;
    if(!protected_root)priority|=0x8000000000000000ull;
    auto less=[&field](size_t a,size_t b){return field.priorities[a]<field.priorities[b];};
    if(field.pending.size()<capacities[kind]){
        size_t slot=field.pending.size();
        field.pending.push_back(instance);field.priorities.push_back(priority);
        field.priority_heap.push_back(slot);
        std::push_heap(field.priority_heap.begin(),field.priority_heap.end(),less);
    }else if(priority<field.priorities[field.priority_heap.front()]){
        // The max-heap keeps the worst retained world priority at its root.
        // Shifted generation bounds cannot evict protected roots in favour of
        // far roots just because their scan order / candidate count changed.
        std::pop_heap(field.priority_heap.begin(),field.priority_heap.end(),less);
        size_t slot=field.priority_heap.back();
        field.pending[slot]=instance;field.priorities[slot]=priority;
        std::push_heap(field.priority_heap.begin(),field.priority_heap.end(),less);
    }
}
WorldModelInstance instance(double x,double z,double anchor_x,double anchor_z,float height,float scale,float yaw){
    return {float(x-anchor_x),height,float(z-anchor_z),yaw,scale};
}
}

struct WorldCover {
    WorldCoverFilter filter=nullptr;const void *filter_data=nullptr;
    float mask(int kind,double x,double z)const{
        if(!filter)return 1;
        const float v=filter(kind,x,z,filter_data);return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;
    }
    Field fields[WORLD_COVER_KIND_COUNT];
    Stage near,trees;
    uint64_t cancelled=0;
    unsigned turn=0,poll_cursor=0;
};

namespace {
void clear_pending(WorldCover &c,WorldCoverKind kind,const Stage &s){
    Field &field=c.fields[kind];field.pending.clear();field.ready=false;
    field.priorities.clear();field.priority_heap.clear();
    field.x=s.x;field.z=s.z;field.generation=s.generation;
}
void begin_near(WorldCover &c,double x,double z){
    Stage &s=c.near;
    if(s.active)c.cancelled++;
    s.initialized=s.active=true;s.x=x;s.z=z;s.phase=0;s.done=0;s.generation++;
    s.tuft_next=s.tuft_count=0;
    s.total=cell_count(x,z,grass_radius,3)+cell_count(x,z,150,8)+cell_count(x,z,230,22);
    set_grid(s,3,grass_radius);
    clear_pending(c,WORLD_COVER_GRASS,s);clear_pending(c,WORLD_COVER_ROCKS,s);clear_pending(c,WORLD_COVER_FERNS,s);
}
void begin_trees(WorldCover &c,double x,double z){
    Stage &s=c.trees;
    if(s.active)c.cancelled++;
    s.initialized=s.active=true;s.x=x;s.z=z;s.phase=0;s.done=0;s.generation++;
    s.total=cell_count(x,z,tree_radius,tree_step_m);set_grid(s,tree_step_m,tree_radius);
    clear_pending(c,WORLD_COVER_TREES,s);
}
void finish_near(WorldCover &c){
    c.near.active=false;
    c.fields[WORLD_COVER_GRASS].ready=true;c.fields[WORLD_COVER_ROCKS].ready=true;c.fields[WORLD_COVER_FERNS].ready=true;
}
void grass_tuft(WorldCover &c){
    Stage &s=c.near;unsigned index=s.tuft_next++;
    int64_t gx=s.tuft_x,gz=s.tuft_z;
    double cx=(gx+.5)*3+(random01(gx,gz,11)-.5)*.75;
    double cz=(gz+.5)*3+(random01(gx,gz,13)-.5)*.75;
    double angle=random01(gx,gz,19)*2*pi+index*2.399963229728653;
    double radius=.08+std::sqrt(random01(gx,gz,101+index))*1.85;
    double x=cx+std::cos(angle)*radius,z=cz+std::sin(angle)*radius;
    if(std::hypot(x-s.x,z-s.z)>grass_radius)return;
    WorldAtlasSample a=world_atlas_sample(x,z);
    if(!dry(a,x,z)||a.height>1450)return;
    WorldAtlasLandscapeSample ecology=world_atlas_landscape_mask(x,z);
    if(ecology.trail>.01f)return;
    WorldAtlasStartingSample start=world_atlas_starting_sample(x,z);
    if(start.path>.01||start.paving>.12||start.grass_exclusion>.01)return;
    double fertility=(.60+.40*a.moisture)*(1-.35*a.forest_density)*(1-smooth(1000,1450,a.height));
    // The same authored masks shade the ground and place its ecology. Exposed
    // headwalls stay rocky while healthy grass fills the sunlit glade margins.
    fertility=std::min(1.0,fertility*(1+.04*ecology.meadow)*(1-smooth(.12,.78,ecology.rock))*(1-start.clearing*.50));
    // Deep shade keeps a fern/moss floor rather than a bright meadow crop.
    // Keep the zero-weight arithmetic and every existing world seed intact.
    float rainforest=a.region==WORLD_ATLAS_OLD_TANTHOON?world_atlas_rainforest_weight(x,z):0;
    if(rainforest>0)fertility*=1-.82*rainforest;
    const double roost=roost_clearance(x,z);if(roost>0)fertility*=1-.90*roost;
    fertility*=c.mask(WORLD_COVER_GRASS,x,z);
    if(fertility<=0||random01(gx,gz,301+index)>fertility)return;
    float scale=float(.57+random01(gx,gz,401+index)*.54);
    scale*=float(.88+.12*random01(gx,gz,409));
    add(c.fields[WORLD_COVER_GRASS],WORLD_COVER_GRASS,
        instance(x,z,s.x,s.z,a.height,scale,float(random01(gx,gz,503+index)*2*pi)),gx,gz,601+index);
}
void prepare_grass_cell(WorldCover &c){
    Stage &s=c.near;int64_t gx=s.cx,gz=s.cz;advance_cell(s);
    double x=(gx+.5)*3,z=(gz+.5)*3,distance=std::hypot(x-s.x,z-s.z);
    if(distance>grass_radius+2)return;
    // A hierarchical tuft cell retains its prefix of rooted instances at every
    // density/distance. Concentrate detail near the camera instead of spending
    // most of the instance cap on subpixel blades near the edge of the field.
    double px=double(gx)/5,pz=double(gz)/5;
    int64_t ix=int64_t(std::floor(px)),iz=int64_t(std::floor(pz));
    double fx=px-ix,fz=pz-iz;fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);
    double north=random01(ix,iz,701)*(1-fx)+random01(ix+1,iz,701)*fx;
    double south=random01(ix,iz+1,701)*(1-fx)+random01(ix+1,iz+1,701)*fx;
    double patch=.35+.65*(north*(1-fz)+south*fz);
    double d=distance/20.0,d2=d*d;
    double target=(.45+500/(1+d2*d2))*patch;
    unsigned count=unsigned(std::floor(target));
    if(random01(gx,gz,709)<target-count)count++;
    s.tuft_x=gx;s.tuft_z=gz;s.tuft_next=0;s.tuft_count=std::min(count,512u);
}
void near_dressing_cell(WorldCover &c){
    Stage &s=c.near;int64_t gx=s.cx,gz=s.cz;advance_cell(s);
    WorldCoverKind kind=s.phase==1?WORLD_COVER_FERNS:WORLD_COVER_ROCKS;
    uint64_t salt=kind==WORLD_COVER_FERNS?1101:1301;
    double x=(gx+.15+random01(gx,gz,salt)*.70)*s.step;
    double z=(gz+.15+random01(gx,gz,salt+2)*.70)*s.step;
    if(std::hypot(x-s.x,z-s.z)>s.radius)return;
    WorldAtlasSample a=world_atlas_sample(x,z);
    if(!dry(a,x,z)||roost_clearance(x,z)>0)return;
    WorldAtlasLandscapeSample ecology=world_atlas_landscape_mask(x,z);
    if(ecology.trail>.01f)return;
    WorldAtlasStartingSample start=world_atlas_starting_sample(x,z);
    if(start.path>.01||start.clearing>.30)return;
    // Authored urban yards own their large props. A boulder's root can sit
    // outside a house mask while its mesh crosses a wall, so reject procedural
    // rocks/ferns over the whole built terrace, as in the earlier waystation.
    if(world_atlas_village_weight(x,z)>.30f)return;
    double probability=kind==WORLD_COVER_FERNS?a.forest_density*.72*(.35+.65*a.moisture):.36;
    if(kind==WORLD_COVER_FERNS)
        probability*=((1-.96*ecology.meadow)*(1-smooth(.25,.85,ecology.rock))*(1+.12*ecology.moss));
    else
        probability*=1+.50*ecology.rock;
    float rainforest=kind==WORLD_COVER_FERNS&&a.region==WORLD_ATLAS_OLD_TANTHOON?world_atlas_rainforest_weight(x,z):0;
    if(rainforest>0)probability=std::min(1.0,probability*(1+1.30*rainforest));
    probability*=c.mask(kind,x,z);
    if(probability<=0||random01(gx,gz,salt+4)>probability)return;
    float scale=float(kind==WORLD_COVER_FERNS?.55+random01(gx,gz,salt+6)*.55:.45+random01(gx,gz,salt+6)*.95);
    if(rainforest>0)scale*=1+.28f*rainforest;
    // At most256 candidate cells intersect this55m circle on the8m grid,
    // below the360 cap. Keep close ferns instead of spending the same cap on
    // roots beyond their visible range; overlapping anchors retain them all.
    add(c.fields[kind],kind,instance(x,z,s.x,s.z,a.height,scale,float(random01(gx,gz,salt+8)*2*pi)),gx,gz,salt+10,55.0*rainforest);
}
void near_step(WorldCover &c){
    Stage &s=c.near;
    if(s.phase==0&&s.tuft_next<s.tuft_count){grass_tuft(c);return;}
    if(s.cz>s.maxz){
        if(++s.phase==1)set_grid(s,8,150);
        else if(s.phase==2)set_grid(s,22,230);
        else{finish_near(c);return;}
    }
    if(s.phase==0)prepare_grass_cell(c);else near_dressing_cell(c);
}
void tree_step(WorldCover &c){
    Stage &s=c.trees;
    if(s.cz>s.maxz){s.active=false;c.fields[WORLD_COVER_TREES].ready=true;return;}
    int64_t gx=s.cx,gz=s.cz;advance_cell(s);
    double x=(gx+.20+random01(gx,gz,1701)*.60)*tree_step_m;
    double z=(gz+.20+random01(gx,gz,1703)*.60)*tree_step_m;
    if(std::hypot(x-s.x,z-s.z)>tree_radius)return;
    WorldAtlasSample a=world_atlas_sample(x,z);
    if(!dry(a,x,z)||a.height>1720||roost_clearance(x,z)>0)return;
    WorldAtlasLandscapeSample ecology=world_atlas_landscape_mask(x,z);
    if(ecology.trail>.01f)return;
    WorldAtlasStartingSample start=world_atlas_starting_sample(x,z);
    if(start.path>.01||start.clearing>.01)return;
    // Continuous woodland masses with clearings, rather than a uniform orchard.
    double grove=.50+.50*random01(int64_t(std::floor(double(gx)/7)),int64_t(std::floor(double(gz)/7)),1721);
    double density=a.forest_density*grove*(1-smooth(.18,.75,ecology.rock))*(1-smooth(.15,.85,ecology.meadow));
    float rainforest=a.region==WORLD_ATLAS_OLD_TANTHOON?world_atlas_rainforest_weight(x,z):0;
    if(rainforest>0)density=std::min(1.0,density*(1+.18*rainforest));
    density*=c.mask(WORLD_COVER_TREES,x,z);
    if(density<=0||random01(gx,gz,1705)>density*.94)return;
    float scale=float(.80+random01(gx,gz,1707)*.50);
    if(rainforest>0)scale*=1+.10f*rainforest;
    add(c.fields[WORLD_COVER_TREES],WORLD_COVER_TREES,
        instance(x,z,s.x,s.z,a.height,scale,float(random01(gx,gz,1709)*2*pi)),gx,gz,1711);
}
}

extern "C" {
WorldCover *world_cover_init(void){
    try{
        WorldCover *cover=new WorldCover;
        try{for(size_t i=0;i<WORLD_COVER_KIND_COUNT;i++){
            cover->fields[i].live.reserve(capacities[i]);cover->fields[i].pending.reserve(capacities[i]);
            cover->fields[i].priorities.reserve(capacities[i]);cover->fields[i].priority_heap.reserve(capacities[i]);
        }}
        catch(...){delete cover;return nullptr;}
        return cover;
    }catch(const std::bad_alloc&){return nullptr;}
}
void world_cover_shutdown(WorldCover *cover){delete cover;}
void world_cover_invalidate(WorldCover *c){if(!c)return;c->near.initialized=c->trees.initialized=false;
    if(c->near.active)c->cancelled++;
    if(c->trees.active)c->cancelled++;
    c->near.active=c->trees.active=false;for(auto &f:c->fields){f.pending.clear();f.ready=false;}
}
void world_cover_set_filter(WorldCover *c,WorldCoverFilter f,const void *data){if(!c)return;c->filter=f;c->filter_data=data;world_cover_invalidate(c);}
bool world_cover_update(WorldCover *cover,double x,double z){
    if(!cover||!std::isfinite(x)||!std::isfinite(z)||std::fabs(x)>1e8||std::fabs(z)>1e8)return false;
    double nx=std::floor(x/24+.5)*24,nz=std::floor(z/24+.5)*24;
    double tx=std::floor(x/128+.5)*128,tz=std::floor(z/128+.5)*128;
    if(!cover->near.initialized||nx!=cover->near.x||nz!=cover->near.z)begin_near(*cover,nx,nz);
    if(!cover->trees.initialized||tx!=cover->trees.x||tz!=cover->trees.z)begin_trees(*cover,tx,tz);
    return true;
}
size_t world_cover_pump(WorldCover *cover,size_t budget){
    if(!cover)return 0;
    budget=std::min(budget,size_t(4096));size_t done=0;
    while(done<budget&&(cover->near.active||cover->trees.active)){
        bool use_tree=cover->trees.active&&(!cover->near.active||((cover->turn++&1u)!=0));
        if(use_tree)tree_step(*cover);else near_step(*cover);
        done++;
    }
    return done;
}
bool world_cover_poll(WorldCover *cover,WorldCoverField *out){
    if(!cover||!out)return false;
    for(unsigned i=0;i<WORLD_COVER_KIND_COUNT;i++){
        unsigned index=(cover->poll_cursor+i)%WORLD_COVER_KIND_COUNT;Field &field=cover->fields[index];
        if(!field.ready)continue;
        field.live.swap(field.pending);field.pending.clear();field.ready=false;
        *out={WorldCoverKind(index),field.generation,field.x,field.z,field.live.size(),field.live.data()};
        cover->poll_cursor=(index+1)%WORLD_COVER_KIND_COUNT;return true;
    }
    return false;
}
void world_cover_metrics(const WorldCover *cover,WorldCoverMetrics *out){
    if(!out)return;
    *out={};if(!cover)return;
    out->near_done=cover->near.done;out->near_total=cover->near.total;
    out->tree_done=cover->trees.done;out->tree_total=cover->trees.total;
    out->near_generation=cover->near.generation;out->tree_generation=cover->trees.generation;
    out->cancelled_generations=cover->cancelled;out->near_building=cover->near.active;out->tree_building=cover->trees.active;
}
}
