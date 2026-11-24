// Connected battlefield opening. Story/QTE owns input and checkpoints; ordinary
// sword combat remains authoritative for the three escape opponents.
using BattlefieldStage=world_battlefield_prologue::Stage;
world_battlefield_audio::System battlefield_audio;
bool battlefield_audio_ready=false;
std::ofstream battlefield_trace;
if(const char *path=std::getenv("VALWOOD_OBSERVE_BATTLEFIELD"))if(std::filesystem::path(path).is_absolute()&&std::filesystem::exists("PRIVATE_CAPTURE_PROJECT.json"))battlefield_trace.open(path);
if(battlefield_enabled){std::string error;battlefield_audio_ready=battlefield_audio.init(".",std::getenv("VALWOOD_SOUND_EFFECTS"),error);if(!battlefield_audio_ready)std::fprintf(stderr,"[Battlefield audio] %s\n",error.c_str());}

std::vector<world_blood_effects::Contact> battlefield_blood_queue;
battlefield_blood_queue.reserve(8);
std::function<void(world_battlefield_scene::Frame&)> battlefield_decorate_scene;
std::function<bool()> battlefield_publish_corpses;
std::array<WorldCombatCrowdInstance,3> battlefield_native_corpses={};
std::array<world_battlefield_prologue::Pose,3> battlefield_defeated_poses={};
std::array<bool,3> battlefield_defeated_visible={};
std::array<std::uint16_t,3> battlefield_defeated_parts={};
const std::array<world_battlefield_prologue::Vec,3> battlefield_hostiles={{{-12070,0,5144},{-12042,0,5150},{-12029,0,5156}}};
// Ordinary WorldTrainingCombat cancels a threat beyond its 9m leash.
// Start inside that contract, with room for one full movement/render tick.
constexpr double battlefield_activation_radius=8.25;
unsigned battlefield_wins=0,battlefield_encounters=0,battlefield_deaths=0,battlefield_retries=0;
unsigned battlefield_stab_contacts=0,battlefield_sword_grants=0,battlefield_complete=0;
int battlefield_enemy_index=-1;
bool battlefield_result_counted=false,battlefield_hold_struggle=false,battlefield_stab_blood_pending=false;
double battlefield_result_age=0,battlefield_enemy_x=0,battlefield_enemy_z=0;
float battlefield_enemy_y=0;
std::array<std::uint64_t,8> battlefield_impact_serials={};
auto battlefield_to_opening_pose=[&](const world_battlefield_prologue::Pose &p){
    return world_opening_prologue::Pose{{p.feet.x,p.feet.y,p.feet.z},3.141592653589793-p.yaw,-.04};
};
auto battlefield_reset_combat=[&](){
    first_journey_clear_combat();fight.health=fight.stamina=100;
    battlefield_enemy_index=-1;battlefield_result_counted=false;battlefield_result_age=0;
    battlefield_wins=0;battlefield_defeated_visible.fill(false);battlefield_defeated_parts.fill(0);battlefield_native_corpses={};
};
auto battlefield_sync_enemy=[&](){
    preview_arena_x=battlefield_enemy_x-fight.enemy_x;
    preview_arena_z=battlefield_enemy_z-fight.enemy_z;preview_arena_feet=battlefield_enemy_y;
};
auto battlefield_grant_sword=[&](){
    auto next=player_profile;
    if(!world_player_inventory::grant_battlefield_sword(next.inventory)){
        // Existing provenance is idempotent; it still equips the recovered blade.
        if(!(next.inventory.receipts&world_player_inventory::battlefield_receipt))return false;
    }
    next.inventory.weapon=world_player_inventory::Item::Sword;
    if((next.inventory.weapon!=player_profile.inventory.weapon||next.inventory.receipts!=player_profile.inventory.receipts||next.inventory.counts!=player_profile.inventory.counts)&&!player_profile_commit(next,inventory_message))return false;
    player_inventory=inventory_profile_view();inventory_apply_weapon();weapon_sheathed=false;
    ++battlefield_sword_grants;return true;
};
if(battlefield_enabled){
    std::string error,tag;unsigned version=0;std::ifstream file("opening/battlefield.settings");
    if(!(file>>tag>>version)||tag!="VALWOOD_BATTLEFIELD_OPENING"||version!=1){std::fprintf(stderr,"[Battlefield] Invalid scene marker\n");return 1;}
    file>>std::ws;if(!file.eof()){std::fprintf(stderr,"[Battlefield] Unexpected scene marker data\n");return 1;}
    // All six anchors must agree with the same runtime walk cook as gameplay.
    for(auto *p:{&battlefield_config.arrival,&battlefield_config.ambush,&battlefield_config.before_ambush,&battlefield_config.escape_spawn,&battlefield_config.forest,&battlefield_config.campsite})
        {p->feet.y=world_atlas_walk_height(p->feet.x,p->feet.z);
         const auto *support=structure_support?structure_support.get():structure_baseline_walk;
         float deck=0;if(support&&support->sample(p->feet.x,p->feet.z,p->feet.y+1.,deck))p->feet.y=deck;}
    if(!battlefield_state.configure(battlefield_config,error)){std::fprintf(stderr,"[Battlefield] %s\n",error.c_str());return 1;}
    battlefield_pending_state=battlefield_state;battlefield_frame=battlefield_state.frame();
}
battlefield_active=[&](){return battlefield_enabled&&battlefield_frame.active;};
battlefield_cinematic=[&](){return battlefield_enabled&&battlefield_frame.active&&battlefield_frame.exclusive_input;};
battlefield_sword_owned=[&](){return battlefield_enabled&&battlefield_state.sword_owned();};
battlefield_active_fn=battlefield_active;battlefield_sword_owned_fn=battlefield_sword_owned;
battlefield_attach_save=[&](world_opening_prologue::Save &checkpoint){
    if(!battlefield_enabled)return;
    std::string error;if(!world_battlefield_prologue::encode_save(battlefield_state.save(),battlefield_config,checkpoint.battlefield_progress,error))opening_tell(error);
};
battlefield_checkpoint_validate_fn=[&](const world_opening_prologue::Save &checkpoint,std::string &error){
    world_battlefield_prologue::Save saved;
    if(!battlefield_enabled){error="This build cannot restore a battlefield checkpoint.";return false;}
    if(!world_battlefield_prologue::decode_save(checkpoint.battlefield_progress,battlefield_config,saved,error))return false;
    if(!saved.present)return true;
    const bool receipt=checkpoint.inventory_present&&(checkpoint.inventory.receipts&world_player_inventory::battlefield_receipt);
    if(saved.sword_owned!=receipt||(saved.sword_owned&&!checkpoint.inventory.counts[unsigned(world_player_inventory::Item::Sword)])){
        error="Battlefield sword receipt does not match the checkpoint.";return false;
    }
    if(checkpoint.checkpoint<world_opening_prologue::Checkpoint::MeadowStart){error="Battlefield progress cannot precede transport.";return false;}
    return true;
};
battlefield_restore_save=[&](const world_opening_prologue::Save &checkpoint,std::string &error){
    battlefield_pending_restore=false;if(!battlefield_enabled)return checkpoint.battlefield_progress.empty();
    world_battlefield_prologue::Save saved;if(!world_battlefield_prologue::decode_save(checkpoint.battlefield_progress,battlefield_config,saved,error))return false;
    battlefield_pending_state=battlefield_state;
    if(!battlefield_pending_state.restore(saved,battlefield_pending_frame,error))return false;
    battlefield_pending_restore=true;return true;
};
battlefield_checkpoint_pose=[&](world_opening_prologue::Pose &pose){
    if(battlefield_pending_restore&&battlefield_pending_frame.active)pose=battlefield_to_opening_pose(battlefield_pending_frame.teleport_pose);
};
battlefield_commit_restore=[&](){
    if(!battlefield_pending_restore)return;
    battlefield_pending_restore=false;battlefield_state=std::move(battlefield_pending_state);battlefield_frame=battlefield_pending_frame;
    battlefield_reset_combat();battlefield_wins=battlefield_frame.combat_wins;
    if(battlefield_state.sword_owned()&&!battlefield_grant_sword()){opening_tell(inventory_message);launch->exit_code=1;running=false;}
    battlefield_visible=battlefield_state.stage()!=BattlefieldStage::Inactive;
    battlefield_scene_seconds=20.;temporal_cut=true;
};
battlefield_reset=[&](){
    battlefield_state.reset();battlefield_frame=battlefield_state.frame();battlefield_pending_restore=false;
    battlefield_reset_combat();battlefield_visible=false;battlefield_scene_seconds=0;
    battlefield_impact_serials.fill(0);battlefield_blood_queue.clear();battlefield_stab_blood_pending=false;
};
if(battlefield_enabled)battlefield_start=[&](){
    // The outer checkpoint must already be outdoors. The nested battlefield
    // save overrides its canonical camp placement until the forest is reached.
    auto checkpoint=world_opening_prologue::checkpoint_save(world_opening_prologue::Checkpoint::MeadowStart,opening_state.config());
    checkpoint.room_lights=opening_state.room_lights();std::string error;
    if(!opening_state.restore(checkpoint,error)){opening_tell(error);launch->exit_code=1;running=false;return;}
    battlefield_reset();
    const auto d=preview_settings.difficulty==world_combat_difficulty::Difficulty::Story?world_battlefield_prologue::Difficulty::Easy:
        preview_settings.difficulty>=world_combat_difficulty::Difficulty::Hard?world_battlefield_prologue::Difficulty::Hard:world_battlefield_prologue::Difficulty::Medium;
    battlefield_frame=battlefield_state.start(d);
    if(!opening_place(battlefield_to_opening_pose(battlefield_config.arrival),false)){launch->exit_code=1;running=false;return;}
    battlefield_visible=true;weapon_sheathed=true;current_name="The Broken Field";
    opening_arrival_music_pending=false;opening_save();
    std::printf("[Battlefield] Arrival: dazed → duck → tackle → disarm → escape → forest → camp\n");
};
battlefield_publish_corpses=[&](){
    if(!battlefield_enabled)return true;
    std::array<WorldCombatCrowdInstance,3> retained={};unsigned count=0;
    if(battlefield_visible)for(unsigned i=0;i<battlefield_native_corpses.size();++i)
        if(battlefield_defeated_visible[i]&&int(i)!=battlefield_enemy_index)retained[count++]=battlefield_native_corpses[i];
    return world_combat_motion_crowd(arena_opponent_motion(),retained.data(),count);
};
battlefield_enemy_visible_fn=[&](){return battlefield_frame.active&&battlefield_enemy_index>=0&&(fight.engaged()||fight.state==TrainingState::Won||fight.state==TrainingState::Lost);};
battlefield_can_step_fn=[&](double,double){return battlefield_frame.controls_enabled;};
battlefield_combat_step_fn=[&](float delta,bool paused){
    if(paused||battlefield_frame.stage!=BattlefieldStage::Escape)return;
    const double step_dt=std::clamp(double(delta),0.,.1);
    if(battlefield_enemy_index>=0&&fight.state==TrainingState::Cancelled){
        // Leaving the ordinary threat leash is a retreat, not a victory.
        // Release the selected slot so approaching this undefeated soldier
        // again can start a real encounter instead of leaving a dead clock.
        const float health=fight.health,stamina=fight.stamina;
        fight=WorldTrainingCombat{};fight.health=health;fight.stamina=stamina;
        live_training_encounter=0;battlefield_enemy_index=-1;
        battlefield_result_counted=false;battlefield_result_age=0;
        sword_stick.reset();fight_mouse_x=fight_mouse_y=0;
        world_combat_motion_arena_enemy_walk(arena_opponent_motion(),false);
        return;
    }
    if(battlefield_enemy_index>=0&&fight.state==TrainingState::Won){
        if(!battlefield_result_counted){
            battlefield_result_counted=true;++battlefield_wins;
            auto &p=battlefield_defeated_poses[unsigned(battlefield_enemy_index)];
            p={{battlefield_enemy_x,battlefield_enemy_y,battlefield_enemy_z},std::atan2(cam.x-battlefield_enemy_x,cam.z-battlefield_enemy_z)};
        }
        battlefield_result_age+=step_dt;
        // Let the committed death complete before recycling the combat rig.
        if(battlefield_result_age<2.3)return;
        // Preserve the exact settled native endpoint before actor1 is reused.
        // Stable corpse IDs2..4 share this same compact pose/material bank.
        const unsigned corpse_index=unsigned(battlefield_enemy_index);
        const auto death=world_combat_motion_death_pose(arena_opponent_motion(),1);
        auto &corpse=battlefield_native_corpses[corpse_index];
        corpse.x=battlefield_enemy_x;corpse.y=battlefield_enemy_y;corpse.z=battlefield_enemy_z;
        float rendered_yaw=battlefield_defeated_poses[corpse_index].yaw;
        if(death.valid)rendered_yaw=death.yaw;
        else world_combat_motion_facing(arena_opponent_motion(),1,rendered_yaw);
        corpse.yaw=rendered_yaw;corpse.yielded=true;corpse.defeat_phase=1;
        corpse.decapitated=fight.enemy_decapitated;corpse.severed_parts=fight.enemy_severed_parts;
        corpse.weapon_hand=fight.enemy_weapon_hand;corpse.actor_id=2+corpse_index;
        battlefield_defeated_poses[corpse_index].yaw=rendered_yaw;
        battlefield_defeated_visible[corpse_index]=true;battlefield_defeated_parts[corpse_index]=corpse.severed_parts;
        const float health=fight.health,stamina=fight.stamina;
        fight=WorldTrainingCombat{};fight.health=health;fight.stamina=stamina;live_training_encounter=0;
        battlefield_enemy_index=-1;battlefield_result_counted=false;battlefield_result_age=0;
        sword_stick.reset();fight_mouse_x=fight_mouse_y=0;
    }
    if(battlefield_enemy_index<0&&battlefield_wins<battlefield_hostiles.size()){
        const auto &p=battlefield_hostiles[battlefield_wins];
        if(std::hypot(cam.x-p.x,cam.z-p.z)<battlefield_activation_radius){
            const float health=fight.health,stamina=fight.stamina;
            equipment_cancel_pending();fight.difficulty=preview_settings.difficulty;
            if(!fight.start_threat(preview_settings.story_assist,false,false))return;
            fight.health=health;fight.stamina=stamina;fight.weapon=world_weapon_loadout::Weapon::Sword;
            battlefield_enemy_index=int(battlefield_wins);battlefield_enemy_x=p.x;battlefield_enemy_z=p.z;
            battlefield_enemy_y=walking_height(p.x,p.z,nullptr);battlefield_sync_enemy();
            weapon_sheathed=false;world_combat_motion_restore_defeated(arena_opponent_motion(),0);
            world_combat_motion_arena_enemy_walk(arena_opponent_motion(),false);++battlefield_encounters;
            fight_mouse_x=fight_mouse_y=0;sword_stick.reset();++fight_starts;
            fight_begin_frame=launch->rendered_frames;fight_end_frame=0;
            opening_tell("A soldier bars the escape. Block his blade, change your angle, then strike.");
        }
    }
    bool moved=false;
    if(battlefield_enemy_index>=0&&fight.state==TrainingState::Fighting&&fight.enemy_phase==TrainingEnemyPhase::Measure&&fight.impact_pause<=0){
        const double dx=cam.x-battlefield_enemy_x,dz=cam.z-battlefield_enemy_z,d=std::hypot(dx,dz);
        if(d>1.55){const double step=std::min(d-1.55,2.15*step_dt),nx=battlefield_enemy_x+dx/d*step,nz=battlefield_enemy_z+dz/d*step;
            const float floor=walking_height(nx,nz,nullptr);
            if(std::abs(floor-battlefield_enemy_y)<.30&&world_scenery_can_move(scenery,battlefield_enemy_x,battlefield_enemy_z,nx,nz,std::min(floor,battlefield_enemy_y)+.06f,1.7f,.34f)){
                battlefield_enemy_x=nx;battlefield_enemy_z=nz;battlefield_enemy_y=floor;moved=true;
            }
        }
    }
    if(battlefield_enemy_index>=0){world_combat_motion_arena_enemy_walk(arena_opponent_motion(),moved);battlefield_sync_enemy();}
};
battlefield_tick=[&](float delta,bool paused){
    // Preserve genuine taps shorter than one frame. Read each non-repeat SDL
    // DOWN event once, then discard it even when the story is suspended.
    const bool duck_press=battlefield_duck_pressed,struggle_press=battlefield_struggle_pressed,retry_press=battlefield_retry_pressed;
    battlefield_release_edges();
    if(!battlefield_enabled||!battlefield_frame.active)return;
    const auto *keys=SDL_GetKeyboardState(nullptr);auto *pad=preview_pad.get();
    world_battlefield_prologue::Input input;input.dt=delta;input.player_feet={cam.x,cam.y-1.75,cam.z};
    input.paused=paused||inventory_open;input.focused=preview_pad.focused;input.loading=destination_loading;
    input.uses_controller=preview_pad.using_pad;input.controller_connected=pad&&SDL_GameControllerGetAttached(pad);
    input.duck_down=keys[SDL_SCANCODE_Q]||(pad&&SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_X));
    input.struggle_down=keys[SDL_SCANCODE_F]||(pad&&SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_A));
    input.retry_down=keys[SDL_SCANCODE_RETURN]||keys[SDL_SCANCODE_R]||(pad&&SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_A));
    input.duck_pressed=duck_press;input.struggle_pressed=struggle_press;input.retry_pressed=retry_press;
    input.hold_struggle=battlefield_hold_struggle;input.reduced_motion=preview_settings.reduced_motion;
    input.player_dead=fight.health<=0||fight.state==TrainingState::Lost;
    input.combat_wins=battlefield_wins;input.escape_combat_complete=battlefield_wins>=battlefield_config.required_escape_wins&&battlefield_enemy_index<0&&!fight.engaged();
    const auto previous_stage=battlefield_frame.stage;
    battlefield_frame=battlefield_state.update(input);const auto events=battlefield_frame.events;
    const bool continuous_exploration=world_battlefield_prologue::preserve_exploration_input(previous_stage,battlefield_frame);
    auto release_battlefield_controls=[&](){
        if(continuous_exploration){
            // Crossing a walkable checkpoint must not require releasing W or
            // centering the controller. Flush actions without interrupting the
            // live movement/camera stream or its existing neutral-ready state.
            opening_release_actions();
        }else opening_release();
        sword_stick.reset();fight.clear_buffer();fight.set_guard(false);
    };
    if(events&world_battlefield_prologue::StageChanged)temporal_cut=true;
    if(events&world_battlefield_prologue::ReleaseControls)release_battlefield_controls();
    if(events&world_battlefield_prologue::AmbushStarted){cam.x=battlefield_config.ambush.feet.x;cam.y=battlefield_config.ambush.feet.y+1.75;cam.z=battlefield_config.ambush.feet.z;temporal_cut=true;}
    if(events&world_battlefield_prologue::TeleportPlayer){
        if(!opening_place(battlefield_to_opening_pose(battlefield_frame.teleport_pose),false)){launch->exit_code=1;running=false;return;}
    }
    if(events&world_battlefield_prologue::Died){++battlefield_deaths;training_sounds.mute();}
    if(events&world_battlefield_prologue::Retried){++battlefield_retries;battlefield_reset_combat();weapon_sheathed=!battlefield_state.sword_owned();}
    if(events&world_battlefield_prologue::GrantSword){if(!battlefield_grant_sword()){opening_tell("Could not save the recovered sword. Your checkpoint is safe.");launch->exit_code=1;running=false;return;}}
    if((events&world_battlefield_prologue::EnsureSword)&&!(events&world_battlefield_prologue::GrantSword))if(!battlefield_grant_sword()){opening_tell(inventory_message);launch->exit_code=1;running=false;return;}
    if(events&world_battlefield_prologue::StabContact){
        ++battlefield_stab_contacts;battlefield_stab_blood_pending=true;
        if(preview_settings.vibration)preview_pad.rumble(.55f,.8f,160,true);
    }
    if(events&world_battlefield_prologue::TackleImpact){if(preview_settings.vibration)preview_pad.rumble(.35f,.6f,180,true);}
    if(events&world_battlefield_prologue::StrugglePress)if(preview_settings.vibration)preview_pad.rumble(.06f,.14f,45,true);
    if(events&world_battlefield_prologue::EscapeStarted){
        // Match the cinematic's final crouch/recovery root without resetting
        // its already committed sword, then release ordinary combat controls.
        const auto p=battlefield_to_opening_pose(battlefield_config.escape_spawn);
        cam.x=p.feet.x;cam.y=p.feet.y+1.75;cam.z=p.feet.z;cam.yaw=float(p.yaw);cam.pitch=-.04f;
        cam.grounded=true;cam.vy=0;avatar_initialized=false;weapon_sheathed=false;temporal_cut=true;
    }
    if(events&world_battlefield_prologue::ForestReached){battlefield_enemy_index=-1;current_name="A Light Between the Trees";opening_tell("A fire through the trees. Someone is alive.");}
    if(events&world_battlefield_prologue::CampsiteReached){
        ++battlefield_complete;
        // Keep the battlefield's ground blood until this play session ends.
        // Only the combat owner is handed back; no effects reset at the camp.
        const float remaining_health=fight.health;
        equipment_cancel_pending();fight=WorldTrainingCombat{};fight.health=std::max(35.f,remaining_health);
        live_training_encounter=0;first_journey_had_fight=first_journey_restored_corpse=first_journey_defeated=false;
        first_journey_fight_token=0;first_journey_sync_enemy();sword_stick.reset();training_sounds.mute();
        weapon_sheathed=true;current_name="Smoke in the Meadows";
        // The stage boundary above already released actions with the correct
        // movement policy; do not disarm its still-held exploration input.
        if(!(events&world_battlefield_prologue::ReleaseControls))release_battlefield_controls();
        opening_frame=opening_state.sample({cam.x,cam.y-1.75,cam.z},preview_settings.reduced_motion);
        opening_tell("Travelers by the fire. Put the battle behind you. Speak to Rowan.");
    }
    if(events&world_battlefield_prologue::CheckpointReached)opening_save();
};
battlefield_pause_options=[&](){if(battlefield_enabled)ImGui::Checkbox("Hold A / F instead of repeated taps",&battlefield_hold_struggle);};
battlefield_apply_frame=[&](world_opening_prologue::Frame &frame){
    if(battlefield_enabled&&battlefield_frame.completed&&frame.camp_dialogue&&frame.camp_actor>=0){
        const auto line=world_opening_arrival_camp::line(unsigned(frame.camp_actor),frame.camp_beat,opening_state.camp().repeat(),true);
        frame.camp_speaker=line.speaker;frame.camp_line=line.text;
    }
    if(!battlefield_enabled||!battlefield_frame.active)return;
    const auto &b=battlefield_frame;frame.cinematic=b.cinematic;frame.controls_enabled=b.controls_enabled;
    frame.camp_dialogue=frame.npc_intro=false;frame.camp_actor=-1;frame.interaction_available=false;
    frame.chapter=b.chapter;frame.objective=b.objective;frame.subtitle=b.subtitle;
    auto target=b.objective_position;
    if(b.stage==BattlefieldStage::Escape&&battlefield_wins<battlefield_hostiles.size())target=battlefield_hostiles[battlefield_wins];
    frame.objective_position={target.x,target.y,target.z};
    frame.camera={{b.camera.eye.x,b.camera.eye.y,b.camera.eye.z},{b.camera.target.x,b.camera.target.y,b.camera.target.z},b.camera.fov,b.camera.fade,b.camera.letterbox};
};
battlefield_decorate_scene=[&](world_battlefield_scene::Frame &frame){
    // Show route defenders before their real encounter activates. Their
    // matching appearance hands over to the live rig at the same authored root.
    // The visible soldier never materializes eight metres from the player.
    if(battlefield_frame.active)for(unsigned i=battlefield_wins;i<battlefield_hostiles.size()&&frame.actor_count<frame.actors.size();++i){
        if(int(i)==battlefield_enemy_index)continue;
        const auto &p=battlefield_hostiles[i];auto &a=frame.actors[frame.actor_count++];
        a.x=p.x;a.y=walking_height(p.x,p.z,nullptr);a.z=p.z;
        a.yaw=float(std::atan2(cam.x-p.x,cam.z-p.z));a.motion=WorldBattlefieldActors::Motion::Guard;a.progress=.25;
    }
    // Defeated escape opponents stay in the native compact crowd, preserving
    // their exact death pose, severed parts and remaining weapon hand.
    for(unsigned i=0;i<frame.impact_count;++i){const auto&e=frame.impacts[i];const unsigned slot=e.blood?unsigned(e.serial%4):4+unsigned(e.serial%4);
        if(battlefield_impact_serials[slot]==e.serial)continue;battlefield_impact_serials[slot]=e.serial;
        if(e.blood&&battlefield_blood_queue.size()<8){const auto&p=e.position;const auto&d=e.direction;
            battlefield_blood_queue.push_back({{p.x,p.y,p.z},{d.x,d.y,d.z},world_atlas_walk_height(p.x,p.z)+.02,true,e.strength,false});
        }
    }
};
battlefield_draw_hud=[&](){
    if(!battlefield_enabled||!battlefield_frame.active)return false;
    const auto &b=battlefield_frame;auto &io=ImGui::GetIO();auto *dl=ImGui::GetForegroundDrawList();
    const float w=io.DisplaySize.x,h=io.DisplaySize.y,u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.5f);
    auto center=[&](float y,float size,ImU32 color,const char *text){const float width=ImGui::GetFont()->CalcTextSizeA(size*u,10000,0,text).x;dl->AddText(ImGui::GetFont(),size*u,{(w-width)*.5f,y},color,text);};
    const float bar=b.cinematic?float(b.camera.letterbox)*h:0;
    if(bar>0){dl->AddRectFilled({0,0},{w,bar},IM_COL32(0,0,0,255));dl->AddRectFilled({0,h-bar},{w,h},IM_COL32(0,0,0,255));}
    if(b.camera.fade>0)dl->AddRectFilled({0,0},{w,h},IM_COL32(0,0,0,int(std::clamp(b.camera.fade,0.,1.)*255)));
    const auto white=IM_COL32(245,239,224,255),gold=IM_COL32(230,198,130,255);
    if(!b.cinematic){
        dl->AddRectFilled({22*u,28*u},{620*u,133*u},IM_COL32(12,20,20,210),5*u);
        dl->AddText(ImGui::GetFont(),13*u,{38*u,42*u},gold,b.chapter);
        dl->AddText(ImGui::GetFont(),19*u,{38*u,66*u},white,b.objective);
        char counter[128];if(b.stage==BattlefieldStage::Escape)std::snprintf(counter,sizeof counter,"Escape route cleared: %u / %u",battlefield_wins,b.combat_required);
        else std::snprintf(counter,sizeof counter,"%s",preview_pad.using_pad?"LS: move  |  click LS: run  |  RS: look":"WASD: move  |  Shift: run  |  Mouse: look");
        dl->AddText(ImGui::GetFont(),14*u,{38*u,103*u},white,counter);
    }
    if(!b.cinematic&&b.stage==BattlefieldStage::Escape){
        const float left=32*u,bottom=h-44*u,width=260*u;
        auto meter=[&](float y,float amount,ImU32 color){dl->AddRectFilled({left,y},{left+width,y+7*u},IM_COL32(20,24,24,215),3*u);dl->AddRectFilled({left,y},{left+width*std::clamp(amount,0.f,1.f),y+7*u},color,3*u);};
        meter(bottom,fight.health/100.f,IM_COL32(170,59,46,245));meter(bottom+12*u,fight.stamina/100.f,IM_COL32(200,176,105,245));
        if(fight.engaged()){
            const float bx=(w-350*u)*.5f,by=h-110*u;
            dl->AddRectFilled({bx-15*u,by-35*u},{bx+365*u,by+57*u},IM_COL32(10,17,19,205),5*u);
            center(by-24*u,14,gold,"SOLDIER");
            dl->AddRectFilled({bx,by},{bx+350*u,by+7*u},IM_COL32(46,41,39,240));
            dl->AddRectFilled({bx,by},{bx+350*u*std::clamp(fight.enemy_health/std::max(1.f,fight.enemy_max_health),0.f,1.f),by+7*u},IM_COL32(181,75,52,255));
            const char *feedback=fight.feedback_time>0?fight.feedback:(preview_pad.using_pad?"Right stick: cut  |  LT + right stick: block":"LMB: cut  |  RMB: block  |  Move the mouse to change angle");
            center(by+20*u,15,white,feedback);
        }
    }
    const char *prompt=preview_pad.using_pad?b.prompt:b.keyboard_prompt;
    if(prompt&&*prompt){
        const float y=h*.72f;dl->AddRectFilled({w*.24f,y-15*u},{w*.76f,y+63*u},IM_COL32(8,14,17,225),8*u);
        center(y,30,gold,prompt);
        if(b.stage==BattlefieldStage::Struggle||b.stage==BattlefieldStage::Duck){
            const float p=b.stage==BattlefieldStage::Struggle?float(b.struggle_progress):float(b.duck_remaining/world_battlefield_prologue::duck_seconds(battlefield_config,b.difficulty));
            dl->AddRectFilled({w*.30f,y+45*u},{w*.70f,y+51*u},IM_COL32(60,63,62,255));
            dl->AddRectFilled({w*.30f,y+45*u},{w*.30f+w*.40f*std::clamp(p,0.f,1.f),y+51*u},gold);
        }
    }
    if(b.subtitle&&*b.subtitle)center(h-bar-62*u,22,white,b.subtitle);
    if(b.stage==BattlefieldStage::Dead){center(h*.39f,38,gold,"YOU FELL");center(h*.47f,21,white,b.objective);}
    return true;
};
