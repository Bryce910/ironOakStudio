#define _POSIX_C_SOURCE 200809L
#include "engine/world_atlas.h"
#include "engine/world_castle_creek.h"
#include "engine/world_tiles.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

static float terrain(double x,double z,void *unused) {
    (void)unused;return world_atlas_height(x,z);
}
static double seconds(void) {
    struct timespec t;assert(clock_gettime(CLOCK_MONOTONIC,&t)==0);
    return t.tv_sec+t.tv_nsec/1e9;
}
int main(void) {
    float level=123,depth=456;
    assert(!world_atlas_local_water(0,0,&level,&depth)&&level==0&&depth==0);
    assert(world_atlas_init());
    double ox,oz;world_atlas_village_origin(&ox,&oz);
    size_t samples=0,wet=0,masked=0,enclosed=0;
    for(int iz=0;iz<=892;iz++)for(int ix=0;ix<=900;ix++) {
        double x=-225+ix*.5,z=-223+iz*.5;
        WorldCastleCreekSample expected=world_castle_creek_sample_local(x,z,24);
        float actual=world_atlas_height(ox+x,oz+z);
        assert(fabs(actual-expected.height)<.00001);
        assert(fabs(world_atlas_sample(ox+x,oz+z).height-actual)<.00001);
        bool water=world_atlas_local_water(ox+x,oz+z,&level,&depth);
        assert(water==expected.submerged);
        if(fabs(x)<=205&&fabs(z)<=205){assert(!water&&actual==24);enclosed++;}
        if(water) {
            assert(fabs(level-22.9)<.00001&&fabs(depth-expected.depth)<.00001);
            assert(actual<level&&depth>.005);wet++;
        } else assert(level==0&&depth==0);
        float floor=world_atlas_walk_height(ox+x,oz+z);
        assert(fabs(floor-world_castle_creek_floor_local(x,z,expected.height))<.00001);
        WorldAtlasVillageSample materials=world_atlas_village_sample(ox+x,oz+z);
        if(expected.bank>.60) {
            assert(materials.grass_exclusion>.9999);
            assert(materials.paving<=1.-expected.bank+.00001);masked++;
        }
        samples++;
    }
    assert(wet>15000&&masked>10000&&enclosed>670000);
    assert(world_atlas_height(ox,oz+219)<22&&fabs(world_atlas_walk_height(ox,oz+219)-24.02)<.00001);
    assert(world_atlas_height(ox,oz-46)==24&&!world_atlas_local_water(ox,oz-46,NULL,NULL));
    assert(world_atlas_height(ox+80,oz-125)==24&&!world_atlas_local_water(ox+80,oz-125,NULL,NULL));
    /* Real near meshes use terrain bed, never bridge walk height. This prevents
     * a paper bridge over an invisible terrain causeway from passing the test. */
    WorldTilesConfig config={.height=terrain,.cells=64,.lod_levels=1};
    WorldTilesWorld *stream=world_tiles_init(&config);assert(stream);
    assert(world_tiles_update(stream,ox,oz+219));
    double deadline=seconds()+30;size_t vertices=0,water_vertices=0,under_bridge=0,meshes=0;
    for(;;) {
        WorldTileMesh mesh={0};
        if(world_tiles_poll_completed(stream,&mesh)) {
            assert(mesh.cells==64&&mesh.key.lod==0);
            size_t count=(size_t)(mesh.cells+1)*(mesh.cells+1);
            for(size_t i=0;i<count;i++) {
                const WorldTileVertex *v=&mesh.vertices[i];
                double x=mesh.origin_x+v->x,z=mesh.origin_z+v->z;
                assert(v->y==world_atlas_height(x,z));vertices++;
                if(world_atlas_local_water(x,z,NULL,NULL)) {
                    assert(v->y<22.9);water_vertices++;
                    if(world_castle_creek_bridge_local(x-ox,z-oz)) {
                        assert(v->y<world_atlas_walk_height(x,z));under_bridge++;
                    }
                }
            }
            meshes++;world_tiles_release(&mesh);
        } else {
            WorldTilesMetrics metrics={0};world_tiles_metrics(stream,&metrics);
            if(meshes&&metrics.delivered==metrics.desired)break;
            assert(seconds()<deadline);
            const struct timespec pause={0,1000000};nanosleep(&pause,NULL);
        }
    }
    assert(water_vertices>80&&under_bridge>4);
    world_tiles_shutdown(stream);
    assert(world_atlas_local_water(NAN,oz,&level,&depth)==false&&level==0&&depth==0);
    world_atlas_shutdown();
    printf("PASS integrated exterior creek: %zu atlas/water/floor samples, %zu completely dry enclosed town samples, %zu wet/%zu grass-masked; %zu real2m mesh vertices in%zu tiles, %zu submerged/%zu beneath bridge.\n",samples,enclosed,wet,masked,vertices,meshes,water_vertices,under_bridge);
}
