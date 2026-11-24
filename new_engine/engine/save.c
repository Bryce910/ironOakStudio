#include "save.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#endif

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
    int inv_equipped[4];
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
    uint8_t equip_rarity[4];
    uint8_t equip_affix[4];
}SaveDataV11;

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
    int inv_equipped[8];
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
    uint8_t equip_rarity[8];
    uint8_t equip_affix[8];
}SaveDataV12;

static void save_upgrade_v12_to_v13(SaveData *dst,const SaveDataV12 *src){
    if(!dst||!src) return;
    memset(dst,0,sizeof(*dst));
    dst->magic=src->magic;
    dst->version=SAVE_VERSION;
    dst->px=src->px; dst->py=src->py; dst->pz=src->pz; dst->pyaw=src->pyaw;
    dst->health=src->health; dst->max_health=src->max_health;
    dst->mana=src->mana; dst->max_mana=src->max_mana;
    dst->stam=src->stam; dst->max_stam=src->max_stam;
    memcpy(dst->ab_cd_timer,src->ab_cd_timer,sizeof(src->ab_cd_timer));
    memcpy(dst->ab_ready,src->ab_ready,sizeof(src->ab_ready));
    dst->gold=src->gold; dst->xp=src->xp; dst->level=src->level;
    dst->quest_count=src->quest_count;
    memcpy(dst->quests,src->quests,sizeof(src->quests));
    dst->ne=src->ne;
    memcpy(dst->enemies,src->enemies,sizeof(src->enemies));
    dst->world_time=src->world_time;
    memcpy(dst->inv_slots,src->inv_slots,sizeof(src->inv_slots));
    for(int i=0;i<8;i++) dst->inv_equipped[i]=src->inv_equipped[i];
    dst->inv_equipped[8]=0;
    dst->lvl_dmg_bonus=src->lvl_dmg_bonus;
    dst->lvl_hp_bonus=src->lvl_hp_bonus;
    dst->lvl_mp_bonus=src->lvl_mp_bonus;
    dst->lvl_regen_bonus=src->lvl_regen_bonus;
    dst->skill_points=src->skill_points;
    memcpy(dst->skill_ranks,src->skill_ranks,sizeof(src->skill_ranks));
    memcpy(dst->appearance_rgb,src->appearance_rgb,sizeof(src->appearance_rgb));
    dst->appearance_enabled=src->appearance_enabled;
    dst->player_class=src->player_class;
    dst->farm_plot_count=src->farm_plot_count;
    memcpy(dst->farm_plots,src->farm_plots,sizeof(src->farm_plots));
    memcpy(dst->storage_slots,src->storage_slots,sizeof(src->storage_slots));
    memcpy(dst->slot_rarity,src->slot_rarity,sizeof(src->slot_rarity));
    memcpy(dst->slot_affix,src->slot_affix,sizeof(src->slot_affix));
    for(int i=0;i<8;i++){
        dst->equip_rarity[i]=src->equip_rarity[i];
        dst->equip_affix[i]=src->equip_affix[i];
    }
    dst->equip_rarity[8]=0;
    dst->equip_affix[8]=0;
}

static void save_upgrade_v11_to_v12(SaveData *dst,const SaveDataV11 *src){
    if(!dst||!src) return;
    memset(dst,0,sizeof(*dst));
    dst->magic=src->magic;
    dst->version=SAVE_VERSION;
    dst->px=src->px; dst->py=src->py; dst->pz=src->pz; dst->pyaw=src->pyaw;
    dst->health=src->health; dst->max_health=src->max_health;
    dst->mana=src->mana; dst->max_mana=src->max_mana;
    dst->stam=src->stam; dst->max_stam=src->max_stam;
    memcpy(dst->ab_cd_timer,src->ab_cd_timer,sizeof(src->ab_cd_timer));
    memcpy(dst->ab_ready,src->ab_ready,sizeof(src->ab_ready));
    dst->gold=src->gold; dst->xp=src->xp; dst->level=src->level;
    dst->quest_count=src->quest_count;
    memcpy(dst->quests,src->quests,sizeof(src->quests));
    dst->ne=src->ne;
    memcpy(dst->enemies,src->enemies,sizeof(src->enemies));
    dst->world_time=src->world_time;
    memcpy(dst->inv_slots,src->inv_slots,sizeof(src->inv_slots));
    for(int i=0;i<4;i++) dst->inv_equipped[i]=src->inv_equipped[i];
    dst->lvl_dmg_bonus=src->lvl_dmg_bonus;
    dst->lvl_hp_bonus=src->lvl_hp_bonus;
    dst->lvl_mp_bonus=src->lvl_mp_bonus;
    dst->lvl_regen_bonus=src->lvl_regen_bonus;
    dst->skill_points=src->skill_points;
    memcpy(dst->skill_ranks,src->skill_ranks,sizeof(src->skill_ranks));
    memcpy(dst->appearance_rgb,src->appearance_rgb,sizeof(src->appearance_rgb));
    dst->appearance_enabled=src->appearance_enabled;
    dst->player_class=src->player_class;
    dst->farm_plot_count=src->farm_plot_count;
    memcpy(dst->farm_plots,src->farm_plots,sizeof(src->farm_plots));
    memcpy(dst->storage_slots,src->storage_slots,sizeof(src->storage_slots));
    memcpy(dst->slot_rarity,src->slot_rarity,sizeof(src->slot_rarity));
    memcpy(dst->slot_affix,src->slot_affix,sizeof(src->slot_affix));
    for(int i=0;i<4;i++){
        dst->equip_rarity[i]=src->equip_rarity[i];
        dst->equip_affix[i]=src->equip_affix[i];
    }
}

bool save_mkdir(void){
#ifdef _WIN32
    _mkdir(SAVE_DIR);
#else
    mkdir(SAVE_DIR,0755);
#endif
    return true;
}

void save_path(char *buf,size_t sz,int slot){
    snprintf(buf,sz,SAVE_DIR"/slot%d.sav",slot);
}

void fmt_playtime(float t,char *buf,int bufsz){
    int h=(int)(t/3600),m=(int)(t/60)%60;
    snprintf(buf,bufsz,"%dh %02dm",h,m);
}

bool save_read_slot(int slot,SaveData *out){
    char path[64];
    save_path(path,sizeof(path),slot);
    FILE *f=fopen(path,"rb");
    if(!f)return false;
    uint32_t header[2]={0};
    bool ok=false;
    if(fread(header,sizeof(header),1,f)==1&&header[0]==SAVE_MAGIC){
        rewind(f);
        if(header[1]==SAVE_VERSION){
            SaveData sd;
            ok=fread(&sd,sizeof(sd),1,f)==1;
            if(ok&&out) *out=sd;
        }else if(header[1]==12){
            SaveDataV12 sd12;
            ok=fread(&sd12,sizeof(sd12),1,f)==1;
            if(ok&&out) save_upgrade_v12_to_v13(out,&sd12);
        }else if(header[1]==11){
            SaveDataV11 sd11;
            ok=fread(&sd11,sizeof(sd11),1,f)==1;
            if(ok&&out) save_upgrade_v11_to_v12(out,&sd11);
        }
    }
    fclose(f);
    return ok;
}

bool save_write_slot(int slot,const SaveData *sd){
    if(!sd)return false;
    save_mkdir();
    char path[64];
    save_path(path,sizeof(path),slot);
    FILE *f=fopen(path,"wb");
    if(!f){
        printf("[Save] Cannot write %s\n",path);
        return false;
    }
    bool ok=fwrite(sd,sizeof(*sd),1,f)==1;
    fclose(f);
    return ok;
}

bool save_read_slot_info(int slot,SaveSlotInfo *out){
    SaveData sd;
    bool ok=save_read_slot(slot,&sd);
    if(out){
        memset(out,0,sizeof(*out));
        out->has_save=ok;
        if(ok){
            out->level=sd.level;
            out->world_time=sd.world_time;
            out->health=sd.health;
        }
    }
    return ok;
}
