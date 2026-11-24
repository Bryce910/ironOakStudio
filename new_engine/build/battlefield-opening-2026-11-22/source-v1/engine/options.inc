typedef enum{DIFF_EASY=0,DIFF_NORMAL,DIFF_HARD}Difficulty;
typedef enum{
    BIND_MOVE_FORWARD=0,
    BIND_MOVE_BACKWARD,
    BIND_MOVE_LEFT,
    BIND_MOVE_RIGHT,
    BIND_JUMP,
    BIND_SPRINT,
    BIND_DODGE,
    BIND_INTERACT,
    BIND_BLOCK,
    BIND_INVENTORY,
    BIND_MAP,
    BIND_SKILLTREE,
    BIND_APPEARANCE,
    BIND_POTION,
    GAME_BIND_COUNT
}GameBindId;

typedef enum{
    PAD_INPUT_A=0,
    PAD_INPUT_B,
    PAD_INPUT_X,
    PAD_INPUT_Y,
    PAD_INPUT_BACK,
    PAD_INPUT_GUIDE,
    PAD_INPUT_START,
    PAD_INPUT_LEFTSTICK,
    PAD_INPUT_RIGHTSTICK,
    PAD_INPUT_LEFTSHOULDER,
    PAD_INPUT_RIGHTSHOULDER,
    PAD_INPUT_DPAD_UP,
    PAD_INPUT_DPAD_DOWN,
    PAD_INPUT_DPAD_LEFT,
    PAD_INPUT_DPAD_RIGHT,
    PAD_INPUT_COUNT
}PadInputId;

typedef enum{
    PAD_BIND_JUMP=0,
    PAD_BIND_ATTACK,
    PAD_BIND_HEAVY_ATTACK,
    PAD_BIND_DODGE,
    PAD_BIND_INTERACT,
    PAD_BIND_BLOCK,
    PAD_BIND_PAUSE,
    PAD_BIND_INVENTORY,
    PAD_BIND_COUNT
}PadBindId;

static const SDL_Scancode OPTIONS_DEFAULT_BINDS[GAME_BIND_COUNT]={
    SDL_SCANCODE_W,
    SDL_SCANCODE_S,
    SDL_SCANCODE_A,
    SDL_SCANCODE_D,
    SDL_SCANCODE_SPACE,
    SDL_SCANCODE_LSHIFT,
    SDL_SCANCODE_Q,
    SDL_SCANCODE_E,
    SDL_SCANCODE_R,
    SDL_SCANCODE_I,
    SDL_SCANCODE_M,
    SDL_SCANCODE_K,
    SDL_SCANCODE_J,
    SDL_SCANCODE_H
};

static const char *OPTIONS_BIND_LABELS[GAME_BIND_COUNT]={
    "Move Forward","Move Backward","Move Left","Move Right",
    "Jump","Sprint","Dodge","Interact","Block",
    "Inventory","Map","Skill Tree","Appearance","Potion"
};

static const SDL_GameControllerButton OPTIONS_PAD_INPUT_BUTTONS[PAD_INPUT_COUNT]={
    SDL_CONTROLLER_BUTTON_A,
    SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_GUIDE,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT
};

static const char *OPTIONS_PAD_INPUT_NAMES[PAD_INPUT_COUNT]={
    "A","B","X","Y","Back","Guide","Start","L3","R3","LB","RB","DPad Up","DPad Down","DPad Left","DPad Right"
};

static const char *OPTIONS_PAD_BIND_LABELS[PAD_BIND_COUNT]={
    "Jump","Attack","Heavy Attack","Dodge","Interact","Block","Pause","Inventory"
};

static const PadInputId OPTIONS_DEFAULT_PAD_BINDS[PAD_BIND_COUNT]={
    PAD_INPUT_A,
    PAD_INPUT_X,
    PAD_INPUT_Y,
    PAD_INPUT_B,
    PAD_INPUT_RIGHTSHOULDER,
    PAD_INPUT_LEFTSHOULDER,
    PAD_INPUT_START,
    PAD_INPUT_BACK
};

typedef struct{
    int  master_vol;
    int  music_vol;
    int  sfx_vol;
    int  mouse_sens;
    int  pad_look_sens;
    int  pad_deadzone;
    bool fullscreen;
    bool show_fps;
    bool vsync;
    bool bloom;
    bool invert_mouse_y;
    bool invert_pad_y;
    int  difficulty;
    int  res_w,res_h;
    int  keybinds[GAME_BIND_COUNT];
    int  pad_binds[PAD_BIND_COUNT];
}Options;

static Options g_options={
    100,70,80,
    100,100,20,
    false,false,true,true,
    false,false,
    DIFF_NORMAL,1280,720,
    {
        SDL_SCANCODE_W,SDL_SCANCODE_S,SDL_SCANCODE_A,SDL_SCANCODE_D,
        SDL_SCANCODE_SPACE,SDL_SCANCODE_LSHIFT,SDL_SCANCODE_Q,SDL_SCANCODE_E,
        SDL_SCANCODE_R,SDL_SCANCODE_I,SDL_SCANCODE_M,SDL_SCANCODE_K,
        SDL_SCANCODE_J,SDL_SCANCODE_H
    },
    {
        PAD_INPUT_A,PAD_INPUT_X,PAD_INPUT_Y,PAD_INPUT_B,
        PAD_INPUT_RIGHTSHOULDER,PAD_INPUT_LEFTSHOULDER,PAD_INPUT_START,PAD_INPUT_BACK
    }
};

static int g_options_capture_bind=-1;
static int g_options_capture_pad_bind=-1;

static const char *options_bind_label(GameBindId id){
    return (id>=0&&id<GAME_BIND_COUNT)?OPTIONS_BIND_LABELS[id]:"Unknown";
}

static const char *options_scancode_name(SDL_Scancode sc){
    const char *name=(sc!=SDL_SCANCODE_UNKNOWN)?SDL_GetScancodeName(sc):"";
    return (name&&name[0])?name:"Unbound";
}

static bool options_bind_down(const Uint8 *keys,GameBindId id){
    SDL_Scancode sc=(SDL_Scancode)g_options.keybinds[id];
    return keys&&sc>SDL_SCANCODE_UNKNOWN&&sc<SDL_NUM_SCANCODES&&keys[sc]!=0;
}

static bool options_bind_matches_scancode(SDL_Scancode sc,GameBindId id){
    return sc>SDL_SCANCODE_UNKNOWN&&sc==(SDL_Scancode)g_options.keybinds[id];
}

static const char *options_pad_bind_label(PadBindId id){
    return (id>=0&&id<PAD_BIND_COUNT)?OPTIONS_PAD_BIND_LABELS[id]:"Unknown";
}

static const char *options_pad_input_name(PadInputId id){
    return (id>=0&&id<PAD_INPUT_COUNT)?OPTIONS_PAD_INPUT_NAMES[id]:"Unbound";
}

static SDL_GameControllerButton options_pad_input_button(PadInputId id){
    return (id>=0&&id<PAD_INPUT_COUNT)?OPTIONS_PAD_INPUT_BUTTONS[id]:SDL_CONTROLLER_BUTTON_INVALID;
}

static bool options_pad_bind_matches_button(SDL_GameControllerButton button,PadBindId id){
    return id>=0&&id<PAD_BIND_COUNT&&button==options_pad_input_button((PadInputId)g_options.pad_binds[id]);
}

static bool options_is_binding_capture_active(void){
    return g_options_capture_bind>=0&&g_options_capture_bind<GAME_BIND_COUNT;
}

static bool options_is_pad_binding_capture_active(void){
    return g_options_capture_pad_bind>=0&&g_options_capture_pad_bind<PAD_BIND_COUNT;
}

static void options_begin_bind_capture(GameBindId id){
    if(id>=0&&id<GAME_BIND_COUNT) g_options_capture_bind=id;
}

static void options_begin_pad_bind_capture(PadBindId id){
    if(id>=0&&id<PAD_BIND_COUNT) g_options_capture_pad_bind=id;
}

static bool options_handle_bind_capture(const SDL_KeyboardEvent *ev){
    if(!ev||!options_is_binding_capture_active()) return false;
    if(ev->repeat) return true;
    if(ev->keysym.scancode==SDL_SCANCODE_ESCAPE){
        g_options_capture_bind=-1;
        return true;
    }
    if(ev->keysym.scancode==SDL_SCANCODE_BACKSPACE||ev->keysym.scancode==SDL_SCANCODE_DELETE){
        g_options.keybinds[g_options_capture_bind]=SDL_SCANCODE_UNKNOWN;
        g_options_capture_bind=-1;
        return true;
    }
    if(ev->keysym.scancode<=SDL_SCANCODE_UNKNOWN||ev->keysym.scancode>=SDL_NUM_SCANCODES) return true;
    {
        int captured=g_options_capture_bind;
        SDL_Scancode old=(SDL_Scancode)g_options.keybinds[captured];
        for(int i=0;i<GAME_BIND_COUNT;i++){
            if(i!=captured&&g_options.keybinds[i]==(int)ev->keysym.scancode){
                g_options.keybinds[i]=(int)old;
                break;
            }
        }
        g_options.keybinds[captured]=(int)ev->keysym.scancode;
    }
    g_options_capture_bind=-1;
    return true;
}

static bool options_handle_pad_bind_capture_button(SDL_GameControllerButton button){
    if(!options_is_pad_binding_capture_active()) return false;
    if(button==SDL_CONTROLLER_BUTTON_INVALID) return true;
    for(int i=0;i<PAD_INPUT_COUNT;i++){
        if(OPTIONS_PAD_INPUT_BUTTONS[i]==button){
            int captured=g_options_capture_pad_bind;
            int old=g_options.pad_binds[captured];
            for(int j=0;j<PAD_BIND_COUNT;j++){
                if(j!=captured&&g_options.pad_binds[j]==i){
                    g_options.pad_binds[j]=old;
                    break;
                }
            }
            g_options.pad_binds[captured]=i;
            g_options_capture_pad_bind=-1;
            return true;
        }
    }
    return true;
}

static void options_save(const Options *o){
    FILE *f=fopen("config.ini","w");if(!f)return;
    fprintf(f,"master_vol=%d\n",o->master_vol);
    fprintf(f,"music_vol=%d\n",o->music_vol);
    fprintf(f,"sfx_vol=%d\n",o->sfx_vol);
    fprintf(f,"mouse_sens=%d\n",o->mouse_sens);
    fprintf(f,"pad_look_sens=%d\n",o->pad_look_sens);
    fprintf(f,"pad_deadzone=%d\n",o->pad_deadzone);
    fprintf(f,"fullscreen=%d\n",(int)o->fullscreen);
    fprintf(f,"show_fps=%d\n",(int)o->show_fps);
    fprintf(f,"vsync=%d\n",(int)o->vsync);
    fprintf(f,"bloom=%d\n",(int)o->bloom);
    fprintf(f,"invert_mouse_y=%d\n",(int)o->invert_mouse_y);
    fprintf(f,"invert_pad_y=%d\n",(int)o->invert_pad_y);
    fprintf(f,"difficulty=%d\n",o->difficulty);
    fprintf(f,"res_w=%d\n",o->res_w);
    fprintf(f,"res_h=%d\n",o->res_h);
    for(int i=0;i<GAME_BIND_COUNT;i++)
        fprintf(f,"bind_%d=%d\n",i,o->keybinds[i]);
    for(int i=0;i<PAD_BIND_COUNT;i++)
        fprintf(f,"pad_bind_%d=%d\n",i,o->pad_binds[i]);
    fclose(f);
    printf("[Config] Saved config.ini\n");
}

static void options_load(Options *o){
    FILE *f=fopen("config.ini","r");if(!f)return;
    char line[128];
    while(fgets(line,sizeof(line),f)){
        int v;
        if(sscanf(line,"master_vol=%d",&v)==1) o->master_vol=v;
        if(sscanf(line,"music_vol=%d",&v)==1)  o->music_vol=v;
        if(sscanf(line,"sfx_vol=%d",&v)==1)    o->sfx_vol=v;
        if(sscanf(line,"mouse_sens=%d",&v)==1) o->mouse_sens=v;
        if(sscanf(line,"pad_look_sens=%d",&v)==1) o->pad_look_sens=v;
        if(sscanf(line,"pad_deadzone=%d",&v)==1) o->pad_deadzone=v;
        if(sscanf(line,"fullscreen=%d",&v)==1) o->fullscreen=(bool)v;
        if(sscanf(line,"show_fps=%d",&v)==1)   o->show_fps=(bool)v;
        if(sscanf(line,"vsync=%d",&v)==1)      o->vsync=(bool)v;
        if(sscanf(line,"bloom=%d",&v)==1)      o->bloom=(bool)v;
        if(sscanf(line,"invert_mouse_y=%d",&v)==1) o->invert_mouse_y=(bool)v;
        if(sscanf(line,"invert_pad_y=%d",&v)==1) o->invert_pad_y=(bool)v;
        if(sscanf(line,"difficulty=%d",&v)==1) o->difficulty=v;
        if(sscanf(line,"res_w=%d",&v)==1)      o->res_w=v;
        if(sscanf(line,"res_h=%d",&v)==1)      o->res_h=v;
        for(int i=0;i<GAME_BIND_COUNT;i++){
            char key[32];
            snprintf(key,sizeof(key),"bind_%d=%%d",i);
            if(sscanf(line,key,&v)==1){
                if(v>=SDL_SCANCODE_UNKNOWN&&v<SDL_NUM_SCANCODES) o->keybinds[i]=v;
            }
        }
        for(int i=0;i<PAD_BIND_COUNT;i++){
            char key[32];
            snprintf(key,sizeof(key),"pad_bind_%d=%%d",i);
            if(sscanf(line,key,&v)==1){
                if(v>=0&&v<PAD_INPUT_COUNT) o->pad_binds[i]=v;
            }
        }
    }
    fclose(f);
    o->master_vol=o->master_vol<0?0:(o->master_vol>100?100:o->master_vol);
    o->music_vol=o->music_vol<0?0:(o->music_vol>100?100:o->music_vol);
    o->sfx_vol=o->sfx_vol<0?0:(o->sfx_vol>100?100:o->sfx_vol);
    o->mouse_sens=o->mouse_sens<10?10:(o->mouse_sens>300?300:o->mouse_sens);
    o->pad_look_sens=o->pad_look_sens<10?10:(o->pad_look_sens>300?300:o->pad_look_sens);
    o->pad_deadzone=o->pad_deadzone<0?0:(o->pad_deadzone>50?50:o->pad_deadzone);
    printf("[Config] Loaded config.ini\n");
}
