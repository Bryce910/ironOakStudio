#include "engine/world_castle_creek.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    size_t excavated=0,wet=0,bridge_wet=0,courtyard=0,envelopes=0;
    double deepest=0,max_neighbor=0;
    const size_t count=sizeof(world_castle_creek_points)/sizeof(world_castle_creek_points[0]);
    assert(count==22&&count<=32);
    assert(world_castle_creek_points[0].x==world_castle_creek_points[count-1].x);
    assert(world_castle_creek_points[0].z==world_castle_creek_points[count-1].z);
    /* Independent shader-envelope proof: each segment expanded by its MAXIMUM
     * radius has one whole axis outside the outer town's enclosed rectangle.
     * Therefore nearest-segment q-squared<1 cannot shade any courtyard. */
    for(size_t i=1;i<count;i++) {
        const WorldCastleCreekPoint *a=&world_castle_creek_points[i-1],*b=&world_castle_creek_points[i];
        double radius=fmax(a->half_width,b->half_width);
        assert(fmin(a->x,b->x)-radius>205||fmax(a->x,b->x)+radius< -205||
               fmin(a->z,b->z)-radius>205||fmax(a->z,b->z)+radius< -205);
        assert(fmin(a->x-a->half_width,b->x-b->half_width)>=-225&&fmax(a->x+a->half_width,b->x+b->half_width)<=225);
        assert(fmin(a->z-a->half_width,b->z-b->half_width)>=-225&&fmax(a->z+a->half_width,b->z+b->half_width)<=225);
        envelopes++;
    }
    for(int iz=0;iz<=896;iz++)for(int ix=0;ix<=904;ix++) {
        double x=-226+ix*.5,z=-224+iz*.5;
        WorldCastleCreekSample a=world_castle_creek_sample_local(x,z,24);
        WorldCastleCreekSample b=world_castle_creek_sample_local(x+.001,z,24);
        assert(isfinite(a.height)&&a.height>=21.54999&&a.height<=24);
        assert(a.height_delta<=0&&a.bank>=0&&a.bank<=1);
        assert(fabs(a.height-(24+a.height_delta))<1e-12);
        assert(a.depth==fmax(0,WORLD_CASTLE_CREEK_SURFACE_Y-a.height));
        assert(a.submerged==(a.depth>.005));
        if(fabs(x)<=205&&fabs(z)<=205){assert(a.height==24&&!a.submerged);courtyard++;}
        if(a.height<24){assert(fabs(x)>205||fabs(z)>205);excavated++;}
        if(a.submerged){wet++;deepest=fmax(deepest,a.depth);}
        max_neighbor=fmax(max_neighbor,fabs(a.height-b.height));
        if(world_castle_creek_bridge_local(x,z)&&a.submerged) {
            assert(world_castle_creek_floor_local(x,z,a.height)==WORLD_CASTLE_CREEK_DECK_Y);bridge_wet++;
        }
    }
    assert(excavated>30000&&wet>15000&&bridge_wet>100&&courtyard>670000);
    assert(deepest>1.3&&max_neighbor<.015);
    for(int sx=-1;sx<=1;sx+=2)for(int sz=-1;sz<=1;sz+=2)
        for(int radius=0;radius<=32;radius++)for(int i=0;i<360;i++) {
            double angle=i*6.283185307179586/360,r=10.6*radius/32;
            WorldCastleCreekSample a=world_castle_creek_sample_local(sx*205+r*cos(angle),sz*205+r*sin(angle),24);
            assert(a.height==24&&!a.submerged);
        }
    WorldCastleCreekSample under=world_castle_creek_sample_local(0,219,24);
    assert(under.submerged&&under.height<22&&under.depth>1.3);
    assert(world_castle_creek_floor_local(0,219,under.height)==24.02);
    assert(world_castle_creek_sample_local(0,225,24).height==24);
    assert(world_castle_creek_sample_local(0,-46,24).height==24);
    assert(world_castle_creek_sample_local(80,-125,24).height==24);
    assert(world_castle_creek_floor_local(7,219,under.height)==under.height);
    assert(world_castle_creek_sample_local(NAN,219,24).height==24);
    assert(!world_castle_creek_bridge_local(NAN,219));
    assert(!world_castle_creek_contains_local(INFINITY,219));
    assert(world_castle_creek_sample_local(0,0,123.25).height==123.25);
    printf("PASS exterior closed creek: %zu segment envelopes outside town; %zu enclosure samples completely dry; %zu excavated/%zu submerged samples; %zu bridge-water samples; deepest%.3fm; max1mm height delta%.6fm; four outer tower footings dry.\n",envelopes,courtyard,excavated,wet,bridge_wet,deepest,max_neighbor);
}
