#ifndef ENGINE_COMBAT_H
#define ENGINE_COMBAT_H

#include "engine/game_types.h"
#include "engine/enemy_types.h"

void proj_update(ProjList *pl,float dt,ParticleSystem *ps,
                 Enemy *enemies,int ne,Player *player);

#endif
