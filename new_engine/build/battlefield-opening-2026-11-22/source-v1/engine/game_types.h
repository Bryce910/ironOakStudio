#ifndef ENGINE_GAME_TYPES_H
#define ENGINE_GAME_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "engine/math.inc"

typedef struct ufbx_scene ufbx_scene;

typedef struct PhysBody{
    Vec3 pos,vel;
    float yaw;
    bool on_ground;
    float height,radius;
}PhysBody;

typedef enum{PLAYER_CLASS_WARRIOR=0,PLAYER_CLASS_ROGUE,PLAYER_CLASS_MAGE,PLAYER_CLASS_RANGER,PLAYER_CLASS_COUNT}PlayerClass;

typedef enum{PS_IDLE,PS_WALK,PS_RUN,PS_MOUNTED,PS_ATTACK,PS_ATTACK2,PS_JUMP,PS_ROLL,PS_BLOCK,PS_HIT,PS_DEAD,PS_SWIM}PlayerState;

typedef struct{
    PhysBody phys;
    PlayerClass class_id;
    PlayerState state;
    float health,max_health;
    double anim_time;
    float anim_speed;
    int anim_idx,prev_anim_idx;
    float attack_timer;
    bool attack_hit;
    float hit_flash;
    float roll_timer;
    float hit_timer;
    bool blocking;
    bool aiming;
    bool attack_ranged;
    float shield_timer;
    float dash_iframes;
    float ooc_timer;
    float ice_wall_fx_timer;
    Vec3 ice_wall_pos;
    Vec3 ice_wall_dir;
    ufbx_scene *anim_eval;
    int anim_eval_frame;
    int anim_eval_slot;
    const void *anim_eval_scene;
}Player;

#define MAX_PROJECTILES 32
typedef enum{PROJ_FIREBALL=0,PROJ_ARROW,PROJ_ICEBALL}ProjType;
typedef enum{PROJ_OWNER_PLAYER=0,PROJ_OWNER_ENEMY}ProjOwner;
typedef enum{STATUS_EFFECT_NONE=0,STATUS_EFFECT_SLOW,STATUS_EFFECT_ROOT}StatusEffectType;

typedef struct{
    Vec3 pos,prev_pos,vel;
    float life,radius,damage;
    float crit_chance;
    float status_value,status_time;
    float fx_timer;
    ProjType type;
    ProjOwner owner;
    int impact_fx;
    int trail_fx;
    StatusEffectType status;
    bool ignore_gravity;
    bool active;
}Projectile;

typedef struct{
    Projectile p[MAX_PROJECTILES];
    int count;
}ProjList;

typedef enum{PT_NORMAL=0,PT_FIRE=1,PT_MAGIC=2,PT_DUST=3,PT_ROCK=4}ParticleType;

typedef struct{
    Vec3 pos,vel;
    float life,max_life,size,r,g,b,a;
    float grav_scale,drag;
    ParticleType type;
    int frame;
    bool active;
}Particle;

typedef struct{
    Vec3 pos;
    float rate,accum,r,g,b,size,speed;
    bool active;
    ParticleType spawn_type;
}Emitter;

#define MAX_PARTICLES 2048
#define MAX_EMITTERS 48
typedef struct{
    Particle particles[MAX_PARTICLES];
    int free_stack[MAX_PARTICLES];
    Emitter emitters[MAX_EMITTERS];
    int pcount,ecount,free_top;
    unsigned int prog;
    unsigned int fire_prog;
    unsigned int magic_prog;
    unsigned int rock_prog;
    unsigned int vao,vbo;
    float total_time;
}ParticleSystem;

#endif
