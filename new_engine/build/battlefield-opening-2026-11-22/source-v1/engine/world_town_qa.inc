    // Opt-in native integration replay; confined to a caller-supplied QA save.
    unsigned town_qa_phase=0,town_qa_point=0,town_qa_checks=0,town_qa_steps=0;
    uint64_t town_qa_wait=150;bool town_qa_passed=false;double town_qa_walk=0;
    auto town_workflow=[&](){
        if(!qa_town||town_qa_passed||destination_loading||preview_travel_phase)return;
        controller_test_pad.tick();
        auto check=[&](bool value,const char *reason){++town_qa_checks;if(!value){std::fprintf(stderr,"[Town QA] FAIL phase %u point %u: %s at %.3f %.3f (%s)\n",town_qa_phase,town_qa_point,reason,cam.x,cam.z,town_message.c_str());launch->exit_code=1;running=false;}return value;};
        auto next=[&](unsigned delay=20){++town_qa_phase;town_qa_wait=frame+delay;};
        auto press=[&](SDL_GameControllerButton b){controller_test_pad.press(b);};
        auto walk=[&](const double (*points)[2],unsigned count){
            if(town_qa_point>=count){town_qa_point=0;return true;}
            const double dx=points[town_qa_point][0]-cam.x,dz=points[town_qa_point][1]-cam.z,d=std::hypot(dx,dz);
            if(d<.07){++town_qa_point;return false;}
            const double step=std::min(.12,d);const auto motion=move_camera(cam,scenery,cam.x+dx/d*step,cam.z+dz/d*step);
            ++town_qa_steps;town_qa_walk+=motion.actual;
            if(!check(!motion.slope&&!motion.collision,"Unobstructed walking capsule"))return false;
            cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.yaw=float(std::atan2(dx,-dz));cam.pitch=.02f;cam.grounded=true;cam.fly=false;return false;
        };
        if(frame==1){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();preview_pad.focused=true;}
        if(frame<town_qa_wait)return;
        if(frame>uint64_t(launch->frames)-120){check(false,"Frame budget exceeded");return;}
        switch(town_qa_phase){
        case 0:if(!check(town_enabled&&preview_pad.get(),"Quarter and virtual gamepad available"))return;preview_begin();next(30);break;
        case 1:{static const double path[][2]={{-1342,-10296},{-1353,-10319},{-1358.4,-10324.3}};if(walk(path,std::size(path)))next();break;}
        case 2:press(SDL_CONTROLLER_BUTTON_A);next();break;
        case 3:if(!check(preview_screen==5&&town_venue==0,"A opens the armorer"))return;press(SDL_CONTROLLER_BUTTON_A);next();break;
        case 4:if(!check(town_state.spent==0&&town_message.find("Not enough")!=std::string::npos,"Armory denies an unaffordable purchase"))return;press(SDL_CONTROLLER_BUTTON_B);next();break;
        case 5:if(!check(preview_screen==1,"B returns to walking"))return;next(1);break;
        case 6:{static const double path[][2]={{-1351,-10322},{-1341,-10314},{-1325,-10314},{-1309,-10314},{-1302,-10303},{-1297,-10303}};if(walk(path,std::size(path)))next();break;}
        case 7:press(SDL_CONTROLLER_BUTTON_A);next();break;
        case 8:if(!check(preview_screen==5&&town_venue==1,"A opens the grocer"))return;press(SDL_CONTROLLER_BUTTON_A);next();break;
        case 9:if(!check(town_state.welcome==1&&town_state.bread==1&&town_state.spent==0,"Focused A collects and saves the welcome ration"))return;press(SDL_CONTROLLER_BUTTON_B);next();break;
        case 10:if(!check(load_journey(),"Inventory and world checkpoint reload"))return;next(30);break;
        case 11:if(!check(town_state.welcome==1&&town_state.bread==1,"Welcome loaf persists after load"))return;press(SDL_CONTROLLER_BUTTON_Y);next();break;
        case 12:if(!check(preview_screen==3,"Y opens journal"))return;press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break;
        case 13:press(SDL_CONTROLLER_BUTTON_A);next();break;
        case 14:if(!check(town_state.prepared==2&&town_state.bread==0,"Journal food action prepares and saves a real benefit"))return;press(SDL_CONTROLLER_BUTTON_B);next();break;
        case 15:{static const double path[][2]={{-1306,-10298},{-1306,-10287},{-1294,-10287},{-1291,-10287},{-1284,-10287},{-1283,-10289}};if(walk(path,std::size(path)))next();break;}
        case 16:press(SDL_CONTROLLER_BUTTON_A);next();break;
        case 17:if(!check(preview_screen==5&&town_venue==2,"Tavern entered on foot; A speaks to innkeeper"))return;press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break;
        case 18:press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);next();break;
        case 19:press(SDL_CONTROLLER_BUTTON_A);next();break;
        case 20:if(!check(town_message.find("Mara")!=std::string::npos,"Controller selects the innkeeper's local news"))return;press(SDL_CONTROLLER_BUTTON_B);next();break;
        case 21:{static const double path[][2]={{-1284,-10287},{-1291,-10287},{-1294,-10287},{-1306,-10287},{-1310,-10287},{-1310,-10264.5},{-1310,-10259.5},{-1310,-10264.5},{-1310,-10287}};if(walk(path,std::size(path)))next();break;}
        case 22:
            if(!check(preview_screen==1&&town_state.prepared==2&&!studio.components_runtime->current_line(),"Return to square and open main gate; quest remains available"))return;
            town_qa_passed=true;std::printf("[Town QA] PASS %u assertions, %u collision-free walking steps, %.2f metres; armory, grocer, saved inventory, food, controller tavern and gate return\n",town_qa_checks,town_qa_steps,town_qa_walk);
            cam.x=-1335;cam.z=-10294;cam.y=25.75;cam.yaw=.95f;cam.pitch=.035f;temporal_cut=true;launch->frames=int(frame+90);break;
        }
    };
