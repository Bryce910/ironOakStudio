#ifndef VALWOOD_COMBAT_VFX_RENDER_H
#define VALWOOD_COMBAT_VFX_RENDER_H
#include "engine/world_combat_vfx.h"
struct WorldCombatVfxRenderer;
struct WorldCombatVfxFrame {unsigned color=0,depth=0;int width=0,height=0;float vp[16]={},near_plane=.4f,far_plane=15000;world_combat_vfx::Point camera;float right[3]={1,0,0},up[3]={0,1,0};};
struct WorldCombatVfxStats {unsigned long long draws=0,uploads=0,failures=0;unsigned peak_particles=0,peak_trails=0;};
WorldCombatVfxRenderer *world_combat_vfx_create();
// Blends into color without sampling it. Samples only the separate opaque depth.
// No textures, simulation buffers, geometry or shader programs allocate here.
bool world_combat_vfx_draw(WorldCombatVfxRenderer *,const world_combat_vfx::System &,const WorldCombatVfxFrame &);
WorldCombatVfxStats world_combat_vfx_stats(const WorldCombatVfxRenderer *);
void world_combat_vfx_free(WorldCombatVfxRenderer *);
#endif
