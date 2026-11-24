#ifndef VALWOOD_WORLD_SOUNDSCAPE_H
#define VALWOOD_WORLD_SOUNDSCAPE_H
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <string>
namespace world_soundscape {
inline constexpr unsigned sound_limit=64,asset_limit=32;
inline constexpr std::size_t name_limit=96,asset_path_limit=240;
enum class WeatherCondition : unsigned {Always=0,Rain=1,Dry=2,Storm=3};
struct Sound {std::string name="Sound",asset;double x=0,y=0,z=0;float volume=.6f,inner_radius=2,radius=20,fade_in=.5f,fade_out=1;bool enabled=true,loop=true,spatial=true;WeatherCondition weather_condition=WeatherCondition::Always;float min_rain=0,max_rain=1;};
struct Emitter {std::uint64_t id=0;Sound sound;float condition_gain=1;};
// Strict UTF-8; no embedded NUL/control bytes or overlong/surrogate codepoints.
inline bool text(const std::string &s,std::size_t limit,bool empty=false){
 if(s.size()>limit||(!empty&&s.empty()))return false;
 for(std::size_t i=0;i<s.size();){const unsigned char c=static_cast<unsigned char>(s[i]);if(c<32||c==127)return false;if(c<128){++i;continue;}
  unsigned count=0,code=0,min=0;if(c>=0xc2&&c<=0xdf){count=2;code=c&31;min=0x80;}else if(c>=0xe0&&c<=0xef){count=3;code=c&15;min=0x800;}else if(c>=0xf0&&c<=0xf4){count=4;code=c&7;min=0x10000;}else return false;
  if(i+count>s.size())return false;for(unsigned k=1;k<count;++k){const unsigned char q=static_cast<unsigned char>(s[i+k]);if((q&0xc0)!=0x80)return false;code=(code<<6)|(q&63);}if(code<min||code>0x10ffff||(code>=0x80&&code<=0x9f)||code==0x2028||code==0x2029||(code>=0xd800&&code<=0xdfff))return false;i+=count;
 }return true;
}
// Project-relative WAV only. Empty is an unassigned editor placeholder; it
// never produces a runtime voice. File existence/PCM decode is an asset-boundary check.
inline bool asset_path(const std::string &path){
 if(!text(path,asset_path_limit)||path.front()=='/'||path.find('\\')!=std::string::npos||path.find(':')!=std::string::npos||path.find_first_of("<>\"|?*")!=std::string::npos)return false;
 std::size_t start=0;while(start<=path.size()){auto end=path.find('/',start);if(end==std::string::npos)end=path.size();const auto n=end-start;if(!n||(n==1&&path[start]=='.')||(n==2&&path[start]=='.'&&path[start+1]=='.'))return false;if(end==path.size())break;start=end+1;}
 if(path.size()<5)return false;const std::size_t n=path.size();return (path[n-4]=='.'&&(path[n-3]=='w'||path[n-3]=='W')&&(path[n-2]=='a'||path[n-2]=='A')&&(path[n-1]=='v'||path[n-1]=='V'));
}
inline bool valid(const Sound &s){
 auto within=[](double x,double lo,double hi){return std::isfinite(x)&&x>=lo&&x<=hi;};
 return text(s.name,name_limit)&&s.name.find_first_not_of(' ')!=std::string::npos&&(s.asset.empty()||asset_path(s.asset))&&within(s.x,-50400,50400)&&within(s.y,-2000,12000)&&within(s.z,-28400,28400)&&within(s.volume,0,1)&&within(s.inner_radius,0,10000)&&within(s.radius,.1,10000)&&s.inner_radius<=s.radius&&within(s.fade_in,0,60)&&within(s.fade_out,0,60)&&unsigned(s.weather_condition)<=3&&within(s.min_rain,0,1)&&within(s.max_rain,0,1)&&s.min_rain<=s.max_rain;
}
inline bool valid(const Sound &s,std::string &error){if(!valid(s)){error="Invalid sound: use a UTF-8 name, relative WAV path, finite world position, gain0–1, inner≤outer distance, and fades0–60 seconds";return false;}error.clear();return true;}
inline bool default_weather(const Sound&s){return s.weather_condition==WeatherCondition::Always&&s.min_rain==0&&s.max_rain==1;}
inline float weather_gain(const Sound&s,float rain,float storm_weight){
 if(!std::isfinite(rain)||!std::isfinite(storm_weight)||unsigned(s.weather_condition)>3)return 0;
 rain=std::clamp(rain,0.f,1.f);storm_weight=std::clamp(storm_weight,0.f,1.f);
 if(s.weather_condition==WeatherCondition::Always)return 1;
 if(rain<s.min_rain||rain>s.max_rain)return 0;
 if(s.weather_condition==WeatherCondition::Rain)return rain;
 if(s.weather_condition==WeatherCondition::Dry)return 1-rain;
 return storm_weight;
}
// Distance multiplier only: volume and time envelopes remain separate. Uses
// actual 3D centre distance, with a hard finite bound at the outer radius.
inline float attenuation(const Sound &s,double x,double y,double z){
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.z)||!std::isfinite(s.inner_radius)||!std::isfinite(s.radius)||s.radius<=0||s.inner_radius<0||s.inner_radius>s.radius)return 0;
 if(!s.spatial)return 1;const double distance=std::hypot(std::hypot(x-s.x,y-s.y),z-s.z);if(!std::isfinite(distance)||distance>=s.radius)return 0;if(distance<=s.inner_radius)return 1;
 const double t=(distance-s.inner_radius)/(s.radius-s.inner_radius);return float(1-t*t*(3-2*t));
}
}
#endif
