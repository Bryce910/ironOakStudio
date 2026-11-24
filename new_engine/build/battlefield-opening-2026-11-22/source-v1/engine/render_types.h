#ifndef ENGINE_RENDER_TYPES_H
#define ENGINE_RENDER_TYPES_H

#include <stdbool.h>

#include <SDL2/SDL_opengl.h>

#include "engine/runtime_types.h"

typedef struct ufbx_scene ufbx_scene;

#ifndef MAX_POINT_LIGHTS
#define MAX_POINT_LIGHTS 32
#endif

#ifndef NPC_MAX
#define NPC_MAX 16
#endif

#ifndef NPC_LINES_MAX
#define NPC_LINES_MAX 8
#endif

#ifndef NPC_LINE_LEN
#define NPC_LINE_LEN 80
#endif

#ifndef NPC_MODEL_MAX
#define NPC_MODEL_MAX 32
#endif

typedef struct {
    Vec3 pos,color,dir;
    float radius,intensity,flicker_speed,flicker_phase;
    float spot_inner_cos,spot_outer_cos;
    int type;
    bool flicker;
    bool active;
} PointLight;

typedef struct {
    Vec3 sun_dir,sun_color,ambient;
    PointLight points[MAX_POINT_LIGHTS];
    int num_points;
    Vec3 view_pos;
    GLuint shadow_tex;
    GLuint shadow_tex_wide;
    Mat4 light_vp;
    Mat4 light_vp_wide;
    float day_frac;
    float sun_elev;
} LightingSystem;

typedef struct {
    GLuint fbo,tex,prog_depth,prog_skin_depth;
} ShadowMap;

typedef struct {
    GLuint prog;
    GLint loc_model,loc_bones;
} SkinProg;

typedef struct NpcDef_s{
    Vec3 pos;
    float yaw;
    float visual_yaw_offset;
    float scale;
    bool collision_enabled;
    bool patrol_loop;
    char name[32];
    char backstory[256];
    char lines[NPC_LINES_MAX][NPC_LINE_LEN];
    int line_count;
    double anim_time;
    int anim_idx;
    int quest_id;
    bool quest_offered;
    bool is_merchant;
    int shop_id;
    int model_idx;
    int dlg_root_id;
    int dlg_cur_node;
    ufbx_scene *anim_eval;
    int anim_eval_frame;
    int anim_eval_slot;
    const void *anim_eval_scene;
} NpcDef;

typedef enum { NPC_STATE_CHAT,NPC_STATE_QUEST_OFFER,NPC_STATE_SHOP } NpcTalkState;

typedef struct NpcSystem_s{
    NpcDef npcs[NPC_MAX];
    int count;
    int active_npc;
    int active_line;
    bool open;
    NpcTalkState talk_state;
} NpcSystem;

#endif
