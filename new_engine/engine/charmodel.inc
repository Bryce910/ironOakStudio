/* Fixed logical animation slot indices — AnimMap fields always equal these */
#define ANIM_IDLE    0
#define ANIM_WALK    1
#define ANIM_RUN     2
#define ANIM_ATTACK  3
#define ANIM_ATTACK2 4
#define ANIM_JUMP    5
#define ANIM_DEATH   6
#define ANIM_ROLL    7
#define ANIM_BLOCK   8
#define ANIM_HIT     9
#define ANIM_SLOTS   10

/* One resolved animation: which scene file + which stack within it */
typedef struct AnimEntry { Scene *scene; int stack; } AnimEntry;

/* A character model: base mesh/skeleton + per-slot animation resolution.
   Works for BOTH single-FBX (multiple stacks) AND multi-FBX (one anim per file). */
#define CHARMODEL_MAX_EXTRA 12   /* max separately-loaded animation FBX files */
typedef struct CharModel {
    Scene      base;                      /* mesh, skeleton, scale info */
    AnimEntry  anims[ANIM_SLOTS];         /* resolved [scene, stack] per slot */
    Scene      extra[CHARMODEL_MAX_EXTRA];/* storage for extra per-anim FBX files */
    int        extra_count;
    bool       owns_base;                 /* false = base is a shallow copy, don't free */
} CharModel;

static float charmodel_asset_scale_override(const char *path){
    /* Scene bounds and skinning share the same coordinate space. User-facing
       character scale is applied through the character preset. */
    (void)path;return 1.0f;
}

static void charmodel_apply_scene_scale(Scene *sc,float scale){
    if(!sc||scale==1.0f) return;
    sc->model_scale*=scale;
    sc->ground_offset*=scale;
}

static void charmodel_init(CharModel *cm, const char *base_fbx){
    float asset_scale=charmodel_asset_scale_override(base_fbx);
    memset(cm,0,sizeof(*cm));
    cm->owns_base=true;
    if(!scene_load(&cm->base,base_fbx)){
        fprintf(stderr,"[CharModel] Failed to load base: %s\n",base_fbx);
        return;
    }
    charmodel_apply_scene_scale(&cm->base,asset_scale);
    /* Slot names for keyword search AND per-file fallback suffix */
    static const char *slot_kws[ANIM_SLOTS]={
        "idle","walk","run","attack","attack2","jump","death","roll","block","hit"
    };
    /* Build base stem for per-file probing: strip extension */
    char stem[512];
    strncpy(stem,base_fbx,sizeof(stem)-1);stem[sizeof(stem)-1]=0;
    char *dot=strrchr(stem,'.');if(dot)*dot=0;   /* e.g. "models/knight" */

    /* First pass: fill from base scene stacks */
    for(int s=0;s<ANIM_SLOTS;s++){
        int idx=scene_find_anim(&cm->base,slot_kws[s]);
        if(idx<0&&s==ANIM_ATTACK2)idx=scene_find_anim(&cm->base,"kick");
        if(idx<0&&s==ANIM_ATTACK2)idx=scene_find_anim(&cm->base,"slash2");
        if(idx<0&&s==ANIM_RUN)    idx=scene_find_anim(&cm->base,"sprint");
        if(idx<0&&s==ANIM_DEATH)  idx=scene_find_anim(&cm->base,"die");
        if(idx<0&&s==ANIM_ROLL)   idx=scene_find_anim(&cm->base,"dodge");
        if(idx<0&&s==ANIM_BLOCK)  idx=scene_find_anim(&cm->base,"guard");
        if(idx<0&&s==ANIM_HIT)    idx=scene_find_anim(&cm->base,"hurt");

        if(idx>=0){
            cm->anims[s].scene=&cm->base;
            cm->anims[s].stack=idx;
        }
    }

    /* Second pass: for any slot still missing, probe {stem}_{slotname}.{fbx|glb}.
       This supports one-file-per-animation workflows for both FBX and glTF assets. */
    for(int s=0;s<ANIM_SLOTS;s++){
        if(cm->anims[s].scene)continue;
        if(cm->extra_count>=CHARMODEL_MAX_EXTRA)break;

        {
            char probe[600];
            static const char *exts[]={"fbx","glb"};
            for(int ei=0;ei<(int)(sizeof(exts)/sizeof(exts[0]));ei++){
                snprintf(probe,sizeof(probe),"%s_%s.%s",stem,slot_kws[s],exts[ei]);
                if(scene_load(&cm->extra[cm->extra_count],probe)){
                    charmodel_apply_scene_scale(&cm->extra[cm->extra_count],asset_scale);
                    cm->anims[s].scene=&cm->extra[cm->extra_count];
                    cm->anims[s].stack=0;
                    cm->extra_count++;
                    printf("[CharModel] Loaded separate anim: %s -> slot %d\n",probe,s);
                    break;
                }
            }
        }
    }

    if(!cm->anims[ANIM_IDLE].scene){
        cm->anims[ANIM_IDLE].scene=&cm->base;
        cm->anims[ANIM_IDLE].stack=scene_anim_count(&cm->base)>0?0:-1;
    }
    for(int s=0;s<ANIM_SLOTS;s++){
        if(!cm->anims[s].scene){
            int fb=ANIM_IDLE;
            if(s==ANIM_ATTACK2)             fb=ANIM_ATTACK;
            cm->anims[s]=cm->anims[fb];
        }
    }
}

/* Shallow-copy another model's base (for fallback enemy reuse of player mesh) */
static void charmodel_share_base(CharModel *dst, CharModel *src){
    memset(dst,0,sizeof(*dst));
    dst->base=src->base;
    dst->owns_base=false;
    for(int s=0;s<ANIM_SLOTS;s++) dst->anims[s]=src->anims[s];
}

static void charmodel_free(CharModel *cm){
    if(cm->owns_base) scene_free(&cm->base);
    for(int i=0;i<cm->extra_count;i++) scene_free(&cm->extra[i]);
    memset(cm,0,sizeof(*cm));
}
