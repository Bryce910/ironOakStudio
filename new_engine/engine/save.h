#ifndef ENGINE_SAVE_H
#define ENGINE_SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SAVE_SLOTS 3
#define SAVE_MAGIC 0x4D505247
#define SAVE_VERSION 13
#define SAVE_DIR "saves"
#define SAVE_HOTBAR_SLOTS 5
#define SAVE_MAX_OBJECTIVES 4
#define SAVE_MAX_QUESTS 8
#define SAVE_MAX_ENEMIES 128
#define SAVE_INV_SLOTS 24
#define SAVE_INV_EQUIP_SLOTS 9
#define SAVE_SKILL_NODES 6
#define SAVE_APPEARANCE_RGB (5*3)
#define SAVE_MAX_FARM_PLOTS 64
#define SAVE_STORAGE_SLOTS 12

typedef struct{
    uint32_t magic,version;
    float px,py,pz,pyaw;
    float health,max_health;
    float mana,max_mana,stam,max_stam;
    float ab_cd_timer[SAVE_HOTBAR_SLOTS];
    bool ab_ready[SAVE_HOTBAR_SLOTS];
    int gold,xp,level;
    int quest_count;
    struct{
        int id;
        int status;
        int obj_current[SAVE_MAX_OBJECTIVES];
        bool obj_done[SAVE_MAX_OBJECTIVES];
    }quests[SAVE_MAX_QUESTS];
    int ne;
    struct{
        bool active;
        int etype;
        float hx,hy,hz;
        float health;
        int state;
    }enemies[SAVE_MAX_ENEMIES];
    float world_time;
    struct{ int type; int count; }inv_slots[SAVE_INV_SLOTS];
    int inv_equipped[SAVE_INV_EQUIP_SLOTS];
    float lvl_dmg_bonus,lvl_hp_bonus,lvl_mp_bonus,lvl_regen_bonus;
    int skill_points;
    uint8_t skill_ranks[SAVE_SKILL_NODES];
    uint8_t appearance_rgb[SAVE_APPEARANCE_RGB];
    uint8_t appearance_enabled;
    uint8_t player_class;
    int farm_plot_count;
    struct{
        bool active;
        uint8_t crop;
        uint8_t stage;
        uint8_t watered;
        float growth;
        float stress;
        float px,py,pz;
    }farm_plots[SAVE_MAX_FARM_PLOTS];
    struct{ int type; int count; }storage_slots[SAVE_STORAGE_SLOTS];
    uint8_t slot_rarity[SAVE_INV_SLOTS];
    uint8_t slot_affix[SAVE_INV_SLOTS];
    uint8_t equip_rarity[SAVE_INV_EQUIP_SLOTS];
    uint8_t equip_affix[SAVE_INV_EQUIP_SLOTS];
}SaveData;

typedef struct{
    bool has_save;
    int level;
    float world_time;
    float health;
}SaveSlotInfo;

bool save_mkdir(void);
void save_path(char *buf,size_t sz,int slot);
void fmt_playtime(float t,char *buf,int bufsz);
bool save_read_slot(int slot,SaveData *out);
bool save_write_slot(int slot,const SaveData *sd);
bool save_read_slot_info(int slot,SaveSlotInfo *out);

#endif
