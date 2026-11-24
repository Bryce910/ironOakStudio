#ifndef VALWOOD_WORLD_STRONGHOLD_H
#define VALWOOD_WORLD_STRONGHOLD_H
#include <cstddef>
#include <string>
struct WorldStronghold;
struct WorldStrongholdStats {unsigned actors=0,walking=0,visible=0,peak_visible=0,poses=0;unsigned long long updates=0,uploads=0,failures=0,frustum_tests=0,frustum_culled=0;};
WorldStronghold *world_stronghold_load(const char *directory,std::string &error);
void world_stronghold_update(WorldStronghold *,double seconds);
void world_stronghold_draw(WorldStronghold *,unsigned program,double x,double y,double z,unsigned pass=0,const float *view_projection=nullptr);
bool world_stronghold_can_move(const WorldStronghold *,double from_x,double from_z,double x,double z,double feet,float radius);
WorldStrongholdStats world_stronghold_stats(const WorldStronghold *);
void world_stronghold_free(WorldStronghold *);
#endif
