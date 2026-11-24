// CPU-only authoring commit. Uses the editor's serializer and terrain brushes.
#include "engine/world_studio.h"
#include "engine/world_surface_field.h"
#include "engine/world_atlas.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cmath>
int main(int argc,char **argv){
 if(argc!=4||!world_atlas_init())return 2;
 world_studio::Document doc;std::string error;
 auto fail=[&](){std::fprintf(stderr,"Jungle authoring: %s\n",error.c_str());return 1;};
 if(!world_studio::load(argv[1],doc,error))return fail();
 // Level small wildlife glades; flat inner cores keep every contact sole on
 // the ground across the full wandering route. Feathered edges stay natural.
 for(auto p:{std::array<double,2>{-26796.,9409.496705929329},{-26627.,9355.496705929329}}){
  if(!world_studio::brush(doc,p[0],p[1],15,4,2,world_atlas_base_height(p[0],p[1]),world_atlas_base_height,error))return fail();
 }
 if(!world_studio::carve_pond(doc,-26795.,9372.496705929329,world_atlas_base_height,error))return fail();
 std::ifstream in(argv[2]);std::ostringstream text;text<<in.rdbuf();
 if(!in||!world_surface::decode(text.str(),doc.surfaces,error))return fail();
 world_surface::TerrainField field;
 auto ground=[](double x,double z,void *d){return world_atlas_base_height(x,z)+world_studio::height_delta(*static_cast<world_studio::Document*>(d),x,z);};
 if(!world_surface::bake_terrain(doc.surfaces,ground,&doc,field,error))return fail();
 if(!world_studio::save(argv[3],doc,error))return fail();
 std::printf("Saved jungle: %zu placements, %zu sculpt tiles, %zu graded trails, %zu water bodies\n",doc.placements.size(),doc.terrain.size(),doc.surfaces.ribbons.size(),doc.waters.size());
}
