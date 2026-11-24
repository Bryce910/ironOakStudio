#include "engine/world_battlefield_audio.h"
#include <cassert>
#include <cstdio>
using namespace world_battlefield_audio;
int main(int argc,char**argv){assert(argc==3);SDL_setenv("SDL_AUDIODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_AUDIO)==0);assert(Mix_OpenAudio(44100,AUDIO_S16SYS,2,512)==0);assert(Mix_AllocateChannels(32)==32);Mix_Volume(0,47);Mix_SetPanning(0,81,171);
 System a;std::string error;assert(a.init(argv[1],argv[2],error));assert(a.stats().assets>=4&&a.stats().voices==8);assert(Mix_AllocateChannels(-1)==40);
 Frame f;f.visible=true;f.focused=true;f.dt=1./60;f.listener[0]=-12115;f.listener[1]=13;f.listener[2]=5150;
 assert(a.update(f));assert(a.event(Cue::Ambush,{-12114,12.8,5150}));assert(a.stats().played==1);
 f.seconds=.1;f.volume=0;assert(a.update(f));assert(Mix_Volume(32,-1)==0);
 f.paused=true;f.seconds=.2;assert(a.update(f));for(int ch=32;ch<40;++ch)assert(!Mix_Playing(ch));assert(!a.event(Cue::Stab,{-12114,12.8,5150}));
 auto before=a.stats().timeline_events;f.paused=false;f.volume=1;f.seconds=30;assert(a.update(f));assert(a.stats().timeline_events==before);
 f.seconds=30.1;assert(a.update(f));f.focused=false;assert(a.update(f));for(int ch=32;ch<40;++ch)assert(!Mix_Playing(ch));
 f.focused=true;f.loading=true;f.seconds=30.2;assert(a.update(f));assert(!a.event(Cue::Body,{-12114,12.8,5150}));f.loading=false;f.visible=false;f.seconds=60;assert(a.update(f));assert(!a.event(Cue::Body,{-12114,12.8,5150}));
 f.visible=true;f.seconds=0;assert(a.update(f));assert(a.stats().timeline_events==before);for(int k=1;k<1200;++k){f.seconds=k/60.;assert(a.update(f));}
 assert(a.stats().timeline_events>10);assert(a.stats().voices==8);assert(Mix_Volume(0,-1)==47);
 // A later audio owner appends channels; release cannot shrink that owner.
 assert(Mix_AllocateChannels(44)==44);a.release();assert(Mix_AllocateChannels(-1)==44);assert(Mix_Volume(0,-1)==47);
 assert(!a.event(Cue::Body,{0,0,0}));a.release();Mix_CloseAudio();SDL_Quit();std::puts("Battlefield audio passed: isolated voices, gain/pause/focus/loading gates, no catch-up, timeline and release ownership");}
