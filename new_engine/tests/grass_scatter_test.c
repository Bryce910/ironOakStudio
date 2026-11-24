#include "engine/grass_scatter.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void){
    GrassScatterSeed near[GRASS_SCATTER_CELL_MAX],far[GRASS_SCATTER_CELL_MAX],again[GRASS_SCATTER_CELL_MAX];
    int total=0,quadrants[4]={0};
    int cells=(int)ceilf(64.0f/GRASS_SCATTER_STEP)+1;
    for(int z=-cells;z<=cells;z++)for(int x=-cells;x<=cells;x++){
        float wx,wz;grass_scatter_center(x,z,&wx,&wz);
        float distance=hypotf(wx,wz),cover=grass_scatter_cover(wx,wz);
        assert(cover>=0&&cover<=1);
        int a=grass_scatter_cell(x,z,1,1,0,near);
        int b=grass_scatter_cell(x,z,1,1,60,far);
        int c=grass_scatter_cell(x,z,1,1,0,again);
        assert(a>=b&&a<=GRASS_SCATTER_CELL_MAX&&b>=0&&a==c);
        assert(memcmp(near,again,a*sizeof(*near))==0);
        assert(memcmp(near,far,b*sizeof(*near))==0); // LOD retains the same rooted blades.
        // Deferred emission must preserve the exact visible field while letting
        // the world reject hidden/wet tufts before generating their blades.
        int split=grass_scatter_count_at(x,z,wx,wz,1,1,distance);
        memset(again,0,sizeof(again));
        grass_scatter_emit_at(x,z,wx,wz,split,again);
        int combined=grass_scatter_cell(x,z,1,1,distance,near);
        assert(split==combined);
        assert(memcmp(near,again,split*sizeof(*near))==0);
        assert(grass_scatter_cell(x,z,0,1,0,again)==0);
        assert(grass_scatter_cell(x,z,1,0,0,again)==0);
        assert(grass_scatter_cell(x,z,NAN,1,0,again)==0);
        for(int i=0;i<a;i++){
            assert(hypotf(near[i].x-wx,near[i].z-wz)<.179f);
            assert(near[i].length_multiplier>0&&near[i].length_multiplier<1.5f);
            assert(isfinite(near[i].yaw)&&fabsf(near[i].variation)<=1);
        }
        if(distance<=64){
            int n=grass_scatter_cell(x,z,1,1,distance,near);
            total+=n;quadrants[(wx>=0)+2*(wz>=0)]+=n;
        }
    }
    // Normal density stays within the renderer's budget over the whole 128 m field.
    assert(total>120000&&total<180000);
    for(int q=0;q<4;q++)assert(quadrants[q]>total*.19&&quadrants[q]<total*.31);
    assert(grass_scatter_count_at(0,0,NAN,0,1,1,0)==0);
    assert(grass_scatter_count_at(0,0,0,INFINITY,1,1,0)==0);
    assert(grass_scatter_count_at(0,0,0,0,1,INFINITY,0)==0);
    assert(grass_scatter_count_at(0,0,0,0,1,1,INFINITY)==0);
    struct {GrassScatterSeed seeds[GRASS_SCATTER_CELL_MAX]; unsigned guard;} bounded={0};
    bounded.guard=0x13579bdfu;
    grass_scatter_emit_at(0,0,0,0,GRASS_SCATTER_CELL_MAX+1,bounded.seeds);
    assert(bounded.guard==0x13579bdfu);
    memcpy(again,bounded.seeds,sizeof(again));
    grass_scatter_emit_at(0,0,0,0,0,bounded.seeds);
    grass_scatter_emit_at(0,0,0,0,-1,bounded.seeds);
    grass_scatter_emit_at(0,0,NAN,0,1,bounded.seeds);
    grass_scatter_emit_at(0,0,0,0,1,NULL);
    assert(memcmp(again,bounded.seeds,sizeof(again))==0);
    puts("Grass scatter: deterministic roots, deferred emission, LOD retention, masks, bounds, negative coordinates, full field budget passed.");
    printf("Full field %d blades; quadrants %d %d %d %d\n",total,quadrants[0],quadrants[1],quadrants[2],quadrants[3]);
    return 0;
}
