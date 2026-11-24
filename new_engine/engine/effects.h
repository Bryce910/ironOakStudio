#ifndef ENGINE_EFFECTS_H
#define ENGINE_EFFECTS_H

#include "engine/game_types.h"

typedef enum{
    EFFECT_LAYER_SPARK=0,
    EFFECT_LAYER_MIST,
    EFFECT_LAYER_CHUNK,
    EFFECT_LAYER_DROPLET,
    EFFECT_LAYER_STREAK
}EffectLayerType;

typedef struct{
    EffectLayerType type;
    const char *name;
    int count;
    float speed_min,speed_max;
    float spread_deg;
    float life_min,life_max;
    float size_min,size_max;
    Vec3 color1,color2,color_splat;
    float gravity;
    float grav_scale;
    float drag;
    bool additive;
    float up_bias;
    float angle_vel_min,angle_vel_max;
    float width_min,width_max;
    float height_min,height_max;
}EffectLayer;

typedef struct{
    const char *name;
    const EffectLayer *layers;
    int layer_count;
}EffectDef;

typedef enum{
    EFFECT_PRESET_AXE_FLESH=0,
    EFFECT_PRESET_FIRE_TORCH,
    EFFECT_PRESET_EXPLOSION,
    EFFECT_PRESET_MAGIC_IMPACT,
    EFFECT_PRESET_FIREBALL_CAST,
    EFFECT_PRESET_FIREBALL_TRAIL,
    EFFECT_PRESET_ICE_SHATTER,
    EFFECT_PRESET_ICEBALL_CAST,
    EFFECT_PRESET_ICEBALL_TRAIL,
    EFFECT_PRESET_POISON_CLOUD,
    EFFECT_PRESET_LIGHTNING,
    EFFECT_PRESET_HEAL,
    EFFECT_PRESET_VINE_BURST,
    EFFECT_PRESET_FLAMETHROWER,
    EFFECT_PRESET_ARROW_IMPACT,
    EFFECT_PRESET_LOOT_SPARKLE,
    EFFECT_PRESET_DEATH_SOUL,
    EFFECT_PRESET_WATER_SPLASH,
    EFFECT_PRESET_SMOKE_TRAIL,
    EFFECT_PRESET_SHIELD_PARRY,
    EFFECT_PRESET_ICE_WALL,
    EFFECT_PRESET_BLUNT_IMPACT,
    EFFECT_PRESET_METEOR_IMPACT,
    EFFECT_PRESET_ROCK_BURST,
    EFFECT_PRESET_LEVEL_UP,
    EFFECT_PRESET_BLEED_TICK,
    EFFECT_PRESET_CAMPFIRE,
    EFFECT_PRESET_SHADOW_BLINK,
    EFFECT_PRESET_ON_FIRE,
    EFFECT_PRESET_STUN,
    EFFECT_PRESET_SAND_DUST,
    EFFECT_PRESET_LAVA_SPLASH,
    EFFECT_PRESET_BLIZZARD_HIT,
    EFFECT_PRESET_COUNT
}EffectPresetId;

EffectLayer effects_default_spark(void);
EffectLayer effects_default_mist(void);
EffectLayer effects_default_chunk(void);
EffectLayer effects_default_droplet(void);
EffectLayer effects_default_streak(void);

const EffectDef *effects_get_preset(EffectPresetId preset);
const EffectDef *effects_preset_level_up(void);
int effects_preset_count(void);

void effects_emit(ParticleSystem *ps,const EffectDef *fx,Vec3 origin,Vec3 dir,float scale);
void effects_emit_preset(ParticleSystem *ps,EffectPresetId preset,Vec3 origin);
void effects_emit_level_up(ParticleSystem *ps,Vec3 origin);

#endif
