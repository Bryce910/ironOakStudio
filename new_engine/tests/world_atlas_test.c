#include "engine/world_atlas.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static WorldAtlasSample at(double u,double v){
    WorldAtlasSample a=world_atlas_sample_uv(u,v);
    assert(isfinite(a.height)&&isfinite(a.shore_distance));
    assert(a.region>=0&&a.region<WORLD_ATLAS_REGION_COUNT);
    assert(a.forest_density>=0&&a.forest_density<=1);
    assert(a.moisture>=0&&a.moisture<=1);
    assert(a.land_weight>=0&&a.land_weight<=1);
    return a;
}
static double eastern_lake_u(double v,double distance){
    double lo=.451,hi=.462;
    for(int step=0;step<45;step++){
        double mid=(lo+hi)*.5;
        if(world_atlas_sample_uv(mid,v).shore_distance<distance)lo=mid;else hi=mid;
    }
    return (lo+hi)*.5;
}
int main(void){
    assert(fabs(world_atlas_width()/world_atlas_walk_speed()-8*3600)<1e-9);
    assert(fabs(world_atlas_depth()/world_atlas_width()-2818.0/5009.0)<1e-12);
    assert(world_atlas_init());assert(world_atlas_init());
    uint64_t classification=UINT64_C(14695981039346656037),effective_classification=classification;
    size_t preserved_core=0;
    for(int z=0;z<=128;z++)for(int x=0;x<=256;x++){
        double u=x/256.0,v=z/128.0,wx,wz,uu,vv;
        world_atlas_uv_to_world(u,v,&wx,&wz);world_atlas_world_to_uv(wx,wz,&uu,&vv);
        assert(fabs(u-uu)<1e-12&&fabs(v-vv)<1e-12);
        WorldAtlasSample a=at(u,v),b=world_atlas_sample(wx,wz);
        assert(fabs(a.height-b.height)<.0001);
        assert(a.height>-100&&a.height<3000);
        float reference=world_atlas_reference_shore_distance(wx,wz);
        // World/UV round trips can turn an exact zero contour point into
        // ~1e-12m; canonicalize only this diagnostic boundary ambiguity.
        classification^=reference>1e-6;classification*=UINT64_C(1099511628211);
        effective_classification^=a.shore_distance>0;effective_classification*=UINT64_C(1099511628211);
        assert(fabs(a.shore_distance-reference)<=8.001);
        if(fabs(reference)>8){assert((a.shore_distance>0)==(reference>0));preserved_core++;}
        WorldAtlasSample coast=at((x+.37)/257.0,(z+.61)/129.0);
        if(coast.shore_distance<-.01)assert(coast.height<=0&&coast.forest_density==0);
        if(coast.shore_distance>.01)assert(coast.height>0);
    }
    // Preserve the ORIGINAL map checksum, not a regenerated effective-shore
    // golden. Only the approved local band within8m may change classification;
    // all33153 reference points still retain their original macro geography.
    assert(classification==UINT64_C(0x08b1e07e55b80fe2));
    // This coarse control grid also keeps its original exact effective golden;
    // the dense local checks below prove that real sub-grid coves were added.
    assert(effective_classification==UINT64_C(0x08b1e07e55b80fe2));
    assert(preserved_core>33000);
    WorldAtlasSample mountain=at(.19,.082),forest=at(.52,.24),wallows=at(.87,.18),plains=at(.80,.56);
    assert(mountain.height>1000&&mountain.region==WORLD_ATLAS_MASHITO);
    assert(forest.region==WORLD_ATLAS_VALWOOD&&forest.forest_density>.3);
    assert(wallows.region==WORLD_ATLAS_WALLOWS&&wallows.height>350);
    assert(plains.region==WORLD_ATLAS_KERONE&&plains.height>0);
    const double water[][2]={{.410,.170},{.170,.120},{.400,.400},{.500,.980},{.580,.130}};
    for(size_t i=0;i<sizeof(water)/sizeof(water[0]);i++){
        WorldAtlasSample a=at(water[i][0],water[i][1]);
        assert(a.height<0&&a.region==WORLD_ATLAS_OCEAN&&a.shore_distance<0);
    }
    // Compare like-for-like bank locations at the same signed distance from
    // water. An old uniform ramp has less than0.9m variation at this distance;
    // the authored shelves/runnels must retain visible lateral variation.
    double bank_min=1e9,bank_max=-1e9;
    for(int row=0;row<=12;row++){
        double v=.171+row*.0015,u=eastern_lake_u(v,30);
        WorldAtlasSample bank=at(u,v);
        assert(fabs(bank.shore_distance-30)<.001&&bank.height>.05);
        bank_min=fmin(bank_min,bank.height);bank_max=fmax(bank_max,bank.height);
        double edge=eastern_lake_u(v,0);
        assert(at(eastern_lake_u(v,-5),v).height<0);
        assert(at(eastern_lake_u(v,.1),v).height>0);
        assert(fabs(at(edge-1e-9,v).height-at(edge+1e-9,v).height)<.01);
    }
    assert(bank_max-bank_min>1.5&&bank_max<4);
    double lake_x,lake_z;world_atlas_uv_to_world(.454,.180,&lake_x,&lake_z);
    double entrance_height=world_atlas_height(lake_x,lake_z),previous_height=entrance_height,max_grade=0;
    assert(fabs(entrance_height-4.054718)<.001);
    assert(fabs(world_atlas_sample(lake_x,lake_z).shore_distance-72.878387)<.001);
    for(int distance=1;distance<=80;distance++){
        double height=world_atlas_height(lake_x-sin(.90)*distance,lake_z-cos(.90)*distance);
        max_grade=fmax(max_grade,fabs(height-previous_height));previous_height=height;
    }
    assert(max_grade<.25);
    WorldAtlasSample close_shore=at(.4534948785,.1792875066);
    assert(close_shore.shore_distance>15&&close_shore.shore_distance<25&&close_shore.height>0);
    double close_x,close_z;world_atlas_uv_to_world(.4534948785,.1792875066,&close_x,&close_z);
    assert(world_atlas_reference_shore_distance(close_x,close_z)>8);
    static const double shore_bounds[][4]={{.4485,.168,.4635,.192},{.510,.914,.540,.938}};
    double offset_min=0,offset_max=0;
    size_t changed_edge=0,checked_shore=0;
    for(size_t patch=0;patch<sizeof(shore_bounds)/sizeof(shore_bounds[0]);patch++){
        const double *b=shore_bounds[patch];
        for(int iz=0;iz<=128;iz++)for(int ix=0;ix<=128;ix++){
            double u=b[0]+(b[2]-b[0])*ix/128.,v=b[1]+(b[3]-b[1])*iz/128.,x,z;
            world_atlas_uv_to_world(u,v,&x,&z);
            WorldAtlasSample actual=at(u,v);
            double reference=world_atlas_reference_shore_distance(x,z),offset=actual.shore_distance-reference;
            assert(fabs(offset)<=8.001);
            if(fabs(reference)>8)assert((actual.shore_distance>0)==(reference>0));
            if((actual.shore_distance>0)!=(reference>0)){assert(fabs(reference)<=8);changed_edge++;}
            if(actual.shore_distance<=0){
                assert(fabs(actual.height+fmin(85,-actual.shore_distance*.07))<.0001);
                assert(actual.region==WORLD_ATLAS_OCEAN&&actual.forest_density==0);
            }else assert(actual.height>0&&actual.region!=WORLD_ATLAS_OCEAN);
            offset_min=fmin(offset_min,offset);offset_max=fmax(offset_max,offset);checked_shore++;
        }
        for(int side=0;side<4;side++)for(int i=0;i<=8;i++){
            double u=side<2?b[side*2]:b[0]+(b[2]-b[0])*i/8.;
            double v=side<2?b[1]+(b[3]-b[1])*i/8.:b[1+(side-2)*2];
            double du=side<2?1e-9:0,dv=side<2?0:1e-9;
            assert(fabs(at(u-du,v-dv).height-at(u+du,v+dv).height)<.01);
            assert(fabs(at(u-du,v-dv).shore_distance-at(u+du,v+dv).shore_distance)<.01);
        }
        assert(isfinite(at(nextafter(b[2],b[0]),nextafter(b[3],b[1])).height));
    }
    assert(offset_min<-2&&offset_max>2&&changed_edge>0);
    printf("Local shoreline offsets: %.3f to %.3fm; %zu local sign changes inside8m across%zu patch samples; %zu global core signs preserved.\n",offset_min,offset_max,changed_edge,checked_shore,preserved_core);
    printf("Natural shore banks: equal-distance relief %.3f–%.3fm, preserved4.055m entrance, max1m approach grade %.3f; original reference-map classification golden unchanged.\n",bank_min,bank_max,max_grade);
    size_t count;const WorldAtlasLandmark *marks=world_atlas_landmarks(&count);assert(count==15);
    bool found=false;
    for(size_t i=0;i<count;i++){
        WorldAtlasSample a=at(marks[i].u,marks[i].v);
        assert(a.height>1&&a.shore_distance>25&&a.region!=WORLD_ATLAS_OCEAN);
        assert(marks[i].id[0]&&marks[i].name[0]);
        if(!strcmp(marks[i].id,"seldonne")){found=true;assert(a.height>4&&a.height<15);}
        for(size_t j=0;j<i;j++)assert(strcmp(marks[i].id,marks[j].id));
    }
    assert(found);
    // The new vista terrain must not move the twelve authored town foundations.
    static const struct {const char *id;double height;} fixed_pads[]={
        {"ulasse",16},{"valdoa",24},{"baldurne",230},{"ishome",12},
        {"seldonne",8},{"sclay",12},{"estuay",6},{"kerone_city",72},
        {"etarma",88},{"fallaway",45},{"bellweather",16},{"gorshett_keep",650}
    };
    size_t fixed_found=0;
    for(size_t i=0;i<count;i++)for(size_t j=0;j<sizeof(fixed_pads)/sizeof(fixed_pads[0]);j++){
        if(!strcmp(marks[i].id,fixed_pads[j].id)){
            assert(fabs(world_atlas_height(marks[i].x,marks[i].z)-fixed_pads[j].height)<2);
            fixed_found++;
        }
    }
    assert(fixed_found==12);
    // Authored fields preserve full-map coordinates and return the same surface
    // used by physics/streaming. Their boundaries blend to the global terrain.
    size_t district_count=0;
    const WorldAtlasDistrict *districts=world_atlas_districts(&district_count);
    assert(district_count==3);
    for(size_t district=0;district<district_count;district++){
        const WorldAtlasDistrict *d=&districts[district];
        assert(d->id[0]&&d->name[0]&&d->width>1000&&d->depth>1000);
        for(int z=0;z<=32;z++)for(int x=0;x<=32;x++){
            double wx=d->xmin+d->width*x/32,wz=d->zmin+d->depth*z/32;
            WorldAtlasLandscapeSample l=world_atlas_landscape_sample(wx,wz);
            assert(l.height==world_atlas_height(wx,wz));
            assert(isfinite(l.height)&&l.height>-100&&l.height<3000);
            assert(l.trail>=0&&l.trail<=1&&l.rock>=0&&l.rock<=1);
            assert(l.meadow>=0&&l.meadow<=1&&l.moss>=0&&l.moss<=1);
        }
        for(int edge=0;edge<4;edge++)for(int i=1;i<16;i++){
            double wx=d->xmin+(edge<2?edge:i/16.0)*d->width;
            double wz=d->zmin+(edge<2?i/16.0:edge-2)*d->depth;
            WorldAtlasLandscapeSample l=world_atlas_landscape_sample(wx,wz);
            assert(l.rock<.0001&&l.meadow<.0001&&l.moss<.0001);
            double dx=edge<2?.001:0,dz=edge<2?0:.001;
            assert(fabs(world_atlas_height(wx-dx,wz-dz)-world_atlas_height(wx+dx,wz+dz))<.05);
        }
        double just_inside_x=nextafter(d->xmin+d->width,d->xmin);
        double just_inside_z=nextafter(d->zmin+d->depth,d->zmin);
        WorldAtlasLandscapeSample corner=world_atlas_landscape_sample(just_inside_x,just_inside_z);
        assert(isfinite(corner.height)&&corner.rock<.0001&&corner.meadow<.0001&&corner.moss<.0001);
    }
    WorldAtlasLandscapeSample outside=world_atlas_landscape_sample(35000,0);
    assert(outside.trail==0&&outside.rock==0&&outside.meadow==0&&outside.moss==0);
    assert(world_atlas_landscape_sample(NAN,0).height<0);
    assert(world_atlas_landscape_sample(INFINITY,0).trail==0);

    // A narrow analytic path must retain its clear 3–4 m surface despite the
    // 9 m local height grid. Check all segments, their off-path shoulders and
    // every meter of the actual continuous walking surface, not a copied field.
    size_t trail_count=0;const WorldAtlasTrailPoint *trail=world_atlas_trail(&trail_count);
    assert(trail_count==9);
    double trail_length=0,trail_max_slope=0;
    for(size_t segment=1;segment<trail_count;segment++){
        const WorldAtlasTrailPoint *a=&trail[segment-1],*b=&trail[segment];
        double dx=b->x-a->x,dz=b->z-a->z,length=hypot(dx,dz),nx=-dz/length,nz=dx/length;
        assert(a->half_width>=1.5&&a->half_width<=2&&length>100);
        int steps=(int)ceil(length);double previous=world_atlas_height(a->x,a->z);
        for(int step=1;step<=steps;step++){
            double t=(double)step/steps,x=a->x+dx*t,z=a->z+dz*t;
            WorldAtlasLandscapeSample l=world_atlas_landscape_sample(x,z);
            assert(l.trail>.999&&l.height>0);
            trail_max_slope=fmax(trail_max_slope,fabs(l.height-previous)/(length/steps));previous=l.height;
        }
        double mx=(a->x+b->x)*.5,mz=(a->z+b->z)*.5,width=(a->half_width+b->half_width)*.5;
        assert(world_atlas_landscape_sample(mx+nx*(width-.05),mz+nz*(width-.05)).trail>.99);
        assert(world_atlas_landscape_sample(mx+nx*(width+.70),mz+nz*(width+.70)).trail<.01);
        trail_length+=length;
    }
    assert(trail_length>2000&&trail_length<2600&&trail_max_slope<.25);
    double entrance_x,entrance_z;world_atlas_uv_to_world(.511,.252,&entrance_x,&entrance_z);
    WorldAtlasLandscapeSample clearing=world_atlas_landscape_sample(entrance_x,entrance_z);
    assert(clearing.trail>.99&&clearing.meadow>.9&&world_atlas_sample(entrance_x,entrance_z).forest_density<.05);
    // The lake backdrop includes an incised channel and a separate projecting
    // buttress. Check opposing local relief against their real neighboring
    // elevations, rather than merely requiring globally high smooth hills.
    double gully_x,gully_z,buttress_x,buttress_z;
    world_atlas_uv_to_world(.387,.115,&gully_x,&gully_z);
    world_atlas_uv_to_world(.374,.108,&buttress_x,&buttress_z);
    WorldAtlasLandscapeSample gully=world_atlas_landscape_sample(gully_x,gully_z);
    WorldAtlasLandscapeSample buttress=world_atlas_landscape_sample(buttress_x,buttress_z);
    double gully_shoulders=(world_atlas_height(gully_x-350,gully_z)+world_atlas_height(gully_x+350,gully_z))*.5;
    double buttress_shoulders=(world_atlas_height(buttress_x-350,buttress_z)+world_atlas_height(buttress_x+350,buttress_z))*.5;
    assert(gully.height>10&&gully_shoulders>gully.height+100&&gully.rock>.5);
    assert(buttress.height>buttress_shoulders+100&&buttress.rock>.8);
    assert(at(.351875,.059219).height>2500);
    printf("Authored districts: 3 bounded fields; %.1fm analytic trail, maximum actual 1m grade %.3f; clear on-foot entrance.\n",trail_length,trail_max_slope);
    // The lake has a high, distinct alpine backdrop while Valdoa's surrounding
    // forest gains middle-distance silhouettes. Water and town valleys stay low.
    WorldAtlasSample crown=at(.353,.066),shoulder=at(.402,.087),forest_ridge=at(.485,.260);
    assert(crown.height>2000&&shoulder.height>1500&&crown.height>shoulder.height+200);
    assert(forest_ridge.height>250&&forest_ridge.forest_density>.3);
    assert(at(.44,.18).height<0);
    assert(forest_ridge.height>at(.487,.320).height+150);
    // A connected middle summit and two lower saddles distinguish the shaped
    // headwall from two isolated radial cones. Check relief, not exact maxima.
    double middle_summit=at(.380,.070).height;
    double west_saddle=at(.366,.076).height,east_saddle=at(.391,.084).height;
    assert(middle_summit>1800&&middle_summit>west_saddle+100&&middle_summit>east_saddle+100);
    printf("Ulasse headwall: middle summit %.1fm, saddles %.1fm / %.1fm\n",middle_summit,west_saddle,east_saddle);
    // Northwest foundations must follow the surrounding mountain, rather than
    // digging an arbitrary low bowl. The 480–640 m ring lies beyond the small
    // flat pad and checks its surrounding natural settlement grade and baseline.
    for(size_t i=0;i<3;i++){
        double low=1e9,high=-1e9,sum=0,max_slope=0;
        for(int direction=0;direction<32;direction++){
            double angle=direction*6.283185307179586/32,dx=cos(angle),dz=sin(angle);
            double previous=world_atlas_height(marks[i].x+dx*90,marks[i].z+dz*90);
            for(int radius=100;radius<=420;radius+=10){
                double h=world_atlas_height(marks[i].x+dx*radius,marks[i].z+dz*radius);
                max_slope=fmax(max_slope,fabs(h-previous)/10);previous=h;
            }
            for(int radius=480;radius<=640;radius+=80){
                double h=world_atlas_height(marks[i].x+dx*radius,marks[i].z+dz*radius);
                low=fmin(low,h);high=fmax(high,h);sum+=h;
            }
        }
        double center=world_atlas_height(marks[i].x,marks[i].z),baseline=sum/96;
        assert(fabs(center-baseline)<fmax(35,(high-low)*.6));
        assert(max_slope<.65);
        printf("%s: natural baseline %.1fm, foundation %.1fm, max pad slope %.3f\n",marks[i].name,baseline,center,max_slope);
    }
    // Both sides of macro-grid boundaries and future stream-tile boundaries
    // sample the same continuous world function, including negative coordinates.
    for(int i=1;i<120;i++){
        double x=-49000+i*800.0,z=-23000+(i%43)*1000.0;
        float left=world_atlas_height(x-.001,z),right=world_atlas_height(x+.001,z);
        assert(fabs(left-right)<.05);
        double u=i/128.0,v=(i%67)/67.0;
        assert(fabs(at(u-1e-9,v).height-at(u+1e-9,v).height)<.01);
    }
    assert(world_atlas_sample(NAN,0).region==WORLD_ATLAS_OCEAN);
    assert(world_atlas_sample(INFINITY,0).height<0);
    assert(world_atlas_sample_uv(-.01,.5).height<0);
    float repeat=at(.44,.57).height;
    double repeat_shore_u=eastern_lake_u(.174,30);float repeat_shore=at(repeat_shore_u,.174).height;
    world_atlas_shutdown();world_atlas_shutdown();assert(world_atlas_init());
    assert(at(.44,.57).height==repeat&&at(repeat_shore_u,.174).height==repeat_shore);world_atlas_shutdown();
    printf("World atlas: geography, 15 dry landmarks, finite bounds, coordinate round trips, continuous seams and exact 8-hour %.1f km width passed.\n",world_atlas_width()/1000);
    return 0;
}
