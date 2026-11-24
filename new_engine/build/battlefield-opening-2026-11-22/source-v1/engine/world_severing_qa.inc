// Private, bounded anatomical rehearsal. Geometric fixture placement/pose holds
// are counted explicitly. Health, injury masks, death and rewards are never
// assigned here: those outcomes must come from SDL actions and real contacts.
struct SeveringQaCase {const char *name;unsigned opponent,weapon,part,mode;};
// mode: 0 edged cut, 1 lethal sharp thrust, 2 blunt thrust, 3 miss,
// 4 actual weapon interception, 5 physical arrow.
const std::array<SeveringQaCase,17> severing_cases={{
 {"right-hand-survivor",0,0,6,0},{"left-hand-disarm",0,0,5,0},{"torso-thrust",0,0,2,1},
 {"left-arm-survivor",1,0,3,0},{"right-arm-disarm",1,0,4,0},{"head-thrust",1,0,1,1},
 {"head-cut",2,0,1,0},{"torso-cut",3,2,2,0},{"left-leg-cut",4,0,7,0},
 {"right-leg-cut",5,3,8,0},{"staff-no-sever",6,4,2,2},{"arrow-no-sever",6,1,2,5},
 {"visible-miss",6,0,0,3},{"steel-interception",6,0,0,4},{"finish-body",6,0,2,1},
 {"axe-head-cut",7,3,1,0},{"victory-save",8,0,0,6}
}};
struct SeveringQaResult {unsigned index=0,opponent=0,weapon=0,direction=0,expected_part=0,actual_part=0,severed_part=0,mask_before=0,mask_after=0,weapon_hand=0;float hp_before=0,hp_after=0;std::uint64_t attack_serial=0,callback_serial=0;double point[3]={};bool weapon_contact=false,projectile=false;};
bool severing_passed=false,severing_failed=false;unsigned severing_phase=0,severing_case=0,severing_checks=0,severing_events=0;
std::uint64_t severing_frame=0,severing_action=0,severing_event_seen=0,severing_attack_before=0,severing_purse=0,severing_wins=0;
unsigned long long severing_player_pose_writes=0,severing_enemy_pose_holds=0,severing_search_queries=0,severing_ai_free_frames=0;
unsigned severing_direction=0,severing_guard=0,severing_hp_events=0;double severing_x=0,severing_z=1.35,severing_yaw=3.141592653589793;
float severing_hp_before=0;unsigned severing_mask_before=0,severing_before_hits=0,severing_before_parries=0,severing_before_blocks=0;
unsigned severing_before_launch=0;unsigned long long severing_before_caps=0,severing_before_mirror=0;
bool severing_free_attack=false,severing_free_parry=false,severing_free_block=false,severing_free_signal=false;
bool severing_signal_pending=false,severing_recovery=false;
std::uint64_t severing_signal_before=0,severing_signal_frame=0,severing_gate_last=~std::uint64_t(0);
unsigned severing_gate_last_phase=~0u,severing_gate_logs=0,severing_signal_requests=0,severing_signal_commits=0;
unsigned long long severing_gate_wait_frames=0,severing_focus_wait_frames=0;
bool severing_free_pad_dirty=false,severing_steel_pending=false;
unsigned severing_free_neutral_frames=0,severing_station_plans=0,severing_steel_requests=0,severing_steel_commits=0;
unsigned long long severing_station_queries=0,severing_navigation_frames=0;
std::uint64_t severing_navigation_last=0,severing_steel_before=0,severing_steel_frame=0;
float severing_station_last_windup=-1;
std::string severing_capture_pending;std::vector<std::string> severing_labels,severing_capture_names;
std::vector<SeveringQaResult> severing_results;
world_blood_effects::Stain severing_stain;bool severing_stain_saved=false,severing_stain_anchored=false;
std::uint64_t severing_stain_frame=0;
const char *severing_view_env=std::getenv("VALWOOD_SEVERING_QA_VIEW");
const bool severing_view_valid=!severing_view_env||!std::strcmp(severing_view_env,"third")||!std::strcmp(severing_view_env,"first");
const bool severing_want_third=!severing_view_env||std::strcmp(severing_view_env,"first");
unsigned severing_view_switches=0;bool severing_view_pending=false;
const bool reaction_sequence_enabled=std::getenv("VALWOOD_QA_REACTION_SEQUENCE")!=nullptr;
std::uint64_t reaction_sequence_start=0;unsigned reaction_sequence_captures=0;
auto severing_workflow=[&](std::uint64_t frame){
 if(!severing_qa||severing_passed||severing_failed||!running)return;
 severing_frame=frame;arena_qa_frame=frame;controller_test_pad.tick();
 // Read-only diagnostics never arm/reset a gameplay gate or change combat.
 // Keep normal output bounded, but always retain a complete timeout snapshot.
 auto log_gates=[&](bool forced){
  const auto flags=SDL_GetWindowFlags(window);const bool focused_now=(flags&SDL_WINDOW_INPUT_FOCUS)&&preview_pad.focused;
  const bool modal=arena_ui_phase!=ArenaUiPhase::Fight||preview_screen!=1||show_graphics||destination_loading;
  const bool held=arena_raw_held(),line=studio.components_runtime&&studio.components_runtime->current_line();
  const bool terrain_ready=terrain.view_ready(cam.x,cam.z,false);
  const std::uint64_t bits=std::uint64_t(focused_now)|(std::uint64_t(preview_pad.focused)<<1)|(std::uint64_t(modal)<<2)|(std::uint64_t(arena_gameplay_ready)<<3)|(std::uint64_t(arena_gameplay_gate.armed)<<4)|(std::uint64_t(held)<<5)|(std::uint64_t(inventory_controls_ready)<<6)|(std::uint64_t(inventory_open)<<7)|(std::uint64_t(preview_blocked())<<8)|(std::uint64_t(show_map)<<9)|(std::uint64_t(show_graphics)<<10)|(std::uint64_t(destination_loading)<<11)|(std::uint64_t(studio.production_freeze())<<12)|(std::uint64_t(studio.cinematic_preview)<<13)|(std::uint64_t(line)<<14)|(std::uint64_t(ImGui::GetIO().WantCaptureKeyboard)<<15)|(std::uint64_t(terrain_ready)<<16)|(std::uint64_t(arena_controls_ready())<<17);
  if(!forced&&((bits==severing_gate_last&&severing_phase==severing_gate_last_phase)||severing_gate_logs>=128))return;
  severing_gate_last=bits;severing_gate_last_phase=severing_phase;++severing_gate_logs;
  const auto *o=arena_game.opponent(0);
  std::printf("[Severing QA] GATE frame%llu phase%u forced%u flags0x%x focused%u pad_focus%u modal%u screen%d ui%u ready%u armed%u held%u inventory_ready%u inventory_open%u preview_blocked%u map%u graphics%u loading%u studio_freeze%u cinematic%u dialogue%u keyboard%u terrain%u controls%u player_phase%u attack%llu enemy_phase%u free_attack%u free_parry%u free_block%u signal_pending%u signal_committed%u gate_wait%llu focus_wait%llu\n",static_cast<unsigned long long>(frame),severing_phase,unsigned(forced),flags,unsigned(focused_now),unsigned(preview_pad.focused),unsigned(modal),preview_screen,unsigned(arena_ui_phase),unsigned(arena_gameplay_ready),unsigned(arena_gameplay_gate.armed),unsigned(held),unsigned(inventory_controls_ready),unsigned(inventory_open),unsigned(preview_blocked()),unsigned(show_map),unsigned(show_graphics),unsigned(destination_loading),unsigned(studio.production_freeze()),unsigned(studio.cinematic_preview),unsigned(line),unsigned(ImGui::GetIO().WantCaptureKeyboard),unsigned(terrain_ready),unsigned(arena_controls_ready()),unsigned(fight.player_phase),static_cast<unsigned long long>(fight.attack_start_serial),o?unsigned(o->phase):~0u,unsigned(severing_free_attack),unsigned(severing_free_parry),unsigned(severing_free_block),unsigned(severing_signal_pending),unsigned(severing_free_signal),severing_gate_wait_frames,severing_focus_wait_frames);
 };
 auto fail=[&](const char *label){log_gates(true);severing_failed=true;std::fprintf(stderr,"[Severing QA] FAIL phase%u case%u %s\n",severing_phase,severing_case,label);launch->exit_code=1;running=false;};
 auto check=[&](bool ok,const char *label){if(!ok){fail(label);return false;}++severing_checks;severing_labels.emplace_back(label);std::printf("[Severing QA] PASS %s\n",label);return true;};
 auto next=[&](unsigned phase){severing_phase=phase;severing_action=frame;};
 auto key=[&](SDL_Keycode code){for(bool down:{true,false}){SDL_Event e={};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.windowID=SDL_GetWindowID(window);e.key.state=down?SDL_PRESSED:SDL_RELEASED;e.key.keysym.sym=code;e.key.keysym.scancode=SDL_GetScancodeFromKey(code);e.key.keysym.unused=0x56414c54u;if(SDL_PushEvent(&e)!=1){fail("tagged SDL key rejected");return false;}++severing_events;}return true;};
 auto axis=[&](SDL_GameControllerAxis a,Sint16 value){controller_test_pad.axis(a,value);++severing_events;};
 auto stick=[&](Sint16 x,Sint16 y){axis(SDL_CONTROLLER_AXIS_RIGHTX,x);axis(SDL_CONTROLLER_AXIS_RIGHTY,y);};
 auto lt=[&](bool down){axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,down?32767:-32768);};
 auto rt=[&](bool down){axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,down?32767:-32768);};
 auto neutral=[&](){stick(0,0);axis(SDL_CONTROLLER_AXIS_LEFTX,0);axis(SDL_CONTROLLER_AXIS_LEFTY,0);lt(false);rt(false);controller_test_pad.button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER,false);controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,false);};
 auto attack=[&](TrainingDirection d){stick(d==TrainingDirection::Left?-32768:d==TrainingDirection::Right?32767:0,d==TrainingDirection::High?32767:d==TrainingDirection::Low?-32768:0);};
 auto guard=[&](TrainingDirection d){stick(d==TrainingDirection::Left?-32768:d==TrainingDirection::Right?32767:0,d==TrainingDirection::High?-32768:d==TrainingDirection::Low?32767:0);lt(true);};
 auto capture=[&](const std::string &name){severing_capture_pending=name;arena_qa_capture_pending=name;};
 auto hold=[&](unsigned id){const bool ok=arena_game.fixture_geometry_pose(id,{severing_x,arena_game.bounds.floor_y,severing_z},float(severing_yaw),TrainingDirection(severing_guard),fight.attack_start_serial,severing_recovery);if(ok)++severing_enemy_pose_holds;return ok;};
 auto player_place=[&](){cam.x=arena_origin_x;cam.y=arena_floor+1.75;cam.z=arena_origin_z;cam.yaw=3.14159265359f;cam.pitch=-.04f;++severing_player_pose_writes;};
 auto legal_setup=[](double x,double z){return std::isfinite(x)&&std::isfinite(z)&&std::hypot(x,z)>=1.35;};
 auto read_hit=[&](unsigned id,unsigned weapon,unsigned direction,double x,double z,TrainingAnatomicalHit &hit,bool actual=false){
  const auto *o=arena_game.opponent(id);if(!o)return false;
  WorldCombatContactFrame f;f.player[0]=arena_origin_x;f.player[1]=arena_floor;f.player[2]=arena_origin_z;f.player_yaw=0;f.defender[0]=arena_origin_x+x;f.defender[1]=arena_floor;f.defender[2]=arena_origin_z+z;f.defender_yaw=float(std::atan2(-x,-z));
  if(actual){const auto &a=arena_game.challengers()[id];f.player[0]=cam.x;f.player[1]=cam.y-1.75;f.player[2]=cam.z;f.player_yaw=3.14159265359f-cam.yaw;world_combat_motion_facing(combat_motion,0,f.player_yaw);f.defender[0]=arena_origin_x+a.position.x;f.defender[1]=arena_floor+a.position.y;f.defender[2]=arena_origin_z+a.position.z;if(!world_combat_motion_facing(combat_motion,id+1,f.defender_yaw))f.defender_yaw=float(std::atan2(cam.x-f.defender[0],cam.z-f.defender[2]));}
  const double dx=f.defender[0]-f.player[0],dz=f.defender[2]-f.player[2];
  const double windup=(direction==4?WorldTrainingCombat::thrust_windup_seconds:WorldTrainingCombat::cut_windup_seconds)*world_weapon_loadout::profile(world_weapon_loadout::Weapon(weapon)).speed,delay=std::ceil(windup*240.)/240.;
  TrainingAnatomicalQuery q;q.opponent_id=id;q.weapon=world_weapon_loadout::Weapon(weapon);q.direction=TrainingDirection(direction);q.attack_serial=fight.attack_start_serial+1;q.local_x=dx*std::cos(f.player_yaw)-dz*std::sin(f.player_yaw);q.local_z=dx*std::sin(f.player_yaw)+dz*std::cos(f.player_yaw);q.distance=std::hypot(dx,dz);q.spatial=true;q.weapon_guard=o->weapon_hand!=TrainingWeaponHand::None&&direction==unsigned(o->guard);q.contact_clock=fight.clock+delay;q.windup_seconds=windup;q.defender_phase=o->phase;q.defender_direction=o->direction;q.defender_guard=o->guard;q.defender_time=severing_recovery?0:o->time+delay-1./240.;q.defender_duration=o->duration;q.severed_parts=o->severed_parts;++severing_search_queries;
  return world_combat_motion_contact(combat_motion,q,f,hit)&&hit.valid;
 };
 auto actual_setup=[&](unsigned id){const auto &a=arena_game.challengers()[id];return std::hypot(arena_origin_x+a.position.x-cam.x,arena_origin_z+a.position.z-cam.z)>=1.35-1.e-6&&std::hypot(a.position.x-severing_x,a.position.z-severing_z)<1.e-6&&std::abs(cam.y-1.75-arena_floor)<.01&&std::abs(a.position.y-arena_game.bounds.floor_y)<.01;};
 if(!severing_view_valid){fail("unsupported requested camera view");return;}
 if(frame+60>=std::uint64_t(launch->frames)){fail("bounded rehearsal timed out");return;}
 const bool focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)&&preview_pad.focused;
 if(severing_phase==50)log_gates(false);
 // A focus handoff resets the host's neutral gate. Release the actual virtual
 // device while waiting, so a previously held defense/walk cannot latch it shut.
 if(!focused){neutral();++severing_focus_wait_frames;if(frame%30==0){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();}return;}
 if(destination_loading||frame<=std::uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false))return;
 const auto age=frame-severing_action;
 if(severing_phase!=0&&age>1800){fail("single phase exceeded bounded thirty seconds");return;}
 if(severing_stain_saved){const auto stage=blood_effects.stage();for(unsigned i=0;i<stage.stain_count;++i){const auto &s=stage.stains[i];if(s.serial==severing_stain.serial&&s.hit_id==severing_stain.hit_id&&s.defender==severing_stain.defender){severing_stain_anchored=s.position.x==severing_stain.position.x&&s.position.y==severing_stain.position.y&&s.position.z==severing_stain.position.z&&s.yaw==severing_stain.yaw&&s.persistent;break;}}}
 if(severing_phase==0){
  if(!check(arena_ui_phase==ArenaUiPhase::Setup&&arena_total_draft==8&&arena_simultaneous_draft==1,"private real setup has eight opponents one at a time"))return;
  if(!check(combat_anatomy_ready&&world_combat_motion_anatomy_stats(combat_motion).loaded,"validated anatomical renderer metadata loaded"))return;
  severing_purse=arena_progress.balance;severing_wins=arena_progress.wins;neutral();if(key(SDLK_RETURN))next(5);return;
 }
 if(severing_phase==5){
  neutral();if(!arena_controls_ready())return;
  // Switch through the same tagged SDL V key as normal play, after Setup's
  // genuine Enter action has launched the fight. Read actual game state back.
  if(third_person!=severing_want_third){
   if(!severing_view_pending){if(key(SDLK_v)){severing_view_pending=true;return;}}
   if(age>120)fail("requested camera switch was not acknowledged");
   return;
  }
  if(severing_view_pending){++severing_view_switches;severing_view_pending=false;}
  capture("severing-setup");next(10);return;
 }
 if(severing_case>=severing_cases.size()){fail("case index exceeded contract");return;}
 const auto &c=severing_cases[severing_case];
 if(severing_phase==10){
  if(c.mode==6){next(80);return;}
  const auto *o=arena_game.opponent(c.opponent);
  if(!arena_controls_ready()||fight.player_phase!=TrainingPlayerPhase::Ready||arena_game.challengers()[c.opponent].state!=world_arena_combat::ChallengerState::Active||!o)return;
  if(!check(fight.difficulty==preview_settings.difficulty,"real encounter latches persisted selected difficulty"))return;
  neutral();const SDL_Keycode keys[]={SDLK_1,SDLK_2,SDLK_3,SDLK_4,SDLK_5};if(!key(keys[c.weapon]))return;
  // All roots share the real arena floor. This fixture never raises a blade or
  // target vertically to manufacture a desired anatomical contact.
  static constexpr double xs[]={0,-.03,-.03,-.03,.05,-.82,-.03,-.96,.10};
  static constexpr double zs[]={1.35,1.35,1.35,1.35,1.37,1.13,1.35,.97,1.35};
  static constexpr unsigned gs[]={0,0,0,0,1,0,4,0,0};
  severing_x=xs[c.part];severing_z=zs[c.part];severing_guard=gs[c.part];severing_yaw=std::atan2(-severing_x,-severing_z);
  // An existing authored recovery exposes the right leg without overlapping
  // bodies. This is a counted geometric pose hold, never an injury assignment.
  severing_recovery=c.mode==0&&c.part==8;
  // Exact additional posed witnesses: head/torso sharp thrust and actual
  // steel interception. These remain XZ-only and outside body push-out radius.
  if(c.mode==1&&c.part==1){severing_x=.04;severing_z=1.53;severing_guard=1;severing_yaw=std::atan2(-severing_x,-severing_z);}
  if(c.mode==1&&c.part==2){severing_x=-.44;severing_z=1.29;severing_guard=0;severing_yaw=std::atan2(-severing_x,-severing_z);}
  if(c.mode==4){severing_x=-.08;severing_z=1.37;severing_guard=1;severing_yaw=std::atan2(-severing_x,-severing_z);}
  if(c.mode==3||c.mode==5){severing_x=0;severing_z=5;severing_guard=0;severing_yaw=3.14159265359;}
  if(!legal_setup(severing_x,severing_z)){fail("fixture seed overlaps production player separation");return;}
  player_place();if(!hold(c.opponent)){fail("geometric fixture pose rejected");return;}next(20);return;
 }
 if(severing_phase==20){
  if(!hold(c.opponent)){fail("geometric fixture lost live target");return;}
  if(age<16||fight.player_phase!=TrainingPlayerPhase::Ready||fight.stamina<55)return;
  if(unsigned(arena_game.weapon())!=c.weapon||unsigned(fight.weapon)!=c.weapon){std::printf("[Severing QA] EQUIP mismatch %s expected%u arena%u combat%u phase%u age%llu\n",c.name,c.weapon,unsigned(arena_game.weapon()),unsigned(fight.weapon),unsigned(fight.player_phase),static_cast<unsigned long long>(age));fail("actual equipped weapon does not match the requested fixture weapon");return;}
  const auto *o=arena_game.opponent(c.opponent);severing_hp_before=o->health;severing_mask_before=o->severed_parts;severing_attack_before=fight.attack_start_serial;severing_event_seen=fight.contact_serial;
  const auto stats=world_combat_motion_anatomy_stats(combat_motion);severing_before_launch=c.part?stats.launches[c.part-1]:0;severing_before_caps=stats.cap_draws;
  if(c.mode==5){if(!actual_setup(c.opponent)){fail("arrow draw roots differ from legal held setup");return;}rt(true);next(25);return;}
  bool found=false;TrainingAnatomicalHit hit;
  const unsigned first=c.mode==1||c.mode==2?4:0,last=c.mode==1||c.mode==2?5:4;
  auto accepts=[&](bool exists,const TrainingAnatomicalHit &h){return c.mode==3?!exists:c.mode==4?exists&&h.weapon_contact:exists&&!h.weapon_contact&&unsigned(h.part)==c.part;};
  // Seed the published real-pose witness first, then search a small bounded XZ
  // neighbourhood using the SAME read-only renderer contact function. Search
  // queries cannot damage an opponent and are reported separately.
  for(unsigned d=first;d<last&&!found;++d){if(legal_setup(severing_x,severing_z)&&accepts(read_hit(c.opponent,c.weapon,d,severing_x,severing_z,hit),hit)){severing_direction=d;found=true;}}
  const double seed_x=severing_x,seed_z=severing_z;
  for(unsigned d=first;d<last&&!found;++d)for(int iz=0;iz<19&&!found;++iz)for(int ix=-9;ix<=9&&!found;++ix){const double x=seed_x+ix*.08,z=.65+iz*.08;if(!legal_setup(x,z))continue;TrainingAnatomicalHit h;const bool exists=read_hit(c.opponent,c.weapon,d,x,z,h);if(accepts(exists,h)){severing_x=x;severing_z=z;severing_direction=d;hit=h;found=true;}}
  if(!check(found,(std::string(c.name)+" has an actual posed same-floor contact setup").c_str()))return;
  severing_yaw=std::atan2(-severing_x,-severing_z);player_place();if(!hold(c.opponent)){fail("selected geometric fixture rejected");return;}
  std::printf("[Severing QA] FIXTURE %s target%u x%.6f z%.6f yaw%.6f guard%u direction%u phase .26 .36 expected%u\n",c.name,c.opponent,severing_x,severing_z,severing_yaw,severing_guard,severing_direction,c.part);
  next(21);return;
 }
 if(severing_phase==21){
  if(!hold(c.opponent)){fail("target lost during setup settling");return;}if(age<12||!arena_controls_ready())return;
  TrainingAnatomicalHit hit;const bool exists=read_hit(c.opponent,c.weapon,severing_direction,severing_x,severing_z,hit,true);
  const bool expected=c.mode==3?!exists:c.mode==4?exists&&hit.weapon_contact:exists&&!hit.weapon_contact&&unsigned(hit.part)==c.part;
  std::printf("[Severing QA] RECHECK %s legal%d expected%d hit%d part%u player%.6f %.6f target%.6f %.6f recovery%d\n",c.name,int(actual_setup(c.opponent)),int(expected),int(exists),unsigned(hit.part),cam.x-arena_origin_x,cam.z-arena_origin_z,arena_game.challengers()[c.opponent].position.x,arena_game.challengers()[c.opponent].position.z,int(severing_recovery));
  if(!actual_setup(c.opponent)||!expected){fail("settled actual roots/facing do not reproduce the selected anatomical contact");return;}
  neutral();if(severing_direction==4){controller_test_pad.button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER,true);controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,true);severing_events+=2;}else attack(TrainingDirection(severing_direction));next(30);return;
 }
 if(severing_phase==25){if(!hold(c.opponent)){fail("arrow fixture lost target");return;}if(age<72)return;if(!actual_setup(c.opponent)){fail("arrow release roots differ from legal held setup");return;}rt(false);next(30);return;}
 if(severing_phase==30){
  neutral();
  for(unsigned i=0;i<arena_game.events().count;++i){const auto &e=arena_game.events().values[i];if(e.serial<=severing_event_seen||e.opponent_id!=c.opponent)continue;
   if(e.kind!=TrainingContactKind::PlayerBody&&e.kind!=TrainingContactKind::EnemyBlock)continue;
   const auto *o=arena_game.opponent(c.opponent);SeveringQaResult r;r.index=severing_case;r.opponent=c.opponent;r.weapon=c.weapon;r.direction=severing_direction;r.expected_part=c.part;r.actual_part=unsigned(e.body_part);r.severed_part=unsigned(e.severed_part);r.mask_before=severing_mask_before;r.mask_after=o->severed_parts;r.hp_before=severing_hp_before;r.hp_after=o->health;r.attack_serial=e.attack_serial;r.callback_serial=combat_anatomical_serials[c.opponent];r.weapon_hand=unsigned(o->weapon_hand);r.weapon_contact=e.kind==TrainingContactKind::EnemyBlock;r.projectile=e.projectile;std::copy(e.point,e.point+3,r.point);severing_results.push_back(r);
   bool good=false;
   if(c.mode==4)good=e.kind==TrainingContactKind::EnemyBlock&&e.anatomical&&o->health==severing_hp_before&&o->severed_parts==severing_mask_before;
   else if(c.mode==5)good=e.projectile&&o->health>0&&o->health<severing_hp_before&&o->severed_parts==severing_mask_before;
   else if(c.mode==2)good=e.anatomical&&unsigned(e.body_part)==c.part&&o->health>0&&o->health<severing_hp_before&&o->severed_parts==severing_mask_before;
   else if(c.mode==1)good=e.anatomical&&unsigned(e.body_part)==c.part&&o->health==0&&e.severed_part==TrainingBodyPart::None&&o->severed_parts==severing_mask_before;
   else if(c.mode==0)good=e.anatomical&&unsigned(e.body_part)==c.part&&unsigned(e.severed_part)==c.part&&(o->severed_parts&world_combat_anatomy::bit(TrainingBodyPart(c.part)))&&((c.part>=3&&c.part<=6)?o->health>0:o->health==0);
   if(c.mode!=5)good=good&&e.attack_serial>severing_attack_before&&r.callback_serial==e.attack_serial;
   if(!check(good,(std::string(c.name)+" real input commits the expected contact and authoritative outcome").c_str()))return;
   severing_hp_events++;arena_game.fixture_release_geometry();capture(std::string(c.name)+"-impact");
   if(reaction_sequence_enabled&&severing_case==6&&c.mode==0&&e.severed_part==TrainingBodyPart::Head)reaction_sequence_start=frame;
   next(40);return;
  }
  if(c.mode==3&&fight.attack_start_serial>severing_attack_before&&fight.player_phase==TrainingPlayerPhase::Ready&&age>36){const auto *o=arena_game.opponent(c.opponent);if(!check(o->health==severing_hp_before&&o->severed_parts==severing_mask_before,"visible blade miss preserves health and injuries"))return;SeveringQaResult r;r.index=severing_case;r.opponent=c.opponent;r.weapon=c.weapon;r.mask_before=r.mask_after=o->severed_parts;r.hp_before=r.hp_after=o->health;r.attack_serial=fight.attack_start_serial;severing_results.push_back(r);arena_game.fixture_release_geometry();capture("visible-miss-impact");next(40);return;}
  if(c.mode!=5&&arena_game.opponent(c.opponent)->health>0&&!hold(c.opponent)){fail("live impact fixture lost target");return;}
  if(age>240){fail("real SDL attack produced no required anatomical event");return;}return;
 }
 if(severing_phase==40){
  neutral();const auto stats=world_combat_motion_anatomy_stats(combat_motion);
  if(c.mode==0&&age>=4){if(!check(stats.launches[c.part-1]>severing_before_launch&&stats.detached_draws[c.part-1]>0&&stats.cap_draws>severing_before_caps,(std::string(c.name)+" draws detached anatomy and closed cut caps").c_str()))return;next(41);return;}
  if(c.mode==0)return;next(42);return;
 }
 if(severing_phase==41){
  neutral();if(age<210)return;const auto stats=world_combat_motion_anatomy_stats(combat_motion);
  // Earlier limbs settle while fighting. Won Results advances only corpse
  // visuals; the final acceptance also checks every active detached piece.
  if(c.opponent<7&&!check(stats.settled[c.part-1]>0&&stats.maximum_ground_penetration<=.03,(std::string(c.name)+" detached part settles above the real floor").c_str()))return;
  capture(std::string(c.name)+"-settled");next(42);return;
 }
 if(severing_phase==42){
  const auto stage=blood_effects.stage();if(!severing_stain_saved&&stage.stain_count){for(unsigned i=0;i<stage.stain_count;++i)if(stage.stains[i].persistent){severing_stain=stage.stains[i];severing_stain_saved=true;severing_stain_frame=frame;break;}}
  if(severing_case==0){severing_before_hits=fight.enemy_hits;severing_before_parries=fight.parries;severing_before_blocks=fight.guarded;severing_before_mirror=world_combat_motion_anatomy_stats(combat_motion).mirrored_draws;next(50);return;}
  ++severing_case;next(10);return;
 }
 if(severing_phase==50){
  // No geometric pose/position helper is called in this phase. The survivor's
  // actual AI owns movement, windup and guard changes; SDL owns player defense.
  arena_game.fixture_release_geometry();neutral();
  if(severing_free_pad_dirty){severing_free_neutral_frames=0;severing_free_pad_dirty=false;}else if(severing_free_neutral_frames<100)++severing_free_neutral_frames;
  // Wait for the SAME normal readiness gates to observe a fully neutral pad.
  // No arming flag, timer, pose, injury or health is assigned by this recovery.
  if(!arena_controls_ready()||!inventory_controls_ready){++severing_gate_wait_frames;return;}
  ++severing_ai_free_frames;const auto *o=arena_game.opponent(0);
  if(!o||o->health<=0){fail("one-hand survivor died before free AI witness");return;}
  for(unsigned i=0;i<arena_game.events().count;++i){const auto &e=arena_game.events().values[i];if(e.opponent_id||e.weapon_hand!=TrainingWeaponHand::Left)continue;severing_free_attack|=e.kind==TrainingContactKind::EnemyBody;severing_free_parry|=e.kind==TrainingContactKind::PlayerParry;severing_free_block|=e.kind==TrainingContactKind::EnemyBlock;}
  if(severing_steel_pending&&fight.attack_start_serial>severing_steel_before){severing_steel_pending=false;++severing_steel_commits;std::printf("[Severing QA] STEEL attack committed: frame%llu attack%llu\n",static_cast<unsigned long long>(frame),static_cast<unsigned long long>(fight.attack_start_serial));}
  if(!severing_free_attack)return;
  if(!severing_free_parry){if(o->phase==TrainingEnemyPhase::Windup&&o->duration-o->time<.17){guard(o->direction);severing_free_pad_dirty=true;}return;}
  if(!severing_free_block){
   // A genuine out-of-range cut gives the AI something to observe. Its own
   // delayed reaction chooses the guard; no guard/phase field is assigned.
   if(!severing_free_signal){
    if(severing_signal_pending){
     if(fight.attack_start_serial>severing_signal_before){severing_free_signal=true;severing_signal_pending=false;++severing_signal_commits;std::printf("[Severing QA] SIGNAL committed: requested_frame%llu attack%llu\n",static_cast<unsigned long long>(severing_signal_frame),static_cast<unsigned long long>(fight.attack_start_serial));}
     else{if(frame-severing_signal_frame>12)severing_signal_pending=false;return;}
    }
    if(!severing_free_signal){if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>50){severing_signal_before=fight.attack_start_serial;severing_signal_frame=frame;severing_signal_pending=true;++severing_signal_requests;attack(TrainingDirection::Right);severing_free_pad_dirty=true;}return;}
   }
   // Only a live Windup plants the free AI long enough to plan a stationary
   // interception. The old Measure-only search chased an unreachable .834m
   // offset while the AI moved toward 2.05m. No production reach is changed.
   constexpr double input_frame=1./60.,walk_speed=2.8;
   const double attack_windup=(o->guard==TrainingDirection::Thrust?WorldTrainingCombat::thrust_windup_seconds:WorldTrainingCombat::cut_windup_seconds)*world_weapon_loadout::profile(arena_game.weapon()).speed;
   const double impact_delay=std::ceil(attack_windup*240.)/240.;
   const double remaining=o->duration-o->time;
   auto defend=[&](){if(o->phase==TrainingEnemyPhase::Windup&&remaining<.24&&fight.player_phase==TrainingPlayerPhase::Ready){guard(o->direction);severing_free_pad_dirty=true;}};
   if(severing_steel_pending){if(frame-severing_steel_frame>12)severing_steel_pending=false;defend();return;}
   if(o->phase!=TrainingEnemyPhase::Windup){severing_station_last_windup=-1;return;}
   if(remaining<=impact_delay+.06||fight.player_phase!=TrainingPlayerPhase::Ready||fight.stamina<=50){defend();return;}
   const auto target=arena_game.challengers()[0].position;
   WorldCombatContactFrame actual;actual.player[0]=cam.x;actual.player[1]=cam.y-1.75;actual.player[2]=cam.z;actual.player_yaw=3.14159265359f-cam.yaw;world_combat_motion_facing(combat_motion,0,actual.player_yaw);
   actual.defender[0]=arena_origin_x+target.x;actual.defender[1]=arena_floor+target.y;actual.defender[2]=arena_origin_z+target.z;actual.defender_yaw=float(std::atan2(cam.x-actual.defender[0],cam.z-actual.defender[2]));world_combat_motion_facing(combat_motion,1,actual.defender_yaw);
   auto steel_at=[&](const WorldCombatContactFrame &f,double future){
    TrainingAnatomicalQuery q;q.opponent_id=0;q.weapon=arena_game.weapon();q.direction=o->guard;q.attack_serial=fight.attack_start_serial+1;q.contact_clock=fight.clock+future;q.windup_seconds=attack_windup;q.defender_phase=o->phase;q.defender_direction=o->direction;q.defender_guard=o->guard;q.defender_time=std::max(0.,o->time+future-1./240.);q.defender_duration=o->duration;q.severed_parts=o->severed_parts;
    q.weapon_guard=o->phase!=TrainingEnemyPhase::Recovery&&o->phase!=TrainingEnemyPhase::Stagger&&world_combat_anatomy::can_fight(o->severed_parts);
    const double dx=f.defender[0]-f.player[0],dz=f.defender[2]-f.player[2],c=std::cos(f.player_yaw),sn=std::sin(f.player_yaw);q.spatial=true;q.distance=std::hypot(dx,dz);q.local_x=dx*c-dz*sn;q.local_z=dx*sn+dz*c;
    if(q.distance<1.35-1.e-6||future<0||q.defender_time>q.defender_duration-.04)return false;
    TrainingAnatomicalHit h;++severing_search_queries;++severing_station_queries;return world_combat_motion_contact(combat_motion,q,f,h)&&h.valid&&h.weapon_contact;
   };
   // Always brake for one real input frame, then recheck the CURRENT actual
   // roots/facing/full sweep. A predicted station alone can never cause damage.
   if(steel_at(actual,impact_delay)){
    if(frame<=severing_navigation_last+1||severing_free_neutral_frames<1)return;
    severing_steel_before=fight.attack_start_serial;severing_steel_frame=frame;severing_steel_pending=true;++severing_steel_requests;severing_free_pad_dirty=true;
    if(o->guard==TrainingDirection::Thrust){controller_test_pad.button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER,true);controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,true);severing_events+=2;}else attack(o->guard);return;
   }
   // Fixed budget: at most 3 radii x 9 angles x 3 instants = 81 planner
   // queries plus one current-position query per eligible frame. These include
   // the certified mirrored 2.05m
   // stations at+.36/+.40/+.44rad; no body/weapon capsules are enlarged.
   static constexpr double radii[]={1.95,2.05,2.15};
   static constexpr double angles[]={-.48,-.44,-.40,-.36,0,.36,.40,.44,.48};
   static constexpr double instants[]={2./3.,.75,5./6.};
   bool planned=false;double best_distance=1.e9,goal_x=cam.x,goal_z=cam.z,planned_time=0;
   const double cs=std::cos(actual.player_yaw),sn=std::sin(actual.player_yaw);
   for(double radius:radii)for(double angle:angles){
    const double lx=std::sin(angle)*radius,lz=std::cos(angle)*radius;
    const double px=actual.defender[0]-cs*lx-sn*lz,pz=actual.defender[2]+sn*lx-cs*lz;
    if(!arena_game.bounds.contains({px-arena_origin_x,0,pz-arena_origin_z},arena_game.bounds.player_radius+.02))continue;
    const double dx=px-cam.x,dz=pz-cam.z,distance=std::hypot(dx,dz);if(distance>=best_distance)continue;
    const double length2=dx*dx+dz*dz,t=length2>1.e-12?std::clamp(((actual.defender[0]-cam.x)*dx+(actual.defender[2]-cam.z)*dz)/length2,0.,1.):0;
    if(std::hypot(cam.x+t*dx-actual.defender[0],cam.z+t*dz-actual.defender[2])<1.35-1.e-6)continue;
    const double arrival=distance/walk_speed+(distance>.03?2*input_frame:0);
    for(double instant:instants){const double contact_time=o->duration*instant,future=contact_time-o->time+1./240.;if(future<arrival+impact_delay||contact_time>o->duration-.04)continue;
     auto f=actual;f.player[0]=px;f.player[2]=pz;const float desired=float(std::atan2(px-f.defender[0],pz-f.defender[2]));f.defender_yaw+=float(std::clamp(std::remainder(double(desired-f.defender_yaw),6.28318530718),-2.8*future,2.8*future));
     if(steel_at(f,future)){planned=true;best_distance=distance;goal_x=px;goal_z=pz;planned_time=contact_time;break;}
    }
   }
   if(!planned){defend();return;}
   if(severing_station_last_windup<0||o->time<severing_station_last_windup){++severing_station_plans;std::printf("[Severing QA] STATION guard%u direction%u time%.6f/%.6f goal%.6f %.6f walk%.6f contact%.6f\n",unsigned(o->guard),unsigned(o->direction),o->time,o->duration,goal_x,goal_z,best_distance,planned_time);}
   severing_station_last_windup=float(o->time);
   if(best_distance<=.03)return;
   const double dx=goal_x-cam.x,dz=goal_z-cam.z,side=dx*std::cos(cam.yaw)+dz*std::sin(cam.yaw),forward=dx*std::sin(cam.yaw)-dz*std::cos(cam.yaw);
   axis(SDL_CONTROLLER_AXIS_LEFTX,Sint16(std::lround(std::clamp(side/best_distance,-1.,1.)*32767)));axis(SDL_CONTROLLER_AXIS_LEFTY,Sint16(std::lround(std::clamp(-forward/best_distance,-1.,1.)*32767)));++severing_navigation_frames;severing_navigation_last=frame;return;
  }
  if(severing_free_attack&&severing_free_parry&&severing_free_block){if(!check(world_combat_motion_anatomy_stats(combat_motion).mirrored_draws>severing_before_mirror,"surviving left hand really attacks parries and blocks with mirrored rendering"))return;neutral();capture("left-hand-free-ai");++severing_case;next(10);return;}
  return;
 }
 if(severing_phase==80){
  neutral();if(arena_game.state()!=world_arena_combat::State::Won||arena_ui_phase!=ArenaUiPhase::Results||arena_reward_pending)return;
  const auto stats=world_combat_motion_anatomy_stats(combat_motion);bool all=true;for(unsigned p=0;p<8;++p)all=all&&stats.launches[p]>0&&stats.detached_draws[p]>0&&stats.active[p]>0&&stats.settled[p]==stats.active[p];
  if(!all&&age<600)return;
  world_player_profile::State saved;std::string error;const bool loaded=world_player_profile::load(player_profile_path,saved,error)==world_arena_progress::Load::Loaded;
  if(!check(loaded&&arena_game.defeated()==8&&saved.arena.balance==severing_purse+80&&saved.arena.wins==severing_wins+1&&arena_saved_reward==80,"eight real defeats award and persist exactly eighty private copper"))return;
  if(!check(severing_stain_saved&&severing_stain_anchored&&frame>severing_stain_frame+300&&world_blood_stats(blood_renderer).persistent_stains_drawn>0,"earlier persistent blood stain stays anchored and rendered"))return;
  if(!check(all&&stats.cap_draws>0&&stats.maximum_ground_penetration<=.03,"all eight real body parts launch draw and retain bounded floor contact"))return;
  capture("severing-victory");next(81);return;
 }
 if(severing_phase==81){if(age<90)return;world_player_profile::State saved;std::string error;if(!check(world_player_profile::load(player_profile_path,saved,error)==world_arena_progress::Load::Loaded&&saved.arena.balance==severing_purse+80&&saved.arena.wins==severing_wins+1,"Results does not award the same victory twice"))return;severing_passed=true;launch->frames=unsigned(frame+30);std::printf("[Severing QA] COMPLETE %u checks %zu cases\n",severing_checks,severing_results.size());return;}
};
auto severing_capture=[&](){
 // The bounded lossless recorder retires BMPs asynchronously. Never produce
 // a third raw frame while its two-frame (8 MiB) queue is occupied. This
 // affects opt-in QA wall time only, not simulation frames or capture spacing.
 auto capture_slot=[&](){
  if(!severing_qa)return true;
  const Uint32 began=SDL_GetTicks();
  for(;;){
   std::error_code error;unsigned pending=0;
   const auto root=std::filesystem::path(arena_qa_capture_dir);
   for(std::filesystem::directory_iterator it(root,error),end;!error&&it!=end;it.increment(error))
    if(it->path().extension()==".bmp")++pending;
   if(error||SDL_GetTicks()-began>=5000){
    std::fprintf(stderr,"[Reaction capture] Bounded recorder queue wait failed: %s\n",error?error.message().c_str():"consumer timeout");
    severing_failed=true;launch->exit_code=1;running=false;return false;
   }
   if(pending<2)return true;
   SDL_Delay(1);
  }
 };
 // A separate animation sequence observes the real rendered head-cut collapse.
 // It never changes the inherited 71 assertions/28 named combat captures.
 if(severing_qa&&reaction_sequence_enabled&&reaction_sequence_start&&reaction_sequence_captures<20&&severing_frame>=reaction_sequence_start&&
    severing_frame-reaction_sequence_start==std::uint64_t(reaction_sequence_captures)*6){
  if(!capture_slot())return;
  const unsigned index=reaction_sequence_captures;char stem[64];std::snprintf(stem,sizeof stem,"reaction-head-%02u",index);
  const auto root=std::filesystem::path(arena_qa_capture_dir);bool ok=display_w==1280&&display_h==720;
  std::vector<unsigned char> pixels(std::size_t(display_w)*display_h*4);GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
  glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);
  glReadPixels(0,0,display_w,display_h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);ok=ok&&glGetError()==GL_NO_ERROR;
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,display_w,display_h,32,SDL_PIXELFORMAT_RGBA32);ok=ok&&surface;
  if(ok){
   for(int y=0;y<display_h;++y)std::memcpy(static_cast<unsigned char*>(surface->pixels)+std::size_t(y)*surface->pitch,pixels.data()+std::size_t(display_h-1-y)*display_w*4,std::size_t(display_w)*4);
   ok=SDL_SaveBMP(surface,(root/(std::string(stem)+".bmp")).u8string().c_str())==0;
   const auto pose=world_combat_motion_death_pose(combat_motion,3);
   const auto physics=world_combat_motion_anatomy_stats(combat_motion);const auto death=world_combat_motion_death_stats(combat_motion);
   std::ofstream state(root/(std::string(stem)+".reaction.json"));state.imbue(std::locale::classic());
   state<<std::setprecision(17)<<"{\"name\":"<<world_json::quote(stem)<<",\"frame\":"<<severing_frame<<",\"index\":"<<index<<",\"actor_id\":3,\"requested_view\":"<<world_json::quote(severing_want_third?"third":"first")<<",\"actual_view\":"<<world_json::quote(third_person?"third":"first")
    <<",\"pose_slot\":"<<pose.slot<<",\"pose_blend\":"<<pose.weight<<",\"defeat_phase\":"<<pose.phase<<",\"death\":{\"valid\":"<<(pose.valid?"true":"false")<<",\"bridging\":"<<(pose.bridging?"true":"false")<<",\"alpha\":"<<pose.alpha<<",\"source_slot\":"<<pose.source_slot<<",\"source_weight\":"<<pose.source_weight<<",\"yaw\":"<<pose.yaw<<','<<combat_reaction_death_fields(death)<<"},\"physics\":{"<<combat_reaction_physics_fields(physics)<<"}}\n";
   state.flush();ok=ok&&bool(state)&&pose.valid;
  }
  if(surface)SDL_FreeSurface(surface);
  if(!ok){severing_failed=true;launch->exit_code=1;running=false;}else ++reaction_sequence_captures;
 }
 if(!severing_qa||severing_capture_pending.empty())return;
 const auto name=severing_capture_pending;severing_capture_pending.clear();
 // Existing capture code samples the real rendered back buffer and actual VFX.
 // Only the new report is interpreted as this rehearsal's proof.
 if(!arena_qa_capture_pending.empty()){if(!capture_slot())return;arena_qa_capture();}
 if(std::find(arena_qa_capture_names.begin(),arena_qa_capture_names.end(),name)==arena_qa_capture_names.end())return;
 severing_capture_names.push_back(name);const auto s=world_combat_motion_anatomy_stats(combat_motion);
 std::ofstream out(std::filesystem::path(arena_qa_capture_dir)/(name+".severing.json"));out.imbue(std::locale::classic());
 out<<std::setprecision(17)<<"{\"name\":"<<world_json::quote(name)<<",\"frame\":"<<severing_frame<<",\"case\":"<<severing_case<<",\"requested_view\":"<<world_json::quote(severing_want_third?"third":"first")<<",\"actual_view\":"<<world_json::quote(third_person?"third":"first")<<",\"fixture_scope\":\"geometric setup; injuries only real SDL contacts\",\"body_draws\":"<<s.body_draws<<",\"cap_draws\":"<<s.cap_draws<<",\"mirrored_draws\":"<<s.mirrored_draws<<",\"maximum_ground_penetration\":"<<s.maximum_ground_penetration<<",\"parts\":[";
 for(unsigned i=0;i<8;++i){if(i)out<<',';out<<"{\"part\":"<<i+1<<",\"launches\":"<<s.launches[i]<<",\"active\":"<<s.active[i]<<",\"settled\":"<<s.settled[i]<<",\"draws\":"<<s.detached_draws[i]<<'}';}out<<"],\"opponents\":[";
 for(unsigned i=0;i<8;++i){if(i)out<<',';const auto *o=arena_game.opponent(i);out<<"{\"id\":"<<i<<",\"health\":"<<(o?o->health:0)<<",\"severed_parts\":"<<(o?o->severed_parts:0)<<",\"weapon_hand\":"<<(o?unsigned(o->weapon_hand):0)<<'}';}out<<"]}\n";out.flush();if(!out){severing_failed=true;launch->exit_code=1;running=false;}
};
auto severing_report=[&](FILE *f){
 std::fprintf(f,"  \"severing_qa\": {\"enabled\":%s,\"passed\":%s,\"requested_view\":\"%s\",\"actual_view\":\"%s\",\"view_switches\":%u,\"phase\":%u,\"checks\":%u,\"events\":%u,\"captures\":%zu,\"reaction_captures\":%u,\"fixture_player_pose_writes\":%llu,\"fixture_enemy_pose_holds\":%llu,\"fixture_search_queries\":%llu,\"ai_free_frames\":%llu,\"source_contract\":\"Reviewed: no health/injury/death/reward assignments; geometric fixture is not ordinary play proof\",\"result_pause_limit\":\"Won Results advances corpse visuals only; every active detached piece must settle\",\"check_labels\":[",severing_qa?"true":"false",severing_passed?"true":"false",severing_want_third?"third":"first",third_person?"third":"first",severing_view_switches,severing_phase,severing_checks,severing_events,severing_capture_names.size(),reaction_sequence_captures,severing_player_pose_writes,severing_enemy_pose_holds,severing_search_queries,severing_ai_free_frames);
 for(unsigned i=0;i<severing_labels.size();++i)std::fprintf(f,"%s%s",i?",":"",world_json::quote(severing_labels[i]).c_str());std::fprintf(f,"],\"capture_names\":[");for(unsigned i=0;i<severing_capture_names.size();++i)std::fprintf(f,"%s%s",i?",":"",world_json::quote(severing_capture_names[i]).c_str());std::fprintf(f,"],\"case_results\":[");
 for(unsigned i=0;i<severing_results.size();++i){const auto &r=severing_results[i];std::fprintf(f,"%s{\"name\":%s,\"opponent\":%u,\"weapon\":%u,\"direction\":%u,\"expected_part\":%u,\"actual_part\":%u,\"severed_part\":%u,\"mask_before\":%u,\"mask_after\":%u,\"hp_before\":%.9g,\"hp_after\":%.9g,\"attack_serial\":%llu,\"callback_serial\":%llu,\"weapon_hand\":%u,\"point\":[%.17g,%.17g,%.17g],\"weapon_contact\":%s,\"projectile\":%s,\"fixture\":true}",i?",":"",world_json::quote(severing_cases[r.index].name).c_str(),r.opponent,r.weapon,r.direction,r.expected_part,r.actual_part,r.severed_part,r.mask_before,r.mask_after,r.hp_before,r.hp_after,(unsigned long long)r.attack_serial,(unsigned long long)r.callback_serial,r.weapon_hand,r.point[0],r.point[1],r.point[2],r.weapon_contact?"true":"false",r.projectile?"true":"false");}std::fprintf(f,"],\"neutral_gate_wait_frames\":%llu,\"focus_wait_frames\":%llu,\"gate_diagnostic_lines\":%u,\"signal_requests\":%u,\"signal_commits\":%u,\"free_attack_observed\":%s,\"free_parry_observed\":%s,\"free_block_observed\":%s,\"station_queries\":%llu,\"station_plans\":%u,\"navigation_frames\":%llu,\"steel_attack_requests\":%u,\"steel_attack_commits\":%u},\n",severing_gate_wait_frames,severing_focus_wait_frames,severing_gate_logs,severing_signal_requests,severing_signal_commits,severing_free_attack?"true":"false",severing_free_parry?"true":"false",severing_free_block?"true":"false",severing_station_queries,severing_station_plans,severing_navigation_frames,severing_steel_requests,severing_steel_commits);
};
