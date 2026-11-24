#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <SDL2/SDL_mixer.h>

#define AMBILOOP_WIND_CH  28
#define AMBILOOP_LAVA_CH  29
#define AMBILOOP_RAIN_CH  30
#define AMBILOOP_NITE_CH  31
#define AMBILOOP_WF_CH    32

typedef struct{int snd;float min_interval;float weight;}AmbEntry;

static const int BIOME_MUSIC[BIOME_COUNT]={
    MUS_BIOME_PLAINS,MUS_BIOME_FOREST,MUS_BIOME_MTN,MUS_BIOME_LAVA,MUS_BIOME_MTN
};

static const AmbEntry BIOME_AMB_DAY[BIOME_COUNT][5]={
    {{SND_AMB_BIRD,7.f,.45f},{SND_AMB_RIVER,15.f,.25f},{SND_AMB_WIND,8.f,.15f},{SND_AMB_CROW,16.f,.10f},{SND_AMB_CRICKET,25.f,.05f}},
    {{SND_AMB_BIRD,4.f,.50f},{SND_AMB_CRICKET,8.f,.20f},{SND_AMB_RIVER,12.f,.20f},{SND_AMB_CROW,9.f,.10f},{-1,0,0}},
    {{SND_AMB_WIND,5.f,.55f},{SND_AMB_THUNDER,30.f,.15f},{SND_AMB_CROW,10.f,.20f},{SND_AMB_BIRD,18.f,.10f},{-1,0,0}},
    {{SND_AMB_WIND,4.f,.60f},{SND_AMB_CROW,12.f,.25f},{SND_AMB_THUNDER,20.f,.15f},{-1,0,0},{-1,0,0}},
    {{SND_AMB_WIND,4.f,.70f},{SND_AMB_CROW,14.f,.20f},{SND_AMB_BIRD,20.f,.10f},{-1,0,0},{-1,0,0}},
};

static const AmbEntry BIOME_AMB_NIGHT[BIOME_COUNT][5]={
    {{SND_AMB_CRICKET,5.f,.50f},{SND_AMB_OWL,20.f,.25f},{SND_AMB_WIND,12.f,.15f},{SND_AMB_CROW,22.f,.10f},{-1,0,0}},
    {{SND_AMB_CRICKET,4.f,.55f},{SND_AMB_OWL,14.f,.30f},{SND_AMB_WIND,10.f,.10f},{SND_AMB_CROW,18.f,.05f},{-1,0,0}},
    {{SND_AMB_WIND,4.f,.65f},{SND_AMB_OWL,20.f,.20f},{SND_AMB_THUNDER,25.f,.15f},{-1,0,0},{-1,0,0}},
    {{SND_AMB_WIND,4.f,.70f},{SND_AMB_THUNDER,18.f,.30f},{-1,0,0},{-1,0,0},{-1,0,0}},
    {{SND_AMB_WIND,3.f,.80f},{SND_AMB_OWL,25.f,.20f},{-1,0,0},{-1,0,0},{-1,0,0}},
};

typedef struct{
    Mix_Chunk *chunks[SND_COUNT];
    Mix_Music *music[MUS_COUNT];
    bool ok;
    float step_timer;
    int cur_music;
    bool in_combat;
    float combat_timer;
    float lava_vol;
    float wind_vol;
    float amb_accum;
    float amb_timers[5];
    BiomeType cur_biome;
    float biome_change_timer;
    bool is_night;
    bool wind_loop_playing;
    bool lava_loop_playing;
    bool rain_loop_playing;
    bool nite_loop_playing;
    float rain_vol;
    float nite_vol;
    float wf_vol;
    bool wf_loop_playing;
    float thunder_timer;
    int master_vol;
    int music_vol;
    int sfx_vol;
    bool override_music_active;
    char override_music_path[256];
    float override_music_mod;
}AudioSystem;

static AudioSystem g_audio;

#define AUDIO_FILE_CACHE_MAX 64
typedef struct{
    char path[256];
    Mix_Chunk *chunk;
}AudioFileCacheEntry;
static AudioFileCacheEntry g_audio_file_cache[AUDIO_FILE_CACHE_MAX];

#define AUDIO_MUSIC_FILE_CACHE_MAX 32
typedef struct{
    char path[256];
    Mix_Music *music;
}AudioMusicFileCacheEntry;
static AudioMusicFileCacheEntry g_audio_music_file_cache[AUDIO_MUSIC_FILE_CACHE_MAX];

static float clampf_local(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}
static float dist3(float ax,float ay,float az,float bx,float by,float bz){
    float dx=ax-bx,dy=ay-by,dz=az-bz;
    return sqrtf(dx*dx+dy*dy+dz*dz);
}

static int audio_active_channels(void){
    int n=0;
    for(int c=0;c<32;c++) if(Mix_Playing(c)) n++;
    return n;
}

static int audio_music_mix_volume(float mod){
    float clamped_mod=clampf_local(mod,0.0f,1.0f);
    int base=(g_audio.music_vol*g_audio.master_vol*MIX_MAX_VOLUME)/(100*100);
    return (int)(base*clamped_mod);
}

static Mix_Music *audio_music_cache_find(const char *path){
    if(!path||!path[0]) return NULL;
    for(int i=0;i<AUDIO_MUSIC_FILE_CACHE_MAX;i++){
        if(g_audio_music_file_cache[i].path[0]&&strcmp(g_audio_music_file_cache[i].path,path)==0)
            return g_audio_music_file_cache[i].music;
    }
    return NULL;
}

static Mix_Music *audio_music_cache_load(const char *path){
    Mix_Music *music=NULL;
    if(!g_audio.ok||!path||!path[0]) return NULL;
    music=audio_music_cache_find(path);
    if(music) return music;
    for(int i=0;i<AUDIO_MUSIC_FILE_CACHE_MAX;i++){
        if(g_audio_music_file_cache[i].path[0]) continue;
        music=Mix_LoadMUS(path);
        if(!music){
            printf("[Audio] custom music load failed: %s (%s)\n",path,Mix_GetError());
            return NULL;
        }
        snprintf(g_audio_music_file_cache[i].path,sizeof(g_audio_music_file_cache[i].path),"%s",path);
        g_audio_music_file_cache[i].music=music;
        printf("[Audio] Custom music: %s\n",path);
        return music;
    }
    printf("[Audio] custom music cache full, cannot cache: %s\n",path);
    return NULL;
}

void audio_init(int master_vol){
    memset(&g_audio,0,sizeof(g_audio));
    g_audio.master_vol=master_vol;
    if(Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048)<0){
        printf("[Audio] SDL_mixer init failed: %s\n",Mix_GetError());
        return;
    }
    Mix_AllocateChannels(33);
    g_audio.ok=true;
    g_audio.cur_music=-1;
    g_audio.music_vol=70;
    g_audio.sfx_vol=80;
    g_audio.override_music_mod=1.0f;

    const char *sfx[SND_COUNT]={
        "sound_effects/snd_step.wav","sound_effects/snd_run.wav","sound_effects/snd_attack.wav",
        "sound_effects/snd_hit.wav","sound_effects/snd_death.wav","sound_effects/snd_enemy_die.wav",
        "sound_effects/snd_jump.wav","sound_effects/snd_fireball.wav","sound_effects/snd_fireball_hit.wav",
        "sound_effects/snd_heal.wav","sound_effects/snd_menu_click.wav","sound_effects/snd_level_up.wav",
        "sound_effects/snd_growl.wav","sound_effects/snd_enemy_atk.wav","sound_effects/amb_wind.wav",
        "sound_effects/amb_bird.wav","sound_effects/amb_cricket.wav","sound_effects/amb_lava.wav",
        "sound_effects/amb_crow.wav","sound_effects/amb_river.wav","sound_effects/amb_thunder.wav",
        "sound_effects/amb_owl.wav","sound_effects/amb_loop_rain.wav","sound_effects/amb_loop_night.wav",
        "sound_effects/snd_dash.wav","sound_effects/snd_shield.wav","sound_effects/snd_meteor.wav",
        "sound_effects/snd_animal_0.wav","sound_effects/snd_animal_1.wav","sound_effects/snd_animal_2.wav",
        "sound_effects/amb_waterfall.wav",
    };
    for(int i=0;i<SND_COUNT;i++){
        g_audio.chunks[i]=Mix_LoadWAV(sfx[i]);
        if(g_audio.chunks[i]) printf("[Audio] SFX: %s\n",sfx[i]);
        else printf("[Audio] optional SFX not found: %s\n",sfx[i]);
    }

    if(!g_audio.chunks[SND_RUN]&&g_audio.chunks[SND_STEP]) g_audio.chunks[SND_RUN]=g_audio.chunks[SND_STEP];
    if(!g_audio.chunks[SND_FIREBALL_HIT]&&g_audio.chunks[SND_HIT]) g_audio.chunks[SND_FIREBALL_HIT]=g_audio.chunks[SND_HIT];
    if(!g_audio.chunks[SND_ENEMY_ATTACK]&&g_audio.chunks[SND_ATTACK]) g_audio.chunks[SND_ENEMY_ATTACK]=g_audio.chunks[SND_ATTACK];
    if(!g_audio.chunks[SND_DASH]&&g_audio.chunks[SND_JUMP]) g_audio.chunks[SND_DASH]=g_audio.chunks[SND_JUMP];
    if(!g_audio.chunks[SND_SHIELD]&&g_audio.chunks[SND_HIT]) g_audio.chunks[SND_SHIELD]=g_audio.chunks[SND_HIT];
    if(!g_audio.chunks[SND_METEOR]&&g_audio.chunks[SND_FIREBALL]) g_audio.chunks[SND_METEOR]=g_audio.chunks[SND_FIREBALL];
    if(!g_audio.chunks[SND_AMB_CROW]&&g_audio.chunks[SND_AMB_BIRD]) g_audio.chunks[SND_AMB_CROW]=g_audio.chunks[SND_AMB_BIRD];
    if(!g_audio.chunks[SND_AMB_OWL]&&g_audio.chunks[SND_AMB_CROW]) g_audio.chunks[SND_AMB_OWL]=g_audio.chunks[SND_AMB_CROW];
    if(!g_audio.chunks[SND_AMB_RIVER]&&g_audio.chunks[SND_AMB_WIND]) g_audio.chunks[SND_AMB_RIVER]=g_audio.chunks[SND_AMB_WIND];
    if(!g_audio.chunks[SND_AMB_THUNDER]&&g_audio.chunks[SND_AMB_WIND]) g_audio.chunks[SND_AMB_THUNDER]=g_audio.chunks[SND_AMB_WIND];
    if(!g_audio.chunks[SND_LOOP_RAIN]&&g_audio.chunks[SND_AMB_RIVER]) g_audio.chunks[SND_LOOP_RAIN]=g_audio.chunks[SND_AMB_RIVER];
    if(!g_audio.chunks[SND_LOOP_NIGHT]&&g_audio.chunks[SND_AMB_CRICKET]) g_audio.chunks[SND_LOOP_NIGHT]=g_audio.chunks[SND_AMB_CRICKET];
    if(!g_audio.chunks[SND_WF_LOOP]&&g_audio.chunks[SND_AMB_RIVER]) g_audio.chunks[SND_WF_LOOP]=g_audio.chunks[SND_AMB_RIVER];

    g_audio.thunder_timer=20.0f+(float)(rand()%20);

    const char *mus[MUS_COUNT]={
        "background_music/mus_explore.ogg","background_music/mus_combat.ogg","background_music/mus_menu.ogg",
        "background_music/mus_biome_mountain.ogg","background_music/mus_biome_forest.ogg",
        "background_music/mus_biome_lava.ogg","background_music/mus_biome_plains.ogg",
    };
    for(int i=0;i<MUS_COUNT;i++){
        g_audio.music[i]=Mix_LoadMUS(mus[i]);
        if(!g_audio.music[i]){
            char wav[64];
            snprintf(wav,sizeof(wav),"%.*s.wav",(int)(strlen(mus[i])-4),mus[i]);
            g_audio.music[i]=Mix_LoadMUS(wav);
        }
        if(g_audio.music[i]) printf("[Audio] Music: %s\n",mus[i]);
        else printf("[Audio] optional music not found: %s\n",mus[i]);
    }
}

void audio_play(int snd,int vol){
    if(!g_audio.ok||snd<0||snd>=SND_COUNT||!g_audio.chunks[snd])return;
    int sv=(vol*g_audio.sfx_vol*g_audio.master_vol)/(100*100);
    Mix_VolumeChunk(g_audio.chunks[snd],sv);
    Mix_PlayChannel(-1,g_audio.chunks[snd],0);
}

void audio_play_3d(int snd,float sx,float sy,float sz,float lx,float ly,float lz,int max_vol,float max_dist){
    if(!g_audio.ok||snd<0||snd>=SND_COUNT||!g_audio.chunks[snd])return;
    float d=dist3(sx,sy,sz,lx,ly,lz);
    if(d>max_dist)return;
    int vol=(int)(max_vol*(1.0f-d/max_dist));
    if(vol<4)return;
    if(audio_active_channels()>=20&&vol<30)return;
    if(audio_active_channels()>=26&&vol<55)return;
    audio_play(snd,vol);
}

void audio_play_file(const char *path,int vol){
    Mix_Chunk *chunk=NULL;
    int sv;
    if(!g_audio.ok||!path||!path[0]) return;
    for(int i=0;i<AUDIO_FILE_CACHE_MAX;i++){
        if(g_audio_file_cache[i].path[0]&&strcmp(g_audio_file_cache[i].path,path)==0){
            chunk=g_audio_file_cache[i].chunk;
            break;
        }
    }
    if(!chunk){
        for(int i=0;i<AUDIO_FILE_CACHE_MAX;i++){
            if(g_audio_file_cache[i].path[0]) continue;
            chunk=Mix_LoadWAV(path);
            if(!chunk){
                printf("[Audio] custom SFX load failed: %s (%s)\n",path,Mix_GetError());
                return;
            }
            snprintf(g_audio_file_cache[i].path,sizeof(g_audio_file_cache[i].path),"%s",path);
            g_audio_file_cache[i].chunk=chunk;
            printf("[Audio] Custom SFX: %s\n",path);
            break;
        }
    }
    if(!chunk) return;
    sv=(vol*g_audio.sfx_vol*g_audio.master_vol)/(100*100);
    Mix_VolumeChunk(chunk,sv);
    Mix_PlayChannel(-1,chunk,0);
}

void audio_play_music(int track,int fade_ms){
    if(!g_audio.ok||track<0||track>=MUS_COUNT)return;
    g_audio.override_music_active=false;
    g_audio.override_music_path[0]=0;
    g_audio.override_music_mod=1.0f;
    if(g_audio.cur_music==track)return;
    if(!g_audio.music[track]){g_audio.cur_music=track;return;}
    if(fade_ms>0) Mix_FadeOutMusic(fade_ms);
    else Mix_HaltMusic();
    Mix_FadeInMusic(g_audio.music[track],-1,fade_ms>0?fade_ms:0);
    Mix_VolumeMusic(audio_music_mix_volume(1.0f));
    g_audio.cur_music=track;
    printf("[Audio] Music -> track %d\n",track);
}

void audio_play_music_file(const char *path,int fade_ms){
    Mix_Music *music=NULL;
    if(!g_audio.ok||!path||!path[0]) return;
    if(g_audio.override_music_active&&strcmp(g_audio.override_music_path,path)==0){
        Mix_VolumeMusic(audio_music_mix_volume(g_audio.override_music_mod));
        return;
    }
    music=audio_music_cache_load(path);
    if(!music) return;
    if(fade_ms>0) Mix_FadeOutMusic(fade_ms);
    else Mix_HaltMusic();
    Mix_FadeInMusic(music,-1,fade_ms>0?fade_ms:0);
    g_audio.cur_music=-1;
    g_audio.override_music_active=true;
    snprintf(g_audio.override_music_path,sizeof(g_audio.override_music_path),"%s",path);
    Mix_VolumeMusic(audio_music_mix_volume(g_audio.override_music_mod));
}

void audio_prefetch_music_files(const char *const *paths,int count){
    if(!g_audio.ok||!paths||count<=0) return;
    for(int i=0;i<count;i++){
        if(!paths[i]||!paths[i][0]) continue;
        audio_music_cache_load(paths[i]);
    }
}

void audio_clear_music_override(int fade_ms){
    int track;
    if(!g_audio.ok) return;
    if(!g_audio.override_music_active) return;
    g_audio.override_music_active=false;
    g_audio.override_music_path[0]=0;
    g_audio.override_music_mod=1.0f;
    if(g_audio.in_combat){
        audio_play_music(MUS_COMBAT,fade_ms);
        return;
    }
    track=BIOME_MUSIC[g_audio.cur_biome];
    if(track<0||track>=MUS_COUNT||!g_audio.music[track]) track=MUS_EXPLORE;
    audio_play_music(track,fade_ms);
}

void audio_set_music_override_volume(float mod){
    g_audio.override_music_mod=clampf_local(mod,0.0f,1.0f);
    if(g_audio.ok&&!g_audio.in_combat){
        float world_mod=(1.0f-g_audio.lava_vol*0.4f)*(1.0f-g_audio.wf_vol*0.15f);
        if(g_audio.override_music_active) world_mod*=g_audio.override_music_mod;
        Mix_VolumeMusic(audio_music_mix_volume(world_mod));
    }
}

void audio_set_volumes(int master_vol,int music_vol,int sfx_vol){
    g_audio.master_vol=master_vol;
    g_audio.music_vol=music_vol;
    g_audio.sfx_vol=sfx_vol;
    if(g_audio.ok) Mix_VolumeMusic(audio_music_mix_volume(1.0f));
}

void audio_footstep(bool running,float dt){
    if(!g_audio.ok)return;
    float interval=running?0.30f:0.48f;
    g_audio.step_timer-=dt;
    if(g_audio.step_timer<=0){
        g_audio.step_timer=interval;
        audio_play(running?SND_RUN:SND_STEP,65+(rand()%15));
    }
}

void audio_reset_footsteps(void){
    g_audio.step_timer=0.0f;
}

void audio_update(float dt,bool enemies_nearby,float day_frac,bool is_raining,const AudioEnv *env){
    if(!g_audio.ok||!env)return;

    Mix_VolumeMusic(audio_music_mix_volume(1.0f));

    bool is_night=(day_frac<0.20f||day_frac>0.75f);
    float night_f=clampf_local(day_frac<0.5f?(0.20f-day_frac)/0.10f:(day_frac-0.75f)/0.10f,0,1);
    g_audio.is_night=is_night;

    if(enemies_nearby){
        g_audio.combat_timer=6.0f;
        if(!g_audio.in_combat){ g_audio.in_combat=true; audio_play_music(MUS_COMBAT,1500); }
    } else {
        g_audio.combat_timer-=dt;
        if(g_audio.combat_timer<0&&g_audio.in_combat){
            g_audio.in_combat=false;
            if(g_audio.override_music_active&&g_audio.override_music_path[0]) audio_play_music_file(g_audio.override_music_path,1500);
            else audio_play_music(BIOME_MUSIC[g_audio.cur_biome],3000);
        }
    }

    if(!g_audio.in_combat){
        if(g_audio.override_music_active){
            g_audio.cur_biome=env->biome;
        }else{
            g_audio.biome_change_timer-=dt;
            if(g_audio.biome_change_timer<=0){
                g_audio.biome_change_timer=4.0f;
                if(env->biome!=g_audio.cur_biome){
                    g_audio.cur_biome=env->biome;
                    int track=BIOME_MUSIC[env->biome];
                    if(!g_audio.music[track]) track=MUS_EXPLORE;
                    audio_play_music(track,3000);
                }
            }
        }
    }

    g_audio.lava_vol+=(clampf_local(env->lava_proximity,0,1)-g_audio.lava_vol)*dt*0.5f;

    float wind_target=0.0f;
    if(g_audio.cur_biome==BIOME_MOUNTAIN||g_audio.cur_biome==BIOME_SNOW) wind_target=0.55f+(is_night?0.1f:0.0f);
    else if(g_audio.cur_biome==BIOME_LAVA) wind_target=0.35f;
    g_audio.wind_vol+=(wind_target-g_audio.wind_vol)*dt*0.3f;
    if(g_audio.chunks[SND_AMB_WIND]){
        int wv=(int)(g_audio.wind_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100));
        if(wind_target>0.05f&&!g_audio.wind_loop_playing){
            Mix_Volume(AMBILOOP_WIND_CH,wv);
            Mix_PlayChannel(AMBILOOP_WIND_CH,g_audio.chunks[SND_AMB_WIND],-1);
            g_audio.wind_loop_playing=true;
        } else if(g_audio.wind_loop_playing){
            if(wind_target<0.02f){Mix_HaltChannel(AMBILOOP_WIND_CH);g_audio.wind_loop_playing=false;}
            else Mix_Volume(AMBILOOP_WIND_CH,wv);
        }
    }

    if(g_audio.chunks[SND_AMB_LAVA]){
        int lv=(int)(g_audio.lava_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100)*0.7f);
        if(g_audio.lava_vol>0.05f&&!g_audio.lava_loop_playing){
            Mix_Volume(AMBILOOP_LAVA_CH,lv);
            Mix_PlayChannel(AMBILOOP_LAVA_CH,g_audio.chunks[SND_AMB_LAVA],-1);
            g_audio.lava_loop_playing=true;
        } else if(g_audio.lava_loop_playing){
            if(g_audio.lava_vol<0.02f){Mix_HaltChannel(AMBILOOP_LAVA_CH);g_audio.lava_loop_playing=false;}
            else Mix_Volume(AMBILOOP_LAVA_CH,lv);
        }
    }

    float rain_target=is_raining?0.75f:0.0f;
    g_audio.rain_vol+=(rain_target-g_audio.rain_vol)*dt*1.2f;
    if(g_audio.chunks[SND_LOOP_RAIN]){
        int rv=(int)(g_audio.rain_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100));
        if(rain_target>0.05f&&!g_audio.rain_loop_playing){
            Mix_Volume(AMBILOOP_RAIN_CH,rv);
            Mix_PlayChannel(AMBILOOP_RAIN_CH,g_audio.chunks[SND_LOOP_RAIN],-1);
            g_audio.rain_loop_playing=true;
        } else if(g_audio.rain_loop_playing){
            if(rain_target<0.02f){Mix_HaltChannel(AMBILOOP_RAIN_CH);g_audio.rain_loop_playing=false;}
            else Mix_Volume(AMBILOOP_RAIN_CH,rv);
        }
    }

    float nite_target=0.0f;
    if(g_audio.cur_biome!=BIOME_LAVA&&g_audio.cur_biome!=BIOME_SNOW&&!is_raining) nite_target=night_f*0.55f;
    g_audio.nite_vol+=(nite_target-g_audio.nite_vol)*dt*0.5f;
    if(g_audio.chunks[SND_LOOP_NIGHT]){
        int nv=(int)(g_audio.nite_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100));
        if(nite_target>0.05f&&!g_audio.nite_loop_playing){
            Mix_Volume(AMBILOOP_NITE_CH,nv);
            Mix_PlayChannel(AMBILOOP_NITE_CH,g_audio.chunks[SND_LOOP_NIGHT],-1);
            g_audio.nite_loop_playing=true;
        } else if(g_audio.nite_loop_playing){
            if(nite_target<0.02f){Mix_HaltChannel(AMBILOOP_NITE_CH);g_audio.nite_loop_playing=false;}
            else Mix_Volume(AMBILOOP_NITE_CH,nv);
        }
    }

    if(is_raining){
        g_audio.thunder_timer-=dt;
        if(g_audio.thunder_timer<=0){
            audio_play(SND_AMB_THUNDER,50+(rand()%30));
            g_audio.thunder_timer=12.0f+(float)(rand()%28);
        }
    } else if(g_audio.thunder_timer<15.0f){
        g_audio.thunder_timer=15.0f;
    }

    {
        float wf_target=clampf_local(env->waterfall_proximity,0.0f,1.0f);
        g_audio.wf_vol+=(wf_target-g_audio.wf_vol)*dt*1.8f;
        if(g_audio.chunks[SND_WF_LOOP]){
            int wfv=(int)(g_audio.wf_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100)*0.85f);
            if(g_audio.wf_vol>0.04f&&!g_audio.wf_loop_playing){
                Mix_Volume(AMBILOOP_WF_CH,wfv);
                Mix_PlayChannel(AMBILOOP_WF_CH,g_audio.chunks[SND_WF_LOOP],-1);
                g_audio.wf_loop_playing=true;
            } else if(g_audio.wf_loop_playing){
                if(g_audio.wf_vol<0.02f){Mix_HaltChannel(AMBILOOP_WF_CH);g_audio.wf_loop_playing=false;}
                else Mix_Volume(AMBILOOP_WF_CH,wfv);
            }
        }
    }

    if(!g_audio.in_combat){
        float music_mod=(1.0f-g_audio.lava_vol*0.4f)*(1.0f-g_audio.wf_vol*0.15f);
        if(g_audio.override_music_active) music_mod*=g_audio.override_music_mod;
        Mix_VolumeMusic(audio_music_mix_volume(music_mod));
    }

    for(int i=0;i<5;i++) if(g_audio.amb_timers[i]>0) g_audio.amb_timers[i]-=dt;
    g_audio.amb_accum+=dt;
    {
        float trigger_interval=is_raining?14.0f:(is_night?8.0f:6.0f);
        if(g_audio.amb_accum>=trigger_interval*(0.7f+(float)(rand()%100)/300.0f)){
            g_audio.amb_accum=0;
            const AmbEntry *table=is_night?BIOME_AMB_NIGHT[g_audio.cur_biome]:BIOME_AMB_DAY[g_audio.cur_biome];
            float roll=(float)(rand()%1000)/1000.0f,cum=0.0f;
            for(int i=0;i<5;i++){
                const AmbEntry *ae=&table[i];
                if(ae->snd<0) break;
                cum+=ae->weight;
                if(roll<=cum&&g_audio.amb_timers[i]<=0){
                    if(ae->snd!=SND_AMB_WIND&&ae->snd!=SND_AMB_LAVA){
                        if(is_raining&&ae->snd==SND_AMB_BIRD) break;
                        int vol=(int)((35+(rand()%25))*g_audio.sfx_vol/100);
                        audio_play(ae->snd,vol);
                        g_audio.amb_timers[i]=ae->min_interval;
                    }
                    break;
                }
            }
        }
    }
}

void audio_shutdown(void){
    if(!g_audio.ok)return;
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    /* Fallback effects alias earlier slots. Each allocation has one owner. */
    for(int i=0;i<SND_COUNT;i++) if(g_audio.chunks[i]){
        bool shared=false;
        for(int j=0;j<i;j++) if(g_audio.chunks[j]==g_audio.chunks[i]){ shared=true; break; }
        if(!shared) Mix_FreeChunk(g_audio.chunks[i]);
    }
    for(int i=0;i<MUS_COUNT;i++) if(g_audio.music[i]) Mix_FreeMusic(g_audio.music[i]);
    for(int i=0;i<AUDIO_FILE_CACHE_MAX;i++) if(g_audio_file_cache[i].chunk) Mix_FreeChunk(g_audio_file_cache[i].chunk);
    for(int i=0;i<AUDIO_MUSIC_FILE_CACHE_MAX;i++) if(g_audio_music_file_cache[i].music) Mix_FreeMusic(g_audio_music_file_cache[i].music);
    Mix_CloseAudio();
    memset(&g_audio,0,sizeof(g_audio));
    memset(g_audio_file_cache,0,sizeof(g_audio_file_cache));
    memset(g_audio_music_file_cache,0,sizeof(g_audio_music_file_cache));
}
