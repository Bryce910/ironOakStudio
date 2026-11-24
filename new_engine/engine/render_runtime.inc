static void ensure_enemy_model_loaded(EnemyType etype, CharModel enemy_models[],
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

extern void charmodel_anim_resolve(CharModel *cm,int anim_slot,Scene **out_scene,int *out_stack);
extern void player_refresh_eval(Player *player,CharModel *cm,int frame);
extern void npc_refresh_eval(NpcDef *npc,CharModel *cm,float dist,int frame);

static void ensure_enemy_models_near_camera(Enemy *enemies,int ne, Vec3 cam_pos,
                                            CharModel enemy_models[],
                                            bool enemy_model_loaded[],
                                            CharModel *fallback_char_model){
    for(int i=0;i<ne;i++){
        Enemy *e=&enemies[i];
        if(!e->active) continue;
        if(vec3_dist(e->phys.pos,cam_pos)<130.0f)
            ensure_enemy_model_loaded(e->etype,enemy_models,enemy_model_loaded,fallback_char_model);
    }
}

static void render_shadow_pass_runtime(ShadowMap *sm, ShadowMap *sm_wide,
                                       LightingSystem *ls, Terrain *terrain,
                                       PropList *props, Camera3P *cam,
                                       Player *player, CharModel *char_model,
                                       Enemy *enemies, int ne,
                                       CharModel enemy_models[],
                                       bool enemy_model_loaded[],
                                       CharModel npc_models[],
                                       bool npc_model_ok[],
                                       int frame_count, NpcSystem *npcs,
                                       bool has_npc_model, int win_w, int win_h){
    Vec3 shadow_center=cam->target;
    shadow_begin(sm,ls,shadow_center);
    shadow_terrain(sm,ls,terrain);
    props_shadow(props,sm,ls,cam->pos);
    {
        Mat4 wm=entity_mat(player->phys.pos,player->phys.yaw,
                           char_model->base.ground_offset,char_model->base.model_scale,
                           char_model->base.cx,char_model->base.cy,char_model->base.cz);
        player_refresh_eval(player,char_model,frame_count);
        {
            Scene *psc=NULL;
            int pstack=-1;
            charmodel_anim_resolve(char_model,player->anim_idx,&psc,&pstack);
            if(psc) shadow_scene_cached(sm,ls,psc,wm,player->anim_eval);
            else shadow_charmodel(sm,ls,char_model,wm,player->anim_idx,player->anim_time);
        }
    }
    for(int i=0;i<ne;i++){
        Enemy *e=&enemies[i];
        if(!e->active) continue;
        {
            float dist=vec3_dist(e->phys.pos,player->phys.pos);
            if(dist>80.0f||!enemy_model_loaded[e->etype]) continue;
            {
                CharModel *em=&enemy_models[e->etype];
                enemy_refresh_eval(e,em,dist,frame_count);
                if(!e->anim_eval&&dist>2.0f) continue;
                {
                    const EnemyDef *d=&EDEF[e->etype];
                    float sc=em->base.model_scale*d->scale;
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
    if(has_npc_model){
        for(int i=0;i<npcs->count;i++){
            NpcDef *n=&npcs->npcs[i];
            float dist=vec3_dist(n->pos,player->phys.pos);
            if(dist>60.0f) continue;
            {
                int mi=n->model_idx;
                if(mi<0||mi>=NPC_MODEL_MAX||!npc_model_ok[mi]) mi=0;
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

    if(frame_count%3==0){
        shadow_begin_wide(sm_wide,ls,shadow_center);
        glViewport(0,0,1024,1024);
        /* Wide pass must render with the wide VP, not the near one.
           Temporarily swap so shadow_terrain/props_shadow pick up the right matrix. */
        { Mat4 saved_near=ls->light_vp; ls->light_vp=ls->light_vp_wide;
        shadow_terrain(sm_wide,ls,terrain);
        props_shadow(props,sm_wide,ls,cam->pos);
        ls->light_vp=saved_near; }
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,win_w,win_h);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glCullFace(GL_BACK);
        ls->shadow_tex_wide=sm_wide->tex;
    }
}

static void render_character_runtime(SkinProg *skin, Mat4 proj, Mat4 view,
                                     Vec3 cam_pos, LightingSystem *ls,
                                     Player *player, CharModel *char_model,
                                     Enemy *enemies, int ne,
                                     CharModel enemy_models[],
                                     bool enemy_model_loaded[],
                                     CharModel npc_models[],
                                     bool npc_model_ok[],
                                     int frame_count, NpcSystem *npcs,
                                     bool has_npc_model){
    {
        Mat4 wm=entity_mat(player->phys.pos,player->phys.yaw,
                           char_model->base.ground_offset,char_model->base.model_scale,
                           char_model->base.cx,char_model->base.cy,char_model->base.cz);
        player_refresh_eval(player,char_model,frame_count);
        {
            Scene *psc=NULL;
            int pstack=-1;
            charmodel_anim_resolve(char_model,player->anim_idx,&psc,&pstack);
            if(psc){
                draw_scene_cached(skin,psc,wm,player->anim_eval,
                                  player->hit_flash,cam_pos,proj,view,ls);
                draw_charmodel_equipment(skin,NULL,psc,wm,pstack,
                                         player->anim_time,proj,view,cam_pos,ls);
            }else{
                draw_charmodel(skin,char_model,wm,player->anim_idx,player->anim_time,
                               player->hit_flash,proj,view,cam_pos,ls);
                draw_charmodel_equipment(skin,NULL,&char_model->base,wm,
                                         player->anim_idx,player->anim_time,
                                         proj,view,cam_pos,ls);
            }
        }
    }

    {
        Mat4 pv=mat4_mul(proj,view);
        float planes[6][4];
        frustum_extract(planes,pv);
        for(int i=0;i<ne;i++){
            Enemy *e=&enemies[i];
            if(!e->active||!enemy_model_loaded[e->etype]) continue;
            {
                float dist=vec3_dist(e->phys.pos,cam_pos);
                if(dist>110.0f) continue;
                if(!frustum_sphere(planes,e->phys.pos.x,e->phys.pos.y,e->phys.pos.z,3.0f)) continue;
                if(dist>80.0f) continue;
                {
                    const EnemyDef *d=&EDEF[e->etype];
                    CharModel *em=&enemy_models[e->etype];
                    enemy_refresh_eval(e,em,dist,frame_count);
                    {
                        float sc=em->base.model_scale*d->scale;
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

        if(has_npc_model){
            GLH_vec3(skin->prog,"uBaseColor",0.85f,0.80f,0.72f);
            for(int i=0;i<npcs->count;i++){
                NpcDef *n=&npcs->npcs[i];
                float dist=vec3_dist(n->pos,cam_pos);
                if(dist>90.0f) continue;
                if(!frustum_sphere(planes,n->pos.x,n->pos.y,n->pos.z,2.0f)) continue;
                {
                    int mi=n->model_idx;
                    if(mi<0||mi>=NPC_MODEL_MAX||!npc_model_ok[mi]) mi=0;
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
