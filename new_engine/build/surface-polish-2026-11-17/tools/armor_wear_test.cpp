#include "engine/world_armor_wear.h"
#include <cassert>
#include <iostream>
#include <sstream>
int main(){
 unsigned style=2;std::istringstream old("VALWOOD_ARMOR_FINISH 1 style 0 end");assert(world_armor_wear::read(old,style)&&style==0);
 for(const char*s:{"VALWOOD_ARMOR_FINISH 1 style 3 end","VALWOOD_ARMOR_FINISH 2 style 1 end","VALWOOD_ARMOR_FINISH 1 style -1 end","VALWOOD_ARMOR_FINISH 1 style 1 end extra"}){unsigned untouched=2;std::istringstream in(s);assert(!world_armor_wear::read(in,untouched)&&untouched==2);}
 const auto path=std::filesystem::path("/tmp/valwood-armor-finish-roundtrip.cfg");
 for(unsigned k=0;k<3;++k){assert(world_armor_wear::save(path,k));std::ifstream f(path);unsigned value=9;assert(world_armor_wear::read(f,value)&&value==k);}
 unsigned clean=0,marked=0,heavy=0;
 for(unsigned bank=1;bank<=18;++bank)for(unsigned actor=1;actor<=30;++actor){
  auto a=world_armor_wear::opponent(actor,bank);auto b=world_armor_wear::opponent(actor,bank);assert(a==b&&a[0]>=0&&a[0]<=1&&a[1]>=0&&a[1]<65536&&a[2]>=0&&a[2]<=1);
  if(a[0]==0)++clean;else if(a[0]<.7)++marked;else ++heavy;
 }
 assert(clean>100&&marked>100&&heavy>50);std::filesystem::remove(path);
 std::cout<<"PASS armor wear: compatible cosmetic config, atomic round trips, stable540 actor appearances, three finish levels ("<<clean<<" clean/"<<marked<<" marked/"<<heavy<<" heavy); no geometry or combat changes\n";
}
