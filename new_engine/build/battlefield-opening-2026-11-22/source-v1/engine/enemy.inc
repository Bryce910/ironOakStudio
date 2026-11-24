#include "engine/enemy_types.h"

const EnemyDef EDEF[ETYPE_COUNT]={
  {"Knight",   "enemy_knight.fbx",  80, 12, 2.8f, 14.0f, 1.8f, 1.5f, 1.0f,  0.35f,0.35f,0.42f, 1.8f,0.4f, false,  25},
  {"Archer",   "enemy_archer.fbx",  55,  8, 2.4f, 20.0f, 8.0f, 2.5f, 0.95f, 0.25f,0.45f,0.25f, 1.8f,0.4f, true,   20},
  {"Brute",    "enemy_brute.fbx",  200, 25, 1.6f, 12.0f, 2.2f, 2.2f, 1.35f, 0.55f,0.20f,0.20f, 2.4f,0.6f, false,  75},
  {"Shade",    "enemy_shade.fbx",   40,  8, 5.0f, 18.0f, 1.4f, 0.8f, 0.82f, 0.15f,0.15f,0.35f, 1.6f,0.35f,false,  15},
  {"Lich King","boss.fbx",         800, 35, 2.2f, 40.0f, 2.5f, 1.8f, 1.8f,  0.05f,0.80f,0.85f, 3.0f,0.7f, false, 500},
};

typedef struct{
    bool any_enemy_nearby;
}EnemyUpdateResult;

typedef void (*EnemyNotifyFn)(void *ctx, const char *msg);

void enemy_refresh_eval(Enemy *e, CharModel *cm, float dist, int frame){
    int skip_rate = (dist < 30.0f) ? 1 : (dist < 60.0f) ? 2 : 3;
    if(e->anim_eval && (frame - e->anim_eval_frame) < skip_rate) return;

    if(e->anim_eval){ ufbx_free_scene(e->anim_eval); e->anim_eval=NULL; }
    if(!cm||!scene_has_data(&cm->base)) return;

    int slot=e->anim_idx;
    if(slot<0||slot>=ANIM_SLOTS) slot=ANIM_IDLE;
    AnimEntry *ae=&cm->anims[slot];
    Scene *sc=ae->scene?ae->scene:&cm->base;
    if(!sc) return;
    if(sc->format==SCENE_FORMAT_GLTF){
        scene_gltf_evaluate(sc,ae->scene?ae->stack:-1,e->anim_time);
        e->anim_eval_frame=frame;
        return;
    }
    if(sc->format!=SCENE_FORMAT_UFBX||!sc->scene) return;

    int ai=ae->scene?ae->stack:-1;
    if(sc->scene->anim_stacks.count>0&&ai>=0){
        int idx=ai<(int)sc->scene->anim_stacks.count?ai:(int)sc->scene->anim_stacks.count-1;
        ufbx_anim_stack *stk=sc->scene->anim_stacks.data[idx];
        double real_end=anim_stack_real_end(stk);
        double dur=real_end-stk->time_begin;
        double t=dur>0?fmod(e->anim_time,dur):0;
        e->anim_eval=ufbx_evaluate_scene(sc->scene,stk->anim,stk->time_begin+t,NULL,NULL);
    }
    e->anim_eval_frame=frame;
}

static void enemy_spawn(Enemy *e,EnemyType type,Vec3 pos,AnimMap *amap){
    memset(e,0,sizeof(*e));
    const EnemyDef *d=&EDEF[type];
    Vec3 dry=pos;
    find_nearby_dry_ground(pos,22.0f,&dry);
    e->etype=type;
    e->phys.pos=dry; e->phys.pos.y=terrain_h(dry.x,dry.z)+0.1f;
    e->phys.height=d->height; e->phys.radius=d->radius;
    e->health=e->max_health=d->hp;
    e->anim_idx=amap->idle; e->anim_speed=1.0f;
    e->attack_cooldown=d->attack_cd;
    e->growl_timer=2.0f+(float)rand()/RAND_MAX*4.0f;
    e->spawn_pos=e->phys.pos;
    e->move_speed_scale=1.0f;
    if(type==ETYPE_BOSS){e->boss_phase=1;e->summon_timer=8.0f;e->boss_no_respawn=true;}
    e->active=true;
}

static EnemyUpdateResult enemy_update_all(Enemy *enemies,int ne,float dt,
                                          Player *player,AnimMap *player_amap,
                                          AnimMap enemy_amaps[],ParticleSystem *ps,
                                          ProjList *projs,float player_dmg_reduce,
                                          float hit_dur,float *death_fade,
                                          float *screen_shake,void *notify_ctx,
                                          EnemyNotifyFn notify){
    EnemyUpdateResult result={0};

    int active_ne=0;
    int e_flank_slot[128];
    for(int fi=0;fi<ne;fi++){
        Enemy *fe=&enemies[fi];
        if(!fe->active||fe->state==ES_DEAD){e_flank_slot[fi]=-1;continue;}
        float _pdx=fe->phys.pos.x-player->phys.pos.x;
        float _pdz=fe->phys.pos.z-player->phys.pos.z;
        if(_pdx*_pdx+_pdz*_pdz>200.0f*200.0f){e_flank_slot[fi]=-1;continue;}
        e_flank_slot[fi]=active_ne++;
    }

    for(int i=0;i<ne;i++){
        Enemy *e=&enemies[i];
        if(!e->active) continue;
        {
            float pdx=e->phys.pos.x-player->phys.pos.x;
            float pdz=e->phys.pos.z-player->phys.pos.z;
            if(pdx*pdx+pdz*pdz > 200.0f*200.0f) continue;
        }

        const EnemyDef *d=e->has_stat_override?&e->stat_override:&EDEF[e->etype];
        float speed_mul=1.0f;
        if(e->state==ES_DEAD){
            e->anim_time+=dt;
            if(!e->boss_no_respawn&&e->respawn_timer>0.0f){
                e->respawn_timer-=dt;
                if(e->respawn_timer<=0.0f){
                    float rd=vec3_dist(e->spawn_pos,player->phys.pos);
                    if(rd>30.0f){
                        enemy_spawn(e,e->etype,e->spawn_pos,&enemy_amaps[e->etype]);
                        e->respawn_timer=0.0f;
                    }else{
                        e->respawn_timer=5.0f;
                    }
                }
            }
            continue;
        }

        float dist=vec3_dist(e->phys.pos,player->phys.pos);
        if(dist>120.0f){e->anim_time+=dt*e->anim_speed;continue;}
        if(e->root_timer>0.0f) e->root_timer=clampf(e->root_timer-dt,0.0f,999.0f);
        if(e->move_speed_timer>0.0f){
            e->move_speed_timer=clampf(e->move_speed_timer-dt,0.0f,999.0f);
            speed_mul=e->move_speed_scale;
            if(e->move_speed_timer<=0.0f) e->move_speed_scale=1.0f;
        }else{
            e->move_speed_scale=1.0f;
        }
        if(e->root_timer>0.0f) speed_mul=0.0f;
        if(e->state==ES_STAGGER){
            e->stagger_timer-=dt;
            if(e->stagger_timer<=0) e->state=ES_CHASE;
            e->anim_time+=dt;
            continue;
        }
        e->attack_timer=clampf(e->attack_timer-dt,0,999);

        e->growl_timer-=dt;
        if(e->growl_timer<=0){
            e->growl_timer=4.0f+(float)rand()/RAND_MAX*6.0f;
            audio_play_3d(SND_ENEMY_GROWL,e->phys.pos.x,e->phys.pos.y,e->phys.pos.z,
                          player->phys.pos.x,player->phys.pos.y,player->phys.pos.z,80,20.0f);
        }

        if(dist<d->chase_r) result.any_enemy_nearby=true;

        if(dist>d->chase_r){
            e->state=ES_IDLE;
            e->anim_idx=enemy_amaps[e->etype].idle;
            e->phys.vel.x=e->phys.vel.z=0;
        }else if(d->ranged){
            Vec3 dir=vec3_norm(vec3_sub(player->phys.pos,e->phys.pos));
            if(dist>d->attack_r+1.0f){
                e->state=ES_CHASE;
                e->anim_idx=enemy_amaps[e->etype].walk;
                Vec3 navdir=nav_sample(e->phys.pos,player->phys.pos);
                e->phys.vel.x=navdir.x*d->speed*speed_mul;
                e->phys.vel.z=navdir.z*d->speed*speed_mul;
            }else if(dist<d->attack_r-1.0f){
                e->state=ES_CHASE;
                e->anim_idx=enemy_amaps[e->etype].walk;
                e->phys.vel.x=-dir.x*d->speed*0.7f*speed_mul;
                e->phys.vel.z=-dir.z*d->speed*0.7f*speed_mul;
            }else{
                e->phys.vel.x=e->phys.vel.z=0;
                e->state=ES_ATTACK;
                e->anim_idx=enemy_amaps[e->etype].attack;
                if(e->attack_timer<=0&&player->state!=PS_DEAD){
                    Vec3 aim=vec3_norm(vec3_sub(player->phys.pos,(Vec3){e->phys.pos.x,e->phys.pos.y+1.0f,e->phys.pos.z}));
                    Vec3 spawn={e->phys.pos.x+aim.x*0.8f,e->phys.pos.y+1.3f,e->phys.pos.z+aim.z*0.8f};
                    proj_spawn(projs,spawn,aim,18.0f,d->damage,PROJ_ARROW,PROJ_OWNER_ENEMY);
                    e->attack_timer=e->attack_cooldown;
                    audio_play_3d(SND_ENEMY_ATTACK,e->phys.pos.x,e->phys.pos.y,e->phys.pos.z,
                                  player->phys.pos.x,player->phys.pos.y,player->phys.pos.z,80,20.0f);
                }
            }
            e->phys.yaw=atan2f(dir.x,-dir.z)+(float)M_PI;
        }else if(dist>d->attack_r){
            e->state=ES_CHASE;
            e->anim_idx=enemy_amaps[e->etype].walk;
            {
                float fang=(active_ne>1)?((float)e_flank_slot[i]/(float)active_ne*(float)(2.0*M_PI)):0.0f;
                Vec3 ftgt={player->phys.pos.x+cosf(fang)*d->attack_r*0.85f,player->phys.pos.y,player->phys.pos.z+sinf(fang)*d->attack_r*0.85f};
                Vec3 dir=nav_sample(e->phys.pos,ftgt);
                e->phys.vel.x=dir.x*d->speed*speed_mul;
                e->phys.vel.z=dir.z*d->speed*speed_mul;
                e->phys.yaw=atan2f(dir.x,-dir.z)+(float)M_PI;
            }
        }else{
            e->state=ES_ATTACK;
            e->anim_idx=enemy_amaps[e->etype].attack;
            if(e->attack_timer>0.4f&&(e->etype==ETYPE_KNIGHT||e->etype==ETYPE_BRUTE)){
                Vec3 tdir=vec3_norm(vec3_sub(player->phys.pos,e->phys.pos));
                float sd=(e_flank_slot[i]%2==0)?1.0f:-1.0f;
                e->phys.vel.x=-tdir.z*sd*d->speed*0.35f*speed_mul;
                e->phys.vel.z=tdir.x*sd*d->speed*0.35f*speed_mul;
            }else{
                e->phys.vel.x=e->phys.vel.z=0;
            }
            if(e->attack_timer<=0&&player->state!=PS_DEAD&&player->state!=PS_HIT){
                if(player->dash_iframes<=0){
                    float shield_reduce=(player->shield_timer>0)?0.75f:0.0f;
                    float block_reduce =(player->blocking)      ?0.60f:0.0f;
                    float total_reduce=clampf(player_dmg_reduce+shield_reduce+block_reduce,0,0.90f);
                    float incoming_dmg=d->damage*(1.0f-total_reduce);
                    player->health-=incoming_dmg;
                    player->hit_flash=1.0f;
                    {Vec3 lp={player->phys.pos.x,player->phys.pos.y+1.0f,player->phys.pos.z};
                    dmg_label_spawn_incoming(lp,incoming_dmg);}
                    e->attack_timer=e->attack_cooldown;
                    ps_burst(ps,player->phys.pos,1,0,0);
                    audio_play(SND_HIT,90);
                    audio_play_3d(SND_ENEMY_ATTACK,e->phys.pos.x,e->phys.pos.y,e->phys.pos.z,
                                  player->phys.pos.x,player->phys.pos.y,player->phys.pos.z,85,10.0f);
                    if(player->health<=0){
                        player->health=0;
                        player->state=PS_DEAD;
                        *death_fade=0.0f;
                        player->anim_idx=player_amap->death;
                        player->anim_time=0;
                        ps_blood(ps,player->phys.pos,1.0f,24);
                        audio_play(SND_DEATH,110);
                    }else if(player->blocking){
                        player->hit_flash=0.3f;
                        audio_play(SND_HIT,60);
                    }else{
                        player->state=PS_HIT;
                        player->anim_idx=player_amap->hit;
                        player->anim_time=0;
                        player->hit_timer=hit_dur;
                        {
                            Vec3 kb=vec3_norm((Vec3){player->phys.pos.x-e->phys.pos.x,0,player->phys.pos.z-e->phys.pos.z});
                            player->phys.vel.x=kb.x*7.0f;
                            player->phys.vel.z=kb.z*7.0f;
                            player->phys.vel.y=2.5f;
                        }
                    }
                }
            }
        }

        if(e->etype==ETYPE_SHADE){
            if(e->shake_timer>0.0f) e->shake_timer-=dt;
            if(e->state==ES_CHASE&&dist>15.0f&&e->shake_timer<=0.0f){
                Vec3 bdir=vec3_norm(vec3_sub(e->phys.pos,player->phys.pos));
                e->phys.pos.x=player->phys.pos.x+bdir.x*(d->attack_r+0.4f);
                e->phys.pos.z=player->phys.pos.z+bdir.z*(d->attack_r+0.4f);
                e->phys.pos.y=terrain_h(e->phys.pos.x,e->phys.pos.z)+0.1f;
                e->shake_timer=10.0f;
                ps_burst(ps,e->phys.pos,0.3f,0.15f,0.4f);
                audio_play_3d(SND_DASH,e->phys.pos.x,e->phys.pos.y,e->phys.pos.z,
                              player->phys.pos.x,player->phys.pos.y,player->phys.pos.z,75,12.0f);
            }
        }

        if(e->etype==ETYPE_BOSS&&e->state!=ES_DEAD){
            float hp_frac=e->health/e->max_health;
            int new_phase=(hp_frac>0.60f)?1:(hp_frac>0.30f)?2:3;
            if(new_phase>e->boss_phase){
                e->boss_phase=new_phase;
                e->phase_flash=true;
                e->phase_flash_t=1.0f;
                *screen_shake=0.6f;
                ps_burst(ps,e->phys.pos,1.2f,0.2f,1.0f);
                ps_burst(ps,e->phys.pos,0.8f,0.0f,0.8f);
                if(notify) notify(notify_ctx,(new_phase==2)?"The Lich King ENRAGES!":"THE LICH KING BERSERKS!");
                audio_play(SND_ENEMY_GROWL,110);
            }
            if(e->phase_flash_t>0){
                e->phase_flash_t-=dt*1.5f;
                if(e->phase_flash_t<=0) e->phase_flash=false;
            }

            if(e->boss_phase>=2&&e->attack_timer<=0&&dist<30.0f&&player->state!=PS_DEAD){
                Vec3 aim=vec3_norm(vec3_sub(player->phys.pos,(Vec3){e->phys.pos.x,e->phys.pos.y+1.5f,e->phys.pos.z}));
                Vec3 sp={e->phys.pos.x,e->phys.pos.y+1.5f,e->phys.pos.z};
                proj_spawn(projs,sp,aim,10.0f,d->damage*0.6f,PROJ_FIREBALL,PROJ_OWNER_ENEMY);
                if(e->boss_phase==3){
                    float a=0.35f;
                    Vec3 left ={(aim.x*cosf(a)-aim.z*sinf(a)),aim.y,(aim.x*sinf(a)+aim.z*cosf(a))};
                    Vec3 right={(aim.x*cosf(-a)-aim.z*sinf(-a)),aim.y,(aim.x*sinf(-a)+aim.z*cosf(-a))};
                    proj_spawn(projs,sp,vec3_norm(left),12.0f,d->damage*0.5f,PROJ_FIREBALL,PROJ_OWNER_ENEMY);
                    proj_spawn(projs,sp,vec3_norm(right),12.0f,d->damage*0.5f,PROJ_FIREBALL,PROJ_OWNER_ENEMY);
                }
                e->attack_timer=e->attack_cooldown*(e->boss_phase==3?0.55f:0.80f);
                audio_play_3d(SND_FIREBALL,e->phys.pos.x,e->phys.pos.y,e->phys.pos.z,
                              player->phys.pos.x,player->phys.pos.y,player->phys.pos.z,95,30.0f);
            }

            if(e->boss_phase>=2&&ne<MAX_ENEMIES){
                e->summon_timer-=dt;
                if(e->summon_timer<=0){
                    e->summon_timer=(e->boss_phase==3)?8.0f:14.0f;
                    for(int si=0;si<MAX_ENEMIES;si++){
                        if(!enemies[si].active){
                            float ang=(float)rand()/(float)RAND_MAX*(float)(2*M_PI);
                            Vec3 spos={e->phys.pos.x+cosf(ang)*5.0f,0,e->phys.pos.z+sinf(ang)*5.0f};
                            enemy_spawn(&enemies[si],ETYPE_KNIGHT,spos,&enemy_amaps[ETYPE_KNIGHT]);
                            ps_burst(ps,spos,0.5f,0.3f,0.4f);
                            if(notify) notify(notify_ctx,"The Lich King summons a Knight!");
                            break;
                        }
                    }
                }
            }

            if(e->boss_phase==3&&dist>d->attack_r){
                float spd3=d->speed*2.0f;
                Vec3 dir=vec3_norm(vec3_sub(player->phys.pos,e->phys.pos));
                e->phys.vel.x=dir.x*spd3*speed_mul;
                e->phys.vel.z=dir.z*spd3*speed_mul;
            }
            if(e->state==ES_ATTACK&&e->attack_timer==e->attack_cooldown)
                *screen_shake=clampf(*screen_shake+0.25f,0,0.8f);
        }

        if(e->state!=ES_IDLE||dist<30.0f){
            float eg=terrain_h(e->phys.pos.x,e->phys.pos.z);
            phys_update(&e->phys,dt,eg);
        }
        e->anim_time+=dt*e->anim_speed;
    }

    for(int si=0;si<ne;si++){
        Enemy *ea=&enemies[si];
        if(!ea->active||ea->state==ES_DEAD) continue;
        for(int sj=si+1;sj<ne;sj++){
            Enemy *eb=&enemies[sj];
            if(!eb->active||eb->state==ES_DEAD) continue;
            float sdx=ea->phys.pos.x-eb->phys.pos.x;
            float sdz=ea->phys.pos.z-eb->phys.pos.z;
            float sd2=sdx*sdx+sdz*sdz;
            float min_sep=ea->phys.radius+eb->phys.radius+0.3f;
            if(sd2<min_sep*min_sep&&sd2>0.0001f){
                float sd=sqrtf(sd2);
                float push=(min_sep-sd)/min_sep*5.0f;
                float nx=sdx/sd, nz=sdz/sd;
                ea->phys.vel.x+=nx*push;
                ea->phys.vel.z+=nz*push;
                eb->phys.vel.x-=nx*push;
                eb->phys.vel.z-=nz*push;
            }
        }
    }

    return result;
}
