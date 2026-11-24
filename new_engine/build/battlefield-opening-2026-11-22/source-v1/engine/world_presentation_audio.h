#ifndef VALWOOD_PRESENTATION_AUDIO_H
#define VALWOOD_PRESENTATION_AUDIO_H
#include <SDL2/SDL_mixer.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "world_presentation_audio_policy.h"
// Dedicated owned channels. All decoding happens at startup; update only mixes.
struct WorldPresentationAudio {
    std::array<Mix_Chunk*,5> clips={};int first=-1;float tension=0,level=0;unsigned cues=0;
    world_presentation_audio_policy::State policy;
    unsigned duplicate_cues=0,muted_cues=0;
    bool init(){
        if(!Mix_QuerySpec(nullptr,nullptr,nullptr))return false;
        const char *names[]={"journey","oathbreaker","chapter","reward","remember"};
        for(unsigned i=0;i<clips.size();i++){char p[256];std::snprintf(p,sizeof p,"audio/presentation/%s.wav",names[i]);clips[i]=Mix_LoadWAV(p);if(!clips[i]){release();return false;}}
        first=Mix_AllocateChannels(-1);if(first<0||first>120||Mix_AllocateChannels(first+3)!=first+3){first=-1;release();return false;}
        Mix_Volume(first,0);Mix_Volume(first+1,0);Mix_Volume(first+2,0);Mix_PlayChannel(first,clips[0],-1);Mix_PlayChannel(first+1,clips[1],-1);return true;
    }
    void cue(unsigned i){
        if(first<0)return;
        using Decision=world_presentation_audio_policy::CueDecision;
        const auto decision=policy.decision(i);
        if(decision==Decision::Muted){++muted_cues;return;}
        if(decision==Decision::Duplicate){++duplicate_cues;return;}
        if(decision!=Decision::Play)return;
        Mix_Volume(first+2,policy.volumes()[2]);
        if(Mix_PlayChannel(first+2,clips[i],0)>=0){policy.started(i);++cues;}
    }
    void apply_gains(){
        if(first<0)return;
        const auto volumes=policy.volumes();
        for(int i=0;i<3;++i)Mix_Volume(first+i,volumes[unsigned(i)]);
        if(!policy.audible()){
            for(int i=0;i<2;++i)if(!Mix_Paused(first+i))Mix_Pause(first+i);
            // A stale reward/transport sting must not resume after a menu.
            if(Mix_Playing(first+2)||Mix_Paused(first+2))Mix_HaltChannel(first+2);
        }else for(int i=0;i<2;++i)if(Mix_Paused(first+i))Mix_Resume(first+i);
    }
    // Call before opening/quest cue dispatch. Permission includes pause, menus,
    // loading and input focus; this changes gain without advancing any clock.
    void set_context(bool allowed,float volume){
        policy.set_context(allowed,volume);tension=policy.tension;level=policy.level;
        apply_gains();
    }
    void update(float dt,bool combat,bool focused,float volume){
        if(first<0)return;
        policy.set_context(focused,volume);policy.advance(dt,combat);
        tension=policy.tension;level=policy.level;apply_gains();
    }
    void release(){
        if(first>=0){for(int i=0;i<3;i++)Mix_HaltChannel(first+i);if(Mix_AllocateChannels(-1)==first+3)Mix_AllocateChannels(first);first=-1;}
        for(auto *&c:clips){if(c)Mix_FreeChunk(c);c=nullptr;}
        policy={};tension=level=0;
    }
};
#endif
