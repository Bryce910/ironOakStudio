#include "engine/world_uniform_cache.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {bool track=false;std::size_t allocations=0;}
[[gnu::noinline]] void *operator new(std::size_t n){
    if(track)allocations++;
    if(void *p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
[[gnu::noinline]] void operator delete(void *p) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t) noexcept{std::free(p);}

namespace {
constexpr const char *names[]={"uLocalProbeCount","uLocalIrradianceWeight","uLocalIrradiance[0]",
    "uVillageSkyDirectional","uCastleLightPositionRadius[3]","uBase","uOptionalAbsent","uLocalIrradiance[8]"};
struct Backend {
    unsigned program=11,generation=0;
    std::array<unsigned,std::size(names)> queries={};
    int operator()(const char *name){
        std::size_t i=0;while(i<std::size(names)&&std::strcmp(name,names[i]))i++;
        assert(i<std::size(names));queries[i]++;
        return i==6?-1:int(program*100+generation*10000+i);
    }
};
}
int main(){
    WorldUniformCache cache;Backend backend;
    assert(cache.get(0,names[0],backend)==-1&&cache.get(11,nullptr,backend)==-1&&cache.size()==0);
    // Stack names are destroyed/overwritten after the first real query.
    // Cache ownership must not depend on string-literal pointer identity.
    for(std::size_t i=0;i<std::size(names);i++){
        char stack_name[128];std::snprintf(stack_name,sizeof(stack_name),"%s",names[i]);
        const int expected=i==6?-1:int(1100+i);assert(cache.get(11,stack_name,backend)==expected);
        std::memset(stack_name,'x',sizeof(stack_name));
    }
    assert(cache.size()==std::size(names));for(unsigned q:backend.queries)assert(q==1);
    allocations=0;track=true;
    for(unsigned frame=0;frame<20000;frame++)for(std::size_t i=0;i<std::size(names);i++){
        char stack_name[128];std::snprintf(stack_name,sizeof(stack_name),"%s",names[i]);
        assert(cache.get(11,stack_name,backend)==(i==6?-1:int(1100+i)));
    }
    track=false;assert(allocations==0);for(unsigned q:backend.queries)assert(q==1);
    // Different linked program IDs invalidate otherwise identical names.
    backend.program=22;assert(cache.get(22,names[0],backend)==2200&&cache.size()==1&&backend.queries[0]==2);
    // GL may reuse a deleted program ID. Explicit reload reset must still
    // discard that generation's cached locations, including negative ones.
    backend.generation=1;cache.reset(22);assert(cache.size()==0);
    for(std::size_t i=0;i<std::size(names);i++)assert(cache.get(22,names[i],backend)==(i==6?-1:int(12200+i)));
    assert(backend.queries[0]==3);for(std::size_t i=1;i<std::size(names);i++)assert(backend.queries[i]==2);
    allocations=0;track=true;
    for(unsigned frame=0;frame<20000;frame++)for(std::size_t i=0;i<std::size(names);i++)
        assert(cache.get(22,names[i],backend)==(i==6?-1:int(12200+i)));
    track=false;assert(allocations==0);
    cache.reset();assert(cache.size()==0);cache.reset();assert(cache.size()==0);
    std::puts("PASS uniform cache: 320000 hot lookups across long/short/array/inactive names, zero C++ allocations or repeated backend queries; stack-name ownership, program switches and same-ID reload invalidation");
}
