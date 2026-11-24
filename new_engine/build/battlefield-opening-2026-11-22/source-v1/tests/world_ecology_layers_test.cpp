#include "engine/world_cover.h"
#include "engine/world_atlas.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// Real cover generator against a deterministic, flat, dry sampling fixture.
// This isolates layer ownership from unrelated map routes and source assets.
extern "C" {
WorldAtlasSample world_atlas_sample(double,double){return {20,WORLD_ATLAS_VALWOOD,.8f,.6f,1000,1};}
float world_atlas_height(double,double){return 20;}
WorldAtlasLandscapeSample world_atlas_landscape_mask(double,double){return {};}
WorldAtlasStartingSample world_atlas_starting_sample(double,double){return {};}
float world_atlas_rainforest_weight(double,double){return 0;}
float world_atlas_village_weight(double,double){return 0;}
void world_atlas_village_origin(double *x,double *z){*x=*z=100000;}
bool world_atlas_local_water(double,double,float *height,float *depth){if(height)*height=0;if(depth)*depth=0;return false;}
}
namespace {
unsigned checks=0;
void expect(bool ok,const char *message){++checks;if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
using Snapshot=std::array<std::vector<WorldModelInstance>,4>;
Snapshot generate(const std::vector<WorldCoverRegion> &regions,bool authored=true,size_t budget=4096){
    auto *cover=world_cover_init();expect(cover!=nullptr,"cover allocation");
    expect(world_cover_set_regions(cover,regions.data(),regions.size(),authored,nullptr,nullptr),"configure regions");
    expect(world_cover_update(cover,0,0),"start cover generation");
    Snapshot result;unsigned published=0,calls=0;
    for(;;){
        expect(world_cover_pump(cover,budget)<=std::min(size_t(4096),budget),"bounded pump");
        WorldCoverField field{};
        while(world_cover_poll(cover,&field)){
            result[field.kind].assign(field.instances,field.instances+field.count);++published;
        }
        WorldCoverMetrics metrics{};world_cover_metrics(cover,&metrics);
        if(!metrics.near_building&&!metrics.tree_building)break;
        expect(++calls<100000,"generation finishes");
    }
    expect(published==4,"all four real fields published");
    world_cover_shutdown(cover);return result;
}
bool equal(const std::vector<WorldModelInstance> &a,const std::vector<WorldModelInstance> &b){
    return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(a[0]))==0);
}
uint64_t fingerprint(const std::vector<WorldModelInstance> &v){
    uint64_t h=14695981039346656037ull;const auto *p=reinterpret_cast<const unsigned char*>(v.data());
    for(size_t i=0;i<v.size()*sizeof(v[0]);++i){h^=p[i];h*=1099511628211ull;}return h;
}
WorldCoverRegion woodland(){
    WorldCoverRegion r;r.radius=40;r.seed=17;r.preset=1;
    r.density[0]=.6f;r.density[1]=.85f;r.density[2]=.8f;r.density[3]=.9f;return r;
}
void legacy_fingerprints(){
    for(bool authored:{false,true})for(unsigned habitat=0;habitat<3;++habitat){
        auto r=woodland();r.preset=habitat;
        auto s=generate({r},authored);
        std::cout<<"legacy "<<authored<<' '<<habitat;
        for(const auto &field:s)std::cout<<' '<<field.size()<<':'<<std::hex<<fingerprint(field)<<std::dec;
        std::cout<<'\n';
    }
    auto r=woodland(),later=r;later.radius=24;later.x=15;later.preset=2;later.seed=99;
    auto s=generate({r,later},false);std::cout<<"legacy overlap";
    for(const auto &field:s)std::cout<<' '<<field.size()<<':'<<std::hex<<fingerprint(field)<<std::dec;
    std::cout<<'\n';
}
}
int main(int argc,char **argv){
    if(argc==2&&std::string(argv[1])=="--legacy-only"){legacy_fingerprints();return 0;}
    using namespace world_ecology;
    auto wood=woodland();
    expect(layer_mask(wood)==15&&habitat(wood)==1,"legacy encoding selects every layer");
    for(unsigned h=0;h<3;++h)for(unsigned mask=0;mask<16;++mask){
        auto r=wood;r.preset=h|(mask<<8);
        expect(valid(r),"all four-bit masks with known habitat valid");
        expect(habitat(r)==h&&layer_mask(r)==(mask?mask:15),"packed mask and habitat decode");
        for(int kind=0;kind<4;++kind)expect(owns(r,kind)==bool((mask?mask:15)&(1u<<kind)),"individual layer ownership");
    }
    for(unsigned bad:{3u,0xffu,0x103u,0x1000u,0x10000u,0x80000000u,0xffffffffu}){
        auto r=wood;r.preset=bad;expect(!valid(r),"reject unknown habitat or reserved bits");
    }
    expect(!owns(wood,-1)&&!owns(wood,4),"invalid kind owns nothing");
    auto grass=wood;grass.preset=2u|(1u<<8);grass.radius=44;grass.seed=321;grass.density[0]=.95f;
    std::vector<WorldCoverRegion> layers{wood,grass};
    expect(at(layers.data(),2,0,0)==1,"legacy any-region query remains compatible");
    expect(at_layer(layers.data(),2,0,0,WORLD_COVER_GRASS)==1,"new grass paint owns grass");
    for(int kind=1;kind<4;++kind)expect(at_layer(layers.data(),2,0,0,kind)==0,"woodland retains other layers");
    expect(world_cover_region_preset(layers.data(),2,0,0)==1,"grass habitat cannot replace tree species");
    expect(at_layer(&grass,1,0,0,WORLD_COVER_TREES)==-1,"unowned layer preserves baseline");
    expect(at_layer(&grass,1,44,0,WORLD_COVER_GRASS)==-1,"circle boundary remains excluded");
    expect(at_layer(nullptr,1,0,0,0)==-1&&at_layer(layers.data(),33,0,0,0)==-1,"invalid region span rejected");
    expect(at_layer(layers.data(),2,0,0,-1)==-1&&at_layer(layers.data(),2,0,0,4)==-1,"invalid layer query rejected");
    expect(at_layer(layers.data(),2,std::numeric_limits<double>::quiet_NaN(),0,0)==-1,"nonfinite query rejected");
    expect(density(grass,WORLD_COVER_TREES,0,0)==0,"unowned density is zero");

    const auto original=generate({wood});
    for(const auto &v:original)expect(!v.empty(),"woodland fixture generates each layer");
    const auto painted=generate(layers);
    expect(!equal(original[0],painted[0]),"grass paint changes actual grass roots");
    for(unsigned kind=1;kind<4;++kind)expect(equal(original[kind],painted[kind]),"grass over full woodland preserves exact other fields");
    grass.density[0]=0;const auto erased=generate({wood,grass});
    expect(erased[0].empty(),"grass-only erase removes overlapping authored grass");
    for(unsigned kind=1;kind<4;++kind)expect(equal(original[kind],erased[kind]),"grass-only erase preserves trees rocks and ferns");
    auto fern=grass;fern.preset=2u|(8u<<8);fern.seed=79;fern.density[3]=.25f;
    const auto fern_painted=generate({wood,fern},true,137);
    for(unsigned kind=0;kind<3;++kind)expect(equal(original[kind],fern_painted[kind]),"fern-only paint preserves all other fields across pump budgets");
    expect(!equal(original[3],fern_painted[3]),"fern-only paint changes actual fern field");
    auto trees=grass;trees.preset=2u|(2u<<8);trees.density[1]=.75f;
    expect(world_cover_region_preset(&trees,1,0,0)==2,"tree species returns low habitat bits");
    const auto tree_painted=generate({wood,trees});
    for(unsigned kind:{0u,2u,3u})expect(equal(original[kind],tree_painted[kind]),"tree-only paint preserves ground cover");
    expect(!equal(original[1],tree_painted[1]),"tree-only paint changes actual trees");

    // Non-authored fields exercise baseline exclusion and grass-cell counting.
    const auto baseline=generate({},false);
    fern.radius=180;
    const auto baseline_fern=generate({fern},false);
    for(unsigned kind=0;kind<3;++kind)expect(equal(baseline[kind],baseline_fern[kind]),"fern layer leaves exact baseline grass/tree/rock candidates intact");
    expect(!equal(baseline[3],baseline_fern[3]),"fern layer changes baseline ferns");
    grass.radius=180;
    const auto baseline_erase=generate({grass},false);
    expect(baseline_erase[0].empty(),"grass-only erase clears complete baseline grass radius");
    for(unsigned kind=1;kind<4;++kind)expect(equal(baseline[kind],baseline_erase[kind]),"grass-only erase preserves exact baseline other fields");
    for(unsigned h=0;h<3;++h){
        auto legacy=wood;legacy.preset=h;auto explicit_all=legacy;explicit_all.preset|=15u<<8;
        const auto a=generate({legacy}),b=generate({explicit_all});
        for(unsigned kind=0;kind<4;++kind)expect(equal(a[kind],b[kind]),"zero-mask legacy and explicit all-mask generate exact same fields");
    }
    auto *c=world_cover_init();expect(c!=nullptr,"atomic config test allocation");
    expect(world_cover_set_regions(c,&wood,1,true,nullptr,nullptr)&&world_cover_update(c,0,0),"atomic config setup");
    WorldCoverMetrics before{},after{};world_cover_metrics(c,&before);auto invalid=wood;invalid.preset=0x1000;
    expect(!world_cover_set_regions(c,&invalid,1,true,nullptr,nullptr),"invalid layer encoding rejected atomically");
    world_cover_metrics(c,&after);expect(before.near_generation==after.near_generation&&before.tree_generation==after.tree_generation&&after.near_building&&after.tree_building,"invalid input preserves staged generation");
    world_cover_shutdown(c);
    std::cout<<"Layered ecology: "<<checks<<" checks passed\n";
}
