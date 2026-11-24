static void editor_shell_reset_modes(EditorShellActionContext *ctx){
    EditorRuntimeState *editor=ctx?ctx->editor:NULL;
    if(!editor) return;
    editor->path_edit=false;
    editor->wzone_edit=false;
    editor->wf_edit_mode=false;
    editor->river_edit=false;
    editor->prop_edit=false;
    editor->show_debug=false;
    editor->prop_place_mode=false;
    editor->surface_edit=false;
    editor->collision_edit=false;
    editor->particle_fx_edit=false;
    editor->marker_edit=false;
    editor->forest_edit=false;
    editor->wind_edit=false;
    editor->sculpt_edit=false;
    editor->npc_edit_mode=false;
    editor->npc_place_mode=false;
    editor->spawn_edit_mode=false;
    editor->shader_edit=false;
    editor->plight_edit=false;
    editor->decal_edit=false;
    editor->szone_edit=false;
    editor->interior_edit=false;
    editor->quest_edit_mode=false;
    editor->main_character_edit=false;
}

static Vec3 editor_scene_default_target(Vec3 fallback){
    if(g_scene_sys.current==SCENE_APARTMENT)
        return (Vec3){APARTMENT_CX,g_apartment_floor_y+1.0f,APARTMENT_CZ};
    return fallback;
}

static void interior_material_assignments_save(void);
static int interior_active_index(void);
static int interior_material_count(void);
static unsigned int interior_material_surface_flags(int idx);
static int interior_material_first_for_surface(int surface);
static int interior_material_cycle_for_surface(int start,int surface,int dir);
static const char *interior_material_name(int idx);
static const char *interior_name(int idx);
static int interior_material_assigned_to_surface(int interior_idx,int surface);
static const char *interior_surface_name(int surface);

static int editor_shell_wrap_interior_surface(int current,int delta){
    int next=(current+delta)%3;
    if(next<0) next+=3;
    return next;
}

static unsigned int editor_shell_interior_surface_flag(int surface){
    switch(surface){
        case 1: return (1u<<1);
        case 2: return (1u<<2);
        default: return (1u<<0);
    }
}

static void editor_shell_sync_interior_material(EditorRuntimeState *editor){
    int assigned;
    if(!editor) return;
    assigned=interior_material_assigned_to_surface(editor->interior_selected_idx,editor->interior_surface_target);
    if(assigned>=0){
        editor->interior_material_sel=assigned;
    }else if(interior_material_count()>0){
        editor->interior_material_sel=interior_material_first_for_surface(editor->interior_surface_target);
    }
}

static void editor_shell_select_interior_material(void *userdata,int index){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->editor) return;
    if(!interior_material_supports_surface(index,ctx->editor->interior_surface_target)) return;
    ctx->editor->interior_material_sel=index;
}

static int editor_shell_active_surface_piece_index(EditorShellActionContext *ctx){
    int idx;
    if(!ctx||!ctx->selection||ctx->selection->kind!=EDITOR_SELECTION_SURFACE) return -1;
    idx=ctx->selection->index;
    return surface_piece_query(idx,NULL,NULL,NULL,NULL,NULL)?idx:-1;
}

static void editor_shell_select_surface_material(void *userdata,int index){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int sp_idx=editor_shell_active_surface_piece_index(ctx);
    int sp_kind=0;
    if(!ctx||!ctx->editor) return;
    if(index<0||index>=interior_material_count()) return;
    if(sp_idx>=0) surface_piece_query(sp_idx,&sp_kind,NULL,NULL,NULL,NULL);
    if((interior_material_surface_flags(index)&surface_piece_surface_flag(ctx->editor->surface_kind))==0 &&
       (sp_idx<0||(interior_material_surface_flags(index)&surface_piece_surface_flag(sp_kind))==0)) return;
    if(sp_idx>=0){
        editor_command_surface_set_material(sp_idx,index,NULL);
    }else{
        ctx->editor->surface_material_sel=index;
    }
}

static void editor_shell_set_surface_kind(void *userdata,int kind){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->editor) return;
    kind=(kind==1)?1:0;
    ctx->editor->surface_kind=kind;
    if(editor_shell_active_surface_piece_index(ctx)>=0)
        editor_command_surface_set_kind(editor_shell_active_surface_piece_index(ctx),kind,NULL);
    if((interior_material_surface_flags(ctx->editor->surface_material_sel)&surface_piece_surface_flag(kind))==0)
        ctx->editor->surface_material_sel=surface_piece_default_material(kind);
}

static void editor_shell_set_surface_width(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int sp_idx=editor_shell_active_surface_piece_index(ctx);
    value=clampf(value,0.25f,64.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->surface_width=value;
    if(sp_idx>=0) editor_command_surface_set_width(sp_idx,value,NULL);
}

static void editor_shell_set_surface_height(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int sp_idx=editor_shell_active_surface_piece_index(ctx);
    value=clampf(value,0.25f,64.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->surface_height=value;
    if(sp_idx>=0) editor_command_surface_set_height(sp_idx,value,NULL);
}

static void editor_shell_set_surface_thickness(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int sp_idx=editor_shell_active_surface_piece_index(ctx);
    value=clampf(value,0.05f,8.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->surface_thickness=value;
    if(sp_idx>=0) editor_command_surface_set_thickness(sp_idx,value,NULL);
}

static void editor_shell_set_surface_collision(void *userdata,int coll_type){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int sp_idx=editor_shell_active_surface_piece_index(ctx);
    coll_type=(coll_type==PROP_COLL_NONE)?PROP_COLL_NONE:PROP_COLL_AABB;
    if(!ctx||!ctx->editor) return;
    ctx->editor->surface_coll_type=coll_type;
    if(sp_idx>=0) editor_command_surface_set_collision(sp_idx,coll_type,NULL);
}

static int editor_shell_active_collision_volume_index(EditorShellActionContext *ctx){
    int idx;
    if(!ctx||!ctx->selection||ctx->selection->kind!=EDITOR_SELECTION_COLLISION_VOLUME) return -1;
    idx=ctx->selection->index;
    return collision_volume_query(idx,NULL,NULL,NULL,NULL,NULL)?idx:-1;
}

static void editor_shell_set_collision_volume_width(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    value=clampf(value,0.25f,64.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_width=value;
    if(idx>=0) collision_volume_set_width_value(idx,value);
}

static void editor_shell_set_collision_volume_height(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    value=clampf(value,0.25f,64.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_height=value;
    if(idx>=0) collision_volume_set_height_value(idx,value);
}

static void editor_shell_set_collision_volume_depth(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    value=clampf(value,0.25f,64.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_depth=value;
    if(idx>=0) collision_volume_set_depth_value(idx,value);
}

static void editor_shell_set_collision_volume_shape(void *userdata,int shape){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_shape=shape;
    if(shape==1) ctx->editor->collision_place_pitch=0.0f;
    if(idx>=0) collision_volume_set_shape_value(idx,shape);
}

static void editor_shell_set_collision_volume_yaw(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_place_yaw=value;
    if(idx>=0) collision_volume_set_yaw_value(idx,value);
}

static void editor_shell_set_collision_volume_pitch(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    value=clampf(value,-89.0f,89.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_place_pitch=value;
    if(idx>=0) collision_volume_set_pitch_value(idx,value);
}

static void editor_shell_set_collision_volume_enabled(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_enabled=enabled;
    if(idx>=0) collision_volume_set_enabled_value(idx,enabled);
}

static void editor_shell_set_collision_volume_nav_blocker(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_nav_blocker=enabled;
    if(idx>=0) collision_volume_set_nav_blocker_value(idx,enabled);
}

static void editor_shell_set_collision_volume_motion_enabled(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_motion_enabled=enabled;
    if(idx>=0) collision_volume_set_motion_enabled_value(idx,enabled);
}

static void editor_shell_set_collision_volume_motion_axis(void *userdata,int axis,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    if(!ctx||!ctx->editor) return;
    if(axis==0) ctx->editor->collision_motion_x=value;
    else if(axis==1) ctx->editor->collision_motion_y=value;
    else if(axis==2) ctx->editor->collision_motion_z=value;
    else return;
    if(idx>=0) collision_volume_set_motion_offset_axis(idx,axis,value);
}

static void editor_shell_set_collision_volume_motion_period(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    value=fmaxf(0.1f,value);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_motion_period=value;
    if(idx>=0) collision_volume_set_motion_period_value(idx,value);
}

static void editor_shell_set_collision_volume_motion_phase(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_collision_volume_index(ctx);
    if(!ctx||!ctx->editor) return;
    ctx->editor->collision_motion_phase=value;
    if(idx>=0) collision_volume_set_motion_phase_value(idx,value);
}

static int editor_shell_active_plight_index(EditorShellActionContext *ctx){
    if(!ctx||!ctx->editor) return -1;
    if(ctx->editor->plight_sel<0||ctx->editor->plight_sel>=g_placed_lights.count) return -1;
    if(!g_placed_lights.items[ctx->editor->plight_sel].active) return -1;
    return ctx->editor->plight_sel;
}

static float editor_shell_current_plight_radius(EditorShellActionContext *ctx){
    int idx=editor_shell_active_plight_index(ctx);
    if(idx>=0) return g_placed_lights.items[idx].radius;
    return (ctx&&ctx->editor)?ctx->editor->plight_place_radius:0.0f;
}

static float editor_shell_current_plight_intensity(EditorShellActionContext *ctx){
    int idx=editor_shell_active_plight_index(ctx);
    if(idx>=0) return g_placed_lights.items[idx].intensity;
    return (ctx&&ctx->editor)?ctx->editor->plight_place_intensity:0.0f;
}

static float editor_shell_current_plight_y_offset(EditorShellActionContext *ctx){
    int idx=editor_shell_active_plight_index(ctx);
    if(idx>=0) return g_placed_lights.items[idx].y_offset;
    return (ctx&&ctx->editor)?ctx->editor->plight_place_y_offset:0.0f;
}

static float editor_shell_current_plight_color(EditorShellActionContext *ctx,int channel){
    int idx=editor_shell_active_plight_index(ctx);
    if(idx>=0){
        if(channel==0) return g_placed_lights.items[idx].color.x;
        if(channel==1) return g_placed_lights.items[idx].color.y;
        return g_placed_lights.items[idx].color.z;
    }
    if(!ctx||!ctx->editor) return 0.0f;
    if(channel==0) return ctx->editor->plight_place_r;
    if(channel==1) return ctx->editor->plight_place_g;
    return ctx->editor->plight_place_b;
}

static bool editor_shell_current_plight_flicker(EditorShellActionContext *ctx){
    int idx=editor_shell_active_plight_index(ctx);
    if(idx>=0) return g_placed_lights.items[idx].flicker;
    return (ctx&&ctx->editor)?ctx->editor->plight_place_flicker:false;
}

static void editor_shell_sync_plight_runtime(EditorShellActionContext *ctx,int idx){
    LightingSystem *ls=ctx?(LightingSystem*)ctx->lighting:NULL;
    if(idx<0||idx>=g_placed_lights.count) return;
    placed_light_runtime_sync(&g_placed_lights,ls,idx);
    placed_lights_save(&g_placed_lights);
}

static void editor_shell_commit_plight_edit(EditorShellActionContext *ctx,int idx,PlacedLight before){
    if(idx<0||idx>=g_placed_lights.count) return;
    undo_push_plight_edit(idx,before,g_placed_lights.items[idx]);
    editor_shell_sync_plight_runtime(ctx,idx);
}

static void editor_shell_plight_set_color(void *userdata,int channel,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    value=clampf(value,0.0f,1.0f);
    if(!ctx||!ctx->editor) return;
    if(channel==0) ctx->editor->plight_place_r=value;
    else if(channel==1) ctx->editor->plight_place_g=value;
    else ctx->editor->plight_place_b=value;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    if(channel==0) g_placed_lights.items[idx].color.x=value;
    else if(channel==1) g_placed_lights.items[idx].color.y=value;
    else g_placed_lights.items[idx].color.z=value;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_plight_set_type(void *userdata,int type){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    if(!ctx||!ctx->editor) return;
    if(type<0) type=0;
    if(type>1) type=1;
    ctx->editor->plight_place_type=type;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    g_placed_lights.items[idx].type=type;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_plight_set_position(void *userdata,int axis,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    if(!ctx||!ctx->editor||axis<0||axis>2) return;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    if(axis==0) g_placed_lights.items[idx].pos.x=value;
    else if(axis==1) g_placed_lights.items[idx].pos.y=value;
    else g_placed_lights.items[idx].pos.z=value;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_plight_set_radius(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    value=clampf(value,2.0f,60.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->plight_place_radius=value;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    g_placed_lights.items[idx].radius=value;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_plight_set_intensity(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    value=clampf(value,0.05f,4.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->plight_place_intensity=value;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    g_placed_lights.items[idx].intensity=value;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_plight_set_y_offset(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    value=clampf(value,0.0f,20.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->plight_place_y_offset=value;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    g_placed_lights.items[idx].y_offset=value;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_plight_set_flicker(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    if(!ctx||!ctx->editor) return;
    ctx->editor->plight_place_flicker=enabled;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    g_placed_lights.items[idx].flicker=enabled;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_plight_set_flicker_speed(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    value=clampf(value,0.5f,20.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->plight_place_flicker_speed=value;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    g_placed_lights.items[idx].flicker_speed=value;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_plight_set_spot_angles(void *userdata,float yaw_deg,float pitch_deg){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    if(!ctx||!ctx->editor) return;
    if(yaw_deg<-180.0f) yaw_deg=-180.0f;
    if(yaw_deg>180.0f) yaw_deg=180.0f;
    pitch_deg=clampf(pitch_deg,-89.0f,89.0f);
    ctx->editor->plight_place_spot_yaw=yaw_deg;
    ctx->editor->plight_place_spot_pitch=pitch_deg;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    g_placed_lights.items[idx].spot_yaw_deg=yaw_deg;
    g_placed_lights.items[idx].spot_pitch_deg=pitch_deg;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_plight_set_spot_cone(void *userdata,float inner_deg,float outer_deg){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_plight_index(ctx);
    PlacedLight before;
    inner_deg=clampf(inner_deg,1.0f,85.0f);
    outer_deg=clampf(outer_deg,2.0f,89.0f);
    if(outer_deg<inner_deg+1.0f) outer_deg=inner_deg+1.0f;
    if(!ctx||!ctx->editor) return;
    ctx->editor->plight_place_spot_inner_deg=inner_deg;
    ctx->editor->plight_place_spot_outer_deg=outer_deg;
    if(idx<0) return;
    before=g_placed_lights.items[idx];
    g_placed_lights.items[idx].spot_inner_deg=inner_deg;
    g_placed_lights.items[idx].spot_outer_deg=outer_deg;
    editor_shell_commit_plight_edit(ctx,idx,before);
}

static void editor_shell_world_light_set_override(void *userdata,bool enabled){
    (void)userdata;
    g_world_light_cfg.override_active=enabled;
    world_lighting_config_save(&g_world_light_cfg);
}

static void editor_shell_world_light_set_angles(void *userdata,float yaw_deg,float pitch_deg){
    (void)userdata;
    if(yaw_deg<-180.0f) yaw_deg=-180.0f;
    if(yaw_deg>180.0f) yaw_deg=180.0f;
    pitch_deg=clampf(pitch_deg,-89.0f,89.0f);
    g_world_light_cfg.sun_yaw_deg=yaw_deg;
    g_world_light_cfg.sun_pitch_deg=pitch_deg;
    world_lighting_config_save(&g_world_light_cfg);
}

static void editor_shell_world_light_set_sun_color(void *userdata,int channel,float value){
    (void)userdata;
    value=clampf(value,0.0f,3.0f);
    if(channel==0) g_world_light_cfg.sun_color.x=value;
    else if(channel==1) g_world_light_cfg.sun_color.y=value;
    else if(channel==2) g_world_light_cfg.sun_color.z=value;
    else return;
    world_lighting_config_save(&g_world_light_cfg);
}

static void editor_shell_world_light_set_ambient_color(void *userdata,int channel,float value){
    (void)userdata;
    value=clampf(value,0.0f,1.5f);
    if(channel==0) g_world_light_cfg.ambient.x=value;
    else if(channel==1) g_world_light_cfg.ambient.y=value;
    else if(channel==2) g_world_light_cfg.ambient.z=value;
    else return;
    world_lighting_config_save(&g_world_light_cfg);
}

static bool editor_plight_delete_index(EditorRuntimeState *editor,LightingSystem *ls,int idx){
    UndoSnap snap;
    if(idx<0||idx>=g_placed_lights.count||!g_placed_lights.items[idx].active) return false;
    memset(&snap,0,sizeof(snap));
    snap.plight=g_placed_lights.items[idx];
    undo_push_delete(UNDO_PLIGHT_DELETE,idx,g_placed_lights.count,snap);
    if(ls&&g_placed_lights.items[idx].light_idx>=0&&g_placed_lights.items[idx].light_idx<ls->num_points)
        lighting_disable_point(ls,g_placed_lights.items[idx].light_idx);
    g_placed_lights.items[idx].active=false;
    if(editor) editor->plight_sel=-1;
    placed_lights_save(&g_placed_lights);
    return true;
}

static void editor_shell_plight_delete_selected(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    LightingSystem *ls=ctx?(LightingSystem*)ctx->lighting:NULL;
    int idx=editor_shell_active_plight_index(ctx);
    if(!ctx||idx<0) return;
    editor_plight_delete_index(ctx->editor,ls,idx);
}

static bool editor_decal_delete_index(EditorRuntimeState *editor,int idx){
    UndoSnap snap;
    if(idx<0||idx>=g_decals.count||!g_decals.items[idx].active) return false;
    memset(&snap,0,sizeof(snap));
    snap.decal=g_decals.items[idx];
    undo_push_delete(UNDO_DECAL_DELETE,idx,g_decals.count,snap);
    g_decals.items[idx].active=false;
    g_decals.gpu_dirty=true;
    decals_rebuild_vbo(&g_decals);
    if(editor) editor->decal_sel=-1;
    decals_save(&g_decals);
    return true;
}

static bool editor_szone_delete_index(EditorRuntimeState *editor,int idx){
    UndoSnap snap;
    if(idx<0||idx>=g_sound_zones.count||!g_sound_zones.items[idx].active) return false;
    memset(&snap,0,sizeof(snap));
    snap.szone=g_sound_zones.items[idx];
    undo_push_delete(UNDO_SZONE_DELETE,idx,g_sound_zones.count,snap);
    g_sound_zones.items[idx].active=false;
    if(editor) editor->szone_sel=-1;
    sound_zones_save(&g_sound_zones);
    return true;
}

static int editor_shell_active_szone_index(EditorShellActionContext *ctx){
    if(!ctx||!ctx->editor) return -1;
    if(ctx->editor->szone_sel<0||ctx->editor->szone_sel>=g_sound_zones.count) return -1;
    if(!g_sound_zones.items[ctx->editor->szone_sel].active) return -1;
    return ctx->editor->szone_sel;
}

static void editor_shell_szone_sync_defaults(EditorRuntimeState *editor,const SoundZone *zone){
    if(!editor||!zone) return;
    editor->szone_type=(zone->type==SOUND_ZONE_TYPE_MUSIC)?SOUND_ZONE_TYPE_MUSIC:SOUND_ZONE_TYPE_AMBIENT;
    editor->szone_sound_idx=(zone->sound_idx>=0&&zone->sound_idx<SZONE_SOUND_COUNT)?zone->sound_idx:0;
    editor->szone_music_idx=0;
    if(zone->type==SOUND_ZONE_TYPE_MUSIC){
        int music_idx=sound_zone_music_index_from_path(zone->music_path);
        if(music_idx>=0) editor->szone_music_idx=music_idx;
    }
    editor->szone_place_radius=zone->radius;
    editor->szone_place_vol=zone->volume;
    editor->szone_fade_len=zone->fade_len;
}

static void editor_shell_szone_set_type(void *userdata,int type){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_szone_index(ctx);
    if(!ctx||!ctx->editor) return;
    ctx->editor->szone_type=(type==SOUND_ZONE_TYPE_MUSIC)?SOUND_ZONE_TYPE_MUSIC:SOUND_ZONE_TYPE_AMBIENT;
    if(idx<0) return;
    g_sound_zones.items[idx].type=ctx->editor->szone_type;
    if(g_sound_zones.items[idx].type==SOUND_ZONE_TYPE_MUSIC&&ctx->editor->szone_music_idx>=0&&ctx->editor->szone_music_idx<g_sound_zone_music_option_count){
        snprintf(g_sound_zones.items[idx].music_path,sizeof(g_sound_zones.items[idx].music_path),"%s",
                 g_sound_zone_music_paths[ctx->editor->szone_music_idx]);
    }
    sound_zones_save(&g_sound_zones);
}

static void editor_shell_szone_set_position(void *userdata,int axis,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_szone_index(ctx);
    if(!ctx||!ctx->editor||idx<0) return;
    if(axis!=0&&axis!=2) return;
    if(axis==0) g_sound_zones.items[idx].cx=value;
    else g_sound_zones.items[idx].cz=value;
    sound_zones_save(&g_sound_zones);
}

static void editor_shell_szone_set_ambient_sound(void *userdata,int sound_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_szone_index(ctx);
    if(!ctx||!ctx->editor) return;
    if(sound_idx<0||sound_idx>=SZONE_SOUND_COUNT) return;
    ctx->editor->szone_sound_idx=sound_idx;
    if(idx<0) return;
    g_sound_zones.items[idx].sound_idx=sound_idx;
    sound_zones_save(&g_sound_zones);
}

static void editor_shell_szone_set_music(void *userdata,int music_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_szone_index(ctx);
    if(!ctx||!ctx->editor) return;
    if(music_idx<0||music_idx>=g_sound_zone_music_option_count) return;
    ctx->editor->szone_music_idx=music_idx;
    if(idx<0) return;
    snprintf(g_sound_zones.items[idx].music_path,sizeof(g_sound_zones.items[idx].music_path),"%s",
             g_sound_zone_music_paths[music_idx]);
    sound_zones_save(&g_sound_zones);
}

static void editor_shell_szone_set_radius(void *userdata,float radius){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_szone_index(ctx);
    radius=clampf(radius,4.0f,160.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->szone_place_radius=radius;
    if(ctx->editor->szone_fade_len>radius) ctx->editor->szone_fade_len=radius;
    if(idx<0) return;
    g_sound_zones.items[idx].radius=radius;
    if(g_sound_zones.items[idx].fade_len>radius) g_sound_zones.items[idx].fade_len=radius;
    sound_zones_save(&g_sound_zones);
}

static void editor_shell_szone_set_volume(void *userdata,float volume){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_szone_index(ctx);
    volume=clampf(volume,0.0f,100.0f);
    if(!ctx||!ctx->editor) return;
    ctx->editor->szone_place_vol=volume;
    if(idx<0) return;
    g_sound_zones.items[idx].volume=volume;
    sound_zones_save(&g_sound_zones);
}

static void editor_shell_szone_set_fade_length(void *userdata,float fade_length){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_szone_index(ctx);
    if(!ctx||!ctx->editor) return;
    fade_length=clampf(fade_length,0.0f,ctx->editor->szone_place_radius);
    ctx->editor->szone_fade_len=fade_length;
    if(idx<0) return;
    g_sound_zones.items[idx].fade_len=clampf(fade_length,0.0f,g_sound_zones.items[idx].radius);
    sound_zones_save(&g_sound_zones);
}

static void editor_shell_szone_delete_selected(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_szone_index(ctx);
    if(!ctx||idx<0) return;
    editor_szone_delete_index(ctx->editor,idx);
}

static void editor_shell_set_editor_camera(EditorShellActionContext *ctx,float min_dist){
    EditorRuntimeState *editor=ctx?ctx->editor:NULL;
    if(!ctx||!editor||!ctx->cam) return;
    editor->editor_mode=true;
    editor->editor_cam_target=editor_scene_default_target(ctx->cam->target);
    ctx->cam->dist=clampf(ctx->cam->dist,min_dist,320.0f);
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

static void editor_shell_save_world(EditorShellActionContext *ctx){
    ForestSystem *forest=(ForestSystem*)ctx->forest;
    WorldMarkerList *world_markers=(WorldMarkerList*)ctx->world_markers;
    WorldFxSystem *world_fx=(WorldFxSystem*)ctx->world_fx;
    SpawnList *spawns=(SpawnList*)ctx->spawns;
    NpcSpawnList *npc_spawns=(NpcSpawnList*)ctx->npc_spawns;

    sculpt_save();
    cpath_save();
    wzone_save();
    terrain_rebuild_all(ctx->terrain);
    wzone_rebuild_vaos(ctx->terrain);
    wf_save();
    criver_save();
    placer_save(&g_props);
    forest_save(forest);
    grass_mask_save();
    flower_mask_save();
    worn_mask_save();
    terrain_style_mask_save();
    farm_field_mask_save();
    windzone_save();
    placed_lights_save(&g_placed_lights);
    decals_save(&g_decals);
    sound_zones_save(&g_sound_zones);
    world_lighting_config_save(&g_world_light_cfg);
    if(ctx->wetness&&ctx->fog_density&&ctx->overcast)
        visual_profile_capture_weather(*ctx->wetness,*ctx->fog_density,*ctx->overcast);
    visual_profile_save();
    interior_material_assignments_save();
    surface_piece_save_active_scene();
    collision_volume_save_active_scene();
    world_markers_save(world_markers);
    world_fx_save(world_fx);
    spawns_save(spawns);
    npc_spawns_save(npc_spawns);
    player_character_library_save(PLAYER_CHARACTER_FILE);
    terrain_rebake_hmap(ctx->terrain);
    save_notify(ctx->save_note,"World data saved");
}

static void editor_shell_player_char_select_preset(void *userdata,int preset_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PlayerCharacterConfig *cfg;
    if(!ctx||!ctx->player_char_editor_index) return;
    if(preset_idx<0||preset_idx>=g_player_char_library.count) return;
    *ctx->player_char_editor_index=preset_idx;
    cfg=player_character_selected_preset();
    if(cfg&&ctx->char_model&&ctx->player_amap)
        player_character_reload_runtime(ctx->char_model,ctx->player_amap,ctx->player,cfg);
}

static void editor_shell_player_char_add_preset(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->player_char_editor_index) return;
    if(g_player_char_library.count>=PLAYER_CHARACTER_PRESET_MAX) return;
    int idx=g_player_char_library.count++;
    player_character_config_defaults(&g_player_char_presets[idx]);
    snprintf(g_player_char_presets[idx].name,sizeof(g_player_char_presets[idx].name),"Character %d",idx+1);
    *ctx->player_char_editor_index=idx;
    if(ctx->char_model&&ctx->player_amap)
        player_character_reload_runtime(ctx->char_model,ctx->player_amap,ctx->player,&g_player_char_presets[idx]);
}

static void editor_shell_player_char_duplicate_preset(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int src;
    char src_name[sizeof(g_player_char_presets[0].name)];
    if(!ctx||!ctx->player_char_editor_index) return;
    if(g_player_char_library.count>=PLAYER_CHARACTER_PRESET_MAX) return;
    src=*ctx->player_char_editor_index;
    if(src<0||src>=g_player_char_library.count) return;
    strncpy(src_name,g_player_char_presets[src].name,sizeof(src_name)-1);
    src_name[sizeof(src_name)-1]=0;
    g_player_char_presets[g_player_char_library.count]=g_player_char_presets[src];
    int prefix_len=(int)sizeof(g_player_char_presets[g_player_char_library.count].name)-6;
    if(prefix_len<0) prefix_len=0;
    snprintf(g_player_char_presets[g_player_char_library.count].name,
             sizeof(g_player_char_presets[g_player_char_library.count].name),
             "%.*s Copy",prefix_len,src_name[0]?src_name:"Character");
    *ctx->player_char_editor_index=g_player_char_library.count++;
    if(ctx->char_model&&ctx->player_amap)
        player_character_reload_runtime(ctx->char_model,ctx->player_amap,ctx->player,player_character_selected_preset());
}

static void editor_shell_player_char_delete_preset(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    if(!ctx||!ctx->player_char_editor_index) return;
    if(g_player_char_library.count<=1) return;
    idx=*ctx->player_char_editor_index;
    if(idx<0||idx>=g_player_char_library.count) return;
    for(int i=idx;i<g_player_char_library.count-1;i++) g_player_char_presets[i]=g_player_char_presets[i+1];
    g_player_char_library.count--;
    if(g_player_char_library.active_index>=g_player_char_library.count) g_player_char_library.active_index=g_player_char_library.count-1;
    if(g_player_char_library.active_index>idx) g_player_char_library.active_index--;
    if(*ctx->player_char_editor_index>=g_player_char_library.count) *ctx->player_char_editor_index=g_player_char_library.count-1;
    player_character_library_save(PLAYER_CHARACTER_FILE);
    if(ctx->char_model&&ctx->player_amap)
        player_character_reload_runtime(ctx->char_model,ctx->player_amap,ctx->player,player_character_selected_preset());
    if(ctx->save_note) save_notify(ctx->save_note,"Player preset deleted");
}

static void editor_shell_player_char_set_active_preset(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->player_char_editor_index) return;
    if(*ctx->player_char_editor_index<0||*ctx->player_char_editor_index>=g_player_char_library.count) return;
    g_player_char_library.active_index=*ctx->player_char_editor_index;
    player_character_library_save(PLAYER_CHARACTER_FILE);
    if(ctx->save_note) save_notify(ctx->save_note,"Active player preset saved");
}

static void editor_shell_player_char_commit_name(void *userdata,const char *name){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PlayerCharacterConfig *cfg=player_character_selected_preset();
    if(!ctx||!name||!cfg) return;
    strncpy(cfg->name,name,sizeof(cfg->name)-1);
    cfg->name[sizeof(cfg->name)-1]=0;
}

static void editor_shell_player_char_set_model(void *userdata,int model_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PlayerCharacterConfig *cfg=player_character_selected_preset();
    if(!ctx||!cfg||!ctx->char_model||!ctx->player_amap) return;
    if(model_idx<0||model_idx>=g_player_char_model_option_count) return;
    strncpy(cfg->model_path,g_player_char_model_paths[model_idx],sizeof(cfg->model_path)-1);
    cfg->model_path[sizeof(cfg->model_path)-1]=0;
    player_character_reload_runtime(ctx->char_model,ctx->player_amap,ctx->player,cfg);
    if(ctx->save_note) save_notify(ctx->save_note,"Player model applied");
}

static void editor_shell_player_char_set_scale(void *userdata,float scale){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PlayerCharacterConfig *cfg=player_character_selected_preset();
    if(!ctx||!cfg||!ctx->char_model||!ctx->player_amap) return;
    cfg->scale=clampf(scale,0.01f,10.0f);
    player_character_reload_runtime(ctx->char_model,ctx->player_amap,ctx->player,cfg);
    if(ctx->save_note) save_notify(ctx->save_note,"Player scale applied");
}

static void editor_shell_player_char_set_anim_slot(void *userdata,int slot,int anim_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PlayerCharacterConfig *cfg=player_character_selected_preset();
    CharModel *cm;
    if(!ctx||!cfg||!ctx->char_model||!ctx->player_amap) return;
    if(slot<0||slot>=PLAYER_CHAR_SLOT_COUNT) return;
    cm=ctx->char_model;
    cfg->anim_override[slot][0]=0;
    if(anim_idx>0){
        int cursor=1;
        for(int si=-1;si<cm->extra_count;si++){
            Scene *scene=(si<0)?&cm->base:&cm->extra[si];
            int count=scene_anim_count(scene);
            for(int ai=0;ai<count;ai++,cursor++){
                if(cursor==anim_idx){
                    const char *name=scene_anim_name_at(scene,ai);
                    if(name&&name[0]){
                        strncpy(cfg->anim_override[slot],name,sizeof(cfg->anim_override[slot])-1);
                        cfg->anim_override[slot][sizeof(cfg->anim_override[slot])-1]=0;
                    }
                    player_character_reload_runtime(ctx->char_model,ctx->player_amap,ctx->player,cfg);
                    if(ctx->save_note) save_notify(ctx->save_note,"Player animation applied");
                    return;
                }
            }
        }
    }
    player_character_reload_runtime(ctx->char_model,ctx->player_amap,ctx->player,cfg);
    if(ctx->save_note) save_notify(ctx->save_note,"Player animation applied");
}

static void editor_shell_player_char_set_anim_sound_slot(void *userdata,int slot,int sound_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PlayerCharacterConfig *cfg=player_character_selected_preset();
    if(!ctx||!cfg) return;
    if(slot<0||slot>=PLAYER_CHAR_SLOT_COUNT) return;
    cfg->anim_sound_path[slot][0]=0;
    if(sound_idx>0){
        int sound_cursor=sound_idx-1;
        if(sound_cursor>=0&&sound_cursor<g_player_char_sound_option_count){
            strncpy(cfg->anim_sound_path[slot],g_player_char_sound_paths[sound_cursor],sizeof(cfg->anim_sound_path[slot])-1);
            cfg->anim_sound_path[slot][sizeof(cfg->anim_sound_path[slot])-1]=0;
        }
    }
    if(ctx->save_note) save_notify(ctx->save_note,"Player animation sound applied");
}

static void editor_shell_player_char_save(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx) return;
    player_character_library_save(PLAYER_CHARACTER_FILE);
    if(ctx->save_note) save_notify(ctx->save_note,"Player character saved");
}

static void editor_shell_prop_set_custom_collision_box_gizmo_mode(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->editor) return;
    ctx->editor->prop_custom_box_gizmo_mode=enabled;
    ctx->editor->prop_gizmo_active_axis=0;
    ctx->editor->prop_mouse_dragging=false;
    if(enabled&&ctx->editor->prop_mouse_mode==0)
        ctx->editor->prop_mouse_mode=1;
}

static void editor_shell_activate_mode(void *userdata,EditorWorldForgeMode mode){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    EditorRuntimeState *editor=ctx?ctx->editor:NULL;
    PropPlacer *placer=(PropPlacer*)ctx->placer;
    WorldFxSystem *world_fx=(WorldFxSystem*)ctx->world_fx;
    WorldFxEditor *world_fx_editor=(WorldFxEditor*)ctx->world_fx_editor;

    if(!ctx||!editor) return;
    switch(mode){
        case EDITOR_WF_MODE_SCULPT:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->sculpt_edit=true;
            break;
        case EDITOR_WF_MODE_FOREST:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->forest_edit=true;
            break;
        case EDITOR_WF_MODE_FENCE:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->forest_edit=true;
            editor->forest_brush_kind=4;
            break;
        case EDITOR_WF_MODE_WIND:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->wind_edit=true;
            break;
        case EDITOR_WF_MODE_WORLD_FX:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->particle_fx_edit=true;
            if(world_fx->selected>=0&&world_fx->selected<world_fx->count&&world_fx->items[world_fx->selected].active)
                world_fx_editor_sync_from_fx(world_fx_editor,&world_fx->items[world_fx->selected]);
            else
                world_fx_editor_reset(world_fx_editor);
            break;
        case EDITOR_WF_MODE_SHADER:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,24.0f);
            editor->shader_edit=true;
            SDL_StopTextInput();
            break;
        case EDITOR_WF_MODE_PATH:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->path_edit=true;
            break;
        case EDITOR_WF_MODE_WATER_ZONE:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->wzone_edit=true;
            editor->wf_edit_mode=false;
            break;
        case EDITOR_WF_MODE_WATERFALL:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->wzone_edit=true;
            editor->wf_edit_mode=true;
            break;
        case EDITOR_WF_MODE_RIVER:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->river_edit=true;
            break;
        case EDITOR_WF_MODE_PROP_EDIT:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->show_debug=true;
            editor->prop_edit=true;
            if(g_props.count>0){
                int first=props_first_active(&g_props);
                if(first>=0){
                    editor->prop_sel=first;
                    placer->selected_prop=first;
                    editor_selection_set(EDITOR_SELECTION_PROP,first);
                    placer_sync_from_prop(placer,&g_props.list[first]);
                }
            }
            break;
        case EDITOR_WF_MODE_PROP_PLACE:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->prop_place_mode=true;
            editor->prop_sel=-1;
            if(placer) placer->selected_prop=-1;
            if(ctx->selection&&ctx->selection->kind==EDITOR_SELECTION_PROP) editor_selection_clear();
            editor->prop_mouse_mode=0;
            if(placer->search_focus) SDL_StartTextInput();
            else SDL_StopTextInput();
            break;
        case EDITOR_WF_MODE_SURFACE_PIECE:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->surface_edit=true;
            break;
        case EDITOR_WF_MODE_COLLISION_VOLUME:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->collision_edit=true;
            break;
        case EDITOR_WF_MODE_MARKER:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->marker_edit=true;
            break;
        case EDITOR_WF_MODE_NPC:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->npc_edit_mode=true;
            editor->npc_place_mode=false;
            break;
        case EDITOR_WF_MODE_SPAWN:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->spawn_edit_mode=true;
            break;
        case EDITOR_WF_MODE_QUEST:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->quest_edit_mode=true;
            break;
        case EDITOR_WF_MODE_MAIN_CHARACTER:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,24.0f);
            editor->main_character_edit=true;
            SDL_StopTextInput();
            break;
        case EDITOR_WF_MODE_POINT_LIGHT:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->plight_edit=true;
            break;
        case EDITOR_WF_MODE_DECAL:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->decal_edit=true;
            break;
        case EDITOR_WF_MODE_SOUND_ZONE:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->szone_edit=true;
            break;
        case EDITOR_WF_MODE_INTERIOR_MATERIAL:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,18.0f);
            editor->interior_edit=true;
            if(editor->interior_selected_idx<0&&interior_active_index()>=0) editor->interior_selected_idx=interior_active_index();
            editor_shell_sync_interior_material(editor);
            break;
        case EDITOR_WF_MODE_EXIT:
            editor_shell_reset_modes(ctx);
            editor->editor_mode=false;
            if(ctx->cam&&g_project.loaded){ctx->cam->dist=7;ctx->cam->pitch=.16f;ctx->cam->target=ctx->player->phys.pos;}
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_SetCursor(SDL_GetDefaultCursor());
            SDL_ShowCursor(SDL_ENABLE);
            SDL_StopTextInput();
            break;
        case EDITOR_WF_MODE_SESSION:
        default:
            editor_shell_reset_modes(ctx);
            editor_shell_set_editor_camera(ctx,24.0f);
            SDL_StopTextInput();
            break;
    }
}

static void editor_shell_save_world_cb(void *userdata){
    editor_shell_save_world((EditorShellActionContext*)userdata);
}

static void editor_shell_save_slot_cb(void *userdata,int slot){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||slot<0||slot>2) return;
    game_save(slot,ctx->player,ctx->qs,ctx->as,ctx->enemies,ctx->ne,ctx->total_time,ctx->inv,ctx->lvlsys);
    {
        char buf[48];
        snprintf(buf,sizeof(buf),"Saved — Slot %d",slot+1);
        save_notify(ctx->save_note,buf);
    }
}

static void editor_shell_browser_refresh(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    placer_scan_models(placer);
    placer_apply_selected_defaults(placer);
}

static void editor_shell_browser_filter(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    placer_rebuild_filter(placer);placer_apply_selected_defaults(placer);
}

static bool editor_shell_browser_import(void *userdata,const char *source,char *message,size_t capacity){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->placer) return false;
    return editor_import_model((PropPlacer*)ctx->placer,source,message,capacity);
}

static void editor_shell_browser_select(void *userdata,int index,bool prefab_mode){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    placer->selected_prop=-1;
    if(ctx->prop_sel) *ctx->prop_sel=-1;
    if(ctx->selection&&ctx->selection->kind==EDITOR_SELECTION_PROP) editor_selection_clear();
    if(prefab_mode){
        if(index>=0&&index<g_prefabs.count){
            placer->prefab_sel=index;
            g_prefabs.selected=index;
            prefab_mark_recent(&g_prefabs,index);
        }
        return;
    }
    if(index>=0&&index<placer->filtered_count){
        int palette_idx=placer->filtered[index];
        placer->palette_sel=index;
        if(placer->palette_scroll>placer->palette_sel) placer->palette_scroll=placer->palette_sel;
        if(placer->palette_sel>=placer->palette_scroll+12) placer->palette_scroll=placer->palette_sel-11;
        placer_mark_recent(placer,palette_idx);
        placer_apply_selected_defaults(placer);
    }
}

static void editor_shell_browser_toggle_favorite(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    if(placer->prefab_mode){
        if(placer->prefab_sel>=0&&placer->prefab_sel<g_prefabs.count)
            g_prefabs.items[placer->prefab_sel].favorite=!g_prefabs.items[placer->prefab_sel].favorite;
        return;
    }
    if(placer->palette_sel>=0&&placer->palette_sel<placer->filtered_count){
        int idx=placer->filtered[placer->palette_sel];
        if(idx>=0&&idx<placer->palette_count) placer->favorite[idx]=!placer->favorite[idx];
        placer_rebuild_filter(placer);
    }
}

static void editor_shell_browser_cycle_category_filter(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    placer->category_filter=(placer->category_filter+1)%BROWSER_CAT_COUNT;
    placer_rebuild_filter(placer);
}

static void editor_shell_browser_toggle_favorites_only(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    placer->filter_favorites_only=!placer->filter_favorites_only;
    placer_rebuild_filter(placer);
}

static void editor_shell_browser_toggle_recent_only(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    placer->filter_recent_only=!placer->filter_recent_only;
    placer_rebuild_filter(placer);
}

static void editor_shell_prefab_capture_selection(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    int idx;
    if(!ctx||!placer) return;
    idx=prefab_capture_current_editor_selection(&g_prefabs,&g_props,(WorldFxSystem*)ctx->world_fx,placer);
    if(idx>=0){
        placer->prefab_mode=true;
        placer->prefab_sel=idx;
        g_prefabs.selected=idx;
        if(ctx->save_note) save_notify(ctx->save_note,"Prefab captured");
    }else if(ctx->save_note){
        save_notify(ctx->save_note,"Prefab capture failed");
    }
}

static void editor_shell_prefab_delete_selected(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    if(prefab_delete_selected(&g_prefabs,placer)){
        if(ctx&&ctx->save_note) save_notify(ctx->save_note,"Prefab deleted");
    }
}

static void editor_shell_prefab_duplicate_selected(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    if(prefab_duplicate_selected(&g_prefabs,placer)){
        if(ctx&&ctx->save_note) save_notify(ctx->save_note,"Prefab duplicated");
    }
}

static void editor_shell_prefab_commit_name(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    if(!placer) return;
    if(prefab_commit_selected_name(&g_prefabs,placer)&&ctx&&ctx->save_note)
        save_notify(ctx->save_note,"Prefab renamed");
}

static void editor_shell_prefab_save_library(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    prefab_library_save(&g_prefabs);
    if(ctx&&ctx->save_note) save_notify(ctx->save_note,"Prefabs saved");
}

static void editor_shell_bulk_align_x(void *userdata){ editor_bulk_align_axis((EditorShellActionContext*)userdata,0); }
static void editor_shell_bulk_align_y(void *userdata){ editor_bulk_align_axis((EditorShellActionContext*)userdata,1); }
static void editor_shell_bulk_align_z(void *userdata){ editor_bulk_align_axis((EditorShellActionContext*)userdata,2); }
static void editor_shell_bulk_distribute_x(void *userdata){ editor_bulk_distribute_axis((EditorShellActionContext*)userdata,0); }
static void editor_shell_bulk_distribute_z(void *userdata){ editor_bulk_distribute_axis((EditorShellActionContext*)userdata,2); }
static void editor_shell_bulk_replace_selection(void *userdata){ editor_bulk_replace_selection_with_current((EditorShellActionContext*)userdata); }
static void editor_shell_bulk_copy_selection(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(g_prop_sel_count<=0) return;
    prop_multi_copy();
    if(ctx&&ctx->save_note) save_notify(ctx->save_note,"Prop selection copied");
}
static void editor_shell_bulk_paste_selection(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:&g_placer;
    Vec3 center={0};
    if(g_prop_copy_count<=0) return;
    if(placer&&placer->has_ghost) center=placer->ghost_pos;
    else if(ctx&&ctx->cam) center=ctx->cam->target;
    center.y=scene_ground_y(center.x,center.z);
    prop_multi_paste(center);
    if(g_prop_sel_count>0){
        for(int i=0;i<g_props.count;i++){
            if(g_prop_multi_sel[i]&&g_props.list[i].active){
                editor_bulk_refresh_selection(ctx,i);
                break;
            }
        }
    }
    if(ctx&&ctx->save_note) save_notify(ctx->save_note,"Prop selection pasted");
}
static void editor_shell_bulk_duplicate_selection(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(g_prop_sel_count<=0) return;
    prop_multi_duplicate();
    if(g_prop_sel_count>0){
        for(int i=0;i<g_props.count;i++){
            if(g_prop_multi_sel[i]&&g_props.list[i].active){
                editor_bulk_refresh_selection(ctx,i);
                break;
            }
        }
    }
    if(ctx&&ctx->save_note) save_notify(ctx->save_note,"Prop selection duplicated");
}
static void editor_shell_bulk_select_all(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    prop_multi_add_all();
    if(g_prop_sel_count>0){
        for(int i=0;i<g_props.count;i++){
            if(g_prop_multi_sel[i]&&g_props.list[i].active){
                editor_bulk_refresh_selection(ctx,i);
                break;
            }
        }
    }
}
static void editor_shell_bulk_clear_selection(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:&g_placer;
    prop_multi_clear();
    if(ctx&&ctx->prop_sel) *ctx->prop_sel=-1;
    if(placer) placer->selected_prop=-1;
    if(ctx&&ctx->selection&&ctx->selection->kind==EDITOR_SELECTION_PROP) editor_selection_clear();
}
static void editor_shell_bulk_scatter_current(void *userdata){ editor_bulk_scatter_current((EditorShellActionContext*)userdata); }
static void editor_shell_bulk_stamp_path_current(void *userdata){ editor_bulk_stamp_path_current((EditorShellActionContext*)userdata); }
static void editor_key_align_z(void){ editor_bulk_align_axis(NULL,2); }
static void editor_key_distribute_z(void){ editor_bulk_distribute_axis(NULL,2); }
static void editor_key_scatter_current(void){ editor_bulk_scatter_current(NULL); }
static void editor_key_stamp_path_current(void){ editor_bulk_stamp_path_current(NULL); }
static void editor_key_distribute_x(void){ editor_bulk_distribute_axis(NULL,0); }
static void editor_key_replace_selection(void){ editor_bulk_replace_selection_with_current(NULL); }
static void editor_key_toggle_browser_favorite(void){
    if(g_placer.prefab_mode){
        if(g_placer.prefab_sel>=0&&g_placer.prefab_sel<g_prefabs.count)
            g_prefabs.items[g_placer.prefab_sel].favorite=!g_prefabs.items[g_placer.prefab_sel].favorite;
    }else if(g_placer.palette_sel>=0&&g_placer.palette_sel<g_placer.filtered_count){
        int idx=g_placer.filtered[g_placer.palette_sel];
        if(idx>=0&&idx<g_placer.palette_count) g_placer.favorite[idx]=!g_placer.favorite[idx];
        placer_rebuild_filter(&g_placer);
    }
}
