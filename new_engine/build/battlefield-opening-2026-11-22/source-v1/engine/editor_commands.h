#ifndef ENGINE_EDITOR_COMMANDS_H
#define ENGINE_EDITOR_COMMANDS_H

#include <stdbool.h>

typedef enum{
    EDITOR_COMMAND_DOMAIN_NONE=0,
    EDITOR_COMMAND_DOMAIN_PROP,
    EDITOR_COMMAND_DOMAIN_SURFACE,
    EDITOR_COMMAND_DOMAIN_MARKER,
    EDITOR_COMMAND_DOMAIN_SPAWN,
    EDITOR_COMMAND_DOMAIN_NPC
}EditorCommandDomain;

typedef enum{
    EDITOR_COMMAND_KIND_NONE=0,
    EDITOR_COMMAND_KIND_TRANSACTION_BEGIN,
    EDITOR_COMMAND_KIND_TRANSACTION_END,
    EDITOR_COMMAND_KIND_PROP_CREATE_MODEL,
    EDITOR_COMMAND_KIND_PROP_DELETE,
    EDITOR_COMMAND_KIND_PROP_SET_Y,
    EDITOR_COMMAND_KIND_PROP_SET_ROTATION_Y,
    EDITOR_COMMAND_KIND_PROP_SET_SCALE,
    EDITOR_COMMAND_KIND_PROP_SET_PICKABLE,
    EDITOR_COMMAND_KIND_PROP_SET_PICKUP_ITEM,
    EDITOR_COMMAND_KIND_PROP_SET_PICKUP_COUNT,
    EDITOR_COMMAND_KIND_PROP_SET_INTERACT_TYPE,
    EDITOR_COMMAND_KIND_PROP_SET_INTERACT_RADIUS,
    EDITOR_COMMAND_KIND_PROP_SET_DOOR_OPEN_ANGLE,
    EDITOR_COMMAND_KIND_PROP_ADD_ACTION,
    EDITOR_COMMAND_KIND_PROP_REMOVE_ACTION,
    EDITOR_COMMAND_KIND_PROP_SET_ACTION_TYPE,
    EDITOR_COMMAND_KIND_PROP_SET_ACTION_RADIUS,
    EDITOR_COMMAND_KIND_PROP_SET_ACTION_TARGET_INDEX,
    EDITOR_COMMAND_KIND_PROP_SET_ACTION_CONDITION_TYPE,
    EDITOR_COMMAND_KIND_PROP_SET_ACTION_CONDITION_FLAG,
    EDITOR_COMMAND_KIND_PROP_SET_ACTION_PROMPT_TEXT,
    EDITOR_COMMAND_KIND_SURFACE_CREATE,
    EDITOR_COMMAND_KIND_SURFACE_DELETE,
    EDITOR_COMMAND_KIND_SURFACE_SET_MATERIAL,
    EDITOR_COMMAND_KIND_SURFACE_SET_KIND,
    EDITOR_COMMAND_KIND_SURFACE_SET_WIDTH,
    EDITOR_COMMAND_KIND_SURFACE_SET_HEIGHT,
    EDITOR_COMMAND_KIND_SURFACE_SET_THICKNESS,
    EDITOR_COMMAND_KIND_SURFACE_SET_COLLISION,
    EDITOR_COMMAND_KIND_SURFACE_SET_POSITION,
    EDITOR_COMMAND_KIND_SURFACE_SET_YAW,
    EDITOR_COMMAND_KIND_SURFACE_SCALE_UNIFORM,
    EDITOR_COMMAND_KIND_SURFACE_COMMIT_XFORM,
    EDITOR_COMMAND_KIND_MARKER_CREATE,
    EDITOR_COMMAND_KIND_MARKER_DELETE,
    EDITOR_COMMAND_KIND_MARKER_SET_KIND,
    EDITOR_COMMAND_KIND_MARKER_SET_RADIUS,
    EDITOR_COMMAND_KIND_MARKER_SET_POSITION,
    EDITOR_COMMAND_KIND_SPAWN_CREATE,
    EDITOR_COMMAND_KIND_SPAWN_DELETE,
    EDITOR_COMMAND_KIND_SPAWN_SET_POSITION,
    EDITOR_COMMAND_KIND_SPAWN_SET_MODEL_IDX,
    EDITOR_COMMAND_KIND_SPAWN_SET_SCALE_OVERRIDE,
    EDITOR_COMMAND_KIND_SPAWN_SET_HP_OVERRIDE,
    EDITOR_COMMAND_KIND_SPAWN_SET_DAMAGE_OVERRIDE,
    EDITOR_COMMAND_KIND_SPAWN_SET_SPEED_OVERRIDE,
    EDITOR_COMMAND_KIND_SPAWN_SET_CHASE_RADIUS_OVERRIDE,
    EDITOR_COMMAND_KIND_SPAWN_SET_ATTACK_RADIUS_OVERRIDE,
    EDITOR_COMMAND_KIND_SPAWN_SET_ATTACK_COOLDOWN_OVERRIDE,
    EDITOR_COMMAND_KIND_NPC_CREATE,
    EDITOR_COMMAND_KIND_NPC_DELETE,
    EDITOR_COMMAND_KIND_NPC_SET_POSITION,
    EDITOR_COMMAND_KIND_NPC_SET_YAW,
    EDITOR_COMMAND_KIND_NPC_SET_MODEL_IDX,
    EDITOR_COMMAND_KIND_NPC_SET_NAME,
    EDITOR_COMMAND_KIND_NPC_SET_BACKSTORY,
    EDITOR_COMMAND_KIND_NPC_PATCH
}EditorCommandKind;

typedef struct{
    EditorCommandDomain domain;
    EditorCommandKind kind;
    int primary_index;
    int secondary_index;
    bool changed;
    bool transaction_open;
    int transaction_size;
    char label[64];
}EditorCommandResult;

typedef struct{
    int transaction_depth;
    int transaction_size;
    char label[64];
}EditorCommandRuntime;

static void editor_commands_runtime_reset(EditorCommandRuntime *rt);
static EditorCommandRuntime *editor_commands_runtime(void);
static bool editor_commands_begin_transaction(EditorCommandRuntime *rt,const char *label,
                                              EditorCommandResult *out);
static bool editor_commands_end_transaction(EditorCommandRuntime *rt,
                                            EditorCommandResult *out);
static bool editor_command_prefab_place_props(int prefab_idx,Vec3 anchor,float yaw_deg,float scale,
                                              int *out_first_idx,EditorCommandResult *out);
static bool editor_command_prefab_place_instance(int prefab_idx,Vec3 anchor,float yaw_deg,float scale,
                                                 LightingSystem *ls,int *out_first_idx,
                                                 EditorCommandResult *out);

static bool editor_command_prop_create_model(const char *path,Vec3 pos,float rot_x,float rot_y,
                                             float rot_z,float scale,int coll,
                                             bool snap_to_ground,int *out_idx,
                                             EditorCommandResult *out);
static bool editor_command_prop_delete(int idx,EditorCommandResult *out);
static bool editor_command_prop_set_y(int idx,float y,void *placer,EditorCommandResult *out);
static bool editor_command_prop_set_rotation_y(int idx,float yaw_deg,void *placer,
                                               EditorCommandResult *out);
static bool editor_command_prop_set_scale(int idx,float scale,void *placer,
                                          EditorCommandResult *out);
static bool editor_command_prop_set_pickable(int idx,bool pickable,EditorCommandResult *out);
static bool editor_command_prop_set_pickup_item(int idx,int item_type,
                                                EditorCommandResult *out);
static bool editor_command_prop_set_pickup_count(int idx,int count,
                                                 EditorCommandResult *out);
static bool editor_command_prop_set_interact_type(int idx,int interact_type,
                                                  EditorCommandResult *out);
static bool editor_command_prop_set_interact_radius(int idx,float radius,
                                                    EditorCommandResult *out);
static bool editor_command_prop_set_door_open_angle(int idx,float angle_deg,
                                                    EditorCommandResult *out);
static bool editor_command_prop_add_action(int idx,EditorCommandResult *out);
static bool editor_command_prop_remove_action(int idx,int action_idx,EditorCommandResult *out);
static bool editor_command_prop_set_action_type(int idx,int action_idx,int action_type,
                                                EditorCommandResult *out);
static bool editor_command_prop_set_action_radius(int idx,int action_idx,float radius,
                                                  EditorCommandResult *out);
static bool editor_command_prop_set_action_target_index(int idx,int action_idx,int target_index,
                                                        EditorCommandResult *out);
static bool editor_command_prop_set_action_sound_idx(int idx,int action_idx,int sound_idx,
                                                     EditorCommandResult *out);
static bool editor_command_prop_set_action_sound_volume(int idx,int action_idx,float volume,
                                                        EditorCommandResult *out);
static bool editor_command_prop_set_action_sound_path(int idx,int action_idx,const char *path,
                                                      EditorCommandResult *out);
static bool editor_command_prop_set_action_condition_type(int idx,int action_idx,int condition_type,
                                                          EditorCommandResult *out);
static bool editor_command_prop_set_action_condition_flag(int idx,int action_idx,int condition_flag,
                                                          EditorCommandResult *out);
static bool editor_command_prop_set_action_prompt_text(int idx,int action_idx,const char *text,
                                                       EditorCommandResult *out);
static bool editor_command_prop_set_collision_mode(int idx,int coll_type,void *placer,
                                                   EditorCommandResult *out);
static bool editor_command_prop_set_custom_collision_enabled(int idx,bool enabled,
                                                             EditorCommandResult *out);
static bool editor_command_prop_add_custom_collision_box(int idx,EditorCommandResult *out);
static bool editor_command_prop_remove_custom_collision_box(int idx,int box_idx,
                                                            EditorCommandResult *out);
static bool editor_command_prop_set_custom_collision_box_shape(int idx,int box_idx,int shape,
                                                               EditorCommandResult *out);
static bool editor_command_prop_set_custom_collision_box_center(int idx,int box_idx,int axis,float value,
                                                                EditorCommandResult *out);
static bool editor_command_prop_set_custom_collision_box_half(int idx,int box_idx,int axis,float value,
                                                              EditorCommandResult *out);
static bool editor_command_prop_set_custom_collision_box_yaw(int idx,int box_idx,float value,
                                                             EditorCommandResult *out);
static bool editor_command_surface_create(int kind,Vec3 pos,float yaw_deg,
                                          float width,float height,float thickness,
                                          int material_id,int coll_type,
                                          int *out_idx,EditorCommandResult *out);
static bool editor_command_surface_delete(int idx,EditorCommandResult *out);
static bool editor_command_surface_set_material(int idx,int material_id,EditorCommandResult *out);
static bool editor_command_surface_set_kind(int idx,int kind,EditorCommandResult *out);
static bool editor_command_surface_set_width(int idx,float value,EditorCommandResult *out);
static bool editor_command_surface_set_height(int idx,float value,EditorCommandResult *out);
static bool editor_command_surface_set_thickness(int idx,float value,EditorCommandResult *out);
static bool editor_command_surface_set_collision(int idx,int coll_type,EditorCommandResult *out);
static bool editor_command_surface_set_position(int idx,Vec3 pos,EditorCommandResult *out);
static bool editor_command_surface_set_yaw(int idx,float yaw_deg,EditorCommandResult *out);
static bool editor_command_surface_scale_uniform(int idx,float mul,EditorCommandResult *out);
static bool editor_command_surface_commit_xform(int idx,
                                                Vec3 before_pos,float before_yaw_deg,
                                                float before_width,float before_height,float before_thickness,
                                                EditorCommandResult *out);
static bool editor_command_marker_create(int kind,Vec3 pos,float radius,
                                         int *out_idx,EditorCommandResult *out);
static bool editor_command_marker_delete(int idx,EditorCommandResult *out);
static bool editor_command_marker_set_kind(int idx,int kind,EditorCommandResult *out);
static bool editor_command_marker_set_radius(int idx,float radius,EditorCommandResult *out);
static bool editor_command_marker_set_position(int idx,Vec3 pos,EditorCommandResult *out);
static bool editor_command_spawn_create(int etype,Vec3 pos,int model_idx,
                                        float hp_over,float dmg_over,float speed_over,
                                        float chase_r_over,float atk_r_over,float atk_cd_over,float scale_over,
                                        int *out_idx,EditorCommandResult *out);
static bool editor_command_spawn_delete(int idx,EditorCommandResult *out);
static bool editor_command_spawn_set_position(int idx,Vec3 pos,EditorCommandResult *out);
static bool editor_command_spawn_set_model_idx(int idx,int model_idx,EditorCommandResult *out);
static bool editor_command_spawn_set_scale_override(int idx,float scale_over,EditorCommandResult *out);
static bool editor_command_spawn_set_hp_override(int idx,float hp_over,EditorCommandResult *out);
static bool editor_command_spawn_set_damage_override(int idx,float dmg_over,EditorCommandResult *out);
static bool editor_command_spawn_set_speed_override(int idx,float speed_over,EditorCommandResult *out);
static bool editor_command_spawn_set_chase_radius_override(int idx,float chase_r_over,EditorCommandResult *out);
static bool editor_command_spawn_set_attack_radius_override(int idx,float atk_r_over,EditorCommandResult *out);
static bool editor_command_spawn_set_attack_cooldown_override(int idx,float atk_cd_over,EditorCommandResult *out);
static bool editor_command_npc_create(Vec3 pos,float yaw,int model_idx,const char *name,
                                      NpcSystem *npcs_runtime,int *out_idx,EditorCommandResult *out);
static bool editor_command_npc_delete(int idx,NpcSystem *npcs_runtime,
                                      int *selected_idx,
                                      bool *wp_mode,int *wp_npc,
                                      bool *dlg_mode,int *dlg_npc,int *dlg_sel,char *dlg_buf,int *dlg_len,
                                      bool *tree_mode,int *tree_npc,int *tree_sel,int *tree_choice,
                                      bool *tree_editing,int *tree_field,char *tree_buf,int *tree_len,
                                      EditorCommandResult *out);
static bool editor_command_npc_set_position(int idx,Vec3 pos,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_yaw(int idx,float yaw,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_visual_yaw_offset(int idx,float yaw_offset,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_model_idx(int idx,int model_idx,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_scale(int idx,float scale,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_collision_enabled(int idx,bool enabled,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_patrol_loop(int idx,bool enabled,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_patrol_speed(int idx,float speed,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_patrol_wait(int idx,float wait_time,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_merchant_enabled(int idx,bool enabled,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_shop_id(int idx,int shop_id,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_name(int idx,const char *name,NpcSystem *npcs_runtime,EditorCommandResult *out);
static bool editor_command_npc_set_backstory(int idx,const char *backstory,NpcSystem *npcs_runtime,EditorCommandResult *out);

#endif
