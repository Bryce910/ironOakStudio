#include "engine/world_atlas.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static double smooth(double lo,double hi,double value){
    double t=fmin(1,fmax(0,(value-lo)/(hi-lo)));
    return t*t*(3-2*t);
}
static double ellipse(double u,double v){
    double du=(u-.235)/.065,dv=(v-.670)/.085;
    return du*du+dv*dv;
}
static float weight_uv(double u,double v){
    double x,z;world_atlas_uv_to_world(u,v,&x,&z);
    return world_atlas_rainforest_weight(x,z);
}
static uint64_t word(uint64_t hash,uint32_t value){
    for(int byte=0;byte<4;byte++){
        hash^=(value>>(byte*8))&255;hash*=UINT64_C(1099511628211);
    }
    return hash;
}
static uint64_t number(uint64_t hash,float value){
    uint32_t bits;memcpy(&bits,&value,sizeof(bits));return word(hash,bits);
}
static uint64_t sample_hash(uint64_t hash,WorldAtlasSample sample){
    hash=number(hash,sample.height);hash=word(hash,(uint32_t)sample.region);
    hash=number(hash,sample.forest_density);hash=number(hash,sample.moisture);
    hash=number(hash,sample.shore_distance);return number(hash,sample.land_weight);
}
int main(void){
    /* The renderer's Python/ctypes clients rely on these exact six fields. */
    assert(sizeof(WorldAtlasSample)==24&&offsetof(WorldAtlasSample,region)==4&&
           offsetof(WorldAtlasSample,forest_density)==8&&offsetof(WorldAtlasSample,land_weight)==20);
    assert(WORLD_ATLAS_REGION_COUNT==8);
    assert(world_atlas_rainforest_weight(0,0)==0);
    assert(world_atlas_init());assert(world_atlas_init());
    assert(weight_uv(.235,.670)==1);
    assert(!world_atlas_rainforest_weight(NAN,0)&&!world_atlas_rainforest_weight(0,INFINITY));
    assert(!world_atlas_rainforest_weight(1e300,-1e300));

    /* Golden bits captured BEFORE the ecology overlay. Terrain fields at every
     * grid point and all fields where weight=0 must remain bit-identical. */
    uint64_t geometry=UINT64_C(14695981039346656037),excluded=geometry;
    size_t excluded_count=0;
    for(int iz=0;iz<=64;iz++)for(int ix=0;ix<=128;ix++){
        double u=ix/128.,v=iz/64.;WorldAtlasSample a=world_atlas_sample_uv(u,v);
        geometry=number(geometry,a.height);geometry=word(geometry,(uint32_t)a.region);
        geometry=number(geometry,a.shore_distance);geometry=number(geometry,a.land_weight);
        if(ellipse(u,v)>=1||a.region!=WORLD_ATLAS_OLD_TANTHOON||a.shore_distance<=120){
            assert(weight_uv(u,v)==0);excluded=sample_hash(excluded,a);excluded_count++;
        }
    }
    assert(geometry==UINT64_C(0x0f40fed12606cfc2));
    assert(excluded_count==8254&&excluded==UINT64_C(0x777f37ef10d6d62a));
    size_t count=0;const WorldAtlasLandmark *towns=world_atlas_landmarks(&count);assert(count==15);
    uint64_t foundations=UINT64_C(14695981039346656037);
    for(size_t i=0;i<count;i++){
        WorldAtlasSample town=world_atlas_sample_uv(towns[i].u,towns[i].v);
        /* The later, compact Valdoa village intentionally replaces ONLY this
         * foundation with an exact24m terrace and a built-surface tree mask.
         * Keep the original pre-village bits for every other landmark. */
        if(strcmp(towns[i].id,"valdoa")==0)assert(town.height==24&&town.forest_density==0);
        else foundations=sample_hash(foundations,town);
        for(int ring=0;ring<=3;ring++)for(int angle=0;angle<16;angle++){
            double theta=angle*6.283185307179586/16;
            assert(world_atlas_rainforest_weight(towns[i].x+ring*140*cos(theta),
                                                towns[i].z+ring*140*sin(theta))==0);
        }
    }
    assert(foundations==UINT64_C(0x1d28d4f951836b94));
    WorldAtlasSample center=world_atlas_sample_uv(.235,.670);
    assert(center.region==WORLD_ATLAS_OLD_TANTHOON&&fabs(center.height-136.44338989257812)<.0001);
    assert(fabs(center.forest_density-.94)<1e-6&&fabs(center.moisture-.98)<1e-6);
    /* Real pre-overlay climate at a point in the ecotone proves the blend,
     * rather than merely checking that core values increased. */
    double blend=1-smooth(.36,1,.65*.65);
    WorldAtlasSample edge=world_atlas_sample_uv(.27725,.670);
    assert(fabs(edge.forest_density-(.6105098128318787+blend*(.94-.6105098128318787)))<1e-6);
    assert(fabs(edge.moisture-(.8865882158279419+blend*(.98-.8865882158279419)))<1e-6);
    assert(!weight_uv(.170,.670)&&!weight_uv(.300,.670));
    assert(!weight_uv(.235,.585)&&!weight_uv(.235,.755));
    assert(weight_uv(.300-1e-7,.670)<1e-8);
    assert(fabs(weight_uv(.235+.065*.6-1e-8,.670)-weight_uv(.235+.065*.6+1e-8,.670))<1e-6);

    size_t wet=0,dry_bank=0,interior=0;
    float max_mask_step=0;
    for(int iz=0;iz<=160;iz++)for(int ix=0;ix<=200;ix++){
        double u=.170+.130*ix/200.,v=.585+.170*iz/160.;
        WorldAtlasSample a=world_atlas_sample_uv(u,v);float w=weight_uv(u,v);
        assert(isfinite(w)&&w>=0&&w<=1&&w==weight_uv(u,v));
        double expected=a.region==WORLD_ATLAS_OLD_TANTHOON?
            (1-smooth(.36,1,ellipse(u,v)))*smooth(120,360,a.shore_distance):0;
        assert(fabs(w-expected)<2e-6);
        if(w>0){assert(a.region==WORLD_ATLAS_OLD_TANTHOON&&a.shore_distance>120);interior++;}
        if(ellipse(u,v)<1&&a.shore_distance<=0){assert(w==0&&a.forest_density==0);wet++;}
        if(ellipse(u,v)<1&&a.shore_distance>0&&a.shore_distance<=120){assert(w==0);dry_bank++;}
        max_mask_step=fmaxf(max_mask_step,fabsf(w-weight_uv(u+1e-7,v)));
    }
    assert(wet>100&&dry_bank>10&&interior>10000&&max_mask_step<.001);

    const WorldAtlasTrailPoint *path=world_atlas_rainforest_trail_points(&count);assert(count==6);
    double max_grade=0,length=0;size_t stations=0;
    for(size_t i=1;i<count;i++){
        double dx=path[i].x-path[i-1].x,dz=path[i].z-path[i-1].z,segment=hypot(dx,dz);
        double nx=-dz/segment,nz=dx/segment;int steps=(int)ceil(segment/.5);length+=segment;
        for(int step=0;step<=steps;step++){
            double t=(double)step/steps,x=path[i-1].x+dx*t,z=path[i-1].z+dz*t;
            WorldAtlasSample a=world_atlas_sample(x,z);
            assert(a.region==WORLD_ATLAS_OLD_TANTHOON&&a.shore_distance>2000&&a.height>0);
            assert(world_atlas_rainforest_weight(x,z)==1);
            assert(world_atlas_rainforest_trail_mask(x,z)==1);
            assert(world_atlas_landscape_mask(x,z).trail==1);
            assert(world_atlas_landscape_sample(x,z).height==a.height);
            if(step){
                double px=path[i-1].x+dx*(step-1)/steps,pz=path[i-1].z+dz*(step-1)/steps;
                max_grade=fmax(max_grade,fabs(a.height-world_atlas_height(px,pz))/(segment/steps));
            }
            /* Stay away from joins when checking exact perpendicular width. */
            if(t>.2&&t<.8){
                assert(world_atlas_rainforest_trail_mask(x+nx*1.59,z+nz*1.59)>.9999f);
                assert(fabs(world_atlas_rainforest_trail_mask(x+nx*2.2,z+nz*2.2)-.5)<1e-5);
                assert(world_atlas_rainforest_trail_mask(x+nx*2.81,z+nz*2.81)==0);
            }
            stations++;
        }
    }
    assert(length>210&&length<220&&max_grade<.15&&stations>400);
    assert(!world_atlas_rainforest_trail_mask(NAN,0)&&!world_atlas_rainforest_trail_mask(0,INFINITY));
    assert(!world_atlas_rainforest_trail_mask(0,0));
    /* Reinitialization must rebuild identical ecology with no retained state. */
    world_atlas_shutdown();assert(world_atlas_rainforest_weight(path[0].x,path[0].z)==0);
    assert(world_atlas_init());WorldAtlasSample rebuilt=world_atlas_sample_uv(.235,.670);
    assert(memcmp(&rebuilt,&center,sizeof(center))==0);world_atlas_shutdown();
    printf("PASS rainforest: unchanged ABI/regions/14foundations plus intentional Valdoa village; 8385 terrain and8254 excluded climate samples bit-identical; %zu interior/%zu wet/%zu dry-bank mask probes; path %.3fm/%zu stations maxgrade%.5f, smooth width/core/edge and deterministic rebuild\n",
           interior,wet,dry_bank,length,stations,max_grade);
    return 0;
}
