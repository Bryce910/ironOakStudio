// Marked scene fixtures only; production geometry/core publish every outcome.
const char *blade_clash_qa_dir=std::getenv("VALWOOD_QA_BLADE_CLASH_DIR");
if(blade_clash_qa){std::ifstream marker(".valwood-blade-clash-qa");std::string token;std::getline(marker,token);
 if(token!="VALWOOD_PRIVATE_BLADE_CLASH_V1"||!blade_clash_qa_dir||!*blade_clash_qa_dir){std::fprintf(stderr,"[Blade clash QA] Invalid private marker/output\n");return 1;}}
std::string clash_player_id="knight01";
// Optional selected source player for privately marked native material checks.
// Public play and existing diagnostic runs retain their original default.
if(blade_clash_qa)if(const char *id=std::getenv("VALWOOD_QA_CLASH_PLAYER_ID"))clash_player_id=id;
struct BladeClashCase {std::string enemy;unsigned weapon=0,mode=0;bool first=false;int matching=-1;};
// Modes: air, outgoing steel, body, incoming block, parry, fixed matching guard.
std::vector<BladeClashCase> clash_cases;
for(const auto *enemy:{"knight01","gnur"})for(unsigned mode:{1u,3u})for(bool fp:{false,true})clash_cases.push_back({enemy,0,mode,fp});
for(unsigned mode:{0u,2u})for(bool fp:{false,true})clash_cases.push_back({"gnur",mode==2?4u:0u,mode,fp});
for(bool fp:{false,true})clash_cases.push_back({"gnur",0,4,fp});
for(unsigned weapon:{2u,3u,4u})for(bool fp:{false,true})clash_cases.push_back({"gnur",weapon,1,fp});
for(const auto *enemy:{"knight01","gnur"})for(unsigned direction=0;direction<5;++direction)clash_cases.push_back({enemy,0,5,direction%2==0,int(direction)});
unsigned clash_case_begin=0,clash_case_end=unsigned(clash_cases.size());
if(blade_clash_qa){
 const char *first=std::getenv("VALWOOD_QA_CLASH_FIRST_CASE"),*last=std::getenv("VALWOOD_QA_CLASH_LAST_CASE");
 if(first||last){
  auto index=[](const char *text,unsigned &value){if(!text||!*text)return false;value=0;for(const char *p=text;*p;++p){if(*p<'0'||*p>'9'||value>30)return false;value=value*10+unsigned(*p-'0');}return value<=30;};
  if(!index(first,clash_case_begin)||!index(last,clash_case_end)||clash_case_begin>=clash_case_end||clash_case_end>clash_cases.size()){
   std::fprintf(stderr,"[Blade clash QA] Invalid private diagnostic case range\n");return 1;
  }
 }
}
unsigned clash_index=clash_case_begin,clash_phase=0,clash_guard=0,clash_captures=0,clash_outcomes=0;
std::uint64_t clash_start=0,clash_ready_frame=0,clash_impact=0,clash_serial=0,clash_audio=0,clash_ma=0,clash_ba=0,clash_haptic=0;
unsigned clash_metal=0,clash_body=0;double clash_distance=1.4,clash_predicted=.36;
TrainingDirection clash_direction=TrainingDirection::Left,clash_enemy_guard=TrainingDirection::Left;
bool clash_done=false,clash_parry_edge=false;std::string clash_pending,clash_row;std::vector<std::string> clash_rows;
std::vector<unsigned char> clash_previous;int clash_previous_w=0,clash_previous_h=0;
bool clash_unblocked_body_threat=false;unsigned clash_unblocked_probes=0;
std::vector<std::string> clash_trace;unsigned clash_trace_dropped=0;
if(blade_clash_qa){
 const auto production_query=combat_anatomical_query;
 combat_anatomical_query=[&,production_query](const TrainingAnatomicalQuery &query,TrainingAnatomicalHit &hit){
  const bool result=production_query(query,hit);
  // Probe exactly the same timed source/body geometry without the held edge.
  // Return the untouched production result to the core in every case.
  if(clash_phase==2&&clash_index<clash_cases.size()&&clash_cases[clash_index].mode==5&&query.incoming){
   if(clash_trace.size()<256){
    const auto &target=guard_target_cache[query.opponent_id];const auto &geometry=incoming_pose_cache;const auto stats=skeletal_hero.stats();
    const auto *npc=arena_game.opponent(query.opponent_id);float facing=0;world_combat_motion_facing(arena_opponent_motion(query.opponent_id),query.opponent_id+1,facing);
    std::ostringstream trace;trace<<std::setprecision(12)<<"{\"stage\":\"query\",\"clock\":"<<query.contact_clock<<",\"pose_clock\":"<<skeletal_contact_pose_clock<<",\"begin\":"<<query.phase_begin<<",\"end\":"<<query.phase_end
     <<",\"direction\":"<<unsigned(query.direction)<<",\"guard\":"<<unsigned(query.defender_guard)<<",\"held\":"<<(fight.guard?"true":"false")<<",\"guard_age\":"<<fight.guard_age<<",\"windup\":"<<query.windup_seconds
     <<",\"npc_phase\":"<<(npc?int(npc->phase):-1)<<",\"npc_time\":"<<(npc?npc->time:-1)<<",\"npc_yaw\":"<<facing<<",\"hero_yaw\":"<<geometry.yaw
     <<",\"result\":"<<(result?"true":"false")<<",\"valid\":"<<(hit.valid?"true":"false")<<",\"steel\":"<<(hit.weapon_contact?"true":"false")<<",\"part\":"<<unsigned(hit.part)<<",\"source_phase\":"<<hit.attack_phase
     <<",\"hit_point\":["<<hit.point[0]<<','<<hit.point[1]<<','<<hit.point[2]<<"],\"hit_relative_to_player\":["<<hit.point[0]-cam.x<<','<<hit.point[1]-(cam.y-1.75)<<','<<hit.point[2]-cam.z
     <<"],\"target_ready\":"<<(target.ready?"true":"false")<<",\"target_adapt\":"<<(target.adapt?"true":"false")<<",\"target_clock\":"<<target.clock<<",\"target\":["<<target.target[0]<<','<<target.target[1]<<','<<target.target[2]
     <<"],\"predicted_steel_phase\":"<<target.steel_phase<<",\"predicted_body_phase\":"<<target.body_phase<<",\"guard_weight\":"<<stats.guard<<",\"adjustment_m\":"<<stats.guard_adjustment_m<<",\"residual_m\":"<<stats.guard_target_residual_m
     <<",\"surface_triangles\":"<<geometry.surfaces_begin.size()<<",\"edges\":[";
    for(unsigned i=0;i<geometry.edge_count;++i){if(i)trace<<',';trace<<'[';for(unsigned k=0;k<3;++k)trace<<(k?",":"")<<geometry.edges[i].base[k];for(unsigned k=0;k<3;++k)trace<<','<<geometry.edges[i].tip[k];trace<<']';}
    trace<<"]}";clash_trace.push_back(trace.str());
   }else ++clash_trace_dropped;
   auto unblocked=query;unblocked.weapon_guard=false;TrainingAnatomicalHit body;
   ++clash_unblocked_probes;
   if(production_query(unblocked,body)&&body.valid&&!body.weapon_contact)clash_unblocked_body_threat=true;
  }
  return result;
 };
}
auto clash_name=[&](){const auto &c=clash_cases[clash_index];const char *w[]={"sword","bow","shield","axe","staff"},*m[]={"air","clash","body","guard","parry","matching"},*d[]={"left","right","high","low","thrust"};return c.enemy+"-"+w[c.weapon]+"-"+m[c.mode]+(c.matching>=0?std::string("-")+d[c.matching]:"")+(c.first?"-first":"-third");};
auto clash_write_trace=[&](){
 if(!clash_phase||clash_index>=clash_cases.size()||clash_trace.empty())return;
 std::ofstream trace(std::filesystem::path(blade_clash_qa_dir)/(clash_name()+"-incoming-trace.json"));
 trace<<"{\"schema\":\"valwood.blade-clash.incoming-native-trace.v1\",\"case\":"<<world_json::quote(clash_name())<<",\"dropped\":"<<clash_trace_dropped<<",\"production_result_unchanged\":true,\"samples\":[";
 for(unsigned i=0;i<clash_trace.size();++i)trace<<(i?",":"")<<clash_trace[i];trace<<"]}\n";
};
auto clash_finish=[&](bool passed,const std::string &error){
 clash_write_trace();
 clash_done=true;std::ofstream out(std::filesystem::path(blade_clash_qa_dir)/"NATIVE_CLASH_RAW.json");
 out<<"{\"schema\":\"valwood.blade-clash.native-scenarios.v1\",\"passed\":"<<(passed?"true":"false")<<",\"error\":"<<world_json::quote(error)
 <<",\"partial_scope\":"<<((clash_case_begin||clash_case_end!=clash_cases.size())?"true":"false")<<",\"first_case\":"<<clash_case_begin<<",\"last_case_exclusive\":"<<clash_case_end<<",\"expected_cases\":"<<(clash_case_end-clash_case_begin)<<",\"reviewed_cases\":"<<clash_rows.size()<<",\"captures\":"<<clash_captures
 <<",\"actual_training_events\":true,\"actual_owned_geometry\":true,\"events_injected\":false,\"requested_player\":"<<world_json::quote(clash_player_id)<<",\"cases\":[";
 for(unsigned i=0;i<clash_rows.size();++i)out<<(i?",":"")<<clash_rows[i];out<<"]}\n";
 if(!out||!passed)launch->exit_code=1;running=false;std::fprintf(stderr,"[Blade clash QA] %s %zu/%zu: %s\n",passed?"PASS":"FAIL",clash_rows.size(),std::size_t(clash_case_end-clash_case_begin),error.c_str());
};
auto clash_place=[&](double distance,TrainingDirection guard,bool recovery=false){return arena_game.fixture_geometry_pose(0,{0,0,distance},3.14159265359f,guard,fight.attack_start_serial,recovery);};
auto clash_search=[&](){
 const auto &c=clash_cases[clash_index];const bool incoming=c.mode>=3;const auto guard=TrainingDirection(clash_guard);
 for(unsigned d=0;d<5;++d)for(unsigned n=0;n<=28;++n){
  const double distance=1.+n*.05;if(!clash_place(distance,guard,c.mode==2))continue;
  const auto t=world_combat_timing::attack(world_weapon_loadout::Weapon(c.weapon),d,false,fight.sword_high_windup_min_seconds);
  TrainingAnatomicalQuery q;q.opponent_id=0;q.weapon=world_weapon_loadout::Weapon(c.weapon);q.direction=TrainingDirection(d);q.attack_serial=fight.attack_start_serial+1;
  q.distance=q.local_z=distance;q.spatial=true;q.incoming=incoming;q.weapon_guard=incoming||guard==q.direction;
  q.defender_guard=q.defender_direction=guard;q.defender_phase=c.mode==2?TrainingEnemyPhase::Recovery:TrainingEnemyPhase::Measure;q.defender_duration=10;
  q.phase_begin=.26;q.phase_end=.60;q.interval_seconds=1./60.;q.contact_clock=fight.clock+1./60.;q.attack_commit_clock=fight.clock;
  q.windup_seconds=incoming?.30:t.windup;q.strike_seconds=incoming?world_combat_timing::incoming_strike(unsigned(q.direction),q.windup_seconds,fight.round):t.strike;q.recovery_seconds=t.recovery;q.combo_link_seconds=t.link;
  TrainingAnatomicalHit hit;if(!combat_anatomical_query(q,hit)||!hit.valid)continue;
  const double canonical=.26+.34*hit.sweep_t,phase=hit.attack_phase>=0?hit.attack_phase:canonical;
  if((c.mode==2?hit.weapon_contact:!hit.weapon_contact)||(c.mode==4&&canonical>.40))continue;
  clash_distance=distance;clash_direction=q.direction;clash_enemy_guard=guard;clash_predicted=phase;return clash_place(distance,guard,c.mode==2);
 }
 return false;
};
auto blade_clash_qa_input=[&](const WorldSkeletalHero::Input &input){
 if(!blade_clash_qa||clash_done||clash_index>=clash_cases.size()||clash_cases[clash_index].mode!=5||clash_phase<1||clash_phase>2)return;
 if(clash_trace.size()>=256){++clash_trace_dropped;return;}
 const auto &target=guard_target_cache[0];const auto stats=skeletal_hero.stats();const auto *npc=arena_game.opponent(0);float facing=0;world_combat_motion_facing(arena_opponent_motion(0),1,facing);
 std::ostringstream trace;trace<<std::setprecision(12)<<"{\"stage\":\"input_after_pose\",\"clock\":"<<fight.clock<<",\"dt\":"<<input.dt<<",\"reset\":"<<(input.reset?"true":"false")<<",\"paused\":"<<(input.paused?"true":"false")
  <<",\"input_target_valid\":"<<(input.guard_target_valid?"true":"false")<<",\"input_direction\":"<<input.guard_target_direction<<",\"input_target\":["<<input.guard_target_world[0]<<','<<input.guard_target_world[1]<<','<<input.guard_target_world[2]
  <<"],\"npc_phase\":"<<(npc?int(npc->phase):-1)<<",\"npc_time\":"<<(npc?npc->time:-1)<<",\"npc_yaw\":"<<facing<<",\"hero_yaw\":"<<input.yaw<<",\"guard_age\":"<<fight.guard_age
  <<",\"target_ready\":"<<(target.ready?"true":"false")<<",\"target_adapt\":"<<(target.adapt?"true":"false")<<",\"target_clock\":"<<target.clock<<",\"target\":["<<target.target[0]<<','<<target.target[1]<<','<<target.target[2]
  <<"],\"predicted_steel_phase\":"<<target.steel_phase<<",\"predicted_body_phase\":"<<target.body_phase<<",\"guard_weight\":"<<stats.guard<<",\"adjustment_m\":"<<stats.guard_adjustment_m<<",\"residual_m\":"<<stats.guard_target_residual_m<<'}';
 clash_trace.push_back(trace.str());
};
auto blade_clash_qa_step=[&](std::uint64_t frame){
 if(!blade_clash_qa||clash_done)return;
 if(frame+20>=std::uint64_t(launch->frames)){clash_finish(false,"Bounded rehearsal timed out");return;}
 if(clash_index>=clash_case_end){arena_show_setup();clash_finish(true,"");return;}
 if(destination_loading||!terrain.view_ready(cam.x,cam.z,false))return;
 const auto &c=clash_cases[clash_index];
 if(clash_phase==0){
  std::error_code ec;std::filesystem::create_directories(blade_clash_qa_dir,ec);if(ec){clash_finish(false,ec.message());return;}
  if(!training_sounds.ready||!arena_player_roster_ready){clash_finish(false,"Actual audio/player roster unavailable");return;}
  const int selected_player=arena_player_roster.index(clash_player_id);
  if(selected_player<0){clash_finish(false,"Requested private clash player is not in the actual knight registry");return;}
  SDL_setenv("VALWOOD_QA_CLASH_ENEMY",c.enemy.c_str(),1);arena_show_setup();arena_player_character_draft=selected_player;
  arena_total_draft=arena_simultaneous_draft=1;arena_weapon_draft=int(c.weapon);arena_opponents_draft=c.enemy=="knight01"?0:1;
  if(!arena_begin()){clash_finish(false,arena_progress_error);return;}
  if(arena_player_character_loaded!=clash_player_id){clash_finish(false,"Requested private clash player identity did not reach the actual renderer");return;}
  if(arena_enemy_appearances.size()!=1||arena_enemy_appearances[0]!=c.enemy){clash_finish(false,"Actual deterministic bank differs");return;}
  cam.x=arena_origin_x;cam.z=arena_origin_z;cam.y=arena_floor+1.75;cam.yaw=3.14159265359f;cam.pitch=-.06f;
  // arena_begin owns the real lower/commit/raise transaction. Clearing
  // its stow request here strands a Sword -> Staff handoff in Lowering.
  third_person=!c.first;avatar_initialized=false;arena_gameplay_ready=true;
  for(unsigned n=0;n<900&&(fight.state!=TrainingState::Fighting||!arena_game.active_count());++n)arena_game.update(1./60.,fight,{0,0,0},0,false);
  if(fight.state!=TrainingState::Fighting||arena_game.active_count()!=1){clash_finish(false,"Ordinary entry failed");return;}
  preview_settings.vibration=true;preview_settings.reduced_motion=false;clash_guard=c.matching>=0?unsigned(c.matching):0;clash_start=frame;clash_ready_frame=0;clash_phase=1;clash_outcomes=0;clash_previous.clear();clash_parry_edge=false;clash_unblocked_body_threat=false;clash_unblocked_probes=0;clash_trace.clear();clash_trace_dropped=0;
 }
 arena_gameplay_ready=true;
 if(frame>clash_start+(animation_polish_qa?300:180)){clash_finish(false,"No bounded outcome/recovery: "+clash_name());return;}
 if(clash_phase==1){
  // Arena setup requests the same asynchronous equipment handoff as public
  // play. Do not ask a Staff contact query of the still-lowering Sword rig,
  // or force guard while the new weapon is being raised. The existing
  // bounded scene timeout still rejects a transaction that never finishes.
  if(unsigned(fight.weapon)!=c.weapon||equipment_change.active()||weapon_sheathed||
     !skeletal_hero.ready()||!skeletal_hero.equipment_combat_ready())return;
  if(!clash_ready_frame)clash_ready_frame=frame;
  sync_skeletal_hero_combat_timing();
  if(!clash_place(c.mode==0?3.8:c.mode==5?1.25:1.4,TrainingDirection(clash_guard),c.mode==2)){clash_finish(false,"Scene placement rejected");return;}
  if(c.mode>=3){fight.choose(TrainingDirection(clash_guard));fight.set_guard(true);}
  if(frame<clash_ready_frame+18)return;
  if(c.mode!=0&&c.mode!=5&&!clash_search()){if(++clash_guard>=5){clash_finish(false,"No actual geometric fixture: "+clash_name());return;}clash_start=clash_ready_frame=frame-6;return;}
  if(!c.mode){clash_distance=3.8;clash_direction=TrainingDirection::Left;clash_enemy_guard=TrainingDirection::High;clash_place(3.8,clash_enemy_guard);}
  if(c.mode==5){clash_distance=1.25;clash_direction=clash_enemy_guard=TrainingDirection(c.matching);clash_predicted=-1;clash_place(1.25,clash_enemy_guard);}
  const auto vfx=combat_vfx.stats();clash_metal=vfx.metal_bursts;clash_body=vfx.body_bursts;
  clash_audio=training_sounds.events;clash_ma=training_sounds.metal_events;clash_ba=training_sounds.body_events;clash_haptic=combat_feedback_haptic_dispatches;
  if(c.mode<3){fight.choose(clash_direction);if(!skeletal_timed_attack()){clash_finish(false,"Ordinary attack rejected");return;}clash_serial=fight.attack_start_serial;
   if(c.mode==1){fight.choose(TrainingDirection((unsigned(clash_direction)+1)%5));if(!fight.attack()){clash_finish(false,"Pre-clash combo rejected");return;}}
  }else{fight.choose(clash_enemy_guard);fight.set_guard(true);if(!arena_game.fixture_attack_pose(0,{0,0,clash_distance},3.14159265359f,clash_direction)){clash_finish(false,"NPC windup rejected");return;}clash_serial=arena_game.opponent(0)->attack_start_serial;}
  clash_phase=2;clash_start=frame;
 }
 if(clash_phase>=2&&c.mode<3&&arena_game.opponent(0)&&arena_game.opponent(0)->health>0)clash_place(clash_distance,clash_enemy_guard,c.mode==2);
 if(clash_phase==2&&c.mode==4&&!clash_parry_edge){const auto *o=arena_game.opponent(0);if(o&&o->time>=.10){fight.choose(clash_enemy_guard);fight.set_guard(false);fight.set_guard(true);clash_parry_edge=true;}}
 if(clash_phase==3&&frame>=clash_impact+3&&clash_pending.empty()){clash_pending=clash_name()+"-recoil";clash_phase=4;}
 if(clash_phase==4&&clash_pending.empty()){
  const auto *o=arena_game.opponent(0);const bool recovered=c.mode<3?fight.player_phase==TrainingPlayerPhase::Ready:
   o&&(o->phase==TrainingEnemyPhase::Measure||(o->phase==TrainingEnemyPhase::Recovery&&o->time>=.28)||(o->phase==TrainingEnemyPhase::Stagger&&o->time>=.65));
  if(recovered){clash_pending=clash_name()+"-recovery";clash_phase=5;}
 }
 if(clash_phase==5&&clash_pending.empty()&&(!animation_polish_clash_hold||!animation_polish_clash_hold(frame))){if(clash_outcomes!=1){clash_finish(false,"Outcome replayed before recovery");return;}
  clash_write_trace();clash_rows.push_back(clash_row);std::fprintf(stderr,"[Blade clash QA] PASS %s\n",clash_name().c_str());++clash_index;clash_phase=0;}
};
auto blade_clash_qa_observe=[&](std::uint64_t frame){
 if(!blade_clash_qa||clash_done||clash_phase<2)return;
 const auto &c=clash_cases[clash_index];const TrainingContactKind expected[]={TrainingContactKind::PlayerMiss,TrainingContactKind::EnemyBlock,TrainingContactKind::PlayerBody,TrainingContactKind::PlayerBlock,TrainingContactKind::PlayerParry,TrainingContactKind::PlayerBlock};
 for(unsigned n=0;n<arena_game.events().count;++n){const auto &e=arena_game.events().values[n];if(e.attack_serial!=clash_serial)continue;
  if((c.mode==1&&e.kind==TrainingContactKind::PlayerBody)||(c.mode>=3&&e.kind==TrainingContactKind::EnemyBody)){clash_finish(false,(clash_outcomes?"Stopped blade leaked body damage: ":"Requested guard did not intercept: ")+clash_name());return;}
  if(e.kind!=expected[c.mode])continue;if(++clash_outcomes!=1||clash_phase!=2){clash_finish(false,"Repeated actual outcome");return;}
  const auto vfx=combat_vfx.stats();const auto metal=vfx.metal_bursts-clash_metal,body=vfx.body_bursts-clash_body;
  const auto ma=training_sounds.metal_events-clash_ma,ba=training_sounds.body_events-clash_ba;
  const auto haptic=combat_feedback_haptic_dispatches-clash_haptic;
  const auto &cue=c.mode<3?combat_feedback.attack_notice():combat_feedback.defense_notice();
  const world_combat_feedback::Kind kinds[]={world_combat_feedback::Kind::Miss,world_combat_feedback::Kind::Blocked,world_combat_feedback::Kind::Body,world_combat_feedback::Kind::Block,world_combat_feedback::Kind::Parry,world_combat_feedback::Kind::Block};
  bool valid=cue.kind==kinds[c.mode]&&!e.guard_broken&&training_sounds.ready;
  if(!c.mode)valid=valid&&!metal&&!body&&!ma&&!ba&&!haptic&&!e.damage&&combat_feedback.camera_impulse()==0;
  else if(c.mode==2)valid=valid&&e.anatomical&&body>0&&ba>0&&!metal&&!ma&&e.damage>0;
  else valid=valid&&e.anatomical&&metal>0&&ma>0&&!body&&!ba&&!e.damage&&haptic>0;
  if(c.mode==1)valid=valid&&!fight.pending_attack&&fight.combo_count==0&&fight.player_phase==TrainingPlayerPhase::Recovery;
  if(c.mode>=3){const auto *o=arena_game.opponent(0);valid=valid&&o&&o->blocked_attack_serial==clash_serial&&o->phase==(c.mode==4?TrainingEnemyPhase::Stagger:TrainingEnemyPhase::Recovery);}
  if(!valid){std::ostringstream error;error<<"Actual feedback differs "<<clash_name()<<" kind="<<unsigned(e.kind)<<" cue="<<unsigned(cue.kind)<<" VFX="<<metal<<"/"<<body<<" audio="<<ma<<"/"<<ba<<" haptic="<<haptic;clash_finish(false,error.str());return;}
  std::ostringstream row;row<<std::setprecision(12)<<"{\"name\":"<<world_json::quote(clash_name())<<",\"enemy\":"<<world_json::quote(c.enemy)<<",\"player\":"<<world_json::quote(arena_player_character_loaded)<<",\"player_registry_identity_verified\":"<<(arena_player_character_loaded==clash_player_id?"true":"false")<<",\"weapon\":"<<c.weapon<<",\"mode\":"<<c.mode<<",\"first_person\":"<<(c.first?"true":"false")
   <<",\"event_serial\":"<<e.serial<<",\"attack_serial\":"<<e.attack_serial<<",\"anatomical\":"<<(e.anatomical?"true":"false")<<",\"damage\":"<<e.damage<<",\"distance\":"<<clash_distance<<",\"direction\":"<<unsigned(clash_direction)<<",\"guard\":"<<unsigned(clash_enemy_guard)
   <<",\"predicted_phase\":"<<clash_predicted<<",\"actual_phase\":"<<(c.mode>=3?arena_game.opponent(0)->blocked_attack_phase:fight.resolved_attack_phase)<<",\"metal_bursts\":"<<metal<<",\"body_bursts\":"<<body<<",\"metal_audio\":"<<ma<<",\"body_audio\":"<<ba<<",\"audio_played\":"<<(training_sounds.events-clash_audio)<<",\"haptic_dispatches\":"<<haptic<<",\"one_outcome_through_recovery\":true,\"combo_cleared\":"<<(fight.pending_attack?"false":"true")
   <<",\"air_from_actual_clear_window\":false,\"unblocked_body_threat\":"<<(clash_unblocked_body_threat?"true":"false")<<",\"unblocked_probe_count\":"<<clash_unblocked_probes<<"}";
  clash_row=row.str();clash_impact=frame;clash_phase=3;clash_pending=clash_name()+"-contact";
 }
 if(c.mode==5&&clash_phase==2){
  const auto *o=arena_game.opponent(0);
  if(o&&o->phase==TrainingEnemyPhase::Recovery){
   const auto vfx=combat_vfx.stats();const auto metal=vfx.metal_bursts-clash_metal,body=vfx.body_bursts-clash_body;
   const auto ma=training_sounds.metal_events-clash_ma,ba=training_sounds.body_events-clash_ba;
   const auto haptic=combat_feedback_haptic_dispatches-clash_haptic;
   if(clash_unblocked_body_threat||!clash_unblocked_probes||metal||body||ma||ba||haptic||fight.health<100){clash_finish(false,"Matching air was not a clear actual unblocked body sweep: "+clash_name());return;}
   std::ostringstream row;row<<"{\"name\":"<<world_json::quote(clash_name())<<",\"enemy\":"<<world_json::quote(c.enemy)<<",\"player\":"<<world_json::quote(arena_player_character_loaded)<<",\"player_registry_identity_verified\":"<<(arena_player_character_loaded==clash_player_id?"true":"false")<<",\"weapon\":0,\"mode\":5,\"first_person\":"<<(c.first?"true":"false")
    <<",\"event_serial\":0,\"attack_serial\":"<<clash_serial<<",\"anatomical\":false,\"damage\":0,\"distance\":1.25,\"direction\":"<<unsigned(clash_direction)<<",\"guard\":"<<unsigned(clash_enemy_guard)
    <<",\"predicted_phase\":-1,\"actual_phase\":-1,\"metal_bursts\":0,\"body_bursts\":0,\"metal_audio\":0,\"body_audio\":0,\"audio_played\":"<<(training_sounds.events-clash_audio)
    <<",\"haptic_dispatches\":0,\"one_outcome_through_recovery\":true,\"combo_cleared\":true,\"air_from_actual_clear_window\":true,\"unblocked_body_threat\":false,\"unblocked_probe_count\":"<<clash_unblocked_probes<<"}";
   clash_row=row.str();clash_outcomes=1;clash_impact=frame;clash_phase=3;clash_pending=clash_name()+"-contact";
  }
 }
};
auto blade_clash_qa_capture=[&](int width,int height){
 if(!blade_clash_qa||clash_done||clash_phase<2)return;
 auto save=[&](const std::string &name,const std::vector<unsigned char>&pixels){
  auto *s=SDL_CreateRGBSurfaceWithFormatFrom(const_cast<unsigned char*>(pixels.data()),width,height,24,width*3,SDL_PIXELFORMAT_RGB24);
  const auto path=std::filesystem::path(blade_clash_qa_dir)/(name+".bmp");const bool ok=s&&SDL_SaveBMP(s,path.c_str())==0;if(s)SDL_FreeSurface(s);if(ok)++clash_captures;return ok;
 };
 std::vector<unsigned char> pixels(std::size_t(width)*height*3);GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
 const std::size_t stride=std::size_t(width)*3;for(int y=0;y<height/2;++y)std::swap_ranges(pixels.data()+y*stride,pixels.data()+(y+1)*stride,pixels.data()+(height-1-y)*stride);
 if(glGetError()!=GL_NO_ERROR){clash_finish(false,"Native readback failed");return;}
 if(!clash_pending.empty()){
  if(clash_pending==clash_name()+"-contact"&&(clash_previous.empty()||clash_previous_w!=width||clash_previous_h!=height||!save(clash_name()+"-before",clash_previous))){clash_finish(false,"Missing previous actual frame");return;}
  if(!save(clash_pending,pixels)){clash_finish(false,"Native capture failed");return;}clash_pending.clear();
 }
 clash_previous=std::move(pixels);clash_previous_w=width;clash_previous_h=height;
};
