/* ═══════════════════════════════════════════════════════════════
   GLOBAL UNDO / REDO  — Ctrl+Z / Ctrl+Shift+Z
   Covers: prop transform, prop place/delete, surfaces, enemy spawns,
           NPCs, world markers, point lights, decals, sound zones.
   ═══════════════════════════════════════════════════════════════ */

/* Forward declarations for functions defined after this include point */
static void npc_spawns_save(NpcSpawnList *sl);
static bool surface_piece_snapshot(int idx, void *out_snap);
static bool surface_piece_restore_from_snap(int idx, const void *snap, bool save_now);
static bool surface_piece_deactivate_at(int idx, bool save_now);
static void world_fx_delete(WorldFxSystem *sys,LightingSystem *ls,int idx);
static bool world_fx_restore_at(WorldFxSystem *sys,LightingSystem *ls,int idx,const WorldFx *snap);
static bool wzone_restore_at(int idx,const WaterZone *snap,Terrain *terrain,bool save_now);
static bool wzone_delete_at(int idx,Terrain *terrain,bool save_now);
static bool windzone_restore_at(int idx,const WindZone *snap,bool save_now);
static bool windzone_delete_at(int idx,bool save_now);

/* Pointer to the game-loop LightingSystem (set each frame before undo calls) */
static LightingSystem *g_undo_ls = NULL;
static Terrain        *g_undo_terrain = NULL;

#define UNDO_STACK_SIZE 64

typedef enum {
    UNDO_NONE = 0,
    UNDO_TRANSACTION,
    UNDO_PROP_XFORM,     /* move / rotate / scale / ground */
    UNDO_PROP_PLACE,
    UNDO_PROP_DELETE,
    UNDO_SURFACE_PLACE,
    UNDO_SURFACE_DELETE,
    UNDO_SURFACE_EDIT,
    UNDO_SPAWN_PLACE,
    UNDO_SPAWN_DELETE,
    UNDO_SPAWN_EDIT,
    UNDO_NPC_PLACE,
    UNDO_NPC_DELETE,
    UNDO_NPC_EDIT,
    UNDO_MARKER_PLACE,
    UNDO_MARKER_DELETE,
    UNDO_MARKER_EDIT,
    UNDO_PLIGHT_PLACE,
    UNDO_PLIGHT_DELETE,
    UNDO_PLIGHT_EDIT,
    UNDO_WFX_PLACE,
    UNDO_WFX_EDIT,
    UNDO_WFX_DELETE,
    UNDO_WZONE_PLACE,
    UNDO_WZONE_DELETE,
    UNDO_WZONE_EDIT,
    UNDO_WIND_PLACE,
    UNDO_WIND_DELETE,
    UNDO_WIND_EDIT,
    UNDO_DECAL_PLACE,
    UNDO_DECAL_DELETE,
    UNDO_SZONE_PLACE,
    UNDO_SZONE_DELETE,
} UndoKind;

/* Prop snapshot (transform + path for re-registration) */
typedef struct {
    int   uid;
    Vec3  pos;
    float rot_x, rot_y, rot_z, scale;
    PropCollType coll_type;
    bool  custom_coll_enabled;
    int   custom_coll_count;
    PropCustomCollisionBox custom_coll[PROP_CUSTOM_COLL_MAX];
    PropInteractType interact_type;
    float interact_radius;
    float door_open_angle;
    float door_closed_yaw;
    bool  door_is_open;
    bool  pickable;
    ItemType pickup_item;
    int   pickup_count;
    int   action_count;
    int   action_selected;
    PropAction actions[PROP_ACTION_MAX];
    char  path[256];
    bool  active;
} PropSnap;

typedef struct {
    bool active;
    int kind;
    Vec3 pos;
    float yaw_deg;
    float width;
    float height;
    float thickness;
    PropCollType coll_type;
    int material_id;
} SurfaceSnap;

typedef union {
    PropSnap    prop;
    SurfaceSnap surface;
    SpawnPoint  spawn;
    NpcSpawnPoint npc_spawn;
    WorldMarker marker;
    PlacedLight plight;
    WorldFx     wfx;
    WaterZone   wzone;
    WindZone    wind;
    WorldDecal  decal;
    SoundZone   szone;
} UndoSnap;

typedef struct {
    UndoKind kind;
    int      item_idx;      /* index in the relevant array */
    int      count_before;  /* list count before this operation */
    int      transaction_size;
    UndoSnap before;        /* state before the action (apply on undo) */
    UndoSnap after;         /* state after the action  (apply on redo) */
    char     label[64];
} UndoEntry;

static UndoEntry g_undo_stack[UNDO_STACK_SIZE];
static int       g_undo_top = 0;   /* number of valid undo entries */
static UndoEntry g_redo_stack[UNDO_STACK_SIZE];
static int       g_redo_top = 0;   /* number of valid redo entries */
static int       g_undo_transaction_depth = 0;
static int       g_undo_transaction_begin_top = 0;
static char      g_undo_transaction_label[64];

/* ── Internal helpers ─────────────────────────────────────────── */

static PropSnap undo_snap_prop(int idx) {
    PropSnap s; memset(&s, 0, sizeof(s));
    if(idx < 0 || idx >= g_props.count) return s;
    Prop *p = &g_props.list[idx];
    s.uid = p->uid;
    s.pos = p->pos;
    s.rot_x = p->rot_x; s.rot_y = p->rot_y; s.rot_z = p->rot_z;
    s.scale = p->scale;
    s.coll_type = p->coll_type;
    s.custom_coll_enabled = p->custom_coll_enabled;
    s.custom_coll_count = p->custom_coll_count;
    memcpy(s.custom_coll,p->custom_coll,sizeof(s.custom_coll));
    s.interact_type = p->interact_type;
    s.interact_radius = p->interact_radius;
    s.door_open_angle = p->door_open_angle;
    s.door_closed_yaw = p->door_closed_yaw;
    s.door_is_open = p->door_is_open;
    s.pickable = p->pickable;
    s.pickup_item = p->pickup_item;
    s.pickup_count = p->pickup_count;
    s.action_count = p->action_count;
    s.action_selected = p->action_selected;
    memcpy(s.actions,p->actions,sizeof(s.actions));
    strncpy(s.path, p->path, 255);
    s.active = p->active;
    return s;
}

static void undo_restore_prop_xform(int idx, const PropSnap *s) {
    bool reload_asset;
    if(idx < 0 || idx >= g_props.count) return;
    Prop *p = &g_props.list[idx];
    reload_asset = strcmp(p->path,s->path)!=0 || p->active!=s->active;
    if(reload_asset) {
        prop_release_all(p);
        memset(p,0,sizeof(*p));
        snprintf(p->path,sizeof(p->path),"%s",s->path);
        p->stream_state=PROP_UNLOADED;
    }
    p->uid   = s->uid;
    world_observe_prop_uid(p->uid);
    p->pos   = s->pos;
    p->rot_x = s->rot_x; p->rot_y = s->rot_y; p->rot_z = s->rot_z;
    p->scale = s->scale;
    p->coll_type = s->coll_type;
    p->custom_coll_enabled = s->custom_coll_enabled;
    p->custom_coll_count = s->custom_coll_count;
    memcpy(p->custom_coll,s->custom_coll,sizeof(p->custom_coll));
    p->interact_type = s->interact_type;
    p->interact_radius = s->interact_radius;
    p->door_open_angle = s->door_open_angle;
    p->door_closed_yaw = s->door_closed_yaw;
    p->door_is_open = s->door_is_open;
    p->pickable = s->pickable;
    p->pickup_item = s->pickup_item;
    p->pickup_count = s->pickup_count;
    p->action_count = s->action_count;
    p->action_selected = s->action_selected;
    memcpy(p->actions,s->actions,sizeof(p->actions));
    if(p->custom_coll_selected>=p->custom_coll_count) p->custom_coll_selected=p->custom_coll_count-1;
    if(p->custom_coll_selected<0) p->custom_coll_selected=0;
    p->active = s->active;
    if(reload_asset&&p->active)
        stream_request(&g_stream,idx);
    else
        prop_refresh_collision(p);
}

static void spawn_remove_at(int idx) {
    if(idx < 0 || idx >= g_spawns.count) return;
    for(int i = idx; i < g_spawns.count - 1; i++)
        g_spawns.points[i] = g_spawns.points[i + 1];
    g_spawns.count--;
}

static bool spawn_insert_at(int idx, const SpawnPoint *sp) {
    if(!sp || g_spawns.count >= MAX_SPAWN_POINTS) return false;
    if(idx < 0) idx = 0;
    if(idx > g_spawns.count) idx = g_spawns.count;
    for(int i = g_spawns.count; i > idx; i--)
        g_spawns.points[i] = g_spawns.points[i - 1];
    g_spawns.points[idx] = *sp;
    g_spawns.points[idx].active = true;
    g_spawns.count++;
    return true;
}

static void npc_remove_at(int idx) {
    if(idx < 0 || idx >= g_npc_spawns.count) return;
    for(int i = idx; i < g_npc_spawns.count - 1; i++)
        g_npc_spawns.points[i] = g_npc_spawns.points[i + 1];
    g_npc_spawns.count--;
}

static bool npc_insert_at(int idx, const NpcSpawnPoint *sp) {
    if(!sp || g_npc_spawns.count >= MAX_NPC_SPAWNS) return false;
    if(idx < 0) idx = 0;
    if(idx > g_npc_spawns.count) idx = g_npc_spawns.count;
    for(int i = g_npc_spawns.count; i > idx; i--)
        g_npc_spawns.points[i] = g_npc_spawns.points[i - 1];
    g_npc_spawns.points[idx] = *sp;
    g_npc_spawns.points[idx].active = true;
    g_npc_spawns.count++;
    return true;
}

/* ── Public push API ──────────────────────────────────────────── */

static void undo_stack_push(UndoEntry *stack,int *top,UndoEntry e) {
    if(!stack||!top) return;
    if(*top < UNDO_STACK_SIZE) {
        stack[(*top)++] = e;
    } else {
        memmove(&stack[0], &stack[1], (UNDO_STACK_SIZE-1)*sizeof(UndoEntry));
        stack[UNDO_STACK_SIZE-1] = e;
    }
}

static void undo_push(UndoEntry e) {
    /* clear redo stack whenever a new action is recorded */
    g_redo_top = 0;
    undo_stack_push(g_undo_stack,&g_undo_top,e);
}

static void undo_begin_transaction(const char *label) {
    g_undo_transaction_depth++;
    if(g_undo_transaction_depth == 1) {
        g_undo_transaction_begin_top = g_undo_top;
        if(label) {
            strncpy(g_undo_transaction_label,label,sizeof(g_undo_transaction_label)-1);
            g_undo_transaction_label[sizeof(g_undo_transaction_label)-1] = 0;
        } else {
            g_undo_transaction_label[0] = 0;
        }
    }
}

static bool undo_end_transaction(void) {
    UndoEntry e;
    int transaction_size;
    if(g_undo_transaction_depth <= 0) return false;
    g_undo_transaction_depth--;
    if(g_undo_transaction_depth > 0) return false;
    transaction_size = g_undo_top - g_undo_transaction_begin_top;
    if(transaction_size <= 0) {
        g_undo_transaction_label[0] = 0;
        return false;
    }
    memset(&e,0,sizeof(e));
    e.kind = UNDO_TRANSACTION;
    e.transaction_size = transaction_size;
    strncpy(e.label,g_undo_transaction_label,sizeof(e.label)-1);
    undo_push(e);
    g_undo_transaction_label[0] = 0;
    return true;
}

/* Prop transform: capture before THEN after the action */
static void undo_push_prop_xform(int idx, PropSnap before, PropSnap after) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = UNDO_PROP_XFORM;
    e.item_idx = idx;
    e.before.prop = before;
    e.after.prop  = after;
    undo_push(e);
}

static void undo_push_surface_edit(int idx, SurfaceSnap before, SurfaceSnap after) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = UNDO_SURFACE_EDIT;
    e.item_idx = idx;
    e.before.surface = before;
    e.after.surface = after;
    undo_push(e);
}

/* Generic place: snap = the placed item (state after placing) */
static void undo_push_place(UndoKind kind, int idx, int count_before, UndoSnap after_snap) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = kind;
    e.item_idx = idx;
    e.count_before = count_before;
    e.after = after_snap;
    undo_push(e);
}

/* Generic delete: snap = the deleted item (state before deletion) */
static void undo_push_delete(UndoKind kind, int idx, int count_before, UndoSnap before_snap) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = kind;
    e.item_idx = idx;
    e.count_before = count_before;
    e.before = before_snap;
    undo_push(e);
}

static void undo_push_marker_edit(int idx, WorldMarker before, WorldMarker after) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = UNDO_MARKER_EDIT;
    e.item_idx = idx;
    e.before.marker = before;
    e.after.marker = after;
    undo_push(e);
}

static void undo_push_spawn_edit(int idx, SpawnPoint before, SpawnPoint after) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = UNDO_SPAWN_EDIT;
    e.item_idx = idx;
    e.before.spawn = before;
    e.after.spawn = after;
    undo_push(e);
}

static void undo_push_npc_edit(int idx, NpcSpawnPoint before, NpcSpawnPoint after) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = UNDO_NPC_EDIT;
    e.item_idx = idx;
    e.before.npc_spawn = before;
    e.after.npc_spawn = after;
    undo_push(e);
}

static void undo_push_plight_edit(int idx, PlacedLight before, PlacedLight after) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = UNDO_PLIGHT_EDIT;
    e.item_idx = idx;
    e.before.plight = before;
    e.after.plight = after;
    undo_push(e);
}

static void undo_push_wfx_edit(int idx, WorldFx before, WorldFx after) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = UNDO_WFX_EDIT;
    e.item_idx = idx;
    e.before.wfx = before;
    e.after.wfx = after;
    undo_push(e);
}

static void undo_push_wzone_edit(int idx, WaterZone before, WaterZone after) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = UNDO_WZONE_EDIT;
    e.item_idx = idx;
    e.before.wzone = before;
    e.after.wzone = after;
    undo_push(e);
}

static void undo_push_wind_edit(int idx, WindZone before, WindZone after) {
    UndoEntry e; memset(&e, 0, sizeof(e));
    e.kind = UNDO_WIND_EDIT;
    e.item_idx = idx;
    e.before.wind = before;
    e.after.wind = after;
    undo_push(e);
}

/* ── Apply undo/redo entry ────────────────────────────────────── */

static void undo_apply_entry(const UndoEntry *e, bool is_undo) {
    const UndoSnap *snap = is_undo ? &e->before : &e->after;
    /* For PLACE: undo = remove; redo = add back */
    /* For DELETE: undo = add back; redo = remove */
    bool is_place  = (e->kind==UNDO_PROP_PLACE||e->kind==UNDO_SPAWN_PLACE||
                      e->kind==UNDO_SURFACE_PLACE||
                      e->kind==UNDO_NPC_PLACE||e->kind==UNDO_MARKER_PLACE||
                      e->kind==UNDO_PLIGHT_PLACE||e->kind==UNDO_WFX_PLACE||
                      e->kind==UNDO_WZONE_PLACE||e->kind==UNDO_WIND_PLACE||
                      e->kind==UNDO_DECAL_PLACE||
                      e->kind==UNDO_SZONE_PLACE);
    bool is_delete = (e->kind==UNDO_PROP_DELETE||e->kind==UNDO_SPAWN_DELETE||
                      e->kind==UNDO_SURFACE_DELETE||
                      e->kind==UNDO_NPC_DELETE||e->kind==UNDO_MARKER_DELETE||
                      e->kind==UNDO_PLIGHT_DELETE||e->kind==UNDO_WFX_DELETE||
                      e->kind==UNDO_WZONE_DELETE||e->kind==UNDO_WIND_DELETE||
                      e->kind==UNDO_DECAL_DELETE||
                      e->kind==UNDO_SZONE_DELETE);
    bool want_remove = (is_place && is_undo) || (is_delete && !is_undo);
    bool want_restore = (is_place && !is_undo) || (is_delete && is_undo);

    switch(e->kind) {
        case UNDO_TRANSACTION:
            break;
        /* ── Prop transform ───────────────────────── */
        case UNDO_PROP_XFORM:
            undo_restore_prop_xform(e->item_idx, &snap->prop);
            printf("[Undo] Prop %d transform %s\n", e->item_idx, is_undo?"restored":"redone");
            break;

        /* ── Prop place / delete ──────────────────── */
        case UNDO_PROP_PLACE:
        case UNDO_PROP_DELETE:
            if(want_remove) {
                /* remove: use the stored index */
                int idx = e->item_idx;
                if(idx>=0 && idx<g_props.count) {
                    prop_release_all(&g_props.list[idx]);
                    printf("[Undo] Prop %d removed\n", idx);
                }
            } else if(want_restore) {
                /* restore: re-register with saved path/transform */
                const PropSnap *ps = (is_undo) ? &e->after.prop : &e->before.prop;
                /* pick the slot by idx if possible, else next */
                int idx = e->item_idx;
                if(idx>=0 && idx<MAX_PROPS && !g_props.list[idx].active) {
                    /* slot is free, force-register into it */
                    Prop *p = &g_props.list[idx];
                    memset(p, 0, sizeof(*p));
                    p->uid = ps->uid;
                    world_observe_prop_uid(p->uid);
                    snprintf(p->path,sizeof(p->path),"%s",ps->path);
                    p->pos   = ps->pos;
                    p->rot_x = ps->rot_x; p->rot_y = ps->rot_y; p->rot_z = ps->rot_z;
                    p->scale = ps->scale;
                    p->coll_type = ps->coll_type;
                    p->custom_coll_enabled = ps->custom_coll_enabled;
                    p->custom_coll_count = ps->custom_coll_count;
                    memcpy(p->custom_coll,ps->custom_coll,sizeof(p->custom_coll));
                    p->interact_type = ps->interact_type;
                    p->interact_radius = ps->interact_radius;
                    p->door_open_angle = ps->door_open_angle;
                    p->action_count = ps->action_count;
                    p->action_selected = ps->action_selected;
                    memcpy(p->actions,ps->actions,sizeof(p->actions));
                    p->door_closed_yaw = ps->door_closed_yaw;
                    p->door_is_open = ps->door_is_open;
                    p->pickable = ps->pickable;
                    p->pickup_item = ps->pickup_item;
                    p->pickup_count = ps->pickup_count;
                    p->active = true;
                    stream_request(&g_stream, idx);
                    if(idx >= g_props.count) g_props.count = idx + 1;
                    printf("[Undo] Prop %d restored (%s)\n", idx, ps->path);
                } else {
                    /* fallback: use prop_register normally */
                    if(prop_register(&g_props, ps->path,
                                     ps->pos.x, ps->pos.y, ps->pos.z,
                                     ps->rot_x, ps->rot_y, ps->rot_z,
                                     ps->scale, ps->coll_type, 0,0,0,0)){
                        int ridx=g_props.count-1;
                        if(ridx>=0&&ridx<g_props.count){
                            g_props.list[ridx].custom_coll_enabled=ps->custom_coll_enabled;
                            g_props.list[ridx].custom_coll_count=ps->custom_coll_count;
                            memcpy(g_props.list[ridx].custom_coll,ps->custom_coll,sizeof(ps->custom_coll));
                            g_props.list[ridx].uid=ps->uid;
                            world_observe_prop_uid(g_props.list[ridx].uid);
                            g_props.list[ridx].interact_type=ps->interact_type;
                            g_props.list[ridx].interact_radius=ps->interact_radius;
                            g_props.list[ridx].door_open_angle=ps->door_open_angle;
                            g_props.list[ridx].action_count=ps->action_count;
                            g_props.list[ridx].action_selected=ps->action_selected;
                            memcpy(g_props.list[ridx].actions,ps->actions,sizeof(g_props.list[ridx].actions));
                            g_props.list[ridx].door_closed_yaw=ps->door_closed_yaw;
                            g_props.list[ridx].door_is_open=ps->door_is_open;
                            g_props.list[ridx].pickable=ps->pickable;
                            g_props.list[ridx].pickup_item=ps->pickup_item;
                            g_props.list[ridx].pickup_count=ps->pickup_count;
                            prop_refresh_collision(&g_props.list[ridx]);
                        }
                    }
                    printf("[Undo] Prop restored (fallback slot) (%s)\n", ps->path);
                }
            }
            break;

        /* ── Enemy spawn place / delete ──────────── */
        case UNDO_SURFACE_PLACE:
        case UNDO_SURFACE_DELETE:
            if(want_remove) {
                surface_piece_deactivate_at(e->item_idx, true);
                printf("[Undo] Surface %s removed\n", is_undo?"placement":"deletion redo");
            } else if(want_restore) {
                const SurfaceSnap *ss = is_undo ? &e->before.surface : &e->after.surface;
                if(surface_piece_restore_from_snap(e->item_idx, ss, true))
                    printf("[Undo] Surface %s restored\n", is_undo?"deletion":"placement redo");
            }
            break;

        case UNDO_SURFACE_EDIT:
            if(surface_piece_restore_from_snap(e->item_idx, &snap->surface, true))
                printf("[Undo] Surface %d edit %s\n", e->item_idx, is_undo?"restored":"redone");
            break;

        /* ── Enemy spawn place / delete ──────────── */
        case UNDO_SPAWN_PLACE:
            if(is_undo) {
                spawn_remove_at(e->item_idx);
                spawns_save(&g_spawns);
                printf("[Undo] Spawn placement removed\n");
            } else if(spawn_insert_at(e->item_idx, &e->after.spawn)) {
                spawns_save(&g_spawns);
                printf("[Undo] Spawn placement redone\n");
            }
            break;

        case UNDO_SPAWN_DELETE:
            if(is_undo) {
                if(spawn_insert_at(e->item_idx, &e->before.spawn)) {
                    spawns_save(&g_spawns);
                    printf("[Undo] Spawn deletion restored\n");
                }
            } else {
                spawn_remove_at(e->item_idx);
                spawns_save(&g_spawns);
                printf("[Undo] Spawn deletion redone\n");
            }
            break;

        case UNDO_SPAWN_EDIT:
            if(e->item_idx >= 0 && e->item_idx < g_spawns.count &&
               g_spawns.points[e->item_idx].active) {
                g_spawns.points[e->item_idx] = snap->spawn;
                spawns_save(&g_spawns);
                printf("[Undo] Spawn %d edit %s\n", e->item_idx, is_undo?"restored":"redone");
            }
            break;

        /* ── NPC place / delete ───────────────────── */
        case UNDO_NPC_PLACE:
            if(is_undo) {
                npc_remove_at(e->item_idx);
                npc_spawns_save(&g_npc_spawns);
                printf("[Undo] NPC placement removed\n");
            } else if(npc_insert_at(e->item_idx, &e->after.npc_spawn)) {
                npc_spawns_save(&g_npc_spawns);
                printf("[Undo] NPC placement redone\n");
            }
            break;

        case UNDO_NPC_DELETE:
            if(is_undo) {
                if(npc_insert_at(e->item_idx, &e->before.npc_spawn)) {
                    npc_spawns_save(&g_npc_spawns);
                    printf("[Undo] NPC deletion restored\n");
                }
            } else {
                npc_remove_at(e->item_idx);
                npc_spawns_save(&g_npc_spawns);
                printf("[Undo] NPC deletion redone\n");
            }
            break;

        case UNDO_NPC_EDIT:
            if(e->item_idx >= 0 && e->item_idx < g_npc_spawns.count &&
               g_npc_spawns.points[e->item_idx].active) {
                g_npc_spawns.points[e->item_idx] = snap->npc_spawn;
                npc_spawns_save(&g_npc_spawns);
                printf("[Undo] NPC %d edit %s\n", e->item_idx, is_undo?"restored":"redone");
            }
            break;

        /* ── World marker place / delete ─────────── */
        case UNDO_MARKER_PLACE:
        case UNDO_MARKER_DELETE:
            if(want_remove) {
                if(g_world_markers.count > e->count_before)
                    g_world_markers.count = e->count_before;
                world_markers_save(&g_world_markers);
                printf("[Undo] Marker placement removed\n");
            } else if(want_restore) {
                const WorldMarker *wm = is_undo ? &e->before.marker : &e->after.marker;
                if(g_world_markers.count < MAX_WORLD_MARKERS) {
                    g_world_markers.items[g_world_markers.count++] = *wm;
                    world_markers_save(&g_world_markers);
                    printf("[Undo] Marker deletion restored\n");
                }
            }
            break;

        case UNDO_MARKER_EDIT:
            if(e->item_idx >= 0 && e->item_idx < g_world_markers.count &&
               g_world_markers.items[e->item_idx].active) {
                g_world_markers.items[e->item_idx] = snap->marker;
                world_markers_save(&g_world_markers);
                printf("[Undo] Marker %d edit %s\n", e->item_idx, is_undo?"restored":"redone");
            }
            break;

        /* ── Point light place / delete ──────────── */
        case UNDO_PLIGHT_PLACE:
        case UNDO_PLIGHT_DELETE:
            if(want_remove) {
                /* deactivate the light at the placed index */
                int idx = e->item_idx;
                if(idx >= 0 && idx < g_placed_lights.count) {
                    if(g_undo_ls && g_placed_lights.items[idx].light_idx >= 0)
                        g_undo_ls->points[g_placed_lights.items[idx].light_idx].intensity = 0.0f;
                    g_placed_lights.items[idx].active = false;
                    if(g_placed_lights.count > e->count_before)
                        g_placed_lights.count = e->count_before;
                }
                placed_lights_save(&g_placed_lights);
                printf("[Undo] Point light placement removed\n");
            } else if(want_restore) {
                const PlacedLight *pl_snap = is_undo ? &e->before.plight : &e->after.plight;
                if(g_placed_lights.count < MAX_PLACED_LIGHTS && g_undo_ls) {
                    PlacedLight *pl = &g_placed_lights.items[g_placed_lights.count];
                    *pl = *pl_snap;
                    pl->light_idx=-1;
                    g_placed_lights.count++;
                    placed_light_runtime_sync(&g_placed_lights,g_undo_ls,g_placed_lights.count-1);
                    placed_lights_save(&g_placed_lights);
                    printf("[Undo] Point light deletion restored\n");
                }
            }
            break;

        case UNDO_PLIGHT_EDIT:
            if(e->item_idx >= 0 && e->item_idx < g_placed_lights.count &&
               g_placed_lights.items[e->item_idx].active) {
                g_placed_lights.items[e->item_idx] = is_undo ? e->before.plight : e->after.plight;
                if(g_undo_ls) placed_light_runtime_sync(&g_placed_lights,g_undo_ls,e->item_idx);
                placed_lights_save(&g_placed_lights);
                printf("[Undo] Point light %d edit %s\n", e->item_idx, is_undo?"restored":"redone");
            }
            break;

        /* ── World FX place / delete ─────────────── */
        case UNDO_WFX_PLACE:
        case UNDO_WFX_DELETE:
            if(want_remove) {
                world_fx_delete(&g_world_fx,g_undo_ls,e->item_idx);
                printf("[Undo] World FX %s removed\n", is_undo?"placement":"deletion redo");
            } else if(want_restore) {
                const WorldFx *fx = is_undo ? &e->before.wfx : &e->after.wfx;
                world_fx_restore_at(&g_world_fx,g_undo_ls,e->item_idx,fx);
                printf("[Undo] World FX %s restored\n", is_undo?"deletion":"placement redo");
            }
            break;

        case UNDO_WFX_EDIT:
            world_fx_restore_at(&g_world_fx,g_undo_ls,e->item_idx,&snap->wfx);
            printf("[Undo] World FX %d edit %s\n", e->item_idx, is_undo?"restored":"redone");
            break;

        case UNDO_WZONE_PLACE:
        case UNDO_WZONE_DELETE:
            if(want_remove) {
                wzone_delete_at(e->item_idx, NULL, true);
                printf("[Undo] Water zone %s removed\n", is_undo?"placement":"deletion redo");
            } else if(want_restore) {
                const WaterZone *z = is_undo ? &e->before.wzone : &e->after.wzone;
                wzone_restore_at(e->item_idx, z, NULL, true);
                printf("[Undo] Water zone %s restored\n", is_undo?"deletion":"placement redo");
            }
            break;

        case UNDO_WZONE_EDIT:
            wzone_restore_at(e->item_idx, &snap->wzone, NULL, true);
            printf("[Undo] Water zone %d edit %s\n", e->item_idx, is_undo?"restored":"redone");
            break;

        case UNDO_WIND_PLACE:
        case UNDO_WIND_DELETE:
            if(want_remove) {
                windzone_delete_at(e->item_idx, true);
                printf("[Undo] Wind zone %s removed\n", is_undo?"placement":"deletion redo");
            } else if(want_restore) {
                const WindZone *z = is_undo ? &e->before.wind : &e->after.wind;
                windzone_restore_at(e->item_idx, z, true);
                printf("[Undo] Wind zone %s restored\n", is_undo?"deletion":"placement redo");
            }
            break;

        case UNDO_WIND_EDIT:
            windzone_restore_at(e->item_idx, &snap->wind, true);
            printf("[Undo] Wind zone %d edit %s\n", e->item_idx, is_undo?"restored":"redone");
            break;

        /* ── Decal place / delete ─────────────────── */
        case UNDO_DECAL_PLACE:
        case UNDO_DECAL_DELETE:
            if(want_remove) {
                if(g_decals.count > e->count_before)
                    g_decals.count = e->count_before;
                g_decals.gpu_dirty = true;
                decals_rebuild_vbo(&g_decals);
                decals_save(&g_decals);
                printf("[Undo] Decal placement removed\n");
            } else if(want_restore) {
                const WorldDecal *wd = is_undo ? &e->before.decal : &e->after.decal;
                if(g_decals.count < MAX_DECALS) {
                    g_decals.items[g_decals.count++] = *wd;
                    g_decals.gpu_dirty = true;
                    decals_rebuild_vbo(&g_decals);
                    decals_save(&g_decals);
                    printf("[Undo] Decal deletion restored\n");
                }
            }
            break;

        /* ── Sound zone place / delete ───────────── */
        case UNDO_SZONE_PLACE:
        case UNDO_SZONE_DELETE:
            if(want_remove) {
                if(g_sound_zones.count > e->count_before)
                    g_sound_zones.count = e->count_before;
                sound_zones_save(&g_sound_zones);
                printf("[Undo] Sound zone placement removed\n");
            } else if(want_restore) {
                const SoundZone *sz = is_undo ? &e->before.szone : &e->after.szone;
                if(g_sound_zones.count < MAX_SOUND_ZONES) {
                    g_sound_zones.items[g_sound_zones.count++] = *sz;
                    sound_zones_save(&g_sound_zones);
                    printf("[Undo] Sound zone deletion restored\n");
                }
            }
            break;

        default: break;
    }
}

/* ── Main undo / redo entry points ───────────────────────────── */

static void undo_perform(void) {
    if(g_undo_top == 0) { printf("[Undo] Nothing to undo\n"); return; }
    UndoEntry e = g_undo_stack[--g_undo_top];
    if(e.kind == UNDO_TRANSACTION) {
        UndoEntry items[UNDO_STACK_SIZE];
        int actual = e.transaction_size;
        if(actual < 0) actual = 0;
        if(actual > g_undo_top) actual = g_undo_top;
        for(int i=0;i<actual;i++) {
            items[i] = g_undo_stack[--g_undo_top];
            undo_apply_entry(&items[i], true);
        }
        for(int i=actual-1;i>=0;i--)
            undo_stack_push(g_redo_stack,&g_redo_top,items[i]);
        undo_stack_push(g_redo_stack,&g_redo_top,e);
        printf("[Undo] Transaction '%s' undone (%d actions)\n",
               e.label[0]?e.label:"transaction",actual);
        return;
    }
    undo_apply_entry(&e, true);
    undo_stack_push(g_redo_stack,&g_redo_top,e);
}

static void redo_perform(void) {
    if(g_redo_top == 0) { printf("[Undo] Nothing to redo\n"); return; }
    UndoEntry e = g_redo_stack[--g_redo_top];
    if(e.kind == UNDO_TRANSACTION) {
        UndoEntry items[UNDO_STACK_SIZE];
        int actual = e.transaction_size;
        if(actual < 0) actual = 0;
        if(actual > g_redo_top) actual = g_redo_top;
        for(int i=0;i<actual;i++)
            items[i] = g_redo_stack[--g_redo_top];
        for(int i=actual-1;i>=0;i--) {
            undo_apply_entry(&items[i], false);
            undo_stack_push(g_undo_stack,&g_undo_top,items[i]);
        }
        undo_stack_push(g_undo_stack,&g_undo_top,e);
        printf("[Undo] Transaction '%s' redone (%d actions)\n",
               e.label[0]?e.label:"transaction",actual);
        return;
    }
    undo_apply_entry(&e, false);
    undo_stack_push(g_undo_stack,&g_undo_top,e);
}
