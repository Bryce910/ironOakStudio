/* ═══════════════════════════════════════════════════════════════
   PROP MULTI-SELECT + COPY / PASTE  (Shift+click, Ctrl+A, Ctrl+C, Ctrl+D)
   Drag on empty world space to box-select.
   Move mode moves the whole selection as a group.
   ═══════════════════════════════════════════════════════════════ */

/* ── Selection state ─────────────────────────────────────────── */
static bool g_prop_multi_sel[MAX_PROPS];
static int  g_prop_sel_count = 0;

/* Per-prop world positions at drag start — used for group move */
static Vec3 g_prop_drag_start_pos[MAX_PROPS];

/* ── Copy buffer ─────────────────────────────────────────────── */
#define PROP_COPY_MAX 256

typedef struct {
    char         path[256];
    Vec3         offset;      /* relative to copy-group center */
    float        rot_x, rot_y, rot_z, scale;
    PropCollType coll_type;
} PropCopyEntry;

static PropCopyEntry g_prop_copy_buf[PROP_COPY_MAX];
static int           g_prop_copy_count = 0;
static Vec3          g_prop_copy_center;

/* ── Selection helpers ───────────────────────────────────────── */

static void prop_multi_clear(void) {
    memset(g_prop_multi_sel, 0, sizeof(g_prop_multi_sel));
    g_prop_sel_count = 0;
}

static void prop_multi_add(int idx) {
    if(idx < 0 || idx >= MAX_PROPS || !g_props.list[idx].active) return;
    if(!g_prop_multi_sel[idx]) { g_prop_multi_sel[idx] = true; g_prop_sel_count++; }
}

static void prop_multi_toggle(int idx) {
    if(idx < 0 || idx >= MAX_PROPS || !g_props.list[idx].active) return;
    if(g_prop_multi_sel[idx]) { g_prop_multi_sel[idx] = false; g_prop_sel_count--; }
    else                      { g_prop_multi_sel[idx] = true;  g_prop_sel_count++; }
}

static bool prop_multi_any(void) { return g_prop_sel_count > 0; }
static int prop_multi_clipboard_count(void) { return g_prop_copy_count; }

static Vec3 prop_multi_center(void) {
    Vec3 c = {0,0,0}; int n = 0;
    for(int i = 0; i < g_props.count; i++) {
        if(g_prop_multi_sel[i] && g_props.list[i].active) {
            c.x += g_props.list[i].pos.x;
            c.y += g_props.list[i].pos.y;
            c.z += g_props.list[i].pos.z;
            n++;
        }
    }
    if(n > 0) { c.x /= (float)n; c.y /= (float)n; c.z /= (float)n; }
    return c;
}

static void prop_multi_add_all(void) {
    prop_multi_clear();
    for(int i = 0; i < g_props.count; i++)
        if(g_props.list[i].active) prop_multi_add(i);
    printf("[MultiSel] Selected all: %d props\n", g_prop_sel_count);
}

/* Box-select: add all props whose screen centers fall within screen rect */
static void prop_multi_box_select(int x0, int y0, int x1, int y1,
                                   Mat4 proj, Mat4 view, int win_w, int win_h,
                                   bool additive) {
    if(!additive) prop_multi_clear();
    float fx0 = (float)(x0 < x1 ? x0 : x1);
    float fx1 = (float)(x0 < x1 ? x1 : x0);
    float fy0 = (float)(y0 < y1 ? y0 : y1);
    float fy1 = (float)(y0 < y1 ? y1 : y0);
    if(fx1 - fx0 < 4.0f || fy1 - fy0 < 4.0f) return; /* too small */
    int added = 0;
    for(int i = 0; i < g_props.count; i++) {
        if(!g_props.list[i].active) continue;
        float nx, ny;
        if(!world_to_ndc(g_props.list[i].pos, proj, view, &nx, &ny)) continue;
        float sx = (nx * 0.5f + 0.5f) * (float)win_w;
        float sy = (-ny * 0.5f + 0.5f) * (float)win_h;
        if(sx >= fx0 && sx <= fx1 && sy >= fy0 && sy <= fy1) {
            prop_multi_add(i); added++;
        }
    }
    printf("[MultiSel] Box selected %d props\n", added);
}

/* ── Multi-move ──────────────────────────────────────────────── */

/* Call at drag start — snapshot positions of all selected props */
static void prop_multi_drag_begin(void) {
    for(int i = 0; i < g_props.count; i++) {
        if(g_prop_multi_sel[i] && g_props.list[i].active)
            g_prop_drag_start_pos[i] = g_props.list[i].pos;
    }
}

/* Call each move frame — moves all selected props by the same delta as primary */
static void prop_multi_drag_apply(int primary_idx, Vec3 new_primary_pos, bool snap_y) {
    if(primary_idx < 0 || primary_idx >= MAX_PROPS) return;
    Vec3 delta = {
        new_primary_pos.x - g_prop_drag_start_pos[primary_idx].x,
        0.0f,
        new_primary_pos.z - g_prop_drag_start_pos[primary_idx].z
    };
    for(int i = 0; i < g_props.count; i++) {
        if(i == primary_idx || !g_prop_multi_sel[i] || !g_props.list[i].active) continue;
        g_props.list[i].pos.x = g_prop_drag_start_pos[i].x + delta.x;
        g_props.list[i].pos.z = g_prop_drag_start_pos[i].z + delta.z;
        if(snap_y)
            g_props.list[i].pos.y = scene_ground_y(g_props.list[i].pos.x, g_props.list[i].pos.z);
        else
            g_props.list[i].pos.y = g_prop_drag_start_pos[i].y;
        prop_refresh_collision(&g_props.list[i]);
    }
}

/* ── Copy / Paste ────────────────────────────────────────────── */

static void prop_multi_copy(void) {
    g_prop_copy_count = 0;
    if(g_prop_sel_count == 0) return;
    g_prop_copy_center = prop_multi_center();
    for(int i = 0; i < g_props.count && g_prop_copy_count < PROP_COPY_MAX; i++) {
        if(!g_prop_multi_sel[i] || !g_props.list[i].active) continue;
        Prop *p = &g_props.list[i];
        PropCopyEntry *e = &g_prop_copy_buf[g_prop_copy_count++];
        strncpy(e->path, p->path, 255);
        e->offset.x = p->pos.x - g_prop_copy_center.x;
        e->offset.y = p->pos.y - g_prop_copy_center.y;
        e->offset.z = p->pos.z - g_prop_copy_center.z;
        e->rot_x = p->rot_x; e->rot_y = p->rot_y; e->rot_z = p->rot_z;
        e->scale = p->scale; e->coll_type = p->coll_type;
    }
    printf("[MultiSel] Copied %d props to clipboard\n", g_prop_copy_count);
}

/* Paste copy buffer at world center, new props become the selection */
static void prop_multi_paste(Vec3 center) {
    if(g_prop_copy_count == 0) return;
    prop_multi_clear();
    int placed = 0;
    for(int i = 0; i < g_prop_copy_count; i++) {
        PropCopyEntry *e = &g_prop_copy_buf[i];
        float wx = center.x + e->offset.x;
        float wz = center.z + e->offset.z;
        float wy = scene_ground_y(wx, wz) + e->offset.y;
        int idx = props_next_slot(&g_props);
        if(prop_register(&g_props, e->path, wx, wy, wz,
                          e->rot_x, e->rot_y, e->rot_z,
                          e->scale, e->coll_type, 0, 0, 0, 0)) {
            stream_request(&g_stream, idx);
            prop_multi_add(idx);
            UndoSnap _s; memset(&_s, 0, sizeof(_s)); _s.prop = undo_snap_prop(idx);
            undo_push_place(UNDO_PROP_PLACE, idx, idx, _s);
            placed++;
        }
    }
    placer_save(&g_props);
    printf("[MultiSel] Pasted %d props\n", placed);
}

/* Duplicate: paste at offset (+3 on X and Z) from copy center */
static void prop_multi_duplicate(void) {
    if(g_prop_sel_count == 0) return;
    prop_multi_copy();
    Vec3 center = g_prop_copy_center;
    center.x += 3.0f; center.z += 3.0f;
    prop_multi_paste(center);
    printf("[MultiSel] Duplicated %d props\n", g_prop_copy_count);
}

/* Delete all selected props with undo support */
static void prop_multi_delete_all(int *prop_sel_ref) {
    for(int i = 0; i < g_props.count; i++) {
        if(!g_prop_multi_sel[i] || !g_props.list[i].active) continue;
        UndoSnap _s; memset(&_s, 0, sizeof(_s)); _s.prop = undo_snap_prop(i);
        undo_push_delete(UNDO_PROP_DELETE, i, g_props.count, _s);
        prop_release_all(&g_props.list[i]);
        if(prop_sel_ref && *prop_sel_ref == i) *prop_sel_ref = -1;
    }
    prop_multi_clear();
}

/* Draw selection highlight for all multi-selected props */
static void prop_multi_draw_highlights(HUD *h, int primary,
                                        int win_w, int win_h,
                                        Mat4 proj, Mat4 view) {
    for(int i = 0; i < g_props.count; i++) {
        if(!g_prop_multi_sel[i] || !g_props.list[i].active || i == primary) continue;
        /* Draw a dimmer cyan bracket for non-primary selected props */
        float x, y, w, ht;
        if(!prop_selection_billboard_rect(&g_props.list[i], win_w, win_h, proj, view,
                                          &x, &y, &w, &ht)) continue;
        /* Thin cyan bracket lines */
        hud_rect(h, x, y,    w,    1.0f, 0.3f, 0.9f, 1.0f, 0.85f);
        hud_rect(h, x, y+ht, w,    1.0f, 0.3f, 0.9f, 1.0f, 0.85f);
        hud_rect(h, x, y,    1.0f, ht,   0.3f, 0.9f, 1.0f, 0.85f);
        hud_rect(h, x+w, y,  1.0f, ht,   0.3f, 0.9f, 1.0f, 0.85f);
    }
}

/* Draw box-select rectangle */
static void prop_box_sel_draw(HUD *h, int x0, int y0, int x1, int y1) {
    float fx0=(float)(x0<x1?x0:x1), fy0=(float)(y0<y1?y0:y1);
    float fw=(float)abs(x1-x0), fh=(float)abs(y1-y0);
    hud_rect(h, fx0, fy0, fw, 1.0f, 0.4f,0.8f,1.0f, 0.85f);
    hud_rect(h, fx0, fy0+fh, fw, 1.0f, 0.4f,0.8f,1.0f, 0.85f);
    hud_rect(h, fx0, fy0, 1.0f, fh, 0.4f,0.8f,1.0f, 0.85f);
    hud_rect(h, fx0+fw, fy0, 1.0f, fh, 0.4f,0.8f,1.0f, 0.85f);
    hud_rect(h, fx0, fy0, fw, fh, 0.15f,0.35f,0.55f, 0.08f); /* fill */
}
