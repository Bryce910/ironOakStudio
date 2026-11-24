#include "engine/world_atlas.h"
#include "engine/world_studio.h"
#include "engine/world_pond_field.h"
#include "engine/world_surface_field.h"
#include "engine/world_river_network.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
struct NativeCallbacks {
 static float height(double x,double z,const void *data){const auto &doc=*static_cast<const world_studio::Document*>(data);return world_studio::height_delta(doc,x,z)+(doc.surface_field?world_surface::terrain_offset(*doc.surface_field,x,z):0);}
 static bool water(double x,double z,float *level,float *depth,const void *data){
  const auto &doc=*static_cast<const world_studio::Document*>(data);
  if(doc.surface_field){float y=0,shore=0;if(world_surface::river(*doc.surface_field,x,z,y,shore)){float d=y-world_atlas_height(x,z);if(d>.005f){if(level)*level=y;if(depth)*depth=d;return true;}}}
  for(const auto &w:doc.waters)if(w.source==3&&w.enabled&&world_entity::visible(doc.entities,{world_entity::Kind::Water,unsigned(&w-doc.waters.data())})){double dx=(x-w.x)/w.radius_x,dz=(z-w.z)/w.radius_z;if(dx*dx+dz*dz>=1)continue;float d=float(w.level)-world_atlas_height(x,z);if(d>.005f){if(level)*level=float(w.level);if(depth)*depth=d;return true;}}
  return false;
 }
};
int main(int argc,char **argv){
 if(argc!=4)return 2;namespace fs=std::filesystem;const auto project=fs::absolute(argv[1]);world_studio::Document doc;std::string error;char why[512]={};
 if(!world_atlas_init()||world_atlas_load_height_region((project/"ground/showcase.height").c_str(),why,sizeof why)!=1||world_atlas_load_fallback_height_region((project/"ground/countryside.height").c_str(),why,sizeof why)<0||!world_studio::load((project/"world.studio").c_str(),doc,error)||!world_pond_field::prepare(doc,{world_atlas_base_height},error)){std::cerr<<why<<' '<<error;return 2;}
 world_atlas_set_height_edit(NativeCallbacks::height,&doc);world_atlas_set_water_edit(NativeCallbacks::water,&doc);
 std::ifstream input(argv[2],std::ios::binary);std::ofstream output(argv[3],std::ios::binary);double xy[2];unsigned count=0;
 while(input.read(reinterpret_cast<char*>(xy),sizeof xy)){
  float water=0,depth=0;bool wet=world_atlas_local_water(xy[0],xy[1],&water,&depth);double v[8]={world_atlas_base_height(xy[0],xy[1]),world_studio::height_delta(doc,xy[0],xy[1]),world_atlas_height(xy[0],xy[1]),world_atlas_walk_height(xy[0],xy[1]),wet?1.:0.,water,depth,0.};
  if(doc.surface_field&&doc.surface_field->river_network)v[7]=doc.surface_field->river_network->sample(xy[0],xy[1]).distance;
  output.write(reinterpret_cast<char*>(v),sizeof v);++count;
 }
 const bool ok=input.eof()&&output.good();std::cout<<"{\"passed\":"<<(ok?"true":"false")<<",\"points\":"<<count<<",\"terrain_tiles\":"<<doc.terrain.size()<<",\"derived_tiles\":"<<(doc.surface_field?doc.surface_field->tiles.size():0)<<"}\n";
 world_atlas_set_height_edit(nullptr,nullptr);world_atlas_set_water_edit(nullptr,nullptr);world_atlas_shutdown();return ok?0:1;
}
