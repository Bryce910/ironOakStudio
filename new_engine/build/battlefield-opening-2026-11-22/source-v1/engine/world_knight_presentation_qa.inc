// Bounded, privately marked exposed-face material survey. These are posed
// source-guard views and make no claim about AI, attacks or combat animation.
const bool knight_presentation_qa=arena_roster_qa&&std::getenv("VALWOOD_QA_KNIGHT_PRESENTATION");
const std::array<const char*,3> knight_presentation_ids={"knight11","knight13","knight17"};
const std::array<const char*,2> knight_presentation_view_names={"face-front","face-three-quarter"};
unsigned knight_presentation_index=0,knight_presentation_view=0,knight_presentation_phase=0,knight_presentation_captures=0;
std::uint64_t knight_presentation_start=0;
std::vector<std::string> knight_presentation_rows,knight_presentation_views;
auto knight_presentation_finish=[&](bool passed,const std::string &error){
 roster_qa_done=true;
 std::error_code ec;std::filesystem::create_directories(arena_roster_qa_dir,ec);
 std::ofstream out(std::filesystem::path(arena_roster_qa_dir)/"KNIGHT_PRESENTATION_RAW.json");
 out<<"{\"schema\":\"valwood.knight-presentation-survey.v1\",\"passed\":"<<(passed?"true":"false")
    <<",\"error\":"<<world_json::quote(error)<<",\"private_profile\":true,\"ordinary_start_and_loading\":true"
    <<",\"capture_posed_not_combatproof\":true,\"source_guard_pose_fixture\":true,\"events_injected\":false"
    <<",\"health_sever_reward_injected\":false,\"first_person_player_hidden_by_flight_camera\":true"
    <<",\"catalog_characters\":"<<arena_player_roster.entries.size()<<",\"expected_characters\":3,\"expected_captures\":6"
    <<",\"characters\":"<<knight_presentation_rows.size()<<",\"captures\":"<<knight_presentation_captures
    <<",\"requested_texture_quality\":"<<unsigned(resident_character_textures)<<",\"selected_ids\":[";
 for(unsigned i=0;i<knight_presentation_ids.size();++i)out<<(i?",":"")<<world_json::quote(knight_presentation_ids[i]);
 out<<"],\"rows\":[";for(unsigned i=0;i<knight_presentation_rows.size();++i)out<<(i?",":"")<<knight_presentation_rows[i];
 out<<"],\"views\":[";for(unsigned i=0;i<knight_presentation_views.size();++i)out<<(i?",":"")<<knight_presentation_views[i];out<<"]}\n";
 if(!out||!passed)launch->exit_code=1;running=false;
 std::fprintf(stderr,"[Knight presentation QA] %s %zu characters / %u captures: %s\n",passed?"PASS":"FAIL",knight_presentation_rows.size(),knight_presentation_captures,error.c_str());
};
auto knight_presentation_camera=[&](){
 const auto stats=world_combat_motion_opponents_stats(arena_opponent_motion(0));
 const double target_y=arena_floor+stats.body_height_metres*.88,target_z=arena_origin_z+1.25;
 double x=arena_origin_x,z=target_z-1.3;
 if(knight_presentation_view==1){x+=1.5*.7071067811865476;z=target_z-1.5*.7071067811865476;}
 cam.x=x;cam.y=target_y;cam.z=z;cam.yaw=float(std::atan2(arena_origin_x-x,z-target_z));cam.pitch=0;
 cam.fov=1.08f;cam.fly=true;cam.grounded=false;cam.swimming=false;cam.vy=0;third_person=false;
};
auto knight_presentation_step=[&](std::uint64_t frame){
 if(roster_qa_done)return;
 if(frame+20>=std::uint64_t(launch->frames)){knight_presentation_finish(false,"Bounded posed knight face survey timed out");return;}
 if(destination_loading||!terrain.view_ready(cam.x,cam.z,cam.fly))return;
 const std::string id=knight_presentation_ids[knight_presentation_index];
 if(knight_presentation_phase==0){
  if(!arena_player_roster_ready||arena_player_roster.entries.size()!=18||arena_player_roster.index(id)<0||arena_player_roster.index("knight04")<0){
   knight_presentation_finish(false,"Exposed-face survey requires the complete eighteen-knight catalog");return;
  }
  arena_show_setup();arena_player_character_draft=arena_player_roster.index("knight04");
  arena_total_draft=arena_simultaneous_draft=1;arena_teammates_draft=0;arena_difficulty_draft=2;arena_weapon_draft=0;arena_opponents_draft=0;
  knight_presentation_start=frame;knight_presentation_phase=1;return;
 }
 if(knight_presentation_phase==1){
  if(frame<knight_presentation_start+3)return;
  // The selector below changes appearance only at the ordinary loading
  // boundary. Restore the caller's environment before any capture or update.
  const char *previous=std::getenv("VALWOOD_QA_PRESENTATION_KNIGHT_ID");const bool had_previous=previous!=nullptr;const std::string saved=previous?previous:"";
#ifdef _WIN32
  const bool configured=_putenv_s("VALWOOD_QA_PRESENTATION_KNIGHT_ID",id.c_str())==0;
#else
  const bool configured=setenv("VALWOOD_QA_PRESENTATION_KNIGHT_ID",id.c_str(),1)==0;
#endif
  const bool started=configured&&arena_begin();
#ifdef _WIN32
  const bool restored=_putenv_s("VALWOOD_QA_PRESENTATION_KNIGHT_ID",had_previous?saved.c_str():"")==0;
#else
  const bool restored=(had_previous?setenv("VALWOOD_QA_PRESENTATION_KNIGHT_ID",saved.c_str(),1):unsetenv("VALWOOD_QA_PRESENTATION_KNIGHT_ID"))==0;
#endif
  if(!configured||!restored||!started){knight_presentation_finish(false,arena_progress_error.empty()?"Private knight selection/start failed":arena_progress_error);return;}
  arena_gameplay_ready=true;
  for(unsigned n=0;n<900&&(fight.state!=TrainingState::Fighting||arena_game.active_count()!=1);++n)arena_game.update(1./60.,fight,{0,0,0},0,false);
  if(fight.state!=TrainingState::Fighting||arena_game.active_count()!=1||arena_enemy_appearances.size()!=1||arena_enemy_appearances[0]!=id||arena_player_character_loaded!="knight04"){
   knight_presentation_finish(false,"Ordinary countdown or requested knight source identity did not reach renderer");return;
  }
  knight_presentation_view=0;knight_presentation_camera();temporal_cut=true;knight_presentation_start=frame;knight_presentation_phase=2;
 }
 if(knight_presentation_phase>=2){
  arena_gameplay_ready=true;
  if(arena_enemy_appearances.size()!=1||arena_enemy_appearances[0]!=id||!arena_game.fixture_geometry_pose(0,{0,0,1.25},3.14159265359f,TrainingDirection::Low,fight.attack_start_serial)){
   knight_presentation_finish(false,"Knight identity or posed source guard fixture failed");return;
  }
  const auto *enemy=arena_game.opponent(0);
  if(!enemy||enemy->health!=enemy->max_health||fight.player_swings||fight.enemy_swings){knight_presentation_finish(false,"Material face survey unexpectedly entered combat");return;}
  knight_presentation_camera();
 }
 if(!roster_qa_pending.empty())return;
 if(knight_presentation_phase==2&&frame>=knight_presentation_start+(knight_presentation_view?12u:36u)){
  roster_qa_pending="knight-"+id+"-"+knight_presentation_view_names[knight_presentation_view];knight_presentation_phase=3;return;
 }
 if(knight_presentation_phase==3&&frame>=knight_presentation_start+3){
  if(++knight_presentation_view<2){knight_presentation_camera();temporal_cut=true;knight_presentation_start=frame;knight_presentation_phase=2;return;}
  auto *motion=arena_opponent_motion(0);const auto compact=world_combat_motion_compact_stats(motion);const auto pose=world_combat_motion_pose(motion,1);
  if(!compact.loaded||compact.failures||compact.actor_mask!=2||compact.resident_body_assets!=1||glGetError()!=GL_NO_ERROR){knight_presentation_finish(false,"Posed source knight compact rendering failed");return;}
  std::ostringstream row;row<<"{\"id\":"<<world_json::quote(id)
   <<",\"source_body\":"<<world_json::quote("npc_knights/"+id+"/body/combat_body_000.glb")
   <<",\"source_packet\":"<<world_json::quote("npc_knights/"+id+"/poses.pnt")
   <<",\"captures\":2,\"source_guard\":\"low\",\"capture_posed_not_combatproof\":true,\"health_unchanged\":true,\"GL_errors\":0"
   <<",\"compact_loaded\":true,\"compact_failures\":"<<compact.failures<<",\"actor_mask\":"<<compact.actor_mask
   <<",\"resident_body_assets\":"<<compact.resident_body_assets<<",\"packet_poses\":"<<compact.packet_poses<<",\"logical_poses\":"<<compact.logical_poses
   <<",\"gpu_bytes\":"<<compact.gpu_bytes<<",\"uploads\":"<<compact.uploads<<",\"source_slot\":"<<pose.slot<<",\"guard_weight\":"<<pose.guard_weight<<"}";
  knight_presentation_rows.push_back(row.str());std::fprintf(stderr,"[Knight presentation QA] Completed %s two posed source face views\n",id.c_str());
  arena_show_setup();
  if(++knight_presentation_index>=knight_presentation_ids.size()){
   const bool complete=knight_presentation_captures==6&&knight_presentation_rows.size()==3;
   knight_presentation_finish(complete,complete?"":"Posed knight survey omitted a face or view");return;
  }
  knight_presentation_phase=0;
 }
};
auto knight_presentation_capture=[&](int width,int height){
 if(roster_qa_done||roster_qa_pending.empty())return;
 if(width<=0||height<=0){knight_presentation_finish(false,"Invalid native framebuffer size");return;}
 std::vector<unsigned char> pixels(std::size_t(width)*height*3);GLint alignment=4;
 glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
 glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
 const std::size_t stride=std::size_t(width)*3;for(int y=0;y<height/2;++y)std::swap_ranges(pixels.data()+y*stride,pixels.data()+(y+1)*stride,pixels.data()+(height-1-y)*stride);
 auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,int(stride),SDL_PIXELFORMAT_RGB24);
 const auto path=std::filesystem::path(arena_roster_qa_dir)/(roster_qa_pending+".bmp");
 const bool ok=surface&&SDL_SaveBMP(surface,path.c_str())==0;if(surface)SDL_FreeSurface(surface);
 if(!ok||glGetError()!=GL_NO_ERROR){knight_presentation_finish(false,"Actual posed knight framebuffer capture failed");return;}
 const auto pose=world_combat_motion_pose(arena_opponent_motion(0),1);const auto compact=world_combat_motion_compact_stats(arena_opponent_motion(0));
 std::ostringstream row;row<<"{\"id\":"<<world_json::quote(knight_presentation_ids[knight_presentation_index])<<",\"view\":"<<world_json::quote(knight_presentation_view_names[knight_presentation_view])
  <<",\"file\":"<<world_json::quote(path.filename().string())<<",\"width\":"<<width<<",\"height\":"<<height
  <<",\"capture_posed_not_combatproof\":true,\"source_slot\":"<<pose.slot<<",\"source_blend\":"<<pose.blend
  <<",\"guard_direction\":"<<pose.guard_direction<<",\"guard_weight\":"<<pose.guard_weight<<",\"compact_failures\":"<<compact.failures
  <<",\"camera\":["<<std::setprecision(12)<<cam.x<<","<<cam.y<<","<<cam.z<<","<<cam.yaw<<","<<cam.pitch<<","<<cam.fov<<"],\"GL_errors\":0}";
 knight_presentation_views.push_back(row.str());++knight_presentation_captures;++roster_qa_captures;roster_qa_pending.clear();
};
