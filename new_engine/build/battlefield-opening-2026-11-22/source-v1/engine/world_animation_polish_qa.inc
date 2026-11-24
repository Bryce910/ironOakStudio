// Private, bounded film observer. Gameplay owns roots, collision, animation and
// contacts. The only injected values are ordinary input intent and scene setup.
const char *animation_polish_dir_env=std::getenv("VALWOOD_QA_ANIMATION_POLISH_DIR");
const char *animation_polish_pipe_env=std::getenv("VALWOOD_QA_ANIMATION_POLISH_PIPE");
std::filesystem::path animation_polish_output=animation_polish_dir_env?animation_polish_dir_env:"";
std::ofstream animation_polish_video,animation_polish_samples;
unsigned animation_polish_view=0,animation_polish_stage=0,animation_polish_stage_age=0;
std::uint64_t animation_polish_movie_frame=0,animation_polish_last_frame=0;
bool animation_polish_started=false,animation_polish_opener_done=false,animation_polish_failed=false;
bool animation_polish_initialized=false;
std::array<unsigned,30> animation_polish_fresh_state={};
std::array<std::uint64_t,30> animation_polish_fresh_serial={},animation_polish_fresh_done={};
float animation_polish_forward=0,animation_polish_side=0,animation_polish_yaw=0;
bool animation_polish_sprint=false;
std::string animation_polish_label="waiting";
struct AnimationPolishStage {const char *label;unsigned frames;float forward,side,yaw;bool sprint;};
const std::array<AnimationPolishStage,23> animation_polish_stages={{{"idle",24,0,0,0,false},
 {"walk-forward",45,1,0,0,false},{"stop",24,0,0,0,false},
 {"strafe-left",30,0,-1,0,false},{"strafe-right",30,0,1,0,false},
 {"reverse",30,-1,0,0,false},{"reverse-stop",24,0,0,0,false},
 {"turn",45,0,0,.9f,false},{"turn-settle",30,0,0,0,false},
 {"sheath",18,0,0,0,false},{"unarmed-run",45,1,0,0,true},{"unarmed-stop",30,0,0,0,false},
 {"draw-sword",30,0,0,0,false},{"guard-left",24,0,0,0,false},{"guard-right",24,0,0,0,false},
 {"guard-high",24,0,0,0,false},{"guard-low",24,0,0,0,false},{"guard-thrust",24,0,0,0,false},
 {"guard-release",30,0,0,0,false},{"bow-idle",90,0,0,0,false},
 {"bow-draw",72,0,0,0,false},{"bow-release",48,0,0,0,false},{"bow-sheath",30,0,0,0,false}}};
auto animation_polish_fail=[&](const std::string &reason){
 if(animation_polish_failed)return;animation_polish_failed=true;launch->exit_code=1;running=false;
 std::fprintf(stderr,"[Animation film] FAIL %s\n",reason.c_str());
 if(!animation_polish_output.empty()){std::ofstream out(animation_polish_output/"ANIMATION_FILM_FAILURE.json");out<<"{\"passed\":false,\"error\":"<<world_json::quote(reason)<<"}\n";}
};
auto animation_polish_prepare=[&](){
 if(!animation_polish_qa||animation_polish_initialized)return !animation_polish_failed;
 std::ifstream marker(".valwood-animation-polish-qa");std::string token;std::getline(marker,token);
 if(token!="VALWOOD_PRIVATE_ANIMATION_POLISH_V1"||animation_polish_output.empty()||!animation_polish_output.is_absolute()||
    !std::filesystem::is_directory(animation_polish_output)||!animation_polish_pipe_env||
    std::filesystem::path(animation_polish_pipe_env).parent_path()!=animation_polish_output||
    !std::filesystem::is_other(animation_polish_pipe_env)){
  animation_polish_fail("Private animation marker, output and runner-owned FIFO required");return false;}
 animation_polish_samples.open(animation_polish_output/"ANIMATION_FRAMES.jsonl");
 animation_polish_video.open(animation_polish_pipe_env,std::ios::binary);
 if(!animation_polish_samples||!animation_polish_video){animation_polish_fail("Frame stream unavailable");return false;}
 animation_polish_samples<<std::setprecision(12);animation_polish_initialized=true;return true;
};
auto animation_polish_qa_controls=[&](float &forward,float &side,bool &sprint){
 if(!animation_polish_qa||!animation_polish_started||animation_polish_opener_done||animation_polish_failed)return;
 forward=animation_polish_forward;side=animation_polish_side;sprint=animation_polish_sprint;
};
auto animation_polish_qa_yaw_rate=[&](){return animation_polish_qa&&!animation_polish_opener_done?animation_polish_yaw:0.f;};
auto animation_polish_qa_step=[&](std::uint64_t frame){
 if(!animation_polish_qa)return true;
 if(!animation_polish_prepare()||animation_polish_failed)return false;
 if(animation_polish_opener_done)return true;
 if(destination_loading||!terrain.view_ready(cam.x,cam.z,false))return false;
 if(frame+40>=std::uint64_t(launch->frames)){animation_polish_fail("Bounded animation budget exhausted");return false;}
 if(!animation_polish_started){
  SDL_setenv("VALWOOD_QA_CLASH_ENEMY","knight01",1);arena_show_setup();
  arena_player_character_draft=arena_player_roster.index(animation_polish_view?"knight18":"knight09");
  arena_total_draft=arena_simultaneous_draft=1;arena_weapon_draft=arena_opponents_draft=0;
  if(!arena_begin()){animation_polish_fail("Ordinary opener start: "+arena_progress_error);return false;}
  cam.x=arena_origin_x;cam.z=arena_origin_z;cam.y=arena_floor+1.75;cam.yaw=3.14159265359f;cam.pitch=-.06f;
  third_person=!animation_polish_view;weapon_sheathed=false;avatar_initialized=false;arena_gameplay_ready=true;
  for(unsigned n=0;n<900&&(fight.state!=TrainingState::Fighting||!arena_game.active_count());++n)arena_game.update(1./60.,fight,{0,0,0},0,false);
  if(fight.state!=TrainingState::Fighting||arena_game.active_count()!=1){animation_polish_fail("Opener did not enter ordinary fight");return false;}
  animation_polish_stage=animation_polish_stage_age=0;animation_polish_started=true;
 }
 arena_gameplay_ready=true;
 if(!arena_game.fixture_geometry_pose(0,{0,0,9},3.14159265359f,TrainingDirection::High,fight.attack_start_serial)){
  animation_polish_fail("Distant nonattacking scene placement rejected");return false;}
 const auto &stage=animation_polish_stages[animation_polish_stage];animation_polish_label=stage.label;
 animation_polish_forward=stage.forward;animation_polish_side=stage.side;animation_polish_yaw=stage.yaw;animation_polish_sprint=stage.sprint;
 if(!animation_polish_stage_age){
  if(animation_polish_stage==9)weapon_sheathed=true;
  if(animation_polish_stage==12)weapon_sheathed=false;
  if(animation_polish_stage>=13&&animation_polish_stage<=17){fight.choose(TrainingDirection(animation_polish_stage-13));fight.set_guard(true);}
  if(animation_polish_stage==18)fight.set_guard(false);
  // The ordinary equipment transaction lowers, commits and raises over
  // several frames. A handled request is not an immediate identity change.
  if(animation_polish_stage==19)arena_weapon(world_arena_combat::Weapon::Bow);
  if(animation_polish_stage==20&&!arena_game.begin_draw(fight)){animation_polish_fail("Ordinary Bow draw rejected");return false;}
  if(animation_polish_stage==21){if(arena_game.draw_fraction()<.99){animation_polish_fail("Bow did not reach full draw");return false;}arena_release_bow();}
  if(animation_polish_stage==22)weapon_sheathed=true;
 }
 ++animation_polish_stage_age;
 if(animation_polish_stage_age>=stage.frames){
  if(animation_polish_stage==19&&(!arena_bow()||equipment_change.active()||weapon_sheathed)){
   animation_polish_fail("Ordinary Bow equip did not finish its bounded transaction");return false;}
  animation_polish_stage_age=0;
  if(++animation_polish_stage>=animation_polish_stages.size()){
   animation_polish_forward=animation_polish_side=animation_polish_yaw=0;animation_polish_sprint=false;
   animation_polish_started=false;arena_show_setup();
   if(++animation_polish_view==2){animation_polish_opener_done=true;animation_polish_label="clash-contact-sequences";}
  }
 }
 return false;
};
if(animation_polish_qa)animation_polish_clash_hold=[&](std::uint64_t frame){
 if(clash_index>=clash_cases.size()||clash_index>=animation_polish_fresh_state.size()){
  animation_polish_fail("Fresh action index outside private case range");return true;}
 if(frame<clash_impact+40)return true;
 const auto &c=clash_cases[clash_index];if(c.mode==0||c.mode==2)return false;
 auto &state=animation_polish_fresh_state[clash_index];
 // Keep a distant scene target while the ordinary player animation/core owns
 // the fresh swing. No prior contact event or damage is injected/replayed.
 clash_distance=6.;
 if(!arena_game.fixture_geometry_pose(0,{0,0,clash_distance},3.14159265359f,TrainingDirection::High,fight.attack_start_serial)){
  animation_polish_fail("Fresh-action distant scene placement rejected");return true;}
 if(!state){
  if(fight.player_phase!=TrainingPlayerPhase::Ready)return true;
  fight.set_guard(false);fight.choose(TrainingDirection::Left);
  if(!skeletal_timed_attack()){animation_polish_fail("Fresh ordinary swing was rejected after steel");return true;}
  animation_polish_fresh_serial[clash_index]=fight.attack_start_serial;state=1;return true;
 }
 if(state==1){
  if(fight.resolved_attack_serial!=animation_polish_fresh_serial[clash_index]||fight.player_phase!=TrainingPlayerPhase::Ready)return true;
  if(fight.resolved_attack_outcome!=TrainingAttackOutcome::Miss){animation_polish_fail("Distant fresh swing did not resolve a genuine miss");return true;}
  animation_polish_fresh_done[clash_index]=frame;state=2;
 }
 return frame<animation_polish_fresh_done[clash_index]+8;
};
auto animation_polish_qa_observe=[&](std::uint64_t frame){
 if(!animation_polish_qa||!animation_polish_initialized||animation_polish_failed||(!animation_polish_started&&!animation_polish_opener_done))return;
 animation_polish_last_frame=frame;
 const auto &s=skeletal_hero.stats();double base[3]={},tip[3]={};const bool blade=skeletal_hero.blade(base,tip);
 WorldCombatBlade npc_blade;const bool npc=arena_game.active_count()&&world_combat_motion_blade(arena_opponent_motion(0),1,npc_blade);
 const auto player_pose=world_combat_motion_pose(arena_player_motion(),0),npc_pose=world_combat_motion_pose(arena_opponent_motion(0),1);
 auto &out=animation_polish_samples;out<<"{\"video_frame\":"<<animation_polish_movie_frame<<",\"game_frame\":"<<frame<<",\"dt\":0.016666666666666666,\"view\":"<<world_json::quote(third_person?"third":"first")
  <<",\"label\":"<<world_json::quote(animation_polish_opener_done&&clash_index<clash_cases.size()?clash_name():animation_polish_label)<<",\"opener\":"<<(animation_polish_opener_done?"false":"true")
  <<",\"equipment_queued\":"<<(equipment_change.active()?"true":"false")<<",\"equipment_target\":"<<equipment_change.target()<<",\"equipment_request\":"<<unsigned(equipment_request_feedback.result)<<",\"equipment_progress\":"<<s.equipment_progress
  <<",\"fresh_state\":"<<(animation_polish_opener_done&&clash_index<30?animation_polish_fresh_state[clash_index]:0)<<",\"weapon\":"<<unsigned(fight.weapon)<<",\"sheath\":"<<(weapon_sheathed?"true":"false")<<",\"root\":["<<cam.x<<','<<cam.y-1.75<<','<<cam.z<<"],\"yaw\":"<<cam.yaw
  <<",\"intent\":["<<animation_polish_forward<<','<<animation_polish_side<<','<<animation_polish_yaw<<"],\"player_phase\":"<<unsigned(fight.player_phase)<<",\"player_time\":"<<fight.player_time
  <<",\"attack_serial\":"<<fight.attack_start_serial<<",\"resolved_serial\":"<<fight.resolved_attack_serial<<",\"resolved_outcome\":"<<unsigned(fight.resolved_attack_outcome)
  <<",\"draw\":"<<arena_game.draw_fraction()<<",\"visible_draw\":"<<arena_bow_visual_draw<<",\"shots\":"<<arena_game.shots()<<",\"hero_active\":"<<(skeletal_hero_active?"true":"false")
  <<",\"grip_error_m\":"<<s.maximum_grip_error_m<<",\"speed\":"<<s.speed<<",\"turn_weight\":"<<s.turn_weight<<",\"reaction_weight\":"<<s.reaction<<",\"steel_recoil\":"<<s.steel_recoil
  <<",\"player_pose\":["<<player_pose.slot<<','<<player_pose.blend<<"],\"npc_pose\":["<<npc_pose.slot<<','<<npc_pose.blend<<"],\"blade_valid\":"<<(blade?"true":"false")<<",\"blade\":[";
 for(unsigned k=0;k<3;++k)out<<(k?",":"")<<base[k];for(unsigned k=0;k<3;++k)out<<','<<tip[k];out<<"],\"npc_blade_valid\":"<<(npc?"true":"false")<<",\"npc_blade\":[";
 for(unsigned k=0;k<3;++k)out<<(k?",":"")<<npc_blade.base[k];for(unsigned k=0;k<3;++k)out<<','<<npc_blade.tip[k];out<<"],\"feet\":[";
 for(unsigned side=0;side<2;++side){unsigned marker=0;double anchor[3]={};float weight=0;bool planted=false;skeletal_hero.active().foot_support(side,marker,anchor,weight,planted);
  out<<(side?",":"")<<"{\"anchor\":["<<anchor[0]<<','<<anchor[1]<<','<<anchor[2]<<"],\"weight\":"<<weight<<",\"planted\":"<<(planted?"true":"false")<<",\"residual_m\":"<<s.foot_residual_m[side]<<'}';}
 out<<"],\"joints\":[";const auto &pose=skeletal_hero.pose();const auto &skeleton=skeletal_hero.asset().skeleton;
 for(unsigned j=0;j<pose.nodes&&j<skeleton.joints.size();++j){if(j)out<<',';const auto &m=pose.global[j].m;out<<'['<<m[12]<<','<<m[13]<<','<<m[14]<<']';}
 out<<"],\"events\":[";for(unsigned n=0;n<arena_game.events().count;++n){const auto &e=arena_game.events().values[n];out<<(n?",":"")<<"["<<unsigned(e.kind)<<','<<e.serial<<','<<e.attack_serial<<','<<e.damage<<']';}out<<"]}\n";
 if(!out)animation_polish_fail("Continuous frame telemetry failed");
};
auto animation_polish_qa_capture=[&](int width,int height,std::uint64_t frame){
 if(!animation_polish_qa||!animation_polish_initialized||animation_polish_failed||animation_polish_last_frame!=frame)return;
 if(width!=1280||height!=720||animation_polish_movie_frame>=7200){animation_polish_fail("Bounded film dimensions/frame limit changed");return;}
 std::vector<unsigned char> full(std::size_t(width)*height*3),small(640*360*3);
 GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,full.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
 if(glGetError()!=GL_NO_ERROR){animation_polish_fail("Continuous framebuffer readback failed");return;}
 for(unsigned y=0;y<360;++y)for(unsigned x=0;x<640;++x)for(unsigned k=0;k<3;++k){unsigned value=0;for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx)value+=full[(std::size_t(719-(2*y+dy))*1280+2*x+dx)*3+k];small[(std::size_t(y)*640+x)*3+k]=static_cast<unsigned char>((value+2)/4);}
 animation_polish_video.write(reinterpret_cast<const char*>(small.data()),small.size());
 if(!animation_polish_video){animation_polish_fail("Runner video consumer ended early");return;}
 ++animation_polish_movie_frame;
};
auto animation_polish_qa_finish=[&](){
 if(!animation_polish_qa)return;
 animation_polish_video.close();animation_polish_samples.close();
 unsigned fresh_completed=0;for(unsigned n:animation_polish_fresh_state)fresh_completed+=n==2;
 const bool passed=fresh_completed==26&&!animation_polish_failed&&animation_polish_opener_done&&clash_done&&clash_rows.size()==30&&animation_polish_movie_frame>1200;
 std::ofstream out(animation_polish_output/"ANIMATION_FILM_RAW.json");out<<"{\"schema\":\"valwood.animation-polish.actual-game-film.v1\",\"passed\":"<<(passed?"true":"false")
  <<",\"frames\":"<<animation_polish_movie_frame<<",\"fps\":60,\"width\":640,\"height\":360,\"opener_views\":"<<animation_polish_view
  <<",\"fresh_attacks_completed\":"<<fresh_completed<<",\"contact_cases\":"<<clash_rows.size()<<",\"production_camera\":true,\"ordinary_movement_collision\":true,\"input_intent_injected\":true,\"natural_focus_certified\":false,\"controller_feel_certified\":false,\"hardware_fps_certified\":false,\"events_injected\":false}\n";
 if(!passed||!out)launch->exit_code=1;
};
