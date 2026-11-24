#ifndef VALWOOD_WORLD_DEMO_CONTOURS_H
#define VALWOOD_WORLD_DEMO_CONTOURS_H
#include <math.h>
#include <stdbool.h>
/* Shared smooth geometry for rendering, walking, props and rain. Local rainforest
 * metres; the path, freshwater basin and inhabited clearing stay protected. */
typedef struct {double x,z,rx,rz,height;} WorldDemoBank;
static const WorldDemoBank world_demo_banks[]={
    {-45,77,15,19,6.2}, {27,78,16,21,7.4},
    {-24,33,12,17,3.6}, {38,-5,18,24,8.5},
    {34,-68,18,23,7.0}, {-31,-78,17,23,5.8},
    {29,-127,18,21,8.6}, {-18,-120,15,20,4.8}
};
static const WorldDemoBank world_demo_ridges[]={
    {-55,-22,12,27,6.8}, /* narrow western spine, with an open saddle */
    {56,36,23,46,13.0},  /* eastern ridge reveals the winding path */
    {-47,-118,22,30,9.5},
    {8,-163,31,25,10.5}
};
static const WorldDemoBank world_demo_west_spurs[]={
    {-47,-5,10,16,4.2},{-49,-43,10,16,4.8}
};
static inline double world_demo_smooth(double a,double b,double x){
    double t=fmax(0,fmin(1,(x-a)/(b-a)));return t*t*(3-2*t);
}
static inline double world_demo_segment(double x,double z,double ax,double az,double bx,double bz){
    double dx=bx-ax,dz=bz-az,t=fmax(0,fmin(1,((x-ax)*dx+(z-az)*dz)/(dx*dx+dz*dz)));
    return hypot(x-ax-dx*t,z-az-dz*t);
}
static inline double world_demo_route_distance(double x,double z){
    static const double p[][2]={{-15,95},{2,60},{-8,20},{5,-25},{-5,-65},{12,-110}};
    double d=1e9;
    for(unsigned i=1;i<6;i++)d=fmin(d,world_demo_segment(x,z,p[i-1][0],p[i-1][1],p[i][0],p[i][1]));
    return d;
}
static inline double world_demo_landform_weight(double x,double z){
    double path=world_demo_smooth(3.6,10.0,world_demo_route_distance(x,z));
    /* Preserve the pool bed, shoreline, access and the camp/animal routes. */
    double pool=world_demo_smooth(1.15,1.85,hypot((x+22)/10.,(z+25)/16.));
    double camp=world_demo_smooth(0,7,fmax(fabs(x-12)-22,fabs(z-30)-17));
    /* Preserve the two surveyed stationary wildlife contact patches while
     * the surrounding bluff rises. Their complete soles stay on the original
     * terrain, with a gradual transition beyond their body footprints. */
    double wildlife=world_demo_smooth(2.2,6.5,hypot(x+68,z+28))*
        world_demo_smooth(2.2,6.5,hypot(x+74,z+32));
    return path*pool*camp*wildlife;
}
static inline double world_demo_bank_shape(const WorldDemoBank *b,double x,double z){
    double dx=(x-b->x)/b->rx,dz=(z-b->z)/b->rz,q=dx*dx+dz*dz;
    if(q>=1)return 0;
    double t=1-q;return b->height*t*t*(3-2*t);
}
static inline double world_demo_ridge_shape(const WorldDemoBank *b,double x,double z){
    double dx=(x-b->x)/b->rx,dz=(z-b->z)/b->rz,q=dx*dx+dz*dz;
    if(q>=1)return 0;
    /* A narrow crest, rather than the old broad, flat-topped dome. */
    double t=1-q;return b->height*t*t;
}
#include "world_demo_detail_reserves.inc"
static inline double world_demo_ridge_field(double x,double z){
    double result=0;
    for(unsigned i=0;i<sizeof(world_demo_banks)/sizeof(world_demo_banks[0]);i++)result+=world_demo_bank_shape(&world_demo_banks[i],x,z);
    for(unsigned i=0;i<sizeof(world_demo_ridges)/sizeof(world_demo_ridges[0]);i++)result+=i==0?world_demo_ridge_shape(&world_demo_ridges[i],x,z):world_demo_bank_shape(&world_demo_ridges[i],x,z);
    for(unsigned i=0;i<sizeof(world_demo_west_spurs)/sizeof(world_demo_west_spurs[0]);i++)result+=world_demo_ridge_shape(&world_demo_west_spurs[i],x,z);
    return result;
}
static inline double world_demo_contour_delta(double x,double z){
    if(!isfinite(x)||!isfinite(z)||fabs(x)>82||z< -190||z>110)return 0;
    double result=world_demo_ridge_field(x,z);
    /* Offset buttresses and connected shallow gullies break the former
     * elliptical mound silhouettes. Detail follows each ridge's long axis;
     * it is shared by render, collision, rain and prop support queries. */
    double western=world_demo_ridge_shape(&world_demo_ridges[0],x,z)/6.8;
    double eastern=world_demo_ridge_shape(&world_demo_ridges[1],x,z)/13.;
    double west_spur=world_demo_segment(x,z,-59,-34,-42,-12);
    double east_spur=world_demo_segment(x,z,50,12,64,52);
    double detail_weight=1.;
    for(unsigned i=0;i<sizeof(world_demo_detail_reserves)/sizeof(world_demo_detail_reserves[0]);i++){
        const double *r=world_demo_detail_reserves[i];double dx=x-r[0],dz=z-r[1];
        double distance=fmax(fabs(r[4]*dx-r[5]*dz)-r[2],fabs(r[5]*dx+r[4]*dz)-r[3]);
        detail_weight*=world_demo_smooth(0,4,distance);
    }
    double detail=western*(1.4*exp(-west_spur*west_spur/18.)-.85*exp(-pow((x+48)-.30*(z+19),2)/6.));
    detail+=eastern*(1.6*exp(-east_spur*east_spur/22.)-.95*exp(-pow((x-54)+.25*(z-36),2)/7.));
    /* Shallow drainage cuts remain on the banks, with silt fans at their toes.
     * Smooth longitudinal endpoints prevent a cut through the route or basin. */
    double g0=world_demo_segment(x,z,-40,63,-29,47);
    double g1=world_demo_segment(x,z,37,-48,24,-63);
    double g2=world_demo_segment(x,z,-48,-18,-34,-8);
    double g3=world_demo_segment(x,z,49,35,33,24);
    double cut=.65*exp(-g0*g0/2.8)+.55*exp(-g1*g1/3.5);
    detail-=western*.70*exp(-g2*g2/2.2)+eastern*.80*exp(-g3*g3/2.8);
    result=result-cut+detail*detail_weight;
    /* Small eroded stone shelves support the surveyed existing rigid bases at
     * the NEW ridge elevation. They do not preserve the old mound's height. */
    double shelf_sum=0,shelf_weight=0;bool on_shelf=false;
    for(unsigned i=0;i<sizeof(world_demo_detail_reserves)/sizeof(world_demo_detail_reserves[0]);i++){
        const double *r=world_demo_detail_reserves[i];if(r[0]>=-39||r[0]<=-60||r[1]<=-40||r[1]>=5)continue;
        double dx=x-r[0],dz=z-r[1];
        double d=fmax(fabs(r[4]*dx-r[5]*dz)-r[2],fabs(r[5]*dx+r[4]*dz)-r[3]);
        if(d>=2.8)continue;
        double level=world_demo_ridge_field(r[0],r[1]);
        if(d<=0){result=level;on_shelf=true;break;}
        double edge=world_demo_smooth(0,2.8,d),weight=(1.-edge)/fmax(edge,1e-9);
        shelf_sum+=weight*level;shelf_weight+=weight;
    }
    if(!on_shelf&&shelf_weight>0)result=(result+shelf_sum)/(1.+shelf_weight);
    return result*world_demo_landform_weight(x,z);
}
/* CPU-baked exposure in the existing shelter field. Fractured mineral pockets
 * leave soil/moss between them; height alone must not turn an entire wooded
 * ridge gray. This changes no terrain heights or runtime shader allocations. */
static inline double world_demo_rock_exposure(double x,double z){
    double h=world_demo_contour_delta(x,z);
    double broken=.5+.25*sin(x*.43+sin(z*.37))+.25*sin(z*.81-x*.21);
    double drainage=world_demo_segment(x,z,-48,-18,-34,-8);
    double erosion=world_demo_smooth(3.0,7.5,h)*(1.-world_demo_smooth(.7,2.6,drainage));
    return fmax(erosion*.78,world_demo_smooth(2.8,7.0,h)*.70*world_demo_smooth(.52,.86,broken));
}
#endif
