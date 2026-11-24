#ifndef VALWOOD_WORLD_VILLAGE_FARMS_H
#define VALWOOD_WORLD_VILLAGE_FARMS_H
#include <math.h>
#include <stddef.h>
/* Village-local east/south metres. These authored rectangles drive both
 * wild-grass suppression and the rendered cultivated soil. Height is unchanged. */
typedef struct {double xmin,zmin,xmax,zmax,row_spacing,row_phase;} WorldVillageFarmField;
#include "engine/world_village_farms.inc"
#define WORLD_VILLAGE_FARM_COUNT (sizeof(world_village_farm_fields)/sizeof(world_village_farm_fields[0]))
static inline double world_village_farm_mask_local(double x,double z) {
    if(!isfinite(x)||!isfinite(z))return 0.;
    double result=0.;
    for(size_t i=0;i<WORLD_VILLAGE_FARM_COUNT;i++) {
        const WorldVillageFarmField *f=&world_village_farm_fields[i];
        double edge=fmin(fmin(x-f->xmin,f->xmax-x),fmin(z-f->zmin,f->zmax-z));
        if(edge<=-.6)continue;
        double t=fmin(1.,fmax(0.,(edge+.6)/.8));
        result=fmax(result,t*t*(3.-2.*t));
    }
    return result;
}
#endif
