#include "engine/frame_runtime.h"
#include "engine/pause_menu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void intro_sequence_set_defaults(IntroSequenceState *state){
    static const char *defaults[]={
        "Night presses against the glass while the city hums in its sleep.",
        "Something small and stubborn still burns inside the room, asking you to begin.",
        "When the last thought settles, morning waits in the apartment."
    };
    int count=(int)(sizeof(defaults)/sizeof(defaults[0]));
    state->section_count=count;
    for(int i=0;i<count;i++){
        snprintf(state->sections[i],sizeof(state->sections[i]),"%s",defaults[i]);
    }
}

void intro_sequence_init(IntroSequenceState *state,unsigned int seed){
    if(!state) return;
    memset(state,0,sizeof(*state));
    if(seed==0u) seed=1u;
    intro_sequence_set_defaults(state);
    for(int i=0;i<INTRO_FIREFLY_COUNT;i++){
        seed=seed*1664525u+1013904223u;
        state->fireflies[i].x=((float)(seed&0xffffu)/(float)0xffffu);
        seed=seed*1664525u+1013904223u;
        state->fireflies[i].y=((float)(seed&0xffffu)/(float)0xffffu);
        seed=seed*1664525u+1013904223u;
        state->fireflies[i].vx=(((float)(seed&0xffffu)/(float)0xffffu)-0.5f)*0.035f;
        seed=seed*1664525u+1013904223u;
        state->fireflies[i].vy=(((float)(seed&0xffffu)/(float)0xffffu)-0.5f)*0.028f;
        seed=seed*1664525u+1013904223u;
        state->fireflies[i].phase=((float)(seed&0xffffu)/(float)0xffffu)*(float)(M_PI*2.0);
        seed=seed*1664525u+1013904223u;
        state->fireflies[i].size=1.0f+((float)(seed&0xffffu)/(float)0xffffu)*2.4f;
    }
}

static void intro_story_trim(char *text){
    size_t len;
    char *start=text;
    while(*start==' '||*start=='\t'||*start=='\r'||*start=='\n') start++;
    if(start!=text) memmove(text,start,strlen(start)+1);
    len=strlen(text);
    while(len>0){
        char c=text[len-1];
        if(c!=' '&&c!='\t'&&c!='\r'&&c!='\n') break;
        text[--len]=0;
    }
}

static void intro_story_commit_section(IntroSequenceState *state,char *section,int *count){
    if(!state||!section||!count) return;
    intro_story_trim(section);
    if(!section[0]||*count>=INTRO_STORY_SECTION_MAX) return;
    snprintf(state->sections[*count],sizeof(state->sections[*count]),"%s",section);
    (*count)++;
    section[0]=0;
}

bool intro_sequence_load_from_file(IntroSequenceState *state,const char *path){
    FILE *f;
    char line[512];
    char section[INTRO_STORY_TEXT_MAX];
    int count=0;
    if(!state||!path) return false;
    f=fopen(path,"r");
    if(!f) return false;
    memset(state->sections,0,sizeof(state->sections));
    section[0]=0;
    while(fgets(line,sizeof(line),f)){
        char trimmed[sizeof(line)];
        snprintf(trimmed,sizeof(trimmed),"%s",line);
        intro_story_trim(trimmed);
        if(!trimmed[0]){
            if(section[0]){
                size_t used=strlen(section);
                if(used+1<sizeof(section)){
                    section[used++]='\n';
                    section[used]=0;
                }
            }
            continue;
        }
        if(trimmed[0]=='#') continue;
        if(strcmp(trimmed,"---")==0){
            intro_story_commit_section(state,section,&count);
            continue;
        }
        {
            size_t used=strlen(section);
            size_t add=strlen(trimmed);
            if(used>0&&section[used-1]!='\n'&&used+1<sizeof(section)){
                section[used++]=' ';
                section[used]=0;
            }
            if(used<sizeof(section)-1){
                size_t cap=sizeof(section)-used-1;
                if(add>cap) add=cap;
                memcpy(section+used,trimmed,add);
                section[used+add]=0;
            }
        }
    }
    fclose(f);
    intro_story_commit_section(state,section,&count);
    if(count<=0){
        intro_sequence_set_defaults(state);
        return false;
    }
    state->section_count=count;
    return true;
}

void intro_sequence_activate(IntroSequenceState *state){
    if(!state||state->section_count<=0) return;
    state->active=true;
    state->completed=false;
    state->current_section=0;
    state->section_alpha=0.0f;
    state->section_timer=0.0f;
    state->prompt_pulse=0.0f;
}

bool intro_sequence_is_active(const IntroSequenceState *state){
    return state&&state->active&&state->section_count>0;
}

static bool intro_sequence_advance(IntroSequenceState *state){
    if(!intro_sequence_is_active(state)) return false;
    if(state->section_alpha<0.98f){
        state->section_alpha=1.0f;
        if(state->section_timer<0.25f) state->section_timer=0.25f;
        return true;
    }
    if(state->current_section+1<state->section_count){
        state->current_section++;
        state->section_alpha=0.0f;
        state->section_timer=0.0f;
        return true;
    }
    state->active=false;
    state->completed=true;
    return true;
}

bool intro_sequence_handle_keydown(IntroSequenceState *state,SDL_Keycode keycode){
    if(!intro_sequence_is_active(state)) return false;
    switch(keycode){
        case SDLK_SPACE:
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_ESCAPE:
        case SDLK_e:
            return intro_sequence_advance(state);
        default:
            return false;
    }
}

bool intro_sequence_handle_mouse_button(IntroSequenceState *state,Uint8 button){
    if(!intro_sequence_is_active(state)) return false;
    if(button==SDL_BUTTON_LEFT||button==SDL_BUTTON_RIGHT||button==SDL_BUTTON_MIDDLE)
        return intro_sequence_advance(state);
    return false;
}

bool intro_sequence_handle_controller_button(IntroSequenceState *state,Uint8 button){
    if(!intro_sequence_is_active(state)) return false;
    switch(button){
        case SDL_CONTROLLER_BUTTON_A:
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_X:
        case SDL_CONTROLLER_BUTTON_START:
            return intro_sequence_advance(state);
        default:
            return false;
    }
}

static void intro_draw_wrapped(void *hud,FrameHUDAPI *hud_api,float x,float y,float scale,
                               float max_width,const char *text,
                               float r,float g,float b,float a){
    char line[256];
    char word[128];
    const char *p=text;
    float line_h=scale*20.0f;
    int approx_chars=(int)(max_width/fmaxf(scale*8.0f,1.0f));
    if(approx_chars<18) approx_chars=18;
    line[0]=0;
    while(*p){
        size_t wl=0;
        while(*p==' ') p++;
        if(*p=='\n'){
            if(line[0]){
                hud_api->text(hud,x,y,scale,line,r,g,b,a);
                y+=line_h;
                line[0]=0;
            }else{
                y+=line_h*0.65f;
            }
            p++;
            continue;
        }
        while(p[wl]&&p[wl]!=' '&&p[wl]!='\n'&&wl<sizeof(word)-1) wl++;
        memcpy(word,p,wl);
        word[wl]=0;
        p+=wl;
        if(!line[0]){
            snprintf(line,sizeof(line),"%s",word);
        }else if((int)(strlen(line)+1+strlen(word))<=approx_chars){
            size_t len=strlen(line);
            if(len+1<sizeof(line)-1){
                line[len++]=' ';
                line[len]=0;
                strncat(line,word,sizeof(line)-strlen(line)-1);
            }
        }else{
            hud_api->text(hud,x,y,scale,line,r,g,b,a);
            y+=line_h;
            snprintf(line,sizeof(line),"%s",word);
        }
    }
    if(line[0]) hud_api->text(hud,x,y,scale,line,r,g,b,a);
}

static void intro_sequence_draw(void *hud,int win_w,int win_h,IntroSequenceState *intro,
                                FrameHUDAPI *hud_api,float dt){
    float fw=(float)win_w;
    float fh=(float)win_h;
    float box_w=fminf(fw*0.78f,860.0f);
    float box_x=(fw-box_w)*0.5f;
    float box_y=fh*0.24f;
    float box_h=fminf(fh*0.42f,320.0f);
    intro->section_timer+=dt;
    intro->prompt_pulse+=dt;
    intro->section_alpha+=dt/0.9f;
    if(intro->section_alpha>1.0f) intro->section_alpha=1.0f;

    hud_api->set_depth_test(false);
    hud_api->rect(hud,0,0,fw,fh,0.01f,0.01f,0.02f,1.0f);
    hud_api->rect(hud,0,0,fw,fh,0.04f,0.06f,0.08f,0.28f);
    for(int i=0;i<INTRO_FIREFLY_COUNT;i++){
        IntroFirefly *fly=&intro->fireflies[i];
        float twinkle=0.55f+0.45f*sinf(intro->prompt_pulse*1.7f+fly->phase);
        float px=fly->x*fw;
        float py=fly->y*fh;
        float size=fly->size*(0.7f+twinkle*0.7f);
        fly->x+=fly->vx*dt;
        fly->y+=fly->vy*dt;
        if(fly->x<0.02f||fly->x>0.98f) fly->vx=-fly->vx;
        if(fly->y<0.06f||fly->y>0.94f) fly->vy=-fly->vy;
        if(fly->x<0.02f) fly->x=0.02f;
        if(fly->x>0.98f) fly->x=0.98f;
        if(fly->y<0.06f) fly->y=0.06f;
        if(fly->y>0.94f) fly->y=0.94f;
        hud_api->rect(hud,px-size*2.4f,py-size*2.4f,size*4.8f,size*4.8f,0.42f,0.34f,0.08f,0.08f*twinkle);
        hud_api->rect(hud,px-size,py-size,size*2.0f,size*2.0f,0.95f,0.78f,0.20f,0.24f*twinkle);
        hud_api->rect(hud,px-size*0.35f,py-size*0.35f,size*0.7f,size*0.7f,1.0f,0.95f,0.72f,0.95f);
    }

    hud_api->rect(hud,box_x,box_y,box_w,box_h,0.02f,0.03f,0.04f,0.70f);
    hud_api->rect(hud,box_x+2.0f,box_y+2.0f,box_w-4.0f,box_h-4.0f,0.14f,0.12f,0.05f,0.12f);
    hud_api->text(hud,box_x+34.0f,box_y+26.0f,1.1f,"Fireflies in the Dark",0.96f,0.86f,0.58f,0.92f*intro->section_alpha);
    intro_draw_wrapped(hud,hud_api,box_x+34.0f,box_y+86.0f,1.55f,box_w-68.0f,
                       intro->sections[intro->current_section],
                       0.93f,0.92f,0.88f,0.90f*intro->section_alpha);
    {
        char footer[96];
        float prompt_a=0.35f+0.35f*(0.5f+0.5f*sinf(intro->prompt_pulse*3.2f));
        snprintf(footer,sizeof(footer),
                 intro->current_section+1<intro->section_count?
                 "Click, Space, Enter, or A to continue":
                 "Click, Space, Enter, or A to begin");
        hud_api->text(hud,box_x+34.0f,box_y+box_h-42.0f,1.0f,footer,0.86f,0.82f,0.68f,prompt_a);
    }
    hud_api->flush(hud);
    hud_api->set_depth_test(true);
}

void frame_runtime_draw_tail(void *hud,void *window,int win_w,int win_h,
                             FrameRuntimeState *state,PauseMenu *pause,
                             IntroSequenceState *intro,
                             PauseMenuAPI *pause_api,FrameHUDAPI *hud_api,
                             bool *running,bool *restart_to_menu,float dt){
    if(state->dead){
        float cx=(float)win_w*0.5f, cy=(float)win_h*0.38f;
        hud_api->rect(hud,0,0,(float)win_w,(float)win_h,0,0,0,0.5f);
        hud_api->text(hud,cx-72,cy,3.5f,"YOU DIED",0.85f,0.05f,0.05f,1.0f);
        hud_api->text(hud,cx-108,cy+52,1.6f,"ENTER to Respawn",0.90f,0.90f,0.90f,0.85f);
        hud_api->flush(hud);
    }

    {
        bool want_resume=false, want_quit_menu=false, want_quit_game=false;
        pause_menu_draw(pause,hud,win_w,win_h,state->total_time,pause_api,
                        &want_resume,&want_quit_menu,&want_quit_game);
        (void)want_resume;
        if(want_quit_game) *running=false;
        if(want_quit_menu){
            *running=false;
            *restart_to_menu=true;
        }
    }

    if(intro&&intro->completed){
        state->intro_fade=1.0f;
        intro->completed=false;
    }

    if(intro_sequence_is_active(intro)){
        intro_sequence_draw(hud,win_w,win_h,intro,hud_api,dt);
    }else if(state->intro_fade>0.0f){
        state->intro_fade -= dt/1.5f;
        if(state->intro_fade<0.0f) state->intro_fade=0.0f;
        if(state->intro_fade>1.0f) state->intro_fade=1.0f;
        hud_api->set_depth_test(false);
        hud_api->rect(hud,0,0,(float)win_w,(float)win_h,0,0,0,state->intro_fade);
        hud_api->flush(hud);
        hud_api->set_depth_test(true);
    }

    {
        char title[256];
        snprintf(title,sizeof(title),"RPG | HP:%.0f | MP:%.0f | Gold:%d XP:%d",
                 (double)state->health,(double)state->mana,state->gold,state->xp);
        hud_api->set_window_title(window,title);
    }

    hud_api->swap_window(window);
}
