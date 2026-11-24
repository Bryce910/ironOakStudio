    unsigned controller_qa_phase=0,controller_qa_checks=0;
    uint64_t controller_qa_wait=0;bool controller_qa_passed=false;
    double controller_qa_x=0,controller_qa_z=0,controller_qa_y=0,controller_qa_yaw=0,controller_qa_clock=0;
    float controller_qa_orbit=0,controller_qa_distance=0;unsigned controller_qa_swings=0,controller_qa_dodges=0;
    unsigned controller_qa_menu_moves=0;
    auto controller_workflow=[&](){
        if(!qa_controller||controller_qa_passed)return;
        controller_test_pad.tick();
        auto fail=[&](const char *reason){std::fprintf(stderr,"[Controller QA] FAIL phase %u: %s (screen=%d review=%d focus=%s)\n",controller_qa_phase,reason,preview_screen,int(milestone_review.view),preview_focus_panel.c_str());launch->exit_code=1;running=false;};
        auto check=[&](bool value,const char *reason){++controller_qa_checks;if(!value)fail(reason);return value;};
        auto next=[&](unsigned delay=18){std::printf("[Controller QA] phase %u passed at frame %llu\n",controller_qa_phase,(unsigned long long)frame);++controller_qa_phase;controller_qa_wait=frame+delay;};
        auto press=[&](SDL_GameControllerButton b){controller_test_pad.press(b);};
        auto axis=[&](SDL_GameControllerAxis a,Sint16 v){controller_test_pad.axis(a,v);};
        auto at_corvin=[&](){world_runtime::Entity e;if(!studio.components_runtime->entity(world_runtime_demo::instructor_id,e))return false;cam.x=e.pose.position.x+2.3;cam.z=e.pose.position.z;cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.yaw=-1.57079633f;cam.pitch=0;cam.fly=false;cam.grounded=true;cam.vy=0;temporal_cut=true;return true;};
        if(frame==1){
            SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();preview_pad.focused=true;
            if(!check(preview_pad.id()==SDL_JoystickInstanceID(controller_test_pad.joystick),"QA owns only its virtual SDL device"))return;
            controller_qa_wait=frame+90;
        }
        if(frame>uint64_t(launch->frames)-60){fail("Frame budget exceeded");return;}
        if(frame<controller_qa_wait||destination_loading||preview_travel_phase)return;
        switch(controller_qa_phase){
            case 0:
                if(!check(preview_pad.get()&&preview_screen==0,"Title and hot-plug"))return;
                // Continue is disabled in this fresh isolated save. Down must
                // select Showcase, and A must not also start a new journey.
                press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break;
            case 1:press(SDL_CONTROLLER_BUTTON_A);next(30);break;
            case 2:
                if(!check(milestone_review.view==world_milestone::View::Courtyard,"A selected focused Showcase, not Begin"))return;
                press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);next();break;
            case 3:
                if(!check(milestone_review.view==world_milestone::View::Jungle,"Shoulder cycles to jungle"))return;
                press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);next(80);break;
            case 4:
                if(!check(milestone_review.portrait(),"Shoulder cycles to portrait"))return;
                controller_qa_orbit=milestone_review.orbit;controller_qa_distance=milestone_review.distance;
                axis(SDL_CONTROLLER_AXIS_RIGHTX,20000);axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);next(22);break;
            case 5:
                if(!check(milestone_review.orbit>controller_qa_orbit+.01f&&milestone_review.distance<controller_qa_distance,"Portrait orbit and framing use sticks/triggers"))return;
                axis(SDL_CONTROLLER_AXIS_RIGHTX,0);axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);press(SDL_CONTROLLER_BUTTON_B);next();break;
            case 6:
                if(!check(preview_screen==0&&!milestone_review.active(),"Showcase Back returns to title"))return;
                press(SDL_CONTROLLER_BUTTON_A);next(40);break;
            case 7:
                if(!check(preview_screen==1,"Title Begin through focused A"))return;
                controller_qa_x=cam.x;controller_qa_z=cam.z;controller_qa_yaw=cam.yaw;
                axis(SDL_CONTROLLER_AXIS_LEFTY,-32768);controller_test_pad.button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER,true);next(25);break;
            case 8:{
                const double forward=(cam.x-controller_qa_x)*std::sin(controller_qa_yaw)-(cam.z-controller_qa_z)*std::cos(controller_qa_yaw);
                if(!check(forward>.15&&preview_pad.sprint,"Forward stick moves forward; sprint is reachable"))return;
                axis(SDL_CONTROLLER_AXIS_LEFTY,0);controller_test_pad.button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER,false);
                controller_qa_y=cam.y;press(SDL_CONTROLLER_BUTTON_X);next(7);break;
            }
            case 9:
                if(!check(cam.y>controller_qa_y+.04,"Controller jump lifts the player"))return;
                press(SDL_CONTROLLER_BUTTON_Y);next();break;
            case 10:
                if(!check(preview_screen==3,"Journal opens"))return;
                press(SDL_CONTROLLER_BUTTON_A);next();break;
            case 11:
                if(!check(preview_screen==1,"Journal returns through A"))return;
                press(SDL_CONTROLLER_BUTTON_START);next();break;
            case 12:
                if(!check(preview_screen==2,"Pause opens"))return;
                controller_qa_x=cam.x;controller_qa_z=cam.z;axis(SDL_CONTROLLER_AXIS_LEFTY,-32768);next(20);break;
            case 13:
                if(!check(std::hypot(cam.x-controller_qa_x,cam.z-controller_qa_z)<.00001,"Menu stick navigation cannot move the player"))return;
                axis(SDL_CONTROLLER_AXIS_LEFTY,0);press(SDL_CONTROLLER_BUTTON_START);next();break;
            case 14:
                if(!check(preview_screen==1,"Resume after menu navigation"))return;
                press(SDL_CONTROLLER_BUTTON_START);controller_qa_phase=35;controller_qa_wait=frame+18;break;
            case 15:
                if(!check(studio.components_runtime->current_line()!=nullptr,"A opens actual NPC conversation"))return;
                press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break;
            case 16:press(SDL_CONTROLLER_BUTTON_A);next();break;
            case 17:
                if(!check(!studio.components_runtime->current_line()&&!fight.engaged(),"D-pad and A choose second dialogue option"))return;
                press(SDL_CONTROLLER_BUTTON_A);next();break;
            case 18:
                if(!check(studio.components_runtime->current_line()!=nullptr,"Conversation reopens"))return;
                press(SDL_CONTROLLER_BUTTON_A);next();break;
            case 19:
                if(!check(fight.engaged(),"First dialogue choice starts actual trial"))return;
                next(200);break;
            case 20:
                if(!check(fight.state==TrainingState::Fighting,"Trial countdown completes"))return;
                controller_qa_swings=fight.player_swings;press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);next(8);break;
            case 21:
                if(!check(fight.player_swings>controller_qa_swings,"RT starts a swing"))return;
                if(!check(fight.selected==TrainingDirection::Low&&!ImGui::IsKeyDown(ImGuiKey_DownArrow),"Combat D-pad changes angle without leaving a held keyboard key in menus"))return;
                axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767);next(8);break;
            case 22:
                if(!check(fight.guard,"LT holds guard"))return;
                axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);press(SDL_CONTROLLER_BUTTON_START);controller_qa_clock=fight.clock;next(25);break;
            case 23:
                if(!check(preview_screen==2&&std::abs(fight.clock-controller_qa_clock)<.02,"Pause freezes actual combat"))return;
                press(SDL_CONTROLLER_BUTTON_START);next(65);break;
            case 24:
                controller_qa_swings=fight.player_swings;press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);next(6);break;
            case 25:
                if(!check(fight.selected==TrainingDirection::Thrust&&fight.player_swings>controller_qa_swings,"RB performs thrust"))return;
                next(65);break;
            case 26:
                controller_qa_dodges=fight.dodges;press(SDL_CONTROLLER_BUTTON_B);next(7);break;
            case 27:
                if(!check(fight.dodges>controller_qa_dodges,"B dodges"))return;
                controller_test_pad.detach();next();break;
            case 28:
                if(!check(!preview_pad.get()&&preview_screen==2&&!fight.guard,"Unplug pauses and releases guard"))return;
                if(!check(controller_test_pad.attach(),"Reconnect virtual controller"))return;
                preview_pad.prefer(SDL_JoystickInstanceID(controller_test_pad.joystick));
                next();break;
            case 29:
                if(!check(preview_pad.get()!=nullptr,"Hot reconnect opens new instance"))return;
                press(SDL_CONTROLLER_BUTTON_START);next();break;
            case 30:
                if(!check(preview_screen==1,"Reconnected controller resumes"))return;
                // Let a real incoming attack defeat a low-health test fixture.
                fight.health=1;cam.x=preview_arena_x+fight.enemy_x-2;cam.z=preview_arena_z+fight.enemy_z;
                next(400);break;
            case 31:
                if(!check(fight.state==TrainingState::Lost,"Actual incoming attack defeats player"))return;
                press(SDL_CONTROLLER_BUTTON_A);next();break;
            case 32:
                if(!check(studio.components_runtime->current_line()!=nullptr,"A activates retry UI"))return;
                press(SDL_CONTROLLER_BUTTON_A);next();break;
            case 33:
                if(!check(fight.engaged(),"Retry conversation restarts countdown"))return;
                if(!check(preview_pad.using_pad,"Controller prompts remain active"))return;
                if(!check(controller_test_pad.rumble_calls>0,"Combat emits supported vibration"))return;
                press(SDL_CONTROLLER_BUTTON_START);next();break;
            case 34:
                if(!check(preview_screen==2,"Final controller settings screen"))return;
                controller_qa_menu_moves=0;controller_qa_phase=46;controller_qa_wait=frame+18;break;
            case 35:press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break; // Showcase
            case 36:press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break; // Save
            case 37:press(SDL_CONTROLLER_BUTTON_A);next();break;
            case 38:
                if(!check(preview_screen==2&&preview_notice=="Journey saved","Save selected through pause menu"))return;
                controller_qa_x=cam.x;controller_qa_z=cam.z;press(SDL_CONTROLLER_BUTTON_START);next();break;
            case 39:axis(SDL_CONTROLLER_AXIS_LEFTY,-32768);next(25);break;
            case 40:
                axis(SDL_CONTROLLER_AXIS_LEFTY,0);press(SDL_CONTROLLER_BUTTON_START);next();break;
            case 41:press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break;
            case 42:press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break;
            case 43:press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break; // Restore
            case 44:press(SDL_CONTROLLER_BUTTON_A);next();break;
            case 45:
                if(!check(preview_screen==1&&std::hypot(cam.x-controller_qa_x,cam.z-controller_qa_z)<.001,"Restore selected through pause menu returns saved position"))return;
                if(!check(at_corvin(),"Valid interaction fixture"))return;
                press(SDL_CONTROLLER_BUTTON_A);controller_qa_phase=15;controller_qa_wait=frame+18;break;
            case 46:
                // Disabled save/showcase entries are skipped; navigation must
                // scroll the lower Graphics button into view on a 720p display.
                if(controller_qa_menu_moves++<10u+(preview_voices.available?1u:0u)){
                    press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);controller_qa_wait=frame+8;
                }else{press(SDL_CONTROLLER_BUTTON_A);next();}break;
            case 47:
                if(!check(show_graphics,"Controller reaches Graphics and Display below the fold"))return;
                press(SDL_CONTROLLER_BUTTON_B);next();break;
            case 48:
                if(!check(!show_graphics&&preview_screen==2,"Back returns from graphics to pause"))return;
                controller_qa_passed=true;launch->frames=int(frame+30);
                std::printf("[Controller QA] PASS %u assertions: real SDL virtual pad, menu focus, three review views, portrait, forward/sprint/jump, journal, save/restore, dialogue branches, combat, pause, unplug/reconnect, defeat/retry, vibration and scrolling graphics navigation\n",controller_qa_checks);break;
        }
    };
