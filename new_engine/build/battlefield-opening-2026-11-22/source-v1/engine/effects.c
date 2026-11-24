#include "engine/effects.h"

#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FX_SPEED_SCALE 0.0050f
#define FX_SIZE_SCALE  0.0100f

static float fx_rand01(void){
    return (float)rand()/(float)RAND_MAX;
}

static float fx_randf(float lo,float hi){
    return lo+(hi-lo)*fx_rand01();
}

static Vec3 fx_lerp3(Vec3 a,Vec3 b,float t){
    return (Vec3){
        a.x+(b.x-a.x)*t,
        a.y+(b.y-a.y)*t,
        a.z+(b.z-a.z)*t
    };
}

static ParticleType fx_particle_type_for_layer(const EffectLayer *layer){
    switch(layer->type){
        case EFFECT_LAYER_STREAK:  return PT_MAGIC;
        case EFFECT_LAYER_MIST:    return layer->additive ? PT_MAGIC : PT_DUST;
        case EFFECT_LAYER_CHUNK:   return layer->additive ? PT_NORMAL : PT_DUST;
        case EFFECT_LAYER_DROPLET: return PT_DUST;
        case EFFECT_LAYER_SPARK:
        default:                   return layer->additive ? PT_NORMAL : PT_DUST;
    }
}

static void fx_spawn_particle(ParticleSystem *ps,Vec3 pos,Vec3 vel,float life,float size,
                              Vec3 color,ParticleType type,float grav_scale,float drag){
    for(int i=0;i<MAX_PARTICLES;i++){
        Particle *p=&ps->particles[i];
        if(p->active) continue;
        p->pos=pos;
        p->vel=vel;
        p->life=life;
        p->max_life=life;
        p->size=size;
        p->r=color.x;
        p->g=color.y;
        p->b=color.z;
        p->a=1.0f;
        p->grav_scale=grav_scale;
        p->drag=drag;
        p->type=type;
        p->frame=0;
        p->active=true;
        if(i>=ps->pcount) ps->pcount=i+1;
        return;
    }
}

static void fx_emit_layer(ParticleSystem *ps,const EffectLayer *layer,Vec3 origin,Vec3 dir,float scale){
    float base_yaw=atan2f(dir.z,dir.x);
    float half_spread=layer->spread_deg*(float)M_PI/180.0f*0.5f;
    ParticleType ptype=fx_particle_type_for_layer(layer);
    float grav_mul=layer->gravity/880.0f;
    for(int i=0;i<layer->count;i++){
        float ang=base_yaw+fx_randf(-half_spread,half_spread);
        float spd=fx_randf(layer->speed_min,layer->speed_max)*FX_SPEED_SCALE*scale;
        float up=fx_randf(0.0f,layer->up_bias)*FX_SPEED_SCALE*scale;
        float side_jitter=(layer->spread_deg>=360.0f)?fx_randf(-0.4f,0.4f):fx_randf(-0.12f,0.22f);
        float life=fx_randf(layer->life_min,layer->life_max);
        float size=fx_randf(layer->size_min,layer->size_max)*FX_SIZE_SCALE*scale;
        float tint_t=fx_rand01();
        Vec3 color=fx_lerp3(layer->color1,layer->color2,tint_t);
        Vec3 vel={(cosf(ang)*spd)+(dir.x*spd*0.35f),
                  up+side_jitter*spd,
                  (sinf(ang)*spd)+(dir.z*spd*0.35f)};

        if(layer->type==EFFECT_LAYER_STREAK){
            size*=1.7f;
            life*=0.75f;
            vel.x*=0.25f;
            vel.z*=0.25f;
            vel.y+=0.25f*scale;
        }else if(layer->type==EFFECT_LAYER_CHUNK){
            size*=0.90f;
            vel.y+=0.08f*scale;
        }else if(layer->type==EFFECT_LAYER_DROPLET){
            size*=0.70f;
            vel.y+=0.04f*scale;
        }else if(layer->type==EFFECT_LAYER_MIST){
            size*=1.15f;
        }

        fx_spawn_particle(ps,origin,vel,life,size,color,ptype,layer->grav_scale*grav_mul,layer->drag);
    }
}

EffectLayer effects_default_spark(void){
    return (EffectLayer){
        .type=EFFECT_LAYER_SPARK,
        .name="Sparks",
        .count=60,
        .speed_min=300.0f,.speed_max=1200.0f,
        .spread_deg=160.0f,
        .life_min=0.15f,.life_max=0.55f,
        .size_min=3.0f,.size_max=12.0f,
        .color1={1.0f,1.0f,1.0f},
        .color2={1.0f,0.2667f,0.0f},
        .color_splat={0.40f,0.0f,0.0f},
        .gravity=880.0f,
        .grav_scale=0.75f,
        .drag=0.972f,
        .additive=true,
        .up_bias=150.0f,
        .angle_vel_min=-20.0f,.angle_vel_max=20.0f,
        .width_min=3.0f,.width_max=12.0f,
        .height_min=6.0f,.height_max=22.0f
    };
}

EffectLayer effects_default_mist(void){
    return (EffectLayer){
        .type=EFFECT_LAYER_MIST,
        .name="Mist/Smoke",
        .count=60,
        .speed_min=20.0f,.speed_max=180.0f,
        .spread_deg=200.0f,
        .life_min=0.4f,.life_max=1.3f,
        .size_min=8.0f,.size_max=28.0f,
        .color1={1.0f,0.40f,0.0f},
        .color2={0.6667f,0.1333f,0.0f},
        .color_splat={0.2667f,0.0667f,0.0f},
        .gravity=880.0f,
        .grav_scale=0.08f,
        .drag=0.920f,
        .additive=false,
        .up_bias=40.0f,
        .angle_vel_min=-20.0f,.angle_vel_max=20.0f,
        .width_min=3.0f,.width_max=12.0f,
        .height_min=6.0f,.height_max=22.0f
    };
}

EffectLayer effects_default_chunk(void){
    return (EffectLayer){
        .type=EFFECT_LAYER_CHUNK,
        .name="Chunks",
        .count=16,
        .speed_min=100.0f,.speed_max=600.0f,
        .spread_deg=140.0f,
        .life_min=0.8f,.life_max=2.0f,
        .size_min=4.0f,.size_max=18.0f,
        .color1={0.7843f,0.4706f,0.2510f},
        .color2={0.3529f,0.1882f,0.0627f},
        .color_splat={0.2275f,0.1020f,0.0f},
        .gravity=880.0f,
        .grav_scale=1.0f,
        .drag=0.985f,
        .additive=false,
        .up_bias=200.0f,
        .angle_vel_min=-20.0f,.angle_vel_max=20.0f,
        .width_min=3.0f,.width_max=12.0f,
        .height_min=6.0f,.height_max=22.0f
    };
}

EffectLayer effects_default_droplet(void){
    return (EffectLayer){
        .type=EFFECT_LAYER_DROPLET,
        .name="Droplets",
        .count=40,
        .speed_min=150.0f,.speed_max=700.0f,
        .spread_deg=120.0f,
        .life_min=0.6f,.life_max=1.6f,
        .size_min=3.0f,.size_max=10.0f,
        .color1={0.80f,0.0f,0.0f},
        .color2={0.40f,0.0f,0.0f},
        .color_splat={0.20f,0.0f,0.0f},
        .gravity=880.0f,
        .grav_scale=1.0f,
        .drag=0.986f,
        .additive=false,
        .up_bias=280.0f,
        .angle_vel_min=-20.0f,.angle_vel_max=20.0f,
        .width_min=3.0f,.width_max=12.0f,
        .height_min=6.0f,.height_max=22.0f
    };
}

EffectLayer effects_default_streak(void){
    return (EffectLayer){
        .type=EFFECT_LAYER_STREAK,
        .name="Streaks",
        .count=12,
        .speed_min=0.0f,.speed_max=0.0f,
        .spread_deg=0.0f,
        .life_min=0.05f,.life_max=0.18f,
        .size_min=50.0f,.size_max=140.0f,
        .color1={0.7843f,0.8627f,1.0f},
        .color2={0.7843f,0.8627f,1.0f},
        .color_splat={0.7843f,0.8627f,1.0f},
        .gravity=0.0f,
        .grav_scale=0.0f,
        .drag=1.0f,
        .additive=true,
        .up_bias=0.0f,
        .angle_vel_min=-20.0f,.angle_vel_max=20.0f,
        .width_min=3.0f,.width_max=12.0f,
        .height_min=6.0f,.height_max=22.0f
    };
}

static bool g_fx_init=false;
static EffectLayer g_fx_layers[EFFECT_PRESET_COUNT][4];
static EffectDef g_fx_defs[EFFECT_PRESET_COUNT];

static void effects_init_presets(void){
    if(g_fx_init) return;
    memset(g_fx_layers,0,sizeof(g_fx_layers));
    memset(g_fx_defs,0,sizeof(g_fx_defs));

#define FX_DEF(ID,NAME,N) g_fx_defs[ID]=(EffectDef){NAME,g_fx_layers[ID],N}

    g_fx_layers[EFFECT_PRESET_AXE_FLESH][0]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_AXE_FLESH][0].count=70;
    g_fx_layers[EFFECT_PRESET_AXE_FLESH][0].up_bias=200.0f;
    g_fx_layers[EFFECT_PRESET_AXE_FLESH][1]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_AXE_FLESH][1].color1=(Vec3){1.0f,0.13f,0.0f};
    g_fx_layers[EFFECT_PRESET_AXE_FLESH][1].color2=(Vec3){0.40f,0.0f,0.0f};
    g_fx_layers[EFFECT_PRESET_AXE_FLESH][2]=effects_default_droplet();
    g_fx_layers[EFFECT_PRESET_AXE_FLESH][2].count=40;
    g_fx_layers[EFFECT_PRESET_AXE_FLESH][2].up_bias=320.0f;
    FX_DEF(EFFECT_PRESET_AXE_FLESH,"AXE_HIT_FLESH",3);

    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][0].count=8;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][0].grav_scale=-0.6f;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][0].additive=true;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1].count=12;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1].speed_min=30.0f;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1].speed_max=120.0f;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1].color1=(Vec3){1.0f,0.67f,0.0f};
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1].color2=(Vec3){1.0f,0.13f,0.0f};
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][2]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][2].count=4;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][2].grav_scale=-0.2f;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][2].size_min=20.0f;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][2].size_max=55.0f;
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][2].color1=(Vec3){0.27f,0.27f,0.27f};
    g_fx_layers[EFFECT_PRESET_FIRE_TORCH][2].color2=(Vec3){0.07f,0.07f,0.07f};
    FX_DEF(EFFECT_PRESET_FIRE_TORCH,"FIRE_TORCH",3);

    g_fx_layers[EFFECT_PRESET_EXPLOSION][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_EXPLOSION][0].count=40;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][0].additive=true;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][1]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_EXPLOSION][1].count=90;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][1].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][1].speed_max=1600.0f;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][2]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_EXPLOSION][2].count=22;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][2].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][3]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_EXPLOSION][3].count=30;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][3].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][3].life_min=1.5f;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][3].life_max=3.5f;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][3].size_min=25.0f;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][3].size_max=65.0f;
    g_fx_layers[EFFECT_PRESET_EXPLOSION][3].color1=(Vec3){0.33f,0.33f,0.33f};
    g_fx_layers[EFFECT_PRESET_EXPLOSION][3].color2=(Vec3){0.07f,0.07f,0.07f};
    FX_DEF(EFFECT_PRESET_EXPLOSION,"EXPLOSION",4);

    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][0].additive=true;
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][0].color1=(Vec3){1.0f,1.0f,1.0f};
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][0].color2=(Vec3){0.53f,0.0f,1.0f};
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][1]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][1].count=80;
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][1].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][1].color2=(Vec3){0.40f,0.0f,1.0f};
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][2]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][2].count=16;
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][2].additive=true;
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][2].color1=(Vec3){0.8f,0.6f,1.0f};
    g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][2].color2=(Vec3){0.27f,0.0f,0.67f};
    FX_DEF(EFFECT_PRESET_MAGIC_IMPACT,"MAGIC_IMPACT",3);

    g_fx_layers[EFFECT_PRESET_FIREBALL_CAST][0]=g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][0];
    g_fx_layers[EFFECT_PRESET_FIREBALL_CAST][0].count=30;
    g_fx_layers[EFFECT_PRESET_FIREBALL_CAST][0].color1=(Vec3){1.0f,0.87f,0.53f};
    g_fx_layers[EFFECT_PRESET_FIREBALL_CAST][0].color2=(Vec3){1.0f,0.33f,0.0f};
    g_fx_layers[EFFECT_PRESET_FIREBALL_CAST][1]=g_fx_layers[EFFECT_PRESET_MAGIC_IMPACT][1];
    g_fx_layers[EFFECT_PRESET_FIREBALL_CAST][1].count=36;
    g_fx_layers[EFFECT_PRESET_FIREBALL_CAST][1].color1=(Vec3){1.0f,0.93f,0.67f};
    g_fx_layers[EFFECT_PRESET_FIREBALL_CAST][1].color2=(Vec3){1.0f,0.47f,0.0f};
    FX_DEF(EFFECT_PRESET_FIREBALL_CAST,"FIREBALL_CAST",2);

    g_fx_layers[EFFECT_PRESET_FIREBALL_TRAIL][0]=g_fx_layers[EFFECT_PRESET_FIRE_TORCH][0];
    g_fx_layers[EFFECT_PRESET_FIREBALL_TRAIL][0].count=6;
    g_fx_layers[EFFECT_PRESET_FIREBALL_TRAIL][0].grav_scale=-0.15f;
    g_fx_layers[EFFECT_PRESET_FIREBALL_TRAIL][1]=g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1];
    g_fx_layers[EFFECT_PRESET_FIREBALL_TRAIL][1].count=8;
    g_fx_layers[EFFECT_PRESET_FIREBALL_TRAIL][1].speed_min=45.0f;
    g_fx_layers[EFFECT_PRESET_FIREBALL_TRAIL][1].speed_max=160.0f;
    FX_DEF(EFFECT_PRESET_FIREBALL_TRAIL,"FIREBALL_TRAIL",2);

    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][0]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][0].count=28;
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][0].color1=(Vec3){0.93f,0.97f,1.0f};
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][0].color2=(Vec3){0.27f,0.60f,0.80f};
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][1]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][1].count=40;
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][1].additive=true;
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][1].color1=(Vec3){0.92f,0.97f,1.0f};
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][1].color2=(Vec3){0.42f,0.74f,1.0f};
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][2]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][2].count=50;
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][2].color1=(Vec3){0.95f,0.98f,1.0f};
    g_fx_layers[EFFECT_PRESET_ICE_SHATTER][2].color2=(Vec3){0.53f,0.87f,1.0f};
    FX_DEF(EFFECT_PRESET_ICE_SHATTER,"ICE_SHATTER",3);

    g_fx_layers[EFFECT_PRESET_ICEBALL_CAST][0]=g_fx_layers[EFFECT_PRESET_ICE_SHATTER][1];
    g_fx_layers[EFFECT_PRESET_ICEBALL_CAST][0].count=24;
    g_fx_layers[EFFECT_PRESET_ICEBALL_CAST][0].additive=true;
    g_fx_layers[EFFECT_PRESET_ICEBALL_CAST][1]=g_fx_layers[EFFECT_PRESET_ICE_SHATTER][2];
    g_fx_layers[EFFECT_PRESET_ICEBALL_CAST][1].count=28;
    g_fx_layers[EFFECT_PRESET_ICEBALL_CAST][1].speed_min=80.0f;
    g_fx_layers[EFFECT_PRESET_ICEBALL_CAST][1].speed_max=420.0f;
    FX_DEF(EFFECT_PRESET_ICEBALL_CAST,"ICEBALL_CAST",2);

    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][0]=g_fx_layers[EFFECT_PRESET_ICE_SHATTER][1];
    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][0].count=7;
    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][0].life_min=0.25f;
    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][0].life_max=0.55f;
    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][0].size_min=10.0f;
    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][0].size_max=24.0f;
    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][1]=g_fx_layers[EFFECT_PRESET_ICE_SHATTER][2];
    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][1].count=8;
    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][1].speed_min=40.0f;
    g_fx_layers[EFFECT_PRESET_ICEBALL_TRAIL][1].speed_max=160.0f;
    FX_DEF(EFFECT_PRESET_ICEBALL_TRAIL,"ICEBALL_TRAIL",2);

    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0].count=50;
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0].life_min=1.5f;
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0].life_max=3.5f;
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0].size_min=22.0f;
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0].size_max=60.0f;
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0].color1=(Vec3){0.53f,1.0f,0.27f};
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0].color2=(Vec3){0.13f,0.33f,0.0f};
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][0].grav_scale=-0.15f;
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][1]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][1].count=35;
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][1].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][1].color1=(Vec3){0.80f,1.0f,0.0f};
    g_fx_layers[EFFECT_PRESET_POISON_CLOUD][1].color2=(Vec3){0.20f,0.40f,0.0f};
    FX_DEF(EFFECT_PRESET_POISON_CLOUD,"POISON_CLOUD",2);

    g_fx_layers[EFFECT_PRESET_LIGHTNING][0]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_LIGHTNING][0].count=120;
    g_fx_layers[EFFECT_PRESET_LIGHTNING][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_LIGHTNING][0].life_min=0.05f;
    g_fx_layers[EFFECT_PRESET_LIGHTNING][0].life_max=0.35f;
    g_fx_layers[EFFECT_PRESET_LIGHTNING][0].color2=(Vec3){0.27f,0.53f,1.0f};
    g_fx_layers[EFFECT_PRESET_LIGHTNING][1]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_LIGHTNING][1].count=15;
    g_fx_layers[EFFECT_PRESET_LIGHTNING][1].size_min=30.0f;
    g_fx_layers[EFFECT_PRESET_LIGHTNING][1].size_max=80.0f;
    g_fx_layers[EFFECT_PRESET_LIGHTNING][1].additive=true;
    FX_DEF(EFFECT_PRESET_LIGHTNING,"LIGHTNING_STRIKE",2);

    g_fx_layers[EFFECT_PRESET_HEAL][0]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_HEAL][0].count=40;
    g_fx_layers[EFFECT_PRESET_HEAL][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_HEAL][0].grav_scale=-0.5f;
    g_fx_layers[EFFECT_PRESET_HEAL][0].color1=(Vec3){0.67f,1.0f,0.67f};
    g_fx_layers[EFFECT_PRESET_HEAL][0].color2=(Vec3){0.0f,0.80f,0.27f};
    g_fx_layers[EFFECT_PRESET_HEAL][1]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_HEAL][1].count=20;
    g_fx_layers[EFFECT_PRESET_HEAL][1].grav_scale=-0.3f;
    g_fx_layers[EFFECT_PRESET_HEAL][1].additive=true;
    g_fx_layers[EFFECT_PRESET_HEAL][1].color1=(Vec3){0.40f,1.0f,0.53f};
    g_fx_layers[EFFECT_PRESET_HEAL][1].color2=(Vec3){0.0f,0.33f,0.13f};
    g_fx_layers[EFFECT_PRESET_HEAL][2]=effects_default_streak();
    g_fx_layers[EFFECT_PRESET_HEAL][2].count=15;
    g_fx_layers[EFFECT_PRESET_HEAL][2].color1=(Vec3){0.8f,1.0f,0.8f};
    g_fx_layers[EFFECT_PRESET_HEAL][2].color2=(Vec3){0.0f,1.0f,0.4f};
    FX_DEF(EFFECT_PRESET_HEAL,"HEAL_EFFECT",3);

    g_fx_layers[EFFECT_PRESET_VINE_BURST][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_VINE_BURST][0].count=26;
    g_fx_layers[EFFECT_PRESET_VINE_BURST][0].spread_deg=50.0f;
    g_fx_layers[EFFECT_PRESET_VINE_BURST][0].size_min=12.0f;
    g_fx_layers[EFFECT_PRESET_VINE_BURST][0].size_max=30.0f;
    g_fx_layers[EFFECT_PRESET_VINE_BURST][0].color1=(Vec3){0.47f,0.80f,0.33f};
    g_fx_layers[EFFECT_PRESET_VINE_BURST][0].color2=(Vec3){0.13f,0.33f,0.07f};
    g_fx_layers[EFFECT_PRESET_VINE_BURST][1]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_VINE_BURST][1].count=16;
    g_fx_layers[EFFECT_PRESET_VINE_BURST][1].color1=(Vec3){0.60f,0.47f,0.20f};
    g_fx_layers[EFFECT_PRESET_VINE_BURST][1].color2=(Vec3){0.27f,0.20f,0.07f};
    FX_DEF(EFFECT_PRESET_VINE_BURST,"VINE_BURST",2);

    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][0]=g_fx_layers[EFFECT_PRESET_FIRE_TORCH][0];
    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][0].count=18;
    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][0].spread_deg=32.0f;
    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][0].life_min=0.18f;
    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][0].life_max=0.45f;
    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][1]=g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1];
    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][1].count=22;
    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][1].spread_deg=24.0f;
    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][1].speed_min=220.0f;
    g_fx_layers[EFFECT_PRESET_FLAMETHROWER][1].speed_max=420.0f;
    FX_DEF(EFFECT_PRESET_FLAMETHROWER,"FLAMETHROWER",2);

    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][0]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][0].count=18;
    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][0].width_min=1.0f;
    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][0].width_max=4.0f;
    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][0].height_min=8.0f;
    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][0].height_max=28.0f;
    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][1]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][1].count=20;
    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][1].color1=(Vec3){0.73f,0.67f,0.53f};
    g_fx_layers[EFFECT_PRESET_ARROW_IMPACT][1].color2=(Vec3){0.33f,0.27f,0.20f};
    FX_DEF(EFFECT_PRESET_ARROW_IMPACT,"ARROW_IMPACT",2);

    g_fx_layers[EFFECT_PRESET_LOOT_SPARKLE][0]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_LOOT_SPARKLE][0].count=45;
    g_fx_layers[EFFECT_PRESET_LOOT_SPARKLE][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_LOOT_SPARKLE][0].grav_scale=-0.4f;
    g_fx_layers[EFFECT_PRESET_LOOT_SPARKLE][0].color2=(Vec3){1.0f,0.67f,0.0f};
    g_fx_layers[EFFECT_PRESET_LOOT_SPARKLE][1]=effects_default_streak();
    g_fx_layers[EFFECT_PRESET_LOOT_SPARKLE][1].count=10;
    g_fx_layers[EFFECT_PRESET_LOOT_SPARKLE][1].color1=(Vec3){1.0f,1.0f,0.8f};
    g_fx_layers[EFFECT_PRESET_LOOT_SPARKLE][1].color2=(Vec3){1.0f,0.87f,0.0f};
    FX_DEF(EFFECT_PRESET_LOOT_SPARKLE,"LOOT_SPARKLE",2);

    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][0].count=30;
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][0].life_min=1.5f;
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][0].life_max=4.0f;
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][0].grav_scale=-0.6f;
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][0].additive=true;
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][0].color1=(Vec3){0.93f,0.93f,1.0f};
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][0].color2=(Vec3){0.27f,0.27f,0.67f};
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][1]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][1].count=25;
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][1].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][1].grav_scale=-0.45f;
    g_fx_layers[EFFECT_PRESET_DEATH_SOUL][1].color2=(Vec3){0.53f,0.53f,1.0f};
    FX_DEF(EFFECT_PRESET_DEATH_SOUL,"DEATH_SOUL_RELEASE",2);

    g_fx_layers[EFFECT_PRESET_WATER_SPLASH][0]=effects_default_droplet();
    g_fx_layers[EFFECT_PRESET_WATER_SPLASH][0].count=55;
    g_fx_layers[EFFECT_PRESET_WATER_SPLASH][0].color1=(Vec3){0.87f,0.93f,1.0f};
    g_fx_layers[EFFECT_PRESET_WATER_SPLASH][0].color2=(Vec3){0.27f,0.60f,0.80f};
    g_fx_layers[EFFECT_PRESET_WATER_SPLASH][1]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_WATER_SPLASH][1].count=60;
    g_fx_layers[EFFECT_PRESET_WATER_SPLASH][1].color1=(Vec3){0.93f,0.97f,1.0f};
    g_fx_layers[EFFECT_PRESET_WATER_SPLASH][1].color2=(Vec3){0.53f,0.73f,0.80f};
    FX_DEF(EFFECT_PRESET_WATER_SPLASH,"WATER_SPLASH",2);

    g_fx_layers[EFFECT_PRESET_SMOKE_TRAIL][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_SMOKE_TRAIL][0].count=12;
    g_fx_layers[EFFECT_PRESET_SMOKE_TRAIL][0].life_min=1.8f;
    g_fx_layers[EFFECT_PRESET_SMOKE_TRAIL][0].life_max=4.0f;
    g_fx_layers[EFFECT_PRESET_SMOKE_TRAIL][0].grav_scale=-0.3f;
    g_fx_layers[EFFECT_PRESET_SMOKE_TRAIL][0].size_min=18.0f;
    g_fx_layers[EFFECT_PRESET_SMOKE_TRAIL][0].size_max=55.0f;
    g_fx_layers[EFFECT_PRESET_SMOKE_TRAIL][0].color1=(Vec3){0.40f,0.40f,0.40f};
    g_fx_layers[EFFECT_PRESET_SMOKE_TRAIL][0].color2=(Vec3){0.07f,0.07f,0.07f};
    FX_DEF(EFFECT_PRESET_SMOKE_TRAIL,"SMOKE_TRAIL",1);

    g_fx_layers[EFFECT_PRESET_SHIELD_PARRY][0]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_SHIELD_PARRY][0].count=80;
    g_fx_layers[EFFECT_PRESET_SHIELD_PARRY][0].spread_deg=80.0f;
    g_fx_layers[EFFECT_PRESET_SHIELD_PARRY][0].color2=(Vec3){0.67f,0.80f,1.0f};
    g_fx_layers[EFFECT_PRESET_SHIELD_PARRY][1]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_SHIELD_PARRY][1].count=10;
    g_fx_layers[EFFECT_PRESET_SHIELD_PARRY][1].width_min=1.0f;
    g_fx_layers[EFFECT_PRESET_SHIELD_PARRY][1].width_max=5.0f;
    FX_DEF(EFFECT_PRESET_SHIELD_PARRY,"SHIELD_PARRY",2);

    g_fx_layers[EFFECT_PRESET_ICE_WALL][0]=g_fx_layers[EFFECT_PRESET_ICE_SHATTER][1];
    g_fx_layers[EFFECT_PRESET_ICE_WALL][0].count=42;
    g_fx_layers[EFFECT_PRESET_ICE_WALL][0].spread_deg=90.0f;
    g_fx_layers[EFFECT_PRESET_ICE_WALL][0].size_min=22.0f;
    g_fx_layers[EFFECT_PRESET_ICE_WALL][0].size_max=54.0f;
    g_fx_layers[EFFECT_PRESET_ICE_WALL][0].grav_scale=-0.05f;
    g_fx_layers[EFFECT_PRESET_ICE_WALL][1]=g_fx_layers[EFFECT_PRESET_ICE_SHATTER][0];
    g_fx_layers[EFFECT_PRESET_ICE_WALL][1].count=24;
    g_fx_layers[EFFECT_PRESET_ICE_WALL][1].spread_deg=95.0f;
    FX_DEF(EFFECT_PRESET_ICE_WALL,"ICE_WALL",2);

    g_fx_layers[EFFECT_PRESET_BLUNT_IMPACT][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_BLUNT_IMPACT][0].count=30;
    g_fx_layers[EFFECT_PRESET_BLUNT_IMPACT][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_BLUNT_IMPACT][0].size_min=16.0f;
    g_fx_layers[EFFECT_PRESET_BLUNT_IMPACT][0].size_max=48.0f;
    g_fx_layers[EFFECT_PRESET_BLUNT_IMPACT][1]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_BLUNT_IMPACT][1].count=20;
    g_fx_layers[EFFECT_PRESET_BLUNT_IMPACT][1].color1=(Vec3){0.93f,0.93f,0.87f};
    g_fx_layers[EFFECT_PRESET_BLUNT_IMPACT][1].color2=(Vec3){0.60f,0.53f,0.47f};
    FX_DEF(EFFECT_PRESET_BLUNT_IMPACT,"BLUNT_HIT",2);

    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][0].count=50;
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][0].size_min=30.0f;
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][0].size_max=80.0f;
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][0].additive=true;
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][1]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][1].count=40;
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][1].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][2]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][2].count=130;
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][2].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_METEOR_IMPACT][2].speed_max=2000.0f;
    FX_DEF(EFFECT_PRESET_METEOR_IMPACT,"METEOR_IMPACT",3);

    g_fx_layers[EFFECT_PRESET_ROCK_BURST][0]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][0].count=26;
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][0].color1=(Vec3){0.70f,0.62f,0.54f};
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][0].color2=(Vec3){0.32f,0.28f,0.24f};
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][1]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][1].count=18;
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][1].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][1].size_min=18.0f;
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][1].size_max=42.0f;
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][1].color1=(Vec3){0.62f,0.57f,0.50f};
    g_fx_layers[EFFECT_PRESET_ROCK_BURST][1].color2=(Vec3){0.24f,0.22f,0.20f};
    FX_DEF(EFFECT_PRESET_ROCK_BURST,"ROCK_BURST",2);

    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].count=30;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].speed_min=60.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].speed_max=220.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].life_min=0.8f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].life_max=1.8f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].size_min=24.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].size_max=64.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].color1=(Vec3){1.0f,1.0f,1.0f};
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].color2=(Vec3){1.0f,0.8667f,0.0f};
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].grav_scale=-0.2f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][0].additive=true;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][1]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][1].count=60;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][1].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][1].life_min=1.5f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][1].life_max=3.5f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][1].size_min=5.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][1].size_max=16.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][1].grav_scale=-0.3f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][1].color2=(Vec3){1.0f,0.6667f,0.0f};
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][2]=effects_default_streak();
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][2].count=20;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][2].life_min=0.25f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][2].life_max=0.6f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][2].size_min=70.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][2].size_max=190.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][2].color1=(Vec3){1.0f,1.0f,0.67f};
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][2].color2=(Vec3){1.0f,0.8f,0.0f};
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].count=25;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].speed_min=10.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].speed_max=60.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].life_min=2.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].life_max=4.5f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].size_min=16.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].size_max=42.0f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].color1=(Vec3){1.0f,0.93f,0.53f};
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].color2=(Vec3){0.53f,0.33f,0.0f};
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].grav_scale=-0.15f;
    g_fx_layers[EFFECT_PRESET_LEVEL_UP][3].additive=true;
    FX_DEF(EFFECT_PRESET_LEVEL_UP,"LEVEL_UP",4);

    g_fx_layers[EFFECT_PRESET_BLEED_TICK][0]=effects_default_droplet();
    g_fx_layers[EFFECT_PRESET_BLEED_TICK][0].count=6;
    g_fx_layers[EFFECT_PRESET_BLEED_TICK][0].speed_min=10.0f;
    g_fx_layers[EFFECT_PRESET_BLEED_TICK][0].speed_max=60.0f;
    g_fx_layers[EFFECT_PRESET_BLEED_TICK][1]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_BLEED_TICK][1].count=4;
    g_fx_layers[EFFECT_PRESET_BLEED_TICK][1].size_min=5.0f;
    g_fx_layers[EFFECT_PRESET_BLEED_TICK][1].size_max=14.0f;
    g_fx_layers[EFFECT_PRESET_BLEED_TICK][1].color1=(Vec3){0.67f,0.0f,0.0f};
    g_fx_layers[EFFECT_PRESET_BLEED_TICK][1].color2=(Vec3){0.20f,0.0f,0.0f};
    FX_DEF(EFFECT_PRESET_BLEED_TICK,"BLEED_TICK",2);

    g_fx_layers[EFFECT_PRESET_CAMPFIRE][0]=g_fx_layers[EFFECT_PRESET_FIRE_TORCH][0];
    g_fx_layers[EFFECT_PRESET_CAMPFIRE][0].count=10;
    g_fx_layers[EFFECT_PRESET_CAMPFIRE][1]=g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1];
    g_fx_layers[EFFECT_PRESET_CAMPFIRE][1].count=18;
    g_fx_layers[EFFECT_PRESET_CAMPFIRE][2]=g_fx_layers[EFFECT_PRESET_FIRE_TORCH][2];
    g_fx_layers[EFFECT_PRESET_CAMPFIRE][2].count=5;
    FX_DEF(EFFECT_PRESET_CAMPFIRE,"CAMPFIRE",3);

    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][0].count=35;
    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][0].color1=(Vec3){0.33f,0.20f,0.53f};
    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][0].color2=(Vec3){0.07f,0.0f,0.13f};
    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][1]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][1].count=40;
    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][1].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][1].color1=(Vec3){0.60f,0.33f,1.0f};
    g_fx_layers[EFFECT_PRESET_SHADOW_BLINK][1].color2=(Vec3){0.13f,0.0f,0.27f};
    FX_DEF(EFFECT_PRESET_SHADOW_BLINK,"SHADOW_BLINK",2);

    g_fx_layers[EFFECT_PRESET_ON_FIRE][0]=g_fx_layers[EFFECT_PRESET_FIRE_TORCH][0];
    g_fx_layers[EFFECT_PRESET_ON_FIRE][0].count=14;
    g_fx_layers[EFFECT_PRESET_ON_FIRE][0].spread_deg=50.0f;
    g_fx_layers[EFFECT_PRESET_ON_FIRE][1]=g_fx_layers[EFFECT_PRESET_FIRE_TORCH][1];
    g_fx_layers[EFFECT_PRESET_ON_FIRE][1].count=20;
    FX_DEF(EFFECT_PRESET_ON_FIRE,"CHARACTER_ON_FIRE",2);

    g_fx_layers[EFFECT_PRESET_STUN][0]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_STUN][0].count=12;
    g_fx_layers[EFFECT_PRESET_STUN][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_STUN][0].grav_scale=-0.35f;
    g_fx_layers[EFFECT_PRESET_STUN][0].color1=(Vec3){1.0f,1.0f,0.53f};
    g_fx_layers[EFFECT_PRESET_STUN][0].color2=(Vec3){1.0f,0.67f,0.0f};
    g_fx_layers[EFFECT_PRESET_STUN][1]=effects_default_streak();
    g_fx_layers[EFFECT_PRESET_STUN][1].count=6;
    g_fx_layers[EFFECT_PRESET_STUN][1].size_min=20.0f;
    g_fx_layers[EFFECT_PRESET_STUN][1].size_max=60.0f;
    FX_DEF(EFFECT_PRESET_STUN,"STUN_STARS",2);

    g_fx_layers[EFFECT_PRESET_SAND_DUST][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_SAND_DUST][0].count=30;
    g_fx_layers[EFFECT_PRESET_SAND_DUST][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_SAND_DUST][0].color1=(Vec3){0.87f,0.80f,0.53f};
    g_fx_layers[EFFECT_PRESET_SAND_DUST][0].color2=(Vec3){0.53f,0.40f,0.20f};
    g_fx_layers[EFFECT_PRESET_SAND_DUST][1]=effects_default_droplet();
    g_fx_layers[EFFECT_PRESET_SAND_DUST][1].count=40;
    g_fx_layers[EFFECT_PRESET_SAND_DUST][1].size_min=1.0f;
    g_fx_layers[EFFECT_PRESET_SAND_DUST][1].size_max=4.0f;
    FX_DEF(EFFECT_PRESET_SAND_DUST,"SAND_DUST_DEVIL",2);

    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][0]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][0].count=25;
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][0].color1=(Vec3){1.0f,0.40f,0.0f};
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][0].color2=(Vec3){0.13f,0.0f,0.0f};
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][1]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][1].count=30;
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][1].additive=true;
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][1].color1=(Vec3){1.0f,0.53f,0.0f};
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][1].color2=(Vec3){0.67f,0.13f,0.0f};
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][2]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][2].count=60;
    g_fx_layers[EFFECT_PRESET_LAVA_SPLASH][2].color2=(Vec3){1.0f,0.27f,0.0f};
    FX_DEF(EFFECT_PRESET_LAVA_SPLASH,"LAVA_SPLASH",3);

    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][0]=effects_default_mist();
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][0].count=50;
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][0].spread_deg=360.0f;
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][0].additive=true;
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][0].color1=(Vec3){1.0f,1.0f,1.0f};
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][0].color2=(Vec3){0.67f,0.87f,1.0f};
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][1]=effects_default_chunk();
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][1].count=30;
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][1].color1=(Vec3){0.87f,0.93f,1.0f};
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][1].color2=(Vec3){0.20f,0.33f,0.47f};
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][2]=effects_default_spark();
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][2].count=70;
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][2].color1=(Vec3){0.93f,0.97f,1.0f};
    g_fx_layers[EFFECT_PRESET_BLIZZARD_HIT][2].color2=(Vec3){0.40f,0.67f,0.80f};
    FX_DEF(EFFECT_PRESET_BLIZZARD_HIT,"BLIZZARD_HIT",3);

    g_fx_init=true;
#undef FX_DEF
}

const EffectDef *effects_preset_level_up(void){
    effects_init_presets();
    return &g_fx_defs[EFFECT_PRESET_LEVEL_UP];
}

int effects_preset_count(void){
    return EFFECT_PRESET_COUNT;
}

const EffectDef *effects_get_preset(EffectPresetId preset){
    effects_init_presets();
    if(preset<0||preset>=EFFECT_PRESET_COUNT) return NULL;
    return &g_fx_defs[preset];
}

void effects_emit(ParticleSystem *ps,const EffectDef *fx,Vec3 origin,Vec3 dir,float scale){
    Vec3 n=vec3_norm(dir);
    if(vec3_len(n)<0.0001f) n=(Vec3){1.0f,0.0f,0.0f};
    if(!ps||!fx||!fx->layers||fx->layer_count<=0) return;
    if(scale<=0.0f) scale=1.0f;
    for(int i=0;i<fx->layer_count;i++) fx_emit_layer(ps,&fx->layers[i],origin,n,scale);
}

void effects_emit_preset(ParticleSystem *ps,EffectPresetId preset,Vec3 origin){
    const EffectDef *fx=effects_get_preset(preset);
    if(fx) effects_emit(ps,fx,origin,(Vec3){1.0f,0.0f,0.0f},1.0f);
}

void effects_emit_level_up(ParticleSystem *ps,Vec3 origin){
    effects_emit_preset(ps,EFFECT_PRESET_LEVEL_UP,origin);
}
