/* CPU-only compact Valdoa overlay, shared masks and preservation regression. */
#include "engine/world_atlas.h"
#include "engine/world_castle_creek.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t sample_hash(uint64_t h,WorldAtlasSample s){
    const unsigned char *bytes=(const unsigned char*)&s;
    for(size_t i=0;i<sizeof(s);i++){h^=bytes[i];h*=UINT64_C(1099511628211);}return h;
}
int main(void){
    assert(sizeof(WorldAtlasSample)==24);
    double x,z,xmin,zmin,width,depth;world_atlas_village_origin(&x,&z);
    world_atlas_village_bounds(&xmin,&zmin,&width,&depth);
    assert(width==560&&depth==560&&xmin==x-280&&zmin==z-280);
    assert(fabs(x-(-1310.4))<1e-8&&fabs(z-(-10207.584747454583))<1e-8);
    assert(world_atlas_village_weight(x,z)==0&&world_atlas_village_sample(x,z).paving==0);
    assert(world_atlas_init()&&world_atlas_init());
    assert(!world_atlas_village_weight(NAN,z)&&!world_atlas_village_weight(x,INFINITY));
    assert(!world_atlas_village_weight(1e300,-1e300)&&!world_atlas_village_sample(NAN,z).path);
    for(int dz=-220;dz<=220;dz+=20)for(int dx=-220;dx<=220;dx+=20){
        WorldAtlasSample s=world_atlas_sample(x+dx,z+dz);
        assert(fabs(s.height-world_castle_creek_sample_local(dx,dz,24).height)<.00001&&s.forest_density==0&&s.shore_distance>1000);
        assert(world_atlas_village_weight(x+dx,z+dz)==1);
    }
    /* Pre-village bytes from the original atlas shared object. Every field
     * outside the new560m square, including its boundary, stays identical. */
    uint64_t unchanged=UINT64_C(14695981039346656037);size_t count=0;
    for(int iz=-40;iz<=40;iz++)for(int ix=-40;ix<=40;ix++){
        double dx=ix*10,dz=iz*10;if(fabs(dx)<280&&fabs(dz)<280)continue;
        unchanged=sample_hash(unchanged,world_atlas_sample(x+dx,z+dz));count++;
        WorldAtlasVillageSample mask=world_atlas_village_sample(x+dx,z+dz);
        assert(mask.path==0&&mask.paving==0&&mask.clearing==0&&mask.grass_exclusion==0);
    }
    assert(count==3536&&unchanged==UINT64_C(0xe4340d323bd69b13));
    double maximum_step=0;
    for(int dz=-280;dz<=280;dz+=7)for(int dx=-280;dx<=280;dx+=7){
        double px=x+dx,pz=z+dz;float h=world_atlas_height(px,pz);
        assert(isfinite(h)&&h>0&&h<200);
        WorldAtlasVillageSample a=world_atlas_village_sample(px,pz);
        assert(a.height_delta==0&&a.path>=0&&a.path<=1&&a.paving>=0&&a.paving<=1&&a.clearing>=0&&a.clearing<=1&&a.grass_exclusion>=0&&a.grass_exclusion<=1);
        maximum_step=fmax(maximum_step,fabs(h-world_atlas_height(px+.001,pz)));
    }
    assert(maximum_step<.003);
    const double route[][2]={{0,220},{0,130},{0,55},{0,-50},{0,-88},{0,-50},{0,55},{-95,55},{-95,125},{0,125},{0,220}};
    size_t stations=0;double length=0;
    for(size_t i=1;i<sizeof(route)/sizeof(route[0]);i++){
        double dx=route[i][0]-route[i-1][0],dz=route[i][1]-route[i-1][1],distance=hypot(dx,dz);int steps=(int)ceil(distance/.5);length+=distance;
        for(int j=0;j<=steps;j++){
            double px=x+route[i-1][0]+dx*j/steps,pz=z+route[i-1][1]+dz*j/steps;
            WorldAtlasVillageSample mask=world_atlas_village_sample(px,pz);
            const double lx=px-x,lz=pz-z;
            const double expected=world_castle_creek_bridge_local(lx,lz)?24.02:24.;
            assert(fabs(world_atlas_walk_height(px,pz)-expected)<.00001&&mask.path>.999&&mask.grass_exclusion>.999);
            assert(world_atlas_landscape_mask(px,pz).trail>.999);stations++;
        }
    }
    assert(length==806&&stations==1622);
    for(int i=0;i<=152;i++) {
        WorldAtlasVillageSample lane=world_atlas_village_sample(x+61.75,z-27+i*.5);
        assert(lane.path>.999&&lane.paving>.94&&lane.grass_exclusion>.999);
        assert(world_atlas_height(x+61.75,z-27+i*.5)==24);
    }
    /* The widened paving and ecology mask must cover the visible shoulders,
     * not merely the existing center walking line. These offsets lay outside
     * the former 2.2m half-width, away from both rounded street endpoints. */
    for(int side=-1;side<=1;side+=2)for(int i=0;i<=132;i++) {
        WorldAtlasVillageSample shoulder=world_atlas_village_sample(x+61.75+side*2.4,z-23+i*.5);
        assert(shoulder.path>.999&&shoulder.paving>.94&&shoulder.grass_exclusion>.999);
    }
    WorldAtlasVillageSample market=world_atlas_village_sample(x,z+8),keep=world_atlas_village_sample(x,z-145);
    assert(market.paving>.94&&keep.grass_exclusion>.999);
    const float before=world_atlas_height(x+251,z+3);world_atlas_shutdown();
    assert(!world_atlas_village_weight(x,z)&&!world_atlas_village_sample(x,z).path);
    assert(world_atlas_init());assert(world_atlas_height(x+251,z+3)==before);world_atlas_shutdown();
    printf("PASS village: sculpted creek on24m terrace, independent bridge floor;806m/%zu stations shared clear roads;3536 pre-village exterior samples bit-identical; finite smooth feather and deterministic rebuild\n",stations);
}
