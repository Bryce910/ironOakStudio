// Marked private fixture only. Supplies ordinary SDL keys/buttons and a real
// SDL virtual controller; it never sets equipment, combat, inventory or poses.
unsigned equipment_qa_phase=0,equipment_qa_checks=0,equipment_qa_events=0,equipment_qa_captures=0;
std::uint64_t equipment_qa_started=0,equipment_qa_phase_frame=0,equipment_qa_attack_serial=0;
bool equipment_qa_done=false,equipment_qa_ok=false,equipment_qa_focus_sent=false,equipment_qa_modal_seen=false;
float equipment_qa_modal_progress=0;std::string equipment_qa_error,equipment_qa_pending;
const char *equipment_qa_directory=std::getenv("VALWOOD_QA_EQUIPMENT_DIR");
std::ofstream equipment_qa_trace;
if(equipment_qa){
    if(!equipment_qa_directory||!*equipment_qa_directory){std::fprintf(stderr,"[Equipment QA] Output directory required\n");launch->exit_code=1;running=false;}
    else{std::filesystem::create_directories(equipment_qa_directory);equipment_qa_trace.open(std::filesystem::path(equipment_qa_directory)/"equipment-frames.jsonl",std::ios::trunc);}
}
auto equipment_qa_finish=[&](bool ok,const char *error){
    if(equipment_qa_done)return;
    equipment_qa_done=true;equipment_qa_ok=ok;equipment_qa_error=error;
    controller_test_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);controller_test_pad.tick();
    if(equipment_qa_trace)equipment_qa_trace.flush();
    if(equipment_qa_directory){
        std::ofstream out(std::filesystem::path(equipment_qa_directory)/"EQUIPMENT_NATIVE_RAW.json");
        out<<"{\"passed\":"<<(ok?"true":"false")<<",\"error\":"<<world_json::quote(error)
           <<",\"phase\":"<<equipment_qa_phase<<",\"checks\":"<<equipment_qa_checks<<",\"tagged_sdl_events\":"<<equipment_qa_events
           <<",\"captures\":"<<equipment_qa_captures<<",\"virtual_pad\":true,\"combat_outcomes_injected\":false,\"equipment_state_injected\":false,\"fixed_dt_hz\":60,\"focus_accommodation\":\"marked private offscreen only\"}\n";
    }
    std::fprintf(stderr,"[Equipment QA] %s phase=%u checks=%u: %s\n",ok?"PASS":"FAIL",equipment_qa_phase,equipment_qa_checks,error);
    if(!ok)launch->exit_code=1;
    running=false;
};
auto equipment_qa_check=[&](bool condition,const char *message){++equipment_qa_checks;if(!condition)equipment_qa_finish(false,message);return condition;};
auto equipment_qa_key=[&](SDL_Keycode code){
    for(bool down:{true,false}){SDL_Event e={};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.windowID=SDL_GetWindowID(window);e.key.state=down?SDL_PRESSED:SDL_RELEASED;e.key.keysym.sym=code;e.key.keysym.unused=0x56414c54u;if(SDL_PushEvent(&e)==1)++equipment_qa_events;}
};
auto equipment_qa_mouse=[&](bool down){SDL_Event e={};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.which=0x56414c54u;e.button.button=SDL_BUTTON_LEFT;e.button.state=down?SDL_PRESSED:SDL_RELEASED;if(SDL_PushEvent(&e)==1)++equipment_qa_events;};
auto equipment_qa_pad_press=[&](SDL_GameControllerButton button){controller_test_pad.button(button,true);controller_test_pad.releases[button]=controller_test_pad.clock+2;SDL_JoystickUpdate();};
auto equipment_qa_next=[&](unsigned phase,std::uint64_t frame){equipment_qa_phase=phase;equipment_qa_phase_frame=frame;};
auto equipment_qa_step=[&](std::uint64_t frame){
    if(!equipment_qa||equipment_qa_done)return;
    controller_test_pad.tick();
    if(!equipment_qa_focus_sent){SDL_Event e={};e.type=SDL_WINDOWEVENT;e.window.windowID=SDL_GetWindowID(window);e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;SDL_PushEvent(&e);equipment_qa_focus_sent=true;}
    if(frame+3>=std::uint64_t(launch->frames)){equipment_qa_finish(false,"Bounded equipment frame budget exhausted");return;}
    if(!equipment_qa_started){
        if(destination_loading||!rig_capture_started||!arena_controls_ready()||!inventory_controls_ready||!skeletal_hero.ready())return;
        if(!equipment_qa_check(controller_test_pad.joystick&&preview_pad.id()==SDL_JoystickInstanceID(controller_test_pad.joystick),"Owned actual virtual controller")||
           !equipment_qa_check(skeletal_hero.supports_equipment()&&fight.weapon==world_weapon_loadout::Weapon::Sword,"Actual extended Sword rig loaded"))return;
        equipment_qa_started=frame;equipment_qa_attack_serial=fight.attack_start_serial;equipment_qa_pending="01-sword-armed";equipment_qa_next(1,frame);return;
    }
    const auto age=frame-equipment_qa_phase_frame;
    if(equipment_qa_phase==1&&age==6){equipment_qa_key(SDLK_z);equipment_qa_next(2,frame);}
    else if(equipment_qa_phase==3&&age==1){controller_test_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);SDL_JoystickUpdate();equipment_qa_mouse(true);}
    else if(equipment_qa_phase==4&&age==1){controller_test_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);SDL_JoystickUpdate();equipment_qa_mouse(false);}
    else if(equipment_qa_phase==4&&age==8){equipment_qa_key(SDLK_3);equipment_qa_next(5,frame);}
    else if(equipment_qa_phase==6&&age==1){equipment_qa_key(SDLK_v);equipment_qa_key(SDLK_4);}
    else if(equipment_qa_phase==7&&age==1&&!equipment_qa_modal_seen){equipment_qa_key(SDLK_i);}
    else if(equipment_qa_phase==8&&age==1){equipment_qa_key(SDLK_i);}
    else if(equipment_qa_phase==10&&age==1){equipment_qa_key(SDLK_5);}
    else if(equipment_qa_phase==12&&age==1){equipment_qa_key(SDLK_2);}
    else if(equipment_qa_phase==14&&age==1){equipment_qa_pad_press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);}
    else if(equipment_qa_phase==15&&age==4){equipment_qa_pad_press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);}
    else if(equipment_qa_phase==16&&age==8){equipment_qa_key(SDLK_1);equipment_qa_next(17,frame);}
    else if(equipment_qa_phase==18&&age==1){equipment_qa_mouse(true);}
    else if(equipment_qa_phase==18&&age==2){equipment_qa_mouse(false);}
};
auto equipment_qa_observe=[&](std::uint64_t frame,bool controls,bool paused){
    if(!equipment_qa||equipment_qa_done||!equipment_qa_started)return;
    const auto &stats=skeletal_hero.stats();double base[3]={},tip[3]={};const bool blade=skeletal_hero.blade(base,tip);
    const bool melee=WorldSkeletalHeroLoadouts::supports_weapon(fight.weapon);
    std::vector<world_combat_anatomy::Capsule> body;std::string body_error;
    const bool body_ok=!melee||skeletal_hero.body_capsules(cam.x,cam.y-1.75,cam.z,avatar_yaw,body,body_error);
    if(equipment_qa_trace)equipment_qa_trace<<"{\"frame\":"<<frame<<",\"qa_phase\":"<<equipment_qa_phase<<",\"weapon\":"<<unsigned(fight.weapon)
        <<",\"saved_weapon\":"<<unsigned(world_player_inventory::weapon_type(player_inventory.weapon))<<",\"sheathed\":"<<(weapon_sheathed?"true":"false")
        <<",\"queued\":"<<(equipment_change.active()?"true":"false")<<",\"queued_target\":"<<equipment_change.target()
        <<",\"supports_equipment\":"<<(skeletal_hero.supports_equipment()?"true":"false")<<",\"transition\":"<<(stats.equipment_transition?"true":"false")
        <<",\"progress\":"<<stats.equipment_progress<<",\"weapon_visible\":"<<(stats.equipment_weapon_visible?"true":"false")<<",\"blade_query\":"<<(blade?"true":"false")
        <<",\"controls\":"<<(controls?"true":"false")<<",\"paused\":"<<(paused?"true":"false")<<",\"inventory_open\":"<<(inventory_open?"true":"false")
        <<",\"health\":"<<fight.health<<",\"attack_serial\":"<<fight.attack_start_serial<<",\"resolved_serial\":"<<fight.resolved_attack_serial
        <<",\"player_phase\":"<<unsigned(fight.player_phase)<<",\"request_result\":"<<unsigned(equipment_request_feedback.result)
        <<",\"body_capsules\":"<<body.size()<<",\"body_query\":"<<(body_ok?"true":"false")
        <<",\"foot_plants\":"<<stats.foot_plants<<",\"foot_releases\":"<<stats.foot_releases<<",\"foot_residual_m\":"<<stats.maximum_foot_residual_m
        <<",\"third_person\":"<<(third_person?"true":"false")<<",\"character\":"<<world_json::quote(arena_player_character_loaded)<<"}\n";
    if(!equipment_qa_check(fight.health>0&&fight.state!=TrainingState::Lost,"Real player remains alive")||
       !equipment_qa_check(!skeletal_hero_loaded||std::isfinite(stats.equipment_progress),"Finite production equipment pose")||
       !equipment_qa_check(body_ok&&(!melee||body.size()>=8),"Actual displayed body collision retained during equipment movement"))return;
    const auto age=frame-equipment_qa_phase_frame;
    if(melee&&(weapon_sheathed||stats.equipment_transition))if(!equipment_qa_check(!blade,"Hidden/transitioning weapon has no physical blade"))return;
    if(equipment_qa_phase==2&&weapon_sheathed&&skeletal_hero.equipment_stowed_settled()){
        equipment_qa_pending="02-sword-stowed";equipment_qa_next(3,frame);
    }else if(equipment_qa_phase==3&&age>60&&skeletal_hero.equipment_combat_ready()){
        if(!equipment_qa_check(!weapon_sheathed&&fight.attack_start_serial==equipment_qa_attack_serial&&!fight.guard,"Held-through-draw input never replayed an attack/guard"))return;
        equipment_qa_pending="03-sword-drawn-held";equipment_qa_next(4,frame);
    }else if(equipment_qa_phase==5&&!equipment_change.active()&&fight.weapon==world_weapon_loadout::Weapon::SwordShield&&skeletal_hero.equipment_combat_ready()){
        if(!equipment_qa_check(world_player_inventory::weapon_type(player_inventory.weapon)==fight.weapon,"Shield identity committed durably"))return;
        equipment_qa_pending="04-shield-ready";equipment_qa_next(6,frame);
    }else if(equipment_qa_phase==6&&equipment_change.active()){
        if(!equipment_qa_check(fight.weapon==world_weapon_loadout::Weapon::SwordShield,"Old identity retained while lowering"))return;
        equipment_qa_next(7,frame);
    }else if(equipment_qa_phase==7&&inventory_open){
        if(!equipment_qa_modal_seen){equipment_qa_modal_seen=true;equipment_qa_modal_progress=stats.equipment_progress;equipment_qa_phase_frame=frame;}
        if(!equipment_qa_check(paused&&!controls&&equipment_change.active()&&fight.weapon==world_weapon_loadout::Weapon::SwordShield&&stats.equipment_progress==equipment_qa_modal_progress,"Backpack pauses the real queued equipment pose and identity"))return;
        if(frame-equipment_qa_phase_frame>=12){equipment_qa_pending="05-queue-paused-inventory";equipment_qa_next(8,frame);}
    }else if(equipment_qa_phase==8&&!inventory_open){equipment_qa_next(9,frame);
    }else if(equipment_qa_phase==9&&!equipment_change.active()&&fight.weapon==world_weapon_loadout::Weapon::GreatAxe&&skeletal_hero.equipment_combat_ready()){
        equipment_qa_pending="06-axe-ready";equipment_qa_next(10,frame);
    }else if(equipment_qa_phase==10&&equipment_change.active()){equipment_qa_next(11,frame);
    }else if(equipment_qa_phase==11&&!equipment_change.active()&&fight.weapon==world_weapon_loadout::Weapon::Staff&&skeletal_hero.equipment_combat_ready()){
        equipment_qa_pending="07-staff-ready";equipment_qa_next(12,frame);
    }else if(equipment_qa_phase==12&&equipment_change.active()){equipment_qa_next(13,frame);
    }else if(equipment_qa_phase==13&&!equipment_change.active()&&fight.weapon==world_weapon_loadout::Weapon::Bow&&!weapon_sheathed){
        equipment_qa_pending="08-bow-ready";equipment_qa_next(14,frame);
    }else if(equipment_qa_phase==14&&weapon_sheathed){equipment_qa_pending="09-bow-stowed-pad";equipment_qa_next(15,frame);
    }else if(equipment_qa_phase==15&&age>5&&!weapon_sheathed){equipment_qa_next(16,frame);
    }else if(equipment_qa_phase==17&&!equipment_change.active()&&fight.weapon==world_weapon_loadout::Weapon::Sword&&skeletal_hero.equipment_combat_ready()&&fight.state==TrainingState::Fighting&&equipment_action_gate.armed()&&controls){
        equipment_qa_attack_serial=fight.attack_start_serial;equipment_qa_next(18,frame);
    }else if(equipment_qa_phase==18&&fight.attack_start_serial>equipment_qa_attack_serial){
        if(!equipment_qa_check(fight.attack_start_serial==equipment_qa_attack_serial+1,"One fresh ordinary attack after all equipment transitions"))return;
        equipment_qa_pending="10-fresh-attack";equipment_qa_next(19,frame);
    }else if(equipment_qa_phase==19&&fight.player_phase==TrainingPlayerPhase::Ready&&age>30){
        if(!equipment_qa_check(fight.attack_start_serial==equipment_qa_attack_serial+1&&fight.resolved_attack_serial==fight.attack_start_serial,"Fresh attack resolved once and recovered without held replay")||
           !equipment_qa_check(world_player_inventory::weapon_type(player_inventory.weapon)==fight.weapon,"Final durable/live weapon identity matches"))return;
        equipment_qa_pending="11-fresh-recovered";equipment_qa_next(20,frame);
    }
};
auto equipment_qa_capture=[&](int width,int height){
    if(!equipment_qa||equipment_qa_done)return;
    if(!equipment_qa_pending.empty()){
        std::vector<unsigned char> pixels(std::size_t(width)*height*3);GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
        const std::size_t stride=std::size_t(width)*3;for(int y=0;y<height/2;++y)std::swap_ranges(pixels.data()+y*stride,pixels.data()+(y+1)*stride,pixels.data()+(height-1-y)*stride);
        auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,width*3,SDL_PIXELFORMAT_RGB24);
        const auto path=std::filesystem::path(equipment_qa_directory)/(equipment_qa_pending+".bmp");const bool ok=surface&&SDL_SaveBMP(surface,path.c_str())==0;if(surface)SDL_FreeSurface(surface);
        if(!equipment_qa_check(ok&&glGetError()==GL_NO_ERROR,"Actual native framebuffer capture"))return;
        ++equipment_qa_captures;equipment_qa_pending.clear();
    }
    if(equipment_qa_phase==20)equipment_qa_finish(true,"Ordinary input equipment sequence and fresh attack completed");
};
