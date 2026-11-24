static void editor_shell_panel(HUD *h,float x,float y,float w,float ht){
    hud_rect(h,x-10.0f,y-10.0f,w+20.0f,ht+20.0f,0.01f,0.02f,0.01f,0.12f);
    hud_rect(h,x-4.0f,y-4.0f,w+8.0f,ht+8.0f,0.03f,0.05f,0.03f,0.60f);
    hud_rect(h,x,y,w,ht,0.07f,0.11f,0.07f,0.95f);
    hud_rect(h,x+1.0f,y+1.0f,w-2.0f,22.0f,0.17f,0.24f,0.10f,0.22f);
    hud_rect(h,x+1.0f,y+23.0f,w-2.0f,1.0f,0.70f,0.58f,0.22f,0.22f);
    hud_rect(h,x+1.0f,y+ht-2.0f,w-2.0f,1.0f,0.10f,0.18f,0.08f,0.24f);
    hud_rect(h,x,y,w,1.0f,0.18f,0.36f,0.12f,0.98f);
    hud_rect(h,x,y+ht-1.0f,w,1.0f,0.18f,0.36f,0.12f,0.98f);
    hud_rect(h,x,y,1.0f,ht,0.18f,0.36f,0.12f,0.98f);
    hud_rect(h,x+w-1.0f,y,1.0f,ht,0.18f,0.36f,0.12f,0.98f);
}

static bool menu_btn(HUD *h,float x,float y,float w,float ht,
                     const char *txt,float s,int mx,int my,bool enabled,bool *clicked);

static int editor_shell_stepper(HUD *h,float x,float y,float w,float ht,
                                const char *label,const char *value,
                                int mx,int my,bool click,
                                const char *left_label,const char *right_label){
    const float btn_w=28.0f;
    const float gap=8.0f;
    const float value_x=x+92.0f;
    float left_x=x+w-(btn_w*2.0f)-gap;
    float right_x=x+w-btn_w;
    bool left_over=false;
    bool right_over=false;

    hud_rect(h,x,y,w,ht,0.09f,0.14f,0.08f,0.74f);
    hud_rect(h,x+1.0f,y+1.0f,w-2.0f,ht-2.0f,0.12f,0.18f,0.10f,0.34f);
    hud_rect(h,x+8.0f,y+ht-1.0f,w-16.0f,1.0f,0.12f,0.20f,0.10f,0.28f);
    hud_str(h,x+10.0f,y+5.0f,0.78f,label,0.60f,0.72f,0.44f,0.94f);
    if(value&&value[0]) hud_str(h,value_x,y+5.0f,0.82f,value,0.90f,0.84f,0.66f,0.98f);

    left_over=menu_btn(h,left_x,y+2.0f,btn_w,ht-4.0f,left_label?left_label:"<",1.0f,mx,my,true,NULL);
    right_over=menu_btn(h,right_x,y+2.0f,btn_w,ht-4.0f,right_label?right_label:">",1.0f,mx,my,true,NULL);
    if(left_over&&click) return -1;
    if(right_over&&click) return 1;
    return 0;
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
typedef struct SurfacePiece SurfacePiece;
typedef struct SurfacePieceList SurfacePieceList;
typedef struct CollisionVolume CollisionVolume;
typedef struct CollisionVolumeList CollisionVolumeList;
static void editor_shell_select_interior_material(void *userdata,int index);
static void editor_shell_select_surface_material(void *userdata,int index);
static void editor_shell_set_surface_kind(void *userdata,int kind);
static void editor_shell_set_surface_width(void *userdata,float value);
static void editor_shell_set_surface_height(void *userdata,float value);
static void editor_shell_set_surface_thickness(void *userdata,float value);
static void editor_shell_set_surface_collision(void *userdata,int coll_type);
static void editor_shell_set_collision_volume_width(void *userdata,float value);
static void editor_shell_set_collision_volume_height(void *userdata,float value);
static void editor_shell_set_collision_volume_depth(void *userdata,float value);
static void editor_shell_set_collision_volume_shape(void *userdata,int shape);
static void editor_shell_set_collision_volume_yaw(void *userdata,float value);
static void editor_shell_set_collision_volume_pitch(void *userdata,float value);
static void editor_shell_set_collision_volume_enabled(void *userdata,bool enabled);
static void editor_shell_set_collision_volume_nav_blocker(void *userdata,bool enabled);
static void editor_shell_set_collision_volume_motion_enabled(void *userdata,bool enabled);
static void editor_shell_set_collision_volume_motion_axis(void *userdata,int axis,float value);
static void editor_shell_set_collision_volume_motion_period(void *userdata,float value);
static void editor_shell_set_collision_volume_motion_phase(void *userdata,float value);
static void editor_shell_prop_set_custom_collision_box_shape(void *userdata,int box_idx,int shape);
static void editor_shell_plight_set_color(void *userdata,int channel,float value);
static void editor_shell_plight_set_radius(void *userdata,float value);
static void editor_shell_plight_set_intensity(void *userdata,float value);
static void editor_shell_plight_set_y_offset(void *userdata,float value);
static void editor_shell_plight_set_flicker(void *userdata,bool enabled);
static void editor_shell_plight_set_flicker_speed(void *userdata,float value);
static void editor_shell_plight_delete_selected(void *userdata);
static void editor_shell_plight_set_position(void *userdata,int axis,float value);
static void editor_shell_plight_set_type(void *userdata,int type);
static void editor_shell_plight_set_spot_angles(void *userdata,float yaw_deg,float pitch_deg);
static void editor_shell_plight_set_spot_cone(void *userdata,float inner_deg,float outer_deg);
static void editor_shell_world_light_set_override(void *userdata,bool enabled);
static void editor_shell_world_light_set_angles(void *userdata,float yaw_deg,float pitch_deg);
static void editor_shell_world_light_set_sun_color(void *userdata,int channel,float value);
static void editor_shell_world_light_set_ambient_color(void *userdata,int channel,float value);
static void editor_shell_szone_set_position(void *userdata,int axis,float value);
static bool interior_material_supports_surface(int idx,int surface);
static const char *surface_piece_kind_name(int kind);
static unsigned int surface_piece_surface_flag(int kind);
static int surface_piece_active_count(void);
static int surface_piece_total_count(void);
static bool surface_piece_query(int idx,int *out_kind,float *out_width,float *out_height,float *out_thickness,int *out_material_id);
static bool surface_piece_describe(int idx,Vec3 *out_pos,float *out_yaw,int *out_kind,float *out_width,float *out_height,float *out_thickness,int *out_material_id);
static bool surface_piece_material_supported(int material_idx,int kind);
static bool surface_piece_get_pos(int idx,Vec3 *out_pos);
static bool surface_piece_set_material_id(int idx,int material_id);
static int surface_piece_default_material(int kind);
static bool surface_piece_set_kind(int idx,int kind);
static bool surface_piece_set_width_value(int idx,float value);
static bool surface_piece_set_height_value(int idx,float value);
static bool surface_piece_set_thickness_value(int idx,float value);
static int surface_piece_collision_type(int idx);
static bool surface_piece_set_collision_type(int idx,int coll_type);
static const char *prop_interact_name(PropInteractType type);
static const char *prop_action_name(PropActionType type);
static const char *prop_action_condition_name(PropActionConditionType type);
static bool placed_light_query(int idx,Vec3 *out_pos,Vec3 *out_color,float *out_radius,
                               float *out_intensity,float *out_y_offset,bool *out_flicker,
                               float *out_flicker_speed,int *out_type,float *out_spot_yaw,
                               float *out_spot_pitch,float *out_spot_inner,float *out_spot_outer);
static void surface_piece_save_active_scene(void);
static void surface_pieces_save(SurfacePieceList *sl);
static bool surface_piece_move_y(int idx,float delta_y,bool snap_to_ground);
static bool surface_piece_move_planar(int idx,Vec3 delta);
static bool surface_piece_adjust_yaw(int idx,float delta_deg);
static bool surface_piece_scale_uniform(int idx,float mul);
static bool surface_piece_snap_grid(int idx,float grid);
static int surface_piece_register(SurfacePieceList *sl,int kind,Vec3 pos,float yaw_deg,
                                  float width,float height,float thickness,int material_id);
static int surface_piece_duplicate_idx(int idx,Vec3 offset,bool snap_to_ground,bool snap_to_grid);
static bool surface_piece_delete_idx(int idx);
static void surface_pieces_activate(SceneID scene);
static int collision_volume_active_count(void);
static int collision_volume_total_count(void);
static bool collision_volume_query(int idx,float *out_width,float *out_height,float *out_depth,
                                   bool *out_enabled,bool *out_nav_blocker);
static bool collision_volume_shape_query(int idx,int *out_shape,float *out_pitch);
static bool collision_volume_describe(int idx,Vec3 *out_pos,float *out_yaw,float *out_width,
                                      float *out_height,float *out_depth,
                                      bool *out_enabled,bool *out_nav_blocker);
static bool collision_volume_get_pos(int idx,Vec3 *out_pos);
static bool collision_volume_set_position(int idx,Vec3 pos);
static bool collision_volume_set_yaw_value(int idx,float yaw_deg);
static bool collision_volume_set_width_value(int idx,float value);
static bool collision_volume_set_height_value(int idx,float value);
static bool collision_volume_set_depth_value(int idx,float value);
static bool collision_volume_set_shape_value(int idx,int shape);
static bool collision_volume_set_enabled_value(int idx,bool enabled);
static bool collision_volume_set_pitch_value(int idx,float pitch_deg);
static bool collision_volume_set_nav_blocker_value(int idx,bool enabled);
static bool collision_volume_motion_describe(int idx,bool *out_motion_enabled,Vec3 *out_motion_offset,
                                             float *out_motion_period,float *out_motion_phase);
static bool collision_volume_set_motion_enabled_value(int idx,bool enabled);
static bool collision_volume_set_motion_offset_axis(int idx,int axis,float value);
static bool collision_volume_set_motion_period_value(int idx,float value);
static bool collision_volume_set_motion_phase_value(int idx,float value);
static bool collision_volume_scale_uniform(int idx,float mul);
static int collision_volume_register(CollisionVolumeList *list,Vec3 pos,int shape,float yaw_deg,float pitch_deg,
                                     float width,float height,float depth,
                                     bool enabled,bool nav_blocker);
static int collision_volume_duplicate_idx(int idx,Vec3 offset,bool snap_to_ground,bool snap_to_grid);
static bool collision_volume_delete_idx(int idx);
static void collision_volume_save_active_scene(void);
static void collision_volumes_save(CollisionVolumeList *list);
static void collision_volumes_activate(SceneID scene);

static void draw_world_forge_editor_shell(HUD *h,int win_w,int win_h,
                                          EditorRuntimeState *editor_state,
                                          bool editor_mode,
                                          bool path_edit,bool wzone_edit,bool wf_edit_mode,
                                          bool river_edit,bool prop_edit,bool particle_fx_edit,
                                          bool marker_edit,bool forest_edit,bool wind_edit,
                                          bool sculpt_edit,bool npc_edit_mode,bool spawn_edit_mode,
                                          bool shader_edit,bool prop_place_mode,
                                          int prop_mouse_mode,
                                          int prop_count,int marker_count,int npc_count,int spawn_count,
                                          Terrain *terrain,
                                          void *forest,void *grass,void *world_markers,void *world_fx,
                                          void *world_fx_editor,void *spawns,void *npc_spawns,void *npcs_runtime,void *placer,
                                          Camera3P *cam,Vec3 *editor_cam_target,
                                          bool *show_debug,int *prop_sel,int wind_sel,
                                          int marker_sel,int marker_sel_type,int marker_total,
                                          int wzone_sel,EnemyType spawn_sel_type,int spawn_sel_model,
                                          int wf_sel,float wf_place_width,float wf_place_height,float wf_place_yaw,
                                          int forest_brush_kind,float forest_brush_radius,int forest_brush_density,
                                          float tree_brush_scale,
                                          float wall_brush_spacing,float wall_brush_scale,
                                          float terrain_brush_strength,int terrain_brush_style,
                                          float grass_brush_radius,int grass_brush_density,float grass_brush_size,
                                          float grass_brush_hmin,float grass_brush_hmax,
                                          int flower_brush_density,float flower_brush_size,const char *forest_detail_line,
                                          int sculpt_mode,float sculpt_radius,float sculpt_strength,
                                          bool npc_place_mode,int npc_sel_type,float npc_ed_yaw,
                                          int npc_selected_index,
                                          bool npc_ed_wp_mode,int npc_ed_wp_npc,
                                          bool npc_ed_dlg_mode,int npc_ed_dlg_npc,char *npc_ed_dlg_buf,int npc_ed_dlg_len,int npc_ed_dlg_sel,
                                          bool npc_ed_tree_mode,int npc_ed_tree_npc,int npc_ed_tree_sel,int npc_ed_tree_choice,
                                          bool npc_has_focus,int npc_focus_index,int npc_focus_model_idx,const char *npc_focus_name,
                                          char *npc_focus_name_buffer,int npc_focus_name_capacity,
                                          char *npc_focus_backstory_buffer,int npc_focus_backstory_capacity,
                                          float npc_focus_scale,float npc_focus_visual_yaw_offset,
                                          bool npc_focus_collision_enabled,bool npc_focus_patrol_loop,
                                          int npc_waypoint_count,const float *npc_waypoint_x,const float *npc_waypoint_y,const float *npc_waypoint_z,
                                          float npc_patrol_speed,float npc_patrol_wait,bool npc_is_merchant,int npc_shop_id,const char *npc_shop_name,
                                          int npc_dialogue_count,const char **npc_dialogue_lines,
                                          int npc_tree_count,int npc_tree_root,char *npc_tree_text,int npc_tree_text_capacity,
                                          char *npc_tree_speaker,int npc_tree_speaker_capacity,int *npc_tree_anim_trigger,
                                          int npc_tree_choice_count,char **npc_tree_choice_labels,int **npc_tree_choice_next,
                                          int fx_editor_tab,float wetness,float fog_density,float overcast,
                                          const char *fx_line_b,const char *fx_line_c,
                                          WeatherSystem *weather,
                                          Player *player,CharModel *char_model,AnimMap *player_amap,
                                          QuestSystem *qs,AbilitySystem *as,
                                          Enemy *enemies,int ne,float *total_time_ptr,
                                          Inventory *inv,LevelSystem *lvlsys,SaveNotify *save_note,
                                          bool plight_edit,int plight_sel,float plight_place_r,float plight_place_g,float plight_place_b,
                                          float plight_place_radius,float plight_place_intensity,float plight_place_y_offset,
                                          bool plight_place_flicker,float plight_place_flicker_speed,
                                          bool decal_edit,int decal_sel,int decal_place_type,
                                          float decal_place_size,float decal_place_alpha,float decal_place_yaw,
                                          bool szone_edit,int szone_sel,int szone_sound_idx,
                                          float szone_place_radius,float szone_place_vol){
    (void)editor_cam_target;
    (void)show_debug;
    (void)spawn_sel_model;
    (void)terrain_brush_strength;
    (void)terrain_brush_style;
    (void)grass_brush_radius;
    (void)grass_brush_density;
    (void)grass_brush_size;
    (void)grass_brush_hmin;
    (void)grass_brush_hmax;
    (void)flower_brush_density;
    (void)flower_brush_size;
    (void)npc_ed_wp_npc;
    (void)npc_ed_dlg_npc;
    (void)npc_ed_dlg_len;
    (void)npc_ed_tree_npc;
    (void)wetness;
    (void)fog_density;
    (void)overcast;
    static const EditorUIRenderAPI render_api={
        editor_ui_rect_cb,
        editor_ui_text_cb,
        editor_ui_title_cb
    };
    EditorWorldForgeShellState shell={0};
    EditorShellActionContext actions={0};
    char mode_buf[96];
    char backend_buf[96];
    char world_buf[96];
    char actor_buf[96];
    char inspect_a[160];
    char inspect_b[160];
    char inspect_c[160];
    char tool_a[160];
    char tool_b[160];
    char tool_c[160];
    const char *inspect_label="ACTIVE SELECTION";
    const char *inspect_a_ptr="Nothing selected";
    const char *inspect_b_ptr="Choose a tool, then click in the world to begin.";
    const char *inspect_c_ptr="Save often from the shell or the active tool panel.";
    const char *tool_title="Shader Lab";
    const char *tool_a_ptr="";
    const char *tool_b_ptr="";
    const char *tool_c_ptr="";
    if(g_quest_edit_mode_ptr&&*g_quest_edit_mode_ptr) tool_title="Quest Editor";
    if(editor_state&&editor_state->main_character_edit) tool_title="Main Character";
    if(szone_edit) tool_title="Sound Zones";
    if(decal_edit) tool_title="Decals";
    if(plight_edit) tool_title="Point Lights";
    if(spawn_edit_mode) tool_title="Spawn Tool";
    if(npc_edit_mode) tool_title="NPC Tool";
    if(editor_state&&editor_state->surface_edit) tool_title="Surface Pieces";
    if(editor_state&&editor_state->collision_edit) tool_title="Collision Volumes";
    if(editor_state&&editor_state->interior_edit) tool_title="Interior Paint";
    if(sculpt_edit) tool_title="Terrain Tool";
    if(wind_edit) tool_title="Wind Tool";
    if(forest_edit) tool_title="Forest Tool";
    if(marker_edit) tool_title="Markers";
    if(particle_fx_edit) tool_title="Effects";
    if(river_edit) tool_title="River Tool";
    if(wzone_edit) tool_title=wf_edit_mode?"Waterfall Tool":"Pond and Lake Tool";
    if(path_edit) tool_title="Path Tool";
    snprintf(mode_buf,sizeof(mode_buf),"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
             sculpt_edit?"Terrain ":"",
             forest_edit?"Forest ":"",
             wind_edit?"Wind ":"",
             particle_fx_edit?"FX ":"",
             path_edit?"Path ":"",
             (wzone_edit&&!wf_edit_mode)?"PondLake ":"",
             (wzone_edit&&wf_edit_mode)?"Waterfall ":"",
             river_edit?"River ":"",
             prop_edit?"Prop ":"",
             prop_place_mode?"Placer ":"",
             (editor_state&&editor_state->surface_edit)?"Surface ":"",
             (editor_state&&editor_state->collision_edit)?"Collision ":"",
             marker_edit?"Marker ":"",
             npc_edit_mode?"NPC ":"",
             spawn_edit_mode?"Spawn ":"",
             (editor_state&&editor_state->main_character_edit)?"MainChar ":"",
             plight_edit?"PLight ":"",
             decal_edit?"Decal ":"",
             szone_edit?"SZone ":"",
             (editor_state&&editor_state->interior_edit)?"Interior ":"");
    if(!mode_buf[0]) snprintf(mode_buf,sizeof(mode_buf),"No editor mode active");
    snprintf(backend_buf,sizeof(backend_buf),"%s%s  Visuals %s",
             editor_ui_backend_name(&g_editor_ui),
             (g_editor_ui.imgui_requested&&!g_editor_ui.imgui_compiled)?" (fallback)":"",
             visual_profile_is_dirty()?"Unsaved":"Saved");
    {
        snprintf(world_buf,sizeof(world_buf),"Props %d  Surfaces %d  Colliders %d  Markers %d",
                 prop_count,surface_piece_active_count(),collision_volume_active_count(),marker_count);
    }
    snprintf(actor_buf,sizeof(actor_buf),"NPC %d  Spawns %d",npc_count,spawn_count);

    if(path_edit){
        snprintf(inspect_a,sizeof(inspect_a),"Path Surface: %s",cpath_style_name(g_cpath.style));
        snprintf(inspect_b,sizeof(inspect_b),"Points %d   Width %.1f",g_cpath.count,(double)g_cpath.width);
        snprintf(inspect_c,sizeof(inspect_c),"Left click adds points. Right click removes the last point.");
        inspect_a_ptr=inspect_a;
        inspect_b_ptr=inspect_b;
        inspect_c_ptr=inspect_c;
        snprintf(tool_a,sizeof(tool_a),"Path %d of %d   Surface: %s",
                 g_cpath_slot>=0?g_cpath_slot+1:0,g_saved_cpath_count,cpath_style_name(g_cpath.style));
        snprintf(tool_b,sizeof(tool_b),"Points: %d / %d   Width: %.1f",
                 g_cpath.count,CPATH_MAX_PTS,(double)g_cpath.width);
        snprintf(tool_c,sizeof(tool_c),"Tint: %.2f %.2f %.2f",
                 (double)g_cpath.tint.x,(double)g_cpath.tint.y,(double)g_cpath.tint.z);
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(river_edit){
        snprintf(tool_a,sizeof(tool_a),"Points: %d",g_criver.count);
        snprintf(tool_b,sizeof(tool_b),"Width: %.0f",(double)g_criver.width);
        snprintf(tool_c,sizeof(tool_c),"Water Style: %s",WATER_PRESETS[g_wstyle.preset_idx].name);
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(wzone_edit&&!wf_edit_mode){
        snprintf(tool_a,sizeof(tool_a),"Zones: %d   Selected: %d",g_wzone_count,wzone_sel>=0?wzone_sel+1:0);
        if(wzone_sel>=0&&wzone_sel<g_wzone_count){
            snprintf(tool_b,sizeof(tool_b),"Radius: %.0f   Level: %.2f",
                     (double)g_wzones[wzone_sel].radius,(double)g_wzones[wzone_sel].level);
            snprintf(tool_c,sizeof(tool_c),"Preset: %s",WATER_PRESETS[g_wzones[wzone_sel].preset_idx].name);
        }else{
            snprintf(tool_b,sizeof(tool_b),"Click terrain to place or select a pond or lake.");
            snprintf(tool_c,sizeof(tool_c),"Adjust radius, level, and surface to shape the shoreline.");
        }
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(wzone_edit&&wf_edit_mode){
        snprintf(tool_a,sizeof(tool_a),"Waterfalls: %d   Selected: %d",g_wfalls.count,wf_sel>=0?wf_sel+1:0);
        if(wf_sel>=0&&wf_sel<g_wfalls.count){
            snprintf(tool_b,sizeof(tool_b),"Width: %.1f   Height: %.1f",
                     (double)g_wfalls.wf[wf_sel].width,(double)g_wfalls.wf[wf_sel].height);
            snprintf(tool_c,sizeof(tool_c),"Yaw: %.0f deg",(double)(g_wfalls.wf[wf_sel].yaw*57.2957795f));
        }else{
            snprintf(tool_b,sizeof(tool_b),"Place Width: %.1f   Height: %.1f",(double)wf_place_width,(double)wf_place_height);
            snprintf(tool_c,sizeof(tool_c),"Preview Yaw: %.0f deg",(double)(wf_place_yaw*57.2957795f));
        }
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(wind_edit){
        snprintf(tool_a,sizeof(tool_a),"Zones: %d   Selected: %d",
                 g_wind_zone_count,wind_sel>=0?wind_sel+1:0);
        if(wind_sel>=0&&wind_sel<g_wind_zone_count){
            snprintf(tool_b,sizeof(tool_b),"Radius %.0f   Power %.2f",
                     (double)g_wind_zones[wind_sel].radius,(double)g_wind_zones[wind_sel].strength);
            snprintf(tool_c,sizeof(tool_c),"Angle %.0f deg   Gust %.2f",
                     (double)(g_wind_zones[wind_sel].dir_angle*57.2957795f),(double)g_wind_zones[wind_sel].gust);
        }else{
            snprintf(tool_b,sizeof(tool_b),"Click terrain to place or select a wind field.");
            snprintf(tool_c,sizeof(tool_c),"Use Clear to remove all wind fields.");
        }
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(marker_edit){
        snprintf(tool_a,sizeof(tool_a),"Marker Type: %d",marker_sel_type+1);
        snprintf(tool_b,sizeof(tool_b),"Selected: %d   Total: %d",
                 marker_sel>=0?marker_sel+1:0,marker_total);
        snprintf(tool_c,sizeof(tool_c),"Click terrain to place a marker or select an existing one.");
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(editor_state&&editor_state->collision_edit){
        int selected_idx=(g_editor_selection.kind==EDITOR_SELECTION_COLLISION_VOLUME)?g_editor_selection.index:-1;
        snprintf(tool_a,sizeof(tool_a),"Volumes: %d   Selected: %d",
                 collision_volume_active_count(),selected_idx>=0?selected_idx+1:0);
        if(selected_idx>=0&&selected_idx<collision_volume_total_count()){
            Vec3 pos={0};
            float yaw_deg=0.0f,width=0.0f,height=0.0f,depth=0.0f;
            bool enabled=false,nav_blocker=false;
            if(collision_volume_describe(selected_idx,&pos,&yaw_deg,&width,&height,&depth,&enabled,&nav_blocker)){
                int shape=0;
                float pitch_deg=0.0f;
                collision_volume_shape_query(selected_idx,&shape,&pitch_deg);
                snprintf(tool_b,sizeof(tool_b),"%.1f x %.1f x %.1f   %s   Y %.0f X %.0f",
                         (double)width,(double)height,(double)depth,
                         shape==1?"Oval":"Box",(double)yaw_deg,(double)pitch_deg);
                snprintf(tool_c,sizeof(tool_c),"Collision %s   Nav %s",
                         enabled?"On":"Off",nav_blocker?"Block":"Ignore");
            }else{
                snprintf(tool_b,sizeof(tool_b),"Click terrain to place or select a collision volume.");
                snprintf(tool_c,sizeof(tool_c),"Use gizmos or the inspector to tune the box.");
            }
        }else{
            snprintf(tool_b,sizeof(tool_b),"Click terrain to place or select a collision volume.");
            snprintf(tool_c,sizeof(tool_c),"Use gizmos or the inspector to tune the box.");
        }
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(spawn_edit_mode){
        snprintf(tool_a,sizeof(tool_a),"Spawn Type: %s",EDEF[spawn_sel_type].name);
        snprintf(tool_b,sizeof(tool_b),"Spawn Points: %d",spawn_count);
        snprintf(tool_c,sizeof(tool_c),"Left click places a spawn. Right click removes one.");
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(npc_edit_mode){
        snprintf(tool_a,sizeof(tool_a),"NPC Type: %d",npc_sel_type+1);
        snprintf(tool_b,sizeof(tool_b),"Placed NPCs: %d",npc_count);
        if(npc_has_focus){
            snprintf(tool_c,sizeof(tool_c),"Yaw %.0f deg   %s #%d   WP %d",
                     (double)(npc_ed_yaw*57.2957795f),npc_focus_name?npc_focus_name:"NPC",npc_focus_index+1,npc_waypoint_count);
        }else{
            snprintf(tool_c,sizeof(tool_c),"Yaw %.0f deg   LMB terrain places, LMB on an NPC selects it.",
                     (double)(npc_ed_yaw*57.2957795f));
        }
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(sculpt_edit){
        const char *sculpt_mode_name=
            sculpt_mode==0?"Raise":
            sculpt_mode==1?"Lower":
            sculpt_mode==2?"Flatten":
            sculpt_mode==3?"Smooth":
            sculpt_mode==4?"Noise":
            sculpt_mode==5?"Grass":
            "Worn";
        snprintf(tool_a,sizeof(tool_a),"Mode: %s   Radius: %.0f",sculpt_mode_name,(double)sculpt_radius);
        snprintf(tool_b,sizeof(tool_b),"Power: %.3f",(double)sculpt_strength);
        snprintf(tool_c,sizeof(tool_c),"%s",
                 sculpt_mode==5?"Grass density mask painter":
                 (sculpt_mode==6?"Worn path painter":"Terrain height sculptor"));
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(forest_edit){
        snprintf(tool_a,sizeof(tool_a),"Brush: %d   Radius: %.0f",forest_brush_kind+1,(double)forest_brush_radius);
        snprintf(tool_b,sizeof(tool_b),"Density: %d",forest_brush_density);
        snprintf(tool_c,sizeof(tool_c),"%s",forest_detail_line?forest_detail_line:"Brush-specific detail controls are available in this panel.");
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(particle_fx_edit){
        snprintf(tool_a,sizeof(tool_a),"FX Tab: %s",fx_editor_tab==0?"World":(fx_editor_tab==1?"Weather":"Post"));
        snprintf(tool_b,sizeof(tool_b),"%s",fx_line_b?fx_line_b:"Live controls are active in the panel.");
        snprintf(tool_c,sizeof(tool_c),"%s",fx_line_c?fx_line_c:"Use in-world selection for targeted FX placement.");
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(plight_edit){
        snprintf(tool_a,sizeof(tool_a),"Color: R%.2f G%.2f B%.2f  Radius:%.1f  Int:%.2f",
                 (double)plight_place_r,(double)plight_place_g,(double)plight_place_b,
                 (double)plight_place_radius,(double)plight_place_intensity);
        snprintf(tool_b,sizeof(tool_b),"Y-Offset:%.2f  Flicker:%s  Speed:%.1f",
                 (double)plight_place_y_offset,plight_place_flicker?"ON":"OFF",(double)plight_place_flicker_speed);
        snprintf(tool_c,sizeof(tool_c),"Sel:%d  LMB=Place  RMB=Delete nearest  S=Save",plight_sel);
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(decal_edit){
        static const char *decal_type_names[]={"Scorch","Moss","Rune","Crack","Blood","Mud"};
        const char *dname=(decal_place_type>=0&&decal_place_type<6)?decal_type_names[decal_place_type]:"?";
        snprintf(tool_a,sizeof(tool_a),"Type: %s  Size:%.1f  Alpha:%.2f  Yaw:%.0f",
                 dname,(double)decal_place_size,(double)decal_place_alpha,(double)(decal_place_yaw*57.2957795f));
        snprintf(tool_b,sizeof(tool_b),"Selected: %d  LMB=Place  RMB=Delete nearest",decal_sel);
        snprintf(tool_c,sizeof(tool_c),"Tab/Shift+Tab=Type  +/-=Size  [/]=Alpha  S=Save");
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(szone_edit){
        extern const char *g_szone_sound_names[];
        const char *sname=(szone_sound_idx>=0&&szone_sound_idx<10)?g_szone_sound_names[szone_sound_idx]:"?";
        snprintf(tool_a,sizeof(tool_a),"Sound: %s  Radius:%.0f  Vol:%.0f",
                 sname,(double)szone_place_radius,(double)szone_place_vol);
        snprintf(tool_b,sizeof(tool_b),"Selected: %d  LMB=Place zone  RMB=Delete nearest",szone_sel);
        snprintf(tool_c,sizeof(tool_c),"Tab=Next sound  +/-=Radius  [/]=Volume  S=Save");
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(editor_state&&editor_state->interior_edit){
        int assigned=interior_material_assigned_to_surface(editor_state->interior_selected_idx,
                                                           editor_state->interior_surface_target);
        const char *target_name=interior_surface_name(editor_state->interior_surface_target);
        snprintf(tool_a,sizeof(tool_a),"Interior: %s",interior_name(editor_state->interior_selected_idx));
        snprintf(tool_b,sizeof(tool_b),"Target: %s  Brush: %s",
                 target_name,interior_material_name(editor_state->interior_material_sel));
        snprintf(tool_c,sizeof(tool_c),"Assigned: %s  LMB paints the clicked surface. Save when done.",
                 interior_material_name(assigned));
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(editor_state&&editor_state->surface_edit){
        snprintf(tool_a,sizeof(tool_a),"Surface Kind: %s",surface_piece_kind_name(editor_state->surface_kind));
        snprintf(tool_b,sizeof(tool_b),"Size %.2f x %.2f  Thick %.2f",
                 (double)editor_state->surface_width,(double)editor_state->surface_height,(double)editor_state->surface_thickness);
        snprintf(tool_c,sizeof(tool_c),"LMB selects or places. Use inspector to size and texture.");
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(editor_state&&editor_state->collision_edit){
        snprintf(tool_a,sizeof(tool_a),"Volume Size %.2f x %.2f x %.2f",
                 (double)editor_state->collision_width,(double)editor_state->collision_height,(double)editor_state->collision_depth);
        snprintf(tool_b,sizeof(tool_b),"Yaw %.1f   Nav %s",
                 (double)editor_state->collision_place_yaw,editor_state->collision_nav_blocker?"Block":"Ignore");
        snprintf(tool_c,sizeof(tool_c),"LMB selects or places. Use inspector or gizmos to tune the box.");
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(shader_edit){
        snprintf(tool_a,sizeof(tool_a),"File %d / %d",g_shader_lab.count>0?g_shader_lab.selected+1:0,g_shader_lab.count);
        snprintf(tool_b,sizeof(tool_b),"%s",shader_lab_selected_name(&g_shader_lab));
        snprintf(tool_c,sizeof(tool_c),"%s",g_shader_lab.status);
        tool_a_ptr=tool_a;
        tool_b_ptr=tool_b;
        tool_c_ptr=tool_c;
    }else if(prop_edit){
        inspect_a_ptr="Prop editing active";
        inspect_b_ptr="Click a prop in the world to select it, then use the floating transform toolbar.";
        inspect_c_ptr="Move, Rotate, Scale, Ground, SnapY, Save, and Delete all stay near the selected object. Del also removes it.";
    }else if(prop_place_mode){
        inspect_a_ptr="Model placement ready";
        inspect_b_ptr="Pick a model from the browser, then click terrain to place it.";
        inspect_c_ptr="Search, browse, rotate, and scale from the placer panel on the right.";
    }else if(editor_state&&editor_state->surface_edit){
        inspect_a_ptr="Surface piece placement ready";
        inspect_b_ptr="Click empty ground or floor to place a wall or floor slab. Click an existing piece to edit it.";
        inspect_c_ptr="Move, yaw, duplicate, and size pieces from the inspector and shared selection tools.";
    }else if(editor_state&&editor_state->collision_edit){
        inspect_a_ptr="Collision volume placement ready";
        inspect_b_ptr="Click empty ground to place a collision-only box. Click an existing box to edit it.";
        inspect_c_ptr="Use this for invisible blockers, smoothing traversal, and nav shaping without changing art.";
    }
    if(g_editor_selection.kind==EDITOR_SELECTION_PROP&&
       g_editor_selection.index>=0&&g_editor_selection.index<g_props.count&&
       g_props.list[g_editor_selection.index].active){
        Prop *sp=&g_props.list[g_editor_selection.index];
        inspect_label="PROP SELECTION";
        snprintf(inspect_a,sizeof(inspect_a),"%s",sp->path);
        snprintf(inspect_b,sizeof(inspect_b),"Pos %.1f %.1f %.1f   RotY %.1f   Scale %.2f",
                 (double)sp->pos.x,(double)sp->pos.y,(double)sp->pos.z,
                 (double)sp->rot_y,(double)sp->scale);
        snprintf(inspect_c,sizeof(inspect_c),"Collision: %s   Pickable: %s",
                 prop_coll_name(sp->coll_type),sp->pickable?"Yes":"No");
        inspect_a_ptr=inspect_a;
        inspect_b_ptr=inspect_b;
        inspect_c_ptr=inspect_c;
    }else if(editor_selection_describe(&g_editor_selection,&inspect_label,
                                       inspect_a,sizeof(inspect_a),
                                       inspect_b,sizeof(inspect_b),
                                       inspect_c,sizeof(inspect_c))){
        inspect_a_ptr=inspect_a;
        inspect_b_ptr=inspect_b;
        inspect_c_ptr=inspect_c;
    }
    {
        static char outliner_labels_storage[256][96];
        shell.outliner_count=editor_shell_build_outliner(world_markers,spawns,npc_spawns,
                                                         outliner_labels_storage,shell.outliner_labels,
                                                         shell.outliner_kinds,shell.outliner_indices);
    }
    shell.editor_mode=editor_mode;
    shell.path_edit=path_edit;
    shell.wzone_edit=wzone_edit;
    shell.wf_edit_mode=wf_edit_mode;
    shell.river_edit=river_edit;
    shell.river_has_selection=(editor_state&&editor_state->river_sel_point>=0&&editor_state->river_sel_point<g_criver.count);
    shell.river_selected_point=editor_state?editor_state->river_sel_point:-1;
    shell.river_width=g_criver.width;
    shell.river_bank_softness=g_criver.bank_softness;
    if(shell.river_has_selection){
        int _rpi=editor_state->river_sel_point;
        shell.river_point_x=g_criver.pts[_rpi*2];
        shell.river_point_z=g_criver.pts[_rpi*2+1];
        shell.river_point_width_scale=g_criver.width_scale[_rpi];
        shell.river_point_depth_scale=g_criver.depth_scale[_rpi];
        shell.river_point_flow_speed=g_criver.flow_speed[_rpi];
        shell.river_point_foam=g_criver.foam[_rpi];
    }else{
        shell.river_point_x=0.0f;
        shell.river_point_z=0.0f;
        shell.river_point_width_scale=1.0f;
        shell.river_point_depth_scale=1.0f;
        shell.river_point_flow_speed=1.0f;
        shell.river_point_foam=0.35f;
    }
    shell.prop_edit=prop_edit;
    shell.surface_edit=editor_state?editor_state->surface_edit:false;
    shell.collision_edit=editor_state?editor_state->collision_edit:false;
    shell.particle_fx_edit=particle_fx_edit;
    shell.marker_edit=marker_edit;
    shell.forest_edit=forest_edit;
    shell.wind_edit=wind_edit;
    shell.sculpt_edit=sculpt_edit;
    shell.npc_edit_mode=npc_edit_mode;
    shell.spawn_edit_mode=spawn_edit_mode;
    shell.shader_edit=shader_edit;
    shell.prop_place_mode=prop_place_mode;
    shell.plight_edit=plight_edit;
    shell.decal_edit=decal_edit;
    shell.szone_edit=szone_edit;
    shell.interior_edit=editor_state?editor_state->interior_edit:false;
    shell.selection=g_editor_selection;
    {
        bool qem=g_quest_edit_mode_ptr?*g_quest_edit_mode_ptr:false;
    shell.quest_edit_mode=qem;
    shell.main_character_edit=editor_state?editor_state->main_character_edit:false;
        shell.quest_count=qs?qs->count:0;
        shell.quest_sel=g_quest_ed_sel_ptr?*g_quest_ed_sel_ptr:0;
        shell.quest_name_buf=g_quest_ed_name_buf_ptr;
        shell.quest_name_buf_cap=64;
        shell.quest_desc_buf=g_quest_ed_desc_buf_ptr;
        shell.quest_desc_buf_cap=128;
        shell.quest_gold_ptr=g_quest_ed_gold_ptr;
        shell.quest_xp_ptr=g_quest_ed_xp_ptr;
        if(qem&&qs&&g_quest_ed_sel_ptr&&*g_quest_ed_sel_ptr>=0&&*g_quest_ed_sel_ptr<qs->count){
            Quest *q=&qs->quests[*g_quest_ed_sel_ptr];
            shell.quest_obj_count=q->obj_count;
            shell.quest_obj_sel=g_quest_ed_obj_sel_ptr?*g_quest_ed_obj_sel_ptr:0;
            int oi=shell.quest_obj_sel;
            if(oi>=0&&oi<q->obj_count){
                Objective *o=&q->objs[oi];
                shell.quest_obj_type=(int)o->type;
                shell.quest_obj_required_ptr=&o->required;
                shell.quest_obj_etype_ptr=&o->etype_filter;
                shell.quest_obj_loc_x=o->loc.x;
                shell.quest_obj_loc_y=o->loc.y;
                shell.quest_obj_loc_z=o->loc.z;
                shell.quest_obj_radius=o->radius;
            }
            shell.quest_obj_desc_buf=g_quest_ed_obj_desc_buf_ptr;
            shell.quest_obj_desc_buf_cap=64;
        }
    }
    shell.player_char_preset_count=g_player_char_library.count;
    shell.player_char_preset_selected=g_player_char_editor_index;
    shell.player_char_active_preset=g_player_char_library.active_index;
    for(int _pi=0;_pi<g_player_char_library.count&&_pi<32;_pi++)
        shell.player_char_preset_labels[_pi]=g_player_char_presets[_pi].name;
    {
        PlayerCharacterConfig *selected_cfg=player_character_selected_preset();
        shell.player_char_name_buf=selected_cfg?selected_cfg->name:NULL;
        shell.player_char_name_cap=selected_cfg?(int)sizeof(selected_cfg->name):0;
        shell.player_char_model_path=selected_cfg?selected_cfg->model_path:NULL;
        shell.player_char_model_label=(selected_cfg&&selected_cfg->model_path[0])?selected_cfg->model_path:"models/main_guy.glb";
        shell.player_char_scale=selected_cfg?selected_cfg->scale:1.0f;
    }
    shell.player_char_model_count=0;
    for(int _mi=0;_mi<g_player_char_model_option_count&&_mi<32;_mi++){
        shell.player_char_model_labels[_mi]=g_player_char_model_names[_mi][0]?g_player_char_model_names[_mi]:g_player_char_model_paths[_mi];
        shell.player_char_model_paths[_mi]=g_player_char_model_paths[_mi];
        shell.player_char_model_count++;
    }
    shell.player_char_anim_count=1;
    shell.player_char_anim_labels[0]="(Auto)";
    for(int _si=0;_si<PLAYER_CHAR_SLOT_COUNT;_si++) shell.player_char_slot_selected[_si]=0;
    shell.player_char_sound_count=1;
    shell.player_char_sound_labels[0]="(None)";
    for(int _si=0;_si<PLAYER_CHAR_SLOT_COUNT;_si++) shell.player_char_slot_sound_selected[_si]=0;
    for(int _si=0;_si<g_player_char_sound_option_count&&_si<127;_si++)
        shell.player_char_sound_labels[_si+1]=g_player_char_sound_names[_si][0]?g_player_char_sound_names[_si]:g_player_char_sound_paths[_si];
    shell.player_char_sound_count=1+((g_player_char_sound_option_count<127)?g_player_char_sound_option_count:127);
    if(char_model&&scene_has_data(&char_model->base)){
        int anim_cursor=1;
        for(int _scene_i=-1;_scene_i<char_model->extra_count&&anim_cursor<128;_scene_i++){
            Scene *anim_scene=(_scene_i<0)?&char_model->base:&char_model->extra[_scene_i];
            int anim_count=scene_anim_count(anim_scene);
            for(int _ai=0;_ai<anim_count&&anim_cursor<128;_ai++){
                const char *anim_name=scene_anim_name_at(anim_scene,_ai);
                shell.player_char_anim_labels[anim_cursor++]=(anim_name&&anim_name[0])?anim_name:"(Unnamed)";
            }
        }
        shell.player_char_anim_count=anim_cursor;
        for(int _slot=0;_slot<PLAYER_CHAR_SLOT_COUNT;_slot++){
            PlayerCharacterConfig *selected_cfg=player_character_selected_preset();
            const char *selected_name=selected_cfg?selected_cfg->anim_override[_slot]:NULL;
            const char *selected_sound=selected_cfg?selected_cfg->anim_sound_path[_slot]:NULL;
            if(selected_name&&selected_name[0]){
                int lookup_cursor=1;
                for(int _scene_i=-1;_scene_i<char_model->extra_count&&lookup_cursor<shell.player_char_anim_count;_scene_i++){
                    Scene *anim_scene=(_scene_i<0)?&char_model->base:&char_model->extra[_scene_i];
                    int anim_count=scene_anim_count(anim_scene);
                    for(int _ai=0;_ai<anim_count&&lookup_cursor<shell.player_char_anim_count;_ai++,lookup_cursor++){
                        const char *anim_name=scene_anim_name_at(anim_scene,_ai);
                        if(anim_name&&strcmp(anim_name,selected_name)==0){
                            shell.player_char_slot_selected[_slot]=lookup_cursor;
                            _scene_i=char_model->extra_count;
                            break;
                        }
                    }
                }
            }
            if(selected_sound&&selected_sound[0]){
                for(int _snd=0;_snd<g_player_char_sound_option_count&&_snd<127;_snd++){
                    if(strcmp(g_player_char_sound_paths[_snd],selected_sound)==0){
                        shell.player_char_slot_sound_selected[_slot]=_snd+1;
                        break;
                    }
                }
            }
        }
    }
    shell.prop_count=prop_count;
    shell.marker_count=marker_count;
    shell.npc_count=npc_count;
    shell.spawn_count=spawn_count;
    shell.spawn_has_selection=g_shell_spawn_has_selection||(g_editor_selection.kind==EDITOR_SELECTION_SPAWN);
    shell.spawn_sel_type_name=g_shell_spawn_focus_type_name;
    shell.spawn_model_count=g_enemy_model_count;
    for(int _mi=0;_mi<g_enemy_model_count&&_mi<32;_mi++) shell.spawn_model_names[_mi]=g_enemy_model_names[_mi];
    shell.spawn_sel_model_idx=g_shell_spawn_model_idx_ptr;
    shell.spawn_model_scale=g_shell_spawn_model_scale_ptr;
    shell.spawn_sel_hp=g_shell_spawn_hp_over_ptr;
    shell.spawn_sel_dmg=g_shell_spawn_dmg_over_ptr;
    shell.spawn_sel_speed=g_shell_spawn_speed_over_ptr;
    shell.spawn_sel_chase_r=g_shell_spawn_chase_r_over_ptr;
    shell.spawn_sel_atk_r=g_shell_spawn_atk_r_over_ptr;
    shell.spawn_sel_atk_cd=g_shell_spawn_atk_cd_over_ptr;
    shell.spawn_sel_scale=g_shell_spawn_scale_over_ptr;
    shell.spawn_def_hp=g_shell_spawn_def_hp;
    shell.spawn_def_dmg=g_shell_spawn_def_dmg;
    shell.spawn_def_speed=g_shell_spawn_def_speed;
    shell.spawn_def_chase_r=g_shell_spawn_def_chase_r;
    shell.spawn_def_atk_r=g_shell_spawn_def_atk_r;
    shell.spawn_def_atk_cd=g_shell_spawn_def_atk_cd;
    shell.spawn_def_scale=g_shell_spawn_def_scale;
    shell.active_mode_text=mode_buf;
    shell.editor_ui_text=backend_buf;
    shell.world_content_text=world_buf;
    shell.population_text=actor_buf;
    shell.inspect_label=inspect_label;
    shell.inspect_line_a=inspect_a_ptr;
    shell.inspect_line_b=inspect_b_ptr;
    shell.inspect_line_c=inspect_c_ptr;
    shell.tool_title=tool_title;
    shell.tool_line_a=tool_a_ptr;
    shell.tool_line_b=tool_b_ptr;
    shell.tool_line_c=tool_c_ptr;
    shell.plight_has_selection=false;
    shell.plight_selected_index=plight_sel;
    shell.plight_pos_x=0.0f;
    shell.plight_pos_y=0.0f;
    shell.plight_pos_z=0.0f;
    shell.plight_color_r=plight_place_r;
    shell.plight_color_g=plight_place_g;
    shell.plight_color_b=plight_place_b;
    shell.plight_type_selected=editor_state?editor_state->plight_place_type:0;
    shell.plight_spot_yaw=editor_state?editor_state->plight_place_spot_yaw:0.0f;
    shell.plight_spot_pitch=editor_state?editor_state->plight_place_spot_pitch:-35.0f;
    shell.plight_spot_inner_deg=editor_state?editor_state->plight_place_spot_inner_deg:18.0f;
    shell.plight_spot_outer_deg=editor_state?editor_state->plight_place_spot_outer_deg:32.0f;
    shell.world_light_override=g_world_light_cfg.override_active;
    shell.world_light_yaw=g_world_light_cfg.sun_yaw_deg;
    shell.world_light_pitch=g_world_light_cfg.sun_pitch_deg;
    shell.world_light_sun_r=g_world_light_cfg.sun_color.x;
    shell.world_light_sun_g=g_world_light_cfg.sun_color.y;
    shell.world_light_sun_b=g_world_light_cfg.sun_color.z;
    shell.world_light_ambient_r=g_world_light_cfg.ambient.x;
    shell.world_light_ambient_g=g_world_light_cfg.ambient.y;
    shell.world_light_ambient_b=g_world_light_cfg.ambient.z;
    shell.plight_radius=plight_place_radius;
    shell.plight_intensity=plight_place_intensity;
    shell.plight_y_offset=plight_place_y_offset;
    shell.plight_flicker=plight_place_flicker;
    shell.plight_flicker_speed=plight_place_flicker_speed;
    shell.szone_has_selection=false;
    shell.szone_selected_index=szone_sel;
    shell.szone_pos_x=0.0f;
    shell.szone_pos_z=0.0f;
    shell.szone_type_selected=editor_state?editor_state->szone_type:SOUND_ZONE_TYPE_AMBIENT;
    shell.szone_ambient_sound_selected=(editor_state&&editor_state->szone_sound_idx>=0&&editor_state->szone_sound_idx<SZONE_SOUND_COUNT)
        ?editor_state->szone_sound_idx:0;
    shell.szone_music_selected=editor_state?editor_state->szone_music_idx:0;
    shell.szone_radius=editor_state?editor_state->szone_place_radius:22.0f;
    shell.szone_volume=editor_state?editor_state->szone_place_vol:80.0f;
    shell.szone_fade_length=editor_state?editor_state->szone_fade_len:6.0f;
    shell.szone_ambient_sound_count=SZONE_SOUND_COUNT;
    for(int _ii=0;_ii<SZONE_SOUND_COUNT&&_ii<32;_ii++) shell.szone_ambient_sound_labels[_ii]=g_szone_sound_names[_ii];
    shell.szone_music_count=(g_sound_zone_music_option_count<128)?g_sound_zone_music_option_count:128;
    for(int _ii=0;_ii<shell.szone_music_count;_ii++) shell.szone_music_labels[_ii]=g_sound_zone_music_names[_ii];
    shell.surface_has_selection=false;
    shell.surface_kind=editor_state?editor_state->surface_kind:0;
    shell.surface_width=editor_state?editor_state->surface_width:4.0f;
    shell.surface_height=editor_state?editor_state->surface_height:3.0f;
    shell.surface_thickness=editor_state?editor_state->surface_thickness:0.20f;
    shell.surface_coll_selected=editor_state?editor_state->surface_coll_type:PROP_COLL_AABB;
    shell.surface_coll_name=prop_coll_name((PropCollType)shell.surface_coll_selected);
    shell.surface_material_count=0;
    shell.surface_material_selected=-1;
    for(int _ii=0;_ii<64;_ii++){ shell.surface_material_labels[_ii]=NULL; shell.surface_material_indices[_ii]=-1; }
    shell.interior_material_count=0;
    shell.interior_material_selected=editor_state?editor_state->interior_material_sel:-1;
    for(int _ii=0;_ii<64;_ii++){ shell.interior_material_labels[_ii]=NULL; shell.interior_material_indices[_ii]=-1; }
    shell.collision_has_selection=false;
    shell.collision_width=editor_state?editor_state->collision_width:4.0f;
    shell.collision_height=editor_state?editor_state->collision_height:3.0f;
    shell.collision_depth=editor_state?editor_state->collision_depth:4.0f;
    shell.collision_shape_selected=editor_state?editor_state->collision_shape:0;
    shell.collision_shape_name=shell.collision_shape_selected==1?"Oval":"Box";
    shell.collision_yaw=editor_state?editor_state->collision_place_yaw:0.0f;
    shell.collision_pitch=editor_state?editor_state->collision_place_pitch:0.0f;
    shell.collision_enabled=editor_state?editor_state->collision_enabled:true;
    shell.collision_nav_blocker=editor_state?editor_state->collision_nav_blocker:true;
    shell.collision_motion_enabled=editor_state?editor_state->collision_motion_enabled:false;
    shell.collision_motion_x=editor_state?editor_state->collision_motion_x:0.0f;
    shell.collision_motion_y=editor_state?editor_state->collision_motion_y:0.0f;
    shell.collision_motion_z=editor_state?editor_state->collision_motion_z:0.0f;
    shell.collision_motion_period=editor_state?editor_state->collision_motion_period:2.0f;
    shell.collision_motion_phase=editor_state?editor_state->collision_motion_phase:0.0f;
    if(editor_state&&editor_state->interior_edit){
        for(int _ii=0;_ii<interior_material_count()&&shell.interior_material_count<64;_ii++){
            if(!interior_material_supports_surface(_ii,editor_state->interior_surface_target)) continue;
            shell.interior_material_labels[shell.interior_material_count]=interior_material_name(_ii);
            shell.interior_material_indices[shell.interior_material_count]=_ii;
            if(_ii==editor_state->interior_material_sel) shell.interior_material_selected=shell.interior_material_count;
            shell.interior_material_count++;
        }
        if(shell.interior_material_selected<0&&shell.interior_material_count>0) shell.interior_material_selected=0;
    }
    if(editor_state&&editor_state->surface_edit){
        int wanted_kind=editor_state->surface_kind;
        int selected_material=editor_state->surface_material_sel;
        if(g_editor_selection.kind==EDITOR_SELECTION_SURFACE&&
           surface_piece_query(g_editor_selection.index,&shell.surface_kind,&shell.surface_width,&shell.surface_height,&shell.surface_thickness,&selected_material)){
            shell.surface_has_selection=true;
            shell.surface_coll_selected=surface_piece_collision_type(g_editor_selection.index);
            shell.surface_coll_name=prop_coll_name((PropCollType)shell.surface_coll_selected);
            wanted_kind=shell.surface_kind;
        }else{
            wanted_kind=editor_state->surface_kind;
        }
        for(int _ii=0;_ii<interior_material_count()&&shell.surface_material_count<64;_ii++){
            if(!surface_piece_material_supported(_ii,wanted_kind)) continue;
            shell.surface_material_labels[shell.surface_material_count]=interior_material_name(_ii);
            shell.surface_material_indices[shell.surface_material_count]=_ii;
            if(_ii==selected_material)
                shell.surface_material_selected=shell.surface_material_count;
            shell.surface_material_count++;
        }
        if(shell.surface_material_selected<0&&shell.surface_material_count>0) shell.surface_material_selected=0;
    }
    if(editor_state&&editor_state->collision_edit){
        if(g_editor_selection.kind==EDITOR_SELECTION_COLLISION_VOLUME&&
           collision_volume_query(g_editor_selection.index,&shell.collision_width,&shell.collision_height,
                                  &shell.collision_depth,&shell.collision_enabled,&shell.collision_nav_blocker)){
            Vec3 motion_offset={0};
            shell.collision_has_selection=true;
            collision_volume_describe(g_editor_selection.index,NULL,&shell.collision_yaw,NULL,NULL,NULL,NULL,NULL);
            if(collision_volume_shape_query(g_editor_selection.index,&shell.collision_shape_selected,&shell.collision_pitch))
                shell.collision_shape_name=shell.collision_shape_selected==1?"Oval":"Box";
            if(collision_volume_motion_describe(g_editor_selection.index,&shell.collision_motion_enabled,&motion_offset,
                                               &shell.collision_motion_period,&shell.collision_motion_phase)){
                shell.collision_motion_x=motion_offset.x;
                shell.collision_motion_y=motion_offset.y;
                shell.collision_motion_z=motion_offset.z;
            }
        }
    }
    if(plight_edit&&placed_light_query(plight_sel,
                                       &(Vec3){0},&(Vec3){0},NULL,NULL,NULL,NULL,NULL,
                                       NULL,NULL,NULL,NULL,NULL)){
        Vec3 pl_pos={0},pl_color={0};
        float pl_radius=0.0f,pl_intensity=0.0f,pl_y_offset=0.0f,pl_flicker_speed=0.0f;
        float pl_spot_yaw=0.0f,pl_spot_pitch=0.0f,pl_spot_inner=0.0f,pl_spot_outer=0.0f;
        bool pl_flicker=false;
        int pl_type=0;
        placed_light_query(plight_sel,&pl_pos,&pl_color,&pl_radius,&pl_intensity,&pl_y_offset,
                           &pl_flicker,&pl_flicker_speed,&pl_type,&pl_spot_yaw,&pl_spot_pitch,
                           &pl_spot_inner,&pl_spot_outer);
        shell.plight_has_selection=true;
        shell.plight_selected_index=plight_sel;
        shell.plight_pos_x=pl_pos.x;
        shell.plight_pos_y=pl_pos.y;
        shell.plight_pos_z=pl_pos.z;
        shell.plight_color_r=pl_color.x;
        shell.plight_color_g=pl_color.y;
        shell.plight_color_b=pl_color.z;
        shell.plight_radius=pl_radius;
        shell.plight_intensity=pl_intensity;
        shell.plight_y_offset=pl_y_offset;
        shell.plight_flicker=pl_flicker;
        shell.plight_flicker_speed=pl_flicker_speed;
        shell.plight_type_selected=pl_type;
        shell.plight_spot_yaw=pl_spot_yaw;
        shell.plight_spot_pitch=pl_spot_pitch;
        shell.plight_spot_inner_deg=pl_spot_inner;
        shell.plight_spot_outer_deg=pl_spot_outer;
    }
    if(szone_edit&&szone_sel>=0&&szone_sel<g_sound_zones.count&&g_sound_zones.items[szone_sel].active){
        SoundZone *sz=&g_sound_zones.items[szone_sel];
        shell.szone_has_selection=true;
        shell.szone_selected_index=szone_sel;
        shell.szone_pos_x=sz->cx;
        shell.szone_pos_z=sz->cz;
        shell.szone_type_selected=(sz->type==SOUND_ZONE_TYPE_MUSIC)?SOUND_ZONE_TYPE_MUSIC:SOUND_ZONE_TYPE_AMBIENT;
        shell.szone_ambient_sound_selected=(sz->sound_idx>=0&&sz->sound_idx<SZONE_SOUND_COUNT)?sz->sound_idx:0;
        shell.szone_music_selected=0;
        if(sz->type==SOUND_ZONE_TYPE_MUSIC){
            int music_idx=sound_zone_music_index_from_path(sz->music_path);
            if(music_idx>=0&&music_idx<shell.szone_music_count) shell.szone_music_selected=music_idx;
        }
        shell.szone_radius=sz->radius;
        shell.szone_volume=sz->volume;
        shell.szone_fade_length=sz->fade_len;
    }
    shell.forest_brush_kind=forest_brush_kind;
    shell.tree_brush_scale=tree_brush_scale;
    shell.wall_brush_spacing=wall_brush_spacing;
    shell.wall_brush_scale=wall_brush_scale;
    shell.npc_place_mode=npc_place_mode;
    shell.npc_sel_type=npc_sel_type;
    shell.npc_type_count=g_npc_model_count>0?g_npc_model_count:1;
    if(shell.npc_sel_type<0||shell.npc_sel_type>=shell.npc_type_count) shell.npc_sel_type=0;
    for(int _ti=0;_ti<shell.npc_type_count&&_ti<NPC_MODEL_MAX;_ti++){
        shell.npc_type_names[_ti]=g_npc_type_names_early[_ti];
        shell.npc_type_loaded[_ti]=g_npc_model_ok[_ti];
        shell.npc_type_r[_ti]=g_npc_type_colors_early[_ti][0];
        shell.npc_type_g[_ti]=g_npc_type_colors_early[_ti][1];
        shell.npc_type_b[_ti]=g_npc_type_colors_early[_ti][2];
        shell.npc_type_placed_counts[_ti]=g_shell_npc_type_counts[_ti];
    }
    shell.npc_has_selection=(g_editor_selection.kind==EDITOR_SELECTION_NPC&&
                             g_editor_selection.index>=0&&g_editor_selection.index<npc_count)||
                            (npc_selected_index>=0&&npc_selected_index<npc_count);
    shell.npc_selected_index=(g_editor_selection.kind==EDITOR_SELECTION_NPC&&
                              g_editor_selection.index>=0&&g_editor_selection.index<npc_count)
        ?g_editor_selection.index:npc_selected_index;
    shell.npc_has_focus=npc_has_focus;
    shell.npc_focus_index=npc_focus_index;
    shell.npc_focus_name=npc_focus_name?npc_focus_name:"";
    shell.npc_focus_name_buffer=npc_focus_name_buffer;
    shell.npc_focus_name_capacity=npc_focus_name_capacity;
    shell.npc_focus_backstory_buffer=npc_focus_backstory_buffer;
    shell.npc_focus_backstory_capacity=npc_focus_backstory_capacity;
    shell.npc_focus_model_idx=npc_focus_model_idx;
    shell.npc_focus_scale=npc_focus_scale;
    shell.npc_focus_visual_yaw_offset=npc_focus_visual_yaw_offset;
    shell.npc_focus_collision_enabled=npc_focus_collision_enabled;
    shell.npc_focus_patrol_loop=npc_focus_patrol_loop;
    if(shell.npc_focus_model_idx<0||shell.npc_focus_model_idx>=shell.npc_type_count) shell.npc_focus_model_idx=0;
    shell.npc_waypoint_mode=npc_ed_wp_mode;
    shell.npc_waypoint_count=npc_waypoint_count;
    for(int i=0;i<npc_waypoint_count&&i<EDITOR_UI_NPC_WAYPOINT_MAX;i++){
        shell.npc_waypoint_x[i]=npc_waypoint_x?npc_waypoint_x[i]:0.0f;
        shell.npc_waypoint_y[i]=npc_waypoint_y?npc_waypoint_y[i]:0.0f;
        shell.npc_waypoint_z[i]=npc_waypoint_z?npc_waypoint_z[i]:0.0f;
    }
    shell.npc_patrol_speed=npc_patrol_speed;
    shell.npc_patrol_wait=npc_patrol_wait;
    shell.npc_is_merchant=npc_is_merchant;
    shell.npc_shop_id=npc_shop_id;
    shell.npc_shop_name=npc_shop_name?npc_shop_name:"";
    shell.npc_dialogue_mode=npc_ed_dlg_mode;
    shell.npc_dialogue_count=npc_dialogue_count;
    shell.npc_dialogue_sel=npc_ed_dlg_sel;
    shell.npc_dialogue_buffer=npc_ed_dlg_buf;
    shell.npc_dialogue_buffer_capacity=NPC_LINE_LEN;
    for(int i=0;i<EDITOR_UI_NPC_DIALOGUE_MAX;i++) shell.npc_dialogue_lines[i]=(npc_dialogue_lines&&i<npc_dialogue_count)?npc_dialogue_lines[i]:NULL;
    shell.npc_tree_mode=npc_ed_tree_mode;
    shell.npc_tree_count=npc_tree_count;
    shell.npc_tree_sel=npc_ed_tree_sel;
    shell.npc_tree_root=npc_tree_root;
    shell.npc_tree_choice_sel=npc_ed_tree_choice;
    shell.npc_tree_choice_count=npc_tree_choice_count;
    shell.npc_tree_text=npc_tree_text;
    shell.npc_tree_text_capacity=npc_tree_text_capacity;
    shell.npc_tree_speaker=npc_tree_speaker;
    shell.npc_tree_speaker_capacity=npc_tree_speaker_capacity;
    shell.npc_tree_anim_trigger=npc_tree_anim_trigger;
    for(int i=0;i<EDITOR_UI_NPC_TREE_CHOICES_MAX;i++){
        shell.npc_tree_choice_labels[i]=(npc_tree_choice_labels&&i<npc_tree_choice_count)?npc_tree_choice_labels[i]:NULL;
        shell.npc_tree_choice_next[i]=(npc_tree_choice_next&&i<npc_tree_choice_count)?npc_tree_choice_next[i]:NULL;
    }
    /* Dialogue graph canvas: filled from file-scope statics updated by dlg_graph_sync() */
    shell.npc_tree_node_canvas_x=g_dlg_canvas_x;
    shell.npc_tree_node_canvas_y=g_dlg_canvas_y;
    for(int _i=0;_i<EDITOR_UI_NPC_TREE_MAX;_i++){
        shell.npc_tree_all_choice_count[_i]=g_dlg_all_cc[_i];
        for(int _ci=0;_ci<4;_ci++){
            shell.npc_tree_all_choice_next[_i][_ci]=g_dlg_all_cn[_i][_ci];
            for(int _k=0;_k<64;_k++) shell.npc_tree_all_choice_labels[_i][_ci][_k]=g_dlg_all_labels[_i][_ci][_k];
        }
        for(int _k=0;_k<48;_k++) shell.npc_tree_all_text[_i][_k]=g_dlg_all_text[_i][_k];
        for(int _k=0;_k<32;_k++) shell.npc_tree_all_speaker[_i][_k]=g_dlg_all_speaker[_i][_k];
    }
    shell.marker_has_selection=false;
    shell.marker_kind=0;
    shell.marker_radius=0.0f;
    shell.marker_pos_y=0.0f;
    shell.marker_has_selection=editor_selection_marker_state(&g_editor_selection,
                                                             &shell.marker_kind,
                                                             &shell.marker_radius,
                                                             &shell.marker_pos_y);
    {
        int selected_prop=-1;
        shell.prop_has_selection=false;
        shell.prop_name="";
        shell.prop_transform_mode=prop_mouse_mode;
        shell.gizmo_local_space=editor_state?(editor_state->selection_gizmo_space!=0):false;
        shell.gizmo_snap_enabled=editor_state?editor_state->selection_gizmo_snap:false;
        shell.prop_snap_y=editor_shell_placer_snap_y(placer);
        shell.prop_pos_x=0.0f;
        shell.prop_pos_y=0.0f;
        shell.prop_pos_z=0.0f;
        shell.prop_rot_y=0.0f;
        shell.prop_scale=1.0f;
        shell.prop_coll_name="None";
        shell.prop_coll_selected=PROP_COLL_NONE;
        shell.prop_custom_coll_enabled=false;
        shell.prop_custom_coll_count=0;
        shell.prop_custom_coll_selected=-1;
        shell.prop_custom_box_gizmo_mode=editor_state?editor_state->prop_custom_box_gizmo_mode:false;
        shell.prop_custom_box_center_x=0.0f;
        shell.prop_custom_box_center_y=0.0f;
        shell.prop_custom_box_center_z=0.0f;
        shell.prop_custom_box_half_x=0.5f;
        shell.prop_custom_box_half_y=0.5f;
        shell.prop_custom_box_half_z=0.5f;
        shell.prop_custom_box_yaw=0.0f;
        shell.prop_pickable=false;
        shell.prop_pickup_item=0;
        shell.prop_pickup_count=1;
        shell.prop_pickup_item_count=ITEM_COUNT;
        shell.prop_interact_name=prop_interact_name(PROP_INTERACT_NONE);
        shell.prop_interact_type=PROP_INTERACT_NONE;
        shell.prop_interact_radius=2.5f;
        shell.prop_door_open_angle=90.0f;
        shell.prop_action_count=0;
        shell.prop_action_selected=0;
        shell.prop_action_type=PROP_ACTION_NONE;
    shell.prop_action_name=prop_action_name(PROP_ACTION_NONE);
    shell.prop_action_radius=8.0f;
    shell.prop_action_target_index=-1;
    shell.prop_action_target_label="None";
    shell.prop_action_target_count=0;
    shell.prop_action_condition_type=PROP_ACTION_COND_NONE;
    shell.prop_action_condition_name=prop_action_condition_name(PROP_ACTION_COND_NONE);
    shell.prop_action_condition_flag=0;
    shell.prop_action_sound_path=NULL;
    shell.prop_action_sound_path_capacity=0;
    shell.prop_action_prompt_text=NULL;
    shell.prop_action_prompt_text_capacity=0;
        for(int i=0;i<ITEM_COUNT&&i<64;i++) shell.prop_pickup_item_names[i]=editor_item_type_name(i);
        if(g_editor_selection.kind==EDITOR_SELECTION_PROP&&
           g_editor_selection.index>=0&&g_editor_selection.index<g_props.count&&
           g_props.list[g_editor_selection.index].active){
            selected_prop=g_editor_selection.index;
        }else if(prop_sel&&*prop_sel>=0&&*prop_sel<g_props.count&&g_props.list[*prop_sel].active){
            selected_prop=*prop_sel;
        }else{
            int placer_selected=editor_shell_placer_selected_prop(placer);
            if(placer_selected>=0&&placer_selected<g_props.count&&g_props.list[placer_selected].active) selected_prop=placer_selected;
        }
        if(selected_prop>=0){
            shell.prop_has_selection=true;
            shell.prop_name=g_props.list[selected_prop].path;
            shell.prop_pos_x=g_props.list[selected_prop].pos.x;
            shell.prop_pos_y=g_props.list[selected_prop].pos.y;
            shell.prop_pos_z=g_props.list[selected_prop].pos.z;
            shell.prop_rot_y=g_props.list[selected_prop].rot_y;
            shell.prop_scale=g_props.list[selected_prop].scale;
            shell.prop_coll_name=prop_coll_name(g_props.list[selected_prop].coll_type);
            shell.prop_coll_selected=(int)g_props.list[selected_prop].coll_type;
        shell.prop_custom_coll_enabled=g_props.list[selected_prop].custom_coll_enabled;
            shell.prop_custom_coll_count=g_props.list[selected_prop].custom_coll_count;
            shell.prop_custom_coll_selected=g_props.list[selected_prop].custom_coll_selected;
            if(shell.prop_custom_coll_selected>=0&&shell.prop_custom_coll_selected<g_props.list[selected_prop].custom_coll_count){
                PropCustomCollisionBox *box=&g_props.list[selected_prop].custom_coll[shell.prop_custom_coll_selected];
                shell.prop_custom_box_center_x=box->center.x;
                shell.prop_custom_box_center_y=box->center.y;
                shell.prop_custom_box_center_z=box->center.z;
                shell.prop_custom_box_shape=box->shape;
                shell.prop_custom_box_half_x=box->half.x;
                shell.prop_custom_box_half_y=box->half.y;
                shell.prop_custom_box_half_z=box->half.z;
                shell.prop_custom_box_yaw=box->yaw_deg;
            }
            shell.prop_pickable=g_props.list[selected_prop].pickable;
            shell.prop_pickup_item=(int)g_props.list[selected_prop].pickup_item;
            shell.prop_pickup_count=g_props.list[selected_prop].pickup_count;
            shell.prop_interact_type=(int)g_props.list[selected_prop].interact_type;
            shell.prop_interact_name=prop_interact_name(g_props.list[selected_prop].interact_type);
            shell.prop_interact_radius=g_props.list[selected_prop].interact_radius;
            shell.prop_door_open_angle=g_props.list[selected_prop].door_open_angle;
            shell.prop_action_count=g_props.list[selected_prop].action_count;
            shell.prop_action_selected=g_props.list[selected_prop].action_selected;
            if(shell.prop_action_selected<0) shell.prop_action_selected=0;
            if(shell.prop_action_selected>=shell.prop_action_count&&shell.prop_action_count>0)
                shell.prop_action_selected=shell.prop_action_count-1;
            if(shell.prop_action_count>0&&shell.prop_action_selected>=0&&shell.prop_action_selected<shell.prop_action_count){
                PropAction *action=&g_props.list[selected_prop].actions[shell.prop_action_selected];
                shell.prop_action_type=(int)action->type;
                shell.prop_action_name=prop_action_name(action->type);
                shell.prop_action_radius=action->radius;
                shell.prop_action_target_index=action->target_index;
                shell.prop_action_sound_idx=action->sound_idx;
                shell.prop_action_sound_volume=action->sound_volume;
                shell.prop_action_sound_name=(action->sound_idx>=0&&action->sound_idx<PROP_ACTION_SOUND_COUNT)?
                    g_prop_action_sound_names[action->sound_idx]:"Menu Click";
                shell.prop_action_sound_count=PROP_ACTION_SOUND_COUNT;
                for(int i=0;i<PROP_ACTION_SOUND_COUNT&&i<32;i++)
                    shell.prop_action_sound_names[i]=g_prop_action_sound_names[i];
                shell.prop_action_sound_path=action->sound_path;
                shell.prop_action_sound_path_capacity=(int)sizeof(action->sound_path);
                shell.prop_action_condition_type=(int)action->condition_type;
                shell.prop_action_condition_name=prop_action_condition_name(action->condition_type);
                shell.prop_action_condition_flag=action->condition_flag;
                shell.prop_action_prompt_text=action->prompt_text;
                shell.prop_action_prompt_text_capacity=(int)sizeof(action->prompt_text);
            }
        }
        {
            static char action_target_labels[128][128];
            int target_count=0;
            if(shell.prop_action_type==PROP_ACTION_TOGGLE_TARGET_LIGHT){
                snprintf(action_target_labels[target_count],sizeof(action_target_labels[target_count]),"None");
                shell.prop_action_target_ids[target_count]=-1;
                shell.prop_action_target_labels[target_count]=action_target_labels[target_count];
                target_count++;
                for(int i=0;i<editor_action_target_light_count()&&target_count<128;i++){
                    int uid=editor_action_target_light_uid_at(i);
                    if(uid<=0) continue;
                    snprintf(action_target_labels[target_count],sizeof(action_target_labels[target_count]),
                             "Light %d (uid %d)",i+1,uid);
                    shell.prop_action_target_ids[target_count]=uid;
                    shell.prop_action_target_labels[target_count]=action_target_labels[target_count];
                    if(uid==shell.prop_action_target_index)
                        shell.prop_action_target_label=shell.prop_action_target_labels[target_count];
                    target_count++;
                }
            }else if(shell.prop_action_type==PROP_ACTION_TOGGLE_TARGET_COLLISION_VOLUME){
                snprintf(action_target_labels[target_count],sizeof(action_target_labels[target_count]),"None");
                shell.prop_action_target_ids[target_count]=-1;
                shell.prop_action_target_labels[target_count]=action_target_labels[target_count];
                target_count++;
                for(int i=0;i<editor_action_target_collision_volume_count()&&target_count<128;i++){
                    int uid=editor_action_target_collision_volume_uid_at(i);
                    if(uid<=0) continue;
                    snprintf(action_target_labels[target_count],sizeof(action_target_labels[target_count]),
                             "Collision %d (uid %d)",i+1,uid);
                    shell.prop_action_target_ids[target_count]=uid;
                    shell.prop_action_target_labels[target_count]=action_target_labels[target_count];
                    if(uid==shell.prop_action_target_index)
                        shell.prop_action_target_label=shell.prop_action_target_labels[target_count];
                    target_count++;
                }
            }else if(shell.prop_action_type==PROP_ACTION_TOGGLE_TARGET_DOOR){
                snprintf(action_target_labels[target_count],sizeof(action_target_labels[target_count]),"None");
                shell.prop_action_target_ids[target_count]=-1;
                shell.prop_action_target_labels[target_count]=action_target_labels[target_count];
                target_count++;
                for(int i=0;i<g_props.count&&target_count<128;i++){
                    if(!g_props.list[i].active||g_props.list[i].interact_type!=PROP_INTERACT_DOOR) continue;
                    snprintf(action_target_labels[target_count],sizeof(action_target_labels[target_count]),
                             "Door %d (uid %d)",i+1,g_props.list[i].uid);
                    shell.prop_action_target_ids[target_count]=g_props.list[i].uid;
                    shell.prop_action_target_labels[target_count]=action_target_labels[target_count];
                    if(g_props.list[i].uid==shell.prop_action_target_index)
                        shell.prop_action_target_label=shell.prop_action_target_labels[target_count];
                    target_count++;
                }
            }
            shell.prop_action_target_count=target_count;
        }
        shell.placer_prefab_mode=editor_shell_placer_prefab_mode(placer);
        shell.placer_search_buffer=editor_shell_placer_search_buffer(placer);
        shell.placer_search_buffer_capacity=editor_shell_placer_search_capacity(placer);
        shell.placer_item_count=editor_shell_placer_item_count(placer);
        shell.placer_item_selected=editor_shell_placer_item_selected(placer);
        shell.placer_snap_y=editor_shell_placer_snap_y(placer);
        shell.placer_rotation_y=editor_shell_placer_rotation_y(placer);
        shell.placer_scale=editor_shell_placer_scale(placer);
        shell.placer_coll_name=editor_shell_placer_coll_name(placer);
        editor_shell_browser_ui_state(placer,
                                      &shell.browser_favorites_only,&shell.browser_recent_only,&shell.browser_category_filter,
                                      &shell.browser_selected_category,&shell.browser_selected_tags,
                                      &shell.browser_selected_status,&shell.browser_selected_favorite,
                                      &shell.browser_selected_recent);
        editor_shell_bulk_ui_state(placer,&shell.multi_prop_count,&shell.multi_prop_clipboard_count,
                                   &shell.bulk_scatter_radius,&shell.bulk_scatter_count,
                                   &shell.bulk_scatter_radius_ptr,&shell.bulk_scatter_count_ptr,
                                   &shell.bulk_path_spacing,&shell.bulk_path_spacing_ptr);
        editor_shell_prefab_ui_state(placer,selected_prop,
                                     &shell.prefab_library_count,&shell.prefab_selected_index,
                                     &shell.prefab_selected_name,&shell.prefab_selected_prop_count,&shell.prefab_selected_fx_count,
                                     &shell.prefab_capture_radius,&shell.prefab_capture_has_anchor,&shell.prefab_capture_uses_multiselect,
                                     &shell.prefab_name_buffer,&shell.prefab_name_buffer_capacity,
                                     &shell.prefab_capture_radius_ptr);
        for(int i=0;i<256;i++){
            shell.placer_item_labels[i]=editor_shell_placer_item_label(placer,i);
            shell.placer_item_thumb_ids[i]=editor_shell_placer_item_thumb(placer,i);
            shell.placer_item_favorite[i]=editor_shell_placer_item_favorite(placer,i);
            shell.placer_item_recent[i]=editor_shell_placer_item_recent(placer,i);
            shell.placer_item_valid[i]=editor_shell_placer_item_valid(placer,i);
            shell.placer_item_category[i]=editor_shell_placer_item_category(placer,i);
        }
    }
    actions.editor=editor_state;
    editor_shell_action_context_bind_editor(&actions);
    actions.selection=&g_editor_selection;
    actions.weather=weather;
    actions.cam=cam;
    actions.player=player;
    actions.char_model=char_model;
    actions.player_amap=player_amap;
    actions.player_char_editor_index=&g_player_char_editor_index;
    actions.qs=qs;
    actions.as=as;
    actions.enemies=enemies;
    actions.ne=ne;
    actions.lighting=g_active_editor_lighting;
    actions.total_time=total_time_ptr?*total_time_ptr:0.0f;
    actions.total_time_ptr=total_time_ptr;
    actions.inv=inv;
    actions.lvlsys=lvlsys;
    actions.save_note=save_note;
    actions.terrain=terrain;
    actions.forest=forest;
    actions.grass=grass;
    actions.world_markers=world_markers;
    actions.world_fx=world_fx;
    actions.world_fx_editor=world_fx_editor;
    actions.spawns=spawns;
    actions.npc_spawns=npc_spawns;
    actions.npcs_runtime=npcs_runtime;
    actions.placer=placer;
    shell.action_ctx=&actions;
    shell.activate_mode=editor_shell_activate_mode;
    shell.save_world=editor_shell_save_world_cb;
    shell.save_slot=editor_shell_save_slot_cb;
    shell.player_char_select_preset=editor_shell_player_char_select_preset;
    shell.player_char_add_preset=editor_shell_player_char_add_preset;
    shell.player_char_duplicate_preset=editor_shell_player_char_duplicate_preset;
    shell.player_char_delete_preset=editor_shell_player_char_delete_preset;
    shell.player_char_set_active_preset=editor_shell_player_char_set_active_preset;
    shell.player_char_commit_name=editor_shell_player_char_commit_name;
    shell.player_char_set_model=editor_shell_player_char_set_model;
    shell.player_char_set_scale=editor_shell_player_char_set_scale;
    shell.player_char_set_anim_slot=editor_shell_player_char_set_anim_slot;
    shell.player_char_set_anim_sound_slot=editor_shell_player_char_set_anim_sound_slot;
    shell.player_char_save=editor_shell_player_char_save;
    shell.tool_action=editor_shell_tool_action;
    shell.surface_material_select=editor_shell_select_surface_material;
    shell.surface_set_kind=editor_shell_set_surface_kind;
    shell.surface_set_width=editor_shell_set_surface_width;
    shell.surface_set_height=editor_shell_set_surface_height;
    shell.surface_set_thickness=editor_shell_set_surface_thickness;
    shell.surface_set_collision=editor_shell_set_surface_collision;
    shell.collision_set_width=editor_shell_set_collision_volume_width;
    shell.collision_set_height=editor_shell_set_collision_volume_height;
    shell.collision_set_depth=editor_shell_set_collision_volume_depth;
    shell.collision_set_shape=editor_shell_set_collision_volume_shape;
    shell.collision_set_yaw=editor_shell_set_collision_volume_yaw;
    shell.collision_set_pitch=editor_shell_set_collision_volume_pitch;
    shell.collision_set_enabled=editor_shell_set_collision_volume_enabled;
    shell.collision_set_nav_blocker=editor_shell_set_collision_volume_nav_blocker;
    shell.collision_set_motion_enabled=editor_shell_set_collision_volume_motion_enabled;
    shell.collision_set_motion_axis=editor_shell_set_collision_volume_motion_axis;
    shell.collision_set_motion_period=editor_shell_set_collision_volume_motion_period;
    shell.collision_set_motion_phase=editor_shell_set_collision_volume_motion_phase;
    shell.plight_set_color=editor_shell_plight_set_color;
    shell.plight_set_type=editor_shell_plight_set_type;
    shell.plight_set_radius=editor_shell_plight_set_radius;
    shell.plight_set_intensity=editor_shell_plight_set_intensity;
    shell.plight_set_y_offset=editor_shell_plight_set_y_offset;
    shell.plight_set_flicker=editor_shell_plight_set_flicker;
    shell.plight_set_flicker_speed=editor_shell_plight_set_flicker_speed;
    shell.plight_set_spot_angles=editor_shell_plight_set_spot_angles;
    shell.plight_set_spot_cone=editor_shell_plight_set_spot_cone;
    shell.plight_delete_selected=editor_shell_plight_delete_selected;
    shell.plight_set_position=editor_shell_plight_set_position;
    shell.world_light_set_override=editor_shell_world_light_set_override;
    shell.world_light_set_angles=editor_shell_world_light_set_angles;
    shell.world_light_set_sun_color=editor_shell_world_light_set_sun_color;
    shell.world_light_set_ambient_color=editor_shell_world_light_set_ambient_color;
    shell.szone_set_type=editor_shell_szone_set_type;
    shell.szone_set_position=editor_shell_szone_set_position;
    shell.szone_set_ambient_sound=editor_shell_szone_set_ambient_sound;
    shell.szone_set_music=editor_shell_szone_set_music;
    shell.szone_set_radius=editor_shell_szone_set_radius;
    shell.szone_set_volume=editor_shell_szone_set_volume;
    shell.szone_set_fade_length=editor_shell_szone_set_fade_length;
    shell.szone_delete_selected=editor_shell_szone_delete_selected;
    shell.interior_material_select=editor_shell_select_interior_material;
    shell.browser_refresh=editor_shell_browser_refresh;
    shell.browser_filter=editor_shell_browser_filter;
    shell.browser_import=editor_shell_browser_import;
    shell.browser_select=editor_shell_browser_select;
    shell.browser_toggle_favorite=editor_shell_browser_toggle_favorite;
    shell.browser_cycle_category_filter=editor_shell_browser_cycle_category_filter;
    shell.browser_toggle_favorites_only=editor_shell_browser_toggle_favorites_only;
    shell.browser_toggle_recent_only=editor_shell_browser_toggle_recent_only;
    shell.prefab_capture_selection=editor_shell_prefab_capture_selection;
    shell.prefab_delete_selected=editor_shell_prefab_delete_selected;
    shell.prefab_duplicate_selected=editor_shell_prefab_duplicate_selected;
    shell.prefab_commit_name=editor_shell_prefab_commit_name;
    shell.prefab_save_library=editor_shell_prefab_save_library;
    shell.bulk_align_x=editor_shell_bulk_align_x;
    shell.bulk_align_y=editor_shell_bulk_align_y;
    shell.bulk_align_z=editor_shell_bulk_align_z;
    shell.bulk_distribute_x=editor_shell_bulk_distribute_x;
    shell.bulk_distribute_z=editor_shell_bulk_distribute_z;
    shell.bulk_replace_selection=editor_shell_bulk_replace_selection;
    shell.bulk_copy_selection=editor_shell_bulk_copy_selection;
    shell.bulk_paste_selection=editor_shell_bulk_paste_selection;
    shell.bulk_duplicate_selection=editor_shell_bulk_duplicate_selection;
    shell.bulk_select_all=editor_shell_bulk_select_all;
    shell.bulk_clear_selection=editor_shell_bulk_clear_selection;
    shell.bulk_scatter_current=editor_shell_bulk_scatter_current;
    shell.bulk_stamp_path_current=editor_shell_bulk_stamp_path_current;
    shell.prop_set_rotation_y=editor_shell_prop_set_rotation_y;
    shell.prop_set_scale=editor_shell_prop_set_scale;
    shell.prop_set_collision_mode=editor_shell_prop_set_collision_mode;
    shell.prop_set_custom_collision_enabled=editor_shell_prop_set_custom_collision_enabled;
    shell.prop_set_custom_collision_box_gizmo_mode=editor_shell_prop_set_custom_collision_box_gizmo_mode;
    shell.prop_add_custom_collision_box=editor_shell_prop_add_custom_collision_box;
    shell.prop_remove_custom_collision_box=editor_shell_prop_remove_custom_collision_box;
    shell.prop_select_custom_collision_box=editor_shell_prop_select_custom_collision_box;
    shell.prop_set_custom_collision_box_shape=editor_shell_prop_set_custom_collision_box_shape;
    shell.prop_set_custom_collision_box_center=editor_shell_prop_set_custom_collision_box_center;
    shell.prop_set_custom_collision_box_half=editor_shell_prop_set_custom_collision_box_half;
    shell.prop_set_custom_collision_box_yaw=editor_shell_prop_set_custom_collision_box_yaw;
    shell.prop_set_pickable=editor_shell_prop_set_pickable;
    shell.prop_set_pickup_item=editor_shell_prop_set_pickup_item;
    shell.prop_set_pickup_count=editor_shell_prop_set_pickup_count;
    shell.prop_set_interact_type=editor_shell_prop_set_interact_type;
    shell.prop_set_interact_radius=editor_shell_prop_set_interact_radius;
    shell.prop_set_door_open_angle=editor_shell_prop_set_door_open_angle;
    shell.prop_add_action=editor_shell_prop_add_action;
    shell.prop_remove_action=editor_shell_prop_remove_action;
    shell.prop_select_action=editor_shell_prop_select_action;
    shell.prop_set_action_type=editor_shell_prop_set_action_type;
    shell.prop_set_action_radius=editor_shell_prop_set_action_radius;
    shell.prop_set_action_target_index=editor_shell_prop_set_action_target_index;
    shell.prop_set_action_sound_idx=editor_shell_prop_set_action_sound_idx;
    shell.prop_set_action_sound_volume=editor_shell_prop_set_action_sound_volume;
    shell.prop_set_action_sound_path=editor_shell_prop_set_action_sound_path;
    shell.prop_set_action_condition_type=editor_shell_prop_set_action_condition_type;
    shell.prop_set_action_condition_flag=editor_shell_prop_set_action_condition_flag;
    shell.prop_set_action_prompt_text=editor_shell_prop_set_action_prompt_text;
    shell.marker_set_kind=editor_shell_marker_set_kind;
    shell.marker_set_radius=editor_shell_marker_set_radius;
    shell.marker_set_y=editor_shell_marker_set_y;
    shell.spawn_set_model_idx=editor_shell_spawn_set_model_idx;
    shell.spawn_set_model_scale=editor_shell_spawn_set_model_scale;
    shell.spawn_set_hp_override=editor_shell_spawn_set_hp_override;
    shell.spawn_set_damage_override=editor_shell_spawn_set_damage_override;
    shell.spawn_set_speed_override=editor_shell_spawn_set_speed_override;
    shell.spawn_set_chase_radius_override=editor_shell_spawn_set_chase_radius_override;
    shell.spawn_set_attack_radius_override=editor_shell_spawn_set_attack_radius_override;
    shell.spawn_set_attack_cooldown_override=editor_shell_spawn_set_attack_cooldown_override;
    shell.spawn_set_scale_override=editor_shell_spawn_set_scale_override;
    shell.forest_set_tree_scale=editor_shell_set_tree_brush_scale;
    shell.forest_set_wall_spacing=editor_shell_set_wall_brush_spacing;
    shell.forest_set_wall_scale=editor_shell_set_wall_brush_scale;
    shell.river_set_width=editor_shell_river_set_width;
    shell.river_set_bank_softness=editor_shell_river_set_bank_softness;
    shell.river_set_selected_point_position=editor_shell_river_set_selected_point_position;
    shell.river_set_selected_point_width_scale=editor_shell_river_set_selected_point_width_scale;
    shell.river_set_selected_point_depth_scale=editor_shell_river_set_selected_point_depth_scale;
    shell.river_set_selected_point_flow_speed=editor_shell_river_set_selected_point_flow_speed;
    shell.river_set_selected_point_foam=editor_shell_river_set_selected_point_foam;
    shell.npc_set_place_mode=editor_shell_npc_set_place_mode;
    shell.npc_set_focus_model_index=editor_shell_npc_set_focus_model_index;
    shell.npc_set_focus_scale=editor_shell_npc_set_focus_scale;
    shell.npc_set_focus_visual_yaw_offset=editor_shell_npc_set_focus_visual_yaw_offset;
    shell.npc_set_focus_collision_enabled=editor_shell_npc_set_focus_collision_enabled;
    shell.npc_set_focus_patrol_loop=editor_shell_npc_set_focus_patrol_loop;
    shell.npc_set_focus_patrol_speed=editor_shell_npc_set_focus_patrol_speed;
    shell.npc_set_focus_patrol_wait=editor_shell_npc_set_focus_patrol_wait;
    shell.npc_set_focus_merchant_enabled=editor_shell_npc_set_focus_merchant_enabled;
    shell.npc_set_focus_shop_id=editor_shell_npc_set_focus_shop_id;
    shell.npc_set_dialogue_selection=editor_shell_npc_set_dialogue_selection;
    shell.npc_commit_dialogue_line=editor_shell_npc_commit_dialogue_line;
    shell.npc_delete_dialogue_line=editor_shell_npc_delete_dialogue_line;
    shell.npc_commit_focus_name=editor_shell_npc_commit_focus_name;
    shell.npc_commit_focus_backstory=editor_shell_npc_commit_focus_backstory;
    shell.selection_set=editor_shell_selection_set;
    editor_ui_draw_world_forge_shell(&g_editor_ui,&render_api,h,win_w,win_h,&shell);
}
