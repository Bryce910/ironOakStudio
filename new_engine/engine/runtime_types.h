#ifndef ENGINE_RUNTIME_TYPES_H
#define ENGINE_RUNTIME_TYPES_H

#include <stdbool.h>

#include "engine/game_types.h"

#ifndef HOTBAR_SLOTS
#define HOTBAR_SLOTS 5
#endif

#ifndef INV_SLOTS
#define INV_SLOTS 24
#endif

#ifndef INV_EQUIP_SLOTS
#define INV_EQUIP_SLOTS 9
#endif

typedef enum{
    EQUIP_SLOT_WEAPON=0,
    EQUIP_SLOT_HELM,
    EQUIP_SLOT_CHEST,
    EQUIP_SLOT_GLOVES,
    EQUIP_SLOT_LEGS,
    EQUIP_SLOT_BOOTS,
    EQUIP_SLOT_SHOULDERS,
    EQUIP_SLOT_CLOAK,
    EQUIP_SLOT_SHIELD
}EquipSlot;

typedef struct { int idle,walk,run,attack,attack2,jump,death,roll,block,hit; } AnimMap;

typedef struct LevelSystem{
    float flash,banner,dmg_bonus,hp_bonus,mp_bonus,regen_bonus;
}LevelSystem;

typedef struct Ability{
    char name[32];
    float cost,cd,cd_timer;
    bool ready;
}Ability;

typedef struct AbilitySystem{
    Ability slots[HOTBAR_SLOTS];
    float mana,max_mana,mp_regen,stam,max_stam,sp_regen,stam_drain_timer;
}AbilitySystem;

#ifndef SKILL_NODE_COUNT
#define SKILL_NODE_COUNT 6
#endif

#ifndef APPEARANCE_COLOR_SLOTS
#define APPEARANCE_COLOR_SLOTS 5
#endif

typedef struct SkillTree{
    bool open;
    int available_points;
    unsigned char ranks[SKILL_NODE_COUNT];
}SkillTree;

typedef enum{
    APPEARANCE_CLOTH=0,
    APPEARANCE_LEATHER,
    APPEARANCE_METAL,
    APPEARANCE_SKIN,
    APPEARANCE_HAIR
}AppearanceColorSlot;

typedef struct PlayerAppearance{
    bool open;
    int selected_slot;
    unsigned char rgb[APPEARANCE_COLOR_SLOTS][3];
}PlayerAppearance;

typedef struct Camera3P{
    float yaw,pitch,dist;
    Vec3 pos,target;
}Camera3P;

#ifndef MAX_MAGIC_VISUALS
#define MAX_MAGIC_VISUALS 16
#endif

typedef enum{
    MAGIC_VIS_NONE=0,
    MAGIC_VIS_CAST_SIGIL,
    MAGIC_VIS_FIRE_PATCH,
    MAGIC_VIS_ICE_PATCH,
    MAGIC_VIS_ICE_WALL,
    MAGIC_VIS_ROCKFALL
}MagicVisualType;

typedef struct MagicVisual{
    bool active;
    MagicVisualType type;
    float time;
    float lifetime;
    Vec3 origin;
    Vec3 dir;
    Vec3 tint;
    Vec3 points[8];
    int point_count;
    float radius;
    float height;
    unsigned char point_state[8];
}MagicVisual;

typedef struct MagicVisualSystem{
    MagicVisual items[MAX_MAGIC_VISUALS];
    int count;
}MagicVisualSystem;

typedef struct SaveNotify{
    char msg[64];
    float timer;
}SaveNotify;

typedef enum{
    ITEM_NONE=0,
    ITEM_POTION_HP,
    ITEM_POTION_MP,
    ITEM_POTION_FULL,
    ITEM_SWORD_IRON,
    ITEM_SWORD_STEEL,
    ITEM_SWORD_MAGIC,
    ITEM_HELM_LEATHER,
    ITEM_HELM_IRON,
    ITEM_CHEST_LEATHER,
    ITEM_CHEST_IRON,
    ITEM_BOOTS_LEATHER,
    ITEM_FISH_RAW,
    ITEM_FISH_TROUT,
    ITEM_FISH_GOLDEN,
    ITEM_MEAL_FISH,
    ITEM_MEAL_TROUT,
    ITEM_MEAL_GOLDEN,
    ITEM_MEAL_BURNT_FISH,
    ITEM_MEAL_BURNT_TROUT,
    ITEM_MEAL_BURNT_FEAST,
    ITEM_SEED_TURNIP,
    ITEM_TURNIP,
    ITEM_SEED_CARROT,
    ITEM_CARROT,
    ITEM_SEED_CABBAGE,
    ITEM_CABBAGE,
    ITEM_SEED_PUMPKIN,
    ITEM_PUMPKIN,
    ITEM_MEAL_STEW,
    ITEM_MEAL_ROAST_ROOTS,
    ITEM_GOLD_COIN,
    ITEM_GLOVES_LEATHER,
    ITEM_LEGS_IRON,
    ITEM_SHOULDERS_IRON,
    ITEM_CLOAK_RANGER,
    ITEM_HELM_KNIGHT,
    ITEM_CHEST_KNIGHT,
    ITEM_BOOTS_KNIGHT,
    ITEM_SHIELD_WOOD,
    ITEM_SHIELD_IRON,
    ITEM_SHIELD_KNIGHT,
    ITEM_COUNT
}ItemType;

typedef enum{RARITY_COMMON=0,RARITY_UNCOMMON,RARITY_RARE,RARITY_LEGENDARY,RARITY_COUNT}ItemRarity;
typedef enum{AFFIX_NONE=0,AFFIX_SWIFT,AFFIX_TOUGH,AFFIX_MIGHTY,AFFIX_COUNT}ItemAffix;

typedef struct InvSlot{
    ItemType type;
    int count;
    uint8_t rarity;
    uint8_t affix;
}InvSlot;

typedef struct Inventory{
    InvSlot slots[INV_SLOTS];
    ItemType equipped[INV_EQUIP_SLOTS];
    bool open;
    bool visual_edit_mode;
    int hover;
    int selected;
    int visual_edit_slot;
    int visual_edit_inst;
    float dmg_bonus;
    float hp_bonus;
    float dmg_reduce;
    float spd_bonus;
    uint8_t equip_rarity[INV_EQUIP_SLOTS];
    uint8_t equip_affix[INV_EQUIP_SLOTS];
}Inventory;

typedef struct{
    bool active;
    ItemType raw_type;
    ItemType cooked_type;
    ItemType overcooked_type;
    float cursor;
    float cursor_vel;
    float good_center;
    float good_width;
    float overcook_line;
    float sizzle;
    int stage;
}CookingSystem;

typedef struct FishingSystem{
    bool  active;
    bool  near_water;
    float water_level;
    Vec3  spot;
    float cast_anim;
    float cursor;
    float cursor_vel;
    float target;
    float target_w;
    float retarget_timer;
    int   misses;
    int   xp;
    int   level;
    float banner;
    ItemType last_catch;
}FishingSystem;

#ifndef MAX_FARM_PLOTS
#define MAX_FARM_PLOTS 64
#endif

typedef enum{
    FARM_CROP_NONE=0,
    FARM_CROP_TURNIP,
    FARM_CROP_CARROT,
    FARM_CROP_CABBAGE,
    FARM_CROP_PUMPKIN
}FarmCropType;

typedef enum{
    FARM_STAGE_EMPTY=0,
    FARM_STAGE_PLANTED,
    FARM_STAGE_READY,
    FARM_STAGE_WITHERED
}FarmPlotStage;

typedef struct FarmPlot{
    bool active;
    Vec3 pos;
    FarmCropType crop;
    FarmPlotStage stage;
    bool watered;
    float growth;
    float stress;
}FarmPlot;

typedef struct FarmingSystem{
    FarmPlot plots[MAX_FARM_PLOTS];
    int count;
    int hovered_plot;
    float prompt_alpha;
}FarmingSystem;

#endif
