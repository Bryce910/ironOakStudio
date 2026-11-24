/* ══════════════════════════════════════════════════════════════
   SCENE SYSTEM IMPLEMENTATION  (included into main.c)

   Requires (all defined earlier in main.c):
     - engine/scene_system.h        (types)
     - GLH_program(), GLH_float()   (gl_helpers.h)
     - interior_add()               (InteriorDef system)
     - npc_add(), npc_add_line()    (NpcSystem)
     - dlg_node(), dlg_choice()     (dialogue tree)
     - ps_spawn_typed()             (ParticleSystem)
     - audio_play()                 (audio)
     - clampf(), vec3_*             (math.inc)
     - DLG_SCENE_TRANSITION         (defined in main.c)
   ══════════════════════════════════════════════════════════════ */

/* ── Globals ──────────────────────────────────────────────── */
/* g_scene_sys and g_shire_spawn are forward-declared in main.c
   before the NPC functions so they can be referenced there.
   Definitions come from those forward declarations (zero-init). */

/* GL resources for the mystical pull overlay */
static GLuint g_mystical_prog    = 0;
static GLuint g_mystical_vao     = 0;   /* empty VAO for fullscreen tri */

/* Duration constants (seconds) for each transition stage */
#define TRANS_RIPPLE_DUR   0.80f
#define TRANS_PULL_DUR     1.70f
#define TRANS_WHITEOUT_DUR 0.70f
#define TRANS_SWITCH_DUR   0.0f    /* single frame */
#define TRANS_FADEIN_DUR   1.60f

/* Particle spawn interval during TRANS_RIPPLE / TRANS_PULL */
static float g_mystical_particle_timer = 0.0f;

/* ── GL setup ─────────────────────────────────────────────── */
static void scene_system_setup_overlay(void) {
    /* Load shader source from disk (same helper the shader lab uses) */
    char *vs_src = shader_lab_read_text("shaders/mystical_pull.vs");
    char *fs_src = shader_lab_read_text("shaders/mystical_pull.fs");
    if (vs_src && fs_src) {
        g_mystical_prog = GLH_program(vs_src, fs_src);
        if (!g_mystical_prog)
            printf("[SceneSystem] WARNING: mystical_pull shader failed to link.\n");
    } else {
        printf("[SceneSystem] WARNING: could not read mystical_pull shader files.\n");
    }
    free(vs_src);
    free(fs_src);

    /* Empty VAO — vertex shader generates fullscreen triangle from gl_VertexID */
    glGenVertexArrays(1, &g_mystical_vao);
}

/* ── Init ─────────────────────────────────────────────────── */
static void scene_system_init(SceneSystem *ss, SceneID start) {
    memset(ss, 0, sizeof(*ss));
    ss->current = start;
    ss->target  = start;
    ss->stage   = TRANS_IDLE;
}

/* ── Trigger ──────────────────────────────────────────────── */
static void scene_system_begin_transition(SceneSystem *ss, SceneID target) {
    if (ss->active) return;   /* already mid-transition */
    ss->target   = target;
    ss->stage    = TRANS_RIPPLE;
    ss->timer    = 0.0f;
    ss->active   = true;
    ss->switched = false;
    ss->overlay_alpha = 0.0f;
    ss->fade_alpha    = 0.0f;
    g_mystical_particle_timer = 0.0f;
    printf("[SceneSystem] Transition began: %d → %d\n", ss->current, target);
}

/* ── Per-frame update ─────────────────────────────────────── */
/* Called from world_runtime.inc each frame.
   ps and player_pos are used to spawn particles.              */
static void scene_system_update_frame(SceneSystem *ss, float dt,
                                      ParticleSystem *ps,
                                      Vec3 player_pos,
                                      Player *player) {
    if (!ss->active) return;

    ss->timer += dt;

    switch (ss->stage) {

    /* ── Stage 0: RIPPLE ───────────────────────────────────── */
    case TRANS_RIPPLE: {
        ss->overlay_alpha = 0.0f;
        ss->fade_alpha    = 0.0f;

        /* Burst MAGIC particles every 0.08s */
        g_mystical_particle_timer -= dt;
        if (g_mystical_particle_timer <= 0.0f) {
            g_mystical_particle_timer = 0.08f;
            Vec3 vel = {
                (float)(rand()%200-100)*0.02f,
                0.4f + (float)(rand()%100)*0.006f,
                (float)(rand()%200-100)*0.02f
            };
            ps_spawn_typed(ps, player_pos, vel,
                           0.25f + (float)(rand()%60)*0.003f,
                           0.9f,
                           0.8f, 0.4f, 1.0f,   /* purple-gold tint */
                           PT_MAGIC);
        }

        if (ss->timer >= TRANS_RIPPLE_DUR) {
            ss->stage = TRANS_PULL;
            ss->timer = 0.0f;
            audio_play(SND_MENU_CLICK, 90);   /* re-use ambient click as "pull" cue */
        }
        break;
    }

    /* ── Stage 1: PULL ─────────────────────────────────────── */
    case TRANS_PULL: {
        float t = clampf(ss->timer / TRANS_PULL_DUR, 0.0f, 1.0f);
        /* Smooth ramp with cubic ease-in */
        ss->overlay_alpha = t * t * (3.0f - 2.0f * t);
        ss->fade_alpha    = 0.0f;

        /* Dense particles throughout */
        g_mystical_particle_timer -= dt;
        if (g_mystical_particle_timer <= 0.0f) {
            g_mystical_particle_timer = 0.04f;
            Vec3 vel = {
                (float)(rand()%200-100)*0.03f,
                0.5f + (float)(rand()%80)*0.008f,
                (float)(rand()%200-100)*0.03f
            };
            float tint_r = 0.5f + 0.5f * t;   /* shifts gold→white */
            ps_spawn_typed(ps, player_pos, vel,
                           0.30f + t * 0.20f,
                           1.2f,
                           tint_r, 0.4f*(1.0f-t), 1.0f,
                           PT_MAGIC);
        }

        if (ss->timer >= TRANS_PULL_DUR) {
            ss->stage = TRANS_WHITEOUT;
            ss->timer = 0.0f;
        }
        break;
    }

    /* ── Stage 2: WHITEOUT ─────────────────────────────────── */
    case TRANS_WHITEOUT: {
        float t = clampf(ss->timer / TRANS_WHITEOUT_DUR, 0.0f, 1.0f);
        ss->overlay_alpha = 1.0f;
        /* fade 0→1 maps white-out → black */
        ss->fade_alpha = t;

        if (ss->timer >= TRANS_WHITEOUT_DUR) {
            ss->stage = TRANS_SWITCH;
            ss->timer = 0.0f;
        }
        break;
    }

    /* ── Stage 3: SWITCH ───────────────────────────────────── */
    case TRANS_SWITCH: {
        if (!ss->switched) {
            ss->switched = true;
            ss->current  = ss->target;

            /* Teleport player to shire spawn */
            player->phys.pos  = g_shire_spawn;
            player->phys.yaw  = 0.0f;
            player->phys.vel  = (Vec3){0,0,0};

            ss->overlay_alpha = 0.0f;   /* kill overlay on new scene */
            ss->fade_alpha    = 1.0f;   /* still black — fade in next */

            printf("[SceneSystem] Scene switched to %d. Player at (%.1f,%.1f,%.1f)\n",
                   ss->current,
                   (double)g_shire_spawn.x,
                   (double)g_shire_spawn.y,
                   (double)g_shire_spawn.z);

            audio_play_music(MUS_EXPLORE, 2000);
        }
        ss->stage = TRANS_FADEIN;
        ss->timer = 0.0f;
        break;
    }

    /* ── Stage 4: FADEIN ───────────────────────────────────── */
    case TRANS_FADEIN: {
        float t = clampf(ss->timer / TRANS_FADEIN_DUR, 0.0f, 1.0f);
        ss->overlay_alpha = 0.0f;
        ss->fade_alpha    = 1.0f - (t * t * (3.0f - 2.0f * t));   /* ease out */

        if (ss->timer >= TRANS_FADEIN_DUR) {
            ss->fade_alpha = 0.0f;
            ss->stage      = TRANS_IDLE;
            ss->active     = false;
            printf("[SceneSystem] Transition complete — now in scene %d\n", ss->current);
        }
        break;
    }

    default: break;
    }
}

/* ── Draw overlay ─────────────────────────────────────────── */
/* Called in main loop after render_scene_runtime, before HUD.
   Draws as an additive + transparent fullscreen quad.         */
static void scene_system_draw_overlay(SceneSystem *ss, int win_w, int win_h,
                                      float total_time) {
    float oa = ss->overlay_alpha;
    float fa = ss->fade_alpha;
    if (oa < 0.001f && fa < 0.001f) return;
    if (!g_mystical_prog || !g_mystical_vao) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, 0, win_w, win_h);
    glUseProgram(g_mystical_prog);

    GLH_float(g_mystical_prog, "u_intensity", oa);
    GLH_float(g_mystical_prog, "u_time",      total_time);
    GLH_float(g_mystical_prog, "u_fade",      fa);

    glBindVertexArray(g_mystical_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glUseProgram(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

/* ── Apartment scene setup ────────────────────────────────── */
/* Called once from interiors_setup().
   Creates the box-room AABB and registers the monitor as a
   virtual NPC interaction point (no visible mesh — just an
   E-key interaction zone in front of the desk).

   All Y values derive from the fixed apartment floor so the
   prologue scene is stable even when the overworld terrain changes. */
static int apartment_register_monitor_npc(NpcSystem *ns) {
    float floor_y = g_apartment_floor_y;
    int mon = npc_add(ns,
        (Vec3){ MONITOR_NPC_X, floor_y + 0.2f, MONITOR_NPC_Z },
        0.0f,
        "Monitor"
    );
    if (mon < 0) {
        printf("[SceneSystem] WARNING: could not register monitor NPC.\n");
        return -1;
    }

    int n0 = dlg_node(
        "The screen flickers. Pixelated text bleeds across it:\n\n"
        "\"ARE YOU MISERABLE?\n DO YOU HATE YOUR LIFE?\"",
        "Monitor"
    );
    int n1 = dlg_node(
        "The text pulses — gold, then violet.\n\n"
        "\"THEN PERHAPS... IT IS TIME TO LEAVE.\"",
        "Monitor"
    );
    dlg_choice(n0, "Yes... I do.",      n1);
    dlg_choice(n0, "No, I'm fine.",     DLG_CLOSE);
    dlg_choice(n1, "[ Step through ]",  DLG_SCENE_TRANSITION);

    ns->npcs[mon].dlg_root_id  = n0;
    ns->npcs[mon].dlg_cur_node = n0;
    return mon;
}

static void apartment_setup(NpcSystem *ns) {
    float floor_y = APARTMENT_FLOOR_Y;
    g_apartment_floor_y = floor_y;
    float cy      = floor_y + APARTMENT_HH;   /* box-room centre Y */

    /* ── Box room ── */
    int apt = interior_add(
        "The Apartment",
        (Vec3){ APARTMENT_CX, cy, APARTMENT_CZ },
        APARTMENT_HW, APARTMENT_HH, APARTMENT_HD,
        /* walls:   grey plaster, faint blue tint */
        0.62f, 0.64f, 0.68f,
        /* floor:   dark laminate wood */
        0.22f, 0.18f, 0.14f,
        /* ceiling: off-white */
        0.78f, 0.78f, 0.76f,
        /* door:    north wall entrance */
        (Vec3){ APARTMENT_CX, floor_y, APARTMENT_CZ + APARTMENT_HD },
        /* interior ambient: cool evening light through a window */
        0.18f, 0.22f, 0.30f
    );
    g_apartment_interior_idx=apt;
    (void)apt;   /* reserved for future interior_add_prop() calls */

    /* ── Monitor NPC (virtual interaction point) ── */
    /* No visible character model — just the dialogue trigger zone.
       Position: desk on the far wall, slightly above floor.    */
    if (apartment_register_monitor_npc(ns) < 0) return;

    printf("[SceneSystem] Apartment interior + monitor NPC registered.\n");
}
