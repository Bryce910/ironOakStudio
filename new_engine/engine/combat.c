#include <stdlib.h>

#include "engine/combat.h"
#include "engine/effects.h"

extern void ps_spawn(ParticleSystem *ps,Vec3 pos,Vec3 vel,float life,float size,float r,float g,float b);
extern void ps_burst(ParticleSystem *ps,Vec3 pos,float r,float g,float b);
extern void ps_blood(ParticleSystem *ps, Vec3 pos, float chest_h, int count);
extern float terrain_h(float wx, float wz);
extern void audio_play(int snd,int vol);
extern void gameplay_spawn_spell_impact_visual(ParticleSystem *ps,ProjType type,Vec3 pos,Vec3 dir);
extern void dmg_label_spawn(Vec3 pos,float damage,bool is_crit);
extern void dmg_label_spawn_incoming(Vec3 pos,float damage);

enum{
    SND_FIREBALL_HIT=4,
    SND_HIT=8
};

static void proj_emit_trail(ParticleSystem *ps,const Projectile *p){
    if(!ps||!p||p->trail_fx<0) return;
    effects_emit(ps,effects_get_preset((EffectPresetId)p->trail_fx),p->pos,vec3_norm(p->vel),0.65f);
}

static void proj_emit_impact(ParticleSystem *ps,const Projectile *p,Vec3 pos){
    if(!ps||!p) return;
    if(p->type==PROJ_FIREBALL||p->type==PROJ_ICEBALL)
        gameplay_spawn_spell_impact_visual(ps,p->type,pos,vec3_norm(p->vel));
    if(p->impact_fx>=0){
        effects_emit(ps,effects_get_preset((EffectPresetId)p->impact_fx),pos,vec3_norm(p->vel),1.0f);
        return;
    }
    if(p->type==PROJ_FIREBALL||p->type==PROJ_ICEBALL){
        ps_burst(ps,pos,1.0f,0.5f,0.0f);
    }
}

void proj_update(ProjList *pl,float dt,ParticleSystem *ps,
                 Enemy *enemies,int ne,Player *player){
    for(int i=0;i<pl->count;i++){
        Projectile *p=&pl->p[i];
        if(!p->active) continue;
        p->prev_pos=p->pos;
        if(!p->ignore_gravity)
            p->vel.y-=(p->type==PROJ_ARROW?9.8f:4.0f)*dt;
        p->pos=vec3_add(p->pos,vec3_scale(p->vel,dt));
        p->life-=dt;
        if(p->life<=0){p->active=false;continue;}
        p->fx_timer-=dt;
        if(p->fx_timer<=0.0f){
            proj_emit_trail(ps,p);
            p->fx_timer=(p->type==PROJ_ARROW)?0.09f:0.05f;
        }
        if(p->trail_fx<0&&p->type==PROJ_FIREBALL){
            for(int k=0;k<3;k++){
                Vec3 off={(((float)rand()/RAND_MAX)-0.5f)*0.2f,
                          (((float)rand()/RAND_MAX)-0.5f)*0.2f,
                          (((float)rand()/RAND_MAX)-0.5f)*0.2f};
                ps_spawn(ps,vec3_add(p->pos,off),(Vec3){off.x*2,0.5f+off.y,off.z*2},
                         0.4f,0.3f+((float)rand()/RAND_MAX)*0.2f,
                         1.0f,0.4f+((float)rand()/RAND_MAX)*0.3f,0.0f);
            }
        }
        {
            float th=terrain_h(p->pos.x,p->pos.z);
            if(p->pos.y<=th){
                if(p->type==PROJ_FIREBALL||p->type==PROJ_ICEBALL){
                    proj_emit_impact(ps,p,p->pos);
                    audio_play(SND_FIREBALL_HIT,85);
                }
                p->active=false;
                continue;
            }
        }
        if(p->owner==PROJ_OWNER_PLAYER){
            for(int ei=0;ei<ne;ei++){
                Enemy *e=&enemies[ei];
                if(!e->active||e->health<=0) continue;
                {
                    float dx=p->pos.x-e->phys.pos.x;
                    float dz=p->pos.z-e->phys.pos.z;
                    float dy=p->pos.y-(e->phys.pos.y+1.0f);
                    float r=(p->radius+e->phys.radius);
                    if(dx*dx+dy*dy+dz*dz<r*r){
                        bool crit=p->crit_chance>0.0f&&((float)rand()/RAND_MAX)<p->crit_chance;
                        float final_dmg=p->damage*(crit?1.75f:1.0f);
                        e->health-=final_dmg;
                        {Vec3 lbl_pos={e->phys.pos.x,e->phys.pos.y+e->phys.height*0.7f,e->phys.pos.z};
                        dmg_label_spawn(lbl_pos,final_dmg,crit);}
                        if(p->status==STATUS_EFFECT_SLOW){
                            float slow_scale=clampf(1.0f-p->status_value,0.20f,1.0f);
                            if(e->move_speed_timer<=0.0f||slow_scale<e->move_speed_scale)
                                e->move_speed_scale=slow_scale;
                            if(p->status_time>e->move_speed_timer)
                                e->move_speed_timer=p->status_time;
                        }else if(p->status==STATUS_EFFECT_ROOT){
                            if(p->status_time>e->root_timer)
                                e->root_timer=p->status_time;
                        }
                        if(p->type==PROJ_ARROW){
                            e->state=ES_STAGGER;
                            e->stagger_timer=0.25f;
                            ps_blood(ps,e->phys.pos,e->phys.height*0.55f,8);
                            audio_play(SND_HIT,88);
                        }else{
                            proj_emit_impact(ps,p,p->pos);
                            audio_play(SND_FIREBALL_HIT,85);
                        }
                        p->active=false;
                        break;
                    }
                }
            }
        }else if(player->state!=PS_DEAD){
            float dx=p->pos.x-player->phys.pos.x;
            float dz=p->pos.z-player->phys.pos.z;
            float dy=p->pos.y-(player->phys.pos.y+0.9f);
            float r2=(p->radius+player->phys.radius)*(p->radius+player->phys.radius);
            if(dx*dx+dy*dy+dz*dz<r2){
                if(player->dash_iframes<=0){
                    float shield_reduce=(player->shield_timer>0)?0.75f:0.0f;
                    float block_reduce =(player->blocking)      ?0.60f:0.0f;
                    float total_reduce=clampf(shield_reduce+block_reduce,0,0.90f);
                    float incoming_dmg=p->damage*(1.0f-total_reduce);
                    player->health-=incoming_dmg;
                    player->hit_flash=1.0f;
                    {Vec3 lp={player->phys.pos.x,player->phys.pos.y+1.0f,player->phys.pos.z};
                    dmg_label_spawn_incoming(lp,incoming_dmg);}
                    ps_blood(ps,player->phys.pos,0.9f,8);
                    audio_play(SND_HIT,90);
                    if(player->health<=0){
                        player->health=0;
                        player->state=PS_DEAD;
                    }
                }
                p->active=false;
            }
        }
    }
}
