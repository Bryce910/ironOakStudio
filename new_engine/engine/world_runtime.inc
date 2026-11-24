static void world_runtime_update(WorldItemList *world_items, Inventory *inv,
                                 QuestSystem *qs, SaveNotify *save_note,
                                 NpcSystem *npcs, bool has_npc_model,
                                 AnimalSystem *animals, ParticleSystem *ps,
                                 Player *player, AnimMap *player_amap,
                                 float *death_fade, bool npc_dialogue_capture,
                                 float dt){
    {
        const uint8_t *ks=SDL_GetKeyboardState(NULL);
        bool press_e=!npc_dialogue_capture&&ks[SDL_SCANCODE_E]&&!inv->open;
        wi_update(world_items,dt,player->phys.pos,inv,qs,save_note,press_e);
        prop_pickable_update(&g_props,player->phys.pos,inv,qs,save_note,press_e);
        farming_update(&g_farming,dt,player->phys.pos,inv,save_note,press_e);
    }

    if(has_npc_model){
        for(int i=0;i<npcs->count;i++) npcs->npcs[i].anim_time+=dt;
    }
    npc_patrol_update(npcs,dt,player->phys.pos);

    animal_system_update(animals,dt,player->phys.pos);
    mount_system_update(&g_mounts,player,dt,player_amap);
    if(animals->pending_damage>0&&player->state!=PS_DEAD&&player->dash_iframes<=0){
        float shield_reduce=(player->shield_timer>0)?0.75f:0.0f;
        float block_reduce =(player->blocking)      ?0.60f:0.0f;
        float total_reduce=clampf(shield_reduce+block_reduce,0,0.90f);
        player->health=clampf(player->health-animals->pending_damage*(1.0f-total_reduce),0,player->max_health);
        player->hit_flash=0.7f;
        ps_blood(ps,player->phys.pos,0.7f,8);
        audio_play(SND_HIT,80);
        if(player->health<=0){
            player->health=0;
            player->state=PS_DEAD;
            *death_fade=0.0f;
            player->anim_idx=player_amap->death;
            player->anim_time=0;
            ps_blood(ps,player->phys.pos,1.0f,24);
            audio_play(SND_DEATH,110);
        }
    }
    if(player->state==PS_DEAD&&mount_system_player_mounted(&g_mounts))
        mount_system_dismount(&g_mounts,player,player_amap,NULL);

    interior_check(player->phys.pos);
    g_interior_enter_timer=clampf(g_interior_enter_timer-dt,0,4.0f);

    /* Scene system: drives the mystical apartment→Shire transition */
    scene_system_update_frame(&g_scene_sys,dt,ps,player->phys.pos,player);
}
