#ifndef VALWOOD_WORLD_MODEL_EQUIPMENT_H
#define VALWOOD_WORLD_MODEL_EQUIPMENT_H
#include <string_view>
namespace world_model_equipment {
constexpr unsigned standard=4|16|64|256,all=511;
// An offline-owned material contract; arbitrary scenery names remain mask0.
inline unsigned mask(const char *name){
 std::string_view s=name?name:"",prefix="VALWOOD_EQUIPMENT:";
 if(s.substr(0,prefix.size())!=prefix)return 0;s.remove_prefix(prefix.size());
 unsigned n=0,count=0;while(!s.empty()&&s.front()>='0'&&s.front()<='9'){
  if(++count>3)throw "Invalid equipment mask";n=n*10+unsigned(s.front()-'0');s.remove_prefix(1);
 }
 if(!count||!n||n>all||(!s.empty()&&s.front()!=' '))throw "Invalid equipment mask";
 return n;
}
inline bool visible(unsigned mesh,unsigned equipped){return !mesh||bool(mesh&equipped);}
}
#endif
