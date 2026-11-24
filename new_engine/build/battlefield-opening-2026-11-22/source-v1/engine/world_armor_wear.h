#ifndef VALWOOD_WORLD_ARMOR_WEAR_H
#define VALWOOD_WORLD_ARMOR_WEAR_H
#include "engine/world_knight_roster.h"
#include <array>
#include <cstdint>

// Cosmetic only. Stable UV-space marks never change armor geometry, health,
// collision, source images, or the purchased animation skeleton.
namespace world_armor_wear {
inline std::uint32_t mix(std::uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline std::uint32_t bank_seed(const char *name){std::uint32_t h=2166136261u;for(;name&&*name;++name){h^=static_cast<unsigned char>(*name);h*=16777619u;}return mix(h);}
inline std::array<float,3> finish(unsigned style,std::uint32_t seed){
 return {style==1?.48f:style==2?.9f:0.f,float(seed%65521u),style==2?.25f:.4f};
}
inline std::array<float,3> opponent(unsigned actor,std::uint32_t bank){
 const auto seed=mix(bank^(actor*0x9e3779b9u));const unsigned choice=seed%5;
 return finish(choice<2?0:choice<4?1:2,seed);
}
inline bool read(std::istream &in,unsigned &style){
 std::string magic,key,end,extra;unsigned version=0,next=0;
 if(!(in>>magic>>version>>key>>next>>end)||magic!="VALWOOD_ARMOR_FINISH"||version!=1||key!="style"||next>2||end!="end"||(in>>extra))return false;
 style=next;return true;
}
inline bool save(const std::filesystem::path &path,unsigned style){
 if(style>2)return false;
 const auto bytes="VALWOOD_ARMOR_FINISH 1\nstyle "+std::to_string(style)+"\nend\n";
 auto pattern=path.u8string()+".tmp-XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
 const int fd=mkstemp(name.data());if(fd<0)return false;FILE *file=fdopen(fd,"wb");bool okay=false;
 if(file){okay=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size()&&std::fflush(file)==0&&fsync(fd)==0;if(std::fclose(file))okay=false;}else close(fd);
 if(okay)okay=vw_replace(name.data(),path.u8string().c_str())==0;
 if(!okay)unlink(name.data());return okay;
}
}
#endif
