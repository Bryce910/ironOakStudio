#include "engine/world_studio.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
int main(int argc,char **argv){
    if(argc!=2||!world_atlas_init())return 2;
    world_studio::Document d;std::string error;
    if(!world_studio::load(argv[1],d,error)||!world_studio::valid(d,error)){
        std::fprintf(stderr,"%s\n",error.c_str());return 1;
    }
    constexpr double ox=-43344.,oz=(.077-.5)*100800.*2818./5009.;
    const double floor=world_atlas_base_height(ox,oz);
    auto height=[&](double x,double z){return world_atlas_base_height(ox+x,oz+z)+world_studio::height_delta(d,ox+x,oz+z);};
    double hall_error=0,path_slope=0,bank_slope=0,bank_min=1e9,bank_max=-1e9,ice_error=0;
    unsigned raised_samples=0;
    for(double z=-119;z<=10;z+=1.7)for(double x=-31;x<=31;x+=1.7)
        hall_error=std::max(hall_error,std::abs(height(x,z)-floor));
    for(double z=16;z<=130;z+=.5)for(double x=-2;x<=2;x+=.5)
        path_slope=std::max(path_slope,std::hypot(height(x+.25,z)-height(x-.25,z),height(x,z+.25)-height(x,z-.25))/.5);
    for(double z=28;z<=36;z+=.5)for(double x=59;x<=67;x+=.5)
        ice_error=std::max(ice_error,std::abs(height(x,z)-floor));
    for(double z=22;z<=240;z+=1)for(double x=-130;x<=130;x+=1){
        const double h=height(x,z)-floor;
        bank_min=std::min(bank_min,h);bank_max=std::max(bank_max,h);
        raised_samples+=h>.55;
        bank_slope=std::max(bank_slope,std::hypot(height(x+.25,z)-height(x-.25,z),height(x,z+.25)-height(x,z-.25))/.5);
    }
    const bool ok=d.terrain.size()==15&&hall_error<.003&&ice_error<.025&&path_slope<.12&&bank_slope<1.2&&bank_max>3&&raised_samples>4000;
    std::printf("{\"passed\":%s,\"tiles\":%zu,\"hall_max_error_m\":%.6f,\"basin_max_error_m\":%.6f,\"path_max_slope\":%.6f,\"exploration_max_slope\":%.6f,\"snow_height_min_m\":%.4f,\"snow_height_max_m\":%.4f,\"raised_square_metres\":%u}\n",ok?"true":"false",d.terrain.size(),hall_error,ice_error,path_slope,bank_slope,bank_min,bank_max,raised_samples);
    return ok?0:1;
}
