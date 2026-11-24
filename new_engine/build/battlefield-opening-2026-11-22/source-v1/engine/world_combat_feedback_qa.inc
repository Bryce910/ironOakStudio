// Private bounded production-path rehearsal. Only geometry placement/countdown
// fast-forward are fixtures; all contacts/outcomes are produced by ordinary AI
// and the real Hero/Orc anatomical queries. Never inject ContactEvents.
const bool combat_feedback_qa=arena_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_CONTACT_FEEDBACK");
const char *combat_feedback_qa_dir=std::getenv("VALWOOD_QA_CONTACT_FEEDBACK_DIR");
if(combat_feedback_qa){std::ifstream marker(".valwood-contact-feedback-qa");std::string token;std::getline(marker,token);
    if(token!="VALWOOD_PRIVATE_CONTACT_FEEDBACK_V1"||!combat_feedback_qa_dir||!*combat_feedback_qa_dir){std::fprintf(stderr,"[Combat feedback QA] Missing private marker/directory\n");return 1;}}
unsigned feedback_qa_case=0,feedback_qa_phase=0,feedback_qa_captures=0;
std::uint64_t feedback_qa_start=0,feedback_qa_impact=0,feedback_qa_before_audio=0,feedback_qa_before_metal_audio=0,feedback_qa_before_body_audio=0,feedback_qa_before_rumble=0;
unsigned feedback_qa_before_metal=0,feedback_qa_before_body=0;
bool feedback_qa_done=false,feedback_qa_found=false,feedback_qa_pre=false;
std::string feedback_qa_pending;std::vector<std::string> feedback_qa_rows;
auto feedback_qa_weapon=[&](){const unsigned ids[]={0,2,3,4};return ids[feedback_qa_case<24?feedback_qa_case/6:0];};
auto feedback_qa_mode=[&](){return feedback_qa_case<24?(feedback_qa_case%6)/2:3+(feedback_qa_case-24)/2;};
auto feedback_qa_name=[&](){const char *weapons[]={"sword","bow","shield","axe","staff"};const char *modes[]={"miss","blocked","body","block","parry"};
    return std::string(weapons[feedback_qa_weapon()])+"-"+modes[feedback_qa_mode()]+(feedback_qa_case%2?"-first":"-third");};
auto feedback_qa_finish=[&](bool passed,const std::string &error){
    feedback_qa_done=true;std::ofstream out(std::filesystem::path(combat_feedback_qa_dir)/"NATIVE_FEEDBACK_RAW.json");
    out<<"{\"schema\":\"valwood.combat-feedback.native-scenarios.v1\",\"passed\":"<<(passed?"true":"false")
       <<",\"error\":"<<world_json::quote(error)<<",\"expected_cases\":28,\"reviewed_cases\":"<<feedback_qa_rows.size()
       <<",\"captures\":"<<feedback_qa_captures<<",\"actual_training_events\":true,\"actual_owned_geometry\":true,\"injected_outcome_events\":false,\"countdown_fastforwarded\":true,\"cases\":[";
    for(unsigned i=0;i<feedback_qa_rows.size();++i)out<<(i?",":"")<<feedback_qa_rows[i];out<<"]}\n";
    if(!out)passed=false;if(!passed)launch->exit_code=1;running=false;
    std::fprintf(stderr,"[Combat feedback QA] %s %zu/28 actual cases: %s\n",passed?"PASS":"FAIL",feedback_qa_rows.size(),error.c_str());
};
auto feedback_qa_step=[&](std::uint64_t frame){
    if(!combat_feedback_qa||feedback_qa_done)return;
    if(feedback_qa_case>=28){arena_show_setup();feedback_qa_finish(true,"");return;}
    if(frame+20>=std::uint64_t(launch->frames)){feedback_qa_finish(false,"Bounded rehearsal timed out: "+feedback_qa_name());return;}
    if(destination_loading||!terrain.view_ready(cam.x,cam.z,false))return;
    if(feedback_qa_phase==0){
        if(!training_sounds.ready||!arena_orc_catalog_ready){feedback_qa_finish(false,"Actual audio/Orc bank unavailable");return;}
        std::error_code error;std::filesystem::create_directories(combat_feedback_qa_dir,error);if(error){feedback_qa_finish(false,error.message());return;}
        preview_settings.vibration=true;preview_settings.reduced_motion=false;
        arena_show_setup();arena_opponents_draft=int(world_arena_opponents::Kind::Orc);arena_orc_character_draft=arena_orc_roster.index("gnur");
        arena_total_draft=2;arena_simultaneous_draft=1;arena_weapon_draft=int(feedback_qa_weapon());
        if(!arena_begin()){feedback_qa_finish(false,arena_progress_error);return;}
        cam.x=arena_origin_x;cam.z=arena_origin_z;cam.y=arena_floor+1.75;cam.yaw=3.14159265359f;cam.pitch=-.06f;
        third_person=feedback_qa_case%2==0;avatar_initialized=false;arena_gameplay_ready=true;
        for(unsigned n=0;n<900&&(fight.state!=TrainingState::Fighting||!arena_game.active_count());++n)arena_game.update(1./60.,fight,{0,0,0},0,false);
        if(fight.state!=TrainingState::Fighting||!arena_game.active_count()){feedback_qa_finish(false,"Ordinary countdown/entry did not complete");return;}
        feedback_qa_start=frame;feedback_qa_phase=1;feedback_qa_found=feedback_qa_pre=false;
    }
    arena_gameplay_ready=true;
    if(frame-feedback_qa_start>240){feedback_qa_finish(false,"No expected actual outcome: "+feedback_qa_name());return;}
    const unsigned mode=feedback_qa_mode();const unsigned weapon=feedback_qa_weapon();
    if(mode<3){
        const double steel=weapon==4?1.10:1.40;const double distance=mode==0?3.8:mode==1?steel:1.25;
        if(arena_game.opponent(0)&&arena_game.opponent(0)->health>0&&!arena_game.fixture_geometry_pose(0,{0,0,distance},3.14159265359f,mode==1?TrainingDirection::Left:TrainingDirection::High,fight.attack_start_serial)){
            feedback_qa_finish(false,"Actual geometric placement rejected");return;}
    }else if(feedback_qa_phase>=2){
        arena_game.fixture_release_geometry();
        if(const auto *enemy=arena_game.opponent(0)){
            if(enemy->phase==TrainingEnemyPhase::Windup){fight.choose(enemy->direction);
                fight.set_guard(mode==3||enemy->duration-enemy->time<.06);
                if(!feedback_qa_pre&&enemy->duration-enemy->time<.055){feedback_qa_pending=feedback_qa_name()+"-before";feedback_qa_pre=true;}}
        }
    }
    if(feedback_qa_phase==1&&frame>=feedback_qa_start+24){
        feedback_qa_before_audio=training_sounds.events;feedback_qa_before_metal_audio=training_sounds.metal_events;feedback_qa_before_body_audio=training_sounds.body_events;feedback_qa_before_rumble=combat_feedback_haptic_dispatches;
        const auto vfx=combat_vfx.stats();feedback_qa_before_metal=vfx.metal_bursts;feedback_qa_before_body=vfx.body_bursts;
        if(mode<3){fight.choose(TrainingDirection::Left);if(!fight.attack()){feedback_qa_finish(false,"Ordinary attack rejected");return;}}
        else {arena_game.fixture_release_geometry();fight.set_guard(false);}
        feedback_qa_phase=2;
    }
    if(mode<3&&feedback_qa_phase==2&&!feedback_qa_pre&&fight.player_phase==TrainingPlayerPhase::Windup&&fight.player_duration-fight.player_time<.055){
        feedback_qa_pending=feedback_qa_name()+"-before";feedback_qa_pre=true;
    }
    if(feedback_qa_phase==3&&frame>=feedback_qa_impact+7&&feedback_qa_pending.empty()){
        feedback_qa_pending=feedback_qa_name()+"-after";feedback_qa_phase=4;
    }else if(feedback_qa_phase==4&&feedback_qa_pending.empty()){
        if(!feedback_qa_found||!feedback_qa_pre){feedback_qa_finish(false,"Missing real event/pre-contact capture");return;}
        ++feedback_qa_case;feedback_qa_phase=0;
    }
};
auto feedback_qa_observe=[&](std::uint64_t frame){
    if(!combat_feedback_qa||feedback_qa_done||feedback_qa_phase!=2)return;
    const unsigned mode=feedback_qa_mode();
    const TrainingContactKind kinds[]={TrainingContactKind::PlayerMiss,TrainingContactKind::EnemyBlock,TrainingContactKind::PlayerBody,TrainingContactKind::PlayerBlock,TrainingContactKind::PlayerParry};
    const auto &events=arena_game.events();
    for(unsigned n=0;n<events.count;++n){const auto &event=events.values[n];if(event.kind!=kinds[mode])continue;
        const auto &cue=mode<3?combat_feedback.attack_notice():combat_feedback.defense_notice();
        const world_combat_feedback::Kind expected[]={world_combat_feedback::Kind::Miss,world_combat_feedback::Kind::Blocked,world_combat_feedback::Kind::Body,world_combat_feedback::Kind::Block,world_combat_feedback::Kind::Parry};
        const auto vfx=combat_vfx.stats();const unsigned metal=vfx.metal_bursts-feedback_qa_before_metal,body=vfx.body_bursts-feedback_qa_before_body;
        const auto metal_audio=training_sounds.metal_events-feedback_qa_before_metal_audio,body_audio=training_sounds.body_events-feedback_qa_before_body_audio;
        const auto haptic=combat_feedback_haptic_dispatches-feedback_qa_before_rumble;
        const bool physical=mode==0||mode>=3||event.anatomical;
        if(cue.kind!=expected[mode]||!physical||!training_sounds.ready||
           (mode==0?(metal||body||metal_audio||body_audio||haptic||combat_feedback.camera_impulse()!=0):
            mode==2?(!body||metal||!body_audio||metal_audio||!haptic):(!metal||body||!metal_audio||body_audio||!haptic))){
            std::ostringstream error;error<<"Real feedback routing differs: "<<feedback_qa_name()<<" kind="<<unsigned(cue.kind)<<" metal/body="<<metal<<"/"<<body<<" audio="<<metal_audio<<"/"<<body_audio<<" haptic="<<haptic;
            feedback_qa_finish(false,error.str());return;
        }
        std::ostringstream row;row<<std::setprecision(12)<<"{\"name\":"<<world_json::quote(feedback_qa_name())<<",\"weapon\":"<<feedback_qa_weapon()
            <<",\"mode\":"<<mode<<",\"first_person\":"<<(third_person?"false":"true")<<",\"event_serial\":"<<event.serial<<",\"attack_serial\":"<<event.attack_serial
            <<",\"anatomical\":"<<(event.anatomical?"true":"false")<<",\"damage\":"<<event.damage<<",\"metal_bursts\":"<<metal<<",\"body_bursts\":"<<body
            <<",\"metal_audio_played\":"<<metal_audio<<",\"body_audio_played\":"<<body_audio<<",\"audio_played\":"<<(training_sounds.events-feedback_qa_before_audio)
            <<",\"haptic_dispatches\":"<<haptic<<",\"camera_impulse\":"<<combat_feedback.camera_impulse()<<",\"cue_kind\":"<<unsigned(cue.kind)<<",\"frame\":"<<frame<<"}";
        feedback_qa_rows.push_back(row.str());feedback_qa_found=true;feedback_qa_impact=frame;feedback_qa_phase=3;feedback_qa_pending=feedback_qa_name()+"-impact";return;
    }
};
auto feedback_qa_capture=[&](int width,int height){
    if(!combat_feedback_qa||feedback_qa_pending.empty()||feedback_qa_done)return;
    const auto path=std::filesystem::path(combat_feedback_qa_dir)/(feedback_qa_pending+".bmp");std::vector<unsigned char> pixels(std::size_t(width)*height*3);
    GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    const std::size_t stride=std::size_t(width)*3;for(int y=0;y<height/2;++y)std::swap_ranges(pixels.data()+y*stride,pixels.data()+(y+1)*stride,pixels.data()+(height-1-y)*stride);
    auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,int(stride),SDL_PIXELFORMAT_RGB24);const bool okay=surface&&SDL_SaveBMP(surface,path.c_str())==0;SDL_FreeSurface(surface);
    if(!okay||glGetError()!=GL_NO_ERROR){feedback_qa_finish(false,"Native framebuffer capture failed");return;}++feedback_qa_captures;feedback_qa_pending.clear();
};
