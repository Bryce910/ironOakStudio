#ifndef ENGINE_SCENE_SYSTEM_H
#define ENGINE_SCENE_SYSTEM_H

#include <stdbool.h>
#include "engine/game_types.h"

/* ══════════════════════════════════════════════════════════════
   SCENE SYSTEM
   Manages discrete world scenes (apartment prologue, the shire,
   future levels) and the mystical transition sequence between them.

   Transition sequence (apartment → shire):
     TRANS_RIPPLE   0.0–0.8s  MAGIC particles burst from player, ambient
                               light pulses gold
     TRANS_PULL     0.8–2.5s  Fullscreen mystical overlay ramps up;
                               rotating golden/purple beams collapse inward
     TRANS_WHITEOUT 2.5–3.2s  Screen fades to white then pure black
     TRANS_SWITCH   1 frame   Player teleported to shire spawn, scene flags set
     TRANS_FADEIN   3.2–4.8s  Fade in from black on the open shire
   ══════════════════════════════════════════════════════════════ */

typedef enum {
    SCENE_APARTMENT = 0,   /* prologue — interior flat              */
    SCENE_SHIRE     = 1,   /* act 1 — open rolling countryside      */
    SCENE_COUNT
} SceneID;

typedef enum {
    TRANS_IDLE     = 0,
    TRANS_RIPPLE,       /* 0.00–0.80s : particles begin, light flicker   */
    TRANS_PULL,         /* 0.80–2.50s : overlay ramps up                 */
    TRANS_WHITEOUT,     /* 2.50–3.20s : full-screen white/black blast    */
    TRANS_SWITCH,       /* 1 frame    : teleport + scene swap            */
    TRANS_FADEIN        /* 3.20–4.80s : fade in on new scene             */
} TransStage;

/* World-space XZ position of the apartment interior.
   Placed far from the campfire (-140, 60) but within WORLD_HALF (250).
   The apartment floor uses a fixed Y so overworld terrain edits do not
   move the prologue scene. */
#define APARTMENT_CX    100.0f
#define APARTMENT_CZ   -185.0f
#define APARTMENT_FLOOR_Y 1.0f
#define APARTMENT_HW    24.0f    /* half-width  (x) → 48 m wide */
#define APARTMENT_HH     9.0f    /* half-height (y) → 18 m tall */
#define APARTMENT_HD    36.0f    /* half-depth  (z) → 72 m long */

/* XZ of player spawn inside apartment (Y derived from the fixed apartment floor) */
#define APARTMENT_SPAWN_X   100.0f
#define APARTMENT_SPAWN_Z  -181.0f   /* near-wall side, facing desk */
#define APARTMENT_SPAWN_YAW  3.14159f

/* Monitor NPC XZ — desk on the far wall (Y derived from the fixed apartment floor) */
#define MONITOR_NPC_X   101.5f
#define MONITOR_NPC_Z  -192.5f

typedef struct {
    SceneID    current;          /* scene currently active              */
    SceneID    target;           /* scene we're transitioning toward    */
    TransStage stage;            /* current transition stage            */
    float      timer;            /* seconds elapsed in current stage    */
    float      overlay_alpha;    /* 0..1 — mystical beam overlay blend  */
    float      fade_alpha;       /* 0..1 — black/white full-screen fade */
    bool       switched;         /* true once the scene swap executed   */
    bool       active;           /* true while any transition is running*/
} SceneSystem;

#endif /* ENGINE_SCENE_SYSTEM_H */
