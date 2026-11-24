// Optional extension of the owned opening SDL workflow. Every approach uses
// the production walking capsule; aiming uses the ordinary right stick. No
// camera, quest, switch-mask or light-power assignments are made by this QA.
const bool opening_room_lights_qa_enabled=opening_qa&&std::getenv("VALWOOD_QA_OPENING_ROOM_LIGHTS")&&
    !std::strcmp(std::getenv("VALWOOD_QA_OPENING_ROOM_LIGHTS"),"1");
bool opening_room_lights_qa_done=false;
unsigned opening_room_lights_qa_phase=0,opening_room_lights_qa_index=0;
uint64_t opening_room_lights_qa_frame=0;
const std::array<unsigned,4> opening_room_lights_qa_order={{0,3,2,1}};
auto opening_room_lights_qa_point=[](double x,double z){return world_opening_prologue::Vec{-3500+x,600.18,-11000+z};};
std::array<std::vector<world_opening_prologue::Vec>,5> opening_room_lights_qa_paths={{
    {opening_room_lights_qa_point(-.4,3.2),opening_room_lights_qa_point(.9,3.9)},
    {opening_room_lights_qa_point(.6,2.45),opening_room_lights_qa_point(3,2.45),opening_room_lights_qa_point(3,.625),opening_room_lights_qa_point(4.22,.625),opening_room_lights_qa_point(4.22,1.25)},
    {opening_room_lights_qa_point(4.22,.625),opening_room_lights_qa_point(3,.625),opening_room_lights_qa_point(3,-.6),opening_room_lights_qa_point(3.05,-2.05),opening_room_lights_qa_point(3.92,-2.25)},
    {opening_room_lights_qa_point(3.05,-2.05),opening_room_lights_qa_point(3.05,-.6),opening_room_lights_qa_point(.76,-.6),opening_room_lights_qa_point(.76,-1.85),opening_room_lights_qa_point(.65,-2.1)},
    {opening_room_lights_qa_point(.76,-1.85),opening_room_lights_qa_point(.76,-.6),opening_room_lights_qa_point(3.05,-.6),opening_room_lights_qa_point(3,2.45),opening_room_lights_qa_point(.6,2.45),opening_room_lights_qa_point(-.4,3.2)}
}};
if(world_opening_room_lights::authored_controls){
    for(size_t i=0;i<opening_room_lights_qa_paths.size();++i){
        opening_room_lights_qa_paths[i].clear();
        for(const auto &p:world_opening_room_lights::authored_paths[i])opening_room_lights_qa_paths[i].push_back({p.x,p.y,p.z});
    }
}
auto opening_room_lights_qa_aim=[&](unsigned room){
    const auto &p=world_opening_room_lights::switches[room].position;
    const double dx=p.x-cam.x,dy=p.y-cam.y,dz=p.z-cam.z;
    const double yaw=std::atan2(dx,-dz),pitch=std::atan2(dy,std::hypot(dx,dz));
    const double yaw_error=std::remainder(yaw-cam.yaw,6.283185307179586),pitch_error=pitch-cam.pitch;
    if(std::abs(yaw_error)<.018&&std::abs(pitch_error)<.018&&opening_room_light_target==int(room)){
        opening_prologue_qa_axes(0,0);return true;
    }
    double x=std::clamp(yaw_error*3.,-1.,1.),y=std::clamp(-pitch_error*3.*(preview_settings.invert_y?-1.:1.),-1.,1.);
    const double magnitude=std::hypot(x,y);
    if(magnitude>.00001){const double raw=preview_settings.deadzone+(1-preview_settings.deadzone)*std::min(magnitude,1.);x=x/magnitude*raw;y=y/magnitude*raw;}
    opening_prologue_qa_axes(0,0,Sint16(std::clamp(x,-1.,1.)*32767));
    controller_test_pad.axis(SDL_CONTROLLER_AXIS_RIGHTY,Sint16(std::clamp(y,-1.,1.)*32767));
    return false;
};
auto opening_room_lights_qa_press=[&](unsigned room,bool down){
    if(room==0||room==2)opening_prologue_qa_key(SDLK_e,down);
    else{controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,down);++opening_prologue_qa_events;}
};
auto opening_room_lights_qa_powers=[&](unsigned room,bool on){
    unsigned count=0;
    for(const auto &b:opening_room_light_bindings)if(b.room==room){
        ++count;if(b.index>=studio.live.lights.size()||std::abs(studio.live.lights[b.index].power-(on?b.power:0.f))>.00001)return false;
    }
    unsigned expected=0;
    for(const auto &fixture:world_opening_room_lights::practicals)expected+=fixture.room==room;
    return expected>0&&count==expected;
};
auto opening_room_lights_qa_tick=[&](uint64_t frame){
    if(!opening_room_lights_enabled||opening_room_light_bindings.size()!=world_opening_room_lights::practicals.size()){opening_prologue_qa_fail("Room-light QA requires four authored room switches and all bound practicals");return;}
    if(opening_room_lights_qa_index>4){opening_prologue_qa_fail("Room-light QA route index is invalid");return;}
    if(!opening_room_lights_qa_frame)opening_room_lights_qa_frame=frame;
    const uint64_t age=frame-opening_room_lights_qa_frame;
    const unsigned room=opening_room_lights_qa_order[std::min(opening_room_lights_qa_index,3u)];
    const char *names[]={"living","kitchen","bedroom","bathroom"};
    auto next=[&](unsigned phase){opening_room_lights_qa_phase=phase;opening_room_lights_qa_frame=frame;};
    auto snapshot=[&](const char *state){opening_prologue_qa_capture_pending=std::string(names[room])+"-light-"+state;};
    auto off_ok=[&](){return opening_state.room_lights()==(15u^(1u<<room))&&opening_room_lights_qa_powers(room,false)&&opening_room_light_controls.toggles[room]==1;};
    switch(opening_room_lights_qa_phase){
    case 0:{
        const auto &path=opening_room_lights_qa_paths[opening_room_lights_qa_index];
        if(!opening_prologue_qa_walk(path,unsigned(path.size()-1),.025))return;
        if(opening_room_lights_qa_index==4){
            if(opening_state.room_lights()!=15){opening_prologue_qa_fail("Room circuits were not restored before continuing the story");return;}
            opening_room_lights_qa_done=true;opening_prologue_qa_node=0;opening_prologue_qa_axes(0,0);opening_prologue_qa_pass("room_lights_walked_all_rooms");return;
        }
        next(1);return;
    }
    case 1:
        if(!opening_room_lights_qa_aim(room))return;
        next(2);return;
    case 2:
        if(age<12)return;
        if(opening_state.room_lights()!=15||opening_room_light_target!=int(room)||!opening_room_lights_qa_powers(room,true)){opening_prologue_qa_fail("Looking at the physical switch did not select its powered room circuit");return;}
        // The workflow runs before the frame's SDL actions and render. Keep
        // the on state for complete rendered frames after scheduling its
        // capture; pressing now would make the labelled on capture show off.
        snapshot("on");next(17);return;
    case 17:
        if(age<3)return;
        if(opening_state.room_lights()!=15||opening_room_light_target!=int(room)||!opening_room_lights_qa_powers(room,true)){opening_prologue_qa_fail("Room power changed before the on capture finished rendering");return;}
        opening_room_lights_qa_press(room,true);next(3);return;
    case 3:
        if(age<12)return;
        if(!off_ok()){opening_prologue_qa_fail("Ordinary E/A did not turn off exactly one room circuit");return;}
        snapshot("off");next(4);return;
    case 4:
        if(age<12)return;
        if(!off_ok()){opening_prologue_qa_fail("Holding E/A repeated the room switch");return;}
        opening_room_lights_qa_press(room,false);next(room==0?5:15);return;
    case 5:
        if(age<3)return;
        opening_prologue_qa_tap(SDLK_ESCAPE,frame);next(6);return;
    case 6:
        if(age<5)return;
        if(preview_screen!=2){opening_prologue_qa_fail("Room-light pause check did not reach the pause menu");return;}
        opening_prologue_qa_key(SDLK_e,true);next(7);return;
    case 7:
        if(age<8)return;
        if(!off_ok()){opening_prologue_qa_fail("Paused E input toggled a room circuit");return;}
        opening_prologue_qa_tap(SDLK_ESCAPE,frame);next(8);return;
    case 8:
        if(age<8)return;
        if(preview_screen!=1||!off_ok()){opening_prologue_qa_fail("A held pause-menu input toggled the switch when resuming");return;}
        opening_prologue_qa_key(SDLK_e,false);opening_prologue_qa_pass("room_lights_pause_and_held_resume");next(9);return;
    case 9:
        if(age<4)return;
        opening_prologue_qa_tap(SDLK_F5,frame);next(10);return;
    case 10:{
        if(age<4)return;
        std::ifstream input("saves/opening.progress");world_opening_prologue::Save saved;std::string error;
        if(!input||!world_opening_prologue::parse_save(input,opening_state.config(),saved,error)||saved.room_lights!=14){opening_prologue_qa_fail("F5 did not persist the individual room circuit state");return;}
        opening_prologue_qa_tap(SDLK_F8,frame);next(11);return;
    }
    case 11:{
        if(age<10)return;
        const auto &p=opening_state.config().house_spawn.feet;
        if(!off_ok()||std::hypot(cam.x-p.x,cam.z-p.z)>.03||opening_state.inspected()){opening_prologue_qa_fail("F8 did not restore the room mask and supported pre-notebook checkpoint");return;}
        opening_prologue_qa_pass("room_lights_checkpoint_save_restore");opening_prologue_qa_tap(SDLK_r,frame);next(12);return;
    }
    case 12:{
        if(age<10)return;
        const auto &p=opening_state.config().house_spawn.feet;
        if(!off_ok()||std::hypot(cam.x-p.x,cam.z-p.z)>.03){opening_prologue_qa_fail("Retry did not preserve the individual room circuit state");return;}
        opening_prologue_qa_pass("room_lights_checkpoint_retry");opening_prologue_qa_node=0;next(13);return;
    }
    case 13:{
        const auto &path=opening_room_lights_qa_paths[0];
        if(!opening_prologue_qa_walk(path,unsigned(path.size()-1),.025))return;
        next(14);return;
    }
    case 14:
        if(!opening_room_lights_qa_aim(room))return;
        next(15);return;
    case 15:
        if(age<8)return;
        opening_room_lights_qa_press(room,true);next(16);return;
    case 16:
        if(age<12)return;
        if(opening_state.room_lights()!=15||!opening_room_lights_qa_powers(room,true)||opening_room_light_controls.toggles[room]!=2){opening_prologue_qa_fail("The next released E/A press did not turn its room circuit back on");return;}
        opening_room_lights_qa_press(room,false);
        opening_prologue_qa_pass((std::string("room_lights_")+names[room]+"_off_on_held").c_str());
        ++opening_room_lights_qa_index;opening_prologue_qa_node=0;next(0);return;
    default:opening_prologue_qa_fail("Invalid room-light QA phase");return;
    }
};
