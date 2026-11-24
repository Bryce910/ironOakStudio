#ifndef VALWOOD_WORLD_SNOW_H
#define VALWOOD_WORLD_SNOW_H
#include <string>
inline bool world_snow_frozen_basin(double x,double z){
    const double dx=(x-63)/13,dz=(z-32)/10;
    return dx*dx+dz*dz<.9*.9;
}
struct WorldSnow;
struct WorldSnowStats {unsigned footprints=0,powder=0,created=0,left=0,right=0,puffs=0,audio_steps=0;unsigned long long draws=0,uploads=0,failures=0,imprint_updates=0;unsigned depressed_texels=0;float deepest_imprint=0;bool audio_ready=false;};
WorldSnow *world_snow_create(const char *shaders,const char *sound,std::string &error);
void world_snow_update(WorldSnow *,double time,double x,double feet,double z,bool grounded,double district_x,double district_z);
void world_snow_draw(WorldSnow *,const float *vp,double x,double y,double z,const float *right,const float *up,const float *sky,const float *sun);
void world_snow_bind_terrain(WorldSnow *,unsigned program);
WorldSnowStats world_snow_stats(const WorldSnow *);
void world_snow_free(WorldSnow *);
#endif
