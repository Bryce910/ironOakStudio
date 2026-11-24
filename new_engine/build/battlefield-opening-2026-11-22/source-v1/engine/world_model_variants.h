#ifndef VALWOOD_WORLD_MODEL_VARIANTS_H
#define VALWOOD_WORLD_MODEL_VARIANTS_H
#include <string_view>
namespace world_model_variants {
// Offline-owned union geometry. Untagged pieces are shared; tagged pieces
// belong to a subset of four appearances. Selection is one stable actor bit.
constexpr unsigned all=15;
inline bool valid_selection(unsigned selected){return !selected||selected==1||selected==2||selected==4||selected==8;}
inline unsigned selection(unsigned selected){return selected?selected:1;}
inline bool visible(unsigned mesh,unsigned selected){return !mesh||bool(mesh&selection(selected));}
inline const char *strip(const char *name,unsigned &mask){
 mask=0;if(!name)return nullptr;
 constexpr std::string_view prefix="VALWOOD_VARIANT:";
 std::string_view s(name);if(s.substr(0,prefix.size())!=prefix)return name;
 const char *p=name+prefix.size();unsigned n=0,digits=0;
 while(*p>='0'&&*p<='9'){if(++digits>2)throw "Invalid arena variant mask";n=n*10+unsigned(*p++-'0');}
 if(!digits||!n||n>all||*p!=' '||!p[1])throw "Invalid arena variant mask";
 mask=n;return p+1;
}
inline unsigned actor(unsigned id){return 1u<<((id?id-1:0)%4);}
}
#endif
