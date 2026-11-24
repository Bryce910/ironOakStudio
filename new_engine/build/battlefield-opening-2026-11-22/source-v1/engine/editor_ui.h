#ifndef ENGINE_EDITOR_UI_H
#define ENGINE_EDITOR_UI_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#ifndef EDITOR_UI_NPC_TYPE_MAX
#define EDITOR_UI_NPC_TYPE_MAX 32
#endif

#ifndef EDITOR_UI_NPC_TREE_MAX
#define EDITOR_UI_NPC_TREE_MAX 32
#endif

#ifndef EDITOR_UI_NPC_WAYPOINT_MAX
#define EDITOR_UI_NPC_WAYPOINT_MAX 8
#endif

#ifndef PLAYER_CHAR_SLOT_COUNT
#define PLAYER_CHAR_SLOT_COUNT 10
#endif

typedef enum{
    EDITOR_UI_BACKEND_HUD=0,
    EDITOR_UI_BACKEND_IMGUI=1
}EditorUIBackend;

typedef enum{
    EDITOR_SELECTION_NONE=0,
    EDITOR_SELECTION_PROP,
    EDITOR_SELECTION_SURFACE,
    EDITOR_SELECTION_COLLISION_VOLUME,
    EDITOR_SELECTION_MARKER,
    EDITOR_SELECTION_NPC,
    EDITOR_SELECTION_SPAWN
}EditorSelectionKind;

typedef struct{
    EditorSelectionKind kind;
    int index;
    int scene;
}EditorSelectionState;

typedef enum{
    EDITOR_WF_MODE_SESSION=0,
    EDITOR_WF_MODE_MAIN_CHARACTER,
    EDITOR_WF_MODE_SCULPT,
    EDITOR_WF_MODE_FOREST,
    EDITOR_WF_MODE_FENCE,
    EDITOR_WF_MODE_WIND,
    EDITOR_WF_MODE_WORLD_FX,
    EDITOR_WF_MODE_SHADER,
    EDITOR_WF_MODE_PATH,
    EDITOR_WF_MODE_WATER_ZONE,
    EDITOR_WF_MODE_WATERFALL,
    EDITOR_WF_MODE_RIVER,
    EDITOR_WF_MODE_PROP_EDIT,
    EDITOR_WF_MODE_PROP_PLACE,
    EDITOR_WF_MODE_SURFACE_PIECE,
    EDITOR_WF_MODE_COLLISION_VOLUME,
    EDITOR_WF_MODE_MARKER,
    EDITOR_WF_MODE_NPC,
    EDITOR_WF_MODE_SPAWN,
    EDITOR_WF_MODE_QUEST,
    EDITOR_WF_MODE_POINT_LIGHT,
    EDITOR_WF_MODE_DECAL,
    EDITOR_WF_MODE_SOUND_ZONE,
    EDITOR_WF_MODE_INTERIOR_MATERIAL,
    EDITOR_WF_MODE_EXIT
}EditorWorldForgeMode;

typedef enum{
    EDITOR_WF_ACTION_PATH_PREV=0,
    EDITOR_WF_ACTION_PATH_NEXT,
    EDITOR_WF_ACTION_PATH_WIDTH_DEC,
    EDITOR_WF_ACTION_PATH_WIDTH_INC,
    EDITOR_WF_ACTION_PATH_STYLE_PREV,
    EDITOR_WF_ACTION_PATH_STYLE_NEXT,
    EDITOR_WF_ACTION_PATH_NEW,
    EDITOR_WF_ACTION_PATH_SAVE,
    EDITOR_WF_ACTION_PATH_UNDO,
    EDITOR_WF_ACTION_PATH_CLEAR,
    EDITOR_WF_ACTION_RIVER_WIDTH_DEC,
    EDITOR_WF_ACTION_RIVER_WIDTH_INC,
    EDITOR_WF_ACTION_RIVER_STYLE_PREV,
    EDITOR_WF_ACTION_RIVER_STYLE_NEXT,
    EDITOR_WF_ACTION_RIVER_UNDO,
    EDITOR_WF_ACTION_RIVER_CLEAR,
    EDITOR_WF_ACTION_RIVER_SAVE,
    EDITOR_WF_ACTION_WIND_ANGLE_DEC,
    EDITOR_WF_ACTION_WIND_ANGLE_INC,
    EDITOR_WF_ACTION_WIND_RADIUS_DEC,
    EDITOR_WF_ACTION_WIND_RADIUS_INC,
    EDITOR_WF_ACTION_WIND_POWER_DEC,
    EDITOR_WF_ACTION_WIND_POWER_INC,
    EDITOR_WF_ACTION_WIND_GUST_DEC,
    EDITOR_WF_ACTION_WIND_GUST_INC,
    EDITOR_WF_ACTION_WIND_CLEAR,
    EDITOR_WF_ACTION_WIND_SAVE,
    EDITOR_WF_ACTION_MARKER_TYPE_PREV,
    EDITOR_WF_ACTION_MARKER_TYPE_NEXT,
    EDITOR_WF_ACTION_MARKER_SAVE,
    EDITOR_WF_ACTION_MARKER_RADIUS_DEC,
    EDITOR_WF_ACTION_MARKER_RADIUS_INC,
    EDITOR_WF_ACTION_MARKER_DELETE,
    EDITOR_WF_ACTION_MARKER_Y_DEC,
    EDITOR_WF_ACTION_MARKER_Y_INC,
    EDITOR_WF_ACTION_WZONE_RADIUS_DEC,
    EDITOR_WF_ACTION_WZONE_RADIUS_INC,
    EDITOR_WF_ACTION_WZONE_LEVEL_DEC,
    EDITOR_WF_ACTION_WZONE_LEVEL_INC,
    EDITOR_WF_ACTION_WZONE_PRESET_PREV,
    EDITOR_WF_ACTION_WZONE_PRESET_NEXT,
    EDITOR_WF_ACTION_WZONE_WATERFALLS,
    EDITOR_WF_ACTION_WZONE_SAVE,
    EDITOR_WF_ACTION_WZONE_CLEAR,
    EDITOR_WF_ACTION_SPAWN_TYPE_PREV,
    EDITOR_WF_ACTION_SPAWN_TYPE_NEXT,
    EDITOR_WF_ACTION_SPAWN_MODEL_PREV,
    EDITOR_WF_ACTION_SPAWN_MODEL_NEXT,
    EDITOR_WF_ACTION_SPAWN_SAVE,
    EDITOR_WF_ACTION_WF_WIDTH_DEC,
    EDITOR_WF_ACTION_WF_WIDTH_INC,
    EDITOR_WF_ACTION_WF_HEIGHT_DEC,
    EDITOR_WF_ACTION_WF_HEIGHT_INC,
    EDITOR_WF_ACTION_WF_YAW_DEC,
    EDITOR_WF_ACTION_WF_YAW_INC,
    EDITOR_WF_ACTION_WF_BACK_ZONES,
    EDITOR_WF_ACTION_WF_SAVE,
    EDITOR_WF_ACTION_SCULPT_MODE_PREV,
    EDITOR_WF_ACTION_SCULPT_MODE_NEXT,
    EDITOR_WF_ACTION_SCULPT_RADIUS_DEC,
    EDITOR_WF_ACTION_SCULPT_RADIUS_INC,
    EDITOR_WF_ACTION_SCULPT_POWER_DEC,
    EDITOR_WF_ACTION_SCULPT_POWER_INC,
    EDITOR_WF_ACTION_SCULPT_SAVE,
    EDITOR_WF_ACTION_SCULPT_UNDO,
    EDITOR_WF_ACTION_FOREST_BRUSH_PREV,
    EDITOR_WF_ACTION_FOREST_BRUSH_NEXT,
    EDITOR_WF_ACTION_FOREST_SAVE_PRIMARY,
    EDITOR_WF_ACTION_FOREST_RADIUS_DEC,
    EDITOR_WF_ACTION_FOREST_RADIUS_INC,
    EDITOR_WF_ACTION_FOREST_UNDO,
    EDITOR_WF_ACTION_FOREST_DENSITY_DEC,
    EDITOR_WF_ACTION_FOREST_DENSITY_INC,
    EDITOR_WF_ACTION_FOREST_MODEL_PREV,
    EDITOR_WF_ACTION_FOREST_MODEL_RANDOM,
    EDITOR_WF_ACTION_FOREST_MODEL_NEXT,
    EDITOR_WF_ACTION_FOREST_GRASS_SIZE_DEC,
    EDITOR_WF_ACTION_FOREST_GRASS_SIZE_INC,
    EDITOR_WF_ACTION_FOREST_GRASS_STYLE_NEXT,
    EDITOR_WF_ACTION_FOREST_GRASS_TINT_NEXT,
    EDITOR_WF_ACTION_FOREST_GRASS_HMIN_DEC,
    EDITOR_WF_ACTION_FOREST_GRASS_HMIN_INC,
    EDITOR_WF_ACTION_FOREST_GRASS_HMAX_DEC,
    EDITOR_WF_ACTION_FOREST_GRASS_HMAX_INC,
    EDITOR_WF_ACTION_FOREST_FLOWER_SIZE_DEC,
    EDITOR_WF_ACTION_FOREST_FLOWER_SIZE_INC,
    EDITOR_WF_ACTION_FOREST_FLOWER_TYPE_PREV,
    EDITOR_WF_ACTION_FOREST_FLOWER_TYPE_NEXT,
    EDITOR_WF_ACTION_FOREST_WALL_SPACING_DEC,
    EDITOR_WF_ACTION_FOREST_WALL_SPACING_INC,
    EDITOR_WF_ACTION_FOREST_WALL_SCALE_DEC,
    EDITOR_WF_ACTION_FOREST_WALL_SCALE_INC,
    EDITOR_WF_ACTION_FOREST_TERRAIN_POWER_DEC,
    EDITOR_WF_ACTION_FOREST_TERRAIN_POWER_INC,
    EDITOR_WF_ACTION_FOREST_TERRAIN_STYLE_NEXT,
    EDITOR_WF_ACTION_NPC_TYPE_PREV,
    EDITOR_WF_ACTION_NPC_TYPE_NEXT,
    EDITOR_WF_ACTION_NPC_SELECT_PREV,
    EDITOR_WF_ACTION_NPC_SELECT_NEXT,
    EDITOR_WF_ACTION_NPC_SELECT_CLEAR,
    EDITOR_WF_ACTION_NPC_DELETE,
    EDITOR_WF_ACTION_NPC_SAVE,
    EDITOR_WF_ACTION_NPC_YAW_DEC,
    EDITOR_WF_ACTION_NPC_YAW_INC,
    EDITOR_WF_ACTION_NPC_SPEED_DEC,
    EDITOR_WF_ACTION_NPC_SPEED_INC,
    EDITOR_WF_ACTION_NPC_WAIT_DEC,
    EDITOR_WF_ACTION_NPC_WAIT_INC,
    EDITOR_WF_ACTION_NPC_MERCHANT_TOGGLE,
    EDITOR_WF_ACTION_NPC_SHOP_PREV,
    EDITOR_WF_ACTION_NPC_SHOP_NEXT,
    EDITOR_WF_ACTION_NPC_WAYPOINT_TOGGLE,
    EDITOR_WF_ACTION_NPC_WAYPOINT_REMOVE_LAST,
    EDITOR_WF_ACTION_NPC_DIALOGUE_OPEN,
    EDITOR_WF_ACTION_NPC_DIALOGUE_CLOSE,
    EDITOR_WF_ACTION_NPC_DIALOGUE_PREV,
    EDITOR_WF_ACTION_NPC_DIALOGUE_NEXT,
    EDITOR_WF_ACTION_NPC_DIALOGUE_COMMIT,
    EDITOR_WF_ACTION_NPC_DIALOGUE_DELETE,
    EDITOR_WF_ACTION_NPC_TREE_OPEN,
    EDITOR_WF_ACTION_NPC_TREE_CLOSE,
    EDITOR_WF_ACTION_NPC_TREE_PREV,
    EDITOR_WF_ACTION_NPC_TREE_NEXT,
    EDITOR_WF_ACTION_NPC_TREE_ADD,
    EDITOR_WF_ACTION_NPC_TREE_DELETE,
    EDITOR_WF_ACTION_NPC_TREE_ROOT,
    EDITOR_WF_ACTION_NPC_TREE_CHOICE_PREV,
    EDITOR_WF_ACTION_NPC_TREE_CHOICE_NEXT,
    EDITOR_WF_ACTION_NPC_TREE_CHOICE_ADD,
    EDITOR_WF_ACTION_NPC_TREE_CHOICE_DELETE,
    EDITOR_WF_ACTION_FX_TAB_WORLD,
    EDITOR_WF_ACTION_FX_TAB_WEATHER,
    EDITOR_WF_ACTION_FX_TAB_POST,
    EDITOR_WF_ACTION_FX_PRESET_PREV,
    EDITOR_WF_ACTION_FX_PRESET_NEXT,
    EDITOR_WF_ACTION_FX_SAVE,
    EDITOR_WF_ACTION_FX_SCALE_DEC,
    EDITOR_WF_ACTION_FX_SCALE_INC,
    EDITOR_WF_ACTION_FX_LIGHT_TOGGLE,
    EDITOR_WF_ACTION_FX_SHADER_TOGGLE,
    EDITOR_WF_ACTION_FX_BLOOM_TOGGLE,
    EDITOR_WF_ACTION_FX_HEIGHT_DEC,
    EDITOR_WF_ACTION_FX_HEIGHT_INC,
    EDITOR_WF_ACTION_FX_GLOW_DEC,
    EDITOR_WF_ACTION_FX_GLOW_INC,
    EDITOR_WF_ACTION_FX_LIGHT_RADIUS_DEC,
    EDITOR_WF_ACTION_FX_LIGHT_RADIUS_INC,
    EDITOR_WF_ACTION_FX_COLOR_R_DEC,
    EDITOR_WF_ACTION_FX_COLOR_R_INC,
    EDITOR_WF_ACTION_FX_COLOR_G_DEC,
    EDITOR_WF_ACTION_FX_COLOR_G_INC,
    EDITOR_WF_ACTION_FX_COLOR_B_DEC,
    EDITOR_WF_ACTION_FX_COLOR_B_INC,
    EDITOR_WF_ACTION_FX_MOVE_FWD,
    EDITOR_WF_ACTION_FX_MOVE_BACK,
    EDITOR_WF_ACTION_FX_MOVE_LEFT,
    EDITOR_WF_ACTION_FX_MOVE_RIGHT,
    EDITOR_WF_ACTION_FX_MOVE_UP,
    EDITOR_WF_ACTION_FX_MOVE_DOWN,
    EDITOR_WF_ACTION_FX_WEATHER_AUTO,
    EDITOR_WF_ACTION_FX_WEATHER_RAIN,
    EDITOR_WF_ACTION_FX_WEATHER_SNOW,
    EDITOR_WF_ACTION_FX_WEATHER_OFF,
    EDITOR_WF_ACTION_FX_WEATHER_TOGGLE,
    EDITOR_WF_ACTION_FX_RATE_DEC,
    EDITOR_WF_ACTION_FX_RATE_INC,
    EDITOR_WF_ACTION_FX_WET_DEC,
    EDITOR_WF_ACTION_FX_WET_INC,
    EDITOR_WF_ACTION_FX_FOG_DEC,
    EDITOR_WF_ACTION_FX_FOG_INC,
    EDITOR_WF_ACTION_FX_CLOUD_DEC,
    EDITOR_WF_ACTION_FX_CLOUD_INC,
    EDITOR_WF_ACTION_FX_EXPOSURE_DEC,
    EDITOR_WF_ACTION_FX_EXPOSURE_INC,
    EDITOR_WF_ACTION_FX_POST_BLOOM_DEC,
    EDITOR_WF_ACTION_FX_POST_BLOOM_INC,
    EDITOR_WF_ACTION_FX_SSAO_DEC,
    EDITOR_WF_ACTION_FX_SSAO_INC,
    EDITOR_WF_ACTION_FX_POST_APPLY,
    EDITOR_WF_ACTION_FX_TOD_DEC,
    EDITOR_WF_ACTION_FX_TOD_INC,
    EDITOR_WF_ACTION_FX_TOD_NOON,
    EDITOR_WF_ACTION_FX_FXAA_TOGGLE,
    EDITOR_WF_ACTION_VISUAL_LOOK_NATURAL,
    EDITOR_WF_ACTION_VISUAL_LOOK_CINEMATIC,
    EDITOR_WF_ACTION_VISUAL_LOOK_STORM,
    EDITOR_WF_ACTION_VISUAL_LOOK_LOW_LIGHT,
    EDITOR_WF_ACTION_VISUAL_RESET_PROFILE,
    EDITOR_WF_ACTION_VISUAL_SAVE_PROFILE,
    EDITOR_WF_ACTION_SHADER_PREV,
    EDITOR_WF_ACTION_SHADER_NEXT,
    EDITOR_WF_ACTION_SHADER_TOGGLE,
    EDITOR_WF_ACTION_SHADER_RELOAD,
    EDITOR_WF_ACTION_SHADER_RESCAN,
    EDITOR_WF_ACTION_SHADER_APPLY,
    EDITOR_WF_ACTION_PROP_MODE_SELECT,
    EDITOR_WF_ACTION_PROP_MODE_MOVE,
    EDITOR_WF_ACTION_PROP_MODE_ROTATE,
    EDITOR_WF_ACTION_PROP_MODE_SCALE,
    EDITOR_WF_ACTION_GIZMO_SPACE_TOGGLE,
    EDITOR_WF_ACTION_GIZMO_SNAP_TOGGLE,
    EDITOR_WF_ACTION_PROP_Y_DEC,
    EDITOR_WF_ACTION_PROP_Y_INC,
    EDITOR_WF_ACTION_PROP_GROUND,
    EDITOR_WF_ACTION_PROP_SAVE,
    EDITOR_WF_ACTION_PROP_SNAP_TOGGLE,
    EDITOR_WF_ACTION_PROP_DELETE,
    EDITOR_WF_ACTION_PROP_UNDO,
    EDITOR_WF_ACTION_PROP_COLL_NONE,
    EDITOR_WF_ACTION_PROP_COLL_AABB,
    EDITOR_WF_ACTION_PROP_COLL_MESH,
    EDITOR_WF_ACTION_PLACER_ROT_DEC,
    EDITOR_WF_ACTION_PLACER_ROT_INC,
    EDITOR_WF_ACTION_PLACER_SCALE_DEC,
    EDITOR_WF_ACTION_PLACER_SCALE_INC,
    EDITOR_WF_ACTION_PLACER_SNAP_TOGGLE,
    EDITOR_WF_ACTION_PLACER_SAVE,
    EDITOR_WF_ACTION_PLACER_RESCAN,
    EDITOR_WF_ACTION_PLACER_MODE_TOGGLE,
    EDITOR_WF_ACTION_PLACER_COLL_NONE,
    EDITOR_WF_ACTION_PLACER_COLL_AABB,
    EDITOR_WF_ACTION_PLACER_COLL_MESH,
    EDITOR_WF_ACTION_QUEST_ADD,
    EDITOR_WF_ACTION_QUEST_DELETE,
    EDITOR_WF_ACTION_QUEST_SAVE,
    EDITOR_WF_ACTION_QUEST_PREV,
    EDITOR_WF_ACTION_QUEST_NEXT,
    EDITOR_WF_ACTION_QUEST_OBJ_ADD_KILL,
    EDITOR_WF_ACTION_QUEST_OBJ_ADD_REACH,
    EDITOR_WF_ACTION_QUEST_OBJ_DELETE,
    EDITOR_WF_ACTION_QUEST_OBJ_PREV,
    EDITOR_WF_ACTION_QUEST_OBJ_NEXT,
    /* Point Light editor */
    EDITOR_WF_ACTION_PLIGHT_RADIUS_DEC,
    EDITOR_WF_ACTION_PLIGHT_RADIUS_INC,
    EDITOR_WF_ACTION_PLIGHT_INTENSITY_DEC,
    EDITOR_WF_ACTION_PLIGHT_INTENSITY_INC,
    EDITOR_WF_ACTION_PLIGHT_R_DEC,
    EDITOR_WF_ACTION_PLIGHT_R_INC,
    EDITOR_WF_ACTION_PLIGHT_G_DEC,
    EDITOR_WF_ACTION_PLIGHT_G_INC,
    EDITOR_WF_ACTION_PLIGHT_B_DEC,
    EDITOR_WF_ACTION_PLIGHT_B_INC,
    EDITOR_WF_ACTION_PLIGHT_Y_DEC,
    EDITOR_WF_ACTION_PLIGHT_Y_INC,
    EDITOR_WF_ACTION_PLIGHT_FLICKER_TOGGLE,
    EDITOR_WF_ACTION_PLIGHT_DELETE,
    EDITOR_WF_ACTION_PLIGHT_SAVE,
    /* Decal editor */
    EDITOR_WF_ACTION_DECAL_TYPE_PREV,
    EDITOR_WF_ACTION_DECAL_TYPE_NEXT,
    EDITOR_WF_ACTION_DECAL_SIZE_DEC,
    EDITOR_WF_ACTION_DECAL_SIZE_INC,
    EDITOR_WF_ACTION_DECAL_ALPHA_DEC,
    EDITOR_WF_ACTION_DECAL_ALPHA_INC,
    EDITOR_WF_ACTION_DECAL_YAW_DEC,
    EDITOR_WF_ACTION_DECAL_YAW_INC,
    EDITOR_WF_ACTION_DECAL_DELETE,
    EDITOR_WF_ACTION_DECAL_SAVE,
    /* Sound Zone editor */
    EDITOR_WF_ACTION_SZONE_SOUND_PREV,
    EDITOR_WF_ACTION_SZONE_SOUND_NEXT,
    EDITOR_WF_ACTION_SZONE_RADIUS_DEC,
    EDITOR_WF_ACTION_SZONE_RADIUS_INC,
    EDITOR_WF_ACTION_SZONE_VOL_DEC,
    EDITOR_WF_ACTION_SZONE_VOL_INC,
    EDITOR_WF_ACTION_SZONE_DELETE,
    EDITOR_WF_ACTION_SZONE_SAVE,
    EDITOR_WF_ACTION_INTERIOR_TARGET_PREV,
    EDITOR_WF_ACTION_INTERIOR_TARGET_NEXT,
    EDITOR_WF_ACTION_INTERIOR_MATERIAL_PREV,
    EDITOR_WF_ACTION_INTERIOR_MATERIAL_NEXT,
    EDITOR_WF_ACTION_INTERIOR_SAVE,
    EDITOR_WF_ACTION_SELECTION_CLEAR,
    EDITOR_WF_ACTION_SELECTION_FRAME,
    EDITOR_WF_ACTION_SELECTION_DELETE,
    EDITOR_WF_ACTION_SELECTION_Y_DEC,
    EDITOR_WF_ACTION_SELECTION_Y_INC,
    EDITOR_WF_ACTION_SELECTION_GROUND,
    EDITOR_WF_ACTION_SELECTION_MOVE_FWD,
    EDITOR_WF_ACTION_SELECTION_MOVE_BACK,
    EDITOR_WF_ACTION_SELECTION_MOVE_LEFT,
    EDITOR_WF_ACTION_SELECTION_MOVE_RIGHT,
    EDITOR_WF_ACTION_SELECTION_DUPLICATE,
    EDITOR_WF_ACTION_SELECTION_DUPLICATE_FWD,
    EDITOR_WF_ACTION_SELECTION_DUPLICATE_RIGHT,
    EDITOR_WF_ACTION_SELECTION_DUPLICATE_GROUNDED,
    EDITOR_WF_ACTION_SELECTION_PREV,
    EDITOR_WF_ACTION_SELECTION_NEXT,
    EDITOR_WF_ACTION_SELECTION_YAW_DEC,
    EDITOR_WF_ACTION_SELECTION_YAW_INC,
    EDITOR_WF_ACTION_SELECTION_SCALE_DEC,
    EDITOR_WF_ACTION_SELECTION_SCALE_INC,
    EDITOR_WF_ACTION_SELECTION_SNAP_GRID,
    EDITOR_WF_ACTION_SELECTION_FACE_CAMERA
}EditorWorldForgeAction;

#define EDITOR_UI_NPC_DIALOGUE_MAX 8
#define EDITOR_UI_NPC_TREE_CHOICES_MAX 4

typedef struct{
    EditorUIBackend backend;
    bool imgui_compiled;
    bool imgui_requested;
}EditorUIState;

typedef struct{
    void (*rect)(void *ctx,float x,float y,float w,float h,float r,float g,float b,float a);
    void (*text)(void *ctx,float x,float y,float scale,const char *text,float r,float g,float b,float a);
    void (*title)(void *ctx,float x,float y,float scale,const char *text,float r,float g,float b,float a);
}EditorUIRenderAPI;

typedef struct{
    bool editor_mode;
    bool path_edit;
    bool wzone_edit;
    bool wf_edit_mode;
    bool river_edit;
    bool river_has_selection;
    int river_selected_point;
    float river_width;
    float river_bank_softness;
    float river_point_x;
    float river_point_z;
    float river_point_width_scale;
    float river_point_depth_scale;
    float river_point_flow_speed;
    float river_point_foam;
    bool prop_edit;
    bool surface_edit;
    bool collision_edit;
    bool particle_fx_edit;
    bool marker_edit;
    bool forest_edit;
    bool wind_edit;
    bool sculpt_edit;
    bool npc_edit_mode;
    bool spawn_edit_mode;
    bool shader_edit;
    bool prop_place_mode;
    int prop_count;
    int marker_count;
    int npc_count;
    int spawn_count;
    const char *active_mode_text;
    const char *editor_ui_text;
    const char *world_content_text;
    const char *population_text;
    const char *inspect_label;
    const char *inspect_line_a;
    const char *inspect_line_b;
    const char *inspect_line_c;
    const char *tool_title;
    const char *tool_line_a;
    const char *tool_line_b;
    const char *tool_line_c;
    int forest_brush_kind;
    float tree_brush_scale;
    float wall_brush_spacing;
    float wall_brush_scale;
    bool npc_place_mode;
    int npc_sel_type;
    int npc_type_count;
    const char *npc_type_names[EDITOR_UI_NPC_TYPE_MAX];
    bool npc_type_loaded[EDITOR_UI_NPC_TYPE_MAX];
    int npc_type_placed_counts[EDITOR_UI_NPC_TYPE_MAX];
    float npc_type_r[EDITOR_UI_NPC_TYPE_MAX];
    float npc_type_g[EDITOR_UI_NPC_TYPE_MAX];
    float npc_type_b[EDITOR_UI_NPC_TYPE_MAX];
    bool npc_has_selection;
    int npc_selected_index;
    bool npc_has_focus;
    int npc_focus_index;
    const char *npc_focus_name;
    char *npc_focus_name_buffer;
    int npc_focus_name_capacity;
    char *npc_focus_backstory_buffer;
    int npc_focus_backstory_capacity;
    int npc_focus_model_idx;
    float npc_focus_scale;
    float npc_focus_visual_yaw_offset;
    bool npc_focus_collision_enabled;
    bool npc_focus_patrol_loop;
    bool npc_waypoint_mode;
    int npc_waypoint_count;
    float npc_waypoint_x[EDITOR_UI_NPC_WAYPOINT_MAX];
    float npc_waypoint_y[EDITOR_UI_NPC_WAYPOINT_MAX];
    float npc_waypoint_z[EDITOR_UI_NPC_WAYPOINT_MAX];
    float npc_patrol_speed;
    float npc_patrol_wait;
    bool npc_is_merchant;
    int npc_shop_id;
    const char *npc_shop_name;
    bool npc_dialogue_mode;
    int npc_dialogue_count;
    int npc_dialogue_sel;
    char *npc_dialogue_buffer;
    int npc_dialogue_buffer_capacity;
    const char *npc_dialogue_lines[EDITOR_UI_NPC_DIALOGUE_MAX];
    bool npc_tree_mode;
    int npc_tree_count;
    int npc_tree_sel;
    int npc_tree_root;
    int npc_tree_choice_sel;
    int npc_tree_choice_count;
    char *npc_tree_text;
    int npc_tree_text_capacity;
    char *npc_tree_speaker;
    int npc_tree_speaker_capacity;
    int *npc_tree_anim_trigger;
    char *npc_tree_choice_labels[EDITOR_UI_NPC_TREE_CHOICES_MAX];
    int *npc_tree_choice_next[EDITOR_UI_NPC_TREE_CHOICES_MAX];
    /* Dialogue graph canvas: direct r/w access to per-node canvas positions */
    float *npc_tree_node_canvas_x;    /* float[EDITOR_UI_NPC_TREE_MAX] owned by NpcSpawnPoint */
    float *npc_tree_node_canvas_y;
    /* Snapshot of every node's topology/content for graph rendering (read-only from ImGui) */
    int   npc_tree_all_choice_count[EDITOR_UI_NPC_TREE_MAX];
    int   npc_tree_all_choice_next[EDITOR_UI_NPC_TREE_MAX][4];
    char  npc_tree_all_choice_labels[EDITOR_UI_NPC_TREE_MAX][4][64];
    char  npc_tree_all_text[EDITOR_UI_NPC_TREE_MAX][48];
    char  npc_tree_all_speaker[EDITOR_UI_NPC_TREE_MAX][32];
    bool quest_edit_mode;
    bool main_character_edit;
    int quest_count;
    int quest_sel;
    char *quest_name_buf;
    int quest_name_buf_cap;
    char *quest_desc_buf;
    int quest_desc_buf_cap;
    int *quest_gold_ptr;
    int *quest_xp_ptr;
    int quest_obj_count;
    int quest_obj_sel;
    char *quest_obj_desc_buf;
    int quest_obj_desc_buf_cap;
    int quest_obj_type;
    int *quest_obj_required_ptr;
    int *quest_obj_etype_ptr;
    float quest_obj_loc_x;
    float quest_obj_loc_y;
    float quest_obj_loc_z;
    float quest_obj_radius;
    int player_char_preset_count;
    int player_char_preset_selected;
    int player_char_active_preset;
    const char *player_char_preset_labels[32];
    char *player_char_name_buf;
    int player_char_name_cap;
    const char *player_char_model_path;
    const char *player_char_model_label;
    float player_char_scale;
    int player_char_model_count;
    const char *player_char_model_labels[32];
    const char *player_char_model_paths[32];
    int player_char_anim_count;
    const char *player_char_anim_labels[128];
    int player_char_slot_selected[PLAYER_CHAR_SLOT_COUNT];
    int player_char_sound_count;
    const char *player_char_sound_labels[128];
    int player_char_slot_sound_selected[PLAYER_CHAR_SLOT_COUNT];
    bool plight_edit;
    bool plight_has_selection;
    int plight_selected_index;
    float plight_pos_x;
    float plight_pos_y;
    float plight_pos_z;
    float plight_color_r;
    float plight_color_g;
    float plight_color_b;
    int plight_type_selected;
    float plight_spot_yaw;
    float plight_spot_pitch;
    float plight_spot_inner_deg;
    float plight_spot_outer_deg;
    bool world_light_override;
    float world_light_yaw;
    float world_light_pitch;
    float world_light_sun_r;
    float world_light_sun_g;
    float world_light_sun_b;
    float world_light_ambient_r;
    float world_light_ambient_g;
    float world_light_ambient_b;
    float plight_radius;
    float plight_intensity;
    float plight_y_offset;
    bool plight_flicker;
    float plight_flicker_speed;
    bool decal_edit;
    bool szone_edit;
    bool szone_has_selection;
    int szone_selected_index;
    float szone_pos_x;
    float szone_pos_z;
    int szone_type_selected;
    int szone_ambient_sound_selected;
    int szone_music_selected;
    float szone_radius;
    float szone_volume;
    float szone_fade_length;
    int szone_ambient_sound_count;
    const char *szone_ambient_sound_labels[32];
    int szone_music_count;
    const char *szone_music_labels[128];
    bool interior_edit;
    bool surface_has_selection;
    int surface_kind;
    float surface_width;
    float surface_height;
    float surface_thickness;
    const char *surface_coll_name;
    int surface_coll_selected;
    int surface_material_count;
    int surface_material_selected;
    const char *surface_material_labels[64];
    int surface_material_indices[64];
    int interior_material_count;
    int interior_material_selected;
    const char *interior_material_labels[64];
    int interior_material_indices[64];
    bool collision_has_selection;
    float collision_width;
    float collision_height;
    float collision_depth;
    const char *collision_shape_name;
    int collision_shape_selected;
    float collision_yaw;
    float collision_pitch;
    bool collision_enabled;
    bool collision_nav_blocker;
    bool collision_motion_enabled;
    float collision_motion_x;
    float collision_motion_y;
    float collision_motion_z;
    float collision_motion_period;
    float collision_motion_phase;
    EditorSelectionState selection;
    bool prop_has_selection;
    const char *prop_name;
    int prop_transform_mode;
    bool gizmo_local_space;
    bool gizmo_snap_enabled;
    bool prop_snap_y;
    float prop_pos_x;
    float prop_pos_y;
    float prop_pos_z;
    float prop_rot_y;
    float prop_scale;
    const char *prop_coll_name;
    int prop_coll_selected;
    bool prop_custom_coll_enabled;
    int prop_custom_coll_count;
    int prop_custom_coll_selected;
    bool prop_custom_box_gizmo_mode;
    float prop_custom_box_center_x;
    float prop_custom_box_center_y;
    float prop_custom_box_center_z;
    int prop_custom_box_shape;
    float prop_custom_box_half_x;
    float prop_custom_box_half_y;
    float prop_custom_box_half_z;
    float prop_custom_box_yaw;
    bool prop_pickable;
    int prop_pickup_item;
    int prop_pickup_count;
    int prop_pickup_item_count;
    const char *prop_pickup_item_names[64];
    const char *prop_interact_name;
    int prop_interact_type;
    float prop_interact_radius;
    float prop_door_open_angle;
    int prop_action_count;
    int prop_action_selected;
    int prop_action_type;
    const char *prop_action_name;
    float prop_action_radius;
    int prop_action_target_index;
    const char *prop_action_target_label;
    int prop_action_target_count;
    int prop_action_target_ids[128];
    const char *prop_action_target_labels[128];
    int prop_action_sound_idx;
    float prop_action_sound_volume;
    const char *prop_action_sound_name;
    int prop_action_sound_count;
    const char *prop_action_sound_names[32];
    char *prop_action_sound_path;
    int prop_action_sound_path_capacity;
    int prop_action_condition_type;
    const char *prop_action_condition_name;
    int prop_action_condition_flag;
    char *prop_action_prompt_text;
    int prop_action_prompt_text_capacity;
    bool placer_prefab_mode;
    char *placer_search_buffer;
    int placer_search_buffer_capacity;
    int placer_item_count;
    int placer_item_selected;
    bool placer_snap_y;
    float placer_rotation_y;
    float placer_scale;
    const char *placer_coll_name;
    const char *placer_item_labels[256];
    unsigned int placer_item_thumb_ids[256];
    bool placer_item_favorite[256];
    bool placer_item_recent[256];
    bool placer_item_valid[256];
    int placer_item_category[256];
    bool browser_favorites_only;
    bool browser_recent_only;
    int browser_category_filter;
    const char *browser_selected_category;
    const char *browser_selected_tags;
    const char *browser_selected_status;
    bool browser_selected_favorite;
    bool browser_selected_recent;
    int multi_prop_count;
    int multi_prop_clipboard_count;
    float bulk_scatter_radius;
    int bulk_scatter_count;
    float *bulk_scatter_radius_ptr;
    int *bulk_scatter_count_ptr;
    float bulk_path_spacing;
    float *bulk_path_spacing_ptr;
    int prefab_library_count;
    int prefab_selected_index;
    const char *prefab_selected_name;
    int prefab_selected_prop_count;
    int prefab_selected_fx_count;
    float prefab_capture_radius;
    float *prefab_capture_radius_ptr;
    bool prefab_capture_has_anchor;
    bool prefab_capture_uses_multiselect;
    char *prefab_name_buffer;
    int prefab_name_buffer_capacity;
    void *action_ctx;
    void (*activate_mode)(void *ctx,EditorWorldForgeMode mode);
    void (*save_world)(void *ctx);
    void (*save_slot)(void *ctx,int slot);
    void (*player_char_select_preset)(void *ctx,int preset_idx);
    void (*player_char_add_preset)(void *ctx);
    void (*player_char_duplicate_preset)(void *ctx);
    void (*player_char_delete_preset)(void *ctx);
    void (*player_char_set_active_preset)(void *ctx);
    void (*player_char_commit_name)(void *ctx,const char *name);
    void (*player_char_set_model)(void *ctx,int model_idx);
    void (*player_char_set_scale)(void *ctx,float scale);
    void (*player_char_set_anim_slot)(void *ctx,int slot,int anim_idx);
    void (*player_char_set_anim_sound_slot)(void *ctx,int slot,int sound_idx);
    void (*player_char_save)(void *ctx);
    void (*tool_action)(void *ctx,EditorWorldForgeAction action);
    void (*surface_material_select)(void *ctx,int index);
    void (*surface_set_kind)(void *ctx,int kind);
    void (*surface_set_width)(void *ctx,float value);
    void (*surface_set_height)(void *ctx,float value);
    void (*surface_set_thickness)(void *ctx,float value);
    void (*surface_set_collision)(void *ctx,int coll_type);
    void (*collision_set_width)(void *ctx,float value);
    void (*collision_set_height)(void *ctx,float value);
    void (*collision_set_depth)(void *ctx,float value);
    void (*collision_set_shape)(void *ctx,int shape);
    void (*collision_set_yaw)(void *ctx,float value);
    void (*collision_set_pitch)(void *ctx,float value);
    void (*collision_set_enabled)(void *ctx,bool enabled);
    void (*collision_set_nav_blocker)(void *ctx,bool enabled);
    void (*collision_set_motion_enabled)(void *ctx,bool enabled);
    void (*collision_set_motion_axis)(void *ctx,int axis,float value);
    void (*collision_set_motion_period)(void *ctx,float value);
    void (*collision_set_motion_phase)(void *ctx,float value);
    void (*plight_set_position)(void *ctx,int axis,float value);
    void (*plight_set_color)(void *ctx,int channel,float value);
    void (*plight_set_type)(void *ctx,int type);
    void (*world_light_set_override)(void *ctx,bool enabled);
    void (*world_light_set_angles)(void *ctx,float yaw_deg,float pitch_deg);
    void (*world_light_set_sun_color)(void *ctx,int channel,float value);
    void (*world_light_set_ambient_color)(void *ctx,int channel,float value);
    void (*plight_set_radius)(void *ctx,float value);
    void (*plight_set_intensity)(void *ctx,float value);
    void (*plight_set_y_offset)(void *ctx,float value);
    void (*plight_set_flicker)(void *ctx,bool enabled);
    void (*plight_set_flicker_speed)(void *ctx,float value);
    void (*plight_set_spot_angles)(void *ctx,float yaw_deg,float pitch_deg);
    void (*plight_set_spot_cone)(void *ctx,float inner_deg,float outer_deg);
    void (*plight_delete_selected)(void *ctx);
    void (*szone_set_type)(void *ctx,int type);
    void (*szone_set_position)(void *ctx,int axis,float value);
    void (*szone_set_ambient_sound)(void *ctx,int sound_idx);
    void (*szone_set_music)(void *ctx,int music_idx);
    void (*szone_set_radius)(void *ctx,float radius);
    void (*szone_set_volume)(void *ctx,float volume);
    void (*szone_set_fade_length)(void *ctx,float fade_length);
    void (*szone_delete_selected)(void *ctx);
    void (*interior_material_select)(void *ctx,int index);
    void (*browser_refresh)(void *ctx);
    void (*browser_filter)(void *ctx);
    bool (*browser_import)(void *ctx,const char *source,char *message,size_t capacity);
    void (*browser_select)(void *ctx,int index,bool prefab_mode);
    void (*browser_toggle_favorite)(void *ctx);
    void (*browser_cycle_category_filter)(void *ctx);
    void (*browser_toggle_favorites_only)(void *ctx);
    void (*browser_toggle_recent_only)(void *ctx);
    void (*prefab_capture_selection)(void *ctx);
    void (*prefab_delete_selected)(void *ctx);
    void (*prefab_duplicate_selected)(void *ctx);
    void (*prefab_commit_name)(void *ctx);
    void (*prefab_save_library)(void *ctx);
    void (*bulk_align_x)(void *ctx);
    void (*bulk_align_y)(void *ctx);
    void (*bulk_align_z)(void *ctx);
    void (*bulk_distribute_x)(void *ctx);
    void (*bulk_distribute_z)(void *ctx);
    void (*bulk_replace_selection)(void *ctx);
    void (*bulk_copy_selection)(void *ctx);
    void (*bulk_paste_selection)(void *ctx);
    void (*bulk_duplicate_selection)(void *ctx);
    void (*bulk_select_all)(void *ctx);
    void (*bulk_clear_selection)(void *ctx);
    void (*bulk_scatter_current)(void *ctx);
    void (*bulk_stamp_path_current)(void *ctx);
    void (*prop_set_rotation_y)(void *ctx,float yaw_deg);
    void (*prop_set_scale)(void *ctx,float scale);
    void (*prop_set_collision_mode)(void *ctx,int coll_type);
    void (*prop_set_custom_collision_enabled)(void *ctx,bool enabled);
    void (*prop_set_custom_collision_box_gizmo_mode)(void *ctx,bool enabled);
    void (*prop_add_custom_collision_box)(void *ctx);
    void (*prop_remove_custom_collision_box)(void *ctx);
    void (*prop_select_custom_collision_box)(void *ctx,int box_idx);
    void (*prop_set_custom_collision_box_shape)(void *ctx,int box_idx,int shape);
    void (*prop_set_custom_collision_box_center)(void *ctx,int box_idx,int axis,float value);
    void (*prop_set_custom_collision_box_half)(void *ctx,int box_idx,int axis,float value);
    void (*prop_set_custom_collision_box_yaw)(void *ctx,int box_idx,float value);
    void (*prop_set_pickable)(void *ctx,bool pickable);
    void (*prop_set_pickup_item)(void *ctx,int item_type);
    void (*prop_set_pickup_count)(void *ctx,int count);
    void (*prop_set_interact_type)(void *ctx,int interact_type);
    void (*prop_set_interact_radius)(void *ctx,float radius);
    void (*prop_set_door_open_angle)(void *ctx,float angle_deg);
    void (*prop_add_action)(void *ctx);
    void (*prop_remove_action)(void *ctx);
    void (*prop_select_action)(void *ctx,int action_idx);
    void (*prop_set_action_type)(void *ctx,int action_idx,int action_type);
    void (*prop_set_action_radius)(void *ctx,int action_idx,float radius);
    void (*prop_set_action_target_index)(void *ctx,int action_idx,int target_index);
    void (*prop_set_action_sound_idx)(void *ctx,int action_idx,int sound_idx);
    void (*prop_set_action_sound_volume)(void *ctx,int action_idx,float volume);
    void (*prop_set_action_sound_path)(void *ctx,int action_idx,const char *path);
    void (*prop_set_action_condition_type)(void *ctx,int action_idx,int condition_type);
    void (*prop_set_action_condition_flag)(void *ctx,int action_idx,int condition_flag);
    void (*prop_set_action_prompt_text)(void *ctx,int action_idx,const char *text);
    void (*marker_set_kind)(void *ctx,int kind);
    void (*marker_set_radius)(void *ctx,float radius);
    void (*marker_set_y)(void *ctx,float y);
    void (*spawn_set_model_idx)(void *ctx,int model_idx);
    void (*spawn_set_model_scale)(void *ctx,float scale);
    void (*spawn_set_hp_override)(void *ctx,float hp_over);
    void (*spawn_set_damage_override)(void *ctx,float dmg_over);
    void (*spawn_set_speed_override)(void *ctx,float speed_over);
    void (*spawn_set_chase_radius_override)(void *ctx,float chase_r_over);
    void (*spawn_set_attack_radius_override)(void *ctx,float atk_r_over);
    void (*spawn_set_attack_cooldown_override)(void *ctx,float atk_cd_over);
    void (*spawn_set_scale_override)(void *ctx,float scale_over);
    void (*river_set_width)(void *ctx,float width);
    void (*river_set_bank_softness)(void *ctx,float softness);
    void (*river_set_selected_point_position)(void *ctx,int axis,float value);
    void (*river_set_selected_point_width_scale)(void *ctx,float value);
    void (*river_set_selected_point_depth_scale)(void *ctx,float value);
    void (*river_set_selected_point_flow_speed)(void *ctx,float value);
    void (*river_set_selected_point_foam)(void *ctx,float value);
    void (*forest_set_tree_scale)(void *ctx,float scale);
    void (*forest_set_wall_spacing)(void *ctx,float spacing);
    void (*forest_set_wall_scale)(void *ctx,float scale);
    void (*npc_set_place_mode)(void *ctx,bool enabled);
    void (*npc_set_focus_model_index)(void *ctx,int model_idx);
    void (*npc_set_focus_scale)(void *ctx,float scale);
    void (*npc_set_focus_visual_yaw_offset)(void *ctx,float yaw_offset);
    void (*npc_set_focus_collision_enabled)(void *ctx,bool enabled);
    void (*npc_set_focus_patrol_loop)(void *ctx,bool enabled);
    void (*npc_set_focus_patrol_speed)(void *ctx,float speed);
    void (*npc_set_focus_patrol_wait)(void *ctx,float wait_time);
    void (*npc_set_focus_merchant_enabled)(void *ctx,bool enabled);
    void (*npc_set_focus_shop_id)(void *ctx,int shop_id);
    void (*npc_set_dialogue_selection)(void *ctx,int index);
    void (*npc_commit_dialogue_line)(void *ctx);
    void (*npc_delete_dialogue_line)(void *ctx);
    void (*npc_commit_focus_name)(void *ctx,const char *name);
    void (*npc_commit_focus_backstory)(void *ctx,const char *backstory);
    bool marker_has_selection;
    int marker_kind;
    float marker_radius;
    float marker_pos_y;
    /* Spawn inspector */
    bool spawn_has_selection;
    const char *spawn_sel_type_name;
    int spawn_model_count;
    const char *spawn_model_names[32];
    int *spawn_sel_model_idx;
    float *spawn_model_scale; /* per-model scale multiplier, NULL if default model selected */
    float *spawn_sel_hp;
    float *spawn_sel_dmg;
    float *spawn_sel_speed;
    float *spawn_sel_chase_r;
    float *spawn_sel_atk_r;
    float *spawn_sel_atk_cd;
    float *spawn_sel_scale;
    float spawn_def_hp;
    float spawn_def_dmg;
    float spawn_def_speed;
    float spawn_def_chase_r;
    float spawn_def_atk_r;
    float spawn_def_atk_cd;
    float spawn_def_scale;
    int outliner_count;
    const char *outliner_labels[256];
    EditorSelectionKind outliner_kinds[256];
    int outliner_indices[256];
    void (*selection_set)(void *ctx,EditorSelectionKind kind,int index,bool frame);
}EditorWorldForgeShellState;

void editor_ui_init(EditorUIState *state);
const char *editor_ui_backend_name(const EditorUIState *state);
bool editor_ui_backend_is_imgui(const EditorUIState *state);
bool editor_ui_startup(EditorUIState *state,void *window,void *gl_context);
void editor_ui_shutdown(EditorUIState *state);
void editor_ui_begin_frame(const EditorUIState *state);
/* Consume input while the game HUD is hidden, without desktop mouse queries. */
void editor_ui_begin_passive_frame(const EditorUIState *state,float seconds,int width,int height);
void editor_ui_render(const EditorUIState *state);
bool editor_ui_process_event(const EditorUIState *state,const SDL_Event *event);
bool editor_ui_wants_mouse(const EditorUIState *state);
bool editor_ui_wants_keyboard(const EditorUIState *state);
void editor_ui_draw_world_forge_shell(const EditorUIState *ui,
                                      const EditorUIRenderAPI *api,
                                      void *ctx,
                                      int win_w,int win_h,
                                      const EditorWorldForgeShellState *state);

#endif
