// Private, bounded production-path roster rehearsal. Only scene placement and
// countdown fast-forward are fixtures; selection/loading/events stay ordinary.
const char *arena_roster_qa_dir=std::getenv("VALWOOD_QA_ARENA_ROSTER_DIR");
if(arena_roster_qa){std::ifstream marker(".valwood-arena-roster-qa");std::string token;std::getline(marker,token);
 if(token!="VALWOOD_PRIVATE_ARENA_ROSTER_V1"||!arena_roster_qa_dir||!*arena_roster_qa_dir){std::fprintf(stderr,"[Arena roster QA] Missing private marker/output\n");return 1;}}
unsigned roster_qa_match=0,roster_qa_phase=0,roster_qa_captures=0;
std::uint64_t roster_qa_start=0,roster_qa_attack=0;
bool roster_qa_done=false;std::string roster_qa_pending;
std::vector<std::string> roster_qa_locked,roster_qa_rows;
auto roster_qa_finish=[&](bool passed,const std::string &error){
 roster_qa_done=true;std::ofstream out(std::filesystem::path(arena_roster_qa_dir)/"NATIVE_ROSTER_RAW.json");
 out<<"{\"passed\":"<<(passed?"true":"false")<<",\"error\":"<<world_json::quote(error)
    <<",\"matches\":"<<roster_qa_rows.size()<<",\"captures\":"<<roster_qa_captures
    <<",\"ordinary_start_and_loading\":true,\"events_injected\":false,\"private_profile\":true,\"rows\":[";
 for(unsigned i=0;i<roster_qa_rows.size();++i)out<<(i?",":"")<<roster_qa_rows[i];out<<"]}\n";
 if(!out||!passed)launch->exit_code=1;running=false;
 std::fprintf(stderr,"[Arena roster QA] %s %zu matches / %u captures: %s\n",passed?"PASS":"FAIL",roster_qa_rows.size(),roster_qa_captures,error.c_str());
};
#include "world_arena_approach_qa.inc"
#include "world_orc_presentation_qa.inc"
#include "world_knight_presentation_qa.inc"
auto roster_qa_step=[&](std::uint64_t frame){
 if(knight_presentation_qa){knight_presentation_step(frame);return;}
 if(orc_presentation_qa){orc_presentation_step(frame);return;}
 if(approach_guard_qa){approach_guard_qa_step(frame);return;}
 if(arena_capacity_qa){arena_capacity_qa_step(frame);return;}
 if(!arena_roster_qa||roster_qa_done)return;
 if(frame+20>=std::uint64_t(launch->frames)){roster_qa_finish(false,"Bounded roster rehearsal timed out");return;}
 if(destination_loading||!terrain.view_ready(cam.x,cam.z,false))return;
 auto name=[&](const char *phase){return std::string(roster_qa_match?"orcs-":"knights-")+phase;};
 if(roster_qa_phase==0){
  std::error_code ec;std::filesystem::create_directories(arena_roster_qa_dir,ec);if(ec){roster_qa_finish(false,ec.message());return;}
  if(!arena_player_roster_ready||arena_player_roster.entries.size()!=18){roster_qa_finish(false,"Actual eighteen-knight picker missing");return;}
  arena_show_setup();arena_player_character_draft=arena_player_roster.index(roster_qa_match?"knight18":"knight01");
  arena_total_draft=8;arena_simultaneous_draft=2;arena_weapon_draft=0;arena_opponents_draft=roster_qa_match?1:0;
  roster_qa_pending=name("setup");roster_qa_start=frame;roster_qa_phase=1;return;
 }
 if(roster_qa_phase==1&&frame>=roster_qa_start+3&&roster_qa_pending.empty()){
  if(!arena_begin()){roster_qa_finish(false,arena_progress_error);return;}
  cam.x=arena_origin_x;cam.z=arena_origin_z;cam.y=arena_floor+1.75;cam.yaw=3.14159265359f;cam.pitch=-.06f;
  avatar_initialized=false;third_person=true;arena_gameplay_ready=true;
  for(unsigned n=0;n<900&&(fight.state!=TrainingState::Fighting||arena_game.active_count()<2);++n)arena_game.update(1./60.,fight,{0,0,0},0,false);
  if(fight.state!=TrainingState::Fighting||arena_game.active_count()<2){roster_qa_finish(false,"Ordinary countdown/entry failed");return;}
  roster_qa_locked=arena_enemy_appearances;auto distinct=roster_qa_locked;std::sort(distinct.begin(),distinct.end());
  if(distinct.size()!=8||std::adjacent_find(distinct.begin(),distinct.end())!=distinct.end()){
   roster_qa_finish(false,"Opponent deck duplicated or omitted an appearance");return;
  }
  if(arena_player_character_loaded!=(roster_qa_match?"knight18":"knight01")){roster_qa_finish(false,"Picker identity did not reach actual Hero");return;}
  roster_qa_start=frame;roster_qa_phase=2;roster_qa_attack=0;
 }
 if(roster_qa_phase>=2){
  arena_gameplay_ready=true;
  if(arena_enemy_appearances!=roster_qa_locked){roster_qa_finish(false,"An opponent changed appearance during a match");return;}
  if(const auto *enemy=arena_game.opponent(0);enemy&&enemy->health>0)
   arena_game.fixture_geometry_pose(0,{0,0,1.25},3.14159265359f,TrainingDirection::High,fight.attack_start_serial);
  if(const auto *enemy=arena_game.opponent(1);enemy&&enemy->health>0)
   arena_game.fixture_geometry_pose(1,{3,0,1.25},3.14159265359f,TrainingDirection::High,fight.attack_start_serial);
 }
 if(!roster_qa_pending.empty())return;
 if(roster_qa_phase==2&&frame>=roster_qa_start+24){
  roster_qa_pending=name("sword-third");roster_qa_phase=3;roster_qa_start=frame;
 }else if(roster_qa_phase==3&&frame>=roster_qa_start+5){
  arena_weapon(world_weapon_loadout::Weapon::SwordShield);third_person=false;roster_qa_phase=4;roster_qa_start=frame;
 }else if(roster_qa_phase==4&&frame>=roster_qa_start+12){
  roster_qa_pending=name("shield-first");fight.choose(TrainingDirection::Left);
  if(!skeletal_timed_attack()){roster_qa_finish(false,"Ordinary attack input rejected");return;}
  roster_qa_attack=fight.attack_start_serial;roster_qa_phase=5;roster_qa_start=frame;
 }else if(roster_qa_phase==5){
  if(fight.resolved_attack_serial==roster_qa_attack&&fight.resolved_attack_outcome!=TrainingAttackOutcome::None){
   roster_qa_pending=name("contact-first");roster_qa_phase=6;roster_qa_start=frame;
  }else if(frame>roster_qa_start+150){roster_qa_finish(false,"Actual attack never resolved");return;}
 }else if(roster_qa_phase==6&&frame>=roster_qa_start+55&&fight.player_phase==TrainingPlayerPhase::Ready){
  arena_weapon(world_weapon_loadout::Weapon::Bow);third_person=true;roster_qa_phase=7;roster_qa_start=frame;
 }else if(roster_qa_phase==7&&frame>=roster_qa_start+14){
  if(!world_combat_motion_unarmed_ready(arena_player_bank)){roster_qa_finish(false,"Selected appearance lacks actual unarmed poses");return;}
  roster_qa_pending=name("bow-third");third_person=true;roster_qa_phase=8;roster_qa_start=frame;
 }else if(roster_qa_phase==8&&frame>=roster_qa_start+5){
  if(!arena_game.begin_draw(fight)){roster_qa_finish(false,"Actual bow draw rejected");return;}
  roster_qa_phase=9;roster_qa_start=frame;
 }else if(roster_qa_phase==9&&frame>=roster_qa_start+65){
  if(arena_game.draw_fraction()<.99){roster_qa_finish(false,"Actual bow draw did not advance");return;}
  roster_qa_pending=name("bow-drawn-third");roster_qa_phase=81;roster_qa_start=frame;
 }else if(roster_qa_phase==81&&frame>=roster_qa_start+5){
  third_person=false;roster_qa_phase=91;roster_qa_start=frame;
 }else if(roster_qa_phase==91&&frame>=roster_qa_start+10){
  roster_qa_pending=name("bow-first");roster_qa_phase=10;roster_qa_start=frame;
 }else if(roster_qa_phase==10&&frame>=roster_qa_start+5){
  arena_game.cancel_draw();weapon_sheathed=true;third_person=true;roster_qa_phase=11;roster_qa_start=frame;
 }else if(roster_qa_phase==11&&frame>=roster_qa_start+20){
  roster_qa_pending=name("sheathed-third");roster_qa_phase=12;roster_qa_start=frame;
 }else if(roster_qa_phase==12&&frame>=roster_qa_start+3){
  third_person=false;roster_qa_phase=13;roster_qa_start=frame;
 }else if(roster_qa_phase==13&&frame>=roster_qa_start+10){
  roster_qa_pending=name("sheathed-first");roster_qa_phase=14;roster_qa_start=frame;
 }else if(roster_qa_phase==14&&frame>=roster_qa_start+3){
  std::ostringstream row;row<<"{\"kind\":"<<world_json::quote(roster_qa_match?"Orcs":"Knights")
   <<",\"player\":"<<world_json::quote(arena_player_character_loaded)<<",\"opponents\":[";
  for(unsigned i=0;i<roster_qa_locked.size();++i)row<<(i?",":"")<<world_json::quote(roster_qa_locked[i]);
  row<<"],\"banks\":[";bool first=true;
  for(const auto &bank:arena_appearance_banks){const auto stats=world_combat_motion_compact_stats(bank.motion.get());
   if(!stats.loaded||stats.failures||stats.resident_body_assets!=1||stats.actor_mask!=bank.mask){roster_qa_finish(false,"Compact actor rendering/ownership failed");return;}
   row<<(first?"":",")<<"{\"id\":"<<world_json::quote(bank.id)<<",\"mask\":"<<bank.mask<<",\"poses\":"<<stats.packet_poses<<",\"anatomical_poses\":"<<stats.logical_poses
      <<",\"gpu_bytes\":"<<stats.gpu_bytes<<",\"uploads\":"<<stats.uploads<<",\"failures\":"<<stats.failures<<"}";first=false;
  }
  std::uint64_t buffers=0,peak_buffers=0,textures=0,peak_textures=0;
#ifndef _WIN32
  using MemoryQuery=int(*)(std::uint64_t*,std::uint64_t*,std::uint64_t*,std::uint64_t*);
  const auto query=reinterpret_cast<MemoryQuery>(dlsym(RTLD_DEFAULT,"valwood_qa_gl_memory"));
  if(!query||!query(&buffers,&peak_buffers,&textures,&peak_textures)){roster_qa_finish(false,"Private actual GL allocation tracker unavailable");return;}
#endif
  row<<"],\"GL_live_buffers\":"<<buffers<<",\"GL_live_textures\":"<<textures<<",\"GL_peak_buffers\":"<<peak_buffers
     <<",\"GL_peak_textures\":"<<peak_textures<<",\"actual_outcome\":"<<unsigned(fight.resolved_attack_outcome)<<",\"actual_serial\":"<<roster_qa_attack<<"}";
  roster_qa_rows.push_back(row.str());std::fprintf(stderr,"[Arena roster QA] Completed %s with8 distinct stable opponents\n",roster_qa_match?"Orcs":"Knights");
  if(++roster_qa_match>=2){arena_show_setup();roster_qa_finish(true,"");return;}
  roster_qa_phase=0;
 }
};
auto roster_qa_capture=[&](int width,int height){
 if(knight_presentation_qa){knight_presentation_capture(width,height);return;}
 if(orc_presentation_qa){orc_presentation_capture(width,height);return;}
 if(arena_capacity_qa){arena_capacity_qa_capture(width,height);return;}
 if(!arena_roster_qa||roster_qa_done||roster_qa_pending.empty())return;
 std::vector<unsigned char> pixels(std::size_t(width)*height*3);GLint alignment=4;
 glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
 glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
 const std::size_t stride=std::size_t(width)*3;for(int y=0;y<height/2;++y)std::swap_ranges(pixels.data()+y*stride,pixels.data()+(y+1)*stride,pixels.data()+(height-1-y)*stride);
 auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,int(stride),SDL_PIXELFORMAT_RGB24);
 const auto path=std::filesystem::path(arena_roster_qa_dir)/(roster_qa_pending+".bmp");
 const bool ok=surface&&SDL_SaveBMP(surface,path.c_str())==0;if(surface)SDL_FreeSurface(surface);
 if(!ok||glGetError()!=GL_NO_ERROR){roster_qa_finish(false,"Actual arena framebuffer capture failed");return;}
 ++roster_qa_captures;roster_qa_pending.clear();
};
