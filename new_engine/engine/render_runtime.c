#include <stdio.h>
#include <unistd.h>

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include "ufbx.h"
#include "cgltf.h"
#include "stb_image.h"
#include "gl_helpers.h"

#include "engine/asset_system.h"
#include "engine/render_runtime.h"

#define MAX_BONES 128
#define MAX_WEIGHTS 4
#define MAX_MESHES 64
#define MAX_GLTF_TEX_CACHE 256

#include "engine/math.inc"
#include "engine/scene.inc"

typedef struct Terrain Terrain;

extern const EnemyDef EDEF[];
extern void enemy_refresh_eval(Enemy *e, CharModel *cm, float dist, int frame);
extern void props_shadow(PropList *pl,ShadowMap *sm,LightingSystem *ls,Vec3 cam);
extern void shadow_begin(ShadowMap *sm,LightingSystem *ls,Vec3 center);
extern void shadow_begin_wide(ShadowMap *sm,LightingSystem *ls,Vec3 center);
extern void shadow_end(ShadowMap *sm,int win_w,int win_h);
extern void shadow_terrain(ShadowMap *sm,LightingSystem *ls,Terrain *t);
extern Mat4 entity_mat(Vec3 pos,float yaw,float dy,float scale,float cx,float cy,float cz);
extern void charmodel_set_render_inventory(const Inventory *inv);
extern void draw_charmodel_equipment(SkinProg *sp,const Inventory *inv,Scene *char_scene,Mat4 wm,int anim_index,double at,Mat4 proj,Mat4 view,Vec3 cam_pos,LightingSystem *ls);
extern void shadow_charmodel_equipment(ShadowMap *sm,LightingSystem *ls,const Inventory *inv,Scene *char_scene,Mat4 wm,int anim_index,double at);
extern void shadow_charmodel(ShadowMap *sm,LightingSystem *ls,CharModel *cm,Mat4 wm,int anim_slot,double at);
extern void shadow_scene_cached(ShadowMap *sm, LightingSystem *ls, Scene *sc, Mat4 wm, ufbx_scene *eval);
extern void draw_charmodel(SkinProg *sp,CharModel *cm,Mat4 wm,int anim_slot,double at,float flash,Mat4 proj,Mat4 view,Vec3 cam_pos,LightingSystem *ls);
extern void draw_scene_cached(SkinProg *sp, Scene *sc, Mat4 wm, ufbx_scene *eval, float flash, Vec3 cam_pos, Mat4 proj, Mat4 view, LightingSystem *ls);
extern void charmodel_anim_resolve(CharModel *cm,int anim_slot,Scene **out_scene,int *out_stack);
extern void player_refresh_eval(Player *player,CharModel *cm,int frame);
extern void npc_refresh_eval(NpcDef *npc,CharModel *cm,float dist,int frame);
extern void frustum_extract(float pl[6][4],Mat4 pv);
extern bool frustum_sphere(float pl[6][4],float cx,float cy,float cz,float r);
extern bool g_player_tint_enabled;
extern bool g_player_render_hidden;
extern void render_mount_shadow_if_active(ShadowMap *sm,LightingSystem *ls,int frame_count);
extern void render_mount_character_pass(SkinProg *skin,Mat4 proj,Mat4 view,Vec3 cam_pos,LightingSystem *ls,int frame_count);

bool g_render_shadow_pass_wide=false;

static int npc_first_loaded_model_index(const bool npc_model_ok[]){
    for(int i=0;i<NPC_MODEL_MAX;i++){
        if(npc_model_ok[i]) return i;
    }
    return -1;
}

static bool wide_shadow_should_refresh(Vec3 center,int frame_count){
    static Vec3 last_center={1e9f,1e9f,1e9f};
    static int last_frame=-9999;
    float dx,dy,dz,dist2;
    if(last_frame<0||frame_count<=last_frame){
        last_center=center;
        last_frame=frame_count;
        return true;
    }
    dx=center.x-last_center.x;
    dy=center.y-last_center.y;
    dz=center.z-last_center.z;
    dist2=dx*dx+dy*dy+dz*dz;
    if(dist2>=100.0f || (frame_count-last_frame)>=12){
        last_center=center;
        last_frame=frame_count;
        return true;
    }
    return false;
}

static void ensure_enemy_model_loaded(EnemyType etype, CharModel *enemy_models,
                                      bool enemy_model_loaded[],
                                      CharModel *fallback_char_model){
    if(enemy_model_loaded[etype]) return;
    {
        const char *fbx=EDEF[etype].fbx;
        if(fbx){
            charmodel_init(&enemy_models[etype],fbx);
            if(!scene_has_data(&enemy_models[etype].base))
                charmodel_share_base(&enemy_models[etype],fallback_char_model);
        }else{
            charmodel_share_base(&enemy_models[etype],fallback_char_model);
        }
        enemy_model_loaded[etype]=true;
        printf("[Enemy] Lazy-loaded model: %s\n",EDEF[etype].name);
    }
}

void ensure_enemy_models_near_camera(Enemy *enemies,int ne, Vec3 cam_pos,
                                     CharModel *enemy_models,
                                     bool enemy_model_loaded[],
                                     CharModel *fallback_char_model){
    for(int i=0;i<ne;i++){
        Enemy *e=&enemies[i];
        if(!e->active) continue;
        if(vec3_dist(e->phys.pos,cam_pos)<130.0f)
            ensure_enemy_model_loaded(e->etype,enemy_models,enemy_model_loaded,fallback_char_model);
    }
}

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
                                bool *enemy_custom_ok){
    float *enemy_custom_scale=g_enemy_custom_scale_mul;
    int npc_fallback_model=npc_first_loaded_model_index(npc_model_ok);
    bool render_shire=(current_scene==SCENE_SHIRE);
    g_render_shadow_pass_wide=false;
    shadow_begin(sm,ls,player->phys.pos);
    if(render_shire){
        shadow_terrain(sm,ls,terrain);
        props_shadow(props,sm,ls,cam->pos);
    }
    if(!g_player_render_hidden){
        Mat4 wm=entity_mat(player->phys.pos,player->phys.yaw,
                           char_model->base.ground_offset,char_model->base.model_scale,
                           char_model->base.cx,char_model->base.cy,char_model->base.cz);
        charmodel_set_render_inventory(player_inventory);
        player_refresh_eval(player,char_model,frame_count);
        {
            Scene *psc=NULL;
            int pstack=-1;
            charmodel_anim_resolve(char_model,player->anim_idx,&psc,&pstack);
            if(psc){
                shadow_scene_cached(sm,ls,psc,wm,player->anim_eval);
                shadow_charmodel_equipment(sm,ls,player_inventory,psc,wm,pstack,player->anim_time);
            }else{
                shadow_charmodel(sm,ls,char_model,wm,player->anim_idx,player->anim_time);
                shadow_charmodel_equipment(sm,ls,player_inventory,&char_model->base,wm,
                                           player->anim_idx,player->anim_time);
            }
        }
        charmodel_set_render_inventory(NULL);
    }
    render_mount_shadow_if_active(sm,ls,frame_count);
    for(int i=0;i<ne;i++){
        Enemy *e=&enemies[i];
        if(!e->active) continue;
        {
            float dist=vec3_dist(e->phys.pos,player->phys.pos);
            if(dist>80.0f) continue;
            {
                int ci=e->model_idx-1;
                bool use_custom=(e->model_idx>0&&ci<enemy_custom_count&&enemy_custom_ok&&enemy_custom_ok[ci]);
                if(!use_custom&&!enemy_model_loaded[e->etype]) continue;
                CharModel *em=use_custom?&enemy_custom_models[ci]:&enemy_models[e->etype];
                enemy_refresh_eval(e,em,dist,frame_count);
                if(!e->anim_eval&&dist>2.0f) continue;
                {
                    const EnemyDef *d=e->has_stat_override?&e->stat_override:&EDEF[e->etype];
                    float extra=(use_custom&&enemy_custom_scale&&ci<enemy_custom_count)?enemy_custom_scale[ci]:1.0f;
                    float sc=em->base.model_scale*d->scale*extra;
                    Mat4 wm=entity_mat(e->phys.pos,e->phys.yaw,em->base.ground_offset,
                                       sc,em->base.cx,em->base.cy,em->base.cz);
                    if(dist<40.0f){
                        int slot=e->anim_idx;
                        if(slot<0||slot>=ANIM_SLOTS) slot=0;
                        {
                            AnimEntry *ae=&em->anims[slot];
                            Scene *scn=ae->scene?ae->scene:&em->base;
                            shadow_scene_cached(sm,ls,scn,wm,e->anim_eval);
                        }
                    }
                }
            }
        }
    }
    if(has_npc_model&&npc_fallback_model>=0){
        for(int i=0;i<npcs->count;i++){
            NpcDef *n=&npcs->npcs[i];
            float dist=vec3_dist(n->pos,player->phys.pos);
            if(dist>60.0f) continue;
            {
                int mi=n->model_idx;
                if(mi<0||mi>=NPC_MODEL_MAX||!npc_model_ok[mi]) mi=npc_fallback_model;
                {
                    CharModel *cm=&npc_models[mi];
                    float npc_scale=(n->scale>0.0f)?n->scale:1.0f;
                    float sc=cm->base.model_scale*npc_scale;
                    Mat4 wm=entity_mat(n->pos,n->yaw+n->visual_yaw_offset,cm->base.ground_offset*npc_scale,
                                       sc,cm->base.cx,cm->base.cy,cm->base.cz);
                    Scene *nsc=NULL;
                    int nstack=-1;
                    npc_refresh_eval(n,cm,dist,frame_count);
                    charmodel_anim_resolve(cm,n->anim_idx,&nsc,&nstack);
                    if(nsc) shadow_scene_cached(sm,ls,nsc,wm,n->anim_eval);
                    else shadow_charmodel(sm,ls,cm,wm,n->anim_idx,n->anim_time);
                }
            }
        }
    }
    shadow_end(sm,win_w,win_h);

    if(render_shire&&wide_shadow_should_refresh(player->phys.pos,frame_count)){
        g_render_shadow_pass_wide=true;
        shadow_begin_wide(sm_wide,ls,player->phys.pos);
        glViewport(0,0,1024,1024);
        // The depth programs consume uLightVP for either target. Preserve the
        // near matrix for receivers after rendering the wide cascade.
        Mat4 near_vp=ls->light_vp;ls->light_vp=ls->light_vp_wide;
        shadow_terrain(sm_wide,ls,terrain);
        props_shadow(props,sm_wide,ls,cam->pos);
        ls->light_vp=near_vp;
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,win_w,win_h);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glCullFace(GL_BACK);
        ls->shadow_tex_wide=sm_wide->tex;
        g_render_shadow_pass_wide=false;
    }
}

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
                              bool *enemy_custom_ok){
    float *enemy_custom_scale=g_enemy_custom_scale_mul;
    int npc_fallback_model=npc_first_loaded_model_index(npc_model_ok);
    if(!g_player_render_hidden){
        Mat4 wm=entity_mat(player->phys.pos,player->phys.yaw,
                           char_model->base.ground_offset,char_model->base.model_scale,
                           char_model->base.cx,char_model->base.cy,char_model->base.cz);
        g_player_tint_enabled=true;
        charmodel_set_render_inventory(player_inventory);
        player_refresh_eval(player,char_model,frame_count);
        {
            Scene *psc=NULL;
            int pstack=-1;
            charmodel_anim_resolve(char_model,player->anim_idx,&psc,&pstack);
            if(psc){
                draw_scene_cached(skin,psc,wm,player->anim_eval,
                                  player->hit_flash,cam_pos,proj,view,ls);
                draw_charmodel_equipment(skin,player_inventory,psc,wm,pstack,
                                         player->anim_time,proj,view,cam_pos,ls);
            }else{
                draw_charmodel(skin,char_model,wm,player->anim_idx,player->anim_time,
                               player->hit_flash,proj,view,cam_pos,ls);
                draw_charmodel_equipment(skin,player_inventory,&char_model->base,wm,
                                         player->anim_idx,player->anim_time,
                                         proj,view,cam_pos,ls);
            }
        }
        charmodel_set_render_inventory(NULL);
        g_player_tint_enabled=false;
    }
    render_mount_character_pass(skin,proj,view,cam_pos,ls,frame_count);

    {
        Mat4 pv=mat4_mul(proj,view);
        float planes[6][4];
        frustum_extract(planes,pv);
        for(int i=0;i<ne;i++){
            Enemy *e=&enemies[i];
            if(!e->active) continue;
            {
                int ci=e->model_idx-1;
                bool use_custom=(e->model_idx>0&&ci<enemy_custom_count&&enemy_custom_ok&&enemy_custom_ok[ci]);
                if(!use_custom&&!enemy_model_loaded[e->etype]) continue;
                {
                    float dist=vec3_dist(e->phys.pos,cam_pos);
                    if(dist>110.0f) continue;
                    if(!frustum_sphere(planes,e->phys.pos.x,e->phys.pos.y,e->phys.pos.z,3.0f)) continue;
                    if(dist>80.0f) continue;
                    {
                        const EnemyDef *d=e->has_stat_override?&e->stat_override:&EDEF[e->etype];
                        CharModel *em=use_custom?&enemy_custom_models[ci]:&enemy_models[e->etype];
                        enemy_refresh_eval(e,em,dist,frame_count);
                        {
                            float extra=(use_custom&&enemy_custom_scale&&ci<enemy_custom_count)?enemy_custom_scale[ci]:1.0f;
                            float sc=em->base.model_scale*d->scale*extra;
                            Mat4 wm=entity_mat(e->phys.pos,e->phys.yaw,em->base.ground_offset,
                                               sc,em->base.cx,em->base.cy,em->base.cz);
                            GLH_vec3(skin->prog,"uBaseColor",d->cr,d->cg,d->cb);
                            {
                                int slot=e->anim_idx;
                                if(slot<0||slot>=ANIM_SLOTS) slot=0;
                                AnimEntry *ae=&em->anims[slot];
                                Scene *scn=ae->scene?ae->scene:&em->base;
                                draw_scene_cached(skin,scn,wm,e->anim_eval,
                                                  e->state==ES_STAGGER?0.4f:0.0f,
                                                  cam_pos,proj,view,ls);
                            }
                        }
                    }
                }
            }
        }

        if(has_npc_model&&npc_fallback_model>=0){
            GLH_vec3(skin->prog,"uBaseColor",0.85f,0.80f,0.72f);
            for(int i=0;i<npcs->count;i++){
                NpcDef *n=&npcs->npcs[i];
                float dist=vec3_dist(n->pos,cam_pos);
                if(dist>90.0f) continue;
                if(!frustum_sphere(planes,n->pos.x,n->pos.y,n->pos.z,2.0f)) continue;
                {
                    int mi=n->model_idx;
                    if(mi<0||mi>=NPC_MODEL_MAX||!npc_model_ok[mi]) mi=npc_fallback_model;
                    {
                        CharModel *cm=&npc_models[mi];
                        float npc_scale=(n->scale>0.0f)?n->scale:1.0f;
                        float sc=cm->base.model_scale*npc_scale;
                        Mat4 wm=entity_mat(n->pos,n->yaw+n->visual_yaw_offset,cm->base.ground_offset*npc_scale,
                                           sc,cm->base.cx,cm->base.cy,cm->base.cz);
                        Scene *nsc=NULL;
                        int nstack=-1;
                        npc_refresh_eval(n,cm,dist,frame_count);
                        charmodel_anim_resolve(cm,n->anim_idx,&nsc,&nstack);
                        if(nsc) draw_scene_cached(skin,nsc,wm,n->anim_eval,0.0f,
                                                  cam_pos,proj,view,ls);
                        else draw_charmodel(skin,cm,wm,n->anim_idx,n->anim_time,0.0f,
                                            proj,view,cam_pos,ls);
                    }
                }
            }
        }
    }
}
