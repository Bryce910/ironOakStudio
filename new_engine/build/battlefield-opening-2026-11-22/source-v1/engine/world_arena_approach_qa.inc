// Private marked native first-approach rehearsal. Starts normal matches, waits
// for real walking/guard poses and sends one ordinary overhead input. No actor
// placement, phase, health, guard or contact outcome is forced.
const bool approach_guard_qa=arena_roster_qa&&std::getenv("VALWOOD_QA_ARENA_APPROACH");
unsigned approach_phase=0;bool approach_saw_guard=false;double approach_commit_distance=0;
auto approach_guard_qa_step=[&](std::uint64_t frame){
 if(roster_qa_done)return;
 if(frame+20>=std::uint64_t(launch->frames)){roster_qa_finish(false,"First approach rehearsal timed out");return;}
 if(destination_loading||!terrain.view_ready(cam.x,cam.z,false))return;
 auto name=[&](const char*s){return std::string(roster_qa_match?"orc-":"knight-")+s;};
 if(approach_phase==0){std::error_code ec;std::filesystem::create_directories(arena_roster_qa_dir,ec);if(ec){roster_qa_finish(false,ec.message());return;}arena_show_setup();arena_player_character_draft=arena_player_roster.index("knight04");arena_total_draft=arena_simultaneous_draft=1;arena_teammates_draft=0;arena_difficulty_draft=2;arena_weapon_draft=0;arena_opponents_draft=roster_qa_match?1:0;roster_qa_start=frame;approach_saw_guard=false;approach_phase=1;return;}
 if(approach_phase==1){if(frame<roster_qa_start+3)return;if(!arena_begin()){roster_qa_finish(false,arena_progress_error);return;}third_person=false;arena_gameplay_ready=true;approach_phase=2;roster_qa_start=frame;return;}
 arena_gameplay_ready=true;
 const auto&a=arena_game.challengers()[0];const double dx=arena_origin_x+a.position.x-cam.x,dz=arena_origin_z+a.position.z-cam.z,distance=std::hypot(dx,dz);
 if(approach_phase==2){cam.yaw=3.14159265359f-float(std::atan2(dx,dz));cam.pitch=-.07f;const auto pose=world_combat_motion_pose(arena_opponent_motion(0),1);
  if(a.moving&&a.state==world_arena_combat::ChallengerState::Entering&&distance<4&&distance>2.5&&pose.guard_weight>.95f&&!approach_saw_guard){approach_saw_guard=true;roster_qa_pending=name("guarded-advance");}
  if(fight.state==TrainingState::Fighting&&distance<=1.7){if(!approach_saw_guard){roster_qa_finish(false,"Approaching NPC never displayed a raised walking guard");return;}if(!roster_qa_pending.empty())return;approach_commit_distance=distance;fight.choose(TrainingDirection::High);if(!skeletal_timed_attack()){roster_qa_finish(false,"Ordinary first attack rejected");return;}roster_qa_attack=fight.attack_start_serial;approach_phase=3;roster_qa_start=frame;}
 }else if(approach_phase==3){
  if(fight.resolved_attack_serial==roster_qa_attack){const auto*o=arena_game.opponent(0);if(!o||fight.resolved_attack_outcome!=TrainingAttackOutcome::EnemyBlock||!fight.resolved_attack_anatomical||o->health!=o->max_health){roster_qa_finish(false,"Approach overhead was not stopped by actual source steel");return;}roster_qa_pending=name("first-contact-block");approach_phase=4;roster_qa_start=frame;}
  else if(frame>roster_qa_start+150){roster_qa_finish(false,"First attack did not resolve");return;}
 }else if(approach_phase==4&&frame>=roster_qa_start+16&&roster_qa_pending.empty()){
  const auto stats=world_combat_motion_compact_stats(arena_opponent_motion(0));if(stats.failures||glGetError()!=GL_NO_ERROR){roster_qa_finish(false,"Guarded advance rendering failed");return;}
  std::ostringstream row;row<<"{\"kind\":"<<world_json::quote(roster_qa_match?"Orc":"Knight")<<",\"appearance\":"<<world_json::quote(arena_enemy_appearances[0])<<",\"walking_guard\":true,\"entering_at_commit\":true,\"commit_distance\":"<<approach_commit_distance<<",\"outcome\":\"source-steel-block\",\"NPC_health_unchanged\":true,\"GL_errors\":0,\"compact_failures\":"<<stats.failures<<"}";roster_qa_rows.push_back(row.str());if(++roster_qa_match==2){arena_show_setup();roster_qa_finish(true,"");return;}approach_phase=0;
 }
};
