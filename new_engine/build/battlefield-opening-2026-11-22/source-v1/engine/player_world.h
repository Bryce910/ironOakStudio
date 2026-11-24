#ifndef ENGINE_PLAYER_WORLD_H
#define ENGINE_PLAYER_WORLD_H

#include <stdbool.h>

#include "engine/runtime_types.h"
#include "engine/scene_system.h"

typedef struct PropList PropList;

void player_world_update(Player *player, LevelSystem *lvlsys,
                         AbilitySystem *as, FishingSystem *fishing,
                         Inventory *inv, SaveNotify *save_note,
                         SceneID current_scene,
                         Camera3P *cam, AnimMap *player_amap,
                         PropList *props, float dt,
                         bool fishing_space_pressed);

#endif
