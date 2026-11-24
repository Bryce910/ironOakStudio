#include "engine/world_atlas.h"
#include "engine/world_height_region.h"
#include "engine/world_castle_creek.h"
#include "engine/world_castle_dragons_layout.h"
#include "engine/world_demo_polish_layout.h"
#include "engine/world_demo_contours.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS_COLS 1025
#define ATLAS_ROWS 577
#define ATLAS_WIDTH 100800.0
#define ATLAS_DEPTH (ATLAS_WIDTH * 2818.0 / 5009.0)
#define LANDSCAPE_SIDE 513
#define DISTRICT_COUNT 3
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
typedef struct { double u,v; } MapPoint;
typedef struct { const MapPoint *points; size_t count; } MapPolygon;
typedef struct { double u,v,radius; } RiverPoint;
typedef struct { const RiverPoint *points; size_t count; } River;
static struct {
    void *storage;
    float *height,*shore,*detail;
    uint8_t *region,*forest,*moisture;
} atlas;
static WorldHeightRegion external_height_region,fallback_height_region;
typedef struct {float delta;uint8_t rock,meadow,moss,pad;} LandscapeCell;
static LandscapeCell *landscape;
typedef struct {float delta,offset;} ShoreCell;
static ShoreCell *shore_relief;
static bool shore_relief_ready;
typedef struct {double r,f,hr,hf,height,sr,sf,feather,paving,surface_hr,surface_hf;} StartingPad;
typedef struct {double ar,af,br,bf,width,feather;} StartingPath;
typedef struct {double r,f,ir,if_,or_,of;} StartingClearing;
typedef struct {double x,z,xmin,xmax,zmin,zmax,cosine,sine;} StartingExclusion;
#include "engine/world_starting_layout.inc"
typedef struct {float delta;uint8_t path,paving,clearing,grass;} StartingCell;
static StartingCell *starting;
static bool starting_ready;
#define VILLAGE_X ((.487-.5)*ATLAS_WIDTH)
#define VILLAGE_Z ((.320-.5)*ATLAS_DEPTH)
typedef struct {double ax,az,bx,bz,width,paving;} VillageRoad;
typedef struct {double x,z,hx,hz,cosine,sine,paving,grass;} VillageRect;
#include "engine/world_village_layout.inc"
#include "engine/world_castle_streets_ground.inc"
#include "engine/world_demo_finish_ground.inc"
#include "engine/world_village_farms.h"
typedef struct {uint8_t path,paving,clearing,grass;} VillageCell;
static VillageCell *village;
static bool village_ready;
#define BANDIT_MASK_SIDE 257
#include "engine/world_bandit_ground.inc"
static VillageCell *bandit;
static bool bandit_ready;
#define DISTRICT(ID,NAME,U0,V0,U1,V1) {ID,NAME,((U0)-.5)*ATLAS_WIDTH,((V0)-.5)*ATLAS_DEPTH,((U1)-(U0))*ATLAS_WIDTH,((V1)-(V0))*ATLAS_DEPTH}
static const WorldAtlasDistrict districts[DISTRICT_COUNT]={
    DISTRICT("ulasse_headwall","Ulasse crown and glacial headwall",.29,.035,.47,.235),
    DISTRICT("valwood_trail","Valwood glade and winding trail valley",.485,.220,.530,.285),
    DISTRICT("crosswind_headlands","Crosswind rocky headlands",.490,.875,.580,.947)
};
/* Fine shoreline geometry is separate from the broad material districts.
 * Review banks and small coves need 3–6 m samples, rather than 35 m cells. */
static const WorldAtlasDistrict shore_patches[]={
    DISTRICT("ulasse_shore","Ulasse eastern gravel banks",.4485,.168,.4635,.192),
    DISTRICT("crosswind_shore","Crosswind headland shore shelves",.510,.914,.540,.938)
};
#define TRAIL(U,V,W) {((U)-.5)*ATLAS_WIDTH,((V)-.5)*ATLAS_DEPTH,W}
/* A walkable surface and its framing valley share one reusable authored spine.
 * Short segments make the exact path cheap for placement and fragment shading. */
static const WorldAtlasTrailPoint trail_points[]={
    TRAIL(.5120,.2540,1.85f),TRAIL(.5110,.2520,1.85f),
    TRAIL(.5101,.2495,1.75f),TRAIL(.5084,.2465,1.75f),
    TRAIL(.5056,.2438,1.65f),TRAIL(.5019,.2408,1.75f),
    TRAIL(.4986,.2398,1.70f),TRAIL(.4952,.2413,1.65f),
    TRAIL(.4928,.2444,1.60f)
};
#define RAINFOREST_X ((.235-.5)*ATLAS_WIDTH)
#define RAINFOREST_Z ((.670-.5)*ATLAS_DEPTH)
#define RAIN_TRAIL(X,Z) {RAINFOREST_X+(X),RAINFOREST_Z+(Z),1.6f}
static const WorldAtlasTrailPoint rainforest_trail[]={
    RAIN_TRAIL(-15,95),RAIN_TRAIL(2,60),RAIN_TRAIL(-8,20),
    RAIN_TRAIL(5,-25),RAIN_TRAIL(-5,-65),RAIN_TRAIL(12,-110)
};

static const WorldAtlasPool rainforest_pool={RAINFOREST_X-22,RAINFOREST_Z-25,10,16,134,6};
/* Hand traced from Valwood-Map.jpg. Decorative frame/text are not terrain.
 * Geography is deliberately independent of the small authored Seldonne scene. */
static const MapPoint mainland[]={
    {.015,.030},{.985,.030},{.985,.627},{.952,.664},{.928,.693},
    {.895,.712},{.879,.752},{.854,.789},{.820,.805},{.800,.847},
    {.756,.876},{.702,.900},{.658,.900},{.618,.887},{.567,.919},
    {.513,.933},{.459,.920},{.441,.905},{.465,.854},{.489,.813},
    {.496,.747},{.511,.705},{.541,.651},{.570,.632},{.593,.604},
    {.597,.541},{.578,.494},{.573,.444},{.584,.400},{.607,.355},
    {.611,.322},{.591,.329},{.570,.363},{.565,.389},{.527,.390},
    {.496,.375},{.459,.366},{.430,.371},{.397,.371},{.366,.385},
    {.350,.413},{.324,.439},{.290,.464},{.256,.463},{.229,.455},
    {.218,.442},{.191,.447},{.162,.429},{.141,.433},{.111,.455},
    {.074,.473},{.031,.471},{.015,.463}
};
static const MapPoint tanthoon[]={
    {.015,.535},{.043,.527},{.068,.502},{.093,.496},{.121,.515},
    {.152,.523},{.185,.512},{.219,.520},{.253,.506},{.285,.523},
    {.309,.508},{.333,.490},{.351,.476},{.356,.496},{.336,.531},
    {.315,.551},{.319,.578},{.342,.603},{.361,.626},{.386,.650},
    {.417,.669},{.441,.693},{.465,.711},{.460,.752},{.440,.775},
    {.416,.780},{.396,.804},{.363,.819},{.349,.858},{.331,.869},
    {.315,.855},{.303,.852},{.279,.838},{.265,.813},{.243,.810},
    {.222,.825},{.203,.851},{.180,.844},{.165,.855},{.147,.860},
    {.132,.837},{.120,.793},{.119,.742},{.108,.701},{.102,.671},
    {.076,.647},{.049,.640},{.033,.619},{.015,.606}
};
static const MapPoint seldonne_island[]={
    {.369,.461},{.397,.431},{.423,.432},{.446,.447},{.477,.454},
    {.515,.456},{.546,.474},{.562,.489},{.577,.510},{.585,.535},
    {.580,.571},{.565,.600},{.534,.622},{.514,.647},{.485,.670},
    {.456,.660},{.430,.641},{.402,.623},{.379,.616},{.358,.594},
    {.341,.563},{.328,.542},{.342,.511},{.355,.489}
};
static const MapPoint bellweather[]={
    {.909,.789},{.952,.787},{.988,.803},{.990,.939},
    {.962,.926},{.928,.907},{.900,.899},{.902,.842}
};
static const MapPoint estuay_island[]={
    {.613,.514},{.627,.509},{.640,.524},{.642,.552},
    {.632,.575},{.615,.576},{.605,.555},{.605,.532}
};
static const MapPolygon lands[]={
    {mainland,COUNT(mainland)},{tanthoon,COUNT(tanthoon)},
    {seldonne_island,COUNT(seldonne_island)},
    {bellweather,COUNT(bellweather)},{estuay_island,COUNT(estuay_island)}
};
static const MapPoint ulasse_lake[]={
    {.377,.128},{.409,.116},{.446,.116},{.457,.133},{.452,.196},
    {.437,.227},{.404,.225},{.372,.206},{.368,.173}
};
static const MapPoint callieut_lake[]={
    {.145,.105},{.197,.101},{.208,.110},{.172,.137},{.138,.137}
};
static const MapPolygon lakes[]={
    {ulasse_lake,COUNT(ulasse_lake)},
    {callieut_lake,COUNT(callieut_lake)}
};
/* Widths are meters. Broad central waterways follow the coast polygons above;
 * these paths carve their connected tributaries, with sea-level water for the
 * base terrain. This is geography, not a complete hydrological simulation. */
static const RiverPoint lake_outlet[]={
    {.371,.190,450},{.337,.193,420},{.298,.190,340},{.264,.199,360},
    {.229,.218,420},{.214,.253,550},{.242,.288,560},
    {.275,.323,620},{.306,.344,680},{.348,.353,720},{.391,.382,1000}
};
static const RiverPoint baldurne_river[]={
    {.574,.030,95},{.582,.080,110},{.578,.115,125},{.585,.160,145},
    {.583,.205,170},{.602,.250,220},{.609,.300,280},{.609,.340,400}
};
static const RiverPoint baldurne_branch[]={
    {.739,.210,65},{.721,.249,95},{.685,.258,120},
    {.677,.286,140},{.651,.297,170},{.628,.319,250}
};
static const RiverPoint east_river[]={
    {.608,.327,300},{.650,.351,230},{.679,.337,190},{.710,.336,155},
    {.741,.313,130},{.774,.320,110},{.798,.307,90},
    {.840,.323,80},{.883,.342,65},{.920,.361,45}
};
static const RiverPoint kerone_stream[]={
    {.612,.480,210},{.626,.431,150},{.637,.402,95},
    {.657,.438,100},{.694,.424,80},{.737,.405,45}
};
static const RiverPoint south_stream[]={
    {.510,.741,170},{.553,.718,140},{.581,.715,110},
    {.607,.678,90},{.642,.658,70},{.664,.653,45}
};
static const RiverPoint tanthoon_stream[]={
    {.153,.592,65},{.190,.609,80},{.223,.590,95},
    {.259,.592,115},{.288,.575,130},{.307,.558,160}
};
static const RiverPoint sclay_stream[]={
    {.149,.760,70},{.178,.741,65},{.191,.702,55},{.204,.663,40}
};
static const River rivers[]={
    {lake_outlet,COUNT(lake_outlet)},{baldurne_river,COUNT(baldurne_river)},
    {baldurne_branch,COUNT(baldurne_branch)},{east_river,COUNT(east_river)},
    {kerone_stream,COUNT(kerone_stream)},{south_stream,COUNT(south_stream)},
    {tanthoon_stream,COUNT(tanthoon_stream)},{sclay_stream,COUNT(sclay_stream)}
};
#define MARK(ID,NAME,U,V) {ID,NAME,U,V,((U)-.5)*ATLAS_WIDTH,((V)-.5)*ATLAS_DEPTH}
static const WorldAtlasLandmark landmarks[]={
    MARK("brandalhall","Brandalhall",.070,.077),
    MARK("callieut","Callieut",.102,.124),
    MARK("rustpatch","Rustpatch",.075,.275),
    MARK("ulasse","Ulasse",.474,.115),
    MARK("valdoa","Valdoa",.487,.320),
    MARK("baldurne","Baldurne",.659,.163),
    MARK("ishome","Ishome",.288,.398),
    MARK("seldonne","Seldonne",.384,.595),
    MARK("sclay","Sclay",.246,.809),
    MARK("estuay","Estuay",.624,.540),
    MARK("kerone_city","Kerone City",.829,.503),
    MARK("etarma","Etarma",.730,.704),
    MARK("fallaway","Fallaway",.573,.820),
    MARK("bellweather","Bellweather",.948,.832),
    MARK("gorshett_keep","Gorshett Keep",.918,.115)
};
/* The three northwest settlements inherit their natural mountain elevation.
 * Other authored foundation elevations, including Seldonne's 8 m, stay fixed. */
static const float settlement_heights[]={0,0,0,16,24,230,12,8,12,6,72,88,45,16,650};
static const char *region_names[]={
    "Crosswind waters","Great Mashito Mountains","Valwood","Old Tanthoon",
    "Kerone Stomplands","Baldurne uplands","The Wallows","Crosswind Coast"
};

static double clamp01(double a){return fmin(fmax(a,0.0),1.0);}
static double smooth(double lo,double hi,double a){double t=clamp01((a-lo)/(hi-lo));return t*t*(3.0-2.0*t);}
static double mix(double a,double b,double t){return a+(b-a)*t;}
/* Exact compact support: an outer ellipse has six extra meters on both axes.
 * C1 feather leaves every point outside this 32x44m patch bit-identical. An
 * irregular inner shelf crosses the water plane before the conservative quad
 * edge, so scene depth, rather than a geometric ellipse cut, owns the shore. */
static double rainforest_pool_height(double base,double x,double z){
    const WorldAtlasPool *p=&rainforest_pool;
    double dx=x-p->x,dz=z-p->z;
    if(fabs(dx)>=p->radius_x+p->bank_feather||fabs(dz)>=p->radius_z+p->bank_feather)return base;
    double outer=hypot(dx/(p->radius_x+p->bank_feather),dz/(p->radius_z+p->bank_feather));
    if(outer>=1)return base;
    double q=hypot(dx/p->radius_x,dz/p->radius_z),theta=atan2(dz/p->radius_z,dx/p->radius_x);
    /* Two unequal wooded promontories interrupt the old nearly oval rim.
     * They only reclaim existing water: the old wet footprint remains a
     * conservative envelope, so purchased reeds/roots cannot be submerged.
     * Cosine lobes are smooth at the angular seam and do not make noisy teeth. */
    double old_bank=.88+.035*sin(theta*3+.7)+.025*sin(theta*5-1.1);
    double east_lobe=.5+.5*cos(theta-.35),west_lobe=.5+.5*cos(theta+2.1);
    double east2=east_lobe*east_lobe,west2=west_lobe*west_lobe;
    double bank=fmax(.76,old_bank-.15*east2*east2*east2*east2-
                     .11*west2*west2*west2*west2*west2*west2),r=q/bank;
    /* A low, asymmetric sediment shelf makes the transmitted bed readable.
     * Zero at the center preserves the .95m deep refuge; zero at the edge
     * keeps one continuous shoreline and the original compact-support join. */
    double sediment=.23*smooth(.08,.40,r)*(1-smooth(.50,.86,r))*
                    (.5+.5*sin(theta+1.4));
    double crest=.16*(.5+.5*sin(theta*2-.6))*smooth(.92,1.35,r);
    double target=p->surface_y-.95*(1-smooth(.08,1,r))+
                  .55*smooth(.90,1.35,r)+sediment+crest;
    double blend=1;
    if(q>1)blend=1-smooth(0,1,(q-1)/(q/outer-1));
    return mix(base,target,blend);
}

static double bilerp(const float *values,size_t a,size_t b,size_t c,size_t d,double tx,double tz){return mix(mix(values[a],values[b],tx),mix(values[c],values[d],tx),tz);}
static double bilerp_byte(const uint8_t *values,size_t a,size_t b,size_t c,size_t d,double tx,double tz){return mix(mix(values[a],values[b],tx),mix(values[c],values[d],tx),tz)/255.0;}
static uint32_t hash(int x,int z){
    uint32_t a=(uint32_t)x*0x1f123bb5u^(uint32_t)z*0x5f356495u^0x72a5d341u;
    a^=a>>16;a*=0x7feb352du;a^=a>>15;a*=0x846ca68bu;return a^(a>>16);
}
static double noise(double x,double z){
    int ix=(int)floor(x),iz=(int)floor(z);double u=x-ix,v=z-iz;
    u=u*u*(3-2*u);v=v*v*(3-2*v);
    double a=(hash(ix,iz)&0xffffffu)/16777216.0;
    double b=(hash(ix+1,iz)&0xffffffu)/16777216.0;
    double c=(hash(ix,iz+1)&0xffffffu)/16777216.0;
    double d=(hash(ix+1,iz+1)&0xffffffu)/16777216.0;
    return mix(mix(a,b,u),mix(c,d,u),v);
}
static double segment_distance2(double px,double pz,double ax,double az,double bx,double bz,double *out_t){
    double dx=bx-ax,dz=bz-az,length2=dx*dx+dz*dz;
    double t=length2>0?clamp01(((px-ax)*dx+(pz-az)*dz)/length2):0;
    double x=px-(ax+dx*t),z=pz-(az+dz*t);if(out_t)*out_t=t;
    return x*x+z*z;
}
/* Signed distance in meters, positive inside the traced polygon. */
static double polygon_distance(double u,double v,const MapPolygon *polygon){
    bool inside=false;double best=1e30;
    for(size_t i=0,j=polygon->count-1;i<polygon->count;j=i++){
        MapPoint a=polygon->points[j],b=polygon->points[i];
        if((a.v>v)!=(b.v>v)&&u<(b.u-a.u)*(v-a.v)/(b.v-a.v)+a.u)inside=!inside;
        double d=segment_distance2(u*ATLAS_WIDTH,v*ATLAS_DEPTH,
            a.u*ATLAS_WIDTH,a.v*ATLAS_DEPTH,b.u*ATLAS_WIDTH,b.v*ATLAS_DEPTH,NULL);
        if(d<best)best=d;
    }
    return (inside?1:-1)*sqrt(best);
}
static double river_distance(double u,double v,const River *river){
    double best=1e30;
    for(size_t i=1;i<river->count;i++){
        RiverPoint a=river->points[i-1],b=river->points[i];double t;
        double d=sqrt(segment_distance2(u*ATLAS_WIDTH,v*ATLAS_DEPTH,
            a.u*ATLAS_WIDTH,a.v*ATLAS_DEPTH,b.u*ATLAS_WIDTH,b.v*ATLAS_DEPTH,&t));
        d-=mix(a.radius,b.radius,t);if(d<best)best=d;
    }
    return best;
}
static double shore_at(double u,double v){
    double shore=-1e30;
    for(size_t i=0;i<COUNT(lands);i++)shore=fmax(shore,polygon_distance(u,v,&lands[i]));
    for(size_t i=0;i<COUNT(lakes);i++)shore=fmin(shore,-polygon_distance(u,v,&lakes[i]));
    for(size_t i=0;i<COUNT(rivers);i++)shore=fmin(shore,river_distance(u,v,&rivers[i]));
    /* Small ponds visible throughout Old Tanthoon. */
    static const double ponds[][4]={{.176,.597,650,450},{.160,.680,650,450},{.272,.688,450,330},{.235,.723,350,350}};
    for(size_t i=0;i<COUNT(ponds);i++){
        double x=(u-ponds[i][0])*ATLAS_WIDTH/ponds[i][2];
        double z=(v-ponds[i][1])*ATLAS_DEPTH/ponds[i][3];
        double distance=(sqrt(x*x+z*z)-1)*fmin(ponds[i][2],ponds[i][3]);
        shore=fmin(shore,distance);
    }
    // Settlement icons are symbolic. A small dry foundation avoids placing a
    // waypoint in a coarse atlas texel's interpolated shore or river bed.
    for(size_t i=0;i<COUNT(landmarks);i++){
        double dx=(u-landmarks[i].u)*ATLAS_WIDTH,dz=(v-landmarks[i].v)*ATLAS_DEPTH;
        shore=fmax(shore,150.0-hypot(dx,dz));
    }
    return shore;
}
static double wallows_edge(double u){
    double t=clamp01((u-.65)/.35);return .14+.20*t+.05*t*t;
}
static int region_at(double u,double v,double shore){
    if(shore<0)return WORLD_ATLAS_OCEAN;
    if(u>.64&&v<wallows_edge(u))return WORLD_ATLAS_WALLOWS;
    if(u<.32&&v<.33)return WORLD_ATLAS_MASHITO;
    if(u>.575&&u<.76&&v<.35)return WORLD_ATLAS_BALDURNE;
    if(v<.40&&u<.61)return WORLD_ATLAS_VALWOOD;
    if(u<.39&&v<.49)return WORLD_ATLAS_VALWOOD;
    if(v>.78&&u>.43)return WORLD_ATLAS_CROSSWIND;
    if(u<.595)return WORLD_ATLAS_OLD_TANTHOON;
    return WORLD_ATLAS_KERONE;
}
static double rainforest_extent(double u,double v){
    /* A regional ecology overlay, not a new political/map region. The finite
     * bounds reject every existing settlement foundation and the starting area
     * before any atlas reads. The nearest settlement (Sclay) is over3km beyond
     * the southern bound; no per-sample landmark traversal is necessary. */
    const double center_u=.235,center_v=.670,radius_u=.065,radius_v=.085;
    if(u<=center_u-radius_u||u>=center_u+radius_u||
       v<=center_v-radius_v||v>=center_v+radius_v)return 0;
    double du=(u-center_u)/radius_u,dv=(v-center_v)/radius_v,r2=du*du+dv*dv;
    return r2>=1?0:1-smooth(.36,1,r2);
}
static double rainforest_weight(double u,double v,double shore,int region){
    if(region!=WORLD_ATLAS_OLD_TANTHOON||shore<=120)return 0;
    return rainforest_extent(u,v)*smooth(120,360,shore);
}
static double original_natural_height(double u,double v,double shore){
    if(shore<=0)return -fmin(85.0,-shore*.07);
    double x=(u-.5)*ATLAS_WIDTH,z=(v-.5)*ATLAS_DEPTH;
    double broad=noise(x/3400,z/3400),rolling=noise(x/780,z/780);
    double h=28+48*broad+15*rolling;
    double nw=(1-smooth(.27,.35,u))*(1-smooth(.24,.34,v));
    double ridge=exp(-pow((v-(.055+.35*u))/.060,2));
    double peak=exp(-pow((u-.19)/.07,2)-pow((v-.082)/.055,2));
    h+=nw*(400+1050*ridge+1000*peak)*(.78+.22*noise(x/1700,z/1700));
    double wallows=smooth(.63,.67,u)*(1-smooth(wallows_edge(u)-.004,wallows_edge(u)+.009,v));
    h+=wallows*(410+190*broad+35*rolling);
    double baldurne=smooth(.54,.63,u)*(1-smooth(.30,.38,v))*(1-wallows);
    h+=baldurne*(85+100*broad);
    double southern_cliffs=smooth(.74,.85,v)*smooth(.43,.52,u)*(1-smooth(.89,.98,u));
    h+=southern_cliffs*(45+65*broad);
    h=1.2*smooth(0,30,shore)+h*smooth(0,1500,shore);
    return h;
}

/* Rounded ridge crests retain arêtes without the single-texel cusps of abs()
 * ridged noise. This is an artist-shaped, erosion-like field, not a simulation. */
static double ridge(double value){
    double t=value*2-1;
    return (sqrt(1.0625)-sqrt(t*t+.0625))/(sqrt(1.0625)-.25);
}
static double square(double value){return value*value;}
typedef struct {double u,v,height;} MountainCrest;
static double crest_segment(double x,double z,MountainCrest a,MountainCrest b,double south_width,double north_width){
    double ax=(a.u-.5)*ATLAS_WIDTH,az=(a.v-.5)*ATLAS_DEPTH;
    double bx=(b.u-.5)*ATLAS_WIDTH,bz=(b.v-.5)*ATLAS_DEPTH,t;
    double distance2=segment_distance2(x,z,ax,az,bx,bz,&t);
    double width=(bx-ax)*(z-az)-(bz-az)*(x-ax)>=0?south_width:north_width;
    double distance=sqrt(distance2+160*160)-160;
    double flank=clamp01(1-distance/width);
    return mix(a.height,b.height,t)*flank*flank;
}
static double ulasse_headwall(double x,double z,double rib){
    double u=x/ATLAS_WIDTH+.5,v=z/ATLAS_DEPTH+.5;
    /* The compact segment supports end well inside this box, so skipping the
     * rest of the map changes no elevations and bounds the one-time bake cost. */
    if(u<.24||u>.50||v>.205)return 0;
    /* Unequal summit/saddle heights along a bent crest survive distant mesh
     * spacing. Cross-sections fall faster toward the glacial lake headwalls. */
    static const MountainCrest chain[]={
        {.294,.050,700},{.322,.054,2150},{.337,.067,1710},
        {.352,.060,2680},{.366,.076,1660},{.380,.070,2370},
        {.391,.084,1630},{.403,.078,2350},{.415,.091,1770},
        {.430,.098,780},{.444,.108,180}
    };
    double height=0;
    for(size_t i=1;i<COUNT(chain);i++)height=fmax(height,
        crest_segment(x,z,chain[i-1],chain[i],3200,4700));
    /* Three descending arêtes separate broad incised basins. Their kilometre
     * spacing gives the silhouette a readable structure instead of cone noise. */
    static const MountainCrest arms[][2]={
        {{.352,.060,2550},{.337,.109,460}},
        {{.380,.070,2260},{.368,.122,420}},
        {{.403,.078,2220},{.395,.124,330}}
    };
    for(size_t i=0;i<COUNT(arms);i++)height=fmax(height,
        crest_segment(x,z,arms[i][0],arms[i][1],1550,1550));
    double incision=280*exp(-square((u-.357)/.011)-square((v-.105)/.022))+
        330*exp(-square((u-.384)/.0095)-square((v-.111)/.020))+
        240*exp(-square((u-.411)/.010)-square((v-.117)/.018));
    return fmax(0,height*(.94+.06*rib)-incision*smooth(150,900,height));
}
static double natural_height(double u,double v,double shore){
    if(shore<=0)return -fmin(85.0,-shore*.07);
    double x=(u-.5)*ATLAS_WIDTH,z=(v-.5)*ATLAS_DEPTH;
    double broad=noise(x/3400,z/3400),rolling=noise(x/780,z/780);
    double warp_x=(noise(x/4200+17,z/4200-31)-.5)*1800;
    double warp_z=(noise(x/4200-41,z/4200+19)-.5)*1400;
    double qx=x+warp_x,qz=z+warp_z;
    double r0=ridge(noise((qx*.91+qz*.27)/2300,(qz*.91-qx*.27)/1500));
    double r1=ridge(noise((qx*.84-qz*.54)/1050,(qz*.84+qx*.54)/850));
    double relief=r0*r0*(.80+.20*r1*r1);
    double h=28+40*broad+18*rolling;

    /* The mapped northwest range has a connected backbone, branching ribs and
     * incised basins, rather than a few large Gaussian domes. */
    double nw=(1-smooth(.29,.39,u))*(1-smooth(.25,.35,v));
    double backbone=exp(-square((v-(.050+.34*u+warp_z/ATLAS_DEPTH*.35))/.058));
    double summit_mass=exp(-square((u-.19)/.080)-square((v-.082)/.065));
    double mountain=nw*(190+390*backbone+340*summit_mass+
        .78*(360+850*backbone+520*summit_mass)*relief);
    double drainage=ridge(noise((qx+qz*.18)/1300,(qz-qx*.12)/2100));
    mountain-=nw*80*drainage*drainage*(1-r0)*smooth(.2,.8,backbone);

    h+=fmax(mountain,ulasse_headwall(x+warp_x*.10,z+warp_z*.10,r1));

    /* Forest valleys need readable silhouettes at walking height as well as
     * distant peaks. These spurs stay subordinate to the mapped mountain range. */
    double forest_spur=exp(-square((u-.485)/.078)-square((v-.260)/.075));
    double tanthoon=smooth(.43,.53,v)*(1-smooth(.72,.84,v))*(1-smooth(.37,.55,u));
    h+=forest_spur*(100+310*relief)+tanthoon*(25+105*relief);

    /* Break the Wallows escarpment into three broad rock benches. A warped
     * front and uneven buttresses avoid an extruded, perfectly straight wall. */
    double edge=wallows_edge(u)+(noise(x/2400+5,7.3)-.5)*.0055;
    double inward=(edge-v)*ATLAS_DEPTH;
    double cliff=.30*smooth(-230,-20,inward)+.40*smooth(30,230,inward)+.30*smooth(280,520,inward);
    double wallows=smooth(.63,.67,u)*cliff;
    double buttress=smooth(.63,.68,u)*(1-smooth(-900,1300,fabs(inward)));
    h+=wallows*(435+160*broad+65*r1*r1)+buttress*65*relief;
    double baldurne=smooth(.54,.63,u)*(1-smooth(.30,.38,v))*(1-wallows);
    h+=baldurne*(70+115*broad+70*relief);

    double southern_cliffs=smooth(.74,.85,v)*smooth(.43,.52,u)*(1-smooth(.89,.98,u));
    h+=southern_cliffs*(55+85*broad+45*relief);
    /* Headlands rise behind a narrow beach; sheltered forest banks stay softer. */
    double coastal_rise=mix(1150,550,southern_cliffs);
    h=1.2*smooth(0,30,shore)+h*smooth(0,coastal_rise,shore);
    return h;
}
static double base_height(double u,double v,double shore,double h,const double *pad_heights,
                          const double *natural_offsets,double *detail_mask){
    *detail_mask=1;
    if(shore<=0)return h;
    double x=(u-.5)*ATLAS_WIDTH,z=(v-.5)*ATLAS_DEPTH;
    double fixed_valley=0,natural_valley=0,natural_offset=0;
    for(size_t i=0;i<COUNT(landmarks);i++){
        double dx=x-landmarks[i].x,dz=z-landmarks[i].z;
        double distance=hypot(dx,dz);
        if(i>=3)fixed_valley=fmax(fixed_valley,1-smooth(450,1800,distance));
        else{
            double weight=1-smooth(650,2000,distance);
            if(weight>natural_valley){natural_valley=weight;natural_offset=natural_offsets[i];}
        }
        *detail_mask=fmin(*detail_mask,smooth(140,700,distance));
    }
    /* Keep existing town valleys instead of carving low circular pads into a
     * newly raised ridge. The twelve authored foundation elevations stay fixed. */
    if(fixed_valley>0)h=mix(h,original_natural_height(u,v,shore),fixed_valley);
    if(natural_valley>0)h=mix(h,original_natural_height(u,v,shore)+natural_offset,natural_valley);
    for(size_t i=0;i<COUNT(landmarks);i++){
        double dx=x-landmarks[i].x,dz=z-landmarks[i].z;
        double weight=1-smooth(90,420,hypot(dx,dz));
        if(weight>0)h=mix(h,pad_heights[i],weight);
    }
    return h;
}

/* Match macro curvature to the 100 m baked sampling grid. The short separable
 * filter removes isolated one-texel creases while preserving kilometre ridges.
 * Apply before foundations, so smoothing cannot bury an authored spawn pad. */
static bool soften_height(size_t count){
    float *source=malloc(count*sizeof(*source));
    if(!source)return false;
    static const unsigned weights[7]={1,6,15,20,15,6,1};
    for(int pass=0;pass<4;pass++){
        memcpy(source,atlas.height,count*sizeof(*source));
        for(int z=0;z<ATLAS_ROWS;z++)for(int x=0;x<ATLAS_COLS;x++){
            double height=0;
            for(int delta=-3;delta<=3;delta++){
                int sx=x+((pass&1)==0?delta:0),sz=z+((pass&1)==1?delta:0);
                if(sx<0)sx=0;else if(sx>=ATLAS_COLS)sx=ATLAS_COLS-1;
                if(sz<0)sz=0;else if(sz>=ATLAS_ROWS)sz=ATLAS_ROWS-1;
                height+=source[(size_t)sz*ATLAS_COLS+sx]*weights[delta+3];
            }
            atlas.height[(size_t)z*ATLAS_COLS+x]=(float)(height/64);
        }
    }
    free(source);
    return true;
}

static void soften_climate(size_t count){
    // Ecotones span a few hundred meters; metadata region IDs stay categorical.
    // Filtering bytes here keeps every later worker/physics query constant cost.
    uint8_t *source=malloc(count*2);
    if(!source)return;
    memcpy(source,atlas.forest,count);memcpy(source+count,atlas.moisture,count);
    const unsigned weights[5]={1,4,6,4,1};
    for(int z=0;z<ATLAS_ROWS;z++)for(int x=0;x<ATLAS_COLS;x++){
        unsigned forest=0,moisture=0;
        for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
            int sx=x+dx,sz=z+dz;
            if(sx<0)sx=0;else if(sx>=ATLAS_COLS)sx=ATLAS_COLS-1;
            if(sz<0)sz=0;else if(sz>=ATLAS_ROWS)sz=ATLAS_ROWS-1;
            size_t index=(size_t)sz*ATLAS_COLS+sx;
            unsigned weight=weights[dx+2]*weights[dz+2];
            forest+=source[index]*weight;moisture+=source[count+index]*weight;
        }
        size_t index=(size_t)z*ATLAS_COLS+x;
        atlas.forest[index]=(uint8_t)((forest+128)/256);
        atlas.moisture[index]=(uint8_t)((moisture+128)/256);
    }
    free(source);
}

typedef struct {double height,shore;} MacroSample;
static MacroSample macro_sample(double x,double z){
    double gx=(x/ATLAS_WIDTH+.5)*(ATLAS_COLS-1),gz=(z/ATLAS_DEPTH+.5)*(ATLAS_ROWS-1);
    int ix=(int)gx,iz=(int)gz;
    if(ix<0)ix=0;else if(ix>=ATLAS_COLS-1)ix=ATLAS_COLS-2;
    if(iz<0)iz=0;else if(iz>=ATLAS_ROWS-1)iz=ATLAS_ROWS-2;
    double tx=clamp01(gx-ix),tz=clamp01(gz-iz);
    size_t a=(size_t)iz*ATLAS_COLS+ix,b=a+1,c=a+ATLAS_COLS,d=c+1;
    MacroSample sample={bilerp(atlas.height,a,b,c,d,tx,tz),bilerp(atlas.shore,a,b,c,d,tx,tz)};
    return sample;
}

static double authored_protection(double x,double z){
    double mask=1;
    for(size_t i=0;i<COUNT(landmarks);i++){
        double radius=i<3?2000:1800;
        mask=fmin(mask,smooth(420,radius,hypot(x-landmarks[i].x,z-landmarks[i].z)));
    }
    return mask;
}

static double trail_distance(double x,double z,double *half_width){
    double best=1e30,width=0;
    for(size_t i=1;i<COUNT(trail_points);i++){
        const WorldAtlasTrailPoint *a=&trail_points[i-1],*b=&trail_points[i];
        double t,d=segment_distance2(x,z,a->x,a->z,b->x,b->z,&t);
        if(d<best){best=d;width=mix(a->half_width,b->half_width,t);}
    }
    if(half_width)*half_width=width;
    return sqrt(best);
}

typedef struct {MapPoint a,b;double width,depth;} Incision;
static double incision_field(double x,double z,const Incision *features,size_t count){
    double total=0;
    for(size_t i=0;i<count;i++){
        const Incision *f=&features[i];
        double t,d=sqrt(segment_distance2(x,z,(f->a.u-.5)*ATLAS_WIDTH,(f->a.v-.5)*ATLAS_DEPTH,
            (f->b.u-.5)*ATLAS_WIDTH,(f->b.v-.5)*ATLAS_DEPTH,&t));
        double length=hypot((f->b.u-f->a.u)*ATLAS_WIDTH,(f->b.v-f->a.v)*ATLAS_DEPTH);
        /* Round the incision ends, avoiding hard feature-start walls. */
        double ends=smooth(0,200,t*length)*smooth(0,250,(1-t)*length);
        total+=f->depth*(1-smooth(0,f->width,d))*ends;
    }
    return total;
}

/* This detail is authored once, independently of the reference-map water mask.
 * It is not an erosion simulation: crests, benches and drainage spines are
 * explicit editable features, with bounded noise adding rock-scale variation. */
static double ulasse_landscape(double x,double z,MacroSample macro,double *rock,double *meadow,double *moss){
    double u=x/ATLAS_WIDTH+.5,v=z/ATLAS_DEPTH+.5;
    static const MountainCrest crown[]={
        {.319,.058,20},{.331,.059,90},{.342,.064,50},
        {.350,.059,240},{.357,.066,80},{.368,.070,25},
        {.376,.068,210},{.384,.076,50},{.394,.079,165},
        {.402,.081,90},{.410,.089,190},{.422,.095,40},{.433,.104,0}
    };
    static const MountainCrest brow[]={
        {.338,.101,0},{.349,.105,160},{.361,.110,70},
        {.374,.108,250},{.384,.118,100},{.397,.115,280},
        {.408,.124,50},{.423,.120,190},{.437,.126,0}
    };
    double crag=0,headwall=0;
    for(size_t i=1;i<COUNT(crown);i++)crag=fmax(crag,crest_segment(x,z,crown[i-1],crown[i],530,900));
    for(size_t i=1;i<COUNT(brow);i++)headwall=fmax(headwall,crest_segment(x,z,brow[i-1],brow[i],450,850));
    static const Incision gullies[]={
        {{.348,.075},{.350,.127},360,145},
        {{.363,.082},{.373,.139},390,175},
        {{.384,.085},{.389,.141},350,180},
        {{.405,.094},{.412,.145},340,150},
        {{.422,.104},{.432,.143},300,105}
    };
    double gully=incision_field(x,z,gullies,COUNT(gullies));
    double cliff_zone=smooth(.075,.10,v)*(1-smooth(.135,.155,v))*smooth(.320,.340,u)*(1-smooth(.438,.460,u));
    double qx=x+(noise(x/1050+23,z/1050-41)-.5)*100;
    double qz=z+(noise(x/1050-19,z/1050+11)-.5)*90;
    double drainage=ridge(noise((qx+qz*.22)/420,(qz-qx*.18)/900));
    double rubble=ridge(noise((qx*.87-qz*.39)/165,(qz*.87+qx*.39)/330));
    double stone=noise(qx/78+7,qz/108-13)-.5;
    double highland=smooth(240,850,macro.height);
    /* Unequal ledges terminate laterally; no periodic altitude stripes. Broad
     * benches and gullies survive the 64/128 m distant surface mesh. */
    double ledge_height=macro.height+(noise(x/1400+4,z/1400)-.5)*130;
    double benches=cliff_zone*(46*(smooth(460,550,ledge_height)-smooth(620,730,ledge_height))
        -35*(smooth(1040,1140,ledge_height)-smooth(1240,1380,ledge_height)));
    double erosion=highland*((drainage-.57)*72+(rubble-.54)*19+stone*8);
    *rock=clamp01(.15+.70*highland+.25*cliff_zone+.25*smooth(15,100,headwall));
    *meadow=(1-smooth(260,700,macro.height))*.18;
    *moss=(1-highland)*.32;
    return crag+headwall-gully+benches+erosion;
}

static double valwood_landscape(double x,double z,MacroSample macro,double *rock,double *meadow,double *moss){
    const double entrance_x=(.511-.5)*ATLAS_WIDTH,entrance_z=(.252-.5)*ATLAS_DEPTH;
    double dx=x-entrance_x,dz=z-entrance_z;
    /* A slightly sloping oval glade opens ahead of the on-foot spawn. The
     * surrounding raised shoulders frame its winding northwest trail. */
    double forward=-dx*.523-dz*.852,side=dx*.852-dz*.523;
    double glade_distance=hypot((forward-48)/190,side/125);
    double glade=1-smooth(.55,1.65,glade_distance);
    double path_distance=trail_distance(x,z,NULL);
    double valley=(1-smooth(20,165,path_distance))*(1-smooth(100,420,hypot(dx,dz))*.55);
    double shoulder=(smooth(45,110,path_distance)-smooth(210,370,path_distance));
    double side_noise=noise(x/230+7,z/310-4);
    double entrance_height=macro_sample(entrance_x,entrance_z).height;
    double clearing_target=entrance_height+dx*.035+dz*.025;
    double delta=(clearing_target-macro.height)*glade-20*valley*(1-glade)+22*shoulder*(.5+.5*side_noise)*(1-glade);
    double rough=(noise(x/90,z/115)-.5)*3.0+(ridge(noise(x/47+3,z/73-7))-.55)*1.5;
    delta+=rough*(1-glade*.85)*(1-valley*.6);
    *meadow=clamp01(glade*.98+valley*.16);
    *moss=clamp01(.28+.54*(1-glade)*(1-smooth(10,170,path_distance))+.20*side_noise);
    *rock=clamp01(shoulder*.32*(1-glade));
    return delta;
}

static double crosswind_landscape(double x,double z,MacroSample macro,double *rock,double *meadow,double *moss){
    double u=x/ATLAS_WIDTH+.5,v=z/ATLAS_DEPTH+.5;
    static const MountainCrest spurs[][3]={
        {{.501,.901,0},{.508,.918,65},{.513,.928,0}},
        {{.521,.898,0},{.529,.917,85},{.533,.930,0}},
        {{.547,.894,0},{.550,.909,75},{.559,.918,0}},
        {{.568,.889,0},{.568,.902,55},{.575,.910,0}}
    };
    double rock_spur=0;
    for(size_t i=0;i<COUNT(spurs);i++)for(size_t p=1;p<COUNT(spurs[i]);p++)
        rock_spur=fmax(rock_spur,crest_segment(x,z,spurs[i][p-1],spurs[i][p],350,480));
    static const Incision coves[]={
        {{.515,.911},{.521,.934},290,38},
        {{.539,.902},{.544,.932},360,48},
        {{.560,.897},{.566,.924},300,32}
    };
    double shore_band=smooth(45,180,macro.shore)*(1-smooth(900,1700,macro.shore));
    double warped=x+(noise(x/700+3,z/700-9)-.5)*90;
    double rib=ridge(noise(warped/245,(z+warped*.16)/510));
    double broken=ridge(noise(warped/93-7,z/170+3));
    double platform=(smooth(150,220,macro.shore)-smooth(300,470,macro.shore))*12;
    double delta=(rock_spur-incision_field(x,z,coves,COUNT(coves))+(rib-.55)*27+(broken-.5)*5+platform)*shore_band;
    *rock=clamp01(shore_band*(.40+.45*smooth(5,55,rock_spur)+.2*rib));
    *meadow=clamp01(.55*(1-shore_band)+.10*shore_band)*(1-smooth(.936,.947,v));
    *moss=shore_band*.13*(1-smooth(.56,.58,u));
    return delta;
}

typedef struct {double delta,offset;} ShoreSample;
static ShoreSample shore_patch_sample(double x,double z);

static bool landscape_init(void){
    size_t count=(size_t)DISTRICT_COUNT*LANDSCAPE_SIDE*LANDSCAPE_SIDE;
    landscape=calloc(count,sizeof(*landscape));
    if(!landscape)return false;
    for(unsigned district=0;district<DISTRICT_COUNT;district++){
        const WorldAtlasDistrict *d=&districts[district];
        for(int iz=0;iz<LANDSCAPE_SIDE;iz++)for(int ix=0;ix<LANDSCAPE_SIDE;ix++){
            double tu=(double)ix/(LANDSCAPE_SIDE-1),tv=(double)iz/(LANDSCAPE_SIDE-1);
            double x=d->xmin+tu*d->width,z=d->zmin+tv*d->depth;
            MacroSample macro=macro_sample(x,z);
            macro.shore+=shore_patch_sample(x,z).offset;
            if(macro.shore<=0)continue;
            double edge=smooth(0,.07,tu)*smooth(0,.07,1-tu)*smooth(0,.07,tv)*smooth(0,.07,1-tv);
            double protected_mask=authored_protection(x,z),rock=0,meadow=0,moss=0;
            double delta=district==0?ulasse_landscape(x,z,macro,&rock,&meadow,&moss):
                district==1?valwood_landscape(x,z,macro,&rock,&meadow,&moss):
                crosswind_landscape(x,z,macro,&rock,&meadow,&moss);
            double mask=edge*protected_mask*smooth(60,260,macro.shore);
            LandscapeCell *cell=&landscape[((size_t)district*LANDSCAPE_SIDE+iz)*LANDSCAPE_SIDE+ix];
            /* Incised dry gullies retain a real land floor. Allowing a deep
             * feature to exhaust the coastal bank would create a flat false
             * sea-level basin while the reference-map water mask stays dry. */
            cell->delta=(float)fmax(delta*mask,-macro.height*.80);
            cell->rock=(uint8_t)(clamp01(rock*mask)*255+.5);
            cell->meadow=(uint8_t)(clamp01(meadow*mask)*255+.5);
            cell->moss=(uint8_t)(clamp01(moss*mask)*255+.5);
            if(district!=1&&macro.shore<230){
                double bank=edge*protected_mask*smooth(10,45,macro.shore)*(1-smooth(100,230,macro.shore));
                double gravel=(district==0?.28:.50)+.18*noise(x/130+23,z/170-11);
                cell->rock=(uint8_t)(fmax(cell->rock/255.,gravel*bank)*255+.5);
                cell->meadow=(uint8_t)(cell->meadow*(1-bank*.9));
                cell->moss=(uint8_t)(fmax(cell->moss/255.,bank*.16)*255+.5);
            }
        }
    }
    return true;
}

typedef struct {double delta,rock,meadow,moss;} LandscapeSample;
static LandscapeSample landscape_sample(double x,double z){
    LandscapeSample sample={0};
    if(!landscape)return sample;
    for(unsigned district=0;district<DISTRICT_COUNT;district++){
        const WorldAtlasDistrict *d=&districts[district];
        if(x<d->xmin||x>=d->xmin+d->width||z<d->zmin||z>=d->zmin+d->depth)continue;
        double gx=(x-d->xmin)*(LANDSCAPE_SIDE-1)/d->width,gz=(z-d->zmin)*(LANDSCAPE_SIDE-1)/d->depth;
        int ix=(int)gx,iz=(int)gz;
        /* A point one ULP inside a rectangle can round to the final grid line. */
        if(ix>=LANDSCAPE_SIDE-1)ix=LANDSCAPE_SIDE-2;
        if(iz>=LANDSCAPE_SIDE-1)iz=LANDSCAPE_SIDE-2;
        double tx=clamp01(gx-ix),tz=clamp01(gz-iz);
        size_t a=((size_t)district*LANDSCAPE_SIDE+iz)*LANDSCAPE_SIDE+ix,b=a+1,c=a+LANDSCAPE_SIDE,e=c+1;
        sample.delta=mix(mix(landscape[a].delta,landscape[b].delta,tx),mix(landscape[c].delta,landscape[e].delta,tx),tz);
        sample.rock=mix(mix(landscape[a].rock,landscape[b].rock,tx),mix(landscape[c].rock,landscape[e].rock,tx),tz)/255.0;
        sample.meadow=mix(mix(landscape[a].meadow,landscape[b].meadow,tx),mix(landscape[c].meadow,landscape[e].meadow,tx),tz)/255.0;
        sample.moss=mix(mix(landscape[a].moss,landscape[b].moss,tx),mix(landscape[c].moss,landscape[e].moss,tx),tz)/255.0;
        break;
    }
    return sample;
}

static ShoreSample shore_patch_sample(double x,double z){
    ShoreSample result={0};
    if(!shore_relief)return result;
    for(size_t patch=0;patch<COUNT(shore_patches);patch++){
        const WorldAtlasDistrict *p=&shore_patches[patch];
        if(x<p->xmin||x>=p->xmin+p->width||z<p->zmin||z>=p->zmin+p->depth)continue;
        double gx=(x-p->xmin)*(LANDSCAPE_SIDE-1)/p->width,gz=(z-p->zmin)*(LANDSCAPE_SIDE-1)/p->depth;
        int ix=(int)gx,iz=(int)gz;
        if(ix>=LANDSCAPE_SIDE-1)ix=LANDSCAPE_SIDE-2;
        if(iz>=LANDSCAPE_SIDE-1)iz=LANDSCAPE_SIDE-2;
        size_t a=((size_t)patch*LANDSCAPE_SIDE+iz)*LANDSCAPE_SIDE+ix,b=a+1,c=a+LANDSCAPE_SIDE,d=c+1;
        double tx=clamp01(gx-ix),tz=clamp01(gz-iz);
        result.offset=mix(mix(shore_relief[a].offset,shore_relief[b].offset,tx),mix(shore_relief[c].offset,shore_relief[d].offset,tx),tz);
        if(shore_relief_ready)
            result.delta=mix(mix(shore_relief[a].delta,shore_relief[b].delta,tx),mix(shore_relief[c].delta,shore_relief[d].delta,tx),tz);
        break;
    }
    return result;
}

static bool shore_offset_init(void){
    size_t count=COUNT(shore_patches)*(size_t)LANDSCAPE_SIDE*LANDSCAPE_SIDE;
    ShoreCell *values=calloc(count,sizeof(*values));
    if(!values)return false;
    for(size_t patch=0;patch<COUNT(shore_patches);patch++){
        const WorldAtlasDistrict *p=&shore_patches[patch];
        for(int iz=0;iz<LANDSCAPE_SIDE;iz++)for(int ix=0;ix<LANDSCAPE_SIDE;ix++){
            double x=p->xmin+p->width*ix/(LANDSCAPE_SIDE-1),z=p->zmin+p->depth*iz/(LANDSCAPE_SIDE-1);
            double distance=macro_sample(x,z).shore;
            if(fabs(distance)>=48)continue;
            double edge=smooth(0,60,fmin(x-p->xmin,p->xmin+p->width-x))*
                smooth(0,60,fmin(z-p->zmin,p->zmin+p->depth-z));
            /* Nonperiodic unequal coves/spits at30–100m scales. A convex
             * bilinear lookup retains the strict8m signed-distance bound. */
            double warp=(noise(x/103+17,z/91-31)-.5)*27;
            double cove=noise((x+warp+z*.23)/61+7,(z-warp-x*.13)/83-11)-.5;
            double shingle=noise((x-z*.17)/31-13,(z+x*.21)/43+19)-.5;
            double offset=8*tanh(cove*3.8+shingle*1.4);
            double band=1-smooth(12,48,fabs(distance));
            size_t at=((size_t)patch*LANDSCAPE_SIDE+iz)*LANDSCAPE_SIDE+ix;
            values[at].offset=(float)(offset*edge*authored_protection(x,z)*band);
        }
    }
    /* Publish offsets atomically with respect to setup; worker threads start
     * only after atlas initialization. Heights remain disabled until baked. */
    shore_relief=values;
    return true;
}

static bool shore_relief_init(void){
    /* Baseline queries use the complete effective shoreline, but no partially
     * baked height residuals. Material masks use this same shoreline. */
    for(size_t patch=0;patch<COUNT(shore_patches);patch++){
        const WorldAtlasDistrict *p=&shore_patches[patch];
        for(int iz=0;iz<LANDSCAPE_SIDE;iz++)for(int ix=0;ix<LANDSCAPE_SIDE;ix++){
            double x=p->xmin+p->width*ix/(LANDSCAPE_SIDE-1),z=p->zmin+p->depth*iz/(LANDSCAPE_SIDE-1);
            WorldAtlasSample base=world_atlas_sample(x,z);
            double distance=base.shore_distance;
            if(distance<=0||distance>=210)continue;
            double edge=smooth(0,60,fmin(x-p->xmin,p->xmin+p->width-x))*
                smooth(0,60,fmin(z-p->zmin,p->zmin+p->depth-z));
            double protected_mask=authored_protection(x,z);
            double broad=noise((x+z*.31)/165,(z-x*.17)/210);
            double broken=noise(x/59+19,z/77-23);
            double fine=noise(x/17-13,z/23+7);
            double foot=9+19*noise(x/190+11,z/230-31);
            double crest=foot+10+12*noise(x/110-17,z/150+5);
            double tail=crest+38+32*noise(x/220+9,z/180-3);
            double height=(patch==0?1.8:6.2)*(.55+.75*broad);
            double bank=height*(smooth(foot,crest,distance)-smooth(crest+14,tail,distance));
            double terrace=(patch==0?.55:2.2)*broad*
                (smooth(70,86,distance)-smooth(115,160,distance));
            double mouth=smooth(.58,.79,noise(x/190+31,z/260-14));
            double incision=height*1.3*mouth*(smooth(12,45,distance)-smooth(100,180,distance));
            double shingle=(.10+.13*broken)*smooth(0,4,distance)*(1-smooth(10,32,distance));
            double ripples=((fine-.5)*.30+(broken-.5)*.45)*smooth(0,7,distance)*(1-smooth(80,150,distance));
            double delta=(bank+terrace-incision+shingle+ripples)*edge*protected_mask;
            /* Preserve the on-foot entrance's existing natural grade with a
             * soft transition, without flattening the surrounding shore. */
            if(patch==0){
                double entrance_x=(.454-.5)*ATLAS_WIDTH,entrance_z=(.180-.5)*ATLAS_DEPTH;
                delta*=smooth(8,42,hypot(x-entrance_x,z-entrance_z));
            }
            size_t at=((size_t)patch*LANDSCAPE_SIDE+iz)*LANDSCAPE_SIDE+ix;
            shore_relief[at].delta=(float)fmax(delta,-base.height*.65);
        }
    }
    shore_relief_ready=true;
    return true;
}

WorldAtlasStartingSample world_atlas_starting_sample(double x,double z){
    WorldAtlasStartingSample out={0};
    if(!starting_ready||!isfinite(x)||!isfinite(z))return out;
    double u=(x-starting_bounds[0])/starting_bounds[2],v=(z-starting_bounds[1])/starting_bounds[3];
    if(u<0||u>1||v<0||v>1)return out;
    double gx=u*(LANDSCAPE_SIDE-1),gz=v*(LANDSCAPE_SIDE-1);
    int ix=(int)gx,iz=(int)gz,jx=ix<LANDSCAPE_SIDE-1?ix+1:ix,jz=iz<LANDSCAPE_SIDE-1?iz+1:iz;
    double tx=gx-ix,tz=gz-iz;
    const StartingCell *a=&starting[(size_t)iz*LANDSCAPE_SIDE+ix],*b=&starting[(size_t)iz*LANDSCAPE_SIDE+jx];
    const StartingCell *c=&starting[(size_t)jz*LANDSCAPE_SIDE+ix],*d=&starting[(size_t)jz*LANDSCAPE_SIDE+jx];
    out.height_delta=mix(mix(a->delta,b->delta,tx),mix(c->delta,d->delta,tx),tz);
    /* Preserve the previous spawn footprint exactly, including off-grid points. */
    out.height_delta*=smooth(starting_spawn_radius,starting_spawn_radius+4,hypot(x-starting_origin[0],z-starting_origin[1]));
    out.path=mix(mix(a->path,b->path,tx),mix(c->path,d->path,tx),tz)/255.;
    out.paving=mix(mix(a->paving,b->paving,tx),mix(c->paving,d->paving,tx),tz)/255.;
    out.clearing=mix(mix(a->clearing,b->clearing,tx),mix(c->clearing,d->clearing,tx),tz)/255.;
    out.grass_exclusion=mix(mix(a->grass,b->grass,tx),mix(c->grass,d->grass,tx),tz)/255.;
    return out;
}
void world_atlas_starting_bounds(double *xmin,double *zmin,double *width,double *depth){
    if(xmin)*xmin=starting_bounds[0];
    if(zmin)*zmin=starting_bounds[1];
    if(width)*width=starting_bounds[2];
    if(depth)*depth=starting_bounds[3];
}
static bool starting_init(void){
    starting=calloc((size_t)LANDSCAPE_SIDE*LANDSCAPE_SIDE,sizeof(*starting));
    if(!starting)return false;
    for(int iz=0;iz<LANDSCAPE_SIDE;iz++)for(int ix=0;ix<LANDSCAPE_SIDE;ix++){
        double x=starting_bounds[0]+starting_bounds[2]*ix/(LANDSCAPE_SIDE-1);
        double z=starting_bounds[1]+starting_bounds[3]*iz/(LANDSCAPE_SIDE-1);
        double dx=x-starting_origin[0],dz=z-starting_origin[1];
        double r=dx*starting_right[0]+dz*starting_right[1],f=dx*starting_forward[0]+dz*starting_forward[1];
        double edge=fmin(fmin(r-starting_rf_bounds[0],starting_rf_bounds[2]-r),fmin(f-starting_rf_bounds[1],starting_rf_bounds[3]-f));
        double boundary=smooth(0,starting_edge_feather,edge),base=world_atlas_height(x,z);
        double target=base,paving=0,path=0,clearing=0,grass=0;
        for(size_t i=0;i<COUNT(starting_pads);i++){
            const StartingPad *p=&starting_pads[i];double dr=r-p->r,df=f-p->f;
            double outside=hypot(fmax(fabs(dr)-p->hr,0),fmax(fabs(df)-p->hf,0));
            double weight=1-smooth(0,p->feather,outside);
            target=mix(target,p->height+dr*p->sr+df*p->sf,weight);
            double qr=fabs(dr)-p->surface_hr,qf=fabs(df)-p->surface_hf;
            double surface_distance=hypot(fmax(qr,0),fmax(qf,0))+fmin(fmax(qr,qf),0);
            double edge_break=(noise(x*.23,z*.23)-.5)*1.10+(noise(x*.71,z*.71)-.5)*.32;
            double surface=1-smooth(-.6,p->feather*.45,surface_distance+edge_break);
            paving=fmax(paving,surface*p->paving);clearing=fmax(clearing,weight);
        }
        for(size_t i=0;i<COUNT(starting_paths);i++){
            const StartingPath *p=&starting_paths[i];double ar=r-p->ar,af=f-p->af,br=p->br-p->ar,bf=p->bf-p->af;
            double t=clamp01((ar*br+af*bf)/(br*br+bf*bf));
            path=fmax(path,1-smooth(p->width,p->width+p->feather,hypot(ar-br*t,af-bf*t)));
        }
        for(size_t i=0;i<COUNT(starting_clearings);i++){
            const StartingClearing *c=&starting_clearings[i];double dr=r-c->r,df=f-c->f;
            double inner=hypot(dr/c->ir,df/c->if_),outer=hypot(dr/c->or_,df/c->of);
            double t=inner<=1?0:(inner-1)/fmax(inner-outer,.0001);
            clearing=fmax(clearing,1-smooth(0,1,t));
        }
        for(size_t i=0;i<COUNT(starting_no_grass);i++){
            const StartingExclusion *e=&starting_no_grass[i];double dx=x-e->x,dz=z-e->z;
            double lx=e->cosine*dx-e->sine*dz,lz=e->sine*dx+e->cosine*dz;
            double outside=hypot(fmax(fmax(e->xmin-lx,lx-e->xmax),0),fmax(fmax(e->zmin-lz,lz-e->zmax),0));
            grass=fmax(grass,1-smooth(.45,.85,outside));
        }
        StartingCell *cell=&starting[(size_t)iz*LANDSCAPE_SIDE+ix];
        cell->delta=(float)((target-base)*boundary);
        cell->path=(uint8_t)(clamp01(path*boundary)*255+.5);
        cell->paving=(uint8_t)(clamp01(paving*boundary)*255+.5);
        cell->clearing=(uint8_t)(clamp01(clearing*boundary)*255+.5);
        cell->grass=(uint8_t)(clamp01(grass*boundary)*255+.5);
    }
    starting_ready=true;return true;
}
void world_atlas_village_origin(double *x,double *z){if(x)*x=VILLAGE_X;if(z)*z=VILLAGE_Z;}
void world_atlas_village_bounds(double *xmin,double *zmin,double *width,double *depth){
    if(xmin)*xmin=VILLAGE_X-280;
    if(zmin)*zmin=VILLAGE_Z-280;
    if(width)*width=560;
    if(depth)*depth=560;
}
float world_atlas_village_weight(double x,double z){
    if(!atlas.storage||!isfinite(x)||!isfinite(z))return 0;
    double dx=fabs(x-VILLAGE_X),dz=fabs(z-VILLAGE_Z);
    if(dx>=280||dz>=280)return 0;
    return (float)((1-smooth(225,280,dx))*(1-smooth(225,280,dz)));
}
WorldAtlasVillageSample world_atlas_village_sample(double x,double z){
    WorldAtlasVillageSample out={0};
    if(!village_ready||!isfinite(x)||!isfinite(z))return out;
    double u=(x-(VILLAGE_X-280))/560,v=(z-(VILLAGE_Z-280))/560;
    if(u<0||u>1||v<0||v>1)return out;
    double gx=u*(LANDSCAPE_SIDE-1),gz=v*(LANDSCAPE_SIDE-1);
    int ix=(int)gx,iz=(int)gz,jx=ix<LANDSCAPE_SIDE-1?ix+1:ix,jz=iz<LANDSCAPE_SIDE-1?iz+1:iz;
    double tx=gx-ix,tz=gz-iz;
    const VillageCell *a=&village[(size_t)iz*LANDSCAPE_SIDE+ix],*b=&village[(size_t)iz*LANDSCAPE_SIDE+jx];
    const VillageCell *c=&village[(size_t)jz*LANDSCAPE_SIDE+ix],*d=&village[(size_t)jz*LANDSCAPE_SIDE+jx];
    out.path=mix(mix(a->path,b->path,tx),mix(c->path,d->path,tx),tz)/255.;
    out.paving=mix(mix(a->paving,b->paving,tx),mix(c->paving,d->paving,tx),tz)/255.;
    out.clearing=mix(mix(a->clearing,b->clearing,tx),mix(c->clearing,d->clearing,tx),tz)/255.;
    out.grass_exclusion=mix(mix(a->grass,b->grass,tx),mix(c->grass,d->grass,tx),tz)/255.;
    WorldCastleCreekSample creek=world_castle_creek_sample_local(x-VILLAGE_X,z-VILLAGE_Z,24.);
    out.path=fmax(out.path,creek.bank);
    out.paving*=1.-creek.bank;
    out.grass_exclusion=fmax(out.grass_exclusion,fmin(1.,creek.bank*1.8));
    return out;
}
static bool village_init(void){
    village=calloc((size_t)LANDSCAPE_SIDE*LANDSCAPE_SIDE,sizeof(*village));
    if(!village)return false;
    for(int iz=0;iz<LANDSCAPE_SIDE;iz++)for(int ix=0;ix<LANDSCAPE_SIDE;ix++){
        double x=-280+560.*ix/(LANDSCAPE_SIDE-1),z=-280+560.*iz/(LANDSCAPE_SIDE-1);
        double path=0,paving=0,grass=0,weight=world_atlas_village_weight(VILLAGE_X+x,VILLAGE_Z+z);
        for(size_t i=0;i<COUNT(village_roads);i++){
            const VillageRoad *r=&village_roads[i];
            double distance=sqrt(segment_distance2(x,z,r->ax,r->az,r->bx,r->bz,NULL));
            double mask=1-smooth(r->width,r->width+1.4,distance);
            path=fmax(path,mask);paving=fmax(paving,mask*r->paving);grass=fmax(grass,mask);
        }
        for(size_t i=0;i<COUNT(village_rects);i++){
            const VillageRect *r=&village_rects[i];double dx=x-r->x,dz=z-r->z;
            double px=r->cosine*dx-r->sine*dz,pz=r->sine*dx+r->cosine*dz;
            double edge=fmax(fabs(px)-r->hx,fabs(pz)-r->hz);
            if(edge>=1.2)continue;
            double mask=1-smooth(0,1.2,edge);
            paving=fmax(paving,mask*r->paving);grass=fmax(grass,mask*r->grass);
        }
        for(size_t i=0;i<COUNT(castle_street_roads);i++){
            const VillageRoad *r=&castle_street_roads[i];
            const double distance=sqrt(segment_distance2(x,z,r->ax,r->az,r->bx,r->bz,NULL));
            const double mask=1-smooth(r->width,r->width+1.4,distance);
            path=fmax(path,mask);paving=fmax(paving,mask*r->paving);grass=fmax(grass,mask);
        }
        for(size_t i=0;i<COUNT(castle_street_pads);i++){
            const VillageRect *r=&castle_street_pads[i];const double dx=x-r->x,dz=z-r->z;
            const double px=r->cosine*dx-r->sine*dz,pz=r->sine*dx+r->cosine*dz;
            const double edge=fmax(fabs(px)-r->hx,fabs(pz)-r->hz);
            if(edge>=1.2)continue;
            const double mask=1-smooth(0,1.2,edge);
            paving=fmax(paving,mask*r->paving);grass=fmax(grass,mask*r->grass);
        }
        for(size_t i=0;i<COUNT(demo_finish_roads);i++){
            const VillageRoad *r=&demo_finish_roads[i];
            const double distance=sqrt(segment_distance2(x,z,r->ax,r->az,r->bx,r->bz,NULL));
            const double mask=1-smooth(r->width,r->width+1.4,distance);
            path=fmax(path,mask);paving=fmax(paving,mask*r->paving);grass=fmax(grass,mask);
        }
        for(size_t i=0;i<COUNT(demo_finish_pads);i++){
            const VillageRect *r=&demo_finish_pads[i];const double dx=x-r->x,dz=z-r->z;
            const double px=r->cosine*dx-r->sine*dz,pz=r->sine*dx+r->cosine*dz;
            const double edge=fmax(fabs(px)-r->hx,fabs(pz)-r->hz);
            if(edge>=1.2)continue;
            const double mask=1-smooth(0,1.2,edge);
            paving=fmax(paving,mask*r->paving);grass=fmax(grass,mask*r->grass);
        }
        grass=fmax(grass,world_village_farm_mask_local(x,z));
        VillageCell *cell=&village[(size_t)iz*LANDSCAPE_SIDE+ix];
        cell->path=(uint8_t)(clamp01(path*weight)*255+.5);
        cell->paving=(uint8_t)(clamp01(paving*weight)*255+.5);
        cell->clearing=(uint8_t)(clamp01(weight)*255+.5);
        cell->grass=(uint8_t)(clamp01(grass*weight)*255+.5);
    }
    village_ready=true;return true;
}
WorldAtlasStartingSample world_atlas_bandit_sample(double x,double z){
    WorldAtlasStartingSample out={0};
    if(!bandit_ready||!isfinite(x)||!isfinite(z))return out;
    const double u=(x-bandit_bounds[0])/bandit_bounds[2],v=(z-bandit_bounds[1])/bandit_bounds[3];
    if(u<0||u>1||v<0||v>1)return out;
    const double gx=u*(BANDIT_MASK_SIDE-1),gz=v*(BANDIT_MASK_SIDE-1);
    const int ix=(int)gx,iz=(int)gz,jx=ix<BANDIT_MASK_SIDE-1?ix+1:ix,jz=iz<BANDIT_MASK_SIDE-1?iz+1:iz;
    const double tx=gx-ix,tz=gz-iz;
    const VillageCell *a=&bandit[(size_t)iz*BANDIT_MASK_SIDE+ix],*b=&bandit[(size_t)iz*BANDIT_MASK_SIDE+jx];
    const VillageCell *c=&bandit[(size_t)jz*BANDIT_MASK_SIDE+ix],*d=&bandit[(size_t)jz*BANDIT_MASK_SIDE+jx];
    out.path=mix(mix(a->path,b->path,tx),mix(c->path,d->path,tx),tz)/255.;
    out.clearing=mix(mix(a->clearing,b->clearing,tx),mix(c->clearing,d->clearing,tx),tz)/255.;
    out.grass_exclusion=mix(mix(a->grass,b->grass,tx),mix(c->grass,d->grass,tx),tz)/255.;
    return out;
}
void world_atlas_bandit_bounds(double *xmin,double *zmin,double *width,double *depth){
    if(xmin)*xmin=bandit_bounds[0];
    if(zmin)*zmin=bandit_bounds[1];
    if(width)*width=bandit_bounds[2];
    if(depth)*depth=bandit_bounds[3];
}
static bool bandit_init(void){
    bandit=calloc((size_t)BANDIT_MASK_SIDE*BANDIT_MASK_SIDE,sizeof(*bandit));
    if(!bandit)return false;
    for(int iz=0;iz<BANDIT_MASK_SIDE;iz++)for(int ix=0;ix<BANDIT_MASK_SIDE;ix++){
        const double wx=bandit_bounds[0]+bandit_bounds[2]*ix/(BANDIT_MASK_SIDE-1);
        const double wz=bandit_bounds[1]+bandit_bounds[3]*iz/(BANDIT_MASK_SIDE-1);
        const double x=wx-bandit_origin[0],z=wz-bandit_origin[1];
        const double edge=fmin(fmin(wx-bandit_bounds[0],bandit_bounds[0]+bandit_bounds[2]-wx),
                               fmin(wz-bandit_bounds[1],bandit_bounds[1]+bandit_bounds[3]-wz));
        const double boundary=smooth(0,3,edge);
        double path=0,grass=0;
        const double ex=(x-bandit_clearing[0])/bandit_clearing[2],ez=(z-bandit_clearing[1])/bandit_clearing[3];
        double clearing=1-smooth(1,1+bandit_clearing[4]/fmin(bandit_clearing[2],bandit_clearing[3]),hypot(ex,ez));
        for(size_t i=0;i<COUNT(bandit_paths);i++){
            const BanditGroundPath *p=&bandit_paths[i];
            const double ax=x-p->ax,az=z-p->az,dx=p->bx-p->ax,dz=p->bz-p->az;
            const double t=fmax(0,fmin(1,(ax*dx+az*dz)/fmax(dx*dx+dz*dz,1.e-9)));
            const double weight=1-smooth(p->half_width,p->half_width+p->feather,hypot(ax-dx*t,az-dz*t));
            path=fmax(path,weight);clearing=fmax(clearing,weight);
        }
        for(size_t i=0;i<COUNT(bandit_pads);i++){
            const BanditGroundPad *p=&bandit_pads[i];
            const double dx=x-p->x,dz=z-p->z,cs=cos(p->yaw),sn=sin(p->yaw);
            const double outside=fmax(fabs(cs*dx-sn*dz)-p->hx,fabs(sn*dx+cs*dz)-p->hz);
            const double weight=1-smooth(0,p->feather,outside);
            grass=fmax(grass,weight);clearing=fmax(clearing,weight);
        }
        VillageCell *cell=&bandit[(size_t)iz*BANDIT_MASK_SIDE+ix];
        cell->path=(uint8_t)(clamp01(path*boundary)*255+.5);
        cell->clearing=(uint8_t)(clamp01(clearing*boundary)*255+.5);
        cell->grass=(uint8_t)(clamp01(fmax(path,grass)*boundary)*255+.5);
    }
    bandit_ready=true;return true;
}
bool world_atlas_init(void){
    if(atlas.storage)return true;
    size_t count=(size_t)ATLAS_COLS*ATLAS_ROWS;
    void *storage=malloc(count*(3*sizeof(float)+3*sizeof(uint8_t)));
    if(!storage)return false;
    atlas.storage=storage;atlas.height=storage;atlas.shore=atlas.height+count;atlas.detail=atlas.shore+count;
    atlas.region=(uint8_t*)(atlas.detail+count);atlas.forest=atlas.region+count;atlas.moisture=atlas.forest+count;
    for(int iz=0;iz<ATLAS_ROWS;iz++)for(int ix=0;ix<ATLAS_COLS;ix++){
        double u=(double)ix/(ATLAS_COLS-1),v=(double)iz/(ATLAS_ROWS-1);
        double shore=shore_at(u,v);size_t index=(size_t)iz*ATLAS_COLS+ix;
        atlas.shore[index]=(float)shore;atlas.height[index]=(float)natural_height(u,v,shore);
    }
    if(!soften_height(count)){world_atlas_shutdown();return false;}
    // Calculate natural anchor elevations from the filtered regional terrain.
    // This small setup array adds no landmark scans to runtime sampling.
    double pad_heights[COUNT(landmarks)],natural_offsets[3];
    for(size_t i=0;i<COUNT(landmarks);i++){
        double gx=landmarks[i].u*(ATLAS_COLS-1),gz=landmarks[i].v*(ATLAS_ROWS-1);
        int ix=(int)gx,iz=(int)gz;double tx=gx-ix,tz=gz-iz;
        size_t a=(size_t)iz*ATLAS_COLS+ix,b=a+1,c=a+ATLAS_COLS,d=c+1;
        pad_heights[i]=i<3?mix(mix(atlas.height[a],atlas.height[b],tx),
            mix(atlas.height[c],atlas.height[d],tx),tz):settlement_heights[i];
        if(i<3)natural_offsets[i]=pad_heights[i]-original_natural_height(
            landmarks[i].u,landmarks[i].v,shore_at(landmarks[i].u,landmarks[i].v));
    }
    static const double forests[]={0,.15,.86,.66,.15,.32,.015,.18};
    static const double moistures[]={1,.38,.78,.88,.43,.59,.12,.64};
    for(int iz=0;iz<ATLAS_ROWS;iz++)for(int ix=0;ix<ATLAS_COLS;ix++){
        double u=(double)ix/(ATLAS_COLS-1),v=(double)iz/(ATLAS_ROWS-1);
        size_t index=(size_t)iz*ATLAS_COLS+ix;
        double shore=atlas.shore[index],detail_mask;
        double height=base_height(u,v,shore,atlas.height[index],pad_heights,natural_offsets,&detail_mask);
        int region=region_at(u,v,shore);
        double forest=forests[region]*(.56+.44*noise(u*47,v*31))*smooth(12,180,shore)*(1-smooth(1150,1600,height));
        double mountain=smooth(240,1400,height),upland=smooth(70,500,height);
        atlas.height[index]=(float)height;atlas.shore[index]=(float)shore;atlas.region[index]=(uint8_t)region;
        atlas.detail[index]=(float)((1.8+6.2*mountain+1.5*upland)*detail_mask*smooth(40,240,shore));
        atlas.forest[index]=(uint8_t)(clamp01(forest)*255+.5);
        atlas.moisture[index]=(uint8_t)(clamp01(moistures[region]+.13*(noise(u*23,v*17)-.5))*255+.5);
    }
    soften_climate(count);
    if(!shore_offset_init()){world_atlas_shutdown();return false;}
    if(!landscape_init()){world_atlas_shutdown();return false;}
    if(!shore_relief_init()){world_atlas_shutdown();return false;}
    if(!starting_init()){world_atlas_shutdown();return false;}
    if(!village_init()){world_atlas_shutdown();return false;}
    if(!bandit_init()){world_atlas_shutdown();return false;}
    return true;
}
void world_atlas_shutdown(void){world_height_region_clear(&external_height_region);world_height_region_clear(&fallback_height_region);free(bandit);bandit=NULL;bandit_ready=false;free(village);village=NULL;village_ready=false;free(starting);starting=NULL;starting_ready=false;free(shore_relief);shore_relief=NULL;shore_relief_ready=false;free(landscape);landscape=NULL;free(atlas.storage);memset(&atlas,0,sizeof(atlas));}
double world_atlas_width(void){return ATLAS_WIDTH;}
double world_atlas_depth(void){return ATLAS_DEPTH;}
double world_atlas_walk_speed(void){return 3.5;}
void world_atlas_uv_to_world(double u,double v,double *x,double *z){if(x)*x=(u-.5)*ATLAS_WIDTH;if(z)*z=(v-.5)*ATLAS_DEPTH;}
void world_atlas_world_to_uv(double x,double z,double *u,double *v){if(u)*u=x/ATLAS_WIDTH+.5;if(v)*v=z/ATLAS_DEPTH+.5;}
static WorldAtlasHeightEdit studio_height_edit=NULL;
static const void *studio_height_data=NULL;
void world_atlas_set_height_edit(WorldAtlasHeightEdit edit,const void *data){studio_height_edit=edit;studio_height_data=data;}
int world_atlas_load_height_region(const char *path,char *error,size_t capacity){
    if(!atlas.storage)return world_height_region_error(error,capacity,"Initialize atlas before loading a height region");
    return world_height_region_load(&external_height_region,path,error,capacity);
}
void world_atlas_clear_height_region(void){world_height_region_clear(&external_height_region);}
static bool height_region_info(const WorldHeightRegion *region,WorldAtlasHeightRegionInfo *info){
    if(info)memset(info,0,sizeof(*info));
    if(!region->heights)return false;
    if(info){info->columns=region->columns;info->rows=region->rows;
        info->xmin=region->xmin;info->zmin=region->zmin;
        info->stepx=region->stepx;info->stepz=region->stepz;
        info->bytes=(size_t)region->columns*region->rows*sizeof(float);}
    return true;
}
bool world_atlas_height_region_info(WorldAtlasHeightRegionInfo *info){return height_region_info(&external_height_region,info);}
int world_atlas_load_fallback_height_region(const char *path,char *error,size_t capacity){
    if(!atlas.storage)return world_height_region_error(error,capacity,"Initialize atlas before loading surrounding terrain");
    return world_height_region_load(&fallback_height_region,path,error,capacity);
}
void world_atlas_clear_fallback_height_region(void){world_height_region_clear(&fallback_height_region);}
bool world_atlas_fallback_height_region_info(WorldAtlasHeightRegionInfo *info){return height_region_info(&fallback_height_region,info);}
static WorldAtlasSample base_sample_uv(double u,double v){
    WorldAtlasSample result={.height=-85,.region=WORLD_ATLAS_OCEAN,.moisture=1,.shore_distance=-10000};
    if(!atlas.storage||!isfinite(u)||!isfinite(v)||u<0||u>1||v<0||v>1)return result;
    double gx=u*(ATLAS_COLS-1),gz=v*(ATLAS_ROWS-1);int ix=(int)gx,iz=(int)gz;
    int jx=ix<ATLAS_COLS-1?ix+1:ix,jz=iz<ATLAS_ROWS-1?iz+1:iz;
    double tx=gx-ix,tz=gz-iz;
    size_t a=(size_t)iz*ATLAS_COLS+ix,b=(size_t)iz*ATLAS_COLS+jx,c=(size_t)jz*ATLAS_COLS+ix,d=(size_t)jz*ATLAS_COLS+jx;
    double shore=bilerp(atlas.shore,a,b,c,d,tx,tz);
    double height=bilerp(atlas.height,a,b,c,d,tx,tz);
    double x=(u-.5)*ATLAS_WIDTH,z=(v-.5)*ATLAS_DEPTH;
    LandscapeSample local=landscape_sample(x,z);
    ShoreSample bank_field=shore_patch_sample(x,z);
    shore+=bank_field.offset;
    // Bilinear interpolation around steep shores must not lift a water texel
    // above sea level or sink its dry neighbor. Both branches meet at Y=0.
    if(shore<=0)height=-fmin(85.0,-shore*.07);
    else{
        height=fmax(.6,fmax(0,height+local.delta))*smooth(0,80,shore);
        double amplitude=bilerp(atlas.detail,a,b,c,d,tx,tz);
        if(amplitude>0){
            double broad_detail=noise(x/155,z/155)-.5;
            double broken=noise((x+broad_detail*16)/47,(z-broad_detail*13)/47)-.5;
            double rib=ridge(noise((x+31)/27,(z-73)/39))-.60;
            height+=amplitude*(broad_detail*.72+broken*.36+rib*.18)*smooth(40,160,shore);
        }
        double bank=bank_field.delta*smooth(0,6,shore);
        height+=fmax(bank,-height*.65);
        height=fmax(height,.05*smooth(0,40,shore));
    }
    WorldAtlasStartingSample start=world_atlas_starting_sample(x,z);
    if(shore>0)height+=start.height_delta;
    /* Shared terrain/collision/rain support, outside the tested path core. */
#ifndef WORLD_ATLAS_DISABLE_DEMO_CONTOURS
    height+=world_demo_contour_delta(x-RAINFOREST_X,z-RAINFOREST_Z);
#endif
    height=rainforest_pool_height(height,x,z);
    /* One compact, gently feathered campsite terrace. Physics, scenery roots,
     * rain support and the rendered terrain all query this same surface. */
    if(fabs(x-WORLD_TRAIL_CAMP_X)<WORLD_TRAIL_CAMP_SUPPORT_RADIUS/1.6&&
       fabs(z-WORLD_TRAIL_CAMP_Z)<WORLD_TRAIL_CAMP_SUPPORT_RADIUS/.65){
        /* Follow the elongated resting/supply area rather than flattening
         * the existing understory on the western approach. */
        double r=hypot((x-WORLD_TRAIL_CAMP_X)*1.6,(z-WORLD_TRAIL_CAMP_Z)*.65);
        height=mix(WORLD_TRAIL_CAMP_Y,height,smooth(8.,WORLD_TRAIL_CAMP_SUPPORT_RADIUS,r));
    }
    double village_weight=world_atlas_village_weight(x,z);
    if(village_weight>0){
        height=mix(height,24,village_weight);
        height=world_castle_creek_sample_local(x-VILLAGE_X,z-VILLAGE_Z,height).height;
    }
    float external_height;
    if(world_height_region_sample(&external_height_region,x,z,&external_height)||
       world_height_region_sample(&fallback_height_region,x,z,&external_height))height=external_height;
    result.height=(float)height;result.shore_distance=(float)shore;
    result.land_weight=(float)smooth(-120,120,shore);
    int nearest_x=(int)(gx+.5),nearest_z=(int)(gz+.5);
    result.region=shore>0?atlas.region[(size_t)nearest_z*ATLAS_COLS+nearest_x]:WORLD_ATLAS_OCEAN;
    if(result.region==WORLD_ATLAS_OCEAN&&shore>0)result.region=region_at(u,v,shore);
    result.forest_density=(float)(bilerp_byte(atlas.forest,a,b,c,d,tx,tz)*smooth(0,120,shore)*(1-local.meadow*.94)*(1-start.clearing));
    result.moisture=(float)bilerp_byte(atlas.moisture,a,b,c,d,tx,tz);
    double rainforest=rainforest_weight(u,v,shore,result.region);
    if(rainforest>0){
        result.forest_density=(float)mix(result.forest_density,fmax(result.forest_density,.94),rainforest);
        result.moisture=(float)mix(result.moisture,fmax(result.moisture,.98),rainforest);
    }
    if(result.height<(float)rainforest_pool.surface_y-.005f&&fabs(x-rainforest_pool.x)<rainforest_pool.radius_x&&fabs(z-rainforest_pool.z)<rainforest_pool.radius_z){
        double px=(x-rainforest_pool.x)/rainforest_pool.radius_x,pz=(z-rainforest_pool.z)/rainforest_pool.radius_z;
        if(px*px+pz*pz<1)result.forest_density=0; /* Actual local freshwater bed is not a tree habitat. */
    }
    if(village_weight>0)result.forest_density*=(float)(1-village_weight);
    result.forest_density*=(float)(1-world_atlas_bandit_sample(x,z).clearing);
    const double roost=world_castle_dragons_ground_exclusion_local(x-VILLAGE_X,z-VILLAGE_Z);
    if(roost>0)result.forest_density*=(float)(1-roost);
    return result;
}
float world_atlas_base_height(double x,double z){return base_sample_uv(x/ATLAS_WIDTH+.5,z/ATLAS_DEPTH+.5).height;}
WorldAtlasSample world_atlas_sample_uv(double u,double v){
    WorldAtlasSample result=base_sample_uv(u,v);
    if(studio_height_edit&&isfinite(u)&&isfinite(v)&&u>=0&&u<=1&&v>=0&&v<=1){
        float delta=studio_height_edit((u-.5)*ATLAS_WIDTH,(v-.5)*ATLAS_DEPTH,studio_height_data);
        if(isfinite(delta)&&fabsf(delta)<=32.f)result.height+=delta;
    }
    return result;
}
WorldAtlasSample world_atlas_sample(double x,double z){
    return world_atlas_sample_uv(x/ATLAS_WIDTH+.5,z/ATLAS_DEPTH+.5);
}
float world_atlas_rainforest_weight(double x,double z){
    if(!atlas.storage||!isfinite(x)||!isfinite(z))return 0;
    double u=x/ATLAS_WIDTH+.5,v=z/ATLAS_DEPTH+.5;
    double extent=rainforest_extent(u,v);
    if(extent<=0)return 0;
    double gx=u*(ATLAS_COLS-1),gz=v*(ATLAS_ROWS-1);int ix=(int)gx,iz=(int)gz;
    double tx=gx-ix,tz=gz-iz;
    size_t a=(size_t)iz*ATLAS_COLS+ix,b=a+1,c=a+ATLAS_COLS,d=c+1;
    double shore=bilerp(atlas.shore,a,b,c,d,tx,tz);
    int region=shore>0?atlas.region[(size_t)(int)(gz+.5)*ATLAS_COLS+(int)(gx+.5)]:WORLD_ATLAS_OCEAN;
    if(region==WORLD_ATLAS_OCEAN&&shore>0)region=region_at(u,v,shore);
    /* Both local shore-offset patches are outside this ellipse, so these
     * macro reads exactly match the effective shore used by sample_uv here. */
    return (float)(region==WORLD_ATLAS_OLD_TANTHOON?extent*smooth(120,360,shore):0);
}
const WorldAtlasPool *world_atlas_rainforest_pool(void){return &rainforest_pool;}
static WorldAtlasWaterEdit studio_water_edit=NULL;
static const void *studio_water_data=NULL;
void world_atlas_set_water_edit(WorldAtlasWaterEdit edit,const void *data){studio_water_edit=edit;studio_water_data=data;}
bool world_atlas_local_water(double x,double z,float *surface_y,float *depth){
    if(surface_y)*surface_y=0;
    if(depth)*depth=0;
    if(!atlas.storage||!isfinite(x)||!isfinite(z))return false;
    if(studio_water_edit&&studio_water_edit(x,z,surface_y,depth,studio_water_data))return true;
    if(world_castle_creek_contains_local(x-VILLAGE_X,z-VILLAGE_Z)){
        WorldCastleCreekSample creek=world_castle_creek_sample_local(x-VILLAGE_X,z-VILLAGE_Z,24.);
        if(creek.submerged){
            if(surface_y)*surface_y=(float)WORLD_CASTLE_CREEK_SURFACE_Y;
            if(depth)*depth=(float)creek.depth;
            return true;
        }
    }
    const WorldAtlasPool *p=&rainforest_pool;
    if(fabs(x-p->x)>=p->radius_x||fabs(z-p->z)>=p->radius_z)return false;
    double dx=(x-p->x)/p->radius_x,dz=(z-p->z)/p->radius_z;
    if(dx*dx+dz*dz>=1)return false;
    float d=(float)p->surface_y-world_atlas_height(x,z);
    if(d<=.005f)return false;
    if(surface_y)*surface_y=(float)p->surface_y;
    if(depth)*depth=d;
    return true;
}
float world_atlas_reference_shore_distance(double x,double z){
    if(!atlas.storage||!isfinite(x)||!isfinite(z)||x<-.5*ATLAS_WIDTH||x>.5*ATLAS_WIDTH||z<-.5*ATLAS_DEPTH||z>.5*ATLAS_DEPTH)return -10000;
    return (float)macro_sample(x,z).shore;
}
float world_atlas_height(double x,double z){return world_atlas_sample(x,z).height;}
float world_atlas_walk_height(double x,double z){
    float bed=world_atlas_height(x,z);
    /* An imported source ground owns its walking surface too. Reapplying the
     * atlas village's old creek cut would open a trench through an unrelated
     * publisher square after its authored height had already been sampled. */
    float source_height;
    if(world_height_region_sample(&external_height_region,x,z,&source_height)||
       world_height_region_sample(&fallback_height_region,x,z,&source_height))return bed;
    return atlas.storage?(float)world_castle_creek_floor_local(x-VILLAGE_X,z-VILLAGE_Z,bed):bed;
}
const WorldAtlasLandmark *world_atlas_landmarks(size_t *count){if(count)*count=COUNT(landmarks);return landmarks;}
const char *world_atlas_region_name(int region){return region>=0&&region<WORLD_ATLAS_REGION_COUNT?region_names[region]:"Unknown region";}
const WorldAtlasDistrict *world_atlas_districts(size_t *count){if(count)*count=COUNT(districts);return districts;}
const WorldAtlasTrailPoint *world_atlas_trail(size_t *count){if(count)*count=COUNT(trail_points);return trail_points;}
const WorldAtlasTrailPoint *world_atlas_rainforest_trail_points(size_t *count){
    if(count)*count=COUNT(rainforest_trail);
    return rainforest_trail;
}
float world_atlas_rainforest_trail_mask(double x,double z){
    if(!isfinite(x)||!isfinite(z)||x<RAINFOREST_X-17.8||x>RAINFOREST_X+14.8||
       z<RAINFOREST_Z-112.8||z>RAINFOREST_Z+97.8)return 0;
    double distance2=1.e30;
    for(size_t i=1;i<COUNT(rainforest_trail);i++){
        const WorldAtlasTrailPoint *a=&rainforest_trail[i-1],*b=&rainforest_trail[i];
        distance2=fmin(distance2,segment_distance2(x,z,a->x,a->z,b->x,b->z,NULL));
    }
    return (float)(1-smooth(1.6,2.8,sqrt(distance2)));
}
WorldAtlasLandscapeSample world_atlas_landscape_mask(double x,double z){
    WorldAtlasLandscapeSample result={0};
    if(!atlas.storage||!isfinite(x)||!isfinite(z))return result;
    LandscapeSample sample=landscape_sample(x,z);
    result.rock=(float)sample.rock;result.meadow=(float)sample.meadow;result.moss=(float)sample.moss;
    const WorldAtlasDistrict *forest=&districts[1];
    if(x>=forest->xmin&&x<forest->xmin+forest->width&&z>=forest->zmin&&z<forest->zmin+forest->depth){
        double width,distance=trail_distance(x,z,&width);
        result.trail=(float)(1-smooth(width,width+.65,distance));
    }
    result.trail=fmaxf(result.trail,world_atlas_rainforest_trail_mask(x,z));
    if(x>WORLD_TRAIL_CAMP_X-36&&x<WORLD_TRAIL_CAMP_X+18&&
       z>WORLD_TRAIL_CAMP_Z-16&&z<WORLD_TRAIL_CAMP_Z+18){
        double d2=1.e30;
        for(size_t i=1;i<COUNT(world_trail_camp_approach);i++){
            const double *a=world_trail_camp_approach[i-1],*b=world_trail_camp_approach[i];
            d2=fmin(d2,segment_distance2(x,z,a[0],a[1],b[0],b[1],NULL));
        }
        double clearing=1-smooth(7.5,10.5,hypot(x-WORLD_TRAIL_CAMP_X,z-WORLD_TRAIL_CAMP_Z));
        result.trail=fmaxf(result.trail,(float)fmax(clearing,1-smooth(1.0,2.2,sqrt(d2))));
        result.meadow=fmaxf(result.meadow,(float)clearing);
    }
    WorldAtlasVillageSample town=world_atlas_village_sample(x,z);
    result.trail=fmaxf(result.trail,(float)fmax(town.path,fmax(town.paving,town.grass_exclusion)));
    result.meadow=fmaxf(result.meadow,(float)town.clearing);
    WorldAtlasStartingSample camp=world_atlas_bandit_sample(x,z);
    result.trail=fmaxf(result.trail,(float)fmax(camp.path,camp.grass_exclusion));
    result.meadow=fmaxf(result.meadow,(float)camp.clearing);
    return result;
}
WorldAtlasLandscapeSample world_atlas_landscape_sample(double x,double z){
    WorldAtlasLandscapeSample result=world_atlas_landscape_mask(x,z);
    result.height=world_atlas_height(x,z);
    return result;
}
