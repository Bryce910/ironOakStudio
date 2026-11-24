#include "engine/world_atlas.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint64_t number(uint64_t hash,float value){
    uint32_t word;memcpy(&word,&value,4);
    for(int i=0;i<4;i++){hash^=(word>>(8*i))&255;hash*=UINT64_C(1099511628211);}return hash;
}
static double smooth(double lo,double hi,double value){
    double t=fmax(0,fmin(1,(value-lo)/(hi-lo)));return t*t*(3-2*t);
}
int main(void){
    const WorldAtlasPool *p=world_atlas_rainforest_pool();assert(p);
    assert(p->surface_y==134&&p->radius_x==10&&p->radius_z==16&&p->bank_feather==6);
    float level=1,depth=1;assert(!world_atlas_local_water(p->x,p->z,&level,&depth)&&level==0&&depth==0);
    assert(world_atlas_init());assert(world_atlas_local_water(p->x,p->z,&level,&depth));
    assert(level==134&&fabs(depth-.95)<.0001);
    assert(world_atlas_sample(p->x,p->z).forest_density==0);
    assert(!world_atlas_local_water(NAN,0,&level,&depth)&&level==0&&depth==0);
    assert(!world_atlas_local_water(0,INFINITY,NULL,NULL));
    size_t wet=0,dry=0;float max_grade=0;double shore_min=1,shore_max=0;
    for(int angle=0;angle<72;angle++){
        double t=angle*6.283185307179586/72,co=cos(t),si=sin(t);int changes=0;bool was_wet=true;
        float prev=world_atlas_height(p->x,p->z);
        for(int step=1;step<=160;step++){
            double q=step/100.,x=p->x+co*p->radius_x*q,z=p->z+si*p->radius_z*q;
            float h=world_atlas_height(x,z);assert(isfinite(h));
            bool water=world_atlas_local_water(x,z,&level,&depth);
            if(water){wet++;assert(q<1&&level==134&&depth>0&&fabs((level-h)-depth)<.00002);}
            else{dry++;assert(level==0&&depth==0);}
            if(water!=was_wet){
                changes++;assert(!water&&q>.65&&q<.97);
                shore_min=fmin(shore_min,q);shore_max=fmax(shore_max,q);
            }
            was_wet=water;
            max_grade=fmaxf(max_grade,fabsf(h-prev)/(float)hypot(co*p->radius_x*.01,si*p->radius_z*.01));prev=h;
        }
        assert(changes==1&&!was_wet);
        /* C1 feather reaches untouched terrain without a cliff. */
        double outer=1/hypot(co*p->radius_x/16,si*p->radius_z/22);
        double x=p->x+co*p->radius_x*outer,z=p->z+si*p->radius_z*outer;
        float a=world_atlas_height(x-co*.01,z-si*.01),b=world_atlas_height(x+co*.01,z+si*.01);
        assert(fabsf(a-b)<.01);
    }
    assert(wet>3000&&dry>3000&&max_grade<.8);
    /* The new asymmetric promontories need visible variation at walking
     * scale. A nearly oval rim, including the old 3/5-wave edge, fails this.
     * Every previous dry point also stays dry: compare the original shelf
     * formula over its complete inner envelope, not a few chosen roots. */
    assert(shore_max-shore_min>.12);
    size_t protected_dry=0,reclaimed=0;
    for(int iz=-160;iz<=160;iz++)for(int ix=-100;ix<=100;ix++){
        double dx=ix*.1,dz=iz*.1,q=hypot(dx/p->radius_x,dz/p->radius_z);
        if(q>=1)continue;
        double theta=atan2(dz/p->radius_z,dx/p->radius_x);
        double r=q/(.88+.035*sin(theta*3+.7)+.025*sin(theta*5-1.1));
        double old=p->surface_y-.95*(1-smooth(.08,1,r))+.55*smooth(.90,1.35,r);
        float actual=world_atlas_height(p->x+dx,p->z+dz);
        assert(actual>=old-.00002);
        bool water=world_atlas_local_water(p->x+dx,p->z+dz,NULL,NULL);
        if(old>=p->surface_y-.005){assert(!water);protected_dry++;}
        else if(!water)reclaimed++;
    }
    assert(protected_dry>10000&&reclaimed>1500);
    /* Golden bits captured from the pre-pool terrain: all10232 surrounding
     * half-meter probes OUTSIDE compact support remain exactly unchanged. */
    uint64_t hash=UINT64_C(14695981039346656037);size_t outside=0;
    for(int iz=-60;iz<=60;iz++)for(int ix=-60;ix<=60;ix++){
        double dx=ix*.5,dz=iz*.5;if((dx/16)*(dx/16)+(dz/22)*(dz/22)<1)continue;
        hash=number(hash,world_atlas_height(p->x+dx,p->z+dz));outside++;
    }
    assert(outside==10232&&hash==UINT64_C(0x93e6356265ca4039));
    size_t count=0;const WorldAtlasTrailPoint *path=world_atlas_rainforest_trail_points(&count);
    for(size_t i=1;i<count;i++)for(int step=0;step<=100;step++){
        double t=step/100.,x=path[i-1].x+(path[i].x-path[i-1].x)*t,z=path[i-1].z+(path[i].z-path[i-1].z)*t;
        assert(!world_atlas_local_water(x,z,NULL,NULL));
        assert(hypot((x-p->x)/16,(z-p->z)/22)>1);
    }
    world_atlas_shutdown();assert(!world_atlas_local_water(p->x,p->z,NULL,NULL));
    printf("PASS local pool: asymmetric shore radius%.3f–%.3f on72rays, %zu wet/%zu dry probes, maximum grade%.4f; %zu original dry probes protected/%zu reclaimed,10232 outside heights bit-identical, route outside support, lifecycle/invalid input clean\n",shore_min,shore_max,wet,dry,max_grade,protected_dry,reclaimed);
    return 0;
}
