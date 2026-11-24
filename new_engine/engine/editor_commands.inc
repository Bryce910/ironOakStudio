#include "engine/editor_commands.h"

static int npc_add(NpcSystem *ns, Vec3 pos, float yaw, const char *name);
static void npc_patrol_clear(int npc_idx);
static void npc_patrol_add(int npc_idx, float speed, float wait_secs, bool loop_path);
static void npc_patrol_waypoint(int npc_idx, Vec3 wp);
static void npc_add_line(NpcSystem *ns, int idx, const char *line);
static void editor_npc_delete_spawn_at(NpcSpawnList *spawns,int deleted_idx,int *selected_idx,
                                       bool *wp_mode,int *wp_npc,
                                       bool *dlg_mode,int *dlg_npc,int *dlg_sel,char *dlg_buf,int *dlg_len,
                                       bool *tree_mode,int *tree_npc,int *tree_sel,int *tree_choice,
                                       bool *tree_editing,int *tree_field,char *tree_buf,int *tree_len);

static EditorCommandRuntime g_editor_command_runtime={0};

static void editor_command_result_reset(EditorCommandResult *out,
                                        EditorCommandDomain domain,
                                        EditorCommandKind kind){
    if(!out) return;
    memset(out,0,sizeof(*out));
    out->domain=domain;
    out->kind=kind;
    out->primary_index=-1;
    out->secondary_index=-1;
}

static void editor_command_runtime_note_change(EditorCommandRuntime *rt){
    if(!rt) return;
    if(rt->transaction_depth>0) rt->transaction_size++;
}

static void editor_commands_runtime_reset(EditorCommandRuntime *rt){
    if(!rt) return;
    memset(rt,0,sizeof(*rt));
}

static EditorCommandRuntime *editor_commands_runtime(void){
    return &g_editor_command_runtime;
}

static bool editor_commands_begin_transaction(EditorCommandRuntime *rt,const char *label,
                                              EditorCommandResult *out){
    if(!rt) return false;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NONE,
                                EDITOR_COMMAND_KIND_TRANSACTION_BEGIN);
    rt->transaction_depth++;
    if(rt->transaction_depth==1){
        rt->transaction_size=0;
        undo_begin_transaction(label);
        if(label){
            strncpy(rt->label,label,sizeof(rt->label)-1);
            rt->label[sizeof(rt->label)-1]=0;
        }else rt->label[0]=0;
    }
    if(out){
        out->transaction_open=true;
        out->transaction_size=rt->transaction_size;
        strncpy(out->label,rt->label,sizeof(out->label)-1);
    }
    return true;
}

static bool editor_commands_end_transaction(EditorCommandRuntime *rt,
                                            EditorCommandResult *out){
    if(!rt||rt->transaction_depth<=0) return false;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NONE,
                                EDITOR_COMMAND_KIND_TRANSACTION_END);
    rt->transaction_depth--;
    if(out){
        out->changed=rt->transaction_size>0;
        out->transaction_open=rt->transaction_depth>0;
        out->transaction_size=rt->transaction_size;
        strncpy(out->label,rt->label,sizeof(out->label)-1);
    }
    if(rt->transaction_depth==0){
        undo_end_transaction();
        rt->transaction_size=0;
        rt->label[0]=0;
    }
    return true;
}

static bool editor_command_prefab_place_props(int prefab_idx,Vec3 anchor,float yaw_deg,float scale,
                                              int *out_first_idx,EditorCommandResult *out){
    PrefabTemplate *pf;
    float yaw_rad;
    float cy,sy;
    int first_prop=-1;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_CREATE_MODEL);
    if(out_first_idx) *out_first_idx=-1;
    if(prefab_idx<0||prefab_idx>=g_prefabs.count) return false;
    pf=&g_prefabs.items[prefab_idx];
    yaw_rad=yaw_deg*(float)M_PI/180.0f;
    cy=cosf(yaw_rad);
    sy=sinf(yaw_rad);
    scale=clampf(scale,0.05f,10.0f);
    for(int i=0;i<pf->prop_count;i++){
        PrefabProp *pp=&pf->props[i];
        Vec3 rel=pp->rel_pos;
        Vec3 pos;
        float rx=rel.x*scale;
        float rz=rel.z*scale;
        int idx=-1;
        pos.x=anchor.x + rx*cy + rz*sy;
        pos.z=anchor.z + (-rx*sy + rz*cy);
        pos.y=anchor.y + rel.y*scale;
        if(editor_command_prop_create_model(pp->path,pos,pp->rot_x,pp->rot_y+yaw_deg,pp->rot_z,
                                            pp->scale*scale,(int)pp->coll,false,&idx,NULL)) {
            if(first_prop<0) first_prop=idx;
        }
    }
    if(first_prop<0) return false;
    if(out_first_idx) *out_first_idx=first_prop;
    if(out){
        out->changed=true;
        out->primary_index=first_prop;
    }
    return true;
}

static bool editor_command_prefab_place_instance(int prefab_idx,Vec3 anchor,float yaw_deg,float scale,
                                                 LightingSystem *ls,int *out_first_idx,
                                                 EditorCommandResult *out){
    PrefabTemplate *pf;
    float yaw_rad;
    float cy,sy;
    int first_prop=-1;
    bool opened_here=false;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_CREATE_MODEL);
    if(out_first_idx) *out_first_idx=-1;
    if(prefab_idx<0||prefab_idx>=g_prefabs.count) return false;
    pf=&g_prefabs.items[prefab_idx];
    yaw_rad=yaw_deg*(float)M_PI/180.0f;
    cy=cosf(yaw_rad);
    sy=sinf(yaw_rad);
    scale=clampf(scale,0.05f,10.0f);
    if(editor_commands_runtime()->transaction_depth<=0){
        editor_commands_begin_transaction(editor_commands_runtime(),"prefab-place",NULL);
        opened_here=true;
    }
    for(int i=0;i<pf->prop_count;i++){
        PrefabProp *pp=&pf->props[i];
        Vec3 rel=pp->rel_pos;
        Vec3 pos;
        float rx=rel.x*scale;
        float rz=rel.z*scale;
        int idx=-1;
        pos.x=anchor.x + rx*cy + rz*sy;
        pos.z=anchor.z + (-rx*sy + rz*cy);
        pos.y=anchor.y + rel.y*scale;
        if(editor_command_prop_create_model(pp->path,pos,pp->rot_x,pp->rot_y+yaw_deg,pp->rot_z,
                                            pp->scale*scale,(int)pp->coll,false,&idx,NULL) &&
           first_prop<0)
            first_prop=idx;
    }
    if(ls){
        for(int i=0;i<pf->fx_count;i++){
            PrefabFx *fx=&pf->fx[i];
            Vec3 rel=fx->rel_pos;
            Vec3 pos;
            WorldFxEditor ed;
            float rx=rel.x*scale;
            float rz=rel.z*scale;
            pos.x=anchor.x + rx*cy + rz*sy;
            pos.z=anchor.z + (-rx*sy + rz*cy);
            pos.y=anchor.y + rel.y*scale;
            memset(&ed,0,sizeof(ed));
            ed.kind=(fx->kind>=0&&fx->kind<WORLD_FX_COUNT)?(WorldFxKind)fx->kind:WORLD_FX_FIRE;
            ed.scale=fx->scale*scale;
            ed.height=fx->height*scale;
            ed.intensity=fx->intensity;
            ed.color=fx->color;
            ed.light_enabled=fx->light_enabled;
            ed.light_color=fx->light_color;
            ed.light_radius=fx->light_radius*scale;
            ed.light_intensity=fx->light_intensity;
            world_fx_place_with_undo(&g_world_fx,ls,pos,&ed);
            editor_command_runtime_note_change(editor_commands_runtime());
        }
    }
    if(opened_here) editor_commands_end_transaction(editor_commands_runtime(),NULL);
    if(first_prop<0 && (!ls || pf->fx_count<=0)) return false;
    if(out_first_idx) *out_first_idx=first_prop;
    if(out){
        out->changed=true;
        out->primary_index=first_prop;
    }
    return true;
}

static bool editor_command_prop_create_model(const char *path,Vec3 pos,float rot_x,float rot_y,
                                             float rot_z,float scale,int coll,
                                             bool snap_to_ground,int *out_idx,
                                             EditorCommandResult *out){
    int idx;
    UndoSnap placed;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_CREATE_MODEL);
    if(out_idx) *out_idx=-1;
    if(!path||!path[0]) return false;
    idx=props_next_slot(&g_props);
    if(idx<0) return false;
    if(!prop_register(&g_props,path,pos.x,snap_to_ground?0.0f:pos.y,pos.z,
                      rot_x,rot_y,rot_z,scale,(PropCollType)coll,0,0,0,0))
        return false;
    stream_request(&g_stream,idx);
    memset(&placed,0,sizeof(placed));
    placed.prop=undo_snap_prop(idx);
    undo_push_place(UNDO_PROP_PLACE,idx,idx,placed);
    if(out_idx) *out_idx=idx;
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_delete(int idx,EditorCommandResult *out){
    UndoSnap deleted;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_DELETE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    memset(&deleted,0,sizeof(deleted));
    deleted.prop=undo_snap_prop(idx);
    undo_push_delete(UNDO_PROP_DELETE,idx,g_props.count,deleted);
    prop_release_all(&g_props.list[idx]);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_y(int idx,float y,void *placer,EditorCommandResult *out){
    PropPlacer *pp=(PropPlacer*)placer;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_Y);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    if(fabsf(g_props.list[idx].pos.y-y)<=0.0001f) return false;
    before=undo_snap_prop(idx);
    g_props.list[idx].pos.y=y;
    prop_refresh_collision(&g_props.list[idx]);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(pp) placer_sync_from_prop(pp,&g_props.list[idx]);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_rotation_y(int idx,float yaw_deg,void *placer,
                                               EditorCommandResult *out){
    PropPlacer *pp=(PropPlacer*)placer;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ROTATION_Y);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    if(fabsf(g_props.list[idx].rot_y-yaw_deg)<=0.0001f) return false;
    before=undo_snap_prop(idx);
    g_props.list[idx].rot_y=yaw_deg;
    g_props.list[idx].door_closed_yaw=yaw_deg;
    g_props.list[idx].door_is_open=false;
    prop_refresh_collision(&g_props.list[idx]);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(pp) placer_sync_from_prop(pp,&g_props.list[idx]);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_scale(int idx,float scale,void *placer,
                                          EditorCommandResult *out){
    PropPlacer *pp=(PropPlacer*)placer;
    float clamped=clampf(scale,0.05f,20.0f);
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_SCALE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    if(fabsf(g_props.list[idx].scale-clamped)<=0.0001f) return false;
    before=undo_snap_prop(idx);
    g_props.list[idx].scale=clamped;
    if(pp&&pp->snap_y)
        g_props.list[idx].pos.y=scene_ground_y(g_props.list[idx].pos.x,g_props.list[idx].pos.z);
    prop_refresh_collision(&g_props.list[idx]);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(pp) placer_sync_from_prop(pp,&g_props.list[idx]);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_pickable(int idx,bool pickable,EditorCommandResult *out){
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_PICKABLE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    if(g_props.list[idx].pickable==pickable) return false;
    before=undo_snap_prop(idx);
    g_props.list[idx].pickable=pickable;
    if(!pickable){
        g_props.list[idx].pickup_item=ITEM_NONE;
        g_props.list[idx].pickup_count=1;
    }else{
        if(g_props.list[idx].pickup_item<=ITEM_NONE||g_props.list[idx].pickup_item>=ITEM_COUNT)
            g_props.list[idx].pickup_item=ITEM_POTION_HP;
        if(g_props.list[idx].pickup_count<=0) g_props.list[idx].pickup_count=1;
    }
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_pickup_item(int idx,int item_type,
                                                EditorCommandResult *out){
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_PICKUP_ITEM);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    if(item_type<0||item_type>=ITEM_COUNT) return false;
    if(g_props.list[idx].pickable&&g_props.list[idx].pickup_item==(ItemType)item_type) return false;
    before=undo_snap_prop(idx);
    g_props.list[idx].pickable=(item_type!=ITEM_NONE);
    g_props.list[idx].pickup_item=(ItemType)item_type;
    if(g_props.list[idx].pickup_count<=0) g_props.list[idx].pickup_count=1;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_pickup_count(int idx,int count,
                                                 EditorCommandResult *out){
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_PICKUP_COUNT);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    if(count<1) count=1;
    if(count>99) count=99;
    if(g_props.list[idx].pickup_count==count) return false;
    before=undo_snap_prop(idx);
    g_props.list[idx].pickup_count=count;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_interact_type(int idx,int interact_type,
                                                  EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    PropInteractType next=(interact_type==PROP_INTERACT_DOOR)?PROP_INTERACT_DOOR:PROP_INTERACT_NONE;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_INTERACT_TYPE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(p->interact_type==next) return false;
    before=undo_snap_prop(idx);
    p->interact_type=next;
    p->interact_radius=(p->interact_radius>0.1f)?p->interact_radius:2.5f;
    p->door_open_angle=(fabsf(p->door_open_angle)>1.0f)?p->door_open_angle:90.0f;
    p->door_closed_yaw=p->rot_y;
    p->door_is_open=false;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_interact_radius(int idx,float radius,
                                                    EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    radius=clampf(radius,0.5f,12.0f);
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_INTERACT_RADIUS);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(fabsf(p->interact_radius-radius)<=0.0001f) return false;
    before=undo_snap_prop(idx);
    p->interact_radius=radius;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_door_open_angle(int idx,float angle_deg,
                                                    EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    angle_deg=clampf(angle_deg,-170.0f,170.0f);
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_DOOR_OPEN_ANGLE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(fabsf(p->door_open_angle-angle_deg)<=0.0001f) return false;
    before=undo_snap_prop(idx);
    p->door_open_angle=angle_deg;
    p->door_closed_yaw=p->rot_y;
    p->door_is_open=false;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_add_action(int idx,EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_ADD_ACTION);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(p->action_count>=PROP_ACTION_MAX) return false;
    before=undo_snap_prop(idx);
    p->actions[p->action_count]=(PropAction){
        PROP_ACTION_TOGGLE_TARGET_LIGHT,
        8.0f,
        -1,
        PROP_ACTION_COND_NONE,
        0,
        0,
        85.0f,
        "",
        ""
    };
    p->action_selected=p->action_count;
    p->action_count++;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_remove_action(int idx,int action_idx,EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_REMOVE_ACTION);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    before=undo_snap_prop(idx);
    for(int i=action_idx;i<p->action_count-1;i++) p->actions[i]=p->actions[i+1];
    if(p->action_count>0) p->action_count--;
    if(p->action_count<0) p->action_count=0;
    if(p->action_count<PROP_ACTION_MAX) memset(&p->actions[p->action_count],0,sizeof(p->actions[p->action_count]));
    if(p->action_selected>=p->action_count) p->action_selected=p->action_count-1;
    if(p->action_selected<0) p->action_selected=0;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_action_type(int idx,int action_idx,int action_type,
                                                EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    PropActionType next=PROP_ACTION_NONE;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ACTION_TYPE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    switch(action_type){
        case PROP_ACTION_TOGGLE_NEAREST_LIGHT:
        case PROP_ACTION_TOGGLE_TARGET_LIGHT:
        case PROP_ACTION_TOGGLE_TARGET_COLLISION_VOLUME:
        case PROP_ACTION_TOGGLE_TARGET_DOOR:
        case PROP_ACTION_SET_WORLD_FLAG:
        case PROP_ACTION_CLEAR_WORLD_FLAG:
        case PROP_ACTION_PLAY_SOUND:
        case PROP_ACTION_READ_PROMPT:
            next=(PropActionType)action_type;
            break;
        default:
            next=PROP_ACTION_NONE;
            break;
    }
    if(p->actions[action_idx].type==next) return false;
    before=undo_snap_prop(idx);
    p->actions[action_idx].type=next;
    if(p->actions[action_idx].radius<=0.1f) p->actions[action_idx].radius=8.0f;
    if(next==PROP_ACTION_TOGGLE_NEAREST_LIGHT) p->actions[action_idx].target_index=-1;
    if(p->actions[action_idx].target_index<-1) p->actions[action_idx].target_index=-1;
    if(p->actions[action_idx].sound_idx<0||p->actions[action_idx].sound_idx>=PROP_ACTION_SOUND_COUNT)
        p->actions[action_idx].sound_idx=0;
    if(p->actions[action_idx].sound_volume<1.0f) p->actions[action_idx].sound_volume=85.0f;
    if(next!=PROP_ACTION_PLAY_SOUND&&p->actions[action_idx].sound_path[0]){
        p->actions[action_idx].sound_path[0]=0;
    }
    if(next!=PROP_ACTION_READ_PROMPT&&p->actions[action_idx].prompt_text[0]){
        p->actions[action_idx].prompt_text[0]=0;
    }
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
        out->secondary_index=action_idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_action_radius(int idx,int action_idx,float radius,
                                                  EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    radius=clampf(radius,0.5f,30.0f);
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ACTION_RADIUS);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    if(fabsf(p->actions[action_idx].radius-radius)<=0.0001f) return false;
    before=undo_snap_prop(idx);
    p->actions[action_idx].radius=radius;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
        out->secondary_index=action_idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_action_target_index(int idx,int action_idx,int target_index,
                                                        EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ACTION_TARGET_INDEX);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    if(target_index<-1) target_index=-1;
    if(p->actions[action_idx].target_index==target_index) return false;
    before=undo_snap_prop(idx);
    p->actions[action_idx].target_index=target_index;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
        out->secondary_index=action_idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_action_sound_idx(int idx,int action_idx,int sound_idx,
                                                     EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ACTION_TARGET_INDEX);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    if(sound_idx<0) sound_idx=0;
    if(sound_idx>=PROP_ACTION_SOUND_COUNT) sound_idx=PROP_ACTION_SOUND_COUNT-1;
    if(p->actions[action_idx].sound_idx==sound_idx) return false;
    before=undo_snap_prop(idx);
    p->actions[action_idx].sound_idx=sound_idx;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
        out->secondary_index=action_idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_action_sound_volume(int idx,int action_idx,float volume,
                                                        EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    volume=clampf(volume,1.0f,128.0f);
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ACTION_RADIUS);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    if(fabsf(p->actions[action_idx].sound_volume-volume)<=0.0001f) return false;
    before=undo_snap_prop(idx);
    p->actions[action_idx].sound_volume=volume;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
        out->secondary_index=action_idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_action_sound_path(int idx,int action_idx,const char *path,
                                                      EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    char normalized[256];
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ACTION_PROMPT_TEXT);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    snprintf(normalized,sizeof(normalized),"%s",path?path:"");
    if(strcmp(p->actions[action_idx].sound_path,normalized)==0) return false;
    before=undo_snap_prop(idx);
    snprintf(p->actions[action_idx].sound_path,sizeof(p->actions[action_idx].sound_path),"%s",normalized);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
        out->secondary_index=action_idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_action_condition_type(int idx,int action_idx,int condition_type,
                                                          EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    PropActionConditionType next=(condition_type==PROP_ACTION_COND_FLAG_SET)
        ?PROP_ACTION_COND_FLAG_SET
        :(condition_type==PROP_ACTION_COND_FLAG_CLEAR?PROP_ACTION_COND_FLAG_CLEAR:PROP_ACTION_COND_NONE);
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ACTION_CONDITION_TYPE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    if(p->actions[action_idx].condition_type==next) return false;
    before=undo_snap_prop(idx);
    p->actions[action_idx].condition_type=next;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
        out->secondary_index=action_idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_action_condition_flag(int idx,int action_idx,int condition_flag,
                                                          EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ACTION_CONDITION_FLAG);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    if(condition_flag<0) condition_flag=0;
    if(condition_flag>63) condition_flag=63;
    if(p->actions[action_idx].condition_flag==condition_flag) return false;
    before=undo_snap_prop(idx);
    p->actions[action_idx].condition_flag=condition_flag;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
        out->secondary_index=action_idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_action_prompt_text(int idx,int action_idx,const char *text,
                                                       EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    char normalized[512];
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_ACTION_PROMPT_TEXT);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(action_idx<0||action_idx>=p->action_count) return false;
    snprintf(normalized,sizeof(normalized),"%s",text?text:"");
    if(strcmp(p->actions[action_idx].prompt_text,normalized)==0) return false;
    before=undo_snap_prop(idx);
    snprintf(p->actions[action_idx].prompt_text,sizeof(p->actions[action_idx].prompt_text),"%s",normalized);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){
        out->changed=true;
        out->primary_index=idx;
        out->secondary_index=action_idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_collision_mode(int idx,int coll_type,void *placer,
                                                   EditorCommandResult *out){
    PropPlacer *pp=(PropPlacer*)placer;
    PropSnap before;
    PropCollType next=(coll_type==PROP_COLL_NONE)?PROP_COLL_NONE:
                      (coll_type==PROP_COLL_MESH)?PROP_COLL_MESH:PROP_COLL_AABB;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_SCALE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    if(g_props.list[idx].coll_type==next) return false;
    before=undo_snap_prop(idx);
    g_props.list[idx].coll_type=next;
    if(!prop_uses_scene_renderer(&g_props.list[idx])){
        if(g_props.list[idx].stream_state==PROP_LOADED) prop_unload_gpu(&g_props.list[idx]);
        if(g_props.list[idx].coll_type!=PROP_COLL_NONE||prop_has_custom_collision(&g_props.list[idx]))
            stream_request(&g_stream,idx);
    }else{
        prop_refresh_collision(&g_props.list[idx]);
    }
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(pp) placer_sync_from_prop(pp,&g_props.list[idx]);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_custom_collision_enabled(int idx,bool enabled,
                                                             EditorCommandResult *out){
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_SCALE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    if(enabled&&g_props.list[idx].custom_coll_count<=0) return false;
    if(g_props.list[idx].custom_coll_enabled==enabled) return false;
    before=undo_snap_prop(idx);
    g_props.list[idx].custom_coll_enabled=enabled;
    prop_refresh_collision(&g_props.list[idx]);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_add_custom_collision_box(int idx,EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_SCALE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(p->custom_coll_count>=PROP_CUSTOM_COLL_MAX) return false;
    before=undo_snap_prop(idx);
    prop_custom_collision_default_box(p,&p->custom_coll[p->custom_coll_count]);
    p->custom_coll_selected=p->custom_coll_count;
    p->custom_coll_count++;
    p->custom_coll_enabled=true;
    prop_refresh_collision(p);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_remove_custom_collision_box(int idx,int box_idx,
                                                            EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_SCALE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(box_idx<0||box_idx>=p->custom_coll_count) return false;
    before=undo_snap_prop(idx);
    for(int i=box_idx;i<p->custom_coll_count-1;i++) p->custom_coll[i]=p->custom_coll[i+1];
    p->custom_coll_count--;
    if(p->custom_coll_selected>=p->custom_coll_count) p->custom_coll_selected=p->custom_coll_count-1;
    if(p->custom_coll_selected<0) p->custom_coll_selected=0;
    if(p->custom_coll_count<=0) p->custom_coll_enabled=false;
    prop_refresh_collision(p);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_custom_collision_box_center(int idx,int box_idx,int axis,float value,
                                                                EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_SCALE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(box_idx<0||box_idx>=p->custom_coll_count||axis<0||axis>2) return false;
    before=undo_snap_prop(idx);
    if(axis==0) p->custom_coll[box_idx].center.x=value;
    else if(axis==1) p->custom_coll[box_idx].center.y=value;
    else p->custom_coll[box_idx].center.z=value;
    prop_refresh_collision(p);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_custom_collision_box_shape(int idx,int box_idx,int shape,
                                                               EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_SCALE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(box_idx<0||box_idx>=p->custom_coll_count) return false;
    before=undo_snap_prop(idx);
    p->custom_coll[box_idx].shape=prop_custom_collision_shape_sanitize(shape);
    prop_custom_collision_clamp_box(&p->custom_coll[box_idx]);
    prop_refresh_collision(p);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_custom_collision_box_half(int idx,int box_idx,int axis,float value,
                                                              EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_SCALE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(box_idx<0||box_idx>=p->custom_coll_count||axis<0||axis>2) return false;
    before=undo_snap_prop(idx);
    if(axis==0) p->custom_coll[box_idx].half.x=value;
    else if(axis==1) p->custom_coll[box_idx].half.y=value;
    else p->custom_coll[box_idx].half.z=value;
    prop_custom_collision_clamp_box(&p->custom_coll[box_idx]);
    prop_refresh_collision(p);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_custom_collision_box_yaw(int idx,int box_idx,float value,
                                                             EditorCommandResult *out){
    Prop *p;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_SCALE);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    p=&g_props.list[idx];
    if(box_idx<0||box_idx>=p->custom_coll_count) return false;
    before=undo_snap_prop(idx);
    p->custom_coll[box_idx].yaw_deg=value;
    prop_custom_collision_clamp_box(&p->custom_coll[box_idx]);
    prop_refresh_collision(p);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_commit_xform(int idx,PropSnap before,void *placer,
                                             EditorCommandResult *out){
    PropPlacer *pp=(PropPlacer*)placer;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_SET_Y);
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    prop_refresh_collision(&g_props.list[idx]);
    if(before.pos.x==g_props.list[idx].pos.x&&
       before.pos.y==g_props.list[idx].pos.y&&
       before.pos.z==g_props.list[idx].pos.z&&
       before.rot_x==g_props.list[idx].rot_x&&
       before.rot_y==g_props.list[idx].rot_y&&
       before.rot_z==g_props.list[idx].rot_z&&
       before.scale==g_props.list[idx].scale)
        return false;
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(pp) placer_sync_from_prop(pp,&g_props.list[idx]);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_prop_set_position(int idx,float x,float y,float z,void *placer,
                                             EditorCommandResult *out){
    PropSnap before;
    if(idx<0||idx>=g_props.count||!g_props.list[idx].active) return false;
    before=undo_snap_prop(idx);
    g_props.list[idx].pos.x=x;
    g_props.list[idx].pos.y=y;
    g_props.list[idx].pos.z=z;
    return editor_command_prop_commit_xform(idx,before,placer,out);
}

static bool editor_command_prop_replace_model_in_place(int idx,const char *path,void *placer,
                                                       EditorCommandResult *out){
    PropPlacer *pp=(PropPlacer*)placer;
    Prop *p;
    float wx,wy,wz,rx,ry,rz,scale;
    PropCollType coll;
    PropSnap before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_PROP,
                                EDITOR_COMMAND_KIND_PROP_CREATE_MODEL);
    if(idx<0||idx>=g_props.count||!path||!path[0]||!g_props.list[idx].active) return false;
    before=undo_snap_prop(idx);
    wx=g_props.list[idx].pos.x; wy=g_props.list[idx].pos.y; wz=g_props.list[idx].pos.z;
    rx=g_props.list[idx].rot_x; ry=g_props.list[idx].rot_y; rz=g_props.list[idx].rot_z;
    scale=pp?pp->place_scale:g_props.list[idx].scale;
    coll=pp?pp->place_coll:g_props.list[idx].coll_type;
    prop_release_all(&g_props.list[idx]);
    p=&g_props.list[idx];
    memset(p,0,sizeof(*p));
    strncpy(p->path,path,255);
    { float asset_scale=prop_asset_scale_override(path); if(asset_scale!=1.0f) scale*=asset_scale; }
    p->rot_x=rx; p->rot_y=ry; p->rot_z=rz;
    p->scale=(scale<=0.0f)?1.0f:scale;
    p->pos.x=wx; p->pos.y=(pp&&pp->snap_y)?scene_ground_y(wx,wz):wy; p->pos.z=wz;
    p->coll_type=coll;
    p->stream_state=PROP_UNLOADED;
    p->active=true;
    stream_request(&g_stream,idx);
    undo_push_prop_xform(idx,before,undo_snap_prop(idx));
    if(pp) placer_sync_from_prop(pp,&g_props.list[idx]);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_marker_create(int kind,Vec3 pos,float radius,
                                         int *out_idx,EditorCommandResult *out){
    WorldMarker *m;
    UndoSnap snap;
    int idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_MARKER,
                                EDITOR_COMMAND_KIND_MARKER_CREATE);
    if(out_idx) *out_idx=-1;
    if(g_world_markers.count>=MAX_WORLD_MARKERS) return false;
    idx=g_world_markers.count;
    m=&g_world_markers.items[idx];
    memset(m,0,sizeof(*m));
    m->kind=(WorldMarkerKind)kind;
    m->pos=pos;
    m->radius=clampf(radius,2.0f,200.0f);
    world_markers_make_name(m->name,sizeof(m->name),m->kind,idx);
    m->active=true;
    g_world_markers.count++;
    memset(&snap,0,sizeof(snap));
    snap.marker=*m;
    undo_push_place(UNDO_MARKER_PLACE,idx,idx,snap);
    world_markers_save(&g_world_markers);
    if(out_idx) *out_idx=idx;
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_marker_delete(int idx,EditorCommandResult *out){
    UndoSnap snap;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_MARKER,
                                EDITOR_COMMAND_KIND_MARKER_DELETE);
    if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) return false;
    memset(&snap,0,sizeof(snap));
    snap.marker=g_world_markers.items[idx];
    undo_push_delete(UNDO_MARKER_DELETE,idx,g_world_markers.count,snap);
    for(int i=idx;i<g_world_markers.count-1;i++) g_world_markers.items[i]=g_world_markers.items[i+1];
    g_world_markers.count--;
    editor_selection_after_compact_delete(EDITOR_SELECTION_MARKER,idx,g_world_markers.count);
    world_markers_save(&g_world_markers);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_marker_set_kind(int idx,int kind,EditorCommandResult *out){
    WorldMarker before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_MARKER,
                                EDITOR_COMMAND_KIND_MARKER_SET_KIND);
    if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) return false;
    if(kind<0||kind>=WORLD_MARKER_COUNT) return false;
    if((int)g_world_markers.items[idx].kind==kind) return false;
    before=g_world_markers.items[idx];
    g_world_markers.items[idx].kind=(WorldMarkerKind)kind;
    world_markers_make_name(g_world_markers.items[idx].name,sizeof(g_world_markers.items[idx].name),
                            g_world_markers.items[idx].kind,idx);
    undo_push_marker_edit(idx,before,g_world_markers.items[idx]);
    world_markers_save(&g_world_markers);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_marker_set_radius(int idx,float radius,EditorCommandResult *out){
    WorldMarker before;
    float clamped=clampf(radius,2.0f,200.0f);
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_MARKER,
                                EDITOR_COMMAND_KIND_MARKER_SET_RADIUS);
    if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) return false;
    if(fabsf(g_world_markers.items[idx].radius-clamped)<=0.0001f) return false;
    before=g_world_markers.items[idx];
    g_world_markers.items[idx].radius=clamped;
    undo_push_marker_edit(idx,before,g_world_markers.items[idx]);
    world_markers_save(&g_world_markers);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_marker_set_position(int idx,Vec3 pos,EditorCommandResult *out){
    WorldMarker before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_MARKER,
                                EDITOR_COMMAND_KIND_MARKER_SET_POSITION);
    if(idx<0||idx>=g_world_markers.count||!g_world_markers.items[idx].active) return false;
    if(fabsf(g_world_markers.items[idx].pos.x-pos.x)<=0.0001f&&
       fabsf(g_world_markers.items[idx].pos.y-pos.y)<=0.0001f&&
       fabsf(g_world_markers.items[idx].pos.z-pos.z)<=0.0001f)
        return false;
    before=g_world_markers.items[idx];
    g_world_markers.items[idx].pos=pos;
    undo_push_marker_edit(idx,before,g_world_markers.items[idx]);
    world_markers_save(&g_world_markers);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_create(int etype,Vec3 pos,int model_idx,
                                        float hp_over,float dmg_over,float speed_over,
                                        float chase_r_over,float atk_r_over,float atk_cd_over,float scale_over,
                                        int *out_idx,EditorCommandResult *out){
    SpawnPoint sp;
    UndoSnap snap;
    int idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_CREATE);
    if(out_idx) *out_idx=-1;
    if(etype<0||etype>=ETYPE_COUNT||g_spawns.count>=MAX_SPAWN_POINTS) return false;
    idx=g_spawns.count;
    memset(&sp,0,sizeof(sp));
    sp.pos=pos;
    sp.etype=(EnemyType)etype;
    sp.active=true;
    sp.model_idx=(g_enemy_model_count>0)
        ?((model_idx<0)?0:((model_idx>g_enemy_model_count)?g_enemy_model_count:model_idx))
        :0;
    sp.hp_over=hp_over;
    sp.dmg_over=dmg_over;
    sp.speed_over=speed_over;
    sp.chase_r_over=chase_r_over;
    sp.atk_r_over=atk_r_over;
    sp.atk_cd_over=atk_cd_over;
    sp.scale_over=scale_over;
    g_spawns.points[idx]=sp;
    g_spawns.count++;
    memset(&snap,0,sizeof(snap));
    snap.spawn=sp;
    undo_push_place(UNDO_SPAWN_PLACE,idx,idx,snap);
    spawns_save(&g_spawns);
    if(out_idx) *out_idx=idx;
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_delete(int idx,EditorCommandResult *out){
    UndoSnap snap;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_DELETE);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    memset(&snap,0,sizeof(snap));
    snap.spawn=g_spawns.points[idx];
    undo_push_delete(UNDO_SPAWN_DELETE,idx,g_spawns.count,snap);
    spawn_remove_at(idx);
    editor_selection_after_compact_delete(EDITOR_SELECTION_SPAWN,idx,g_spawns.count);
    spawns_save(&g_spawns);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_set_position(int idx,Vec3 pos,EditorCommandResult *out){
    SpawnPoint before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_SET_POSITION);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    if(fabsf(g_spawns.points[idx].pos.x-pos.x)<=0.0001f&&
       fabsf(g_spawns.points[idx].pos.y-pos.y)<=0.0001f&&
       fabsf(g_spawns.points[idx].pos.z-pos.z)<=0.0001f)
        return false;
    before=g_spawns.points[idx];
    g_spawns.points[idx].pos=pos;
    undo_push_spawn_edit(idx,before,g_spawns.points[idx]);
    spawns_save(&g_spawns);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_set_model_idx(int idx,int model_idx,EditorCommandResult *out){
    SpawnPoint before;
    int clamped_model=(g_enemy_model_count>0)
        ?((model_idx<0)?0:((model_idx>g_enemy_model_count)?g_enemy_model_count:model_idx))
        :0;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_SET_MODEL_IDX);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    if(g_spawns.points[idx].model_idx==clamped_model) return false;
    before=g_spawns.points[idx];
    g_spawns.points[idx].model_idx=clamped_model;
    undo_push_spawn_edit(idx,before,g_spawns.points[idx]);
    spawns_save(&g_spawns);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_set_scale_override(int idx,float scale_over,EditorCommandResult *out){
    SpawnPoint before;
    float next_scale=(scale_over<0.0f)?-1.0f:clampf(scale_over,0.10f,5.0f);
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_SET_SCALE_OVERRIDE);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    if(fabsf(g_spawns.points[idx].scale_over-next_scale)<=0.0001f) return false;
    before=g_spawns.points[idx];
    g_spawns.points[idx].scale_over=next_scale;
    undo_push_spawn_edit(idx,before,g_spawns.points[idx]);
    spawns_save(&g_spawns);
    if(out){
        out->changed=true;
        out->primary_index=idx;
    }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_set_hp_override(int idx,float hp_over,EditorCommandResult *out){
    SpawnPoint before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_SET_HP_OVERRIDE);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    if(fabsf(g_spawns.points[idx].hp_over-hp_over)<=0.0001f) return false;
    before=g_spawns.points[idx];
    g_spawns.points[idx].hp_over=hp_over;
    undo_push_spawn_edit(idx,before,g_spawns.points[idx]);
    spawns_save(&g_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_set_damage_override(int idx,float dmg_over,EditorCommandResult *out){
    SpawnPoint before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_SET_DAMAGE_OVERRIDE);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    if(fabsf(g_spawns.points[idx].dmg_over-dmg_over)<=0.0001f) return false;
    before=g_spawns.points[idx];
    g_spawns.points[idx].dmg_over=dmg_over;
    undo_push_spawn_edit(idx,before,g_spawns.points[idx]);
    spawns_save(&g_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_set_speed_override(int idx,float speed_over,EditorCommandResult *out){
    SpawnPoint before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_SET_SPEED_OVERRIDE);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    if(fabsf(g_spawns.points[idx].speed_over-speed_over)<=0.0001f) return false;
    before=g_spawns.points[idx];
    g_spawns.points[idx].speed_over=speed_over;
    undo_push_spawn_edit(idx,before,g_spawns.points[idx]);
    spawns_save(&g_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_set_chase_radius_override(int idx,float chase_r_over,EditorCommandResult *out){
    SpawnPoint before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_SET_CHASE_RADIUS_OVERRIDE);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    if(fabsf(g_spawns.points[idx].chase_r_over-chase_r_over)<=0.0001f) return false;
    before=g_spawns.points[idx];
    g_spawns.points[idx].chase_r_over=chase_r_over;
    undo_push_spawn_edit(idx,before,g_spawns.points[idx]);
    spawns_save(&g_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_set_attack_radius_override(int idx,float atk_r_over,EditorCommandResult *out){
    SpawnPoint before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_SET_ATTACK_RADIUS_OVERRIDE);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    if(fabsf(g_spawns.points[idx].atk_r_over-atk_r_over)<=0.0001f) return false;
    before=g_spawns.points[idx];
    g_spawns.points[idx].atk_r_over=atk_r_over;
    undo_push_spawn_edit(idx,before,g_spawns.points[idx]);
    spawns_save(&g_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_spawn_set_attack_cooldown_override(int idx,float atk_cd_over,EditorCommandResult *out){
    SpawnPoint before;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_SPAWN,
                                EDITOR_COMMAND_KIND_SPAWN_SET_ATTACK_COOLDOWN_OVERRIDE);
    if(idx<0||idx>=g_spawns.count||!g_spawns.points[idx].active) return false;
    if(fabsf(g_spawns.points[idx].atk_cd_over-atk_cd_over)<=0.0001f) return false;
    before=g_spawns.points[idx];
    g_spawns.points[idx].atk_cd_over=atk_cd_over;
    undo_push_spawn_edit(idx,before,g_spawns.points[idx]);
    spawns_save(&g_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_create(Vec3 pos,float yaw,int model_idx,const char *name,
                                      NpcSystem *npcs_runtime,int *out_idx,EditorCommandResult *out){
    NpcSpawnPoint sp;
    UndoSnap snap;
    int idx;
    const char *npc_name=(name&&name[0])?name:"NPC";
    static Uint32 last_create_ticks=0;
    static Vec3 last_create_pos={0};
    static float last_create_yaw=0.0f;
    static int last_create_model_idx=-1;
    static bool last_create_mouse_press_consumed=false;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_CREATE);
    if(out_idx) *out_idx=-1;
    if(model_idx<0||model_idx>=NPC_MODEL_MAX||g_npc_spawns.count>=MAX_NPC_SPAWNS) return false;
    if(npcs_runtime&&npcs_runtime->count>=NPC_MAX) return false;
    {
        Uint32 mouse_buttons=SDL_GetMouseState(NULL,NULL);
        bool lmb_down=(mouse_buttons&SDL_BUTTON(SDL_BUTTON_LEFT))!=0;
        Uint32 now=SDL_GetTicks();
        if(!lmb_down) last_create_mouse_press_consumed=false;
        if(lmb_down&&last_create_mouse_press_consumed&&
           last_create_model_idx==model_idx&&
           fabsf(last_create_pos.x-pos.x)<=0.001f&&
           fabsf(last_create_pos.y-pos.y)<=0.001f&&
           fabsf(last_create_pos.z-pos.z)<=0.001f&&
           fabsf(last_create_yaw-yaw)<=0.001f){
            return false;
        }
        if(last_create_model_idx==model_idx&&
           (now-last_create_ticks)<=350u&&
           fabsf(last_create_pos.x-pos.x)<=0.001f&&
           fabsf(last_create_pos.y-pos.y)<=0.001f&&
           fabsf(last_create_pos.z-pos.z)<=0.001f&&
           fabsf(last_create_yaw-yaw)<=0.001f){
            return false;
        }
        if(lmb_down) last_create_mouse_press_consumed=true;
        last_create_ticks=now;
        last_create_pos=pos;
        last_create_yaw=yaw;
        last_create_model_idx=model_idx;
    }
    idx=g_npc_spawns.count;
    memset(&sp,0,sizeof(sp));
    sp.pos=pos;
    sp.yaw=yaw;
    sp.visual_yaw_offset=0.0f;
    sp.scale=1.0f;
    sp.collision_enabled=true;
    sp.patrol_loop=true;
    sp.model_idx=model_idx;
    sp.active=true;
    sp.patrol_speed=1.4f;
    sp.patrol_wait=2.0f;
    sp.tree_root=-1;
    strncpy(sp.name,npc_name,sizeof(sp.name)-1);
    g_npc_spawns.points[idx]=sp;
    g_npc_spawns.count++;
    if(npcs_runtime){
        int runtime_idx=npc_add(npcs_runtime,pos,yaw,npc_name);
        if(runtime_idx>=0){
            npcs_runtime->npcs[runtime_idx].model_idx=model_idx;
            npcs_runtime->npcs[runtime_idx].scale=sp.scale;
            npcs_runtime->npcs[runtime_idx].collision_enabled=sp.collision_enabled;
            npcs_runtime->npcs[runtime_idx].patrol_loop=sp.patrol_loop;
            npcs_runtime->npcs[runtime_idx].anim_idx=0;
        }
    }
    memset(&snap,0,sizeof(snap));
    snap.npc_spawn=sp;
    undo_push_place(UNDO_NPC_PLACE,idx,idx,snap);
    npc_spawns_save(&g_npc_spawns);
    if(out_idx) *out_idx=idx;
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static int editor_command_npc_runtime_index_for_spawn(int spawn_idx,const NpcSystem *npcs_runtime){
    if(!npcs_runtime||spawn_idx<0||spawn_idx>=g_npc_spawns.count) return -1;
    if(npcs_runtime->count==g_npc_spawns.count+1&&
       npcs_runtime->count>0&&
       strcmp(npcs_runtime->npcs[0].name,"Monitor")==0){
        int runtime_idx=spawn_idx+1;
        return runtime_idx<npcs_runtime->count?runtime_idx:-1;
    }
    return spawn_idx<npcs_runtime->count?spawn_idx:-1;
}

static void editor_command_npc_sync_runtime_from_spawn(int idx,NpcSystem *npcs_runtime){
    NpcDef *rt;
    int runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx<0) return;
    rt=&npcs_runtime->npcs[runtime_idx];
    rt->pos=g_npc_spawns.points[idx].pos;
    rt->pos.y=scene_ground_y(rt->pos.x,rt->pos.z)+0.05f;
    rt->yaw=g_npc_spawns.points[idx].yaw;
    rt->visual_yaw_offset=g_npc_spawns.points[idx].visual_yaw_offset;
    rt->scale=(g_npc_spawns.points[idx].scale>0.0f)?g_npc_spawns.points[idx].scale:1.0f;
    rt->collision_enabled=g_npc_spawns.points[idx].collision_enabled;
    rt->patrol_loop=g_npc_spawns.points[idx].patrol_loop;
    rt->model_idx=g_npc_spawns.points[idx].model_idx;
    rt->anim_idx=0;
    rt->is_merchant=g_npc_spawns.points[idx].is_merchant;
    rt->shop_id=g_npc_spawns.points[idx].shop_id;
    snprintf(rt->name,sizeof(rt->name),"%s",g_npc_spawns.points[idx].name);
    snprintf(rt->backstory,sizeof(rt->backstory),"%s",g_npc_spawns.points[idx].backstory);
    npc_patrol_clear(runtime_idx);
    if(g_npc_spawns.points[idx].wp_count>=2){
        float spd=g_npc_spawns.points[idx].patrol_speed>0?g_npc_spawns.points[idx].patrol_speed:1.4f;
        float wait=g_npc_spawns.points[idx].patrol_wait>0?g_npc_spawns.points[idx].patrol_wait:2.0f;
        npc_patrol_add(runtime_idx,spd,wait,g_npc_spawns.points[idx].patrol_loop);
        for(int wi=0;wi<g_npc_spawns.points[idx].wp_count;wi++)
            npc_patrol_waypoint(runtime_idx,g_npc_spawns.points[idx].waypoints[wi]);
    }
    rt->line_count=0;
    rt->dlg_root_id=-1;
    rt->dlg_cur_node=-1;
    if(g_npc_spawns.points[idx].tree_count<=0||g_npc_spawns.points[idx].tree_root<0){
        for(int di=0;di<g_npc_spawns.points[idx].dialogue_count;di++)
            npc_add_line(npcs_runtime,runtime_idx,g_npc_spawns.points[idx].dialogue[di]);
    }
}

static bool editor_command_npc_create_from_spawn(const NpcSpawnPoint *spawn,
                                                 NpcSystem *npcs_runtime,
                                                 int *out_idx,EditorCommandResult *out){
    UndoSnap snap;
    int idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_CREATE);
    if(out_idx) *out_idx=-1;
    if(!spawn||!spawn->active) return false;
    if(spawn->model_idx<0||spawn->model_idx>=NPC_MODEL_MAX||g_npc_spawns.count>=MAX_NPC_SPAWNS) return false;
    if(npcs_runtime&&npcs_runtime->count>=NPC_MAX) return false;
    idx=g_npc_spawns.count;
    g_npc_spawns.points[idx]=*spawn;
    g_npc_spawns.points[idx].active=true;
    g_npc_spawns.count++;
    if(npcs_runtime){
        int runtime_idx=npc_add(npcs_runtime,spawn->pos,spawn->yaw,spawn->name[0]?spawn->name:"NPC");
        if(runtime_idx>=0) editor_command_npc_sync_runtime_from_spawn(idx,npcs_runtime);
    }
    memset(&snap,0,sizeof(snap));
    snap.npc_spawn=g_npc_spawns.points[idx];
    undo_push_place(UNDO_NPC_PLACE,idx,idx,snap);
    npc_spawns_save(&g_npc_spawns);
    if(out_idx) *out_idx=idx;
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_delete(int idx,NpcSystem *npcs_runtime,
                                      int *selected_idx,
                                      bool *wp_mode,int *wp_npc,
                                      bool *dlg_mode,int *dlg_npc,int *dlg_sel,char *dlg_buf,int *dlg_len,
                                      bool *tree_mode,int *tree_npc,int *tree_sel,int *tree_choice,
                                      bool *tree_editing,int *tree_field,char *tree_buf,int *tree_len,
                                      EditorCommandResult *out){
    UndoSnap snap;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_DELETE);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    memset(&snap,0,sizeof(snap));
    snap.npc_spawn=g_npc_spawns.points[idx];
    undo_push_delete(UNDO_NPC_DELETE,idx,g_npc_spawns.count,snap);
    editor_npc_delete_spawn_at(&g_npc_spawns,idx,
                               selected_idx,
                               wp_mode,wp_npc,
                               dlg_mode,dlg_npc,dlg_sel,dlg_buf,dlg_len,
                               tree_mode,tree_npc,tree_sel,tree_choice,
                               tree_editing,tree_field,tree_buf,tree_len);
    editor_selection_after_compact_delete(EDITOR_SELECTION_NPC,idx,g_npc_spawns.count);
    if(npcs_runtime){
        int runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
        if(runtime_idx>=0){
            for(int i=runtime_idx;i<npcs_runtime->count-1;i++) npcs_runtime->npcs[i]=npcs_runtime->npcs[i+1];
            npcs_runtime->count--;
        }
    }
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_position(int idx,Vec3 pos,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_SET_POSITION);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(fabsf(g_npc_spawns.points[idx].pos.x-pos.x)<=0.0001f&&
       fabsf(g_npc_spawns.points[idx].pos.y-pos.y)<=0.0001f&&
       fabsf(g_npc_spawns.points[idx].pos.z-pos.z)<=0.0001f) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].pos=pos;
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0) npcs_runtime->npcs[runtime_idx].pos=pos;
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_yaw(int idx,float yaw,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_SET_YAW);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(fabsf(g_npc_spawns.points[idx].yaw-yaw)<=0.0001f) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].yaw=yaw;
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0) npcs_runtime->npcs[runtime_idx].yaw=yaw;
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_visual_yaw_offset(int idx,float yaw_offset,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_PATCH);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(fabsf(g_npc_spawns.points[idx].visual_yaw_offset-yaw_offset)<=0.0001f) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].visual_yaw_offset=yaw_offset;
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0) npcs_runtime->npcs[runtime_idx].visual_yaw_offset=yaw_offset;
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_model_idx(int idx,int model_idx,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_SET_MODEL_IDX);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(model_idx<0||model_idx>=g_npc_model_count) return false;
    if(g_npc_spawns.points[idx].model_idx==model_idx) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].model_idx=model_idx;
    if(g_npc_spawns.points[idx].name[0]==0) snprintf(g_npc_spawns.points[idx].name,sizeof(g_npc_spawns.points[idx].name),"%s",NPC_TYPE_NAMES[model_idx]);
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0) npcs_runtime->npcs[runtime_idx].model_idx=model_idx;
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_scale(int idx,float scale,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    float next_scale;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_PATCH);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    next_scale=clampf(scale,0.01f,10.0f);
    if(fabsf(g_npc_spawns.points[idx].scale-next_scale)<=0.0001f) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].scale=next_scale;
    if(editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime)>=0)
        editor_command_npc_sync_runtime_from_spawn(idx,npcs_runtime);
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_collision_enabled(int idx,bool enabled,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_PATCH);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(g_npc_spawns.points[idx].collision_enabled==enabled) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].collision_enabled=enabled;
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0) npcs_runtime->npcs[runtime_idx].collision_enabled=enabled;
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_patrol_loop(int idx,bool enabled,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_PATCH);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(g_npc_spawns.points[idx].patrol_loop==enabled) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].patrol_loop=enabled;
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0){
        npcs_runtime->npcs[runtime_idx].patrol_loop=enabled;
        editor_command_npc_sync_runtime_from_spawn(idx,npcs_runtime);
    }
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_patrol_speed(int idx,float speed,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    float next_speed=clampf(speed,0.2f,6.0f);
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_PATCH);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(fabsf(g_npc_spawns.points[idx].patrol_speed-next_speed)<=0.0001f) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].patrol_speed=next_speed;
    if(editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime)>=0) editor_command_npc_sync_runtime_from_spawn(idx,npcs_runtime);
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_patrol_wait(int idx,float wait_time,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    float next_wait=clampf(wait_time,0.0f,15.0f);
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_PATCH);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(fabsf(g_npc_spawns.points[idx].patrol_wait-next_wait)<=0.0001f) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].patrol_wait=next_wait;
    if(editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime)>=0) editor_command_npc_sync_runtime_from_spawn(idx,npcs_runtime);
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_merchant_enabled(int idx,bool enabled,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_PATCH);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(g_npc_spawns.points[idx].is_merchant==enabled) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].is_merchant=enabled;
    if(!enabled) g_npc_spawns.points[idx].shop_id=0;
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0){
        npcs_runtime->npcs[runtime_idx].is_merchant=enabled;
        if(!enabled) npcs_runtime->npcs[runtime_idx].shop_id=0;
    }
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_shop_id(int idx,int shop_id,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    int next_shop=(shop_id<0)?0:(shop_id%SHOP_PROFILE_COUNT);
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_PATCH);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    if(!g_npc_spawns.points[idx].is_merchant) return false;
    if(g_npc_spawns.points[idx].shop_id==next_shop) return false;
    before=g_npc_spawns.points[idx];
    g_npc_spawns.points[idx].shop_id=next_shop;
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0) npcs_runtime->npcs[runtime_idx].shop_id=next_shop;
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_name(int idx,const char *name,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    char normalized[sizeof(g_npc_spawns.points[0].name)];
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_SET_NAME);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    snprintf(normalized,sizeof(normalized),"%s",(name&&name[0])?name:"NPC");
    if(strcmp(g_npc_spawns.points[idx].name,normalized)==0) return false;
    before=g_npc_spawns.points[idx];
    snprintf(g_npc_spawns.points[idx].name,sizeof(g_npc_spawns.points[idx].name),"%s",normalized);
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0){
        strncpy(npcs_runtime->npcs[runtime_idx].name,normalized,sizeof(npcs_runtime->npcs[runtime_idx].name)-1);
        npcs_runtime->npcs[runtime_idx].name[sizeof(npcs_runtime->npcs[runtime_idx].name)-1]=0;
    }
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_set_backstory(int idx,const char *backstory,NpcSystem *npcs_runtime,EditorCommandResult *out){
    NpcSpawnPoint before;
    char normalized[sizeof(g_npc_spawns.points[0].backstory)];
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,
                                EDITOR_COMMAND_KIND_NPC_SET_BACKSTORY);
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    snprintf(normalized,sizeof(normalized),"%s",backstory?backstory:"");
    if(strcmp(g_npc_spawns.points[idx].backstory,normalized)==0) return false;
    before=g_npc_spawns.points[idx];
    snprintf(g_npc_spawns.points[idx].backstory,sizeof(g_npc_spawns.points[idx].backstory),"%s",normalized);
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0){
        strncpy(npcs_runtime->npcs[runtime_idx].backstory,normalized,sizeof(npcs_runtime->npcs[runtime_idx].backstory)-1);
        npcs_runtime->npcs[runtime_idx].backstory[sizeof(npcs_runtime->npcs[runtime_idx].backstory)-1]=0;
    }
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}

static bool editor_command_npc_patch(int idx,const NpcSpawnPoint *updated,NpcSystem *npcs_runtime,
                                     EditorCommandKind kind,EditorCommandResult *out){
    NpcSpawnPoint before;
    bool patrol_changed;
    bool dialogue_changed;
    bool tree_changed;
    NpcDef *rt=NULL;
    int runtime_idx;
    editor_command_result_reset(out,EDITOR_COMMAND_DOMAIN_NPC,kind);
    if(!updated) return false;
    if(idx<0||idx>=g_npc_spawns.count||!g_npc_spawns.points[idx].active) return false;
    before=g_npc_spawns.points[idx];
    patrol_changed=(before.wp_count!=updated->wp_count)||
                   (fabsf(before.patrol_speed-updated->patrol_speed)>0.0001f)||
                   (fabsf(before.patrol_wait-updated->patrol_wait)>0.0001f)||
                   (before.patrol_loop!=updated->patrol_loop)||
                   (memcmp(before.waypoints,updated->waypoints,sizeof(before.waypoints))!=0);
    dialogue_changed=(before.dialogue_count!=updated->dialogue_count)||
                     (memcmp(before.dialogue,updated->dialogue,sizeof(before.dialogue))!=0);
    tree_changed=(before.tree_count!=updated->tree_count)||
                 (before.tree_root!=updated->tree_root)||
                 (memcmp(before.tree,updated->tree,sizeof(before.tree))!=0)||
                 (memcmp(before.node_canvas_x,updated->node_canvas_x,sizeof(before.node_canvas_x))!=0)||
                 (memcmp(before.node_canvas_y,updated->node_canvas_y,sizeof(before.node_canvas_y))!=0);
    if(memcmp(&before,updated,sizeof(before))==0) return false;
    g_npc_spawns.points[idx]=*updated;
    g_npc_spawns.points[idx].active=true;
    runtime_idx=editor_command_npc_runtime_index_for_spawn(idx,npcs_runtime);
    if(runtime_idx>=0){
        rt=&npcs_runtime->npcs[runtime_idx];
        if(patrol_changed){
            editor_command_npc_sync_runtime_from_spawn(idx,npcs_runtime);
        }else{
            rt->pos=g_npc_spawns.points[idx].pos;
            rt->pos.y=scene_ground_y(rt->pos.x,rt->pos.z)+0.05f;
            rt->yaw=g_npc_spawns.points[idx].yaw;
            rt->visual_yaw_offset=g_npc_spawns.points[idx].visual_yaw_offset;
            rt->scale=(g_npc_spawns.points[idx].scale>0.0f)?g_npc_spawns.points[idx].scale:1.0f;
            rt->collision_enabled=g_npc_spawns.points[idx].collision_enabled;
            rt->patrol_loop=g_npc_spawns.points[idx].patrol_loop;
            rt->model_idx=g_npc_spawns.points[idx].model_idx;
            rt->is_merchant=g_npc_spawns.points[idx].is_merchant;
            rt->shop_id=g_npc_spawns.points[idx].shop_id;
            snprintf(rt->name,sizeof(rt->name),"%s",g_npc_spawns.points[idx].name);
            snprintf(rt->backstory,sizeof(rt->backstory),"%s",g_npc_spawns.points[idx].backstory);
        }
        if(dialogue_changed&&!(g_npc_spawns.points[idx].tree_count>0&&g_npc_spawns.points[idx].tree_root>=0)){
            rt->line_count=0;
            rt->dlg_root_id=-1;
            rt->dlg_cur_node=-1;
            for(int di=0;di<g_npc_spawns.points[idx].dialogue_count;di++)
                npc_add_line(npcs_runtime,runtime_idx,g_npc_spawns.points[idx].dialogue[di]);
        }else if(tree_changed){
            rt->dlg_cur_node=-1;
        }
    }
    undo_push_npc_edit(idx,before,g_npc_spawns.points[idx]);
    npc_spawns_save(&g_npc_spawns);
    if(out){ out->changed=true; out->primary_index=idx; }
    editor_command_runtime_note_change(editor_commands_runtime());
    return true;
}
