menu_start:
    options_load(&g_options);
    audio_set_volumes(g_options.master_vol,g_options.music_vol,g_options.sfx_vol);
    SDL_GL_SetSwapInterval(g_options.vsync?1:0);
    if(g_options.fullscreen) SDL_SetWindowFullscreen(win,SDL_WINDOW_FULLSCREEN_DESKTOP);
    if(show_startup_splash&&!g_launch.direct){
        float splash_t=0.0f;
        const float splash_fade_in=0.45f;
        const float splash_hold=1.60f;
        const float splash_fade_out=0.65f;
        const float splash_total=splash_fade_in+splash_hold+splash_fade_out;
        Uint64 splash_last=SDL_GetPerformanceCounter();
        bool splash_done=false;

        while(!splash_done){
            SDL_Event ev;
            bool skip=false;
            while(SDL_PollEvent(&ev)){
                if(ev.type==SDL_QUIT){SDL_Quit();return 0;}
                if(ev.type==SDL_WINDOWEVENT&&ev.window.event==SDL_WINDOWEVENT_RESIZED){
                    win_w=ev.window.data1;win_h=ev.window.data2;
                    hud_resize(&hud,win_w,win_h);glViewport(0,0,win_w,win_h);
                }
                if(ev.type==SDL_KEYDOWN||ev.type==SDL_MOUSEBUTTONDOWN||ev.type==SDL_CONTROLLERBUTTONDOWN)
                    skip=true;
            }

            Uint64 splash_now=SDL_GetPerformanceCounter();
            float dt=clampf((float)(splash_now-splash_last)/(float)SDL_GetPerformanceFrequency(),0,0.05f);
            splash_last=splash_now;
            if(skip&&splash_t<(splash_fade_in+splash_hold)) splash_t=splash_fade_in+splash_hold;
            splash_t+=dt;

            float overlay_alpha=1.0f;
            if(splash_t<splash_fade_in) overlay_alpha=splash_t/splash_fade_in;
            else if(splash_t>=(splash_fade_in+splash_hold))
                overlay_alpha=1.0f-(splash_t-(splash_fade_in+splash_hold))/splash_fade_out;
            overlay_alpha=clampf(overlay_alpha,0.0f,1.0f);

            float W=(float)win_w,H=(float)win_h;
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            studio_splash_draw(&hud,W,H,overlay_alpha);
            hud_flush(&hud);
            SDL_GL_SwapWindow(win);

            if(splash_t>=splash_total) splash_done=true;
        }
        show_startup_splash=false;
    }
    audio_play_music(MUS_MENU,0);
    int menu_load_slot=-1;
    int menu_char_sel=-1;
    (void)menu_char_sel;
    {
        MenuPage page=MPAGE_MAIN;
        bool running_menu=!g_launch.direct;
        bool lmb_prev=false;
        bool slider_held=false;
        int active_slider=-1;
        int charsel_hover=-1;
        int prev_mx=0,prev_my=0;
        bool mouse_prev_valid=false;

        while(running_menu){
            SDL_Event ev;
            while(SDL_PollEvent(&ev)){
                if(ev.type==SDL_QUIT){SDL_Quit();return 0;}
                if(ev.type==SDL_WINDOWEVENT&&ev.window.event==SDL_WINDOWEVENT_RESIZED){
                    win_w=ev.window.data1;win_h=ev.window.data2;
                    hud_resize(&hud,win_w,win_h);glViewport(0,0,win_w,win_h);
                }
                if(page==MPAGE_OPTIONS&&ev.type==SDL_KEYDOWN&&options_handle_bind_capture(&ev.key)){
                    continue;
                }
                if(page==MPAGE_OPTIONS&&ev.type==SDL_CONTROLLERBUTTONDOWN&&options_handle_pad_bind_capture_button((SDL_GameControllerButton)ev.cbutton.button)){
                    continue;
                }
                if(ev.type==SDL_KEYDOWN&&ev.key.keysym.sym==SDLK_ESCAPE){
                    if(page!=MPAGE_MAIN)page=MPAGE_MAIN;
                    else{SDL_Quit();return 0;}
                }
            }
            int mx,my;
            bool lmb=(SDL_GetMouseState(&mx,&my)&SDL_BUTTON(1))!=0;
            bool rmb=(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(3))!=0;
            bool clicked=lmb&&!lmb_prev;
            slider_held=lmb;
            if(!slider_held) active_slider=-1;
            if(page==MPAGE_CHARSELECT&&rmb&&mouse_prev_valid){
                g_charsel_preview_yaw+=(float)(mx-prev_mx)*0.0125f;
            }
            prev_mx=mx;
            prev_my=my;
            mouse_prev_valid=true;

            float W=(float)win_w,H=(float)win_h;
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

            if(page==MPAGE_MAIN){
                bool start=false,load=false,opts=false,quit=false;
                page_main(&hud,W,H,mx,my,&start,&load,&opts,&quit);
                if(clicked){
                    if(start){menu_load_slot=-1;running_menu=false;}
                    if(load) page=MPAGE_LOAD;
                    if(opts) page=MPAGE_OPTIONS;
                    if(quit){SDL_Quit();return 0;}
                }
            } else if(page==MPAGE_CHARSELECT){
                int sel=-1; bool back=false;
                page_charselect(&hud,W,H,mx,my,clicked,&charsel_hover,&sel,&back);
                if(back&&clicked){ page=MPAGE_MAIN; }
                if(sel>=0){
                    menu_char_sel=sel;
                    strncpy(char_path_buf,g_chardefs[sel].fbx_path,sizeof(char_path_buf)-1);
                    menu_load_slot=-1;
                    running_menu=false;
                    audio_play(SND_MENU_CLICK,90);
                }
            } else if(page==MPAGE_LOAD){
                int chosen=-1;bool back=false;
                page_load(&hud,W,H,mx,my,&chosen,&back);
                if(clicked){
                    if(chosen>=0){menu_load_slot=chosen;running_menu=false;}
                    if(back)page=MPAGE_MAIN;
                }
            } else if(page==MPAGE_OPTIONS){
                bool back=false,credits=false;
                page_options(&hud,W,H,mx,my,clicked,&g_options,slider_held,&active_slider,&back,&credits);
                audio_set_volumes(g_options.master_vol,g_options.music_vol,g_options.sfx_vol);
                SDL_GL_SetSwapInterval(g_options.vsync?1:0);
                if(clicked){
                    if(g_options.fullscreen)
                        SDL_SetWindowFullscreen(win,SDL_WINDOW_FULLSCREEN_DESKTOP);
                    else
                        SDL_SetWindowFullscreen(win,0);
                    if(back){options_save(&g_options);page=MPAGE_MAIN;}
                    if(credits) page=MPAGE_CREDITS;
                    if(!g_options.fullscreen){
                        SDL_SetWindowSize(win,g_options.res_w,g_options.res_h);
                        win_w=g_options.res_w; win_h=g_options.res_h;
                        hud_resize(&hud,win_w,win_h);
                        glViewport(0,0,win_w,win_h);
                    }
                }
            } else if(page==MPAGE_CREDITS){
                bool back=false;
                page_credits(&hud,W,H,mx,my,&back);
                if(clicked&&back) page=MPAGE_OPTIONS;
            }

            SDL_GL_SwapWindow(win);
            lmb_prev=lmb;
        }
    }
