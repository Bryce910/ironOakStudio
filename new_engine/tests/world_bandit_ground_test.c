#include "engine/world_atlas.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "engine/world_bandit_route.inc"

static void empty(double x,double z){
    WorldAtlasStartingSample s=world_atlas_bandit_sample(x,z);
    assert(s.height_delta==0&&s.path==0&&s.paving==0&&s.clearing==0&&s.grass_exclusion==0);
}
int main(void){
    empty(-20600,8946.320622878819);
    assert(world_atlas_init());
    double x,z,w,d;world_atlas_bandit_bounds(&x,&z,&w,&d);
    assert(w>100&&w<200&&d>100&&d<200);
    empty(NAN,z);empty(x,INFINITY);empty(0,0);
    empty(x-.01,z+d*.5);empty(x+w+.01,z+d*.5);
    empty(x+w*.5,z-.01);empty(x+w*.5,z+d+.01);
    WorldAtlasSample center=world_atlas_sample(-20600,8946.320622878819);
    assert(center.region==WORLD_ATLAS_OLD_TANTHOON&&center.shore_distance>500&&center.height>187&&center.height<189);
    assert(center.forest_density==0);
    WorldAtlasStartingSample hearth=world_atlas_bandit_sample(-20600,8946.320622878819);
    assert(hearth.grass_exclusion>.99&&hearth.clearing>.99&&hearth.height_delta==0&&hearth.paving==0);
    for(size_t k=0;k<sizeof(bandit_walk_route)/sizeof(bandit_walk_route[0]);k++){
        WorldAtlasStartingSample s=world_atlas_bandit_sample(bandit_walk_route[k][0],bandit_walk_route[k][1]);
        assert(s.path>.99&&s.grass_exclusion>.99&&s.height_delta==0);
        assert(world_atlas_sample(bandit_walk_route[k][0],bandit_walk_route[k][1]).forest_density<.001);
    }
    for(int iz=0;iz<=80;iz++)for(int ix=0;ix<=80;ix++){
        double wx=x+w*ix/80.,wz=z+d*iz/80.;
        WorldAtlasStartingSample s=world_atlas_bandit_sample(wx,wz);
        assert(s.height_delta==0&&s.paving==0);
        assert(isfinite(s.path)&&s.path>=0&&s.path<=1&&s.clearing>=0&&s.clearing<=1&&s.grass_exclusion>=0&&s.grass_exclusion<=1);
        if(ix==0||ix==80||iz==0||iz==80)assert(s.path<.00001&&s.clearing<.00001&&s.grass_exclusion<.00001);
    }
    world_atlas_shutdown();
    empty(-20600,8946.320622878819);
    puts("PASS bandit ground: dry mapped site, walkable clear loop, hearth vegetation exclusion, finite bounded cached masks, natural heights retained, init/shutdown safety.");
}
