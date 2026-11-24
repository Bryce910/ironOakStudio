#ifndef VALWOOD_WORLD_CASTLE_CREEK_H
#define VALWOOD_WORLD_CASTLE_CREEK_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

/* Local east/south metres from world_atlas_village_origin(). This compact
 * closed freshwater course is OUTSIDE the outer town walls at x/z+/-205m.
 * Narrow outward bends preserve the established exterior hero-grove roots.
 * All bed edits remain inside the original flat225m terrace, leaving the
 * four outer tower foundations and every existing public street intact.
 * Terrain streaming and collision use the same excavated bed. The lowered
 * timber bridge has a separate walking floor; it does not fill in the stream.
 * Every function is finite, allocation-free and has a constant bounded cost. */
#define WORLD_CASTLE_CREEK_SURFACE_Y 22.9
#define WORLD_CASTLE_CREEK_DECK_Y 24.02
#define WORLD_CASTLE_CREEK_MIN_X (-225.0)
#define WORLD_CASTLE_CREEK_MAX_X 225.0
#define WORLD_CASTLE_CREEK_MIN_Z (-223.0)
#define WORLD_CASTLE_CREEK_MAX_Z 223.0

typedef struct { double x,z,half_width; } WorldCastleCreekPoint;
typedef struct {
    double height,height_delta,bank,depth;
    bool submerged;
} WorldCastleCreekSample;

static const WorldCastleCreekPoint world_castle_creek_points[]={
    {-205,-219,3.8}, {205,-219,3.8}, {214.8994949366,-214.8994949366,3.8},
    {219,-205,3.8}, {219,-25,3.8}, {223.6,15,1.4}, {223.6,110,1.4},
    {219,145,3.8}, {219,205,3.8}, {214.8994949366,214.8994949366,3.8},
    {205,219,3.8}, {0,219,3.8}, {-205,219,3.8},
    {-214.8994949366,214.8994949366,3.8}, {-219,205,3.8}, {-219,145,3.8},
    {-223.6,110,1.4}, {-223.6,15,1.4}, {-219,-25,3.8}, {-219,-205,3.8},
    {-214.8994949366,-214.8994949366,3.8}, {-205,-219,3.8}
};

static inline double world_castle_creek_smooth(double lo,double hi,double value) {
    double t=fmin(1.,fmax(0.,(value-lo)/(hi-lo)));
    return t*t*(3.-2.*t);
}
static inline bool world_castle_creek_contains_local(double x,double z) {
    return isfinite(x)&&isfinite(z)&&x>WORLD_CASTLE_CREEK_MIN_X&&
        x<WORLD_CASTLE_CREEK_MAX_X&&z>WORLD_CASTLE_CREEK_MIN_Z&&z<WORLD_CASTLE_CREEK_MAX_Z;
}
static inline WorldCastleCreekSample world_castle_creek_sample_local(double x,double z,double base) {
    WorldCastleCreekSample out={base,0,0,0,false};
    if(!world_castle_creek_contains_local(x,z)||!isfinite(base))return out;
    /* Hard guard as well as exterior path geometry: no courtyard water. */
    if(fabs(x)<=205.&&fabs(z)<=205.)return out;
    double q=2.;
    const size_t count=sizeof(world_castle_creek_points)/sizeof(world_castle_creek_points[0]);
    for(size_t i=1;i<count;i++) {
        const WorldCastleCreekPoint *a=&world_castle_creek_points[i-1],*b=&world_castle_creek_points[i];
        double dx=b->x-a->x,dz=b->z-a->z;
        double t=fmin(1.,fmax(0.,((x-a->x)*dx+(z-a->z)*dz)/(dx*dx+dz*dz)));
        double width=a->half_width+(b->half_width-a->half_width)*t;
        q=fmin(q,hypot(x-a->x-dx*t,z-a->z-dz*t)/width);
    }
    if(q>=1.)return out;
    /* Outer towers use source7.92m radius at1.28scale=10.14m. Their full
     *10.6m footing reserve retains exact24m ground with a C1 bank feather. */
    double protect=world_castle_creek_smooth(10.6,11.8,hypot(fabs(x)-205,fabs(z)-205));
    double weight=(1.-world_castle_creek_smooth(.14,1.,q))*protect;
    out.height_delta=-2.45*weight;
    out.height=base+out.height_delta;
    out.bank=weight;
    out.depth=fmax(0.,WORLD_CASTLE_CREEK_SURFACE_Y-out.height);
    out.submerged=out.depth>.005;
    return out;
}
static inline bool world_castle_creek_bridge_local(double x,double z) {
    return isfinite(x)&&isfinite(z)&&fabs(x)<=6.5&&z>=212.5&&z<=225.;
}
static inline double world_castle_creek_floor_local(double x,double z,double bed) {
    return world_castle_creek_bridge_local(x,z)?fmax(bed,WORLD_CASTLE_CREEK_DECK_Y):bed;
}

#endif
