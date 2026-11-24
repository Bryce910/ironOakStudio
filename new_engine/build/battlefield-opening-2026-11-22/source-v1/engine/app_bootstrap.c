#include "engine/app_bootstrap.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>

#include "engine/audio.h"

void app_bootstrap_apply_vsync(bool enabled){
    /* Ordinary VSync produced steadier native frame pacing than adaptive
       swap control on the target Radeon/compositor combination. */
    SDL_GL_SetSwapInterval(enabled?1:0);
}

bool app_bootstrap_run(const AppBootstrapConfig *config,AppBootstrapResult *result){
    if(!config||!result) return false;

    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){
        fprintf(stderr,"[Bootstrap] SDL_Init failed: %s\n",SDL_GetError());
        return false;
    }

    audio_init(config->master_vol);
    audio_set_volumes(config->master_vol,config->music_vol,config->sfx_vol);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    /* The world renders to a single-sample HDR target and is antialiased by
       the presentation FXAA pass. Multisampling only the final blit/UI adds
       a redundant full-window resolve without antialiasing world geometry. */
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0);

    result->window=SDL_CreateWindow(config->title?config->title:"RPG",
                                    SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                                    config->win_w,config->win_h,
                                    SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!result->window){
        fprintf(stderr,"[Bootstrap] SDL_CreateWindow failed: %s\n",SDL_GetError());
        SDL_Quit();
        return false;
    }

    result->gl_context=SDL_GL_CreateContext(result->window);
    if(!result->gl_context){
        fprintf(stderr,"[Bootstrap] SDL_GL_CreateContext failed: %s\n",SDL_GetError());
        SDL_DestroyWindow(result->window);
        result->window=NULL;
        SDL_Quit();
        return false;
    }

#ifdef _WIN32
    if(!vw_windows_gl_init()){
        fprintf(stderr,"[Bootstrap] OpenGL 3.3 functions unavailable; install the GPU vendor's Windows driver.\n");
        SDL_GL_DeleteContext(result->gl_context);SDL_DestroyWindow(result->window);
        result->gl_context=NULL;result->window=NULL;SDL_Quit();return false;
    }
#endif
    app_bootstrap_apply_vsync(true);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_MULTISAMPLE);
    glClearColor(0.04f,0.03f,0.06f,1.0f);
    srand((unsigned)time(NULL));

    if(config->editor_ui){
        editor_ui_startup(config->editor_ui,result->window,result->gl_context);
    }
    if(config->input_runtime){
        input_runtime_try_open_gamepad(config->input_runtime);
    }

    result->win_w=config->win_w;
    result->win_h=config->win_h;
    return true;
}
