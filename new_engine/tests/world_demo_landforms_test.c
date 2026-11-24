#include "engine/world_demo_contours.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    double high=0,step=0;unsigned rocks=0;
    for(int iz=-400;iz<=250;iz++)for(int ix=-190;ix<=190;ix++){
        double x=ix*.5,z=iz*.5,h=world_demo_contour_delta(x,z),r=world_demo_rock_exposure(x,z);
        assert(isfinite(h)&&isfinite(r)&&h>=-.66&&h<20&&r>=0&&r<=1);
        high=fmax(high,h);rocks+=r>.5;
        step=fmax(step,fabs(h-world_demo_contour_delta(x+.001,z)));
        step=fmax(step,fabs(h-world_demo_contour_delta(x,z+.001)));
        if(world_demo_route_distance(x,z)<=3.6)assert(h==0&&r==0);
        if(fabs(x-12)<=22&&fabs(z-30)<=17)assert(h==0&&r==0);
        if(hypot((x+22)/10.,(z+25)/16.)<=1.15)assert(h==0&&r==0);
    }
    for(int animal=0;animal<2;animal++)for(int z=-2;z<=2;z++)for(int x=-2;x<=2;x++){
        double px=(animal?-74:-68)+x*.25,pz=(animal?-32:-28)+z*.425;
        assert(world_demo_contour_delta(px,pz)==0&&world_demo_rock_exposure(px,pz)==0);
    }
    assert(high>12&&rocks>250&&step<.005);
    assert(world_demo_contour_delta(NAN,0)==0&&world_demo_contour_delta(0,INFINITY)==0);
    assert(world_demo_rock_exposure(NAN,0)==0&&world_demo_rock_exposure(1000,1000)==0);
    printf("PASS landforms:248031 support probes;route/camp/pool protected;bounded continuous geometry (max %.3fm,1mm step %.6fm);rock-mask support;invalid coordinates\n",high,step);
}
