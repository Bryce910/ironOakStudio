#ifndef ENGINE_RENDER_RUNTIME_H
#define ENGINE_RENDER_RUNTIME_H

#include <stdbool.h>

#include "engine/enemy_types.h"
#include "engine/render_types.h"
#include "engine/scene_system.h"

typedef struct Terrain Terrain;
typedef struct PropList PropList;
typedef struct CharModel CharModel;
typedef struct Inventory Inventory;

void ensure_enemy_models_near_camera(Enemy *enemies,int ne, Vec3 cam_pos,
                                     CharModel *enemy_models,
                                     bool enemy_model_loaded[],
                                     CharModel *fallback_char_model);

/* Per-custom-model visual scale multiplier — defined in main.c */
extern float g_enemy_custom_scale_mul[];

void render_shadow_pass_runtime(ShadowMap *sm, ShadowMap *sm_wide,
                                LightingSystem *ls, Terrain *terrain,
                                PropList *props, Camera3P *cam,
                                Inventory *player_inventory,
                                SceneID current_scene,
                                Player *player, CharModel *char_model,
                                Enemy *enemies, int ne,
                                CharModel *enemy_models,
                                bool enemy_model_loaded[],
                                CharModel *npc_models,
                                bool npc_model_ok[],
                                int frame_count, NpcSystem *npcs,
                                bool has_npc_model, int win_w, int win_h,
                                CharModel *enemy_custom_models,
                                int enemy_custom_count,
                                bool *enemy_custom_ok);

void render_character_runtime(SkinProg *skin, Mat4 proj, Mat4 view,
                              Vec3 cam_pos, LightingSystem *ls,
                              Inventory *player_inventory,
                              SceneID current_scene,
                              Player *player, CharModel *char_model,
                              Enemy *enemies, int ne,
                              CharModel *enemy_models,
                              bool enemy_model_loaded[],
                              CharModel *npc_models,
                              bool npc_model_ok[],
                              int frame_count, NpcSystem *npcs,
                              bool has_npc_model,
                              CharModel *enemy_custom_models,
                              int enemy_custom_count,
                              bool *enemy_custom_ok);

#endif
