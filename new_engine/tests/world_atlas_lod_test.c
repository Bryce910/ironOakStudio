#define _POSIX_C_SOURCE 200809L
#include "engine/world_atlas.h"
#include "engine/world_tiles.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

static float atlas_height(double x, double z, void *userdata) {
    (void)userdata;
    return world_atlas_height(x,z);
}

static double seconds(void) {
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return now.tv_sec + now.tv_nsec / 1e9;
}

/* Use actual atlas meshes rather than duplicate the skirt policy. Both supported
 * quality configurations must preserve sample positions, bounds and ownership. */
static void generated_skirt_depths(unsigned cells,double depths[WORLD_TILES_DEFAULT_LODS]) {
    for (unsigned i = 0; i < WORLD_TILES_DEFAULT_LODS; ++i) depths[i] = NAN;
    WorldTilesConfig config = {.height = atlas_height,.cells = cells};
    WorldTilesWorld *world = world_tiles_init(&config);
    assert(world && world_tiles_update(world, -40000, -24500));
    double deadline = seconds() + 20;
    unsigned seen=0;
    while (seen<WORLD_TILES_DEFAULT_LODS) {
        WorldTileMesh mesh = {0};
        if (world_tiles_poll_completed(world, &mesh)) {
            size_t surface_count = (size_t)(mesh.cells + 1) * (mesh.cells + 1);
            assert(mesh.cells==cells);
            assert(mesh.vertex_count > surface_count && mesh.key.lod < WORLD_TILES_DEFAULT_LODS);
            double depth = (double)mesh.vertices[0].y - mesh.vertices[surface_count].y;
            assert(depth > 0);
            float minimum=INFINITY,maximum=-INFINITY;
            for(size_t i=0;i<mesh.vertex_count;i++){
                const WorldTileVertex *v=&mesh.vertices[i];
                assert(isfinite(v->x)&&isfinite(v->y)&&isfinite(v->z));
                assert(isfinite(v->nx)&&isfinite(v->ny)&&isfinite(v->nz));
                double normal_length=sqrt(v->nx*v->nx+v->ny*v->ny+v->nz*v->nz);
                assert(fabs(normal_length-1)<1e-5);
                minimum=fminf(minimum,v->y);maximum=fmaxf(maximum,v->y);
                if(i<surface_count){
                    unsigned col=(unsigned)(i%(mesh.cells+1)),row=(unsigned)(i/(mesh.cells+1));
                    double spacing=(double)mesh.size/mesh.cells;
                    assert(v->x==col*spacing&&v->z==row*spacing);
                    assert(v->y==world_atlas_height(mesh.origin_x+v->x,mesh.origin_z+v->z));
                }
            }
            for(size_t i=0;i<mesh.index_count;i++)assert(mesh.indices[i]<mesh.vertex_count);
            assert(mesh.min_y==minimum&&mesh.max_y==maximum);
            if(isfinite(depths[mesh.key.lod]))assert(fabs(depths[mesh.key.lod]-depth)<.001);
            else seen++;
            depths[mesh.key.lod] = depth;
            world_tiles_release(&mesh);
        } else {
            const struct timespec pause = {0, 100000};
            assert(seconds() < deadline);
            nanosleep(&pause, NULL);
        }
    }
    world_tiles_shutdown(world);
}

int main(void) {
    assert(world_atlas_init());
    const double half_width = world_atlas_width() * .5;
    const double half_depth = world_atlas_depth() * .5;
    const unsigned configurations[]={WORLD_TILES_DEFAULT_CELLS,64};
    for(size_t quality=0;quality<sizeof(configurations)/sizeof(configurations[0]);quality++){
    unsigned cells=configurations[quality];
    double depths[WORLD_TILES_DEFAULT_LODS];
    generated_skirt_depths(cells,depths);
    /* Local crags and trail-valley shoulders have smaller curvature scales than
     * the global atlas. Exhaustively check every possible near-ring interface
     * through the authored rectangles using the actual generated skirt depths. */
    size_t district_count=0;
    const WorldAtlasDistrict *districts=world_atlas_districts(&district_count);
    for(unsigned lod=0;lod<4;lod++){
        double cell=ldexp(WORLD_TILES_DEFAULT_SIZE/cells,(int)lod);
        double coarse_tile=ldexp(WORLD_TILES_DEFAULT_SIZE,(int)lod+1);
        double maximum=0,worst_x=0,worst_z=0;
        size_t count=0;
        for(size_t district=0;district<district_count;district++)for(int axis=0;axis<2;axis++){
            const WorldAtlasDistrict *d=&districts[district];
            double normal_min=axis?d->zmin:d->xmin,normal_max=normal_min+(axis?d->depth:d->width);
            double along_min=axis?d->xmin:d->zmin,along_max=along_min+(axis?d->width:d->depth);
            int edge_begin=(int)floor(normal_min/coarse_tile),edge_end=(int)ceil(normal_max/coarse_tile);
            int along_begin=(int)floor(along_min/cell)-2,along_end=(int)ceil(along_max/cell)+2;
            for(int edge=edge_begin;edge<=edge_end;edge++)for(int sample=along_begin;sample<=along_end;sample++){
                if(sample%2==0)continue;
                double x=axis?sample*cell:edge*coarse_tile,z=axis?edge*coarse_tile:sample*cell;
                double fine=world_atlas_height(x,z);
                double first=world_atlas_height(axis?x-cell:x,axis?z:z-cell);
                double second=world_atlas_height(axis?x+cell:x,axis?z:z+cell);
                double error=fabs(fine-(first+second)*.5);
                if(error>maximum){maximum=error;worst_x=x;worst_z=z;}
                count++;
            }
        }
        double covering_depth=fmin(depths[lod],depths[lod+1]);
        printf("Authored cells%u LOD%u/%u: %zu border vertices; maximum gap %.6f m at %.1f %.1f; actual covering skirt %.1f m\n",
            cells,lod,lod+1,count,maximum,worst_x,worst_z,covering_depth);
        assert(count>100&&maximum<covering_depth);
    }
    for (unsigned lod = 4; lod <= 5; ++lod) {
        double cell = ldexp(WORLD_TILES_DEFAULT_SIZE / cells, (int)lod);
        double coarse_tile = ldexp(WORLD_TILES_DEFAULT_SIZE, (int)lod + 1);
        double maximum = 0, worst_x = 0, worst_z = 0;
        size_t count = 0;
        /* Every mixed-LOD border lies on a coarse tile edge. Only odd fine
         * vertices differ from the straight edge between two coarse samples.
         * Enumerate all possible edges, plus a complete outer-ocean guard. */
        for (int axis = 0; axis < 2; ++axis) {
            double normal_extent = axis ? half_depth : half_width;
            double along_extent = axis ? half_width : half_depth;
            int edge_begin = (int)floor(-normal_extent / coarse_tile) - 1;
            int edge_end = (int)ceil(normal_extent / coarse_tile) + 1;
            int along_begin = (int)floor(-along_extent / cell) - 2;
            int along_end = (int)ceil(along_extent / cell) + 2;
            for (int edge = edge_begin; edge <= edge_end; ++edge) {
                for (int sample = along_begin; sample <= along_end; ++sample) {
                    if (sample % 2 == 0) continue;
                    double x = axis ? sample * cell : edge * coarse_tile;
                    double z = axis ? edge * coarse_tile : sample * cell;
                    double fine = world_atlas_height(x, z);
                    double first = world_atlas_height(axis ? x - cell : x, axis ? z : z - cell);
                    double second = world_atlas_height(axis ? x + cell : x, axis ? z : z + cell);
                    assert(isfinite(fine) && isfinite(first) && isfinite(second));
                    double error = fabs(fine - (first + second) * .5);
                    if (error > maximum) { maximum = error; worst_x = x; worst_z = z; }
                    ++count;
                }
            }
        }
        assert(count > 1000);
        double covering_depth = fmin(depths[lod], depths[lod + 1]);
        printf("Atlas cells%u LOD%u/%u: %zu possible border vertices; maximum gap %.6f m at %.1f %.1f; actual covering skirt %.1f m\n",
               cells,lod, lod + 1, count, maximum, worst_x, worst_z, covering_depth);
        assert(maximum < covering_depth);
    }
    }
    world_atlas_shutdown();
    puts("World atlas LOD skirts cover every possible far-ring border, including negative coordinates and the outer ocean.");
    return 0;
}
