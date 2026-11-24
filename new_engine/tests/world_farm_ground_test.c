/* Compile the real atlas once with a test-local gate on its farm mask. This
 * gives an independent pre-farm control without keeping a second atlas copy
 * or adding a test switch to production code. */
#include "engine/world_village_farms.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
static bool test_farms_enabled=true;
static double test_farm_mask(double x,double z) {
    return test_farms_enabled?world_village_farm_mask_local(x,z):0.;
}
#define world_village_farm_mask_local test_farm_mask
#include "engine/world_atlas.c"
#undef world_village_farm_mask_local

typedef struct {
    double x,z;
    WorldAtlasSample sample;
    WorldAtlasVillageSample village;
    WorldAtlasLandscapeSample landscape;
    float walk;
    bool interior,outside;
} FarmProbe;
static FarmProbe probes[8192];
static size_t probe_count;
static double farm_edge(const WorldVillageFarmField *f,double x,double z) {
    return fmin(fmin(x-f->xmin,f->xmax-x),fmin(z-f->zmin,f->zmax-z));
}
static void record_probe(double x,double z,double ox,double oz,bool interior,bool outside) {
    assert(probe_count<sizeof(probes)/sizeof(probes[0]));
    FarmProbe *p=&probes[probe_count++];p->x=x;p->z=z;p->interior=interior;p->outside=outside;
    p->sample=world_atlas_sample(ox+x,oz+z);
    p->village=world_atlas_village_sample(ox+x,oz+z);
    p->landscape=world_atlas_landscape_mask(ox+x,oz+z);
    p->walk=world_atlas_walk_height(ox+x,oz+z);
    assert(isfinite(p->sample.height)&&isfinite(p->walk)&&isfinite(p->landscape.trail));
    assert(p->village.height_delta==0&&p->village.grass_exclusion>=0&&p->village.grass_exclusion<=1);
    if(interior) {
        assert(p->sample.height==24&&p->walk==24);
        assert(p->village.grass_exclusion>.999999&&p->landscape.trail>.999999);
        float water_y=0,water_depth=0;
        assert(!world_atlas_local_water(ox+x,oz+z,&water_y,&water_depth));
    }
}
int main(void) {
    assert(WORLD_VILLAGE_FARM_COUNT==2&&WORLD_VILLAGE_FARM_COUNT<=4);
    assert(!world_village_farm_mask_local(NAN,0)&&!world_village_farm_mask_local(0,INFINITY));
    assert(!world_village_farm_mask_local(-INFINITY,0)&&!world_village_farm_mask_local(1e300,-1e300));
    assert(!world_village_farm_mask_local(0,0));
    size_t analytic=0;
    for(size_t i=0;i<WORLD_VILLAGE_FARM_COUNT;i++) {
        const WorldVillageFarmField *f=&world_village_farm_fields[i];
        assert(isfinite(f->xmin)&&isfinite(f->zmin)&&isfinite(f->xmax)&&isfinite(f->zmax));
        assert(f->xmax>f->xmin&&f->zmax>f->zmin&&f->row_spacing>=.1);
        assert(f->row_phase==f->row_spacing*.5);
        assert(f->row_spacing==(i?.25:1.));
        const double cx=(f->xmin+f->xmax)*.5,cz=(f->zmin+f->zmax)*.5;
        assert(world_village_farm_mask_local(cx,cz)==1);
        /* Each of four field edges has the same C1 .8m feather. Corn and
         * wheat remain independent: the .5 boundary value is not a max with
         * a mistakenly enlarged neighboring field. */
        for(int axis=0;axis<2;axis++)for(int side=0;side<2;side++) {
            for(int station=-64;station<=64;station++) {
                const double e=station/64.,x=axis?cx:(side?f->xmax-e:f->xmin+e);
                const double z=axis?(side?f->zmax-e:f->zmin+e):cz;
                const double t=fmin(1.,fmax(0.,(e+.6)/.8)),expected=t*t*(3.-2.*t);
                const double actual=world_village_farm_mask_local(x,z);
                assert(isfinite(actual)&&actual>=0&&actual<=1&&fabs(actual-expected)<1e-12);analytic++;
            }
            const double h=1e-5;
            for(int endpoint=0;endpoint<2;endpoint++) {
                const double e=endpoint?.2:-.6;
                const double x0=axis?cx:(side?f->xmax-e:f->xmin+e),z0=axis?(side?f->zmax-e:f->zmin+e):cz;
                const double dx=axis?0:(side?-h:h),dz=axis?(side?-h:h):0;
                const double derivative=(world_village_farm_mask_local(x0+dx,z0+dz)-world_village_farm_mask_local(x0-dx,z0-dz))/(2*h);
                assert(fabs(derivative)<.000025);
            }
        }
    }
    double ox,oz;world_atlas_village_origin(&ox,&oz);assert(world_atlas_init());
    for(size_t i=0;i<WORLD_VILLAGE_FARM_COUNT;i++) {
        const WorldVillageFarmField *f=&world_village_farm_fields[i];
        /* Include all boundaries plus enough shoulder for the 1.094m cache
         * texels. Deep interiors must exclude grass completely; the cache
         * is intentionally smoother than the analytic .8m boundary. */
        for(double z=f->zmin-4;z<=f->zmax+4;z+=1.25)for(double x=f->xmin-4;x<=f->xmax+4;x+=1.25) {
            const double edge=farm_edge(f,x,z);
            record_probe(x,z,ox,oz,edge>2,edge< -3);
        }
    }
    const double controls[][2]={{0,8},{0,-145},{61.75,0},{0,220},{-95,125},{-150,-160},{0,-200},{200,-160},{-280,0},{280,0}};
    for(size_t i=0;i<sizeof(controls)/sizeof(controls[0]);i++)record_probe(controls[i][0],controls[i][1],ox,oz,false,true);
    for(int z=-280;z<=280;z+=35)for(int x=-280;x<=280;x+=35) {
        bool outside=true;
        for(size_t i=0;i<WORLD_VILLAGE_FARM_COUNT;i++)
            if(farm_edge(&world_village_farm_fields[i],x,z)>-3)outside=false;
        if(outside)record_probe(x,z,ox,oz,false,true);
    }
    world_atlas_shutdown();test_farms_enabled=false;assert(world_atlas_init());
    size_t suppressed=0,interiors=0,unchanged=0;
    for(size_t i=0;i<probe_count;i++) {
        const FarmProbe *p=&probes[i];
        const WorldAtlasSample baseline=world_atlas_sample(ox+p->x,oz+p->z);
        const WorldAtlasVillageSample mask=world_atlas_village_sample(ox+p->x,oz+p->z);
        const WorldAtlasLandscapeSample landscape=world_atlas_landscape_mask(ox+p->x,oz+p->z);
        /* Every source atlas property and existing paving value survives the
         * farm overlay exactly, including already-paved control streets. */
        assert(p->sample.height==baseline.height&&p->sample.region==baseline.region);
        assert(p->sample.forest_density==baseline.forest_density&&p->sample.moisture==baseline.moisture);
        assert(p->sample.shore_distance==baseline.shore_distance&&p->sample.land_weight==baseline.land_weight);
        assert(p->village.height_delta==mask.height_delta&&p->village.path==mask.path);
        assert(p->village.paving==mask.paving&&p->village.clearing==mask.clearing);
        assert(p->walk==world_atlas_walk_height(ox+p->x,oz+p->z));
        assert(p->landscape.height==landscape.height&&p->landscape.rock==landscape.rock);
        assert(p->landscape.meadow==landscape.meadow&&p->landscape.moss==landscape.moss);
        assert(p->village.grass_exclusion>=mask.grass_exclusion&&p->landscape.trail>=landscape.trail);
        if(p->interior) {
            interiors++;
            if(mask.grass_exclusion<.01&&landscape.trail<.01) {
                assert(p->village.grass_exclusion>.999999&&p->landscape.trail>.999999);suppressed++;
            }
        }
        if(p->outside) {
            assert(p->village.grass_exclusion==mask.grass_exclusion&&p->landscape.trail==landscape.trail);unchanged++;
        }
    }
    if(!(interiors>500&&suppressed>500&&unchanged>500))
        fprintf(stderr,"farm coverage interiors=%zu suppressed=%zu unchanged=%zu probes=%zu\n",interiors,suppressed,unchanged,probe_count);
    assert(interiors>500&&suppressed>500&&unchanged>500);
    world_atlas_shutdown();
    printf("PASS farm ground CPU: %zu analytic edge samples; %zu actual atlas probes, %zu interiors, %zu previously-wild interiors suppressed, %zu exterior/street controls exact; height24/path/paving/source ecology preserved in enabled/disabled rebuild\n",analytic,probe_count,interiors,suppressed,unchanged);
}
