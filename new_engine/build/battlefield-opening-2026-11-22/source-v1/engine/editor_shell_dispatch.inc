static int editor_shell_active_prop_index(EditorShellActionContext *ctx){
    PropPlacer *placer;
    if(!ctx) return -1;
    if(ctx->selection&&ctx->selection->kind==EDITOR_SELECTION_PROP&&
       ctx->selection->index>=0&&ctx->selection->index<g_props.count&&
       g_props.list[ctx->selection->index].active)
        return ctx->selection->index;
    if(ctx->prop_edit&&*ctx->prop_edit&&ctx->prop_sel&&*ctx->prop_sel>=0&&*ctx->prop_sel<g_props.count) return *ctx->prop_sel;
    placer=(PropPlacer*)ctx->placer;
    if(ctx->prop_place_mode&&*ctx->prop_place_mode&&placer&&placer->selected_prop>=0&&placer->selected_prop<g_props.count) return placer->selected_prop;
    return -1;
}

static int editor_bulk_collect_props(EditorShellActionContext *ctx,int *out_indices,int cap){
    int count=0;
    if(!out_indices||cap<=0) return 0;
    if(g_prop_sel_count>0){
        for(int i=0;i<g_props.count&&count<cap;i++)
            if(g_prop_multi_sel[i]&&g_props.list[i].active) out_indices[count++]=i;
    }else{
        int idx=ctx?editor_shell_active_prop_index(ctx):g_placer.selected_prop;
        if(idx>=0&&idx<g_props.count&&g_props.list[idx].active) out_indices[count++]=idx;
    }
    return count;
}

static void editor_bulk_refresh_selection(EditorShellActionContext *ctx,int primary_idx){
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:&g_placer;
    if(primary_idx<0||primary_idx>=g_props.count||!g_props.list[primary_idx].active) return;
    if(ctx&&ctx->prop_sel) *ctx->prop_sel=primary_idx;
    if(placer){ placer->selected_prop=primary_idx; placer_sync_from_prop(placer,&g_props.list[primary_idx]); }
    editor_selection_set(EDITOR_SELECTION_PROP,primary_idx);
}

static void editor_bulk_align_axis(EditorShellActionContext *ctx,int axis){
    int ids[MAX_PROPS];
    int count=editor_bulk_collect_props(ctx,ids,MAX_PROPS);
    float avg=0.0f;
    if(count<=0) return;
    for(int i=0;i<count;i++){
        Vec3 p=g_props.list[ids[i]].pos;
        avg+=(axis==0)?p.x:((axis==1)?p.y:p.z);
    }
    avg/=(float)count;
    for(int i=0;i<count;i++){
        int idx=ids[i];
        float x=g_props.list[idx].pos.x;
        float y=g_props.list[idx].pos.y;
        float z=g_props.list[idx].pos.z;
        if(axis==0) x=avg;
        else if(axis==1) y=avg;
        else z=avg;
        if(axis!=1&&((ctx&&ctx->placer)?((PropPlacer*)ctx->placer)->snap_y:g_placer.snap_y))
            y=scene_ground_y(x,z);
        editor_command_prop_set_position(idx,x,y,z,ctx?ctx->placer:&g_placer,NULL);
    }
    editor_bulk_refresh_selection(ctx,ids[0]);
}

static void editor_bulk_distribute_axis(EditorShellActionContext *ctx,int axis){
    int ids[MAX_PROPS];
    float vals[MAX_PROPS];
    int count=editor_bulk_collect_props(ctx,ids,MAX_PROPS);
    if(count<3) return;
    for(int i=0;i<count;i++){
        Vec3 p=g_props.list[ids[i]].pos;
        vals[i]=(axis==0)?p.x:p.z;
    }
    for(int i=0;i<count-1;i++) for(int j=i+1;j<count;j++) if(vals[j]<vals[i]){
        float tv=vals[i]; vals[i]=vals[j]; vals[j]=tv;
        { int ti=ids[i]; ids[i]=ids[j]; ids[j]=ti; }
    }
    {
        float minv=vals[0],maxv=vals[count-1];
        float step=(maxv-minv)/(float)(count-1);
        for(int i=1;i<count-1;i++){
            int idx=ids[i];
            float x=g_props.list[idx].pos.x;
            float y=g_props.list[idx].pos.y;
            float z=g_props.list[idx].pos.z;
            if(axis==0) x=minv+step*(float)i;
            else z=minv+step*(float)i;
            if((ctx&&ctx->placer)?((PropPlacer*)ctx->placer)->snap_y:g_placer.snap_y)
                y=scene_ground_y(x,z);
            editor_command_prop_set_position(idx,x,y,z,ctx?ctx->placer:&g_placer,NULL);
        }
    }
    editor_bulk_refresh_selection(ctx,ids[0]);
}

static void editor_bulk_replace_selection_with_current(EditorShellActionContext *ctx){
    int ids[MAX_PROPS];
    int count=editor_bulk_collect_props(ctx,ids,MAX_PROPS);
    int replacement_primary=-1;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:&g_placer;
    if(count<=0||!placer) return;
    editor_commands_begin_transaction(editor_commands_runtime(),"bulk-replace",NULL);
    if(placer->prefab_mode){
        int prefab_idx=placer->prefab_sel;
        if(prefab_idx<0||prefab_idx>=g_prefabs.count){
            editor_commands_end_transaction(editor_commands_runtime(),NULL);
            return;
        }
        for(int i=count-1;i>=0;i--){
            int idx=ids[i];
            int new_idx=-1;
            Vec3 pos=g_props.list[idx].pos;
            editor_command_prop_delete(idx,NULL);
            if(editor_command_prefab_place_props(prefab_idx,pos,placer->place_ry,placer->place_scale,&new_idx,NULL) && i==0)
                replacement_primary=new_idx;
        }
        prefab_mark_recent(&g_prefabs,prefab_idx);
    }else{
        const char *path=placer_selected_model(placer);
        if(!path){
            editor_commands_end_transaction(editor_commands_runtime(),NULL);
            return;
        }
        for(int i=0;i<count;i++){
            int idx=ids[i];
            editor_command_prop_replace_model_in_place(idx,path,placer,NULL);
        }
        replacement_primary=ids[0];
        if(placer->palette_sel>=0&&placer->palette_sel<placer->filtered_count)
            placer_mark_recent(placer,placer->filtered[placer->palette_sel]);
    }
    editor_commands_end_transaction(editor_commands_runtime(),NULL);
    if(replacement_primary>=0) editor_bulk_refresh_selection(ctx,replacement_primary);
}

static void editor_bulk_scatter_current(EditorShellActionContext *ctx){
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:&g_placer;
    Vec3 center;
    if(!placer) return;
    center=(ctx&&ctx->cam)?ctx->cam->target:(g_placer.has_ghost?g_placer.ghost_pos:(Vec3){0,0,0});
    center.y=scene_ground_y(center.x,center.z);
    editor_commands_begin_transaction(editor_commands_runtime(),"bulk-scatter",NULL);
    for(int i=0;i<placer->bulk_scatter_count;i++){
        float ang=((float)rand()/(float)RAND_MAX)*(float)(2.0*M_PI);
        float rad=sqrtf((float)rand()/(float)RAND_MAX)*placer->bulk_scatter_radius;
        Vec3 pos={center.x+cosf(ang)*rad,0.0f,center.z+sinf(ang)*rad};
        pos.y=scene_ground_y(pos.x,pos.z);
        if(placer->prefab_mode){
            if(placer->prefab_sel>=0&&placer->prefab_sel<g_prefabs.count)
                editor_command_prefab_place_props(placer->prefab_sel,pos,
                             ((float)rand()/(float)RAND_MAX)*360.0f,placer->place_scale,NULL,NULL);
        }else{
            const char *path=placer_selected_model(placer);
            int idx=-1;
            if(path) editor_command_prop_create_model(path,pos,0,
                                   ((float)rand()/(float)RAND_MAX)*360.0f,0,placer->place_scale,
                                   (int)placer->place_coll,placer->snap_y,&idx,NULL);
        }
    }
    editor_commands_end_transaction(editor_commands_runtime(),NULL);
}

static void editor_bulk_stamp_path_current(EditorShellActionContext *ctx){
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:&g_placer;
    if(!placer||g_cpath.count<2||placer->bulk_path_spacing<=0.1f) return;
    editor_commands_begin_transaction(editor_commands_runtime(),"bulk-stamp-path",NULL);
    for(int seg=0;seg<g_cpath.count-1;seg++){
        float x0=g_cpath.pts[seg*2],z0=g_cpath.pts[seg*2+1];
        float x1=g_cpath.pts[(seg+1)*2],z1=g_cpath.pts[(seg+1)*2+1];
        float dx=x1-x0,dz=z1-z0;
        float len=sqrtf(dx*dx+dz*dz);
        int steps=(int)floorf(len/placer->bulk_path_spacing);
        float yaw=(float)(atan2f(dx,dz)*180.0f/M_PI);
        if(len<0.001f) continue;
        for(int s=0;s<=steps;s++){
            float t=(steps<=0)?0.0f:(float)s/(float)steps;
            Vec3 pos={x0+dx*t,0.0f,z0+dz*t};
            pos.y=scene_ground_y(pos.x,pos.z);
            if(placer->prefab_mode){
                if(placer->prefab_sel>=0&&placer->prefab_sel<g_prefabs.count)
                    editor_command_prefab_place_props(placer->prefab_sel,pos,yaw,placer->place_scale,NULL,NULL);
            }else{
                const char *path=placer_selected_model(placer);
                int idx=-1;
                if(path) editor_command_prop_create_model(path,pos,0,yaw,0,placer->place_scale,
                                                          (int)placer->place_coll,placer->snap_y,&idx,NULL);
            }
        }
    }
    editor_commands_end_transaction(editor_commands_runtime(),NULL);
}

static bool editor_selection_world_pos(const EditorSelectionState *sel,Vec3 *out_pos){
    if(!sel||!out_pos||sel->index<0) return false;
    switch(sel->kind){
        case EDITOR_SELECTION_PROP:
            if(sel->index<g_props.count&&g_props.list[sel->index].active){
                *out_pos=g_props.list[sel->index].pos;
                return true;
            }
            break;
        case EDITOR_SELECTION_SURFACE:
            if(surface_piece_get_pos(sel->index,out_pos)){
                return true;
            }
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            if(collision_volume_get_pos(sel->index,out_pos)){
                return true;
            }
            break;
        case EDITOR_SELECTION_MARKER:
            if(sel->index<g_world_markers.count&&g_world_markers.items[sel->index].active){
                *out_pos=g_world_markers.items[sel->index].pos;
                return true;
            }
            break;
        case EDITOR_SELECTION_SPAWN:
            if(sel->index<g_spawns.count&&g_spawns.points[sel->index].active){
                *out_pos=g_spawns.points[sel->index].pos;
                return true;
            }
            break;
        case EDITOR_SELECTION_NPC:
            if(sel->index<g_npc_spawns.count&&g_npc_spawns.points[sel->index].active){
                *out_pos=g_npc_spawns.points[sel->index].pos;
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

static void editor_shell_selection_clear(EditorShellActionContext *ctx){
    PropPlacer *placer;
    if(!ctx) return;
    placer=(PropPlacer*)ctx->placer;
    if(ctx->prop_sel) *ctx->prop_sel=-1;
    if(placer) placer->selected_prop=-1;
    if(ctx->marker_sel) *ctx->marker_sel=-1;
    if(ctx->spawn_sel_idx) *ctx->spawn_sel_idx=-1;
    if(ctx->npc_selected_npc) *ctx->npc_selected_npc=-1;
    if(ctx->npc_ed_wp_mode) *ctx->npc_ed_wp_mode=false;
    if(ctx->npc_ed_wp_npc) *ctx->npc_ed_wp_npc=-1;
    if(ctx->npc_ed_dlg_mode) *ctx->npc_ed_dlg_mode=false;
    if(ctx->npc_ed_dlg_npc) *ctx->npc_ed_dlg_npc=-1;
    if(ctx->npc_ed_dlg_sel) *ctx->npc_ed_dlg_sel=-1;
    if(ctx->npc_ed_dlg_buf) ctx->npc_ed_dlg_buf[0]=0;
    if(ctx->npc_ed_dlg_len) *ctx->npc_ed_dlg_len=0;
    if(ctx->npc_ed_tree_mode) *ctx->npc_ed_tree_mode=false;
    if(ctx->npc_ed_tree_npc) *ctx->npc_ed_tree_npc=-1;
    if(ctx->npc_ed_tree_sel) *ctx->npc_ed_tree_sel=0;
    if(ctx->npc_ed_tree_choice) *ctx->npc_ed_tree_choice=-1;
    SDL_StopTextInput();
    editor_selection_clear();
}

static void editor_shell_selection_frame(EditorShellActionContext *ctx){
    Vec3 pos;
    if(!ctx||!ctx->selection||!ctx->editor_cam_target||!ctx->cam) return;
    if(!editor_selection_world_pos(ctx->selection,&pos)) return;
    *ctx->editor_mode=true;
    *ctx->editor_cam_target=pos;
    ctx->cam->dist=clampf(ctx->cam->dist,12.0f,320.0f);
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

static void editor_shell_selection_nudge_y(EditorShellActionContext *ctx,float delta_y,bool snap_to_ground){
    int idx;
    if(!ctx||!ctx->selection||ctx->selection->index<0) return;
    idx=ctx->selection->index;
    switch(ctx->selection->kind){
        case EDITOR_SELECTION_PROP:
            if(idx<0||idx>=g_props.count||!g_props.list[idx].active) break;
            editor_command_prop_set_y(idx,
                snap_to_ground?scene_ground_y(g_props.list[idx].pos.x,g_props.list[idx].pos.z)
                              :(g_props.list[idx].pos.y+delta_y),
                ctx?ctx->placer:NULL,NULL);
            if(ctx->prop_sel) *ctx->prop_sel=idx;
            break;
        case EDITOR_SELECTION_SURFACE:
            if(!surface_piece_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            {
                Vec3 pos={0};
                float yaw_deg=0.0f,width=0.0f,height=0.0f,thickness=0.0f;
                int kind=0,material_id=0;
                float y;
                if(!surface_piece_describe(idx,&pos,&yaw_deg,&kind,&width,&height,&thickness,&material_id)) break;
                y=pos.y+delta_y;
                if(snap_to_ground){
                    float ground_y=scene_ground_y(pos.x,pos.z);
                    y=ground_y+((kind==1)?(thickness*0.5f):(height*0.5f));
                }
                pos.y=y;
                editor_command_surface_set_position(idx,pos,NULL);
            }
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            if(!collision_volume_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            {
                Vec3 pos={0};
                float height=0.0f;
                if(!collision_volume_describe(idx,&pos,NULL,NULL,&height,NULL,NULL,NULL)) break;
                pos.y=snap_to_ground?(scene_ground_y(pos.x,pos.z)+height*0.5f):(pos.y+delta_y);
                collision_volume_set_position(idx,pos);
            }
            break;
        case EDITOR_SELECTION_MARKER:
            if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) break;
            editor_command_marker_set_position(idx,(Vec3){
                g_world_markers.items[idx].pos.x,
                snap_to_ground?scene_ground_y(g_world_markers.items[idx].pos.x,g_world_markers.items[idx].pos.z)
                              :(g_world_markers.items[idx].pos.y+delta_y),
                g_world_markers.items[idx].pos.z
            },NULL);
            if(ctx->marker_sel) *ctx->marker_sel=idx;
            break;
        case EDITOR_SELECTION_SPAWN:
            if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) break;
            editor_command_spawn_set_position(idx,(Vec3){
                g_spawns.points[idx].pos.x,
                snap_to_ground?scene_ground_y(g_spawns.points[idx].pos.x,g_spawns.points[idx].pos.z)
                              :(g_spawns.points[idx].pos.y+delta_y),
                g_spawns.points[idx].pos.z
            },NULL);
            if(ctx->spawn_sel_idx) *ctx->spawn_sel_idx=idx;
            break;
        case EDITOR_SELECTION_NPC:
            if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) break;
            editor_command_npc_set_position(idx,(Vec3){
                g_npc_spawns.points[idx].pos.x,
                snap_to_ground?scene_ground_y(g_npc_spawns.points[idx].pos.x,g_npc_spawns.points[idx].pos.z)
                              :(g_npc_spawns.points[idx].pos.y+delta_y),
                g_npc_spawns.points[idx].pos.z
            },(NpcSystem*)ctx->npcs_runtime,NULL);
            if(ctx->npc_selected_npc) *ctx->npc_selected_npc=idx;
            break;
        default:
            break;
    }
}

static void editor_shell_selection_move_planar(EditorShellActionContext *ctx,Vec3 dir,float step){
    int idx;
    Vec3 delta;
    if(!ctx||!ctx->selection||ctx->selection->index<0) return;
    delta=vec3_scale(dir,step);
    idx=ctx->selection->index;
    switch(ctx->selection->kind){
        case EDITOR_SELECTION_PROP:
            if(idx<0||idx>=g_props.count||!g_props.list[idx].active) break;
            editor_command_prop_set_position(idx,
                g_props.list[idx].pos.x+delta.x,
                g_props.list[idx].pos.y,
                g_props.list[idx].pos.z+delta.z,
                ctx?ctx->placer:NULL,NULL);
            if(ctx->prop_sel) *ctx->prop_sel=idx;
            break;
        case EDITOR_SELECTION_SURFACE:
            if(!surface_piece_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            {
                Vec3 pos={0};
                if(!surface_piece_describe(idx,&pos,NULL,NULL,NULL,NULL,NULL,NULL)) break;
                pos.x+=delta.x;
                pos.z+=delta.z;
                editor_command_surface_set_position(idx,pos,NULL);
            }
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            if(!collision_volume_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            {
                Vec3 pos={0};
                if(!collision_volume_describe(idx,&pos,NULL,NULL,NULL,NULL,NULL,NULL)) break;
                pos.x+=delta.x;
                pos.z+=delta.z;
                collision_volume_set_position(idx,pos);
            }
            break;
        case EDITOR_SELECTION_MARKER:
            if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) break;
            editor_command_marker_set_position(idx,(Vec3){
                g_world_markers.items[idx].pos.x+delta.x,
                g_world_markers.items[idx].pos.y,
                g_world_markers.items[idx].pos.z+delta.z
            },NULL);
            if(ctx->marker_sel) *ctx->marker_sel=idx;
            break;
        case EDITOR_SELECTION_SPAWN:
            if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) break;
            editor_command_spawn_set_position(idx,(Vec3){
                g_spawns.points[idx].pos.x+delta.x,
                g_spawns.points[idx].pos.y,
                g_spawns.points[idx].pos.z+delta.z
            },NULL);
            if(ctx->spawn_sel_idx) *ctx->spawn_sel_idx=idx;
            break;
        case EDITOR_SELECTION_NPC:
            if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) break;
            editor_command_npc_set_position(idx,(Vec3){
                g_npc_spawns.points[idx].pos.x+delta.x,
                g_npc_spawns.points[idx].pos.y,
                g_npc_spawns.points[idx].pos.z+delta.z
            },(NpcSystem*)ctx->npcs_runtime,NULL);
            if(ctx->npc_selected_npc) *ctx->npc_selected_npc=idx;
            break;
        default:
            break;
    }
}

static void editor_shell_selection_adjust_yaw(EditorShellActionContext *ctx,float delta_deg){
    int idx;
    if(!ctx||!ctx->selection||ctx->selection->index<0) return;
    idx=ctx->selection->index;
    switch(ctx->selection->kind){
        case EDITOR_SELECTION_PROP:
            if(idx<0||idx>=g_props.count||!g_props.list[idx].active) break;
            editor_command_prop_set_rotation_y(idx,g_props.list[idx].rot_y+delta_deg,
                                               ctx?ctx->placer:NULL,NULL);
            if(ctx->prop_sel) *ctx->prop_sel=idx;
            break;
        case EDITOR_SELECTION_SURFACE:
            if(!surface_piece_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            {
                float yaw_deg=0.0f;
                if(!surface_piece_describe(idx,NULL,&yaw_deg,NULL,NULL,NULL,NULL,NULL)) break;
                editor_command_surface_set_yaw(idx,yaw_deg+delta_deg,NULL);
            }
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            if(!collision_volume_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            {
                float yaw_deg=0.0f;
                if(!collision_volume_describe(idx,NULL,&yaw_deg,NULL,NULL,NULL,NULL,NULL)) break;
                collision_volume_set_yaw_value(idx,yaw_deg+delta_deg);
            }
            break;
        case EDITOR_SELECTION_NPC:
            if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) break;
            editor_command_npc_set_yaw(idx,g_npc_spawns.points[idx].yaw+delta_deg*(float)M_PI/180.0f,
                                       (NpcSystem*)ctx->npcs_runtime,NULL);
            if(ctx->npc_selected_npc) *ctx->npc_selected_npc=idx;
            break;
        default:
            break;
    }
}

static void editor_shell_selection_adjust_scale(EditorShellActionContext *ctx,float delta){
    int idx;
    if(!ctx||!ctx->selection||ctx->selection->index<0) return;
    idx=ctx->selection->index;
    switch(ctx->selection->kind){
        case EDITOR_SELECTION_PROP:
            if(idx<0||idx>=g_props.count||!g_props.list[idx].active) break;
            editor_command_prop_set_scale(idx,g_props.list[idx].scale+delta,
                                          ctx?ctx->placer:NULL,NULL);
            if(ctx->prop_sel) *ctx->prop_sel=idx;
            break;
        case EDITOR_SELECTION_SURFACE:
            {
                float mul=1.0f+delta*0.5f;
                if(mul<0.5f) mul=0.5f;
                editor_command_surface_scale_uniform(idx,mul,NULL);
            }
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            {
                float mul=1.0f+delta*0.5f;
                if(mul<0.5f) mul=0.5f;
                collision_volume_scale_uniform(idx,mul);
            }
            break;
        case EDITOR_SELECTION_MARKER:
            if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) break;
            editor_command_marker_set_radius(idx,g_world_markers.items[idx].radius+delta*10.0f,NULL);
            if(ctx->marker_sel) *ctx->marker_sel=idx;
            break;
        case EDITOR_SELECTION_SPAWN:
            if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) break;
            {
                float cur=g_spawns.points[idx].scale_over>0.0f?g_spawns.points[idx].scale_over:1.0f;
                editor_command_spawn_set_scale_override(idx,cur+delta,NULL);
            }
            if(ctx->spawn_sel_idx) *ctx->spawn_sel_idx=idx;
            break;
        default:
            break;
    }
}

static void editor_shell_selection_snap_grid(EditorShellActionContext *ctx,float grid){
    int idx;
    if(!ctx||!ctx->selection||ctx->selection->index<0||grid<=0.0f) return;
    idx=ctx->selection->index;
    switch(ctx->selection->kind){
        case EDITOR_SELECTION_PROP:
            if(idx<0||idx>=g_props.count||!g_props.list[idx].active) break;
            editor_command_prop_set_position(idx,
                roundf(g_props.list[idx].pos.x/grid)*grid,
                g_props.list[idx].pos.y,
                roundf(g_props.list[idx].pos.z/grid)*grid,
                ctx?ctx->placer:NULL,NULL);
            if(ctx->prop_sel) *ctx->prop_sel=idx;
            break;
        case EDITOR_SELECTION_SURFACE:
            if(!surface_piece_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            {
                Vec3 pos={0};
                if(!surface_piece_describe(idx,&pos,NULL,NULL,NULL,NULL,NULL,NULL)) break;
                pos.x=roundf(pos.x/grid)*grid;
                pos.z=roundf(pos.z/grid)*grid;
                editor_command_surface_set_position(idx,pos,NULL);
            }
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            if(!collision_volume_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            {
                Vec3 pos={0};
                if(!collision_volume_describe(idx,&pos,NULL,NULL,NULL,NULL,NULL,NULL)) break;
                pos.x=roundf(pos.x/grid)*grid;
                pos.z=roundf(pos.z/grid)*grid;
                collision_volume_set_position(idx,pos);
            }
            break;
        case EDITOR_SELECTION_MARKER:
            if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) break;
            editor_command_marker_set_position(idx,(Vec3){
                roundf(g_world_markers.items[idx].pos.x/grid)*grid,
                g_world_markers.items[idx].pos.y,
                roundf(g_world_markers.items[idx].pos.z/grid)*grid
            },NULL);
            if(ctx->marker_sel) *ctx->marker_sel=idx;
            break;
        case EDITOR_SELECTION_SPAWN:
            if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) break;
            editor_command_spawn_set_position(idx,(Vec3){
                roundf(g_spawns.points[idx].pos.x/grid)*grid,
                g_spawns.points[idx].pos.y,
                roundf(g_spawns.points[idx].pos.z/grid)*grid
            },NULL);
            if(ctx->spawn_sel_idx) *ctx->spawn_sel_idx=idx;
            break;
        case EDITOR_SELECTION_NPC:
            if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) break;
            editor_command_npc_set_position(idx,(Vec3){
                roundf(g_npc_spawns.points[idx].pos.x/grid)*grid,
                g_npc_spawns.points[idx].pos.y,
                roundf(g_npc_spawns.points[idx].pos.z/grid)*grid
            },(NpcSystem*)ctx->npcs_runtime,NULL);
            if(ctx->npc_selected_npc) *ctx->npc_selected_npc=idx;
            break;
        default:
            break;
    }
}

static void editor_shell_selection_face_camera(EditorShellActionContext *ctx){
    int idx;
    Vec3 fwd;
    if(!ctx||!ctx->selection||ctx->selection->index<0||!ctx->cam) return;
    idx=ctx->selection->index;
    fwd=cam_forward(ctx->cam);
    switch(ctx->selection->kind){
        case EDITOR_SELECTION_PROP:
            if(idx<0||idx>=g_props.count||!g_props.list[idx].active) break;
            editor_command_prop_set_rotation_y(idx,atan2f(fwd.x,-fwd.z)*(180.0f/(float)M_PI),
                                               ctx?ctx->placer:NULL,NULL);
            if(ctx->prop_sel) *ctx->prop_sel=idx;
            break;
        case EDITOR_SELECTION_NPC:
            if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) break;
            editor_command_npc_set_yaw(idx,atan2f(fwd.x,-fwd.z),(NpcSystem*)ctx->npcs_runtime,NULL);
            if(ctx->npc_selected_npc) *ctx->npc_selected_npc=idx;
            break;
        default:
            break;
    }
}

static void editor_shell_selection_duplicate_offset(EditorShellActionContext *ctx,Vec3 offset,bool snap_to_ground,bool snap_to_grid){
    int idx,new_idx;
    if(!ctx||!ctx->selection||ctx->selection->index<0) return;
    idx=ctx->selection->index;
    new_idx=-1;
    switch(ctx->selection->kind){
        case EDITOR_SELECTION_PROP:
            if(idx<0||idx>=g_props.count||!g_props.list[idx].active) break;
            {
                Prop *src=&g_props.list[idx];
                float new_x=src->pos.x+offset.x;
                float new_z=src->pos.z+offset.z;
                float new_y=src->pos.y;
                PropPlacer *placer=(PropPlacer*)ctx->placer;
                if(snap_to_grid){
                    new_x=roundf(new_x/0.5f)*0.5f;
                    new_z=roundf(new_z/0.5f)*0.5f;
                }
                if(snap_to_ground) new_y=scene_ground_y(new_x,new_z);
                if(editor_command_prop_create_model(src->path,(Vec3){new_x,new_y,new_z},
                                             src->rot_x,src->rot_y,src->rot_z,src->scale,
                                             (int)src->coll_type,snap_to_ground,&new_idx,NULL)){
                    Prop *dst=&g_props.list[new_idx];
                    dst->pickable=src->pickable;
                    dst->pickup_item=src->pickup_item;
                    dst->pickup_count=src->pickup_count;
                    dst->custom_coll_enabled=src->custom_coll_enabled;
                    dst->custom_coll_count=src->custom_coll_count;
                    dst->custom_coll_selected=src->custom_coll_selected;
                    memcpy(dst->custom_coll,src->custom_coll,sizeof(src->custom_coll));
                    dst->interact_type=src->interact_type;
                    dst->interact_radius=src->interact_radius;
                    dst->door_open_angle=src->door_open_angle;
                    dst->action_count=src->action_count;
                    dst->action_selected=src->action_selected;
                    memcpy(dst->actions,src->actions,sizeof(src->actions));
                    dst->door_closed_yaw=dst->rot_y;
                    dst->door_is_open=false;
                    prop_refresh_collision(dst);
                    if(ctx->prop_sel) *ctx->prop_sel=new_idx;
                    if(placer) placer_sync_from_prop(placer,&g_props.list[new_idx]);
                    if(placer) placer->selected_prop=new_idx;
                    editor_selection_set(EDITOR_SELECTION_PROP,new_idx);
                }
            }
            break;
        case EDITOR_SELECTION_SURFACE:
            if(!surface_piece_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            {
                Vec3 new_pos={0};
                float yaw_deg=0.0f,width=0.0f,height=0.0f,thickness=0.0f;
                int kind=0,material_id=0;
                int coll_type=surface_piece_collision_type(idx);
                if(!surface_piece_describe(idx,&new_pos,&yaw_deg,&kind,&width,&height,&thickness,&material_id)) break;
                new_pos.x+=offset.x;
                new_pos.z+=offset.z;
                if(snap_to_grid){
                    new_pos.x=roundf(new_pos.x/0.5f)*0.5f;
                    new_pos.z=roundf(new_pos.z/0.5f)*0.5f;
                }
                if(snap_to_ground){
                    float ground_y=scene_ground_y(new_pos.x,new_pos.z);
                    new_pos.y=ground_y+((kind==1)?(thickness*0.5f):(height*0.5f));
                }
                if(editor_command_surface_create(kind,new_pos,yaw_deg,
                                                 width,height,thickness,
                                                 material_id,coll_type,
                                                 &new_idx,NULL))
                    editor_selection_set(EDITOR_SELECTION_SURFACE,new_idx);
            }
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            if(!collision_volume_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            new_idx=collision_volume_duplicate_idx(idx,offset,snap_to_ground,snap_to_grid);
            if(new_idx>=0) editor_selection_set(EDITOR_SELECTION_COLLISION_VOLUME,new_idx);
            break;
        case EDITOR_SELECTION_MARKER:
            if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active||g_world_markers.count>=MAX_WORLD_MARKERS) break;
            {
                WorldMarker *src=&g_world_markers.items[idx];
                Vec3 new_pos=src->pos;
                new_pos.x+=offset.x;
                new_pos.z+=offset.z;
                if(snap_to_grid){
                    new_pos.x=roundf(new_pos.x/0.5f)*0.5f;
                    new_pos.z=roundf(new_pos.z/0.5f)*0.5f;
                }
                if(snap_to_ground) new_pos.y=scene_ground_y(new_pos.x,new_pos.z);
                if(editor_command_marker_create(src->kind,new_pos,src->radius,&new_idx,NULL)){
                    if(ctx->marker_sel) *ctx->marker_sel=new_idx;
                    if(ctx->marker_sel_type) *ctx->marker_sel_type=(int)g_world_markers.items[new_idx].kind;
                    editor_selection_set(EDITOR_SELECTION_MARKER,new_idx);
                }
            }
            break;
        case EDITOR_SELECTION_SPAWN:
            if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active||g_spawns.count>=MAX_SPAWN_POINTS) break;
            {
                SpawnPoint *src=&g_spawns.points[idx];
                Vec3 new_pos=src->pos;
                new_pos.x+=offset.x;
                new_pos.z+=offset.z;
                if(snap_to_grid){
                    new_pos.x=roundf(new_pos.x/0.5f)*0.5f;
                    new_pos.z=roundf(new_pos.z/0.5f)*0.5f;
                }
                if(snap_to_ground) new_pos.y=scene_ground_y(new_pos.x,new_pos.z);
                if(editor_command_spawn_create((int)src->etype,new_pos,src->model_idx,
                                               src->hp_over,src->dmg_over,src->speed_over,
                                               src->chase_r_over,src->atk_r_over,src->atk_cd_over,src->scale_over,
                                               &new_idx,NULL)){
                    if(ctx->spawn_sel_idx) *ctx->spawn_sel_idx=new_idx;
                    if(ctx->spawn_sel_type) *ctx->spawn_sel_type=g_spawns.points[new_idx].etype;
                    if(ctx->spawn_sel_model) *ctx->spawn_sel_model=g_spawns.points[new_idx].model_idx;
                    editor_selection_set(EDITOR_SELECTION_SPAWN,new_idx);
                }
            }
            break;
        case EDITOR_SELECTION_NPC:
            if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active||g_npc_spawns.count>=MAX_NPC_SPAWNS) break;
            {
                NpcSystem *npcs=(NpcSystem*)ctx->npcs_runtime;
                NpcSpawnPoint src;
                Vec3 new_pos;
                if(npcs&&npcs->count>=NPC_MAX) break;
                src=g_npc_spawns.points[idx];
                new_pos=src.pos;
                new_pos.x+=offset.x;
                new_pos.z+=offset.z;
                if(snap_to_grid){
                    new_pos.x=roundf(new_pos.x/0.5f)*0.5f;
                    new_pos.z=roundf(new_pos.z/0.5f)*0.5f;
                }
                if(snap_to_ground) new_pos.y=scene_ground_y(new_pos.x,new_pos.z);
                src.pos=new_pos;
                src.active=true;
                if(editor_command_npc_create_from_spawn(&src,npcs,&new_idx,NULL)){
                    if(ctx->npc_selected_npc) *ctx->npc_selected_npc=new_idx;
                    if(ctx->npc_sel_type) *ctx->npc_sel_type=g_npc_spawns.points[new_idx].model_idx;
                    editor_selection_set(EDITOR_SELECTION_NPC,new_idx);
                }
            }
            break;
        default:
            break;
    }
}

static void editor_shell_selection_duplicate(EditorShellActionContext *ctx){
    Vec3 offset;
    offset=vec3_add(vec3_scale(cam_forward(ctx->cam),2.0f),vec3_scale(cam_right(ctx->cam),2.0f));
    editor_shell_selection_duplicate_offset(ctx,offset,false,false);
}

static void editor_shell_selection_cycle(EditorShellActionContext *ctx,int dir){
    int idx,start,count;
    PropPlacer *placer;
    if(!ctx||!ctx->selection||ctx->selection->index<0||dir==0) return;
    dir=(dir<0)?-1:1;
    idx=ctx->selection->index;
    placer=(PropPlacer*)ctx->placer;
    switch(ctx->selection->kind){
        case EDITOR_SELECTION_PROP:
            count=g_props.count;
            if(count<=0) break;
            start=idx;
            do{
                idx=(idx+dir+count)%count;
            }while(idx!=start&&(!g_props.list[idx].active));
            if(!g_props.list[idx].active) break;
            if(ctx->prop_sel) *ctx->prop_sel=idx;
            if(placer){
                placer->selected_prop=idx;
                placer_sync_from_prop(placer,&g_props.list[idx]);
            }
            editor_selection_set(EDITOR_SELECTION_PROP,idx);
            break;
        case EDITOR_SELECTION_SURFACE:
            count=surface_piece_total_count();
            if(count<=0) break;
            start=idx;
            do{
                idx=(idx+dir+count)%count;
            }while(idx!=start&&(!surface_piece_query(idx,NULL,NULL,NULL,NULL,NULL)));
            if(!surface_piece_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            editor_selection_set(EDITOR_SELECTION_SURFACE,idx);
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            count=collision_volume_total_count();
            if(count<=0) break;
            start=idx;
            do{
                idx=(idx+dir+count)%count;
            }while(idx!=start&&(!collision_volume_query(idx,NULL,NULL,NULL,NULL,NULL)));
            if(!collision_volume_query(idx,NULL,NULL,NULL,NULL,NULL)) break;
            editor_selection_set(EDITOR_SELECTION_COLLISION_VOLUME,idx);
            break;
        case EDITOR_SELECTION_MARKER:
            count=g_world_markers.count;
            if(count<=0) break;
            idx=(idx+dir+count)%count;
            if(!g_world_markers.items[idx].active) break;
            if(ctx->marker_sel) *ctx->marker_sel=idx;
            if(ctx->marker_sel_type) *ctx->marker_sel_type=(int)g_world_markers.items[idx].kind;
            editor_selection_set(EDITOR_SELECTION_MARKER,idx);
            break;
        case EDITOR_SELECTION_SPAWN:
            count=g_spawns.count;
            if(count<=0) break;
            idx=(idx+dir+count)%count;
            if(!g_spawns.points[idx].active) break;
            if(ctx->spawn_sel_idx) *ctx->spawn_sel_idx=idx;
            if(ctx->spawn_sel_type) *ctx->spawn_sel_type=g_spawns.points[idx].etype;
            if(ctx->spawn_sel_model) *ctx->spawn_sel_model=g_spawns.points[idx].model_idx;
            editor_selection_set(EDITOR_SELECTION_SPAWN,idx);
            break;
        case EDITOR_SELECTION_NPC:
            count=g_npc_spawns.count;
            if(count<=0) break;
            idx=(idx+dir+count)%count;
            if(!g_npc_spawns.points[idx].active) break;
            if(ctx->npc_selected_npc) *ctx->npc_selected_npc=idx;
            if(ctx->npc_sel_type) *ctx->npc_sel_type=g_npc_spawns.points[idx].model_idx;
            editor_selection_set(EDITOR_SELECTION_NPC,idx);
            break;
        default:
            break;
    }
}

static void editor_shell_selection_set(void *userdata,EditorSelectionKind kind,int index,bool frame){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer;
    if(!ctx||index<0) return;
    placer=(PropPlacer*)ctx->placer;
    switch(kind){
        case EDITOR_SELECTION_PROP:
            if(index>=g_props.count||!g_props.list[index].active) return;
            if(ctx->prop_sel) *ctx->prop_sel=index;
            if(placer){
                placer->selected_prop=index;
                placer_sync_from_prop(placer,&g_props.list[index]);
            }
            break;
        case EDITOR_SELECTION_SURFACE:
            if(!surface_piece_query(index,NULL,NULL,NULL,NULL,NULL)) return;
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            if(!collision_volume_query(index,NULL,NULL,NULL,NULL,NULL)) return;
            break;
        case EDITOR_SELECTION_MARKER:
            if(index>=g_world_markers.count||!g_world_markers.items[index].active) return;
            if(ctx->marker_sel) *ctx->marker_sel=index;
            if(ctx->marker_sel_type) *ctx->marker_sel_type=(int)g_world_markers.items[index].kind;
            break;
        case EDITOR_SELECTION_SPAWN:
            if(index>=g_spawns.count||!g_spawns.points[index].active) return;
            if(ctx->spawn_sel_idx) *ctx->spawn_sel_idx=index;
            if(ctx->spawn_sel_type) *ctx->spawn_sel_type=g_spawns.points[index].etype;
            if(ctx->spawn_sel_model) *ctx->spawn_sel_model=g_spawns.points[index].model_idx;
            break;
        case EDITOR_SELECTION_NPC:
            if(index>=g_npc_spawns.count||!g_npc_spawns.points[index].active) return;
            if(ctx->npc_selected_npc) *ctx->npc_selected_npc=index;
            if(ctx->npc_sel_type) *ctx->npc_sel_type=g_npc_spawns.points[index].model_idx;
            break;
        default:
            return;
    }
    editor_selection_set(kind,index);
    if(frame) editor_shell_selection_frame(ctx);
}

static void editor_shell_selection_delete(EditorShellActionContext *ctx){
    PropPlacer *placer;
    int idx;
    if(!ctx||!ctx->selection||ctx->selection->index<0) return;
    placer=(PropPlacer*)ctx->placer;
    idx=ctx->selection->index;
    switch(ctx->selection->kind){
        case EDITOR_SELECTION_PROP:
            if(idx<0||idx>=g_props.count||!g_props.list[idx].active) break;
            editor_command_prop_delete(idx,NULL);
            if(ctx->prop_sel&&*ctx->prop_sel==idx) *ctx->prop_sel=-1;
            if(placer&&placer->selected_prop==idx) placer->selected_prop=-1;
            editor_selection_clear();
            break;
        case EDITOR_SELECTION_SURFACE:
            if(!editor_command_surface_delete(idx,NULL)) break;
            editor_selection_clear();
            break;
        case EDITOR_SELECTION_COLLISION_VOLUME:
            if(!collision_volume_delete_idx(idx)) break;
            editor_selection_clear();
            break;
        case EDITOR_SELECTION_MARKER:
            if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) break;
            editor_command_marker_delete(idx,NULL);
            if(ctx->marker_sel){
                if(*ctx->marker_sel>idx) (*ctx->marker_sel)--;
                else if(*ctx->marker_sel==idx) *ctx->marker_sel=(idx<g_world_markers.count)?idx:g_world_markers.count-1;
            }
            break;
        case EDITOR_SELECTION_SPAWN:
            if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) break;
            editor_command_spawn_delete(idx,NULL);
            if(ctx->spawn_sel_idx){
                if(*ctx->spawn_sel_idx>idx) (*ctx->spawn_sel_idx)--;
                else if(*ctx->spawn_sel_idx==idx) *ctx->spawn_sel_idx=(idx<g_spawns.count)?idx:g_spawns.count-1;
            }
            break;
        case EDITOR_SELECTION_NPC:
            if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) break;
            editor_command_npc_delete(idx,(NpcSystem*)ctx->npcs_runtime,
                                      ctx->npc_selected_npc,
                                      ctx->npc_ed_wp_mode,ctx->npc_ed_wp_npc,
                                      ctx->npc_ed_dlg_mode,ctx->npc_ed_dlg_npc,ctx->npc_ed_dlg_sel,ctx->npc_ed_dlg_buf,ctx->npc_ed_dlg_len,
                                      ctx->npc_ed_tree_mode,ctx->npc_ed_tree_npc,ctx->npc_ed_tree_sel,ctx->npc_ed_tree_choice,
                                      NULL,NULL,NULL,NULL,NULL);
            break;
        default:
            break;
    }
}

static void editor_shell_prop_set_rotation_y(void *userdata,float yaw_deg){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_rotation_y(idx,yaw_deg,placer,&result);
}

static void editor_shell_prop_set_scale(void *userdata,float scale){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_scale(idx,scale,placer,&result);
}

static void editor_shell_prop_set_pickable(void *userdata,bool pickable){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_pickable(idx,pickable,&result);
}

static void editor_shell_prop_set_pickup_item(void *userdata,int item_type){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_pickup_item(idx,item_type,&result);
}

static void editor_shell_prop_set_pickup_count(void *userdata,int count){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_pickup_count(idx,count,&result);
}

static void editor_shell_prop_set_interact_type(void *userdata,int interact_type){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_interact_type(idx,interact_type,&result);
}

static void editor_shell_prop_set_interact_radius(void *userdata,float radius){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_interact_radius(idx,radius,&result);
}

static void editor_shell_prop_set_door_open_angle(void *userdata,float angle_deg){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_door_open_angle(idx,angle_deg,&result);
}

static void editor_shell_prop_add_action(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_add_action(idx,&result);
}

static void editor_shell_prop_remove_action(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_remove_action(idx,g_props.list[idx].action_selected,&result);
}

static void editor_shell_prop_select_action(void *userdata,int action_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    if(action_idx<0||action_idx>=g_props.list[idx].action_count) return;
    g_props.list[idx].action_selected=action_idx;
}

static void editor_shell_prop_set_action_type(void *userdata,int action_idx,int action_type){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_action_type(idx,action_idx,action_type,&result);
}

static void editor_shell_prop_set_action_radius(void *userdata,int action_idx,float radius){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_action_radius(idx,action_idx,radius,&result);
}

static void editor_shell_prop_set_action_target_index(void *userdata,int action_idx,int target_index){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_action_target_index(idx,action_idx,target_index,&result);
}

static void editor_shell_prop_set_action_sound_idx(void *userdata,int action_idx,int sound_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_action_sound_idx(idx,action_idx,sound_idx,&result);
}

static void editor_shell_prop_set_action_sound_volume(void *userdata,int action_idx,float volume){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_action_sound_volume(idx,action_idx,volume,&result);
}

static void editor_shell_prop_set_action_sound_path(void *userdata,int action_idx,const char *path){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_action_sound_path(idx,action_idx,path,&result);
}

static void editor_shell_prop_set_action_condition_type(void *userdata,int action_idx,int condition_type){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_action_condition_type(idx,action_idx,condition_type,&result);
}

static void editor_shell_prop_set_action_condition_flag(void *userdata,int action_idx,int condition_flag){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_action_condition_flag(idx,action_idx,condition_flag,&result);
}

static void editor_shell_prop_set_action_prompt_text(void *userdata,int action_idx,const char *text){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_action_prompt_text(idx,action_idx,text,&result);
}

static void editor_shell_prop_set_collision_mode(void *userdata,int coll_type){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    PropPlacer *placer=ctx?(PropPlacer*)ctx->placer:NULL;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_collision_mode(idx,coll_type,placer,&result);
}

static void editor_shell_prop_set_custom_collision_enabled(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_custom_collision_enabled(idx,enabled,&result);
}

static void editor_shell_prop_add_custom_collision_box(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_add_custom_collision_box(idx,&result);
}

static void editor_shell_prop_remove_custom_collision_box(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_remove_custom_collision_box(idx,g_props.list[idx].custom_coll_selected,&result);
}

static void editor_shell_prop_select_custom_collision_box(void *userdata,int box_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    if(box_idx<0||box_idx>=g_props.list[idx].custom_coll_count) return;
    g_props.list[idx].custom_coll_selected=box_idx;
}

static void editor_shell_prop_set_custom_collision_box_center(void *userdata,int box_idx,int axis,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_custom_collision_box_center(idx,box_idx,axis,value,&result);
}

static void editor_shell_prop_set_custom_collision_box_shape(void *userdata,int box_idx,int shape){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_custom_collision_box_shape(idx,box_idx,shape,&result);
}

static void editor_shell_prop_set_custom_collision_box_half(void *userdata,int box_idx,int axis,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_custom_collision_box_half(idx,box_idx,axis,value,&result);
}

static void editor_shell_prop_set_custom_collision_box_yaw(void *userdata,int box_idx,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_prop_index(ctx);
    EditorCommandResult result;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return;
    editor_command_prop_set_custom_collision_box_yaw(idx,box_idx,value,&result);
}

static int editor_shell_active_spawn_index(EditorShellActionContext *ctx){
    if(!ctx) return -1;
    if(ctx->selection&&ctx->selection->kind==EDITOR_SELECTION_SPAWN&&
       ctx->selection->index>=0&&ctx->selection->index<g_spawns.count&&
       g_spawns.points[ctx->selection->index].active) return ctx->selection->index;
    if(ctx->spawn_sel_idx&&*ctx->spawn_sel_idx>=0&&*ctx->spawn_sel_idx<g_spawns.count&&
       g_spawns.points[*ctx->spawn_sel_idx].active) return *ctx->spawn_sel_idx;
    return -1;
}

static int editor_shell_active_npc_index(EditorShellActionContext *ctx){
    if(!ctx) return -1;
    if(ctx->selection&&ctx->selection->kind==EDITOR_SELECTION_NPC&&
       ctx->selection->index>=0&&ctx->selection->index<g_npc_spawns.count&&
       g_npc_spawns.points[ctx->selection->index].active) return ctx->selection->index;
    if(ctx->npc_selected_npc&&*ctx->npc_selected_npc>=0&&*ctx->npc_selected_npc<g_npc_spawns.count&&
       g_npc_spawns.points[*ctx->npc_selected_npc].active) return *ctx->npc_selected_npc;
    return -1;
}

static void editor_shell_spawn_set_model_idx(void *userdata,int model_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_spawn_index(ctx);
    if(idx<0) return;
    editor_command_spawn_set_model_idx(idx,model_idx,NULL);
    if(ctx->spawn_sel_model) *ctx->spawn_sel_model=g_spawns.points[idx].model_idx;
}

static void editor_shell_spawn_set_model_scale(void *userdata,float scale){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_spawn_index(ctx);
    int model_slot;
    if(idx<0) return;
    model_slot=g_spawns.points[idx].model_idx-1;
    if(model_slot<0||model_slot>=g_enemy_model_count) return;
    g_enemy_custom_scale_mul[model_slot]=clampf(scale,0.01f,5.0f);
    enemy_model_scales_save();
}

static void editor_shell_spawn_set_hp_override(void *userdata,float hp_over){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_spawn_index(ctx);
    if(idx<0) return;
    editor_command_spawn_set_hp_override(idx,hp_over,NULL);
}

static void editor_shell_spawn_set_damage_override(void *userdata,float dmg_over){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_spawn_index(ctx);
    if(idx<0) return;
    editor_command_spawn_set_damage_override(idx,dmg_over,NULL);
}

static void editor_shell_spawn_set_speed_override(void *userdata,float speed_over){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_spawn_index(ctx);
    if(idx<0) return;
    editor_command_spawn_set_speed_override(idx,speed_over,NULL);
}

static void editor_shell_spawn_set_chase_radius_override(void *userdata,float chase_r_over){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_spawn_index(ctx);
    if(idx<0) return;
    editor_command_spawn_set_chase_radius_override(idx,chase_r_over,NULL);
}

static void editor_shell_spawn_set_attack_radius_override(void *userdata,float atk_r_over){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_spawn_index(ctx);
    if(idx<0) return;
    editor_command_spawn_set_attack_radius_override(idx,atk_r_over,NULL);
}

static void editor_shell_spawn_set_attack_cooldown_override(void *userdata,float atk_cd_over){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_spawn_index(ctx);
    if(idx<0) return;
    editor_command_spawn_set_attack_cooldown_override(idx,atk_cd_over,NULL);
}

static void editor_shell_spawn_set_scale_override(void *userdata,float scale_over){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx=editor_shell_active_spawn_index(ctx);
    if(idx<0) return;
    editor_command_spawn_set_scale_override(idx,scale_over,NULL);
}

static void editor_shell_set_wall_brush_spacing(void *userdata,float spacing){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->wall_brush_spacing) return;
    *ctx->wall_brush_spacing=clampf(spacing,1.0f,24.0f);
}

static void editor_shell_set_wall_brush_scale(void *userdata,float scale){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->wall_brush_scale) return;
    *ctx->wall_brush_scale=clampf(scale,0.25f,20.0f);
}

static void editor_shell_set_tree_brush_scale(void *userdata,float scale){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->tree_brush_scale) return;
    *ctx->tree_brush_scale=clampf(scale,0.25f,20.0f);
}

static void editor_shell_river_rebuild(EditorShellActionContext *ctx){
    if(!ctx||!ctx->terrain) return;
    g_criver.dirty=true;
    terrain_rebuild_all(ctx->terrain);
    terrain_rebake_hmap(ctx->terrain);
}

static void editor_shell_river_set_width(void *userdata,float width){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    g_criver.width=clampf(width,10.0f,400.0f);
    editor_shell_river_rebuild(ctx);
}

static void editor_shell_river_set_bank_softness(void *userdata,float softness){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    g_criver.bank_softness=clampf(softness,0.55f,2.50f);
    editor_shell_river_rebuild(ctx);
}

static void editor_shell_river_set_selected_point_position(void *userdata,int axis,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    if(!ctx||!ctx->river_sel_point) return;
    idx=*ctx->river_sel_point;
    if(idx<0||idx>=g_criver.count) return;
    if(axis==0) g_criver.pts[idx*2]=value;
    else if(axis==1) g_criver.pts[idx*2+1]=value;
    editor_shell_river_rebuild(ctx);
}

static void editor_shell_river_set_selected_point_width_scale(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    if(!ctx||!ctx->river_sel_point) return;
    idx=*ctx->river_sel_point;
    if(idx<0||idx>=g_criver.count) return;
    g_criver.width_scale[idx]=clampf(value,0.35f,2.50f);
    editor_shell_river_rebuild(ctx);
}

static void editor_shell_river_set_selected_point_depth_scale(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    if(!ctx||!ctx->river_sel_point) return;
    idx=*ctx->river_sel_point;
    if(idx<0||idx>=g_criver.count) return;
    g_criver.depth_scale[idx]=clampf(value,0.35f,2.50f);
    editor_shell_river_rebuild(ctx);
}

static void editor_shell_river_set_selected_point_flow_speed(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    if(!ctx||!ctx->river_sel_point) return;
    idx=*ctx->river_sel_point;
    if(idx<0||idx>=g_criver.count) return;
    g_criver.flow_speed[idx]=clampf(value,0.20f,3.00f);
    editor_shell_river_rebuild(ctx);
}

static void editor_shell_river_set_selected_point_foam(void *userdata,float value){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    if(!ctx||!ctx->river_sel_point) return;
    idx=*ctx->river_sel_point;
    if(idx<0||idx>=g_criver.count) return;
    g_criver.foam[idx]=clampf(value,0.0f,1.0f);
    editor_shell_river_rebuild(ctx);
}

static void editor_shell_npc_set_place_mode(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx||!ctx->editor) return;
    ctx->editor->npc_place_mode=enabled;
}

static void editor_shell_marker_set_kind(void *userdata,int kind){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    if(!ctx||!ctx->selection||kind<0||kind>=WORLD_MARKER_COUNT) return;
    idx=(ctx->selection->kind==EDITOR_SELECTION_MARKER)?ctx->selection->index:-1;
    if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) return;
    editor_command_marker_set_kind(idx,kind,NULL);
    if(ctx->marker_sel_type) *ctx->marker_sel_type=kind;
}

static void editor_shell_marker_set_radius(void *userdata,float radius){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    if(!ctx||!ctx->selection) return;
    idx=(ctx->selection->kind==EDITOR_SELECTION_MARKER)?ctx->selection->index:-1;
    if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) return;
    editor_command_marker_set_radius(idx,radius,NULL);
}

static void editor_shell_marker_set_y(void *userdata,float y){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    if(!ctx||!ctx->selection) return;
    idx=(ctx->selection->kind==EDITOR_SELECTION_MARKER)?ctx->selection->index:-1;
    if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) return;
    editor_command_marker_set_position(idx,(Vec3){
        g_world_markers.items[idx].pos.x,
        y,
        g_world_markers.items[idx].pos.z
    },NULL);
}

static void editor_shell_npc_set_focus_model_index(void *userdata,int model_idx){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx||model_idx<0||model_idx>=g_npc_model_count) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_model_idx(idx,model_idx,npcs,NULL);
    if(ctx->npc_sel_type) *ctx->npc_sel_type=g_npc_spawns.points[idx].model_idx;
}

static void editor_shell_npc_set_focus_scale(void *userdata,float scale){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_scale(idx,scale,npcs,NULL);
}

static void editor_shell_npc_set_focus_visual_yaw_offset(void *userdata,float yaw_offset){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_visual_yaw_offset(idx,yaw_offset,npcs,NULL);
}

static void editor_shell_npc_set_focus_collision_enabled(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_collision_enabled(idx,enabled,npcs,NULL);
}

static void editor_shell_npc_set_focus_patrol_loop(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_patrol_loop(idx,enabled,npcs,NULL);
}

static void editor_shell_npc_set_focus_patrol_speed(void *userdata,float speed){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_patrol_speed(idx,speed,npcs,NULL);
}

static void editor_shell_npc_set_focus_patrol_wait(void *userdata,float wait_time){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_patrol_wait(idx,wait_time,npcs,NULL);
}

static void editor_shell_npc_set_focus_merchant_enabled(void *userdata,bool enabled){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_merchant_enabled(idx,enabled,npcs,NULL);
}

static void editor_shell_npc_set_focus_shop_id(void *userdata,int shop_id){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_shop_id(idx,shop_id,npcs,NULL);
}

static void editor_shell_npc_set_dialogue_selection(void *userdata,int index){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    int idx;
    NpcSpawnPoint *sp;
    if(!ctx||!ctx->npc_ed_dlg_mode||!ctx->npc_ed_dlg_npc||!ctx->npc_ed_dlg_sel||
       !ctx->npc_ed_dlg_buf||!ctx->npc_ed_dlg_len) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return;
    sp=&g_npc_spawns.points[idx];
    *ctx->npc_ed_dlg_mode=true;
    *ctx->npc_ed_dlg_npc=idx;
    if(index<0||index>=sp->dialogue_count){
        *ctx->npc_ed_dlg_sel=-1;
        ctx->npc_ed_dlg_buf[0]=0;
        *ctx->npc_ed_dlg_len=0;
    }else{
        *ctx->npc_ed_dlg_sel=index;
        strncpy(ctx->npc_ed_dlg_buf,sp->dialogue[index],NPC_LINE_LEN-1);
        ctx->npc_ed_dlg_buf[NPC_LINE_LEN-1]=0;
        *ctx->npc_ed_dlg_len=(int)strlen(ctx->npc_ed_dlg_buf);
    }
    SDL_StartTextInput();
}

static void editor_shell_npc_commit_dialogue_line(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    NpcSpawnPoint *sp;
    NpcSpawnPoint next;
    if(!ctx||!ctx->npc_ed_dlg_mode||!ctx->npc_ed_dlg_npc||!ctx->npc_ed_dlg_sel||
       !ctx->npc_ed_dlg_buf||!ctx->npc_ed_dlg_len) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return;
    sp=&g_npc_spawns.points[idx];
    if(!ctx->npc_ed_dlg_buf[0]) return;
    next=*sp;
    *ctx->npc_ed_dlg_mode=true;
    *ctx->npc_ed_dlg_npc=idx;
    if(*ctx->npc_ed_dlg_sel>=0&&*ctx->npc_ed_dlg_sel<sp->dialogue_count){
        strncpy(next.dialogue[*ctx->npc_ed_dlg_sel],ctx->npc_ed_dlg_buf,NPC_LINE_LEN-1);
        next.dialogue[*ctx->npc_ed_dlg_sel][NPC_LINE_LEN-1]=0;
    }else if(sp->dialogue_count<NPC_LINES_MAX){
        strncpy(next.dialogue[sp->dialogue_count],ctx->npc_ed_dlg_buf,NPC_LINE_LEN-1);
        next.dialogue[sp->dialogue_count][NPC_LINE_LEN-1]=0;
        next.dialogue_count++;
        *ctx->npc_ed_dlg_sel=next.dialogue_count-1;
    }else{
        return;
    }
    editor_command_npc_patch(idx,&next,npcs,EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
}

static void editor_shell_npc_delete_dialogue_line(void *userdata){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    NpcSpawnPoint *sp;
    NpcSpawnPoint next;
    if(!ctx||!ctx->npc_ed_dlg_mode||!ctx->npc_ed_dlg_npc||!ctx->npc_ed_dlg_sel||
       !ctx->npc_ed_dlg_buf||!ctx->npc_ed_dlg_len) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return;
    sp=&g_npc_spawns.points[idx];
    if(*ctx->npc_ed_dlg_sel<0||*ctx->npc_ed_dlg_sel>=sp->dialogue_count) return;
    next=*sp;
    for(int di=*ctx->npc_ed_dlg_sel;di<sp->dialogue_count-1;di++)
        strncpy(next.dialogue[di],sp->dialogue[di+1],NPC_LINE_LEN);
    next.dialogue_count--;
    if(*ctx->npc_ed_dlg_sel>=next.dialogue_count) *ctx->npc_ed_dlg_sel=next.dialogue_count-1;
    if(*ctx->npc_ed_dlg_sel>=0){
        strncpy(ctx->npc_ed_dlg_buf,next.dialogue[*ctx->npc_ed_dlg_sel],NPC_LINE_LEN-1);
        ctx->npc_ed_dlg_buf[NPC_LINE_LEN-1]=0;
        *ctx->npc_ed_dlg_len=(int)strlen(ctx->npc_ed_dlg_buf);
    }else{
        ctx->npc_ed_dlg_buf[0]=0;
        *ctx->npc_ed_dlg_len=0;
    }
    editor_command_npc_patch(idx,&next,npcs,EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
}

static void editor_shell_npc_commit_focus_name(void *userdata,const char *name){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_name(idx,name,npcs,NULL);
}

static void editor_shell_npc_commit_focus_backstory(void *userdata,const char *backstory){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    NpcSystem *npcs=ctx?(NpcSystem*)ctx->npcs_runtime:NULL;
    int idx;
    if(!ctx) return;
    idx=editor_shell_active_npc_index(ctx);
    if(idx<0) return;
    editor_command_npc_set_backstory(idx,backstory,npcs,NULL);
}

static void editor_shell_tool_action(void *userdata,EditorWorldForgeAction action){
    EditorShellActionContext *ctx=(EditorShellActionContext*)userdata;
    if(!ctx) return;

    switch(action){
        case EDITOR_WF_ACTION_SELECTION_CLEAR:
            editor_shell_selection_clear(ctx);
            break;
        case EDITOR_WF_ACTION_SELECTION_FRAME:
            editor_shell_selection_frame(ctx);
            break;
        case EDITOR_WF_ACTION_SELECTION_DELETE:
            editor_shell_selection_delete(ctx);
            break;
        case EDITOR_WF_ACTION_SELECTION_Y_DEC:
            editor_shell_selection_nudge_y(ctx,-0.5f,false);
            break;
        case EDITOR_WF_ACTION_SELECTION_Y_INC:
            editor_shell_selection_nudge_y(ctx,0.5f,false);
            break;
        case EDITOR_WF_ACTION_SELECTION_GROUND:
            editor_shell_selection_nudge_y(ctx,0.0f,true);
            break;
        case EDITOR_WF_ACTION_SELECTION_MOVE_FWD:
            editor_shell_selection_move_planar(ctx,cam_forward(ctx->cam),0.5f);
            break;
        case EDITOR_WF_ACTION_SELECTION_MOVE_BACK:
            editor_shell_selection_move_planar(ctx,cam_forward(ctx->cam),-0.5f);
            break;
        case EDITOR_WF_ACTION_SELECTION_MOVE_LEFT:
            editor_shell_selection_move_planar(ctx,cam_right(ctx->cam),-0.5f);
            break;
        case EDITOR_WF_ACTION_SELECTION_MOVE_RIGHT:
            editor_shell_selection_move_planar(ctx,cam_right(ctx->cam),0.5f);
            break;
        case EDITOR_WF_ACTION_SELECTION_DUPLICATE:
            editor_shell_selection_duplicate(ctx);
            break;
        case EDITOR_WF_ACTION_SELECTION_DUPLICATE_FWD:
            editor_shell_selection_duplicate_offset(ctx,vec3_scale(cam_forward(ctx->cam),3.0f),false,true);
            break;
        case EDITOR_WF_ACTION_SELECTION_DUPLICATE_RIGHT:
            editor_shell_selection_duplicate_offset(ctx,vec3_scale(cam_right(ctx->cam),3.0f),false,true);
            break;
        case EDITOR_WF_ACTION_SELECTION_DUPLICATE_GROUNDED:
            editor_shell_selection_duplicate_offset(ctx,vec3_add(vec3_scale(cam_forward(ctx->cam),2.0f),vec3_scale(cam_right(ctx->cam),2.0f)),true,true);
            break;
        case EDITOR_WF_ACTION_SELECTION_PREV:
            editor_shell_selection_cycle(ctx,-1);
            break;
        case EDITOR_WF_ACTION_SELECTION_NEXT:
            editor_shell_selection_cycle(ctx,1);
            break;
        case EDITOR_WF_ACTION_SELECTION_YAW_DEC:
            editor_shell_selection_adjust_yaw(ctx,-15.0f);
            break;
        case EDITOR_WF_ACTION_SELECTION_YAW_INC:
            editor_shell_selection_adjust_yaw(ctx,15.0f);
            break;
        case EDITOR_WF_ACTION_SELECTION_SCALE_DEC:
            editor_shell_selection_adjust_scale(ctx,-0.10f);
            break;
        case EDITOR_WF_ACTION_SELECTION_SCALE_INC:
            editor_shell_selection_adjust_scale(ctx,0.10f);
            break;
        case EDITOR_WF_ACTION_SELECTION_SNAP_GRID:
            editor_shell_selection_snap_grid(ctx,0.5f);
            break;
        case EDITOR_WF_ACTION_SELECTION_FACE_CAMERA:
            editor_shell_selection_face_camera(ctx);
            break;
        case EDITOR_WF_ACTION_PATH_PREV:
            if(g_saved_cpath_count>0){
                cpath_store_active();
                {
                    int next=(g_cpath_slot>=0?g_cpath_slot:g_saved_cpath_count)-1;
                    if(next<0) next=g_saved_cpath_count-1;
                    cpath_select_slot(next);
                }
            }
            break;
        case EDITOR_WF_ACTION_PATH_NEXT:
            if(g_saved_cpath_count>0){
                cpath_store_active();
                {
                    int next=(g_cpath_slot>=0?g_cpath_slot:-1)+1;
                    if(next>=g_saved_cpath_count) next=0;
                    cpath_select_slot(next);
                }
            }
            break;
        case EDITOR_WF_ACTION_PATH_WIDTH_DEC:
            g_cpath.width=clampf(g_cpath.width-0.5f,0.5f,40.0f);
            if(g_cpath.count>=2) path_rebuild_mesh();
            break;
        case EDITOR_WF_ACTION_PATH_WIDTH_INC:
            g_cpath.width=clampf(g_cpath.width+0.5f,0.5f,40.0f);
            if(g_cpath.count>=2) path_rebuild_mesh();
            break;
        case EDITOR_WF_ACTION_PATH_STYLE_PREV:
            g_cpath.style=(g_cpath.style+CPATH_STYLE_COUNT-1)%CPATH_STYLE_COUNT;
            if(g_cpath.count>=2) path_rebuild_mesh();
            break;
        case EDITOR_WF_ACTION_PATH_STYLE_NEXT:
            g_cpath.style=(g_cpath.style+1)%CPATH_STYLE_COUNT;
            if(g_cpath.count>=2) path_rebuild_mesh();
            break;
        case EDITOR_WF_ACTION_PATH_NEW:
            cpath_store_active();
            cpath_select_slot(-1);
            break;
        case EDITOR_WF_ACTION_PATH_SAVE:
            cpath_save();
            break;
        case EDITOR_WF_ACTION_PATH_UNDO:
            if(g_cpath.count>0){
                g_cpath.count--;
                if(g_cpath.count>=2) path_rebuild_mesh();
                else g_pmesh.valid=false;
            }
            break;
        case EDITOR_WF_ACTION_PATH_CLEAR:
            g_cpath.count=0;
            g_pmesh.valid=false;
            break;

        case EDITOR_WF_ACTION_RIVER_WIDTH_DEC:
            g_criver.width=clampf(g_criver.width-5.0f,10.0f,400.0f);
            editor_shell_river_rebuild(ctx);
            break;
        case EDITOR_WF_ACTION_RIVER_WIDTH_INC:
            g_criver.width=clampf(g_criver.width+5.0f,10.0f,400.0f);
            editor_shell_river_rebuild(ctx);
            break;
        case EDITOR_WF_ACTION_RIVER_STYLE_PREV:
            wstyle_apply_preset((g_wstyle.preset_idx+WATER_PRESET_COUNT-1)%WATER_PRESET_COUNT);
            break;
        case EDITOR_WF_ACTION_RIVER_STYLE_NEXT:
            wstyle_apply_preset((g_wstyle.preset_idx+1)%WATER_PRESET_COUNT);
            break;
        case EDITOR_WF_ACTION_RIVER_UNDO:
            if(g_criver.count>0){
                criver_delete_point(g_criver.count-1);
                if(ctx->river_sel_point&&*ctx->river_sel_point>=g_criver.count)
                    *ctx->river_sel_point=g_criver.count-1;
                editor_shell_river_rebuild(ctx);
            }
            break;
        case EDITOR_WF_ACTION_RIVER_CLEAR:
            g_criver.count=0;
            if(ctx->river_sel_point) *ctx->river_sel_point=-1;
            editor_shell_river_rebuild(ctx);
            break;
        case EDITOR_WF_ACTION_RIVER_SAVE:
            criver_save();
            editor_shell_river_rebuild(ctx);
            break;

        case EDITOR_WF_ACTION_WIND_ANGLE_DEC:
            if(*ctx->wind_sel>=0&&*ctx->wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[*ctx->wind_sel]; z.dir_angle-=0.15f; windzone_update_with_undo(*ctx->wind_sel,&z); }
            break;
        case EDITOR_WF_ACTION_WIND_ANGLE_INC:
            if(*ctx->wind_sel>=0&&*ctx->wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[*ctx->wind_sel]; z.dir_angle+=0.15f; windzone_update_with_undo(*ctx->wind_sel,&z); }
            break;
        case EDITOR_WF_ACTION_WIND_RADIUS_DEC:
            if(*ctx->wind_sel>=0&&*ctx->wind_sel<g_wind_zone_count)
                { WindZone z=g_wind_zones[*ctx->wind_sel]; z.radius=clampf(z.radius-4.0f,8.0f,800.0f); windzone_update_with_undo(*ctx->wind_sel,&z); }
            break;
        case EDITOR_WF_ACTION_WIND_RADIUS_INC:
            if(*ctx->wind_sel>=0&&*ctx->wind_sel<g_wind_zone_count)
                { WindZone z=g_wind_zones[*ctx->wind_sel]; z.radius=clampf(z.radius+4.0f,8.0f,800.0f); windzone_update_with_undo(*ctx->wind_sel,&z); }
            break;
        case EDITOR_WF_ACTION_WIND_POWER_DEC:
            if(*ctx->wind_sel>=0&&*ctx->wind_sel<g_wind_zone_count)
                { WindZone z=g_wind_zones[*ctx->wind_sel]; z.strength=clampf(z.strength-0.1f,0.0f,5.0f); windzone_update_with_undo(*ctx->wind_sel,&z); }
            break;
        case EDITOR_WF_ACTION_WIND_POWER_INC:
            if(*ctx->wind_sel>=0&&*ctx->wind_sel<g_wind_zone_count)
                { WindZone z=g_wind_zones[*ctx->wind_sel]; z.strength=clampf(z.strength+0.1f,0.0f,5.0f); windzone_update_with_undo(*ctx->wind_sel,&z); }
            break;
        case EDITOR_WF_ACTION_WIND_GUST_DEC:
            if(*ctx->wind_sel>=0&&*ctx->wind_sel<g_wind_zone_count)
                { WindZone z=g_wind_zones[*ctx->wind_sel]; z.gust=clampf(z.gust-0.05f,0.0f,1.0f); windzone_update_with_undo(*ctx->wind_sel,&z); }
            break;
        case EDITOR_WF_ACTION_WIND_GUST_INC:
            if(*ctx->wind_sel>=0&&*ctx->wind_sel<g_wind_zone_count)
                { WindZone z=g_wind_zones[*ctx->wind_sel]; z.gust=clampf(z.gust+0.05f,0.0f,1.0f); windzone_update_with_undo(*ctx->wind_sel,&z); }
            break;
        case EDITOR_WF_ACTION_WIND_CLEAR:
            g_wind_zone_count=0;
            *ctx->wind_sel=-1;
            break;
        case EDITOR_WF_ACTION_WIND_SAVE:
            windzone_save();
            break;

        case EDITOR_WF_ACTION_MARKER_TYPE_PREV:
            *ctx->marker_sel_type=(*ctx->marker_sel_type+WORLD_MARKER_COUNT-1)%WORLD_MARKER_COUNT;
            break;
        case EDITOR_WF_ACTION_MARKER_TYPE_NEXT:
            *ctx->marker_sel_type=(*ctx->marker_sel_type+1)%WORLD_MARKER_COUNT;
            break;
        case EDITOR_WF_ACTION_MARKER_SAVE:
            world_markers_save((WorldMarkerList*)ctx->world_markers);
            break;
        case EDITOR_WF_ACTION_MARKER_RADIUS_DEC:
            if(*ctx->marker_sel>=0&&*ctx->marker_sel<g_world_markers.count&&g_world_markers.items[*ctx->marker_sel].active)
                editor_command_marker_set_radius(*ctx->marker_sel,g_world_markers.items[*ctx->marker_sel].radius-2.0f,NULL);
            break;
        case EDITOR_WF_ACTION_MARKER_RADIUS_INC:
            if(*ctx->marker_sel>=0&&*ctx->marker_sel<g_world_markers.count&&g_world_markers.items[*ctx->marker_sel].active)
                editor_command_marker_set_radius(*ctx->marker_sel,g_world_markers.items[*ctx->marker_sel].radius+2.0f,NULL);
            break;
        case EDITOR_WF_ACTION_MARKER_DELETE:
            if(*ctx->marker_sel>=0&&*ctx->marker_sel<g_world_markers.count){
                editor_command_marker_delete(*ctx->marker_sel,NULL);
                if(*ctx->marker_sel>=g_world_markers.count) *ctx->marker_sel=g_world_markers.count-1;
            }
            break;
        case EDITOR_WF_ACTION_MARKER_Y_DEC:
            if(*ctx->marker_sel>=0&&*ctx->marker_sel<g_world_markers.count&&g_world_markers.items[*ctx->marker_sel].active)
                editor_command_marker_set_position(*ctx->marker_sel,(Vec3){
                    g_world_markers.items[*ctx->marker_sel].pos.x,
                    g_world_markers.items[*ctx->marker_sel].pos.y-0.5f,
                    g_world_markers.items[*ctx->marker_sel].pos.z
                },NULL);
            break;
        case EDITOR_WF_ACTION_MARKER_Y_INC:
            if(*ctx->marker_sel>=0&&*ctx->marker_sel<g_world_markers.count&&g_world_markers.items[*ctx->marker_sel].active)
                editor_command_marker_set_position(*ctx->marker_sel,(Vec3){
                    g_world_markers.items[*ctx->marker_sel].pos.x,
                    g_world_markers.items[*ctx->marker_sel].pos.y+0.5f,
                    g_world_markers.items[*ctx->marker_sel].pos.z
                },NULL);
            break;

        case EDITOR_WF_ACTION_WZONE_RADIUS_DEC:
            if(*ctx->wzone_sel>=0&&*ctx->wzone_sel<g_wzone_count){
                WaterZone z=g_wzones[*ctx->wzone_sel];
                z.radius=clampf(z.radius-5.0f,10.0f,1000.0f);
                wzone_update_with_undo(*ctx->wzone_sel,&z,ctx->terrain);
            }
            break;
        case EDITOR_WF_ACTION_WZONE_RADIUS_INC:
            if(*ctx->wzone_sel>=0&&*ctx->wzone_sel<g_wzone_count){
                WaterZone z=g_wzones[*ctx->wzone_sel];
                z.radius=clampf(z.radius+5.0f,10.0f,1000.0f);
                wzone_update_with_undo(*ctx->wzone_sel,&z,ctx->terrain);
            }
            break;
        case EDITOR_WF_ACTION_WZONE_LEVEL_DEC:
            if(*ctx->wzone_sel>=0&&*ctx->wzone_sel<g_wzone_count){
                WaterZone z=g_wzones[*ctx->wzone_sel];
                z.level-=0.25f;
                wzone_update_with_undo(*ctx->wzone_sel,&z,ctx->terrain);
            }
            break;
        case EDITOR_WF_ACTION_WZONE_LEVEL_INC:
            if(*ctx->wzone_sel>=0&&*ctx->wzone_sel<g_wzone_count){
                WaterZone z=g_wzones[*ctx->wzone_sel];
                z.level+=0.25f;
                wzone_update_with_undo(*ctx->wzone_sel,&z,ctx->terrain);
            }
            break;
        case EDITOR_WF_ACTION_WZONE_PRESET_PREV:
            if(*ctx->wzone_sel>=0&&*ctx->wzone_sel<g_wzone_count)
                { WaterZone z=g_wzones[*ctx->wzone_sel]; z.preset_idx=(z.preset_idx+WATER_PRESET_COUNT-1)%WATER_PRESET_COUNT; wzone_update_with_undo(*ctx->wzone_sel,&z,ctx->terrain); }
            break;
        case EDITOR_WF_ACTION_WZONE_PRESET_NEXT:
            if(*ctx->wzone_sel>=0&&*ctx->wzone_sel<g_wzone_count)
                { WaterZone z=g_wzones[*ctx->wzone_sel]; z.preset_idx=(z.preset_idx+1)%WATER_PRESET_COUNT; wzone_update_with_undo(*ctx->wzone_sel,&z,ctx->terrain); }
            break;
        case EDITOR_WF_ACTION_WZONE_WATERFALLS:
            *ctx->wzone_edit=true;
            *ctx->wf_edit_mode=true;
            break;
        case EDITOR_WF_ACTION_WZONE_SAVE:
            wzone_save();
            terrain_rebuild_all(ctx->terrain);
            terrain_rebake_hmap(ctx->terrain);
            break;
        case EDITOR_WF_ACTION_WZONE_CLEAR:
            g_wzone_count=0;
            *ctx->wzone_sel=-1;
            terrain_rebuild_all(ctx->terrain);
            wzone_rebuild_vaos(ctx->terrain);
            break;

        case EDITOR_WF_ACTION_SPAWN_TYPE_PREV:
            *ctx->spawn_sel_type=(EnemyType)(((int)(*ctx->spawn_sel_type)+ETYPE_COUNT-1)%ETYPE_COUNT);
            break;
        case EDITOR_WF_ACTION_SPAWN_TYPE_NEXT:
            *ctx->spawn_sel_type=(EnemyType)(((int)(*ctx->spawn_sel_type)+1)%ETYPE_COUNT);
            break;
        case EDITOR_WF_ACTION_SPAWN_MODEL_PREV:
            if(g_enemy_model_count>0)*ctx->spawn_sel_model=(*ctx->spawn_sel_model+g_enemy_model_count-1)%g_enemy_model_count;
            break;
        case EDITOR_WF_ACTION_SPAWN_MODEL_NEXT:
            if(g_enemy_model_count>0)*ctx->spawn_sel_model=(*ctx->spawn_sel_model+1)%g_enemy_model_count;
            break;
        case EDITOR_WF_ACTION_SPAWN_SAVE:
            spawns_save((SpawnList*)ctx->spawns);
            enemy_model_scales_save();
            break;

        case EDITOR_WF_ACTION_WF_WIDTH_DEC:
            if(*ctx->wf_sel>=0&&*ctx->wf_sel<g_wfalls.count) g_wfalls.wf[*ctx->wf_sel].width=clampf(g_wfalls.wf[*ctx->wf_sel].width-0.5f,1.0f,80.0f);
            else *ctx->wf_place_width=clampf(*ctx->wf_place_width-0.5f,1.0f,80.0f);
            break;
        case EDITOR_WF_ACTION_WF_WIDTH_INC:
            if(*ctx->wf_sel>=0&&*ctx->wf_sel<g_wfalls.count) g_wfalls.wf[*ctx->wf_sel].width=clampf(g_wfalls.wf[*ctx->wf_sel].width+0.5f,1.0f,80.0f);
            else *ctx->wf_place_width=clampf(*ctx->wf_place_width+0.5f,1.0f,80.0f);
            break;
        case EDITOR_WF_ACTION_WF_HEIGHT_DEC:
            if(*ctx->wf_sel>=0&&*ctx->wf_sel<g_wfalls.count) g_wfalls.wf[*ctx->wf_sel].height=clampf(g_wfalls.wf[*ctx->wf_sel].height-1.0f,2.0f,200.0f);
            else *ctx->wf_place_height=clampf(*ctx->wf_place_height-1.0f,2.0f,200.0f);
            break;
        case EDITOR_WF_ACTION_WF_HEIGHT_INC:
            if(*ctx->wf_sel>=0&&*ctx->wf_sel<g_wfalls.count) g_wfalls.wf[*ctx->wf_sel].height=clampf(g_wfalls.wf[*ctx->wf_sel].height+1.0f,2.0f,200.0f);
            else *ctx->wf_place_height=clampf(*ctx->wf_place_height+1.0f,2.0f,200.0f);
            break;
        case EDITOR_WF_ACTION_WF_YAW_DEC:
            if(*ctx->wf_sel>=0&&*ctx->wf_sel<g_wfalls.count) g_wfalls.wf[*ctx->wf_sel].yaw-=0.1745f;
            else *ctx->wf_place_yaw-=0.1745f;
            break;
        case EDITOR_WF_ACTION_WF_YAW_INC:
            if(*ctx->wf_sel>=0&&*ctx->wf_sel<g_wfalls.count) g_wfalls.wf[*ctx->wf_sel].yaw+=0.1745f;
            else *ctx->wf_place_yaw+=0.1745f;
            break;
        case EDITOR_WF_ACTION_WF_BACK_ZONES:
            *ctx->wf_edit_mode=false;
            *ctx->wf_sel=-1;
            break;
        case EDITOR_WF_ACTION_WF_SAVE:
            wf_save();
            break;

        case EDITOR_WF_ACTION_SCULPT_MODE_PREV:
            *ctx->sculpt_mode=(*ctx->sculpt_mode+6)%7;
            break;
        case EDITOR_WF_ACTION_SCULPT_MODE_NEXT:
            *ctx->sculpt_mode=(*ctx->sculpt_mode+1)%7;
            break;
        case EDITOR_WF_ACTION_SCULPT_RADIUS_DEC:
            *ctx->sculpt_radius=clampf(*ctx->sculpt_radius-2.0f,3.0f,300.0f);
            break;
        case EDITOR_WF_ACTION_SCULPT_RADIUS_INC:
            *ctx->sculpt_radius=clampf(*ctx->sculpt_radius+2.0f,3.0f,300.0f);
            break;
        case EDITOR_WF_ACTION_SCULPT_POWER_DEC:
            *ctx->sculpt_strength=clampf(*ctx->sculpt_strength-0.005f,0.002f,0.5f);
            break;
        case EDITOR_WF_ACTION_SCULPT_POWER_INC:
            *ctx->sculpt_strength=clampf(*ctx->sculpt_strength+0.005f,0.002f,0.5f);
            break;
        case EDITOR_WF_ACTION_SCULPT_SAVE:
            if(*ctx->sculpt_mode==5) grass_mask_save();
            else if(*ctx->sculpt_mode==6){ worn_mask_save(); terrain_style_mask_save(); }
            else { sculpt_save(); terrain_rebake_hmap(ctx->terrain); }
            break;
        case EDITOR_WF_ACTION_SCULPT_UNDO:
            g_editor_ui_request_sculpt_undo=true;
            break;

        case EDITOR_WF_ACTION_FOREST_BRUSH_PREV:
            *ctx->forest_brush_kind=(*ctx->forest_brush_kind+6)%7;
            break;
        case EDITOR_WF_ACTION_FOREST_BRUSH_NEXT:
            *ctx->forest_brush_kind=(*ctx->forest_brush_kind+1)%7;
            break;
        case EDITOR_WF_ACTION_FOREST_SAVE_PRIMARY:
            if(*ctx->forest_brush_kind==0) forest_save((ForestSystem*)ctx->forest);
            else if(*ctx->forest_brush_kind==2) flower_mask_save();
            else if(*ctx->forest_brush_kind==3||*ctx->forest_brush_kind==4) placer_save(&g_props);
            else if(*ctx->forest_brush_kind==5){ worn_mask_save(); terrain_style_mask_save(); }
            else if(*ctx->forest_brush_kind==6) farm_field_mask_save();
            else grass_mask_save();
            break;
        case EDITOR_WF_ACTION_FOREST_RADIUS_DEC:
            *ctx->forest_brush_radius=clampf(*ctx->forest_brush_radius-2.0f,4.0f,220.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_RADIUS_INC:
            *ctx->forest_brush_radius=clampf(*ctx->forest_brush_radius+2.0f,4.0f,220.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_UNDO:
            g_editor_ui_request_forest_undo=(*ctx->forest_brush_kind==1||*ctx->forest_brush_kind==2||
                                             *ctx->forest_brush_kind==5||*ctx->forest_brush_kind==6);
            break;
        case EDITOR_WF_ACTION_FOREST_DENSITY_DEC:
            *ctx->forest_brush_density=*ctx->forest_brush_density>1?*ctx->forest_brush_density-1:1;
            break;
        case EDITOR_WF_ACTION_FOREST_DENSITY_INC:
            *ctx->forest_brush_density=*ctx->forest_brush_density<120?*ctx->forest_brush_density+1:120;
            break;
        case EDITOR_WF_ACTION_FOREST_MODEL_PREV:
            if(((ForestSystem*)ctx->forest)->fbx_path_count>0){
                ((ForestSystem*)ctx->forest)->active_model_idx--;
                if(((ForestSystem*)ctx->forest)->active_model_idx<-1) ((ForestSystem*)ctx->forest)->active_model_idx=((ForestSystem*)ctx->forest)->fbx_path_count-1;
            }
            break;
        case EDITOR_WF_ACTION_FOREST_MODEL_RANDOM:
            ((ForestSystem*)ctx->forest)->active_model_idx=-1;
            break;
        case EDITOR_WF_ACTION_FOREST_MODEL_NEXT:
            if(((ForestSystem*)ctx->forest)->fbx_path_count>0){
                ((ForestSystem*)ctx->forest)->active_model_idx++;
                if(((ForestSystem*)ctx->forest)->active_model_idx>=((ForestSystem*)ctx->forest)->fbx_path_count) ((ForestSystem*)ctx->forest)->active_model_idx=-1;
            }
            break;
        case EDITOR_WF_ACTION_FOREST_GRASS_SIZE_DEC:
            *ctx->grass_brush_size=clampf(*ctx->grass_brush_size-0.08f,0.35f,3.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_GRASS_SIZE_INC:
            *ctx->grass_brush_size=clampf(*ctx->grass_brush_size+0.08f,0.35f,3.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_GRASS_STYLE_NEXT:
            ((GrassSystem*)ctx->grass)->author_style=(GrassStyleId)((((GrassSystem*)ctx->grass)->author_style+1)%GRASS_STYLE_COUNT);
            break;
        case EDITOR_WF_ACTION_FOREST_GRASS_TINT_NEXT:
            ((GrassSystem*)ctx->grass)->author_tint=(GrassTintId)((((GrassSystem*)ctx->grass)->author_tint+1)%GRASS_TINT_COUNT);
            break;
        case EDITOR_WF_ACTION_FOREST_GRASS_HMIN_DEC:
            *ctx->grass_brush_hmin=clampf(*ctx->grass_brush_hmin-0.06f,0.20f,3.5f);
            if(*ctx->grass_brush_hmin>*ctx->grass_brush_hmax) *ctx->grass_brush_hmin=*ctx->grass_brush_hmax;
            break;
        case EDITOR_WF_ACTION_FOREST_GRASS_HMIN_INC:
            *ctx->grass_brush_hmin=clampf(*ctx->grass_brush_hmin+0.06f,0.20f,3.5f);
            if(*ctx->grass_brush_hmin>*ctx->grass_brush_hmax) *ctx->grass_brush_hmin=*ctx->grass_brush_hmax;
            break;
        case EDITOR_WF_ACTION_FOREST_GRASS_HMAX_DEC:
            *ctx->grass_brush_hmax=clampf(*ctx->grass_brush_hmax-0.08f,0.30f,4.0f);
            if(*ctx->grass_brush_hmax<*ctx->grass_brush_hmin) *ctx->grass_brush_hmax=*ctx->grass_brush_hmin;
            break;
        case EDITOR_WF_ACTION_FOREST_GRASS_HMAX_INC:
            *ctx->grass_brush_hmax=clampf(*ctx->grass_brush_hmax+0.08f,0.30f,4.0f);
            if(*ctx->grass_brush_hmax<*ctx->grass_brush_hmin) *ctx->grass_brush_hmax=*ctx->grass_brush_hmin;
            break;
        case EDITOR_WF_ACTION_FOREST_FLOWER_SIZE_DEC:
            *ctx->flower_brush_size=clampf(*ctx->flower_brush_size-0.08f,0.35f,3.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_FLOWER_SIZE_INC:
            *ctx->flower_brush_size=clampf(*ctx->flower_brush_size+0.08f,0.35f,3.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_FLOWER_TYPE_PREV:
            ((GrassSystem*)ctx->grass)->author_flower_type=(FlowerTypeId)((((GrassSystem*)ctx->grass)->author_flower_type+FLOWER_TYPE_COUNT-1)%FLOWER_TYPE_COUNT);
            break;
        case EDITOR_WF_ACTION_FOREST_FLOWER_TYPE_NEXT:
            ((GrassSystem*)ctx->grass)->author_flower_type=(FlowerTypeId)((((GrassSystem*)ctx->grass)->author_flower_type+1)%FLOWER_TYPE_COUNT);
            break;
        case EDITOR_WF_ACTION_FOREST_WALL_SPACING_DEC:
            *ctx->wall_brush_spacing=clampf(*ctx->wall_brush_spacing-0.25f,1.0f,24.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_WALL_SPACING_INC:
            *ctx->wall_brush_spacing=clampf(*ctx->wall_brush_spacing+0.25f,1.0f,24.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_WALL_SCALE_DEC:
            *ctx->wall_brush_scale=clampf(*ctx->wall_brush_scale-0.05f,0.25f,20.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_WALL_SCALE_INC:
            *ctx->wall_brush_scale=clampf(*ctx->wall_brush_scale+0.05f,0.25f,20.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_TERRAIN_POWER_DEC:
            *ctx->terrain_brush_strength=clampf(*ctx->terrain_brush_strength-0.04f,0.04f,1.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_TERRAIN_POWER_INC:
            *ctx->terrain_brush_strength=clampf(*ctx->terrain_brush_strength+0.04f,0.04f,1.0f);
            break;
        case EDITOR_WF_ACTION_FOREST_TERRAIN_STYLE_NEXT:
            *ctx->terrain_brush_style=(*ctx->terrain_brush_style+1)%TERRAIN_PAINT_COUNT;
            break;

        case EDITOR_WF_ACTION_NPC_TYPE_PREV:
            if(g_npc_model_count>0) *ctx->npc_sel_type=(*ctx->npc_sel_type+g_npc_model_count-1)%g_npc_model_count;
            break;
        case EDITOR_WF_ACTION_NPC_TYPE_NEXT:
            if(g_npc_model_count>0) *ctx->npc_sel_type=(*ctx->npc_sel_type+1)%g_npc_model_count;
            break;
        case EDITOR_WF_ACTION_NPC_SELECT_PREV:
        case EDITOR_WF_ACTION_NPC_SELECT_NEXT:
            if(ctx->npc_selected_npc&&g_npc_spawns.count>0){
                int idx=*ctx->npc_selected_npc;
                if(idx<0||idx>=g_npc_spawns.count) idx=(action==EDITOR_WF_ACTION_NPC_SELECT_PREV)?g_npc_spawns.count-1:0;
                else idx=(idx+(action==EDITOR_WF_ACTION_NPC_SELECT_PREV?(g_npc_spawns.count-1):1))%g_npc_spawns.count;
                *ctx->npc_selected_npc=idx;
                editor_selection_set(EDITOR_SELECTION_NPC,idx);
                if(ctx->npc_ed_wp_npc) *ctx->npc_ed_wp_npc=idx;
                if(ctx->npc_ed_dlg_npc) *ctx->npc_ed_dlg_npc=idx;
                if(ctx->npc_ed_tree_npc) *ctx->npc_ed_tree_npc=idx;
            }
            break;
        case EDITOR_WF_ACTION_NPC_SELECT_CLEAR:
            if(ctx->npc_selected_npc) *ctx->npc_selected_npc=-1;
            if(ctx->npc_ed_wp_mode) *ctx->npc_ed_wp_mode=false;
            if(ctx->npc_ed_wp_npc) *ctx->npc_ed_wp_npc=-1;
            if(ctx->npc_ed_dlg_mode) *ctx->npc_ed_dlg_mode=false;
            if(ctx->npc_ed_dlg_npc) *ctx->npc_ed_dlg_npc=-1;
            if(ctx->npc_ed_dlg_sel) *ctx->npc_ed_dlg_sel=-1;
            if(ctx->npc_ed_dlg_buf) ctx->npc_ed_dlg_buf[0]=0;
            if(ctx->npc_ed_dlg_len) *ctx->npc_ed_dlg_len=0;
            if(ctx->npc_ed_tree_mode) *ctx->npc_ed_tree_mode=false;
            if(ctx->npc_ed_tree_npc) *ctx->npc_ed_tree_npc=-1;
            if(ctx->npc_ed_tree_sel) *ctx->npc_ed_tree_sel=0;
            if(ctx->npc_ed_tree_choice) *ctx->npc_ed_tree_choice=-1;
            SDL_StopTextInput();
            editor_selection_clear();
            break;
        case EDITOR_WF_ACTION_NPC_DELETE:
            if(ctx->npc_selected_npc&&*ctx->npc_selected_npc>=0&&*ctx->npc_selected_npc<g_npc_spawns.count){
                int deleted_idx=*ctx->npc_selected_npc;
                editor_command_npc_delete(deleted_idx,(NpcSystem*)ctx->npcs_runtime,
                                          ctx->npc_selected_npc,
                                          ctx->npc_ed_wp_mode,ctx->npc_ed_wp_npc,
                                          ctx->npc_ed_dlg_mode,ctx->npc_ed_dlg_npc,ctx->npc_ed_dlg_sel,ctx->npc_ed_dlg_buf,ctx->npc_ed_dlg_len,
                                          ctx->npc_ed_tree_mode,ctx->npc_ed_tree_npc,ctx->npc_ed_tree_sel,ctx->npc_ed_tree_choice,
                                          NULL,NULL,NULL,NULL,NULL);
            }
            break;
        case EDITOR_WF_ACTION_NPC_SAVE:
            npc_spawns_save((NpcSpawnList*)ctx->npc_spawns);
            break;
        case EDITOR_WF_ACTION_NPC_YAW_DEC:
            *ctx->npc_ed_yaw-=0.15f;
            break;
        case EDITOR_WF_ACTION_NPC_YAW_INC:
            *ctx->npc_ed_yaw+=0.15f;
            break;
        case EDITOR_WF_ACTION_NPC_SPEED_DEC:
        case EDITOR_WF_ACTION_NPC_SPEED_INC:
        case EDITOR_WF_ACTION_NPC_WAIT_DEC:
        case EDITOR_WF_ACTION_NPC_WAIT_INC:
        case EDITOR_WF_ACTION_NPC_MERCHANT_TOGGLE:
        case EDITOR_WF_ACTION_NPC_SHOP_PREV:
        case EDITOR_WF_ACTION_NPC_SHOP_NEXT:
        case EDITOR_WF_ACTION_NPC_WAYPOINT_TOGGLE:
        case EDITOR_WF_ACTION_NPC_WAYPOINT_REMOVE_LAST:
        case EDITOR_WF_ACTION_NPC_DIALOGUE_OPEN:
        case EDITOR_WF_ACTION_NPC_DIALOGUE_CLOSE:
        case EDITOR_WF_ACTION_NPC_DIALOGUE_PREV:
        case EDITOR_WF_ACTION_NPC_DIALOGUE_NEXT:
        case EDITOR_WF_ACTION_NPC_DIALOGUE_COMMIT:
        case EDITOR_WF_ACTION_NPC_DIALOGUE_DELETE:
        case EDITOR_WF_ACTION_NPC_TREE_OPEN:
        case EDITOR_WF_ACTION_NPC_TREE_CLOSE:
        case EDITOR_WF_ACTION_NPC_TREE_PREV:
        case EDITOR_WF_ACTION_NPC_TREE_NEXT:
        case EDITOR_WF_ACTION_NPC_TREE_ADD:
        case EDITOR_WF_ACTION_NPC_TREE_DELETE:
        case EDITOR_WF_ACTION_NPC_TREE_ROOT:
        case EDITOR_WF_ACTION_NPC_TREE_CHOICE_PREV:
        case EDITOR_WF_ACTION_NPC_TREE_CHOICE_NEXT:
        case EDITOR_WF_ACTION_NPC_TREE_CHOICE_ADD:
        case EDITOR_WF_ACTION_NPC_TREE_CHOICE_DELETE:
            {
                int npc_idx=-1;
                if(ctx->selection&&ctx->selection->kind==EDITOR_SELECTION_NPC&&
                   ctx->selection->index>=0&&ctx->selection->index<g_npc_spawns.count&&
                   g_npc_spawns.points[ctx->selection->index].active)
                    npc_idx=ctx->selection->index;
                else if(ctx->npc_selected_npc&&*ctx->npc_selected_npc>=0) npc_idx=*ctx->npc_selected_npc;
                else if(ctx->editor_cam_target){
                    float best_d2=6.0f*6.0f;
                    for(int i=0;i<g_npc_spawns.count;i++){
                        NpcSpawnPoint *cand=&g_npc_spawns.points[i];
                        if(!cand->active) continue;
                        float dx=cand->pos.x-ctx->editor_cam_target->x;
                        float dz=cand->pos.z-ctx->editor_cam_target->z;
                        float d2=dx*dx+dz*dz;
                        if(d2<best_d2){ best_d2=d2; npc_idx=i; }
                    }
                }
                if(npc_idx<0||npc_idx>=g_npc_spawns.count) break;
                NpcSpawnPoint *sp=&g_npc_spawns.points[npc_idx];
                NpcSpawnPoint next=*sp;
                if(ctx->npc_selected_npc) *ctx->npc_selected_npc=npc_idx;
                editor_selection_set(EDITOR_SELECTION_NPC,npc_idx);
                switch(action){
                    case EDITOR_WF_ACTION_NPC_SPEED_DEC:{
                        float v=sp->patrol_speed>0?sp->patrol_speed:1.4f;
                        next.patrol_speed=clampf(v-0.2f,0.2f,6.0f);
                        editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                 EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        break;
                    }
                    case EDITOR_WF_ACTION_NPC_SPEED_INC:{
                        float v=sp->patrol_speed>0?sp->patrol_speed:1.4f;
                        next.patrol_speed=clampf(v+0.2f,0.2f,6.0f);
                        editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                 EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        break;
                    }
                    case EDITOR_WF_ACTION_NPC_WAIT_DEC:{
                        float v=sp->patrol_wait>0?sp->patrol_wait:2.0f;
                        next.patrol_wait=clampf(v-0.5f,0.0f,15.0f);
                        editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                 EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        break;
                    }
                    case EDITOR_WF_ACTION_NPC_WAIT_INC:{
                        float v=sp->patrol_wait>0?sp->patrol_wait:2.0f;
                        next.patrol_wait=clampf(v+0.5f,0.0f,15.0f);
                        editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                 EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        break;
                    }
                    case EDITOR_WF_ACTION_NPC_MERCHANT_TOGGLE:
                        next.is_merchant=!sp->is_merchant;
                        editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                 EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        break;
                    case EDITOR_WF_ACTION_NPC_SHOP_PREV:
                        if(sp->is_merchant){
                            next.shop_id=(sp->shop_id+SHOP_PROFILE_COUNT-1)%SHOP_PROFILE_COUNT;
                            editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                     EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_SHOP_NEXT:
                        if(sp->is_merchant){
                            next.shop_id=(sp->shop_id+1)%SHOP_PROFILE_COUNT;
                            editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                     EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_WAYPOINT_TOGGLE:
                        if(*ctx->npc_ed_wp_mode&&*ctx->npc_ed_wp_npc==npc_idx){
                            *ctx->npc_ed_wp_mode=false;
                            *ctx->npc_ed_wp_npc=-1;
                        }else{
                            *ctx->npc_ed_wp_mode=true;
                            *ctx->npc_ed_wp_npc=npc_idx;
                            *ctx->npc_ed_dlg_mode=false;
                            *ctx->npc_ed_tree_mode=false;
                            if(ctx->npc_ed_dlg_npc) *ctx->npc_ed_dlg_npc=-1;
                            if(ctx->npc_ed_tree_npc) *ctx->npc_ed_tree_npc=-1;
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_WAYPOINT_REMOVE_LAST:
                        if(*ctx->npc_ed_wp_mode&&*ctx->npc_ed_wp_npc==npc_idx&&sp->wp_count>0){
                            next.wp_count--;
                            editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                     EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_DIALOGUE_OPEN:
                        *ctx->npc_ed_wp_mode=false;
                        *ctx->npc_ed_tree_mode=false;
                        *ctx->npc_ed_dlg_mode=true;
                        *ctx->npc_ed_dlg_npc=npc_idx;
                        *ctx->npc_ed_dlg_sel=-1;
                        ctx->npc_ed_dlg_buf[0]=0;
                        *ctx->npc_ed_dlg_len=0;
                        SDL_StartTextInput();
                        break;
                    case EDITOR_WF_ACTION_NPC_DIALOGUE_CLOSE:
                        *ctx->npc_ed_dlg_mode=false;
                        *ctx->npc_ed_dlg_npc=-1;
                        *ctx->npc_ed_dlg_sel=-1;
                        ctx->npc_ed_dlg_buf[0]=0;
                        *ctx->npc_ed_dlg_len=0;
                        SDL_StopTextInput();
                        break;
                    case EDITOR_WF_ACTION_NPC_DIALOGUE_PREV:
                        if(!(*ctx->npc_ed_dlg_mode&&*ctx->npc_ed_dlg_npc==npc_idx)) break;
                        if(*ctx->npc_ed_dlg_sel==-1&&sp->dialogue_count>0) *ctx->npc_ed_dlg_sel=sp->dialogue_count-1;
                        else if(*ctx->npc_ed_dlg_sel>0) (*ctx->npc_ed_dlg_sel)--;
                        if(*ctx->npc_ed_dlg_sel>=0){
                            strncpy(ctx->npc_ed_dlg_buf,sp->dialogue[*ctx->npc_ed_dlg_sel],NPC_LINE_LEN-1);
                            ctx->npc_ed_dlg_buf[NPC_LINE_LEN-1]=0;
                            *ctx->npc_ed_dlg_len=(int)strlen(ctx->npc_ed_dlg_buf);
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_DIALOGUE_NEXT:
                        if(!(*ctx->npc_ed_dlg_mode&&*ctx->npc_ed_dlg_npc==npc_idx)) break;
                        if(*ctx->npc_ed_dlg_sel<sp->dialogue_count-1){
                            (*ctx->npc_ed_dlg_sel)++;
                            strncpy(ctx->npc_ed_dlg_buf,sp->dialogue[*ctx->npc_ed_dlg_sel],NPC_LINE_LEN-1);
                            ctx->npc_ed_dlg_buf[NPC_LINE_LEN-1]=0;
                            *ctx->npc_ed_dlg_len=(int)strlen(ctx->npc_ed_dlg_buf);
                        }else{
                            *ctx->npc_ed_dlg_sel=-1;
                            ctx->npc_ed_dlg_buf[0]=0;
                            *ctx->npc_ed_dlg_len=0;
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_DIALOGUE_COMMIT:
                        if(!(*ctx->npc_ed_dlg_mode&&*ctx->npc_ed_dlg_npc==npc_idx)) break;
                        if(ctx->npc_ed_dlg_buf[0]){
                            if(*ctx->npc_ed_dlg_sel>=0&&*ctx->npc_ed_dlg_sel<sp->dialogue_count){
                                strncpy(next.dialogue[*ctx->npc_ed_dlg_sel],ctx->npc_ed_dlg_buf,NPC_LINE_LEN-1);
                                next.dialogue[*ctx->npc_ed_dlg_sel][NPC_LINE_LEN-1]=0;
                            }else if(sp->dialogue_count<NPC_LINES_MAX){
                                strncpy(next.dialogue[sp->dialogue_count],ctx->npc_ed_dlg_buf,NPC_LINE_LEN-1);
                                next.dialogue[sp->dialogue_count][NPC_LINE_LEN-1]=0;
                                next.dialogue_count++;
                            }
                            *ctx->npc_ed_dlg_sel=-1;
                            ctx->npc_ed_dlg_buf[0]=0;
                            *ctx->npc_ed_dlg_len=0;
                            editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                     EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_DIALOGUE_DELETE:
                        if(!(*ctx->npc_ed_dlg_mode&&*ctx->npc_ed_dlg_npc==npc_idx)) break;
                        if(*ctx->npc_ed_dlg_sel>=0&&*ctx->npc_ed_dlg_sel<sp->dialogue_count){
                            for(int di=*ctx->npc_ed_dlg_sel;di<sp->dialogue_count-1;di++)
                                strncpy(next.dialogue[di],sp->dialogue[di+1],NPC_LINE_LEN);
                            next.dialogue_count--;
                            *ctx->npc_ed_dlg_sel=-1;
                            ctx->npc_ed_dlg_buf[0]=0;
                            *ctx->npc_ed_dlg_len=0;
                            editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                     EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_OPEN:
                        *ctx->npc_ed_wp_mode=false;
                        *ctx->npc_ed_dlg_mode=false;
                        *ctx->npc_ed_tree_mode=true;
                        *ctx->npc_ed_tree_npc=npc_idx;
                        *ctx->npc_ed_tree_sel=sp->tree_count>0?0:-1;
                        *ctx->npc_ed_tree_choice=-1;
                        SDL_StopTextInput();
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_CLOSE:
                        *ctx->npc_ed_tree_mode=false;
                        *ctx->npc_ed_tree_npc=-1;
                        *ctx->npc_ed_tree_sel=0;
                        *ctx->npc_ed_tree_choice=-1;
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_PREV:
                        if(*ctx->npc_ed_tree_mode&&*ctx->npc_ed_tree_npc==npc_idx&&*ctx->npc_ed_tree_sel>0){
                            (*ctx->npc_ed_tree_sel)--;
                            *ctx->npc_ed_tree_choice=-1;
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_NEXT:
                        if(*ctx->npc_ed_tree_mode&&*ctx->npc_ed_tree_npc==npc_idx&&sp->tree_count>0&&*ctx->npc_ed_tree_sel<sp->tree_count-1){
                            (*ctx->npc_ed_tree_sel)++;
                            *ctx->npc_ed_tree_choice=-1;
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_ADD:
                        if(sp->tree_count<NPC_TREE_MAX){
                            NpcTreeNode *newn=&next.tree[sp->tree_count];
                            next.tree_count++;
                            memset(newn,0,sizeof(*newn));
                            newn->anim_trigger=-1;
                            *ctx->npc_ed_tree_mode=true;
                            *ctx->npc_ed_tree_npc=npc_idx;
                            *ctx->npc_ed_tree_sel=next.tree_count-1;
                            *ctx->npc_ed_tree_choice=-1;
                            if(next.tree_count==1) next.tree_root=0;
                            editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                     EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_DELETE:
                        if(!(*ctx->npc_ed_tree_mode&&*ctx->npc_ed_tree_npc==npc_idx)) break;
                        if(*ctx->npc_ed_tree_choice>=0&&*ctx->npc_ed_tree_sel>=0&&*ctx->npc_ed_tree_sel<sp->tree_count){
                            NpcTreeNode *tn=&next.tree[*ctx->npc_ed_tree_sel];
                            int choice=*ctx->npc_ed_tree_choice;
                            if(choice<tn->num_choices){
                                for(int ci=choice;ci<tn->num_choices-1;ci++){
                                    tn->choice_next[ci]=tn->choice_next[ci+1];
                                    strncpy(tn->choice_label[ci],tn->choice_label[ci+1],64);
                                }
                                tn->num_choices--;
                                if(*ctx->npc_ed_tree_choice>=tn->num_choices) *ctx->npc_ed_tree_choice=tn->num_choices-1;
                                editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                         EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                            }
                        }else if(*ctx->npc_ed_tree_sel>=0&&*ctx->npc_ed_tree_sel<sp->tree_count){
                            int del=*ctx->npc_ed_tree_sel;
                            for(int ni=del;ni<sp->tree_count-1;ni++) next.tree[ni]=sp->tree[ni+1];
                            for(int ni=del;ni<sp->tree_count-1;ni++){
                                next.node_canvas_x[ni]=sp->node_canvas_x[ni+1];
                                next.node_canvas_y[ni]=sp->node_canvas_y[ni+1];
                            }
                            next.tree_count--;
                            for(int ni=0;ni<next.tree_count;ni++){
                                NpcTreeNode *tn=&next.tree[ni];
                                for(int ci=0;ci<tn->num_choices;ci++){
                                    if(tn->choice_next[ci]==del) tn->choice_next[ci]=-1;
                                    else if(tn->choice_next[ci]>del) tn->choice_next[ci]--;
                                }
                            }
                            if(next.tree_root==del) next.tree_root=(next.tree_count>0)?0:-1;
                            else if(next.tree_root>del) next.tree_root--;
                            if(*ctx->npc_ed_tree_sel>=next.tree_count) *ctx->npc_ed_tree_sel=next.tree_count-1;
                            *ctx->npc_ed_tree_choice=-1;
                            editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                     EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_ROOT:
                        if(*ctx->npc_ed_tree_mode&&*ctx->npc_ed_tree_npc==npc_idx&&*ctx->npc_ed_tree_sel>=0&&*ctx->npc_ed_tree_sel<sp->tree_count){
                            next.tree_root=*ctx->npc_ed_tree_sel;
                            editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                     EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_CHOICE_PREV:
                        if(*ctx->npc_ed_tree_mode&&*ctx->npc_ed_tree_npc==npc_idx&&*ctx->npc_ed_tree_sel>=0&&*ctx->npc_ed_tree_sel<sp->tree_count){
                            NpcTreeNode *tn=&sp->tree[*ctx->npc_ed_tree_sel];
                            if(tn->num_choices>0){
                                if(*ctx->npc_ed_tree_choice<0) *ctx->npc_ed_tree_choice=tn->num_choices-1;
                                else *ctx->npc_ed_tree_choice=(*ctx->npc_ed_tree_choice+tn->num_choices-1)%tn->num_choices;
                            }
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_CHOICE_NEXT:
                        if(*ctx->npc_ed_tree_mode&&*ctx->npc_ed_tree_npc==npc_idx&&*ctx->npc_ed_tree_sel>=0&&*ctx->npc_ed_tree_sel<sp->tree_count){
                            NpcTreeNode *tn=&sp->tree[*ctx->npc_ed_tree_sel];
                            if(tn->num_choices>0){
                                if(*ctx->npc_ed_tree_choice<0) *ctx->npc_ed_tree_choice=0;
                                else *ctx->npc_ed_tree_choice=(*ctx->npc_ed_tree_choice+1)%tn->num_choices;
                            }
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_CHOICE_ADD:
                        if(*ctx->npc_ed_tree_mode&&*ctx->npc_ed_tree_npc==npc_idx&&*ctx->npc_ed_tree_sel>=0&&*ctx->npc_ed_tree_sel<sp->tree_count){
                            NpcTreeNode *tn=&next.tree[*ctx->npc_ed_tree_sel];
                            if(tn->num_choices<4){
                                int ci=tn->num_choices++;
                                tn->choice_label[ci][0]=0;
                                tn->choice_next[ci]=-1;
                                *ctx->npc_ed_tree_choice=ci;
                                editor_command_npc_patch(npc_idx,&next,(NpcSystem*)ctx->npcs_runtime,
                                                         EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                            }
                        }
                        break;
                    case EDITOR_WF_ACTION_NPC_TREE_CHOICE_DELETE:
                        if(*ctx->npc_ed_tree_choice>=0){
                            editor_shell_tool_action(userdata,EDITOR_WF_ACTION_NPC_TREE_DELETE);
                        }
                        break;
                    default:
                        break;
                }
            }
            break;

        case EDITOR_WF_ACTION_FX_TAB_WORLD:
            *ctx->fx_editor_tab=0;
            break;
        case EDITOR_WF_ACTION_FX_TAB_WEATHER:
            *ctx->fx_editor_tab=1;
            break;
        case EDITOR_WF_ACTION_FX_TAB_POST:
            *ctx->fx_editor_tab=2;
            break;
        case EDITOR_WF_ACTION_FX_PRESET_PREV:
            {
                int next=(int)g_world_fx_editor.kind-1;
                while(next<0) next+=WORLD_FX_COUNT;
                g_world_fx_editor.kind=(WorldFxKind)(next%WORLD_FX_COUNT);
                if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            }
            break;
        case EDITOR_WF_ACTION_FX_PRESET_NEXT:
            g_world_fx_editor.kind=(WorldFxKind)(((int)g_world_fx_editor.kind+1)%WORLD_FX_COUNT);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_SAVE:
            world_fx_save((WorldFxSystem*)ctx->world_fx);
            if(ctx->wetness&&ctx->fog_density&&ctx->overcast)
                visual_profile_capture_weather(*ctx->wetness,*ctx->fog_density,*ctx->overcast);
            visual_profile_save();
            break;
        case EDITOR_WF_ACTION_FX_SCALE_DEC:
            g_world_fx_editor.scale=clampf(g_world_fx_editor.scale-0.05f,0.05f,12.0f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_SCALE_INC:
            g_world_fx_editor.scale=clampf(g_world_fx_editor.scale+0.05f,0.05f,12.0f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_LIGHT_TOGGLE:
            g_world_fx_editor.light_enabled=!g_world_fx_editor.light_enabled;
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_SHADER_TOGGLE:
            if(g_shader_lab.prog) g_shader_lab.enabled=!g_shader_lab.enabled;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_BLOOM_TOGGLE:
            g_options.bloom=!g_options.bloom;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_HEIGHT_DEC:
            g_world_fx_editor.height=clampf(g_world_fx_editor.height-0.10f,0.0f,20.0f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_HEIGHT_INC:
            g_world_fx_editor.height=clampf(g_world_fx_editor.height+0.10f,0.0f,20.0f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_GLOW_DEC:
            g_world_fx_editor.intensity=clampf(g_world_fx_editor.intensity-0.10f,0.10f,4.0f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_GLOW_INC:
            g_world_fx_editor.intensity=clampf(g_world_fx_editor.intensity+0.10f,0.10f,4.0f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_LIGHT_RADIUS_DEC:
            g_world_fx_editor.light_radius=clampf(g_world_fx_editor.light_radius-0.5f,0.0f,40.0f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_LIGHT_RADIUS_INC:
            g_world_fx_editor.light_radius=clampf(g_world_fx_editor.light_radius+0.5f,0.0f,40.0f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_COLOR_R_DEC:
            g_world_fx_editor.color.x=clampf(g_world_fx_editor.color.x-0.05f,0.0f,1.5f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_COLOR_R_INC:
            g_world_fx_editor.color.x=clampf(g_world_fx_editor.color.x+0.05f,0.0f,1.5f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_COLOR_G_DEC:
            g_world_fx_editor.color.y=clampf(g_world_fx_editor.color.y-0.05f,0.0f,1.5f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_COLOR_G_INC:
            g_world_fx_editor.color.y=clampf(g_world_fx_editor.color.y+0.05f,0.0f,1.5f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_COLOR_B_DEC:
            g_world_fx_editor.color.z=clampf(g_world_fx_editor.color.z-0.05f,0.0f,1.5f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_COLOR_B_INC:
            g_world_fx_editor.color.z=clampf(g_world_fx_editor.color.z+0.05f,0.0f,1.5f);
            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
            break;
        case EDITOR_WF_ACTION_FX_MOVE_FWD:
        case EDITOR_WF_ACTION_FX_MOVE_BACK:
        case EDITOR_WF_ACTION_FX_MOVE_LEFT:
        case EDITOR_WF_ACTION_FX_MOVE_RIGHT:
        case EDITOR_WF_ACTION_FX_MOVE_UP:
        case EDITOR_WF_ACTION_FX_MOVE_DOWN:
            if(g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active){
                if(action==EDITOR_WF_ACTION_FX_MOVE_UP) world_fx_move_selected_with_undo(g_undo_ls,(Vec3){0.0f,0.5f,0.0f});
                else if(action==EDITOR_WF_ACTION_FX_MOVE_DOWN) world_fx_move_selected_with_undo(g_undo_ls,(Vec3){0.0f,-0.5f,0.0f});
                else if(action==EDITOR_WF_ACTION_FX_MOVE_FWD||action==EDITOR_WF_ACTION_FX_MOVE_BACK){
                    Vec3 fw=cam_forward(ctx->cam);
                    float sign=action==EDITOR_WF_ACTION_FX_MOVE_FWD?1.0f:-1.0f;
                    world_fx_move_selected_with_undo(g_undo_ls,(Vec3){fw.x*0.5f*sign,0.0f,fw.z*0.5f*sign});
                }else{
                    Vec3 ri=cam_right(ctx->cam);
                    float sign=action==EDITOR_WF_ACTION_FX_MOVE_RIGHT?1.0f:-1.0f;
                    world_fx_move_selected_with_undo(g_undo_ls,(Vec3){ri.x*0.5f*sign,0.0f,ri.z*0.5f*sign});
                }
            }
            break;
        case EDITOR_WF_ACTION_FX_WEATHER_AUTO:
            g_weather_editor_override=-1;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_WEATHER_RAIN:
            g_weather_editor_override=1;
            ctx->weather->enabled=true;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_WEATHER_SNOW:
            g_weather_editor_override=2;
            ctx->weather->enabled=true;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_WEATHER_OFF:
            g_weather_editor_override=0;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_WEATHER_TOGGLE:
            ctx->weather->enabled=!ctx->weather->enabled;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_RATE_DEC:
            g_weather_editor_spawn_mul=clampf(g_weather_editor_spawn_mul-0.1f,0.1f,4.0f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_RATE_INC:
            g_weather_editor_spawn_mul=clampf(g_weather_editor_spawn_mul+0.1f,0.1f,4.0f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_WET_DEC:
            *ctx->wetness=clampf(*ctx->wetness-0.05f,0.0f,1.0f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_WET_INC:
            *ctx->wetness=clampf(*ctx->wetness+0.05f,0.0f,1.0f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_FOG_DEC:
            *ctx->fog_density=clampf(*ctx->fog_density-0.0002f,0.0f,0.05f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_FOG_INC:
            *ctx->fog_density=clampf(*ctx->fog_density+0.0002f,0.0f,0.05f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_CLOUD_DEC:
            *ctx->overcast=clampf(*ctx->overcast-0.05f,0.0f,1.0f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_CLOUD_INC:
            *ctx->overcast=clampf(*ctx->overcast+0.05f,0.0f,1.0f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_EXPOSURE_DEC:
            g_postfx_exposure_mul=clampf(g_postfx_exposure_mul-0.05f,0.25f,3.0f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_EXPOSURE_INC:
            g_postfx_exposure_mul=clampf(g_postfx_exposure_mul+0.05f,0.25f,3.0f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_POST_BLOOM_DEC:
            g_postfx_bloom_strength=clampf(g_postfx_bloom_strength-0.05f,0.0f,2.5f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_POST_BLOOM_INC:
            g_postfx_bloom_strength=clampf(g_postfx_bloom_strength+0.05f,0.0f,2.5f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_SSAO_DEC:
            g_postfx_ssao_strength=clampf(g_postfx_ssao_strength-0.05f,0.0f,1.5f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_SSAO_INC:
            g_postfx_ssao_strength=clampf(g_postfx_ssao_strength+0.05f,0.0f,1.5f);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_POST_APPLY:
            if(g_shader_lab.prog&&g_shader_lab.selected>=0) g_shader_lab.enabled=true;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_FX_TOD_DEC:
            if(ctx->total_time_ptr){
                *ctx->total_time_ptr-=DAY_DURATION/24.0f;
                if(*ctx->total_time_ptr<0.0f)*ctx->total_time_ptr=0.0f;
            }
            break;
        case EDITOR_WF_ACTION_FX_TOD_INC:
            if(ctx->total_time_ptr) *ctx->total_time_ptr+=DAY_DURATION/24.0f;
            break;
        case EDITOR_WF_ACTION_FX_TOD_NOON:
            if(ctx->total_time_ptr){
                float _t=*ctx->total_time_ptr;
                *ctx->total_time_ptr=floorf(_t/DAY_DURATION)*DAY_DURATION+DAY_DURATION*0.5f;
            }
            break;
        case EDITOR_WF_ACTION_FX_FXAA_TOGGLE:
            g_fxaa_enabled=!g_fxaa_enabled;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_VISUAL_LOOK_NATURAL:
            visual_profile_apply_look(VISUAL_LOOK_NATURAL,ctx->weather,
                                      ctx->wetness,ctx->fog_density,ctx->overcast,
                                      ctx->total_time_ptr);
            break;
        case EDITOR_WF_ACTION_VISUAL_LOOK_CINEMATIC:
            visual_profile_apply_look(VISUAL_LOOK_CINEMATIC,ctx->weather,
                                      ctx->wetness,ctx->fog_density,ctx->overcast,
                                      ctx->total_time_ptr);
            break;
        case EDITOR_WF_ACTION_VISUAL_LOOK_STORM:
            visual_profile_apply_look(VISUAL_LOOK_STORM,ctx->weather,
                                      ctx->wetness,ctx->fog_density,ctx->overcast,
                                      ctx->total_time_ptr);
            break;
        case EDITOR_WF_ACTION_VISUAL_LOOK_LOW_LIGHT:
            visual_profile_apply_look(VISUAL_LOOK_LOW_LIGHT,ctx->weather,
                                      ctx->wetness,ctx->fog_density,ctx->overcast,
                                      ctx->total_time_ptr);
            break;
        case EDITOR_WF_ACTION_VISUAL_RESET_PROFILE:
            visual_profile_apply_look(VISUAL_LOOK_RESET,ctx->weather,
                                      ctx->wetness,ctx->fog_density,ctx->overcast,
                                      ctx->total_time_ptr);
            break;
        case EDITOR_WF_ACTION_VISUAL_SAVE_PROFILE:
            if(ctx->wetness&&ctx->fog_density&&ctx->overcast)
                visual_profile_capture_weather(*ctx->wetness,*ctx->fog_density,*ctx->overcast);
            visual_profile_save();
            break;

        case EDITOR_WF_ACTION_SHADER_PREV:
            if(g_shader_lab.count>0) g_shader_lab.selected=(g_shader_lab.selected+g_shader_lab.count-1)%g_shader_lab.count;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_SHADER_NEXT:
            if(g_shader_lab.count>0) g_shader_lab.selected=(g_shader_lab.selected+1)%g_shader_lab.count;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_SHADER_TOGGLE:
            if(g_shader_lab.prog) g_shader_lab.enabled=!g_shader_lab.enabled;
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_SHADER_RELOAD:
            shader_lab_reload_selected(&g_shader_lab);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_SHADER_RESCAN:
            shader_lab_refresh(&g_shader_lab);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_SHADER_APPLY:
            shader_lab_reload_selected(&g_shader_lab);
            visual_profile_mark_dirty();
            break;
        case EDITOR_WF_ACTION_PROP_MODE_SELECT:
            if(ctx->prop_mouse_mode) *ctx->prop_mouse_mode=0;
            break;
        case EDITOR_WF_ACTION_PROP_MODE_MOVE:
            if(ctx->prop_mouse_mode) *ctx->prop_mouse_mode=1;
            break;
        case EDITOR_WF_ACTION_PROP_MODE_ROTATE:
            if(ctx->prop_mouse_mode) *ctx->prop_mouse_mode=2;
            break;
        case EDITOR_WF_ACTION_PROP_MODE_SCALE:
            if(ctx->prop_mouse_mode) *ctx->prop_mouse_mode=3;
            break;
        case EDITOR_WF_ACTION_GIZMO_SPACE_TOGGLE:
            if(ctx->editor) ctx->editor->selection_gizmo_space=ctx->editor->selection_gizmo_space?0:1;
            break;
        case EDITOR_WF_ACTION_GIZMO_SNAP_TOGGLE:
            if(ctx->editor) ctx->editor->selection_gizmo_snap=!ctx->editor->selection_gizmo_snap;
            break;
        case EDITOR_WF_ACTION_PROP_Y_DEC:
        case EDITOR_WF_ACTION_PROP_Y_INC:
        case EDITOR_WF_ACTION_PROP_GROUND:
        case EDITOR_WF_ACTION_PROP_SAVE:
        case EDITOR_WF_ACTION_PROP_SNAP_TOGGLE:
        case EDITOR_WF_ACTION_PROP_DELETE:
        case EDITOR_WF_ACTION_PROP_COLL_NONE:
        case EDITOR_WF_ACTION_PROP_COLL_AABB:
        case EDITOR_WF_ACTION_PROP_COLL_MESH:
            {
                if(ctx->editor&&ctx->editor->surface_edit){
                    int idx=(ctx->selection&&ctx->selection->kind==EDITOR_SELECTION_SURFACE)?ctx->selection->index:-1;
                    if(idx>=0&&surface_piece_query(idx,NULL,NULL,NULL,NULL,NULL)){
                        if(action==EDITOR_WF_ACTION_PROP_COLL_NONE) editor_shell_set_surface_collision(ctx,PROP_COLL_NONE);
                        else if(action==EDITOR_WF_ACTION_PROP_COLL_AABB) editor_shell_set_surface_collision(ctx,PROP_COLL_AABB);
                    }
                    break;
                }
                int idx=(*ctx->prop_edit&&*ctx->prop_sel>=0&&*ctx->prop_sel<g_props.count)?*ctx->prop_sel:
                        ((*ctx->prop_place_mode&&((PropPlacer*)ctx->placer)->selected_prop>=0&&((PropPlacer*)ctx->placer)->selected_prop<g_props.count)?
                         ((PropPlacer*)ctx->placer)->selected_prop:-1);
                PropPlacer *placer=(PropPlacer*)ctx->placer;
                if(action==EDITOR_WF_ACTION_PROP_SAVE){ placer_save(&g_props); break; }
                if(action==EDITOR_WF_ACTION_PROP_SNAP_TOGGLE){ placer->snap_y=!placer->snap_y; break; }
                if(action==EDITOR_WF_ACTION_PROP_UNDO){ undo_perform(); break; }
                if(idx<0) break;
                if(action==EDITOR_WF_ACTION_PROP_Y_DEC||action==EDITOR_WF_ACTION_PROP_Y_INC||
                   action==EDITOR_WF_ACTION_PROP_GROUND){
                if(action==EDITOR_WF_ACTION_PROP_Y_DEC)
                    editor_command_prop_set_y(idx,g_props.list[idx].pos.y-0.5f,placer,NULL);
                else if(action==EDITOR_WF_ACTION_PROP_Y_INC)
                    editor_command_prop_set_y(idx,g_props.list[idx].pos.y+0.5f,placer,NULL);
                else
                    editor_command_prop_set_y(idx,scene_ground_y(g_props.list[idx].pos.x,g_props.list[idx].pos.z),placer,NULL);
                break;
            }
            if(action==EDITOR_WF_ACTION_PROP_DELETE){
                editor_command_prop_delete(idx,NULL);
                if(*ctx->prop_edit) *ctx->prop_sel=-1;
                if(*ctx->prop_place_mode) placer->selected_prop=-1;
                if(ctx->selection&&editor_selection_is(EDITOR_SELECTION_PROP,idx)) editor_selection_clear();
                break;
                }else if(action==EDITOR_WF_ACTION_PROP_COLL_NONE) prop_apply_collision_mode(idx,PROP_COLL_NONE);
                else if(action==EDITOR_WF_ACTION_PROP_COLL_AABB) prop_apply_collision_mode(idx,PROP_COLL_AABB);
                else if(action==EDITOR_WF_ACTION_PROP_COLL_MESH) prop_apply_collision_mode(idx,PROP_COLL_MESH);
                if(action!=EDITOR_WF_ACTION_PROP_COLL_NONE&&action!=EDITOR_WF_ACTION_PROP_COLL_AABB&&action!=EDITOR_WF_ACTION_PROP_COLL_MESH)
                    prop_refresh_collision(&g_props.list[idx]);
                placer_sync_from_prop(placer,&g_props.list[idx]);
            }
            break;
        case EDITOR_WF_ACTION_PLACER_ROT_DEC:
            ((PropPlacer*)ctx->placer)->place_ry=fmodf(((PropPlacer*)ctx->placer)->place_ry+15.0f,360.0f);
            break;
        case EDITOR_WF_ACTION_PLACER_ROT_INC:
            ((PropPlacer*)ctx->placer)->place_ry=fmodf(((PropPlacer*)ctx->placer)->place_ry-15.0f+360.0f,360.0f);
            break;
        case EDITOR_WF_ACTION_PLACER_SCALE_DEC:
            ((PropPlacer*)ctx->placer)->place_scale=clampf(((PropPlacer*)ctx->placer)->place_scale-0.05f,0.05f,10.0f);
            break;
        case EDITOR_WF_ACTION_PLACER_SCALE_INC:
            ((PropPlacer*)ctx->placer)->place_scale=clampf(((PropPlacer*)ctx->placer)->place_scale+0.05f,0.05f,10.0f);
            break;
        case EDITOR_WF_ACTION_PLACER_SNAP_TOGGLE:
            ((PropPlacer*)ctx->placer)->snap_y=!((PropPlacer*)ctx->placer)->snap_y;
            break;
        case EDITOR_WF_ACTION_PLACER_SAVE:
            placer_save(&g_props);
            break;
        case EDITOR_WF_ACTION_PLACER_RESCAN:
            placer_scan_models((PropPlacer*)ctx->placer);
            break;
        case EDITOR_WF_ACTION_PLACER_MODE_TOGGLE:
            ((PropPlacer*)ctx->placer)->prefab_mode=!((PropPlacer*)ctx->placer)->prefab_mode;
            ((PropPlacer*)ctx->placer)->search_focus=false;
            SDL_StopTextInput();
            break;
        case EDITOR_WF_ACTION_PLACER_COLL_NONE:
            ((PropPlacer*)ctx->placer)->place_coll=PROP_COLL_NONE;
            break;
        case EDITOR_WF_ACTION_PLACER_COLL_AABB:
            ((PropPlacer*)ctx->placer)->place_coll=PROP_COLL_AABB;
            break;
        case EDITOR_WF_ACTION_PLACER_COLL_MESH:
            ((PropPlacer*)ctx->placer)->place_coll=PROP_COLL_MESH;
            break;
        case EDITOR_WF_ACTION_PLIGHT_RADIUS_DEC:
            editor_shell_plight_set_radius(ctx,editor_shell_current_plight_radius(ctx)-1.0f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_RADIUS_INC:
            editor_shell_plight_set_radius(ctx,editor_shell_current_plight_radius(ctx)+1.0f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_INTENSITY_DEC:
            editor_shell_plight_set_intensity(ctx,editor_shell_current_plight_intensity(ctx)-0.05f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_INTENSITY_INC:
            editor_shell_plight_set_intensity(ctx,editor_shell_current_plight_intensity(ctx)+0.05f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_R_DEC:
            editor_shell_plight_set_color(ctx,0,editor_shell_current_plight_color(ctx,0)-0.05f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_R_INC:
            editor_shell_plight_set_color(ctx,0,editor_shell_current_plight_color(ctx,0)+0.05f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_G_DEC:
            editor_shell_plight_set_color(ctx,1,editor_shell_current_plight_color(ctx,1)-0.05f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_G_INC:
            editor_shell_plight_set_color(ctx,1,editor_shell_current_plight_color(ctx,1)+0.05f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_B_DEC:
            editor_shell_plight_set_color(ctx,2,editor_shell_current_plight_color(ctx,2)-0.05f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_B_INC:
            editor_shell_plight_set_color(ctx,2,editor_shell_current_plight_color(ctx,2)+0.05f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_Y_DEC:
            editor_shell_plight_set_y_offset(ctx,editor_shell_current_plight_y_offset(ctx)-0.1f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_Y_INC:
            editor_shell_plight_set_y_offset(ctx,editor_shell_current_plight_y_offset(ctx)+0.1f);
            break;
        case EDITOR_WF_ACTION_PLIGHT_FLICKER_TOGGLE:
            editor_shell_plight_set_flicker(ctx,!editor_shell_current_plight_flicker(ctx));
            break;
        case EDITOR_WF_ACTION_PLIGHT_DELETE:
            editor_shell_plight_delete_selected(ctx);
            break;
        case EDITOR_WF_ACTION_PLIGHT_SAVE:
            placed_lights_save(&g_placed_lights);
            if(ctx->save_note) save_notify(ctx->save_note,"Lights saved");
            break;

        case EDITOR_WF_ACTION_INTERIOR_TARGET_PREV:
            if(ctx->editor){
                ctx->editor->interior_surface_target=editor_shell_wrap_interior_surface(ctx->editor->interior_surface_target,-1);
                editor_shell_sync_interior_material(ctx->editor);
            }
            break;
        case EDITOR_WF_ACTION_INTERIOR_TARGET_NEXT:
            if(ctx->editor){
                ctx->editor->interior_surface_target=editor_shell_wrap_interior_surface(ctx->editor->interior_surface_target,1);
                editor_shell_sync_interior_material(ctx->editor);
            }
            break;
        case EDITOR_WF_ACTION_INTERIOR_MATERIAL_PREV:
        case EDITOR_WF_ACTION_INTERIOR_MATERIAL_NEXT:
            if(ctx->editor&&interior_material_count()>0){
                int dir=(action==EDITOR_WF_ACTION_INTERIOR_MATERIAL_NEXT)?1:-1;
                ctx->editor->interior_material_sel=
                    interior_material_cycle_for_surface(ctx->editor->interior_material_sel,
                                                        ctx->editor->interior_surface_target,dir);
            }
            break;
        case EDITOR_WF_ACTION_INTERIOR_SAVE:
            interior_material_assignments_save();
            if(ctx->save_note) save_notify(ctx->save_note,"Interior materials saved");
            break;

        case EDITOR_WF_ACTION_QUEST_SAVE:
            quest_save_file(ctx->qs);
            break;
        case EDITOR_WF_ACTION_QUEST_PREV:
            if(*ctx->quest_ed_sel>0){
                (*ctx->quest_ed_sel)--;
                if(*ctx->quest_ed_sel<ctx->qs->count){
                    Quest *q=&ctx->qs->quests[*ctx->quest_ed_sel];
                    strncpy(ctx->quest_ed_name_buf,q->name,63);
                    strncpy(ctx->quest_ed_desc_buf,q->desc,127);
                    *ctx->quest_ed_gold=q->gold;
                    *ctx->quest_ed_xp=q->xp;
                    *ctx->quest_ed_obj_sel=0;
                }
            }
            break;
        case EDITOR_WF_ACTION_QUEST_NEXT:
            if(*ctx->quest_ed_sel<ctx->qs->count-1){
                (*ctx->quest_ed_sel)++;
                {
                    Quest *q=&ctx->qs->quests[*ctx->quest_ed_sel];
                    strncpy(ctx->quest_ed_name_buf,q->name,63);
                    strncpy(ctx->quest_ed_desc_buf,q->desc,127);
                    *ctx->quest_ed_gold=q->gold;
                    *ctx->quest_ed_xp=q->xp;
                    *ctx->quest_ed_obj_sel=0;
                }
            }
            break;
        case EDITOR_WF_ACTION_QUEST_ADD:{
            Quest *q=quest_add(ctx->qs,"New Quest","Description");
            if(q){
                *ctx->quest_ed_sel=ctx->qs->count-1;
                strncpy(ctx->quest_ed_name_buf,q->name,63);
                strncpy(ctx->quest_ed_desc_buf,q->desc,127);
                *ctx->quest_ed_gold=0;
                *ctx->quest_ed_xp=50;
                *ctx->quest_ed_obj_sel=0;
            }
            break;}
        case EDITOR_WF_ACTION_QUEST_DELETE:
            if(*ctx->quest_ed_sel>=0&&*ctx->quest_ed_sel<ctx->qs->count){
                int si=*ctx->quest_ed_sel;
                for(int i=si;i<ctx->qs->count-1;i++) ctx->qs->quests[i]=ctx->qs->quests[i+1];
                ctx->qs->count--;
                if(*ctx->quest_ed_sel>=ctx->qs->count) *ctx->quest_ed_sel=ctx->qs->count-1;
                if(*ctx->quest_ed_sel>=0&&*ctx->quest_ed_sel<ctx->qs->count){
                    Quest *q=&ctx->qs->quests[*ctx->quest_ed_sel];
                    strncpy(ctx->quest_ed_name_buf,q->name,63);
                    strncpy(ctx->quest_ed_desc_buf,q->desc,127);
                    *ctx->quest_ed_gold=q->gold;
                    *ctx->quest_ed_xp=q->xp;
                }
            }
            break;
        case EDITOR_WF_ACTION_QUEST_OBJ_ADD_KILL:
            if(*ctx->quest_ed_sel>=0&&*ctx->quest_ed_sel<ctx->qs->count){
                Quest *q=&ctx->qs->quests[*ctx->quest_ed_sel];
                quest_kill_obj_type(q,"Kill enemies",*ctx->quest_ed_obj_required,*ctx->quest_ed_obj_etype);
                *ctx->quest_ed_obj_sel=q->obj_count-1;
                if(*ctx->quest_ed_obj_sel>=0) strncpy(ctx->quest_ed_obj_desc_buf,q->objs[*ctx->quest_ed_obj_sel].desc,63);
            }
            break;
        case EDITOR_WF_ACTION_QUEST_OBJ_ADD_REACH:
            if(*ctx->quest_ed_sel>=0&&*ctx->quest_ed_sel<ctx->qs->count){
                Quest *q=&ctx->qs->quests[*ctx->quest_ed_sel];
                quest_reach_obj(q,"Reach location",(Vec3){0,0,0},8.0f);
                *ctx->quest_ed_obj_sel=q->obj_count-1;
                if(*ctx->quest_ed_obj_sel>=0) strncpy(ctx->quest_ed_obj_desc_buf,q->objs[*ctx->quest_ed_obj_sel].desc,63);
            }
            break;
        case EDITOR_WF_ACTION_QUEST_OBJ_DELETE:
            if(*ctx->quest_ed_sel>=0&&*ctx->quest_ed_sel<ctx->qs->count){
                Quest *q=&ctx->qs->quests[*ctx->quest_ed_sel];
                int oi=*ctx->quest_ed_obj_sel;
                if(oi>=0&&oi<q->obj_count){
                    for(int i=oi;i<q->obj_count-1;i++) q->objs[i]=q->objs[i+1];
                    q->obj_count--;
                    if(*ctx->quest_ed_obj_sel>=q->obj_count) *ctx->quest_ed_obj_sel=q->obj_count-1;
                }
            }
            break;
        case EDITOR_WF_ACTION_QUEST_OBJ_PREV:
            if(*ctx->quest_ed_obj_sel>0){
                (*ctx->quest_ed_obj_sel)--;
                if(*ctx->quest_ed_sel>=0&&*ctx->quest_ed_sel<ctx->qs->count){
                    Quest *q=&ctx->qs->quests[*ctx->quest_ed_sel];
                    int oi=*ctx->quest_ed_obj_sel;
                    if(oi>=0&&oi<q->obj_count) strncpy(ctx->quest_ed_obj_desc_buf,q->objs[oi].desc,63);
                }
            }
            break;
        case EDITOR_WF_ACTION_QUEST_OBJ_NEXT:
            if(*ctx->quest_ed_sel>=0&&*ctx->quest_ed_sel<ctx->qs->count){
                Quest *q=&ctx->qs->quests[*ctx->quest_ed_sel];
                if(*ctx->quest_ed_obj_sel<q->obj_count-1){
                    (*ctx->quest_ed_obj_sel)++;
                    int oi=*ctx->quest_ed_obj_sel;
                    if(oi>=0&&oi<q->obj_count) strncpy(ctx->quest_ed_obj_desc_buf,q->objs[oi].desc,63);
                }
            }
            break;
        default:
            break;
    }
}
