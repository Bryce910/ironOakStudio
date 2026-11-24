#include "engine/world_atlas.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {double r,f,hr,hf,height,sr,sf,feather,paving,surface_hr,surface_hf;} StartingPad;
typedef struct {double ar,af,br,bf,width,feather;} StartingPath;
typedef struct {double r,f,ir,if_,or_,of;} StartingClearing;
typedef struct {double x,z,xmin,xmax,zmin,zmax,cosine,sine;} StartingExclusion;
#include "engine/world_starting_layout.inc"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static void world(double r,double f,double *x,double *z){
    *x=starting_origin[0]+r*starting_right[0]+f*starting_forward[0];
    *z=starting_origin[1]+r*starting_right[1]+f*starting_forward[1];
}
int main(void){
    assert(world_atlas_starting_sample(0,0).height_delta==0);
    assert(world_atlas_init());
    assert(fabs(world_atlas_height(starting_origin[0],starting_origin[1])-335.796)<.002);
    assert(starting_spawn_radius==4&&starting_edge_feather>=8);
    assert(COUNT(starting_pads)==6&&COUNT(starting_clearings)==6&&COUNT(starting_paths)==15);
    assert(COUNT(starting_no_grass)>40);
    for(size_t i=0;i<COUNT(starting_no_grass);i++){
        const StartingExclusion *e=&starting_no_grass[i];
        assert(world_atlas_starting_sample(e->x,e->z).grass_exclusion>.99);
    }
    for(size_t i=0;i<COUNT(starting_pads);i++){
        const StartingPad *p=&starting_pads[i];double x,z;world(p->r,p->f,&x,&z);
        assert(fabs(world_atlas_height(x,z)-p->height)<.002);
        WorldAtlasStartingSample a=world_atlas_starting_sample(x,z);
        assert(a.clearing>.999&&a.paving>=(p->paving-.001));
        assert(world_atlas_sample(x,z).forest_density==0);
        // A stable foundation, rather than a slope hidden by burial.
        assert(fabs(world_atlas_height(x+.5,z)-world_atlas_height(x-.5,z))<.01);
        assert(fabs(world_atlas_height(x,z+.5)-world_atlas_height(x,z-.5))<.01);
    }
    double length=0,max_grade=0;
    for(size_t i=0;i<COUNT(starting_paths);i++){
        const StartingPath *p=&starting_paths[i];double span=hypot(p->br-p->ar,p->bf-p->af);length+=span;
        unsigned steps=(unsigned)ceil(span*4);double previous=0;
        for(unsigned j=0;j<=steps;j++){
            double t=(double)j/steps,x,z;world(p->ar+(p->br-p->ar)*t,p->af+(p->bf-p->af)*t,&x,&z);
            WorldAtlasStartingSample a=world_atlas_starting_sample(x,z);
            assert(a.path>.98&&a.height_delta>-20&&a.height_delta<20);
            double h=world_atlas_height(x,z);
            if(j){double grade=fabs(h-previous)/(span/steps);max_grade=fmax(max_grade,grade);assert(grade<.25);}
            previous=h;
        }
    }
    assert(length>300&&length<310); // arrival plus closed loop; return adds43m.
    double xmin,zmin,width,depth;world_atlas_starting_bounds(&xmin,&zmin,&width,&depth);
    assert(xmin==starting_bounds[0]&&zmin==starting_bounds[1]&&width==starting_bounds[2]&&depth==starting_bounds[3]);
    for(unsigned i=0;i<=128;i++)for(unsigned j=0;j<=128;j++){
        double x=xmin+width*i/128.,z=zmin+depth*j/128.;WorldAtlasStartingSample a=world_atlas_starting_sample(x,z);
        assert(isfinite(a.height_delta)&&fabs(a.height_delta)<20);
        assert(a.path>=0&&a.path<=1&&a.paving>=0&&a.paving<=1&&a.clearing>=0&&a.clearing<=1);
        if(i==0||j==0||i==128||j==128)assert(fabs(a.height_delta)<1e-9&&a.path==0&&a.clearing==0);
    }
    for(unsigned i=0;i<16;i++){
        double x,z;world(starting_rf_bounds[0],starting_rf_bounds[1]+i*(starting_rf_bounds[3]-starting_rf_bounds[1])/15,&x,&z);
        assert(fabs(world_atlas_height(x-.001,z)-world_atlas_height(x+.001,z))<.01);
    }
    assert(world_atlas_starting_sample(NAN,0).path==0&&world_atlas_starting_sample(INFINITY,0).clearing==0);
    world_atlas_shutdown();assert(world_atlas_starting_sample(starting_origin[0],starting_origin[1]).path==0);
    assert(world_atlas_init());assert(fabs(world_atlas_height(starting_origin[0],starting_origin[1])-335.796)<.002);world_atlas_shutdown();
    printf("Starting terrain:6 stable foundations,%.3fm route+return, maximum0.25m grade %.4f, zero-mask boundary, preserved spawn and clean reinitialization.\n",length+43.007,max_grade);
}
