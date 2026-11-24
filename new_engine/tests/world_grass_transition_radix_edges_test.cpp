#include "engine/world_cover.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <random>
#include <vector>
#define private public
#include "engine/world_grass_transition.h"
#undef private
static bool armed=false;static std::size_t allocations=0;
[[gnu::noinline]] void *operator new(std::size_t n){if(armed)allocations++;if(auto *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
int main(){
 WorldGrassTransition registry;using Ref=WorldGrassTransition::RootRef;
 std::vector<Ref> roots,expected;std::vector<float> fades;roots.reserve(92000);expected.reserve(92000);fades.reserve(92000);std::mt19937_64 random(790133);
 armed=true;allocations=0;void *positive=::operator new(17);armed=false;assert(allocations==1);::operator delete(positive);
 for(std::size_t n:{0u,1u,2u,63u,64u,65u,255u,256u,4095u,4096u,46000u,92000u}){
  roots.clear();for(std::size_t i=0;i<n;i++)roots.push_back({i%13==0?0:i%13==1?~std::uint64_t(0):i%13==2?std::uint64_t(1)<<63:random(),i});
  expected=roots;std::stable_sort(expected.begin(),expected.end(),[](auto a,auto b){return a.id<b.id;});auto *original=roots.data();
  armed=true;allocations=0;registry.radix_sort<64>(roots,[](const Ref &r){return r.id;});armed=false;assert(!allocations&&roots.data()==original);
  for(std::size_t i=0;i<n;i++)assert(roots[i].id==expected[i].id&&roots[i].index==expected[i].index);
  roots.clear();fades.clear();
  const float edges[]={std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::min(),.00001f,.1f,.5f,std::nextafter(1.f,0.f),1.f,std::numeric_limits<float>::max()};
  for(std::size_t i=0;i<n;i++){roots.push_back({i*101+7,i});std::uint32_t bits=std::uint32_t(random()%0x7f7fffffu)+1;float f;std::memcpy(&f,&bits,4);fades.push_back(i%3?edges[i%std::size(edges)]:f);}
  expected=roots;std::sort(expected.begin(),expected.end(),[&](auto a,auto b){return fades[a.index]!=fades[b.index]?fades[a.index]>fades[b.index]:a.id<b.id;});original=roots.data();
  armed=true;allocations=0;registry.radix_sort<32>(roots,[&](const Ref &r){std::uint32_t bits;std::memcpy(&bits,&fades[r.index],4);return~bits;});armed=false;assert(!allocations&&roots.data()==original);
  for(std::size_t i=0;i<n;i++)assert(roots[i].id==expected[i].id&&roots[i].index==expected[i].index);
 }
 std::puts("PASS radix edges:64bit unsigned extremes/duplicate-key stability,positive finite fade bit boundaries/subnormals/ID ties,12sizes0..92000,original backing pointers and zero guarded allocations");
}
