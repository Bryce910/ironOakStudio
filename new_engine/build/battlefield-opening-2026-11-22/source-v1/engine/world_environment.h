#ifndef VALWOOD_WORLD_ENVIRONMENT_H
#define VALWOOD_WORLD_ENVIRONMENT_H
#include <cstdint>
#include <string>
struct WorldEnvironment;
struct WorldEnvironmentStats {unsigned raw=0,filtered=0,brdf=0,side=0,levels=0;std::uint64_t bytes=0;float sh[9][3]={};};
// Loading/Apply only. Linear Radiance HDR panorama, max4096x2048/64MiB.
// Immutable256-square raw + GGX-filtered cubes and128-square split-sum BRDF LUT.
// CPU source data and filter FBO/programs are released before returning.
WorldEnvironment *world_environment_create(const char *path,std::string &error,unsigned side=256);
WorldEnvironmentStats world_environment_stats(const WorldEnvironment *);
// Owner-thread, fixed program-location cache. Cube15 / BRDF6; terrain uses SH
// uniforms only, preserving its16 existing samplers. Disabled weights are zero.
void world_environment_bind(WorldEnvironment *,unsigned program,float rotation,float intensity,float strength);
void world_environment_invalidate_programs(WorldEnvironment *);
void world_environment_free(WorldEnvironment *);
#endif
