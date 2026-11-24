#include "engine/world_texture_pool_policy.h"
#include "engine/world_graphics_settings.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace pool=world_texture_pool_policy;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
struct Query {
    int dedicated=-1,available=-1;std::array<int,4> ati{{-1,-1,-1,-1}};
    std::vector<unsigned> calls;
    void operator()(unsigned token,int *out){
        calls.push_back(token);
        if(token==pool::nvx_dedicated)*out=dedicated;
        else if(token==pool::nvx_available)*out=available;
        else if(token==pool::ati_texture_free){for(unsigned i=0;i<4;++i)out[i]=ati[i];}
        else throw std::runtime_error("Unexpected driver query");
    }
    auto run(bool nvx,bool ati_supported){return pool::query(nvx,ati_supported,[&](unsigned t,int*p){(*this)(t,p);});}
};
int main(){try{
    const std::uint64_t geometry=1280*pool::mib,registered=512*pool::mib;
    const std::array<WorldEnvironmentTextures,4> tiers{{WorldEnvironmentTextures::Low,WorldEnvironmentTextures::Medium,WorldEnvironmentTextures::High,WorldEnvironmentTextures::Ultra}};
    const std::array<std::uint64_t,4> legacy{{1024*pool::mib,1280*pool::mib,1536*pool::mib,2048*pool::mib}};
    const auto graphics=world_graphics_settings_default();
    CHECK(graphics.environment_textures==WorldEnvironmentTextures::High);
    for(unsigned tier=0;tier<4;++tier){
        const auto fallback=world_environment_texture_budget_bytes(tiers[tier]);CHECK(fallback==legacy[tier]);
        CHECK(world_environment_texture_edge(tiers[tier])==(512u<<tier));
        for(auto source:{pool::Source::Unknown,pool::Source::Nvx,pool::Source::Ati}){
            for(std::uint64_t free:{std::uint64_t(0),std::uint64_t(1),512*pool::mib,2*pool::gib,3*pool::gib,4*pool::gib}){
                const pool::Hints hint{source,free,source==pool::Source::Nvx?4*pool::gib:0};
                const auto result=pool::resolve(fallback,hint,registered,geometry);
                CHECK(result.bytes==fallback&&!result.grew);
            }
        }
        // Actual3GB capacity can never take the ample-headroom branch.
        for(std::uint64_t free=0;free<=3*pool::gib;free+=37*pool::mib){
            const auto result=pool::resolve(fallback,{pool::Source::Nvx,free,3*pool::gib},registered,geometry);
            CHECK(result.bytes==fallback&&!result.grew);
        }
        for(std::uint64_t free:{5*pool::gib,8*pool::gib,12*pool::gib,24*pool::gib,128*pool::gib}){
            for(auto source:{pool::Source::Nvx,pool::Source::Ati}){
                const auto total=source==pool::Source::Nvx?free+2*pool::gib:0;
                const auto result=pool::resolve(fallback,{source,free,total},registered,geometry);
                const auto reserve=std::max<std::uint64_t>(512*pool::mib,free/4);
                const auto expected=registered+free-reserve-geometry;
                CHECK(result.bytes==std::max(fallback,expected)&&result.grew);
                CHECK(result.bytes>2048*pool::mib&&result.source==source);
                CHECK(result.driver_reserve_bytes==reserve&&result.geometry_reserve_bytes==geometry);
                CHECK(result.bytes-registered+reserve+geometry<=free);
                CHECK(result.legacy_bytes==fallback&&result.registered_start_bytes==registered);
            }
        }
    }
    // Unsupported extensions issue zero GL queries. NVX values are individual
    // integers in KiB; a valid NVX hint takes priority over the ATI fallback.
    Query stub;CHECK(stub.run(false,false).source==pool::Source::Unknown&&stub.calls.empty());
    stub.dedicated=12*1024*1024;stub.available=8*1024*1024;
    auto hint=stub.run(true,true);
    CHECK(hint.source==pool::Source::Nvx&&hint.dedicated_bytes==12*pool::gib&&hint.available_bytes==8*pool::gib);
    CHECK((stub.calls==std::vector<unsigned>{pool::nvx_dedicated,pool::nvx_available}));
    // ATI writes all four elements under ASan. Auxiliary pools are ignored,
    // including wildly large or negative values, never added as extra VRAM.
    for(int aux:{-1,0,std::numeric_limits<int>::max()}){
        Query ati;ati.ati={8*1024*1024,2*1024*1024,aux,aux};hint=ati.run(false,true);
        CHECK(hint.source==pool::Source::Ati&&hint.available_bytes==8*pool::gib&&hint.dedicated_bytes==0);
        CHECK((ati.calls==std::vector<unsigned>{pool::ati_texture_free}));
        CHECK(pool::resolve(2*pool::gib,hint,registered,geometry).bytes==5376*pool::mib);
    }
    for(unsigned invalid=0;invalid<5;++invalid){
        Query bad;bad.ati={8*1024*1024,1024*1024,0,0};
        switch(invalid){
        case 0:bad.dedicated=-1;bad.available=1;break;
        case 1:bad.dedicated=0;bad.available=0;break;
        case 2:bad.dedicated=12*1024*1024;bad.available=-1;break;
        case 3:bad.dedicated=4*1024*1024;bad.available=8*1024*1024;break;
        case 4:bad.dedicated=bad.available=std::numeric_limits<int>::max();break;
        }
        CHECK(bad.run(true,false).source==pool::Source::Unknown);
        bad.calls.clear();const auto fallback=bad.run(true,true);
        CHECK(fallback.source==(invalid==3?pool::Source::Unknown:pool::Source::Ati)&&bad.calls.size()==3);
        if(invalid==2)CHECK(fallback.dedicated_bytes==12*pool::gib);
    }
    // A known3GB dedicated total cannot be replaced by an8GB pageable/free
    // pool hint. Even with a broken NVX available query, it stays conservative.
    Query contradiction;contradiction.dedicated=3*1024*1024;contradiction.available=-1;
    contradiction.ati={8*1024*1024,2*1024*1024,0,0};
    CHECK(contradiction.run(true,true).source==pool::Source::Unknown);
    contradiction.dedicated=8*1024*1024;contradiction.ati={6*1024*1024,2*1024*1024,0,0};
    const auto capped_ati=contradiction.run(true,true);
    CHECK(capped_ati.source==pool::Source::Ati&&capped_ati.dedicated_bytes==8*pool::gib);
    CHECK(pool::resolve(2*pool::gib,capped_ati,4*pool::gib,geometry).bytes==2*pool::gib);
    for(auto values:{std::array<int,4>{-1,0,0,0},{1,-1,0,0},{1,2,0,0},
                     {std::numeric_limits<int>::max(),0,0,0}}){
        Query bad;bad.ati=values;CHECK(bad.run(false,true).source==pool::Source::Unknown);
    }
    Query empty;empty.dedicated=8*1024*1024;empty.available=0;
    CHECK(empty.run(true,false).source==pool::Source::Nvx);
    empty.ati={0,0,0,0};CHECK(empty.run(false,true).source==pool::Source::Ati);
    // Reject inconsistent current-cache accounting instead of claiming the
    // same memory is both free and already occupied on a known dedicated GPU.
    for(std::uint64_t current:{4*pool::gib,9*pool::gib}){
        auto result=pool::resolve(2*pool::gib,{pool::Source::Nvx,6*pool::gib,8*pool::gib},current,geometry);
        CHECK(result.bytes==2*pool::gib&&!result.grew);
    }
    const auto huge=std::numeric_limits<std::uint64_t>::max();
    CHECK(pool::add(huge,1)==pool::maximum_bytes&&pool::add(pool::maximum_bytes-1,2)==pool::maximum_bytes);
    CHECK(pool::subtract(1,huge)==0&&pool::subtract(huge,huge)==0);
    for(auto malformed:{pool::Hints{pool::Source::Unknown,8*pool::gib,0},
                        pool::Hints{pool::Source::Nvx,8*pool::gib,0},
                        pool::Hints{pool::Source::Ati,8*pool::gib,4*pool::gib},
                        pool::Hints{pool::Source::Ati,huge,0},
                        pool::Hints{static_cast<pool::Source>(99),8*pool::gib,0}}){
        CHECK(pool::resolve(2*pool::gib,malformed,registered,geometry).bytes==2*pool::gib);
    }
    const pool::Hints ample{pool::Source::Ati,24*pool::gib,0};
    CHECK(pool::resolve(2*pool::gib,ample,huge,geometry).bytes==2*pool::gib);
    CHECK(pool::resolve(2*pool::gib,ample,registered,huge).bytes==2*pool::gib);
    CHECK(pool::resolve(huge,ample,registered,geometry).bytes==pool::maximum_bytes);
    CHECK(pool::resolve(2*pool::gib,ample,registered,pool::maximum_bytes).bytes==2*pool::gib);
    // Exhaust a large deterministic range: no wrap/underflow, no reduction of
    // the selected legacy pool, and every growth leaves both reservations.
    for(std::uint64_t free=0;free<=64*pool::gib;free+=113*pool::mib){
        for(std::uint64_t buffers:{std::uint64_t(0),192*pool::mib,1280*pool::mib,8*pool::gib}){
            const auto result=pool::resolve(2*pool::gib,{pool::Source::Ati,free,0},registered,buffers);
            CHECK(result.bytes>=2*pool::gib&&result.bytes<=pool::maximum_bytes);
            if(result.grew)CHECK(result.bytes-registered+result.driver_reserve_bytes+buffers<=free);
        }
    }
    // Resolving a pool has no access to or mutation path into saved choices.
    CHECK(graphics.environment_textures==WorldEnvironmentTextures::High&&graphics.character_textures==WorldCharacterTextures::Medium);
    CHECK(graphics.reflections&&graphics.clouds&&graphics.volumetric_fog&&graphics.contact_shadows&&graphics.render_scale==1.f);
    std::cout<<"PASS "<<checks<<" startup texture-pool checks: exact3GB/unknown fallback, ample8/12/24GB growth, supported-only NVX/4-int ATI hints, primary-only KiB units, geometry/driver reserves, bounded arithmetic and settings isolation\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
