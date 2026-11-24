#ifndef VALWOOD_PRESENTATION_AUDIO_H
#define VALWOOD_PRESENTATION_AUDIO_H
#include <SDL2/SDL_mixer.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
// Dedicated owned channels. All decoding happens at startup; update only mixes.
struct WorldPresentationAudio {
    std::array<Mix_Chunk*,5> clips={};int first=-1;float tension=0,level=0;unsigned cues=0;
    bool init(){
        if(!Mix_QuerySpec(nullptr,nullptr,nullptr))return false;
        const char *names[]={"journey","oathbreaker","chapter","reward","remember"};
        for(unsigned i=0;i<clips.size();i++){char p[256];std::snprintf(p,sizeof p,"audio/presentation/%s.wav",names[i]);clips[i]=Mix_LoadWAV(p);if(!clips[i]){release();return false;}}
        first=Mix_AllocateChannels(-1);if(first<0||first>120||Mix_AllocateChannels(first+3)!=first+3){first=-1;release();return false;}
        Mix_Volume(first,0);Mix_Volume(first+1,0);Mix_PlayChannel(first,clips[0],-1);Mix_PlayChannel(first+1,clips[1],-1);return true;
    }
    void cue(unsigned i){if(first>=0&&i>=2&&i<clips.size()){Mix_Volume(first+2,65);Mix_PlayChannel(first+2,clips[i],0);cues++;}}
    void update(float dt,bool combat,bool focused,float volume){
        if(first<0)return;
        const float t=1-std::exp(-std::clamp(dt,0.f,.1f)*1.8f);
        tension+=(float(combat)-tension)*t;level+=((focused?volume:0.f)-level)*t;
        if(!focused){Mix_Volume(first,0);Mix_Volume(first+1,0);Mix_Volume(first+2,0);return;}
        Mix_Volume(first,int(55*level*(1-tension)));Mix_Volume(first+1,int(63*level*tension));
    }
    void release(){
        if(first>=0){for(int i=0;i<3;i++)Mix_HaltChannel(first+i);if(Mix_AllocateChannels(-1)==first+3)Mix_AllocateChannels(first);first=-1;}
        for(auto *&c:clips){if(c)Mix_FreeChunk(c);c=nullptr;}
    }
};
#endif
