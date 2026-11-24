/* engine/dungeon.inc — Dungeon / instance system
   ─────────────────────────────────────────────────────────────────────────
   Three dungeon zones are scattered around the world.  Each has a boulder
   entrance marker (placed at init) and its own enemy composition.

   Flow:
     Player walks within DUNGEON_ENTER_RADIUS of an entrance, presses E
     → fade to black → world enemies saved → dungeon enemies spawned → fade in.
     Kill all enemies → CLEARED banner.
     Walk back to entrance + press E → fade to black → world enemies restored
     → fade in.

   Integration (add to main.c):
     After spawns_apply:
         dungeon_init(&g_dungeon, &g_props);
     After world_runtime_update (inside !is_paused):
         { const uint8_t *_dks=SDL_GetKeyboardState(NULL);
           bool _dpe=!npc_dialogue_capture&&_dks[SDL_SCANCODE_E]&&!inv.open&&player.state!=PS_DEAD;
           dungeon_update(&g_dungeon,&player,enemies,&ne,enemy_amaps,&ps,&save_note,dt,_dpe); }
     Before hud_flush:
         dungeon_draw_hud(&g_dungeon,&hud,win_w,win_h);
*/

#ifndef ENGINE_DUNGEON_INC
#define ENGINE_DUNGEON_INC

#include <string.h>
#include <stdio.h>

/* ── Config ─────────────────────────────────────────────────── */
#ifndef DUNGEON_COUNT
#define DUNGEON_COUNT 3
#endif
#ifndef DUNGEON_MAX_SPAWNS
#define DUNGEON_MAX_SPAWNS 12
#endif
#ifndef DUNGEON_ENTER_RADIUS
#define DUNGEON_ENTER_RADIUS 7.0f
#endif

#define DGN_FADE_SPEED   1.8f   /* seconds to go 0→1 */
#define DGN_NAME_DUR     4.0f   /* seconds banner shows on enter */
#define DGN_CLEAR_DUR    5.0f   /* seconds "CLEARED" banner shows */

/* ── Spawn definition ───────────────────────────────────────── */
typedef struct {
    EnemyType type;
    float ox, oz;   /* world-space offset from dungeon centre */
} DgnSpawn;

typedef struct {
    const char *name;
    const char *flavor;
    float cx, cz;                       /* world-space centre */
    DgnSpawn spawns[DUNGEON_MAX_SPAWNS];
    int nspawns;
} DgnDef;

/* ── The three dungeons ─────────────────────────────────────── */
static const DgnDef DGN_DEFS[DUNGEON_COUNT] = {
    {
        "Ruins of the Iron Guard",
        "Ancient soldiers, rusted but relentless.",
        -185.0f, -185.0f,
        {
            {ETYPE_KNIGHT,  -3.5f,   0.0f},
            {ETYPE_KNIGHT,   3.5f,   0.0f},
            {ETYPE_BRUTE,    0.0f,  -6.0f},
            {ETYPE_KNIGHT,  -7.0f,   4.0f},
            {ETYPE_ARCHER,   7.0f,   4.0f},
            {ETYPE_BRUTE,    0.0f,   7.0f},
        },
        6
    },
    {
        "The Shade Crypts",
        "Shadows stir beneath the ancient stone.",
        185.0f, -185.0f,
        {
            {ETYPE_SHADE,  -4.0f,   0.0f},
            {ETYPE_SHADE,   4.0f,   0.0f},
            {ETYPE_ARCHER, -2.0f,  -6.0f},
            {ETYPE_ARCHER,  2.0f,  -6.0f},
            {ETYPE_SHADE,   0.0f,   6.0f},
            {ETYPE_KNIGHT,  0.0f,  -4.0f},
        },
        6
    },
    {
        "Lich King's Sanctum",
        "The darkness here is absolute.  Survive.",
        0.0f, 185.0f,
        {
            {ETYPE_BOSS,    0.0f,  -9.0f},
            {ETYPE_SHADE,  -6.0f,   0.0f},
            {ETYPE_SHADE,   6.0f,   0.0f},
            {ETYPE_KNIGHT, -4.0f,   6.0f},
            {ETYPE_KNIGHT,  4.0f,   6.0f},
            {ETYPE_ARCHER,  0.0f,   9.0f},
        },
        6
    }
};

/* ── Runtime state ──────────────────────────────────────────── */
typedef struct {
    bool  active;                       /* true while inside a dungeon */
    int   id;                           /* current dungeon index (-1 = none) */
    bool  completed;                    /* all enemies dead this run */
    bool  dgn_completed[DUNGEON_COUNT]; /* permanent per-dungeon clear flag */

    float fade;     /* 0 = fully visible, 1 = fully black */
    int   fade_dir; /* 1 = darkening, -1 = brightening, 0 = idle */
    bool  at_black; /* swap pending: will fire when fade reaches 1 */
    int   pending_id; /* >=0 = enter that dungeon, -1 = exit */

    float name_timer;   /* counts down: show dungeon name while > 0 */
    float clear_timer;  /* counts down: show CLEARED while > 0 */

    int   alive_count;  /* cached by update, used by draw */
    bool  near_entrance;
    int   near_id;

    /* World-enemy save / restore */
    Enemy saved_enemies[MAX_ENEMIES];
    int   saved_ne;
} DgnState;

static DgnState g_dungeon;

/* ── Helpers ────────────────────────────────────────────────── */
static int dgn_count_alive(Enemy *enemies, int ne) {
    int n = 0;
    for(int i = 0; i < ne; i++)
        if(enemies[i].active && enemies[i].state != ES_DEAD) n++;
    return n;
}

/* ── dungeon_init ───────────────────────────────────────────── */
/* Call once after spawns_apply.  Places a boulder cluster at each
   entrance so the player can spot dungeon locations on the map. */
static void dungeon_init(DgnState *d, PropList *pl) {
    memset(d, 0, sizeof(*d));
    d->id         = -1;
    d->pending_id = -1;
    d->near_id    = -1;

    for(int i = 0; i < DUNGEON_COUNT; i++) {
        const DgnDef *def = &DGN_DEFS[i];
        /* Rotate each entrance cluster slightly differently */
        float ry = (float)(i * 60);
        prop_place(pl, "models/boulders.glb",
                   def->cx, 0.0f, def->cz,
                   0.0f, ry, 0.0f,
                   1.8f, PROP_COLL_NONE,
                   0.0f, 0.0f, 0.0f, 0.0f);
    }
}

/* ── Internal enter / exit ──────────────────────────────────── */
static void dgn_enter(DgnState *d, int id,
                      Enemy *enemies, int *ne,
                      AnimMap amaps[]) {
    const DgnDef *def = &DGN_DEFS[id];

    /* Save world enemies */
    if(*ne > MAX_ENEMIES) *ne = MAX_ENEMIES;
    memcpy(d->saved_enemies, enemies, sizeof(Enemy) * (size_t)*ne);
    d->saved_ne = *ne;

    /* Spawn dungeon enemies */
    memset(enemies, 0, sizeof(Enemy) * MAX_ENEMIES);
    *ne = 0;
    for(int i = 0; i < def->nspawns && *ne < MAX_ENEMIES; i++) {
        Vec3 sp = {
            def->cx + def->spawns[i].ox,
            0.0f,
            def->cz + def->spawns[i].oz
        };
        enemy_spawn(&enemies[*ne], def->spawns[i].type, sp, &amaps[def->spawns[i].type]);
        (*ne)++;
    }

    d->active      = true;
    d->id          = id;
    d->completed   = d->dgn_completed[id]; /* already cleared? */
    d->name_timer  = DGN_NAME_DUR;
    d->clear_timer = 0.0f;
    d->alive_count = dgn_count_alive(enemies, *ne);
}

static void dgn_exit(DgnState *d, Enemy *enemies, int *ne) {
    memcpy(enemies, d->saved_enemies, sizeof(Enemy) * (size_t)d->saved_ne);
    *ne       = d->saved_ne;
    d->active = false;
    d->id     = -1;
}

/* ── dungeon_update ─────────────────────────────────────────── */
static void dungeon_update(DgnState *d,
                           Player  *player,
                           Enemy   *enemies, int *ne,
                           AnimMap  amaps[],
                           ParticleSystem *ps,
                           SaveNotify *note,
                           float dt, bool press_e) {
    (void)ps; (void)note;

    /* ── Execute pending swap at peak black ── */
    if(d->at_black) {
        d->at_black = false;
        if(d->pending_id >= 0)
            dgn_enter(d, d->pending_id, enemies, ne, amaps);
        else
            dgn_exit(d, enemies, ne);
        d->pending_id = -1;
        d->fade_dir   = -1;   /* begin fading back in */
    }

    /* ── Fade ── */
    if(d->fade_dir != 0) {
        d->fade += (float)d->fade_dir * DGN_FADE_SPEED * dt;
        if(d->fade >= 1.0f) {
            d->fade      = 1.0f;
            d->at_black  = true;
            d->fade_dir  = 0;
        } else if(d->fade <= 0.0f) {
            d->fade     = 0.0f;
            d->fade_dir = 0;
        }
    }

    /* ── Timers ── */
    if(d->name_timer  > 0.0f) d->name_timer  -= dt;
    if(d->clear_timer > 0.0f) d->clear_timer -= dt;

    /* ── Alive count + completion ── */
    if(d->active) {
        d->alive_count = dgn_count_alive(enemies, *ne);
        if(!d->completed && d->alive_count == 0) {
            d->completed          = true;
            d->dgn_completed[d->id] = true;
            d->clear_timer        = DGN_CLEAR_DUR;
        }
    }

    /* Skip interaction logic while transitioning */
    if(d->fade_dir != 0 || d->at_black) {
        d->near_entrance = false;
        d->near_id       = -1;
        return;
    }

    Vec3 pp = player->phys.pos;

    /* ── Entrance proximity ── */
    if(!d->active) {
        d->near_entrance = false;
        d->near_id       = -1;
        for(int i = 0; i < DUNGEON_COUNT; i++) {
            float dx = pp.x - DGN_DEFS[i].cx;
            float dz = pp.z - DGN_DEFS[i].cz;
            if(dx*dx + dz*dz < DUNGEON_ENTER_RADIUS * DUNGEON_ENTER_RADIUS) {
                d->near_entrance = true;
                d->near_id       = i;
                if(press_e) {
                    d->pending_id = i;
                    d->fade       = 0.0f;
                    d->fade_dir   = 1;
                }
                break;
            }
        }
    } else {
        /* ── Exit proximity ── */
        const DgnDef *def = &DGN_DEFS[d->id];
        float dx = pp.x - def->cx;
        float dz = pp.z - def->cz;
        d->near_entrance = (dx*dx + dz*dz < DUNGEON_ENTER_RADIUS * DUNGEON_ENTER_RADIUS);
        d->near_id       = d->id;
        if(d->near_entrance && press_e) {
            d->pending_id = -1;   /* -1 = exit */
            d->fade       = 0.0f;
            d->fade_dir   = 1;
        }
    }
}

/* ── dungeon_draw_hud ───────────────────────────────────────── */
static void dungeon_draw_hud(DgnState *d, HUD *h, int win_w, int win_h) {
    float fw = (float)win_w;
    float fh = (float)win_h;

    /* ── Fade overlay ── */
    if(d->fade > 0.001f)
        hud_rect(h, 0.0f, 0.0f, fw, fh, 0.0f, 0.0f, 0.0f, d->fade);

    /* ── Outside dungeon: entrance prompt ── */
    if(!d->active) {
        if(d->near_entrance && d->near_id >= 0 && d->fade_dir == 0) {
            const DgnDef *def = &DGN_DEFS[d->near_id];
            float sc_name  = 2.2f;
            float sc_flav  = 1.3f;
            float sc_enter = 1.5f;
            float cy       = fh * 0.38f;
            char buf[128];

            /* Name */
            float tw = (float)strlen(def->name) * 9.0f * sc_name;
            hud_str(h, (fw - tw) * 0.5f, cy, sc_name,
                    def->name, 1.0f, 0.82f, 0.25f, 1.0f);
            cy += 9.0f * sc_name + 4.0f;

            /* Flavor */
            tw = (float)strlen(def->flavor) * 9.0f * sc_flav;
            hud_str(h, (fw - tw) * 0.5f, cy, sc_flav,
                    def->flavor, 0.78f, 0.78f, 0.78f, 0.9f);
            cy += 9.0f * sc_flav + 8.0f;

            /* Completion status */
            if(d->dgn_completed[d->near_id]) {
                const char *done = "[Cleared]";
                tw = (float)strlen(done) * 9.0f * sc_flav;
                hud_str(h, (fw - tw) * 0.5f, cy, sc_flav,
                        done, 0.3f, 0.9f, 0.3f, 1.0f);
                cy += 9.0f * sc_flav + 4.0f;
            }

            /* Enter prompt */
            snprintf(buf, sizeof(buf), "E: Enter Dungeon");
            tw = (float)strlen(buf) * 9.0f * sc_enter;
            hud_rect(h, (fw - tw) * 0.5f - 8.0f, cy - 4.0f,
                     tw + 16.0f, 9.0f * sc_enter + 8.0f,
                     0.0f, 0.0f, 0.0f, 0.50f);
            hud_str(h, (fw - tw) * 0.5f, cy, sc_enter,
                    buf, 0.35f, 1.0f, 0.35f, 1.0f);
        }
        return;
    }

    /* ── Inside dungeon ── */

    /* Name banner (fades as timer decays) */
    if(d->name_timer > 0.0f) {
        const char *name = DGN_DEFS[d->id].name;
        float a   = clampf(d->name_timer, 0.0f, 1.0f);
        float sc  = 2.5f;
        float tw  = (float)strlen(name) * 9.0f * sc;
        float ty  = fh * 0.18f;
        hud_rect(h, (fw - tw) * 0.5f - 14.0f, ty - 8.0f,
                 tw + 28.0f, 9.0f * sc + 16.0f,
                 0.0f, 0.0f, 0.0f, 0.60f * a);
        hud_str(h, (fw - tw) * 0.5f, ty, sc,
                name, 1.0f, 0.82f, 0.18f, a);
    }

    /* CLEARED banner */
    if(d->clear_timer > 0.0f) {
        const char *msg = "DUNGEON CLEARED!";
        float a   = clampf(d->clear_timer, 0.0f, 1.0f);
        float sc  = 3.0f;
        float tw  = (float)strlen(msg) * 9.0f * sc;
        float ty  = fh * 0.28f;
        hud_rect(h, (fw - tw) * 0.5f - 16.0f, ty - 10.0f,
                 tw + 32.0f, 9.0f * sc + 20.0f,
                 0.0f, 0.0f, 0.0f, 0.65f * a);
        hud_str(h, (fw - tw) * 0.5f, ty, sc,
                msg, 0.25f, 1.0f, 0.35f, a);
    }

    /* Enemy counter (bottom-centre) */
    {
        char buf[64];
        float sc = 1.5f;
        float ty = fh - 80.0f;
        if(d->alive_count > 0) {
            snprintf(buf, sizeof(buf), "%d %s remaining",
                     d->alive_count,
                     d->alive_count == 1 ? "enemy" : "enemies");
            float tw = (float)strlen(buf) * 9.0f * sc;
            hud_rect(h, (fw - tw) * 0.5f - 8.0f, ty - 4.0f,
                     tw + 16.0f, 9.0f * sc + 8.0f,
                     0.0f, 0.0f, 0.0f, 0.50f);
            hud_str(h, (fw - tw) * 0.5f, ty, sc,
                    buf, 1.0f, 0.45f, 0.35f, 1.0f);
        } else {
            snprintf(buf, sizeof(buf), "All enemies slain");
            float tw = (float)strlen(buf) * 9.0f * sc;
            hud_rect(h, (fw - tw) * 0.5f - 8.0f, ty - 4.0f,
                     tw + 16.0f, 9.0f * sc + 8.0f,
                     0.0f, 0.0f, 0.0f, 0.50f);
            hud_str(h, (fw - tw) * 0.5f, ty, sc,
                    buf, 0.35f, 1.0f, 0.35f, 1.0f);
        }
    }

    /* Exit prompt (near entrance, after dungeon complete or always) */
    if(d->near_entrance && d->fade_dir == 0) {
        const char *msg = "E: Exit Dungeon";
        float sc  = 1.5f;
        float tw  = (float)strlen(msg) * 9.0f * sc;
        float ty  = fh - 52.0f;
        hud_rect(h, (fw - tw) * 0.5f - 8.0f, ty - 4.0f,
                 tw + 16.0f, 9.0f * sc + 8.0f,
                 0.0f, 0.0f, 0.0f, 0.50f);
        hud_str(h, (fw - tw) * 0.5f, ty, sc,
                msg, 0.85f, 0.85f, 0.35f, 1.0f);
    }
}

#endif /* ENGINE_DUNGEON_INC */
