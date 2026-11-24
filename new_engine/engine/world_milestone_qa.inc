    const bool qa_milestone=presentation&&milestone_finish&&launch->frames&&std::getenv("VALWOOD_QA_MILESTONE_WORKFLOW");
    unsigned qa_milestone_phase=0;uint64_t qa_milestone_wait=0;Camera qa_milestone_camera;
    std::array<int64_t,5> qa_milestone_counters={};
    auto milestone_workflow=[&](){
        if(!qa_milestone||qa_milestone_phase==7||destination_loading||frame<qa_milestone_wait)return;
        auto fail=[&](const char *message){std::fprintf(stderr,"[Milestone QA] FAIL phase%u: %s\n",qa_milestone_phase,message);launch->exit_code=1;running=false;};
        auto key=[&](SDL_Keycode k){SDL_Event e={};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=k;SDL_PushEvent(&e);};
        auto counters=[&](){return std::array<int64_t,5>{studio.components_runtime->counter(world_runtime_demo::mastery_counter),studio.components_runtime->counter(world_runtime_demo::camp_counter),studio.components_runtime->counter(world_runtime_demo::return_counter),studio.components_runtime->counter(world_runtime_demo::renown_counter),studio.components_runtime->counter(world_presentation::threat_counter)};};
        if(qa_milestone_phase==0){qa_milestone_camera=cam;qa_milestone_counters=counters();key(SDLK_F7);}
        if(qa_milestone_phase==1){if(milestone_review.view!=world_milestone::View::Courtyard){fail("Courtyard hotkey");return;}key(SDLK_2);}
        if(qa_milestone_phase==2){if(milestone_review.view!=world_milestone::View::Jungle){fail("Jungle hotkey");return;}key(SDLK_3);}
        if(qa_milestone_phase==3){if(!milestone_review.portrait()||milestone_review.portrait_frames<60||!third_person){fail("Actual player portrait");return;}key(SDLK_v);key(SDLK_e);key(SDLK_F5);SDL_Event e={};e.type=SDL_MOUSEWHEEL;e.wheel.y=100;SDL_PushEvent(&e);}
        if(qa_milestone_phase==4){if(!milestone_review.portrait()||!third_person||milestone_review.distance!=1.45f||counters()!=qa_milestone_counters){fail("Portrait controls or progress isolation");return;}key(SDLK_ESCAPE);}
        if(qa_milestone_phase==5){if(milestone_review.active()||preview_screen!=0||std::hypot(cam.x-qa_milestone_camera.x,cam.z-qa_milestone_camera.z)>.001||counters()!=qa_milestone_counters){fail("Return camera, title, or quest state");return;}key(SDLK_F7);}
        if(qa_milestone_phase==6){if(milestone_review.view!=world_milestone::View::Courtyard){fail("Re-entry");return;}key(SDLK_ESCAPE);std::puts("[Milestone QA] PASS three views, actual player portrait, camera controls, progress isolation, return and re-entry");launch->frames=int(frame+90);}
        ++qa_milestone_phase;qa_milestone_wait=frame+100;
    };
