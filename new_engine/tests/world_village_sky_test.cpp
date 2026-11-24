#include "engine/world_village_sky.h"
#include "engine/world_village_indirect.h"
#include <cassert>
#include <cstdio>
#include <limits>

namespace {
std::uint8_t quantize(double access){return std::uint8_t(std::lround(std::clamp(access,.30,1.)*255));}
void rejects(const float *field,std::size_t count,const WorldVillageSkyConfig &config){
    std::vector<std::uint8_t> output={3,7,11};const auto before=output;
    assert(!world_village_sky_build(field,count,config,output));assert(output==before);
}
void directional() {
    WorldVillageSkyConfig grid;grid.columns=grid.rows=129;
    WorldVillageIndirectConfig config;config.columns=config.rows=65;
    std::vector<float> roofs(grid.columns*grid.rows,24.f);
    WorldVillageIndirectField field;
    assert(world_village_indirect_build(roofs.data(),roofs.size(),grid,config,field));
    for(auto value:field.horizontal_rgba)assert(value==255);
    for(auto value:field.up_r)assert(value==255);
    const auto index=[&](std::size_t x,std::size_t z,std::size_t y=0){return (y*config.rows+z)*config.columns+x;};
    // One actual wall5m east,10m above the first height slice. The east-facing
    // hemisphere is obstructed; the west-facing one stays completely open.
    for(std::size_t z=0;z<grid.rows;z++)roofs[z*grid.columns+69]=34.f;
    assert(world_village_indirect_build(roofs.data(),roofs.size(),grid,config,field));
    const std::size_t center=index(32,32);
    const auto horizontal=[](double slope) {
        const double angle=std::atan(slope);
        // Independent elevation integral of cos^2(theta), normalized to its
        // open-sky value pi/4. This is not a copied lookup-table expectation.
        const double pi=std::acos(-1.);
        return (pi/4.-angle/2.-std::sin(2*angle)/4.)/(pi/4.);
    };
    const auto byte=[](double value){return std::uint8_t(std::lround(value*255.));};
    const double diagonal=1/std::sqrt(2.);
    const auto east=byte((horizontal(2.)+2*diagonal*horizontal(std::sqrt(2.)))/(1+2*diagonal));
    assert(field.horizontal_rgba[center*4]==east&&east<35);
    assert(field.horizontal_rgba[center*4+1]==255);
    assert(field.horizontal_rgba[center*4+2]==field.horizontal_rgba[center*4+3]);
    assert(field.up_r[center]==byte((1./5.+2./3.+5.)/8.));
    // The far west plaza cannot see the wall inside any bounded ray.
    for(std::size_t c=0;c<4;c++)assert(field.horizontal_rgba[index(0,32)*4+c]==255);
    for(std::size_t y=1;y<config.slices;y++) {
        assert(field.up_r[index(32,32,y)]>=field.up_r[index(32,32,y-1)]);
        for(std::size_t c=0;c<4;c++)assert(field.horizontal_rgba[index(32,32,y)*4+c]>=field.horizontal_rgba[index(32,32,y-1)*4+c]);
    }
    const auto top=index(32,32,config.slices-1);
    assert(field.up_r[top]==255);
    for(std::size_t c=0;c<4;c++)assert(field.horizontal_rgba[top*4+c]==255);
    // A mirrored wall must swap only east/west lobes at this symmetric point.
    const auto one_wall=field;
    std::fill(roofs.begin(),roofs.end(),24.f);
    for(std::size_t z=0;z<grid.rows;z++)roofs[z*grid.columns+59]=34.f;
    assert(world_village_indirect_build(roofs.data(),roofs.size(),grid,config,field));
    assert(field.horizontal_rgba[center*4]==one_wall.horizontal_rgba[center*4+1]);
    assert(field.horizontal_rgba[center*4+1]==one_wall.horizontal_rgba[center*4]);
    assert(field.up_r[center]==one_wall.up_r[center]);
    // Uniform world scaling preserves directional integration and height
    // clearance exactly, including every byte away from the analytic point.
    for(auto &height:roofs)height*=2;
    grid.cell_width=grid.cell_depth=2;
    auto scaled=config;scaled.first_height*=2;scaled.height_step*=2;
    WorldVillageIndirectField double_scale;
    assert(world_village_indirect_build(roofs.data(),roofs.size(),grid,scaled,double_scale));
    assert(double_scale.horizontal_rgba==field.horizontal_rgba&&double_scale.up_r==field.up_r);
    const auto before=field;
    const auto reject=[&](const float *data,std::size_t count,const WorldVillageSkyConfig &input,const WorldVillageIndirectConfig &cfg) {
        assert(!world_village_indirect_build(data,count,input,cfg,field));
        assert(field.horizontal_rgba==before.horizontal_rgba&&field.up_r==before.up_r);
        assert(field.config.columns==before.config.columns&&field.config.first_height==before.config.first_height);
    };
    reject(nullptr,roofs.size(),grid,scaled);reject(roofs.data(),roofs.size()-1,grid,scaled);
    for(float invalid:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        auto bad=scaled;bad.height_step=invalid;reject(roofs.data(),roofs.size(),grid,bad);
    }
    for(std::size_t invalid:{std::size_t(0),std::size_t(1),std::size_t(34)}) {
        auto bad=scaled;bad.slices=invalid;reject(roofs.data(),roofs.size(),grid,bad);
    }
    auto bad=scaled;bad.columns=64;reject(roofs.data(),roofs.size(),grid,bad); // nondividing nodes
    bad=scaled;bad.slices=2;reject(roofs.data(),roofs.size(),grid,bad); // volume fails to clear roofs
    roofs.back()=std::numeric_limits<float>::quiet_NaN();reject(roofs.data(),roofs.size(),grid,scaled);
    std::printf("PASS directional village sky: analytic east-wall lobe%u/255 vs open west255, mirrored lobes, true height clearance, exterior open, scale exactness and transactional invalid rejection\n",unsigned(east));
}
}
int main(){
    WorldVillageSkyConfig config;
    std::vector<float> roofs(config.columns*config.rows,24.f);
    std::vector<std::uint8_t> output;
    assert(world_village_sky_build(roofs.data(),roofs.size(),config,output));
    assert(output.size()==roofs.size());for(auto value:output)assert(value==255);
    constexpr std::size_t center=256;
    auto index=[&](std::size_t x,std::size_t z){return z*config.columns+x;};
    auto corridor=[&](float height){
        std::fill(roofs.begin(),roofs.end(),24.f);
        for(std::size_t z=0;z<config.rows;z++)roofs[index(center-5,z)]=roofs[index(center+5,z)]=height;
    };
    corridor(config.reference_y+10.f);
    assert(world_village_sky_build(roofs.data(),roofs.size(),config,output));
    // Two cardinal horizons at slope2, four diagonal horizons at sqrt2,
    // two open directions. This independent analytic integral is119/255.
    const auto expected=quantize((2./5.+4./3.+2.)/8.);
    assert(expected==119&&output[index(center,center)]==expected);
    for(std::size_t delta=0;delta<35;delta++)
        assert(output[index(center-delta,center)]==output[index(center+delta,center)]);
    // The plaza is95cells beyond the nearest wall: none of the bounded
    // horizon rays can intersect it, so it retains full open-sky access.
    assert(output[index(center+100,center)]==255);
    const auto square=output;

    config.cell_width=2;
    assert(world_village_sky_build(roofs.data(),roofs.size(),config,output));
    const auto rectangular=quantize((2./2.+4./1.8+2.)/8.);
    assert(rectangular==166&&output[index(center,center)]==rectangular);
    assert(output[index(center,center)]>square[index(center,center)]);
    config.cell_width=config.cell_depth=2;
    corridor(config.reference_y+20.f);
    assert(world_village_sky_build(roofs.data(),roofs.size(),config,output));
    assert(output==square); // Uniform world scaling preserves every horizon.

    config.cell_width=config.cell_depth=1;
    corridor(config.reference_y+30.f);
    assert(world_village_sky_build(roofs.data(),roofs.size(),config,output));
    assert(output[index(center,center)]==77);
    for(auto value:output)assert(value>=77);
    // A field below the observer never creates a negative horizon/false AO.
    std::fill(roofs.begin(),roofs.end(),-1000.f);
    assert(world_village_sky_build(roofs.data(),roofs.size(),config,output));
    for(auto value:output)assert(value==255);

    rejects(nullptr,roofs.size(),config);rejects(roofs.data(),roofs.size()-1,config);
    for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        roofs.back()=bad;rejects(roofs.data(),roofs.size(),config);roofs.back()=24;
        auto invalid=config;invalid.reference_y=bad;rejects(roofs.data(),roofs.size(),invalid);
    }
    for(double bad:{0.,-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        auto invalid=config;invalid.cell_width=bad;rejects(roofs.data(),roofs.size(),invalid);
        invalid=config;invalid.cell_depth=bad;rejects(roofs.data(),roofs.size(),invalid);
    }
    for(std::size_t bad:{std::size_t(0),std::size_t(1),std::size_t(4098),std::numeric_limits<std::size_t>::max()}) {
        auto invalid=config;invalid.columns=bad;rejects(roofs.data(),roofs.size(),invalid);
        invalid=config;invalid.rows=bad;rejects(roofs.data(),roofs.size(),invalid);
    }
    // Inputs at the floating-point spacing extremes must not overflow the
    // precomputed ray distances into an apparently valid open-sky result.
    auto invalid=config;invalid.cell_width=std::numeric_limits<double>::max();rejects(roofs.data(),roofs.size(),invalid);
    invalid=config;invalid.cell_depth=std::numeric_limits<double>::denorm_min();rejects(roofs.data(),roofs.size(),invalid);
    std::puts("PASS village sky:513x513 openfield255; analytical corridor119/255, rectangular166/255, uniform-scale exactness, symmetric walls, open plaza, minimum77/255; invalid/nonfinite transactional rejection");
    directional();
}
