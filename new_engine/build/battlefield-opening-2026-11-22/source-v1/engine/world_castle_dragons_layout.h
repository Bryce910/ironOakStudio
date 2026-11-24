#ifndef VALWOOD_WORLD_CASTLE_DRAGONS_LAYOUT_H
#define VALWOOD_WORLD_CASTLE_DRAGONS_LAYOUT_H
#include <math.h>

/* Local east/south metres from world_atlas_village_origin(). The entire
 * seventeen-metre wing sweep is outside the +/-205m town wall and creek.
 * These existing natural benches were surveyed without changing terrain.
 * Reserve trees/large plants through radius12, feather to18m; low grass may
 * remain at the edge. C compatible so ecology and runtime share one reserve. */
#define WORLD_CASTLE_DRAGON_COUNT 2u
static const double world_castle_dragon_roosts[WORLD_CASTLE_DRAGON_COUNT][2]={
    {350.,60.}, {-375.,60.}
};
static inline double world_castle_dragons_ground_exclusion_local(double x,double z){
    if(!isfinite(x)||!isfinite(z))return 0.;
    double weight=0.;
    for(unsigned i=0;i<WORLD_CASTLE_DRAGON_COUNT;i++){
        const double dx=x-world_castle_dragon_roosts[i][0],dz=z-world_castle_dragon_roosts[i][1];
        if(fabs(dx)>=18.||fabs(dz)>=18.)continue;
        const double distance=hypot(dx,dz);
        double t=fmin(1.,fmax(0.,(distance-12.)/6.));
        const double w=1.-t*t*(3.-2.*t);
        if(w>weight)weight=w;
    }
    return weight;
}
/* VSync completion is useful at these two exterior observation clearings.
 * Keep the presentation region independent of dragon visibility/residency so
 * their arrival, departure or a QA-disabled bank cannot change frame pacing.
 * The previous state supplies90m entry/110m exit hysteresis. This deliberately
 * excludes the distant forest and never changes the selected swap interval. */
static inline int world_castle_dragons_roost_presentation_local(double x,double z,int previous){
    if(!isfinite(x)||!isfinite(z))return 0;
    const double radius=previous?110.:90.;
    if(fabs(z-60.)>=radius||x<=-375.-radius||x>=350.+radius)return 0;
    for(unsigned i=0;i<WORLD_CASTLE_DRAGON_COUNT;i++){
        const double dx=x-world_castle_dragon_roosts[i][0],dz=z-world_castle_dragon_roosts[i][1];
        if(fabs(dx)>=radius||fabs(dz)>=radius)continue;
        if(dx*dx+dz*dz<radius*radius)return 1;
    }
    return 0;
}
#endif
