#include "engine/world_atlas.h"
#include "engine/world_surface_field.h"
#include <array>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
struct Tile{int x,z;std::array<float,65*65>d;};
static std::vector<Tile> tiles;static world_surface::TerrainField field;
static float sculpt(double x,double z){int tx=int(std::floor(x/128)),tz=int(std::floor(z/128));for(auto&t:tiles)if(t.x==tx&&t.z==tz){double u=(x-tx*128)/2,v=(z-tz*128)/2;int ix=std::clamp(int(u),0,63),iz=std::clamp(int(v),0,63);float a=u-ix,b=v-iz,p00=t.d[iz*65+ix],p10=t.d[iz*65+ix+1],p01=t.d[(iz+1)*65+ix],p11=t.d[(iz+1)*65+ix+1];return a+b<=1?p00+(p10-p00)*a+(p01-p00)*b:p11+(p01-p11)*(1-a)+(p10-p11)*(1-b);}return 0;}
static float ground(double x,double z,void*){return world_atlas_height(x,z)+sculpt(x,z);}
static float edit(double x,double z,const void*){return sculpt(x,z)+world_surface::terrain_offset(field,x,z);}
static bool water(double x,double z,float*l,float*d,const void*){float level=0,shore=0;if(world_surface::river(field,x,z,level,shore)){float depth=level-world_atlas_height(x,z);if(depth>.005f){if(l)*l=level;if(d)*d=depth;return true;}}return false;}
static int initialize_world(){if(!world_atlas_init())return 1;char err[256]={};if(world_atlas_load_height_region("ground/showcase.height",err,sizeof err)<0)return 2;world_atlas_load_fallback_height_region("ground/countryside.height",err,sizeof err);std::ifstream in("world.studio");std::string l,metadata;while(std::getline(in,l)){std::istringstream q(l);std::string kind;q>>kind;if(kind=="surfaces"){std::string s;q>>std::quoted(s);metadata+=s+'\n';}else if(kind=="terrain"){Tile t;q>>t.x>>t.z;for(auto&v:t.d)if(!(q>>v))return 3;tiles.push_back(t);}}world_surface::Data d;std::string error;if(!world_surface::decode(metadata,d,error)||!world_surface::bake_terrain(d,ground,nullptr,field,error)){std::cerr<<error<<'\n';return 4;}world_atlas_set_height_edit(edit,nullptr);world_atlas_set_water_edit(water,nullptr);return 0;}
#ifndef BATTLEFIELD_SAMPLE_LIBRARY
int main(){int status=initialize_world();if(status)return status;double x,z;while(std::cin>>x>>z){float level=0,depth=0;bool wet=world_atlas_local_water(x,z,&level,&depth);std::cout<<std::setprecision(12)<<x<<' '<<z<<' '<<world_atlas_height(x,z)<<' '<<wet<<' '<<level<<' '<<depth<<'\n';}world_atlas_shutdown();}

#endif
