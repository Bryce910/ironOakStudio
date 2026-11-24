// Bounded private load/render coverage. This selects ordinary setup drafts and
// invokes arena_begin; it does not alter damage, anatomy, AI or award a victory.
const bool orc_roster_qa=arena_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_ORC_ROSTER");
const char *orc_roster_qa_directory=std::getenv("VALWOOD_QA_ORC_ROSTER_DIR");
unsigned orc_roster_qa_index=0,orc_roster_qa_phase=0,orc_roster_qa_captures=0;
std::uint64_t orc_roster_qa_frame=0,orc_roster_qa_balance=0,orc_roster_qa_wins=0;
bool orc_roster_qa_started=false,orc_roster_qa_done=false;
std::string orc_roster_qa_pending;
std::vector<std::string> orc_roster_qa_rows;
auto orc_roster_qa_finish=[&](bool passed,const std::string &error){
    orc_roster_qa_done=true;
    if(orc_roster_qa_directory&&*orc_roster_qa_directory){
        std::ofstream report(std::filesystem::path(orc_roster_qa_directory)/"NATIVE_ROSTER_CYCLE.json");
        report<<"{\"schema\":\"valwood.orc-roster.native-cycle.v1\",\"passed\":"<<(passed?"true":"false")
              <<",\"error\":"<<world_json::quote(error)<<",\"expected_characters\":"<<arena_orc_roster.entries.size()
              <<",\"reviewed_characters\":"<<orc_roster_qa_rows.size()<<",\"captures\":"<<orc_roster_qa_captures
              <<",\"peak_resident_orc_banks\":"<<arena_orc_peak_banks
              <<",\"coins_awarded\":"<<(arena_progress.balance-orc_roster_qa_balance)
              <<",\"scripted_load_render_only\":true,\"controller_feel_accepted\":false,\"characters\":[";
        for(unsigned i=0;i<orc_roster_qa_rows.size();++i)report<<(i?",":"")<<orc_roster_qa_rows[i];
        report<<"]}\n";
        if(!report)passed=false;
    }else passed=false;
    if(!passed)launch->exit_code=1;
    std::fprintf(stderr,"[Orc roster QA] %s %u/%zu characters, %u captures: %s\n",passed?"PASS":"FAIL",
        unsigned(orc_roster_qa_rows.size()),arena_orc_roster.entries.size(),orc_roster_qa_captures,error.c_str());
    running=false;
};
auto orc_roster_qa_step=[&](std::uint64_t frame){
    if(!orc_roster_qa||orc_roster_qa_done)return;
    if(!orc_roster_qa_started){
        orc_roster_qa_started=true;orc_roster_qa_balance=arena_progress.balance;orc_roster_qa_wins=arena_progress.wins;
        if(!arena_orc_catalog_ready||!arena_progress_ready||!orc_roster_qa_directory||!*orc_roster_qa_directory){
            orc_roster_qa_finish(false,"Private roster/profile/capture directory unavailable.");return;
        }
        std::error_code error;std::filesystem::create_directories(orc_roster_qa_directory,error);
        if(error){orc_roster_qa_finish(false,"Could not create private capture directory.");return;}
    }
    if(frame+12>=std::uint64_t(launch->frames)){orc_roster_qa_finish(false,"Bounded roster cycle timed out.");return;}
    if(destination_loading||!terrain.view_ready(cam.x,cam.z,false))return;
    if(orc_roster_qa_index>=arena_orc_roster.entries.size()){
        arena_show_setup();
        orc_roster_qa_finish(arena_progress.balance==orc_roster_qa_balance&&arena_progress.wins==orc_roster_qa_wins,"");return;
    }
    const auto &entry=arena_orc_roster.entries[orc_roster_qa_index];
    if(orc_roster_qa_phase==0){
        arena_show_setup();arena_orc_character_draft=int(orc_roster_qa_index);
        arena_opponents_draft=int(world_arena_opponents::Kind::Orc);arena_total_draft=arena_simultaneous_draft=1;
        if(!arena_begin()){orc_roster_qa_finish(false,arena_progress_error);return;}
        third_person=true;orc_roster_qa_frame=frame;orc_roster_qa_phase=1;
    }else if(orc_roster_qa_phase==1&&frame>=orc_roster_qa_frame+5){
        orc_roster_qa_pending=entry.id+"-third";orc_roster_qa_phase=2;
    }else if(orc_roster_qa_phase==2&&orc_roster_qa_pending.empty()){
        third_person=false;orc_roster_qa_frame=frame;orc_roster_qa_phase=3;
    }else if(orc_roster_qa_phase==3&&frame>=orc_roster_qa_frame+4){
        orc_roster_qa_pending=entry.id+"-first";orc_roster_qa_phase=4;
    }else if(orc_roster_qa_phase==4&&orc_roster_qa_pending.empty()){
        const auto bank=world_combat_motion_opponents_stats(arena_orc_motion);
        const auto motion=world_combat_motion_stats(arena_orc_motion);
        const auto anatomy=world_combat_motion_anatomy_stats(arena_orc_motion);
        world_player_profile::State saved;std::string error;
        if(!bank.ready||bank.body_poses!=172||bank.arm_poses||bank.certified_weapon_pairs!=217||
           arena_orc_character_loaded!=entry.id||arena_orc_resident_banks!=1||arena_orc_peak_banks!=1||
           !arena_orc_original_textures_ready||!motion.uploads||!motion.actors||
           std::abs(arena_game.opponent_body_radius()-bank.body_radius_metres)>1.e-6||
           std::abs(arena_game.opponent_body_height()-bank.body_height_metres)>1.e-6||
           std::abs(arena_game.opponent_movement_radius()-(bank.body_radius_metres+.04))>1.e-6||
           std::abs(arena_game.opponent_pair_clearance()-std::max(.88,2*(bank.body_radius_metres+.04)))>1.e-6||
           arena_progress.balance!=orc_roster_qa_balance||arena_progress.wins!=orc_roster_qa_wins||
           world_player_profile::load(player_profile_path,saved,error)!=world_arena_progress::Load::Loaded||
           saved.arena.balance!=orc_roster_qa_balance||saved.arena.wins!=orc_roster_qa_wins){
            orc_roster_qa_finish(false,"Selected character, measured profile, native draw, memory boundary or purse verification failed.");return;
        }
        std::ostringstream row;row<<std::setprecision(12)<<"{\"id\":"<<world_json::quote(entry.id)
            <<",\"body_poses\":"<<bank.body_poses<<",\"rigid_pairs\":"<<bank.certified_weapon_pairs
            <<",\"body_radius_m\":"<<bank.body_radius_metres<<",\"body_height_m\":"<<bank.body_height_metres
            <<",\"movement_radius_m\":"<<arena_game.opponent_movement_radius()
            <<",\"pair_clearance_m\":"<<arena_game.opponent_pair_clearance()
            <<",\"stride_m\":"<<bank.walk_stride_metres<<",\"pose_uploads\":"<<motion.uploads
            <<",\"active_actors\":"<<motion.actors<<",\"clipped_body_draws\":"<<anatomy.body_draws
            <<",\"resident_orc_banks\":"<<arena_orc_resident_banks<<",\"original_textures\":true}";
        orc_roster_qa_rows.push_back(row.str());++orc_roster_qa_index;orc_roster_qa_phase=0;
    }
};
auto orc_roster_qa_capture=[&](int width,int height){
    if(!orc_roster_qa||orc_roster_qa_pending.empty()||orc_roster_qa_done)return;
    const auto path=std::filesystem::path(orc_roster_qa_directory)/(orc_roster_qa_pending+".bmp");
    std::vector<unsigned char> pixels(std::size_t(width)*height*3);
    GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    const std::size_t stride=std::size_t(width)*3;
    for(int row=0;row<height/2;++row)std::swap_ranges(pixels.data()+row*stride,pixels.data()+(row+1)*stride,pixels.data()+(height-1-row)*stride);
    auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,int(stride),SDL_PIXELFORMAT_RGB24);
    const bool okay=surface&&SDL_SaveBMP(surface,path.c_str())==0;SDL_FreeSurface(surface);
    const auto error=glGetError();
    if(!okay||error!=GL_NO_ERROR){orc_roster_qa_finish(false,"Native roster capture or OpenGL failed.");return;}
    ++orc_roster_qa_captures;orc_roster_qa_pending.clear();
};
