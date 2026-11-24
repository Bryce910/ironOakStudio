#include "engine/world_cover.h"
#include "engine/world_atlas.h"
#include "engine/world_castle_dragons_layout.h"
#include "engine/world_ground_cover_quality.h"
#include <algorithm>
#include <array>
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
    size_t region_index=0;
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
void add(Field &field,WorldCoverKind kind,size_t capacity,const WorldModelInstance &instance,int64_t x,int64_t z,uint64_t salt,double dressing_protected_radius=0){
    double protected_radius=kind==WORLD_COVER_GRASS?WORLD_COVER_GRASS_PROTECTED_RADIUS:
                            kind==WORLD_COVER_TREES?WORLD_COVER_TREE_PROTECTED_RADIUS:dressing_protected_radius;
    double radius2=double(instance.x)*instance.x+double(instance.z)*instance.z;
    bool protected_root=protected_radius>0&&radius2<=protected_radius*protected_radius;
    uint64_t priority=hash(x,z,salt^0x6375727461696eull)&0x7fffffffffffffffull;
    if(!protected_root)priority|=0x8000000000000000ull;
    auto less=[&field](size_t a,size_t b){return field.priorities[a]<field.priorities[b];};
    if(field.pending.size()<capacity){
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
    std::array<WorldCoverRegion,WORLD_COVER_REGION_LIMIT> regions={};
    size_t region_count=0;bool authored_only=false;
    WorldCoverProtected protected_point=nullptr;const void *protected_data=nullptr;
    int region_at(int kind,double x,double z)const{return world_ecology::at_layer(regions.data(),region_count,x,z,kind);}

    float mask(int kind,double x,double z)const{
        if(!filter)return 1;
        const float v=filter(kind,x,z,filter_data);return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;
    }
    double detail_scale=1.,grass_radius=108.,fern_radius=150.;
    int quality=-1; // Explicit opt-in preserves legacy/offline fingerprints.
    size_t capacity(WorldCoverKind kind)const{
        if(quality<0)return capacities[kind];
        const auto p=world_ground_cover_quality::profile(quality);
        const size_t values[]={p.grass,p.trees,p.rocks,p.ferns};return values[kind];
    }
    double fern_step()const{return quality<0?8.:2.5;}
    double grass_candidates()const{return quality<0?1.:2.;}
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
// Every authored grid uses world coordinates and the region seed. Camera
// anchors only bound its work; they never move, reseed or respawn its roots.
int authored_kind(const Stage &s,bool tree){return tree?WORLD_COVER_TREES:s.phase==3?WORLD_COVER_GRASS:s.phase==4?WORLD_COVER_FERNS:WORLD_COVER_ROCKS;}
bool select_region_grid(WorldCover &c,Stage &s,bool tree){
    while(s.region_index<c.region_count){
        const auto &r=c.regions[s.region_index];const int kind=authored_kind(s,tree);
        const double radius=tree?tree_radius:kind==WORLD_COVER_GRASS?c.grass_radius:kind==WORLD_COVER_FERNS?c.fern_radius:230.;
        const double step=tree?8.:kind==WORLD_COVER_GRASS?3.:kind==WORLD_COVER_FERNS?(c.quality<0?5.:2.5):18.;
        const double margin=kind==WORLD_COVER_GRASS?3.:0.;
        const double lo_x=std::max(s.x-radius,r.x-r.radius-margin),hi_x=std::min(s.x+radius,r.x+r.radius+margin);
        const double lo_z=std::max(s.z-radius,r.z-r.radius-margin),hi_z=std::min(s.z+radius,r.z+r.radius+margin);
        bool covered=false;
        for(size_t i=s.region_index+1;i<c.region_count;++i){const auto &later=c.regions[i];
            if(world_ecology::owns(later,kind)&&std::hypot(r.x-later.x,r.z-later.z)+r.radius<=later.radius){covered=true;break;}}
        if(world_ecology::owns(r,kind)&&!covered&&r.density[kind]>0&&lo_x<=hi_x&&lo_z<=hi_z){
            s.step=step;s.radius=radius;
            s.minx=int64_t(std::floor(lo_x/step));s.maxx=int64_t(std::ceil(hi_x/step));
            s.minz=int64_t(std::floor(lo_z/step));s.maxz=int64_t(std::ceil(hi_z/step));
            s.cx=s.minx;s.cz=s.minz;s.tuft_count=s.tuft_next=0;
            s.total+=size_t(s.maxx-s.minx+1)*size_t(s.maxz-s.minz+1);return true;
        }
        if(tree||s.phase==5){++s.region_index;if(!tree)s.phase=3;}else ++s.phase;
    }
    s.cz=1;s.maxz=0;return false;
}
uint64_t region_salt(const WorldCoverRegion &r){return uint64_t(r.seed)*0x9e3779b97f4a7c15ull^0x65636f6c6f6779ull;}
bool authored_ground(const WorldCover &c,const WorldCoverRegion &r,int kind,double x,double z,float &height){
    if(world_atlas_local_water(x,z,nullptr,nullptr))return false;
    if(c.protected_point&&c.protected_point(kind,x,z,c.protected_data))return false;
    height=world_atlas_height(x,z);if(!std::isfinite(height)||height<=0||height>12000)return false;
    const double dx=(world_atlas_height(x+1,z)-world_atlas_height(x-1,z))*.5;
    const double dz=(world_atlas_height(x,z+1)-world_atlas_height(x,z-1))*.5;
    return std::isfinite(dx)&&std::isfinite(dz)&&std::atan(std::hypot(dx,dz))*180/pi<=r.max_slope;
}
void authored_grass_cell(WorldCover &c){
    Stage &s=c.near;const auto &r=c.regions[s.region_index];const auto salt=region_salt(r);
    const int64_t gx=s.cx,gz=s.cz;advance_cell(s);
    const double x=(gx+.5)*3,z=(gz+.5)*3,distance=std::hypot(x-s.x,z-s.z);
    if(distance>c.grass_radius+3||std::hypot(x-r.x,z-r.z)>r.radius+3)return;
    const double d=distance/20.,d2=d*d,patch=.35+.65*random01(gx/5,gz/5,salt+701);
    const double target=(.45+500/(1+d2*d2))*patch*c.grass_candidates();
    unsigned count=unsigned(std::floor(target));if(random01(gx,gz,salt+709)<target-count)++count;
    s.tuft_x=gx;s.tuft_z=gz;s.tuft_next=0;s.tuft_count=std::min(count,c.quality<0?512u:1024u);
}
void authored_grass_tuft(WorldCover &c){
    Stage &s=c.near;const auto &r=c.regions[s.region_index];const auto salt=region_salt(r);
    const unsigned index=s.tuft_next++;const int64_t gx=s.tuft_x,gz=s.tuft_z;
    const double cx=(gx+.5)*3+(random01(gx,gz,salt+11)-.5)*.75;
    const double cz=(gz+.5)*3+(random01(gx,gz,salt+13)-.5)*.75;
    const double angle=random01(gx,gz,salt+19)*2*pi+index*2.399963229728653;
    const double radius=.08+std::sqrt(random01(gx,gz,salt+101+index))*1.85;
    const double x=cx+std::cos(angle)*radius,z=cz+std::sin(angle)*radius;
    if(std::hypot(x-s.x,z-s.z)>c.grass_radius||c.region_at(WORLD_COVER_GRASS,x,z)!=int(s.region_index))return;
    const float density=world_ecology::density(r,WORLD_COVER_GRASS,x,z)*c.mask(WORLD_COVER_GRASS,x,z);
    if(density<=0||random01(gx,gz,salt+301+index)>=density)return;
    float height=0;if(!authored_ground(c,r,WORLD_COVER_GRASS,x,z,height))return;
    const float scale=float(.57+random01(gx,gz,salt+401+index)*.54)*(world_ecology::habitat(r)==2?.82f:1.f);
    add(c.fields[WORLD_COVER_GRASS],WORLD_COVER_GRASS,c.capacity(WORLD_COVER_GRASS),
        instance(x,z,s.x,s.z,height,scale,float(random01(gx,gz,salt+503+index)*2*pi)),gx,gz,salt+601+index);
}
// AAA merge: smooth value noise (bilinear over the same hash) so ferns grow in
// natural drifts instead of an even, polka-dot scatter. Mean is preserved.
double clump_noise(double x,double z,double cell,uint64_t salt){
    const double fx=x/cell,fz=z/cell;const int64_t ix=int64_t(std::floor(fx)),iz=int64_t(std::floor(fz));
    double tx=fx-double(ix),tz=fz-double(iz);tx=tx*tx*(3-2*tx);tz=tz*tz*(3-2*tz);
    const double a=random01(ix,iz,salt),b=random01(ix+1,iz,salt),c=random01(ix,iz+1,salt),d=random01(ix+1,iz+1,salt);
    return (a+(b-a)*tx)*(1-tz)+(c+(d-c)*tx)*tz;
}
void authored_object_cell(WorldCover &c,Stage &s,bool tree){
    const auto &r=c.regions[s.region_index];const int kind=authored_kind(s,tree);
    const auto salt=region_salt(r)+unsigned(kind)*1901;
    const int64_t gx=s.cx,gz=s.cz;advance_cell(s);
    const double x=(gx+.2+random01(gx,gz,salt+1)*.6)*s.step;
    const double z=(gz+.2+random01(gx,gz,salt+3)*.6)*s.step;
    if(std::hypot(x-s.x,z-s.z)>s.radius||c.region_at(kind,x,z)!=int(s.region_index))return;
    float density=world_ecology::density(r,kind,x,z)*c.mask(kind,x,z);
    double clump=1.;
    if(kind==WORLD_COVER_FERNS&&density>0){
        // Two octaves (~14 m drifts, ~5 m sub-clumps); dense centres, bare gaps.
        const double n=.65*clump_noise(x,z,14.,salt+8101)+.35*clump_noise(x,z,5.,salt+8203);
        clump=std::clamp((n-.38)/.30,0.,1.);clump=clump*clump*2.1;
        density=float(std::min(1.,double(density)*clump));
    }
    if(density<=0||random01(gx,gz,salt+5)>=density)return;
    float height=0;if(!authored_ground(c,r,kind,x,z,height))return;
    float scale=kind==WORLD_COVER_FERNS?float((.50+random01(gx,gz,salt+7)*.55)*(.80+.30*std::min(clump,1.5))):
                kind==WORLD_COVER_TREES?float(.8+random01(gx,gz,salt+7)*.5):
                kind==WORLD_COVER_FERNS?float(.55+random01(gx,gz,salt+7)*.65):float(.45+random01(gx,gz,salt+7)*.95);
    if(world_ecology::habitat(r)==2&&kind!=WORLD_COVER_ROCKS)scale*=1.15f;
    add(c.fields[kind],WorldCoverKind(kind),c.capacity(WorldCoverKind(kind)),instance(x,z,s.x,s.z,height,scale,float(random01(gx,gz,salt+9)*2*pi)),gx,gz,salt+11,kind==WORLD_COVER_FERNS?55.:0.);
}
void begin_near(WorldCover &c,double x,double z){
    Stage &s=c.near;
    if(s.active)c.cancelled++;
    s.initialized=s.active=true;s.x=x;s.z=z;s.phase=0;s.done=0;s.generation++;
    s.tuft_next=s.tuft_count=0;
    s.total=cell_count(x,z,c.grass_radius,3)+cell_count(x,z,c.fern_radius,c.fern_step())+cell_count(x,z,230,22);
    set_grid(s,3,c.grass_radius);
    if(c.authored_only){s.phase=3;s.region_index=0;s.total=0;select_region_grid(c,s,false);}
    clear_pending(c,WORLD_COVER_GRASS,s);clear_pending(c,WORLD_COVER_ROCKS,s);clear_pending(c,WORLD_COVER_FERNS,s);
}
void begin_trees(WorldCover &c,double x,double z){
    Stage &s=c.trees;
    if(s.active)c.cancelled++;
    s.initialized=s.active=true;s.x=x;s.z=z;s.phase=0;s.done=0;s.generation++;
    s.total=cell_count(x,z,tree_radius,tree_step_m);set_grid(s,tree_step_m,tree_radius);
    if(c.authored_only){s.phase=1;s.region_index=0;s.total=0;select_region_grid(c,s,true);}
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
    if(std::hypot(x-s.x,z-s.z)>c.grass_radius)return;
    if(c.region_count&&c.region_at(WORLD_COVER_GRASS,x,z)>=0)return;
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
    add(c.fields[WORLD_COVER_GRASS],WORLD_COVER_GRASS,c.capacity(WORLD_COVER_GRASS),
        instance(x,z,s.x,s.z,a.height,scale,float(random01(gx,gz,503+index)*2*pi)),gx,gz,601+index);
}
void prepare_grass_cell(WorldCover &c){
    Stage &s=c.near;int64_t gx=s.cx,gz=s.cz;advance_cell(s);
    double x=(gx+.5)*3,z=(gz+.5)*3,distance=std::hypot(x-s.x,z-s.z);
    if(distance>c.grass_radius+2)return;
    if(c.region_count)for(size_t i=0;i<c.region_count;++i){const auto &r=c.regions[i];
        if(world_ecology::owns(r,WORLD_COVER_GRASS)&&std::hypot(x-r.x,z-r.z)+3<r.radius)return;}
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
    double target=(.45+500/(1+d2*d2))*patch*c.grass_candidates();
    unsigned count=unsigned(std::floor(target));
    if(random01(gx,gz,709)<target-count)count++;
    s.tuft_x=gx;s.tuft_z=gz;s.tuft_next=0;s.tuft_count=std::min(count,c.quality<0?512u:1024u);
}
void near_dressing_cell(WorldCover &c){
    Stage &s=c.near;int64_t gx=s.cx,gz=s.cz;advance_cell(s);
    WorldCoverKind kind=s.phase==1?WORLD_COVER_FERNS:WORLD_COVER_ROCKS;
    uint64_t salt=kind==WORLD_COVER_FERNS?1101:1301;
    double x=(gx+.15+random01(gx,gz,salt)*.70)*s.step;
    double z=(gz+.15+random01(gx,gz,salt+2)*.70)*s.step;
    if(std::hypot(x-s.x,z-s.z)>s.radius)return;
    if(c.region_count&&c.region_at(kind,x,z)>=0)return;
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
    if(kind==WORLD_COVER_FERNS&&probability>0){   // AAA merge: drifts, not polka dots
        const double n=.65*clump_noise(x,z,14.,8101)+.35*clump_noise(x,z,5.,8203);
        double clump=std::clamp((n-.38)/.30,0.,1.);probability=std::min(1.,probability*clump*clump*2.1);
    }
    if(probability<=0||random01(gx,gz,salt+4)>probability)return;
    float scale=float(kind==WORLD_COVER_FERNS?.55+random01(gx,gz,salt+6)*.55:.45+random01(gx,gz,salt+6)*.95);
    if(rainforest>0)scale*=1+.28f*rainforest;
    // Prefer the visible fern floor over distant dressing. Explicit density
    // tiers share a finer grid and rank, so increasing capacity adds roots
    // without moving any of the retained lower-tier plants.
    add(c.fields[kind],kind,c.capacity(kind),instance(x,z,s.x,s.z,a.height,scale,float(random01(gx,gz,salt+8)*2*pi)),gx,gz,salt+10,55.0*rainforest);
}
void near_step(WorldCover &c){
    Stage &s=c.near;
    if(s.phase>=3){
        if(s.phase==3&&s.tuft_next<s.tuft_count){authored_grass_tuft(c);return;}
        if(s.cz>s.maxz){
            if(s.region_index>=c.region_count){finish_near(c);return;}
            if(s.phase==5){++s.region_index;s.phase=3;}else ++s.phase;
            if(!select_region_grid(c,s,false)){finish_near(c);return;}
        }
        if(s.phase==3)authored_grass_cell(c);else authored_object_cell(c,s,false);
        return;
    }
    if(s.phase==0&&s.tuft_next<s.tuft_count){grass_tuft(c);return;}
    if(s.cz>s.maxz){
        if(++s.phase==1)set_grid(s,c.fern_step(),c.fern_radius);
        else if(s.phase==2)set_grid(s,22,230);
        else{if(c.region_count){s.phase=3;s.region_index=0;if(select_region_grid(c,s,false))return;}finish_near(c);return;}
    }
    if(s.phase==0)prepare_grass_cell(c);else near_dressing_cell(c);
}
void tree_step(WorldCover &c){
    Stage &s=c.trees;
    if(s.phase==1){
        if(s.cz>s.maxz){
            if(s.region_index<c.region_count)++s.region_index;
            if(!select_region_grid(c,s,true)){s.active=false;c.fields[WORLD_COVER_TREES].ready=true;return;}
        }
        authored_object_cell(c,s,true);return;
    }
    if(s.cz>s.maxz){
        if(c.region_count){s.phase=1;s.region_index=0;if(select_region_grid(c,s,true))return;}
        s.active=false;c.fields[WORLD_COVER_TREES].ready=true;return;
    }
    int64_t gx=s.cx,gz=s.cz;advance_cell(s);
    double x=(gx+.20+random01(gx,gz,1701)*.60)*tree_step_m;
    double z=(gz+.20+random01(gx,gz,1703)*.60)*tree_step_m;
    if(std::hypot(x-s.x,z-s.z)>tree_radius)return;
    if(c.region_count&&c.region_at(WORLD_COVER_TREES,x,z)>=0)return;
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
    add(c.fields[WORLD_COVER_TREES],WORLD_COVER_TREES,c.capacity(WORLD_COVER_TREES),
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
bool world_cover_set_detail_scale(WorldCover *c,float scale){
    if(!c||!std::isfinite(scale)||scale<.65f||scale>2.f)return false;
    if(c->detail_scale==scale)return true;
    c->detail_scale=scale;c->grass_radius=90.*scale+18.;c->fern_radius=150.*scale;
    // Retain published fields while the bounded near generator catches up.
    if(c->near.active)c->cancelled++;
    c->near.initialized=c->near.active=false;
    for(auto kind:{WORLD_COVER_GRASS,WORLD_COVER_ROCKS,WORLD_COVER_FERNS}){c->fields[kind].pending.clear();c->fields[kind].ready=false;}
    return true;
}
bool world_cover_set_quality(WorldCover *c,int tier){
    if(!c||tier<0||tier>3)return false;
    if(c->quality==tier)return true;
    const auto p=world_ground_cover_quality::profile(tier);
    // Reserve only private staging buffers: successful published pointers must
    // survive a menu Apply until the replacement field is actually polled.
    try{for(auto kind:{WORLD_COVER_GRASS,WORLD_COVER_FERNS}){
        auto &f=c->fields[kind];const size_t cap=kind==WORLD_COVER_GRASS?p.grass:p.ferns;
        f.pending.reserve(cap);f.priorities.reserve(cap);f.priority_heap.reserve(cap);
    }}catch(const std::bad_alloc&){return false;}
    c->quality=tier;
    if(c->near.active)c->cancelled++;
    c->near.initialized=c->near.active=false;
    for(auto kind:{WORLD_COVER_GRASS,WORLD_COVER_ROCKS,WORLD_COVER_FERNS}){
        c->fields[kind].pending.clear();c->fields[kind].ready=false;
    }
    return true;
}
void world_cover_set_filter(WorldCover *c,WorldCoverFilter f,const void *data){if(!c)return;c->filter=f;c->filter_data=data;world_cover_invalidate(c);}
bool world_cover_set_regions(WorldCover *c,const WorldCoverRegion *regions,size_t count,
                             bool authored_only,WorldCoverProtected fn,const void *data){
    if(!c||count>WORLD_COVER_REGION_LIMIT||(count&&!regions))return false;
    for(size_t i=0;i<count;++i)if(!world_ecology::valid(regions[i]))return false;
    for(size_t i=0;i<count;++i)c->regions[i]=regions[i];
    c->region_count=count;c->authored_only=authored_only;c->protected_point=fn;c->protected_data=data;
    world_cover_invalidate(c);return true;
}
int world_cover_region_preset(const WorldCoverRegion *regions,size_t count,double x,double z){
    const int at=world_ecology::at_layer(regions,count,x,z,WORLD_COVER_TREES);
    return at<0?-1:int(world_ecology::habitat(regions[unsigned(at)]));
}
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
    budget=std::min(budget,size_t(cover->region_count?1024:4096));size_t done=0;
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
