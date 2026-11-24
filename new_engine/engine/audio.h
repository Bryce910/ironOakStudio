#ifndef ENGINE_AUDIO_H
#define ENGINE_AUDIO_H

#include <stdbool.h>

#define SND_STEP        0
#define SND_RUN         1
#define SND_ATTACK      2
#define SND_HIT         3
#define SND_DEATH       4
#define SND_ENEMY_DIE   5
#define SND_JUMP        6
#define SND_FIREBALL    7
#define SND_FIREBALL_HIT 8
#define SND_HEAL        9
#define SND_MENU_CLICK  10
#define SND_LEVEL_UP    11
#define SND_ENEMY_GROWL 12
#define SND_ENEMY_ATTACK 13
#define SND_AMB_WIND    14
#define SND_AMB_BIRD    15
#define SND_AMB_CRICKET 16
#define SND_AMB_LAVA    17
#define SND_AMB_CROW    18
#define SND_AMB_RIVER   19
#define SND_AMB_THUNDER 20
#define SND_AMB_OWL     21
#define SND_LOOP_RAIN   22
#define SND_LOOP_NIGHT  23
#define SND_DASH        24
#define SND_SHIELD      25
#define SND_METEOR      26
#define SND_ANIMAL_0    27
#define SND_ANIMAL_1    28
#define SND_ANIMAL_2    29
#define SND_WF_LOOP     30
#define SND_COUNT       31

#define MUS_EXPLORE      0
#define MUS_COMBAT       1
#define MUS_MENU         2
#define MUS_BIOME_MTN    3
#define MUS_BIOME_FOREST 4
#define MUS_BIOME_LAVA   5
#define MUS_BIOME_PLAINS 6
#define MUS_COUNT        7

typedef enum{ BIOME_PLAINS=0,BIOME_FOREST,BIOME_MOUNTAIN,BIOME_LAVA,BIOME_SNOW,BIOME_COUNT }BiomeType;

typedef struct{
    BiomeType biome;
    float lava_proximity;
    float waterfall_proximity;
}AudioEnv;

void audio_init(int master_vol);
void audio_shutdown(void);
void audio_play(int snd,int vol);
void audio_play_3d(int snd,float sx,float sy,float sz,float lx,float ly,float lz,int max_vol,float max_dist);
void audio_play_file(const char *path,int vol);
void audio_play_music(int track,int fade_ms);
void audio_play_music_file(const char *path,int fade_ms);
void audio_prefetch_music_files(const char *const *paths,int count);
void audio_clear_music_override(int fade_ms);
void audio_set_music_override_volume(float mod);
void audio_set_volumes(int master_vol,int music_vol,int sfx_vol);
void audio_footstep(bool running,float dt);
void audio_reset_footsteps(void);
void audio_update(float dt,bool enemies_nearby,float day_frac,bool is_raining,const AudioEnv *env);

#endif
