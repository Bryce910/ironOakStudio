// Opt-in, bounded native integration rehearsal in a marked private project.
// Draft/countdown/idle-player fixtures only; no hit, block, damage or reward injection.
const bool arena_capacity_qa=arena_roster_qa&&std::getenv("VALWOOD_QA_ARENA_CAPACITY");
const char *arena_capacity_dir=std::getenv("VALWOOD_QA_ARENA_ROSTER_DIR");
unsigned capacity_match=0,capacity_phase=0,capacity_captures=0,capacity_team_body=0,capacity_team_steel=0,capacity_visible_peak=0;
std::uint64_t capacity_frame=0,capacity_initial_purse=0;
bool capacity_done=false;std::string capacity_capture;
std::vector<std::string> capacity_rows;
auto arena_capacity_finish=[&](bool passed,const std::string &error){
    capacity_done=true;std::ofstream out(std::filesystem::path(arena_capacity_dir)/"NATIVE_ARENA_SETTINGS_RAW.json");
    out<<"{\"passed\":"<<(passed?"true":"false")<<",\"error\":"<<world_json::quote(error)
       <<",\"matches\":"<<capacity_rows.size()<<",\"captures\":"<<capacity_captures
       <<",\"private_profile\":true,\"ordinary_start_and_geometry\":true,\"events_injected\":false,\"ui_mouse_tested\":false,\"rows\":[";
    for(unsigned i=0;i<capacity_rows.size();++i)out<<(i?",":"")<<capacity_rows[i];out<<"]}\n";
    if(!passed||!out)launch->exit_code=1;running=false;
    std::fprintf(stderr,"[Arena settings QA] %s: %s\n",passed?"PASS":"FAIL",error.c_str());
};
auto arena_capacity_qa_step=[&](std::uint64_t frame){
    if(!arena_capacity_qa||capacity_done)return;
    if(!arena_capacity_dir||!*arena_capacity_dir){launch->exit_code=1;running=false;return;}
    if(frame+20>=std::uint64_t(launch->frames)){arena_capacity_finish(false,"Native capacity rehearsal timed out");return;}
    if(destination_loading||!terrain.view_ready(cam.x,cam.z,false))return;
    if(capacity_phase==0){
        std::filesystem::create_directories(arena_capacity_dir);
        if(!arena_player_roster_ready||arena_player_roster.entries.size()!=18||!arena_progress_ready){arena_capacity_finish(false,"Knight roster or shared purse unavailable");return;}
        if(capacity_match==0)capacity_initial_purse=player_profile_balance();
        arena_show_setup();arena_player_character_draft=arena_player_roster.index(capacity_match?"knight18":"knight01");
        arena_total_draft=24;arena_simultaneous_draft=24;arena_teammates_draft=6;arena_difficulty_draft=capacity_match?2:1;arena_weapon_draft=0;arena_opponents_draft=capacity_match?1:0;
        capacity_capture=capacity_match?"orcs-setup":"knights-setup";capacity_frame=frame;capacity_phase=1;return;
    }
    if(capacity_phase==1&&frame>capacity_frame+3&&capacity_capture.empty()){
        if(!arena_begin()){arena_capacity_finish(false,arena_progress_error);return;}
        if(arena_game.config().total!=24||arena_game.config().simultaneous!=24||arena_game.config().teammates!=6||
           arena_game.config().difficulty!=world_arena_options::difficulty(unsigned(capacity_match?2:1))||fight.difficulty!=arena_game.config().difficulty||
           arena_enemy_appearances.size()!=24||arena_ally_appearances.size()!=6){arena_capacity_finish(false,"Challenge selection did not reach live arena");return;}
        for(unsigned id=0;id<world_combat_capacity::npcs;++id)if(!arena_actor_banks[id]||!world_combat_motion_anatomy_stats(arena_actor_banks[id]).loaded){arena_capacity_finish(false,"A selected NPC has no real anatomy bank");return;}
        unsigned ownership=0;for(const auto &bank:arena_appearance_banks){if(ownership&bank.mask){arena_capacity_finish(false,"Actor has duplicate appearance ownership");return;}ownership|=bank.mask;}
        if(ownership!=world_combat_capacity::actor_mask){arena_capacity_finish(false,"Missing player, hostile or allied appearance");return;}
        cam.x=arena_origin_x;cam.z=arena_origin_z-10;cam.y=arena_floor+1.75;cam.yaw=3.14159265359f;cam.pitch=-.08f;avatar_initialized=false;third_person=true;
        capacity_team_body=capacity_team_steel=capacity_visible_peak=0;capacity_frame=frame;capacity_phase=2;
    }
    if(capacity_phase==2){
        // Keep ordinary simulation running while the private test is unfocused.
        arena_gameplay_ready=true;
        const auto &events=arena_game.team_events();for(unsigned i=0;i<events.count;++i){const auto &e=events.values[i];
            if(arena_game.teammate(e.attacker_id)==arena_game.teammate(e.defender_id)){arena_capacity_finish(false,"Friendly NPC contact was accepted");return;}
            capacity_team_body+=e.kind==TrainingContactKind::EnemyBody||e.kind==TrainingContactKind::PlayerBody;
            capacity_team_steel+=e.kind==TrainingContactKind::EnemyBlock||e.kind==TrainingContactKind::PlayerBlock||e.kind==TrainingContactKind::PlayerParry;
        }
        unsigned visible=0;for(const auto &actor:arena_motion_actors)visible+=actor.visible;capacity_visible_peak=std::max(capacity_visible_peak,visible);
        if(frame==capacity_frame+360)capacity_capture=capacity_match?"orcs-fight":"knights-fight";
        if(frame<capacity_frame+720&&!arena_finished())return;
        if(!capacity_capture.empty())return;
        if(capacity_team_body==0){arena_capacity_finish(false,"Allies produced no actual source-geometry body contact");return;}
        if(capacity_visible_peak<24){arena_capacity_finish(false,"Large encounter did not render its selected fighters");return;}
        if(player_profile_balance()!=capacity_initial_purse){arena_capacity_finish(false,"Private incomplete fight altered purse");return;}
        if(glGetError()!=GL_NO_ERROR){arena_capacity_finish(false,"OpenGL error in full capacity encounter");return;}
        std::uint64_t gpu=0;for(const auto &bank:arena_appearance_banks){const auto stats=world_combat_motion_compact_stats(bank.motion.get());
            if(!stats.loaded||stats.failures||stats.actor_mask!=bank.mask){arena_capacity_finish(false,"Compact actor upload/ownership failure");return;}gpu+=stats.gpu_bytes;
        }
        std::ostringstream row;row<<"{\"opponents\":24,\"simultaneous\":24,\"teammates\":6,\"difficulty\":"<<world_json::quote(world_arena_options::difficulty_name(arena_game.config().difficulty))
            <<",\"species\":"<<world_json::quote(capacity_match?"Orcs":"Knights")<<",\"visible_peak\":"<<capacity_visible_peak
            <<",\"team_body_contacts\":"<<capacity_team_body<<",\"team_steel_contacts\":"<<capacity_team_steel<<",\"compact_gpu_bytes\":"<<gpu<<"}";
        capacity_rows.push_back(row.str());arena_show_setup();
        if(++capacity_match>=2){arena_capacity_finish(true,"");return;}capacity_phase=0;
    }
};
auto arena_capacity_qa_capture=[&](int width,int height){
    if(!arena_capacity_qa||capacity_done||capacity_capture.empty())return;
    std::vector<unsigned char> pixels(std::size_t(width)*height*3);GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    const std::size_t stride=std::size_t(width)*3;for(int y=0;y<height/2;++y)std::swap_ranges(pixels.data()+y*stride,pixels.data()+(y+1)*stride,pixels.data()+(height-1-y)*stride);
    auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,int(stride),SDL_PIXELFORMAT_RGB24);
    const auto path=std::filesystem::path(arena_capacity_dir)/(capacity_capture+".bmp");const bool ok=surface&&SDL_SaveBMP(surface,path.c_str())==0;if(surface)SDL_FreeSurface(surface);
    if(!ok){arena_capacity_finish(false,"Native capture failed");return;}++capacity_captures;capacity_capture.clear();
};
