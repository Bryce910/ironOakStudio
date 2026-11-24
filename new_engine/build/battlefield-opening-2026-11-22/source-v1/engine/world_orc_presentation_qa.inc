// Private, bounded material survey. These are deliberately posed source-guard
// views, not evidence of ordinary AI, attack timing, contact or combat outcome.
// The enclosing roster harness verifies the private project marker and requires
// a frame-limited, non-benchmark native launch before this branch is reachable.
const bool orc_presentation_qa=arena_roster_qa&&std::getenv("VALWOOD_QA_ORC_PRESENTATION");
unsigned orc_presentation_phase=0,orc_presentation_index=0,orc_presentation_view=0;
unsigned orc_presentation_first=0,orc_presentation_last=0,orc_presentation_captures=0;
std::uint64_t orc_presentation_start=0;
bool orc_presentation_initialized=false;
std::vector<unsigned> orc_presentation_selected;
std::vector<std::string> orc_presentation_rows,orc_presentation_views;
const std::array<const char*,4> orc_presentation_view_names={"fullbody-front","face-front","face-three-quarter","fullbody-back"};
auto orc_presentation_finish=[&](bool passed,const std::string &error){
 roster_qa_done=true;
 std::error_code ec;std::filesystem::create_directories(arena_roster_qa_dir,ec);
 std::ofstream out(std::filesystem::path(arena_roster_qa_dir)/"ORC_PRESENTATION_RAW.json");
 out<<"{\"schema\":\"valwood.orc-presentation-survey.v1\",\"passed\":"<<(passed?"true":"false")
    <<",\"error\":"<<world_json::quote(error)<<",\"private_profile\":true,\"ordinary_start_and_loading\":true"
    <<",\"capture_posed_not_combatproof\":true,\"source_guard_pose_fixture\":true,\"events_injected\":false"
    <<",\"health_sever_reward_injected\":false,\"first_person_player_hidden_by_flight_camera\":true"
    <<",\"first_catalog_index\":"<<orc_presentation_first<<",\"last_catalog_index_exclusive\":"<<orc_presentation_last
    <<",\"catalog_characters\":"<<arena_orc_roster.entries.size()<<",\"expected_characters\":"<<orc_presentation_selected.size()
    <<",\"expected_captures\":"<<orc_presentation_selected.size()*4<<",\"characters\":"<<orc_presentation_rows.size()
    <<",\"captures\":"<<orc_presentation_captures<<",\"requested_texture_quality\":"<<unsigned(resident_character_textures)
    <<",\"selected_ids\":[";
 for(unsigned i=0;i<orc_presentation_selected.size();++i)out<<(i?",":"")<<world_json::quote(arena_orc_roster.entries[orc_presentation_selected[i]].id);
 out<<"],\"rows\":[";for(unsigned i=0;i<orc_presentation_rows.size();++i)out<<(i?",":"")<<orc_presentation_rows[i];
 out<<"],\"views\":[";for(unsigned i=0;i<orc_presentation_views.size();++i)out<<(i?",":"")<<orc_presentation_views[i];out<<"]}\n";
 if(!out||!passed)launch->exit_code=1;running=false;
 std::fprintf(stderr,"[Orc presentation QA] %s %zu characters / %u captures: %s\n",passed?"PASS":"FAIL",orc_presentation_rows.size(),orc_presentation_captures,error.c_str());
};
auto orc_presentation_camera=[&](){
 const auto stats=world_combat_motion_opponents_stats(arena_opponent_motion(0));
 const double h=stats.body_height_metres;
 const bool face=orc_presentation_view==1||orc_presentation_view==2;
 const double target_y=arena_floor+h*(face?.88:.50),target_z=arena_origin_z+1.25;
 double x=arena_origin_x,z=target_z-3.6;
 if(orc_presentation_view==1)z=target_z-1.3;
 if(orc_presentation_view==2){x+=1.5*.7071067811865476;z=target_z-1.5*.7071067811865476;}
 if(orc_presentation_view==3)z=target_z+3.3;
 cam.x=x;cam.y=target_y;cam.z=z;cam.yaw=float(std::atan2(arena_origin_x-x,z-target_z));cam.pitch=0;
 cam.fov=1.08f;cam.fly=true;cam.grounded=false;cam.swimming=false;cam.vy=0;third_person=false;
};
auto orc_presentation_step=[&](std::uint64_t frame){
 if(roster_qa_done)return;
 if(frame+20>=std::uint64_t(launch->frames)){orc_presentation_finish(false,"Bounded posed Orc material survey timed out");return;}
 if(!orc_presentation_initialized){
  orc_presentation_initialized=true;
  std::error_code ec;std::filesystem::create_directories(arena_roster_qa_dir,ec);
  if(ec){orc_presentation_finish(false,ec.message());return;}
  if(!approach_guard_qa||!arena_orc_catalog_ready||arena_orc_roster.entries.size()!=14||!arena_player_roster_ready||arena_player_roster.index("knight04")<0){
   orc_presentation_finish(false,"Posed survey needs ARENA_APPROACH, the complete fourteen-Orc catalog and knight04");return;
  }
  orc_presentation_last=unsigned(arena_orc_roster.entries.size());
  auto range=[&](const char *name,unsigned &value){const char *text=std::getenv(name);if(!text)return true;const std::string input=text;
   if(input.empty()||input.size()>2||!std::all_of(input.begin(),input.end(),[](unsigned char c){return c>='0'&&c<='9';}))return false;
   unsigned parsed=0;for(unsigned char c:input)parsed=parsed*10+unsigned(c-'0');value=parsed;return true;
  };
  if(!range("VALWOOD_QA_ORC_FIRST",orc_presentation_first)||!range("VALWOOD_QA_ORC_LAST",orc_presentation_last)||
     orc_presentation_first>=orc_presentation_last||orc_presentation_last>arena_orc_roster.entries.size()){
   orc_presentation_finish(false,"Invalid Orc survey range; require 0 <= FIRST < LAST <= 14, LAST exclusive");return;
  }
  const char *target=std::getenv("VALWOOD_QA_APPROACH_ORC_ID");
  for(unsigned i=orc_presentation_first;i<orc_presentation_last;++i)if(!target||arena_orc_roster.entries[i].id==target)orc_presentation_selected.push_back(i);
  if(orc_presentation_selected.empty()){orc_presentation_finish(false,"Requested Orc ID is missing or outside the survey range");return;}
 }
 if(destination_loading||!terrain.view_ready(cam.x,cam.z,cam.fly))return;
 const unsigned catalog_index=orc_presentation_selected[orc_presentation_index];
 const auto &entry=arena_orc_roster.entries[catalog_index];
 if(orc_presentation_phase==0){
  arena_show_setup();arena_player_character_draft=arena_player_roster.index("knight04");
  arena_total_draft=arena_simultaneous_draft=1;arena_teammates_draft=0;arena_difficulty_draft=2;arena_weapon_draft=0;arena_opponents_draft=1;
  orc_presentation_start=frame;orc_presentation_phase=1;return;
 }
 if(orc_presentation_phase==1){
  if(frame<orc_presentation_start+3)return;
  // Reuse the existing private approach selector at the ordinary loading
  // boundary. Restore the caller's environment immediately after selection.
  const char *previous=std::getenv("VALWOOD_QA_APPROACH_ORC_ID");const bool had_previous=previous!=nullptr;const std::string saved=previous?previous:"";
#ifdef _WIN32
  const bool configured=_putenv_s("VALWOOD_QA_APPROACH_ORC_ID",entry.id.c_str())==0;
#else
  const bool configured=setenv("VALWOOD_QA_APPROACH_ORC_ID",entry.id.c_str(),1)==0;
#endif
  const bool started=configured&&arena_begin();
#ifdef _WIN32
  const bool restored=_putenv_s("VALWOOD_QA_APPROACH_ORC_ID",had_previous?saved.c_str():"")==0;
#else
  const bool restored=(had_previous?setenv("VALWOOD_QA_APPROACH_ORC_ID",saved.c_str(),1):unsetenv("VALWOOD_QA_APPROACH_ORC_ID"))==0;
#endif
  if(!configured||!restored||!started){orc_presentation_finish(false,arena_progress_error.empty()?"Private Orc selection/start failed":arena_progress_error);return;}
  arena_gameplay_ready=true;
  for(unsigned n=0;n<900&&(fight.state!=TrainingState::Fighting||arena_game.active_count()!=1);++n)arena_game.update(1./60.,fight,{0,0,0},0,false);
  if(fight.state!=TrainingState::Fighting||arena_game.active_count()!=1||arena_enemy_appearances.size()!=1||arena_enemy_appearances[0]!=entry.id||arena_player_character_loaded!="knight04"){
   orc_presentation_finish(false,"Ordinary countdown or requested source identity did not reach renderer");return;
  }
  orc_presentation_view=0;orc_presentation_camera();temporal_cut=true;orc_presentation_start=frame;orc_presentation_phase=2;
 }
 if(orc_presentation_phase>=2){
  arena_gameplay_ready=true;
  if(arena_enemy_appearances.size()!=1||arena_enemy_appearances[0]!=entry.id||!arena_game.fixture_geometry_pose(0,{0,0,1.25},3.14159265359f,TrainingDirection::Low,fight.attack_start_serial)){
   orc_presentation_finish(false,"Source Orc identity or posed guard fixture failed");return;
  }
  const auto *enemy=arena_game.opponent(0);
  if(!enemy||enemy->health!=enemy->max_health||fight.player_swings||fight.enemy_swings){orc_presentation_finish(false,"Material survey unexpectedly entered real combat");return;}
  orc_presentation_camera();
 }
 if(!roster_qa_pending.empty())return;
 if(orc_presentation_phase==2&&frame>=orc_presentation_start+(orc_presentation_view?12u:36u)){
  roster_qa_pending="orc-"+entry.id+"-"+orc_presentation_view_names[orc_presentation_view];orc_presentation_phase=3;return;
 }
 if(orc_presentation_phase==3&&frame>=orc_presentation_start+3){
  if(++orc_presentation_view<4){orc_presentation_camera();temporal_cut=true;orc_presentation_start=frame;orc_presentation_phase=2;return;}
  auto *motion=arena_opponent_motion(0);const auto compact=world_combat_motion_compact_stats(motion);const auto pose=world_combat_motion_pose(motion,1);
  if(!compact.loaded||compact.failures||compact.actor_mask!=2||compact.resident_body_assets!=1||glGetError()!=GL_NO_ERROR){orc_presentation_finish(false,"Posed source Orc compact rendering failed");return;}
  std::uint64_t buffers=0,peak_buffers=0,textures=0,peak_textures=0;bool memory_query=false;
#ifndef _WIN32
  using OrcMemoryQuery=int(*)(std::uint64_t*,std::uint64_t*,std::uint64_t*,std::uint64_t*);
  const auto query=reinterpret_cast<OrcMemoryQuery>(dlsym(RTLD_DEFAULT,"valwood_qa_gl_memory"));
  memory_query=query&&query(&buffers,&peak_buffers,&textures,&peak_textures);
#endif
  std::ostringstream row;row<<"{\"id\":"<<world_json::quote(entry.id)<<",\"catalog_index\":"<<catalog_index
   <<",\"source_body\":"<<world_json::quote("arena_orcs_roster/"+entry.body_directory+"/combat_body_000.glb")
   <<",\"source_packet\":"<<world_json::quote("compact_orcs/"+entry.id+"/poses.pnt")
   <<",\"captures\":4,\"source_guard\":\"low\",\"capture_posed_not_combatproof\":true,\"health_unchanged\":true,\"GL_errors\":0"
   <<",\"compact_loaded\":true,\"compact_failures\":"<<compact.failures<<",\"actor_mask\":"<<compact.actor_mask
   <<",\"resident_body_assets\":"<<compact.resident_body_assets<<",\"packet_poses\":"<<compact.packet_poses<<",\"logical_poses\":"<<compact.logical_poses
   <<",\"gpu_bytes\":"<<compact.gpu_bytes<<",\"uploads\":"<<compact.uploads<<",\"source_slot\":"<<pose.slot<<",\"guard_weight\":"<<pose.guard_weight
   <<",\"memory_query_available\":"<<(memory_query?"true":"false")<<",\"GL_live_buffers\":"<<buffers<<",\"GL_peak_buffers\":"<<peak_buffers
   <<",\"GL_live_textures\":"<<textures<<",\"GL_peak_textures\":"<<peak_textures<<"}";
  orc_presentation_rows.push_back(row.str());
  std::fprintf(stderr,"[Orc presentation QA] Completed %s four posed source views\n",entry.id.c_str());
  arena_show_setup();
  if(++orc_presentation_index>=orc_presentation_selected.size()){
   const bool complete=orc_presentation_captures==orc_presentation_selected.size()*4&&orc_presentation_rows.size()==orc_presentation_selected.size();
   orc_presentation_finish(complete,complete?"":"Posed survey omitted a character or view");return;
  }
  orc_presentation_phase=0;
 }
};
auto orc_presentation_capture=[&](int width,int height){
 if(roster_qa_done||roster_qa_pending.empty())return;
 if(width<=0||height<=0){orc_presentation_finish(false,"Invalid native framebuffer size");return;}
 std::vector<unsigned char> pixels(std::size_t(width)*height*3);GLint alignment=4;
 glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
 glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
 const std::size_t stride=std::size_t(width)*3;for(int y=0;y<height/2;++y)std::swap_ranges(pixels.data()+y*stride,pixels.data()+(y+1)*stride,pixels.data()+(height-1-y)*stride);
 auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,int(stride),SDL_PIXELFORMAT_RGB24);
 const auto path=std::filesystem::path(arena_roster_qa_dir)/(roster_qa_pending+".bmp");
 const bool ok=surface&&SDL_SaveBMP(surface,path.c_str())==0;if(surface)SDL_FreeSurface(surface);
 if(!ok||glGetError()!=GL_NO_ERROR){orc_presentation_finish(false,"Actual posed Orc framebuffer capture failed");return;}
 const auto &entry=arena_orc_roster.entries[orc_presentation_selected[orc_presentation_index]];
 const auto pose=world_combat_motion_pose(arena_opponent_motion(0),1);const auto compact=world_combat_motion_compact_stats(arena_opponent_motion(0));
 std::ostringstream row;row<<"{\"id\":"<<world_json::quote(entry.id)<<",\"view\":"<<world_json::quote(orc_presentation_view_names[orc_presentation_view])
  <<",\"file\":"<<world_json::quote(path.filename().string())<<",\"width\":"<<width<<",\"height\":"<<height
  <<",\"capture_posed_not_combatproof\":true,\"source_slot\":"<<pose.slot<<",\"source_blend\":"<<pose.blend
  <<",\"guard_direction\":"<<pose.guard_direction<<",\"guard_weight\":"<<pose.guard_weight<<",\"compact_failures\":"<<compact.failures
  <<",\"camera\":["<<std::setprecision(12)<<cam.x<<","<<cam.y<<","<<cam.z<<","<<cam.yaw<<","<<cam.pitch<<","<<cam.fov<<"],\"GL_errors\":0}";
 orc_presentation_views.push_back(row.str());++orc_presentation_captures;++roster_qa_captures;roster_qa_pending.clear();
};
