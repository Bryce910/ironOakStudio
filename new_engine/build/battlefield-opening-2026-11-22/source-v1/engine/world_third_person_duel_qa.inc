// Private ordinary-input rehearsal. No actor transform, AI phase, health,
// damage, contact, equipment identity or victory state is assigned here.
struct DuelScene {const char *name;unsigned weapon,total,kind;};
const std::array<DuelScene,6> duel_scenes={{{"sword-knight-duel",0,1,0},{"sword-orc-duel",0,1,1},{"sword-three-knights",0,3,0},{"shield-two-orcs",2,2,1},{"axe-two-orcs",3,2,1},{"staff-two-orcs",4,2,1}}};
unsigned duel_index=0,duel_round_frames=0,duel_movie_frames=0,duel_input_events=0;
unsigned duel_body=0,duel_steel=0,duel_defense=0,duel_miss=0,duel_blunt_staggers=0,duel_crowd_frames=0,duel_focus_mask=0;
std::uint64_t duel_round_start=0,duel_last_action=0,duel_last_attack_serial=0;
bool duel_started=false,duel_focused=false,duel_done=false,duel_dodge_sent=false;
unsigned duel_dodge_frames=0,duel_dodge_rig_failures=0;double duel_travel=0,duel_prior_x=0,duel_prior_z=0;
std::filesystem::path duel_dir;std::ofstream duel_trace,duel_video;std::vector<std::string> duel_rounds;
if(duel_qa){
 const char *dir=std::getenv("VALWOOD_QA_DUEL_DIR"),*pipe=std::getenv("VALWOOD_QA_DUEL_PIPE");std::ifstream marker(".valwood-third-person-duel-qa");std::string token;std::getline(marker,token);
 if(token!="VALWOOD_PRIVATE_THIRD_PERSON_DUEL_V1"||!dir||!pipe||!std::filesystem::path(dir).is_absolute()||std::filesystem::path(pipe).parent_path()!=dir||!std::filesystem::is_other(pipe)){std::fprintf(stderr,"[Third-person duel] Private marker/output/FIFO required\n");return 1;}
 duel_dir=dir;duel_trace.open(duel_dir/"DUEL_FRAMES.jsonl");duel_video.open(pipe,std::ios::binary);
 if(!duel_trace||!duel_video)return 1;duel_trace<<std::setprecision(12);
}
auto duel_axis=[&](SDL_GameControllerAxis axis,Sint16 value){controller_test_pad.axis(axis,value);++duel_input_events;};
auto duel_button=[&](SDL_GameControllerButton button){controller_test_pad.button(button,true);controller_test_pad.releases[button]=controller_test_pad.clock+2;++duel_input_events;};
auto duel_neutral=[&](){for(auto a:{SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY})duel_axis(a,0);duel_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);};
auto duel_finish=[&](bool passed,const std::string &error){
 if(duel_done)return;duel_done=true;duel_neutral();duel_trace.flush();duel_video.close();
 std::ofstream out(duel_dir/"DUEL_NATIVE_RAW.json");out<<"{\"passed\":"<<(passed?"true":"false")<<",\"error\":"<<world_json::quote(error)<<",\"completed_scenes\":"<<duel_rounds.size()<<",\"frames\":"<<duel_movie_frames<<",\"input_events\":"<<duel_input_events
 <<",\"dodge_frames\":"<<duel_dodge_frames<<",\"dodge_rig_failures\":"<<duel_dodge_rig_failures<<",\"body_hits\":"<<duel_body<<",\"steel_contacts\":"<<duel_steel<<",\"defenses\":"<<duel_defense<<",\"misses\":"<<duel_miss<<",\"blunt_staggers\":"<<duel_blunt_staggers<<",\"crowded_frames\":"<<duel_crowd_frames<<",\"focus_mask\":"<<duel_focus_mask
 <<",\"ordinary_virtual_controller\":true,\"poses_injected\":false,\"combat_outcomes_injected\":false,\"ai_phases_injected\":false,\"focus_accommodation\":\"marked private offscreen only\",\"natural_focus_certified\":false,\"hardware_fps_certified\":false,\"scenes\":[";
 for(unsigned i=0;i<duel_rounds.size();++i)out<<(i?",":"")<<duel_rounds[i];out<<"]}\n";
 std::fprintf(stderr,"[Third-person duel] %s %zu/6: %s\n",passed?"PASS":"FAIL",duel_rounds.size(),error.c_str());if(!passed)launch->exit_code=1;running=false;
};
auto duel_qa_step=[&](std::uint64_t frame){
 if(!duel_qa||duel_done)return;controller_test_pad.tick();
 if(!duel_focused){SDL_Event e={};e.type=SDL_WINDOWEVENT;e.window.windowID=SDL_GetWindowID(window);e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;SDL_PushEvent(&e);duel_focused=true;}
 if(frame+5>=std::uint64_t(launch->frames)){duel_finish(false,"Bounded ordinary-input rehearsal exhausted");return;}
 if(destination_loading||!terrain.view_ready(cam.x,cam.z,false))return;
 if(duel_index>=duel_scenes.size()){
  const bool passed=duel_dodge_frames>=60&&!duel_dodge_rig_failures&&duel_body>0&&duel_steel+duel_defense>0&&duel_miss>0&&duel_crowd_frames>30;
  duel_finish(passed,passed?"Ordinary movement, live AI, contacts and focus reviewed":"Required ordinary combat witnesses missing");return;
 }
 const auto &scene=duel_scenes[duel_index];
 if(!duel_started){
  duel_neutral();arena_show_setup();arena_total_draft=arena_simultaneous_draft=int(scene.total);arena_teammates_draft=0;arena_weapon_draft=int(scene.weapon);arena_opponents_draft=int(scene.kind);arena_difficulty_draft=0;
  if(!arena_begin()){duel_finish(false,"Ordinary arena entry rejected: "+arena_progress_error);return;}
  duel_dodge_sent=false;duel_started=true;duel_round_start=frame;duel_round_frames=0;duel_last_action=0;duel_last_attack_serial=0;duel_travel=0;duel_prior_x=cam.x;duel_prior_z=cam.z;return;
 }
 if(arena_finished()||duel_round_frames>=720){
  duel_neutral();std::ostringstream row;row<<"{\"name\":"<<world_json::quote(scene.name)<<",\"weapon\":"<<scene.weapon<<",\"opponents\":"<<scene.total<<",\"frames\":"<<duel_round_frames<<",\"travel_m\":"<<duel_travel<<",\"dodges\":"<<fight.dodges<<",\"attacks\":"<<fight.attack_start_serial<<",\"combo_links\":"<<fight.combo_links<<",\"hits\":"<<fight.hits<<",\"guarded\":"<<fight.guarded<<",\"blocks\":"<<fight.blocks<<",\"parries\":"<<fight.parries<<",\"health\":"<<fight.health<<",\"state\":"<<unsigned(arena_game.state())<<'}';
  duel_rounds.push_back(row.str());std::fprintf(stderr,"[Third-person duel] SCENE %s\n",row.str().c_str());arena_show_setup();duel_started=false;++duel_index;return;
 }
 if(!arena_controls_ready()||equipment_change.active()||!skeletal_hero.ready()||!skeletal_hero.equipment_combat_ready()){duel_neutral();return;}
 if(!third_person){duel_finish(false,"Ordinary entry did not retain third person");return;}
 if(fight.state!=TrainingState::Fighting){duel_neutral();return;}
 const unsigned age=duel_round_frames++;duel_neutral();
 const auto target=arena_target_lock.locked()?arena_target_lock.target():unsigned(arena_game.current_index());
 const auto *opponent=arena_game.opponent(target);const auto enemy=target<world_combat_capacity::opponents?arena_game.challengers()[target].position:arena_game.active_position();
 const double distance=std::hypot(cam.x-arena_origin_x-enemy.x,cam.z-arena_origin_z-enemy.z);
 // RB is ordinary focus: repeated presses choose live opponents in the crowd.
 if(age==5||(scene.total>1&&age%75==0))duel_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
 float forward=0,side=0;
 if(duel_index<2&&age<240){
  if(age<45)forward=.70f;else if(age<90)side=-.65f;else if(age<135)side=.65f;else if(age<185)forward=-.70f;else forward=.65f;
 }else{forward=distance>2.05?.55f:distance<1.30?-.45f:0;side=age%180<90?.25f:-.25f;}
 duel_axis(SDL_CONTROLLER_AXIS_LEFTX,Sint16(side*32767));duel_axis(SDL_CONTROLLER_AXIS_LEFTY,Sint16(-forward*32767));
 const bool incoming=opponent&&(opponent->phase==TrainingEnemyPhase::Windup||opponent->phase==TrainingEnemyPhase::Strike);
 const bool moving_review=duel_index<2&&age<240;
 if((incoming||moving_review)&&fight.player_phase==TrainingPlayerPhase::Ready){
  const auto direction=opponent?opponent->direction:TrainingDirection::High;duel_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767);
  duel_axis(SDL_CONTROLLER_AXIS_RIGHTX,direction==TrainingDirection::Left?-32768:direction==TrainingDirection::Right?32767:0);
  duel_axis(SDL_CONTROLLER_AXIS_RIGHTY,direction==TrainingDirection::High?-32768:direction==TrainingDirection::Low?32767:0);
 }else if(!moving_review&&age>duel_last_action+9&&fight.stamina>18&&distance<2.8){
  const bool link=fight.player_phase==TrainingPlayerPhase::Recovery&&!fight.pending_attack&&fight.combo_count<3;
  if(fight.player_phase==TrainingPlayerPhase::Ready||link){
   const unsigned direction=(unsigned(opponent?opponent->guard:TrainingDirection::High)+1+age/80)%4;
   duel_axis(SDL_CONTROLLER_AXIS_RIGHTX,direction==0?-32768:direction==1?32767:0);duel_axis(SDL_CONTROLLER_AXIS_RIGHTY,direction==2?32767:direction==3?-32768:0);duel_last_action=age;
  }
 }
 // One genuine out-of-range swing during the initial approach. It is input,
 // not a synthetic miss event, and the AI continues to approach normally.
 if(duel_index==0&&age==1){duel_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);duel_axis(SDL_CONTROLLER_AXIS_RIGHTX,-32768);}
 if(!duel_dodge_sent&&age>=120&&age<=220&&fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=23){duel_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);duel_button(SDL_CONTROLLER_BUTTON_B);duel_dodge_sent=true;}
 SDL_JoystickUpdate();
};
auto duel_qa_capture=[&](int width,int height,std::uint64_t frame,const Camera &eye){
 if(!duel_qa||duel_done||!duel_started)return;
 if(width!=1280||height!=720){duel_finish(false,"Unexpected capture size");return;}
 const double travel=std::hypot(cam.x-duel_prior_x,cam.z-duel_prior_z);if(travel<1)duel_travel+=travel;duel_prior_x=cam.x;duel_prior_z=cam.z;
 if(arena_game.active_count()>1)++duel_crowd_frames;if(arena_target_lock.locked())duel_focus_mask|=1u<<arena_target_lock.target();
 for(unsigned n=0;n<arena_game.events().count;++n){const auto &e=arena_game.events().values[n];
  duel_body+=e.kind==TrainingContactKind::PlayerBody;duel_steel+=e.kind==TrainingContactKind::EnemyBlock;duel_defense+=e.kind==TrainingContactKind::PlayerBlock||e.kind==TrainingContactKind::PlayerParry;duel_miss+=e.kind==TrainingContactKind::PlayerMiss;
  if(e.kind==TrainingContactKind::PlayerBody&&fight.weapon==world_weapon_loadout::Weapon::Staff){const auto *o=arena_game.opponent(e.opponent_id);if(o&&o->health>0&&o->phase==TrainingEnemyPhase::Stagger)++duel_blunt_staggers;}
 }
 if(fight.dodge_time>0){++duel_dodge_frames;if(!skeletal_hero_active||!skeletal_hero_draw||arena_player_character_loaded!="knight09")++duel_dodge_rig_failures;}
 const auto&s=skeletal_hero.stats();duel_trace<<"{\"video_frame\":"<<duel_movie_frames<<",\"game_frame\":"<<frame<<",\"scene\":"<<world_json::quote(duel_scenes[duel_index].name)<<",\"round_frame\":"<<duel_round_frames<<",\"weapon\":"<<unsigned(fight.weapon)<<",\"root\":["<<cam.x<<','<<cam.y-1.75<<','<<cam.z<<"],\"yaw\":"<<cam.yaw<<",\"active\":"<<arena_game.active_count()<<",\"attack_serial\":"<<fight.attack_start_serial<<",\"phase\":"<<unsigned(fight.player_phase)<<",\"dodge_time\":"<<fight.dodge_time<<",\"skeletal_active\":"<<(skeletal_hero_active?"true":"false")<<",\"skeletal_draw\":"<<(skeletal_hero_draw?"true":"false")<<",\"character\":"<<world_json::quote(arena_player_character_loaded)<<",\"guard\":"<<(fight.guard?"true":"false")<<",\"grip_error\":"<<s.maximum_grip_error_m<<",\"foot_residual\":["<<s.foot_residual_m[0]<<','<<s.foot_residual_m[1]<<"],\"focus\":"<<(arena_target_lock.locked()?int(arena_target_lock.target()):-1)<<",\"eye\":["<<eye.x<<','<<eye.y<<','<<eye.z<<','<<eye.yaw<<','<<eye.pitch<<','<<eye.fov<<"],\"npcs\":[";
 for(unsigned i=0;i<arena_game.config().total;++i){const auto&o=arena_game.challengers()[i];const auto *c=arena_game.opponent(i);duel_trace<<(i?",":"")<<"{\"id\":"<<i<<",\"position\":["<<o.position.x<<','<<o.position.y<<','<<o.position.z<<"],\"phase\":"<<(c?int(c->phase):-1)<<",\"health\":"<<(c?c->health:0)<<",\"reactions\":"<<world_combat_motion_stats(arena_opponent_motion(i)).directional_reactions<<'}';}duel_trace<<"]}\n";
 std::vector<unsigned char> full(std::size_t(width)*height*3),small(640*360*3);GLint pack;glGetIntegerv(GL_PACK_ALIGNMENT,&pack);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,full.data());glPixelStorei(GL_PACK_ALIGNMENT,pack);
 if(glGetError()!=GL_NO_ERROR){duel_finish(false,"Framebuffer readback failed");return;}
 for(unsigned y=0;y<360;++y)for(unsigned x=0;x<640;++x)for(unsigned k=0;k<3;++k){unsigned value=0;for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx)value+=full[(std::size_t(719-2*y-dy)*1280+2*x+dx)*3+k];small[(std::size_t(y)*640+x)*3+k]=static_cast<unsigned char>((value+2)/4);}
 duel_video.write(reinterpret_cast<const char*>(small.data()),small.size());++duel_movie_frames;if(!duel_video)duel_finish(false,"Video consumer failed");
};
