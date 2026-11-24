#define _POSIX_C_SOURCE 200809L
#include "engine/audio.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static void wav(const char *path){
    /* PCM silence; only the wind file exists, exercising several aliased fallbacks. */
    static const unsigned char header[]={
        'R','I','F','F',38,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
        1,0,1,0,0x44,0xac,0,0,0x88,0x58,1,0,2,0,16,0,'d','a','t','a',2,0,0,0,0,0};
    FILE *f=fopen(path,"wb");assert(f);assert(fwrite(header,1,sizeof(header),f)==sizeof(header));fclose(f);
}
int main(void){
    char directory[]="/tmp/valwood-audio-test-XXXXXX";
    assert(mkdtemp(directory)&&chdir(directory)==0&&mkdir("sound_effects",0700)==0);
    wav("sound_effects/amb_wind.wav");
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_AUDIO)==0);
    for(int pass=0;pass<2;pass++){
        audio_init(10); audio_shutdown(); audio_shutdown();
    }
    SDL_Quit();puts("PASS shared audio fallback ownership, shutdown, and reinitialization");return 0;
}
