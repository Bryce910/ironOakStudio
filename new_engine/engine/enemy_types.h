#ifndef ENGINE_ENEMY_TYPES_H
#define ENGINE_ENEMY_TYPES_H

#include <stdbool.h>
#include "ufbx.h"
#include "engine/game_types.h"

typedef enum{ES_IDLE,ES_CHASE,ES_ATTACK,ES_STAGGER,ES_DEAD}EnemyState;

typedef enum{
    ETYPE_KNIGHT=0,
    ETYPE_ARCHER,
    ETYPE_BRUTE,
    ETYPE_SHADE,
    ETYPE_BOSS,
    ETYPE_COUNT
}EnemyType;

typedef struct{
    const char *name;
    const char *fbx;
    float hp;
    float damage;
    float speed;
    float chase_r;
    float attack_r;
    float attack_cd;
    float scale;
    float cr,cg,cb;
    float height,radius;
    bool ranged;
    int xp;
}EnemyDef;

typedef struct{
    PhysBody phys;
    EnemyState state;
    EnemyType etype;
    float health,max_health;
    double anim_time;
    float anim_speed;
    int anim_idx;
    float attack_timer,attack_cooldown,stagger_timer;
    float growl_timer;
    bool active;
    Vec3 spawn_pos;
    float respawn_timer;
    bool boss_no_respawn;
    int boss_phase;
    float summon_timer;
    float shake_timer;
    float move_speed_scale;
    float move_speed_timer;
    float root_timer;
    bool phase_flash;
    float phase_flash_t;
    ufbx_scene *anim_eval;
    int anim_throttle;
    int anim_eval_frame;
    int model_idx; /* 0=use EDEF default, >0=g_enemy_custom_models[model_idx-1] */
    EnemyDef stat_override;
    bool has_stat_override;
}Enemy;

#endif
