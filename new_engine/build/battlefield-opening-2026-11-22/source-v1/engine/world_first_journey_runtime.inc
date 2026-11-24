// Optional battlefield bridge. Root assigns these after its encounter host is
// constructed. Defaults preserve the original camp-only demo and arena routes.
std::function<bool()> battlefield_active_fn,battlefield_sword_owned_fn,battlefield_enemy_visible_fn;
std::function<bool(double,double)> battlefield_can_step_fn;
std::function<void(float,bool)> battlefield_combat_step_fn;
// Must decode the bounded blob against the active authored battlefield config
// and compare its sword flag with checkpoint.inventory's battlefield receipt.
std::function<bool(const world_opening_prologue::Save&,std::string&)> battlefield_checkpoint_validate_fn;
auto battlefield_is_active=[&](){return battlefield_active_fn&&battlefield_active_fn();};
auto battlefield_has_sword=[&](){return battlefield_sword_owned_fn&&battlefield_sword_owned_fn();};
// Production host for the connected opening. Story receipts come only from
// the same combat simulation and physical inputs used by the practice arena.
world_first_journey::State first_journey_pending_state;
bool first_journey_pending_restore=false,first_journey_had_fight=false,first_journey_restored_corpse=false;
bool first_journey_click=false,first_journey_retry_click=false;
double first_journey_enemy_x=0,first_journey_enemy_z=0;
float first_journey_enemy_feet=0,first_journey_banner=0,first_journey_reward_banner=0;
unsigned first_journey_enemy_steps=0,first_journey_live_wins=0,first_journey_live_losses=0;
float first_journey_reward_health_before=0,first_journey_reward_health_after=0;
world_journey_presentation::Notice first_journey_notice;
std::array<unsigned,6> first_journey_notice_events={};
unsigned first_journey_notice_frames=0,first_journey_context_hint_frames=0,first_journey_loot_prompt_suppressed_frames=0;
unsigned first_journey_guidance_frames=0,first_journey_guidance_stage_mask=0;
std::array<unsigned,2> first_journey_camera_preserved_starts={},first_journey_camera_preserved_retries={};
std::function<bool()> first_journey_loot_prompt_available;
std::uint64_t first_journey_fight_token=0;
auto first_journey_outdoors=[&](){
    return first_journey_enabled&&!battlefield_is_active()&&(opening_state.stage()==world_opening_prologue::Stage::MeadowExplore||opening_state.stage()==world_opening_prologue::Stage::Complete);
};
auto first_journey_sync_enemy=[&](){
    if(!first_journey_enabled)return;
    preview_arena_x=first_journey_enemy_x-fight.enemy_x;
    preview_arena_z=first_journey_enemy_z-fight.enemy_z;
    preview_arena_feet=first_journey_enemy_feet;
};
auto first_journey_clear_combat=[&](){
    equipment_cancel_pending();
    fight=WorldTrainingCombat{};live_training_encounter=0;first_journey_had_fight=false;first_journey_restored_corpse=false;first_journey_fight_token=0;
    first_journey_defeated=false;first_journey_retry_down=false;first_journey_retry_click=false;
    sword_stick.reset();combat_vfx.clear();blood_effects.reset();combat_feedback.reset();
    combat_feedback_legacy_serial=combat_feedback_legacy_attack=0;combat_feedback_clock=-1;combat_feedback_was_paused=false;
    training_sounds.mute();preview_pad.stop_rumble();
    const auto &p=first_journey_state.config().encounter;
    first_journey_enemy_x=p.x;first_journey_enemy_z=p.z;first_journey_enemy_feet=walking_height(p.x,p.z,nullptr);
    first_journey_sync_enemy();
    if(first_journey_state.encounter_won()){
        // The checkpoint owns the victory. Recreate its visible fallen actor,
        // including an unsearched body, without replaying damage/reward events.
        fight.state=TrainingState::Won;fight.trail_threat=true;fight.enemy_health=0;fight.enemy_max_health=90;
        first_journey_restored_corpse=true;world_combat_motion_restore_defeated(combat_motion,1);
    }
};
if(first_journey_enabled){
    std::string error;
    if(!first_journey_state.configure(opening_state.config().first_journey,error)){
        std::fprintf(stderr,"[First journey] Invalid quest: %s\n",error.c_str());return 1;
    }
    const auto &c=first_journey_state.config();
    const auto &camp=opening_state.config().arrival_camp;
    if(std::hypot(c.camp.x-camp.center.x,c.camp.z-camp.center.z)>.02||
       std::hypot(c.rowan.x-camp.actors[0].feet.x,c.rowan.z-camp.actors[0].feet.z)>.02||
       std::hypot(c.bram.x-camp.actors[1].feet.x,c.bram.z-camp.actors[1].feet.z)>.02){
        std::fprintf(stderr,"[First journey] Quest travelers do not match visible camp roots\n");return 1;
    }
    std::ifstream people("opening/first-journey.town-people.settings");std::string tag;unsigned version=0;
    WorldTrailCampPlacement actors[3];bool ok=bool(people>>tag>>version)&&tag=="VALWOOD_FIRST_JOURNEY_PEOPLE"&&version==1;
    for(unsigned i=0;ok&&i<3;++i){unsigned id=0,seated=0;auto &a=actors[i];
        ok=bool(people>>tag>>id>>a.x>>a.y>>a.z>>a.yaw>>seated)&&tag=="actor"&&id==i&&seated<=1&&
            std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&std::isfinite(a.yaw)&&std::hypot(a.x-c.town.x,a.z-c.town.z)<12;
        a.seated=seated!=0;
    }
    ok=ok&&bool(people>>tag)&&tag=="end";people>>std::ws;ok=ok&&people.eof();
    if(!ok||std::hypot(actors[0].x-c.town.x,actors[0].z-c.town.z)>.05){std::fprintf(stderr,"[First journey] Invalid visible town greeters\n");return 1;}
    first_journey_townsfolk=world_trail_camp_create("models");char message[256]={};
    if(!first_journey_townsfolk||!world_trail_camp_configure_arrival(first_journey_townsfolk,c.town.x,c.town.y,c.town.z,actors,3)||
       !world_trail_camp_commit(first_journey_townsfolk,true,message,sizeof message)){
        std::fprintf(stderr,"[First journey] Town people unavailable: %s\n",message);return 1;
    }
    world_trail_camp_update(first_journey_townsfolk,scenery,17.f);
    first_journey_clear_combat();
    std::printf("[First journey] A Place at the Fire: visible Bram, Rowan and Hugh; real sword encounter, supplies, reward and town checkpoint\n");
}
first_journey_attach_save=[&](world_opening_prologue::Save &checkpoint){
    if(!first_journey_enabled)return;
    auto saved=first_journey_state.stable_save();
    // Camp completion and its autosave are one host tick ahead of the quest
    // clock. Derive the nested snapshot from the actual committed camp mask.
    if(saved.stage==world_first_journey::Stage::Travelers){
        saved.camp_spoken=checkpoint.camp_spoken;
        if(saved.camp_spoken==world_first_journey::all_travelers)saved.stage=world_first_journey::Stage::Offer;
    }
    checkpoint.first_journey_present=true;checkpoint.first_journey=saved;
};
first_journey_restore_save=[&](const world_opening_prologue::Save &checkpoint,std::string &error){
    if(!first_journey_enabled)return true;
    auto staged=first_journey_state;
    auto saved=checkpoint.first_journey;
    if(!checkpoint.first_journey_present){
        staged.reset();saved=staged.stable_save();saved.camp_spoken=checkpoint.camp_spoken;
        if(saved.camp_spoken==world_first_journey::all_travelers)saved.stage=world_first_journey::Stage::Offer;
    }
    first_journey_pending_restore=false;
    if(!staged.restore(saved,error))return false;
    first_journey_pending_state=std::move(staged);first_journey_pending_restore=true;return true;
};
first_journey_checkpoint_pose=[&](world_opening_prologue::Pose &pose){
    if(!first_journey_enabled||!first_journey_pending_restore)return;
    const auto &state=first_journey_pending_state;const auto &c=state.config();
    if(state.stage()!=world_first_journey::Stage::Road&&state.stage()!=world_first_journey::Stage::RecoverSupplies&&state.stage()!=world_first_journey::Stage::Complete)return;
    world_first_journey::Vec target=state.checkpoint_position();
    if(state.stage()==world_first_journey::Stage::Road)target=c.retry;
    if(state.stage()==world_first_journey::Stage::Complete){target=c.town;target.x+=.6;target.z+=1.9;}
    const float floor=std::max(world_atlas_walk_height(target.x,target.z),0.f);
    pose.feet={target.x,floor,target.z};
    const auto aim=state.stage()==world_first_journey::Stage::Complete?c.town:state.stage()==world_first_journey::Stage::Road||state.stage()==world_first_journey::Stage::RecoverSupplies?c.encounter:c.camp;
    pose.yaw=std::atan2(aim.x-target.x,-(aim.z-target.z));pose.pitch=-.04;
};
first_journey_commit_restore=[&](){
    if(!first_journey_enabled||!first_journey_pending_restore)return;
    first_journey_state=std::move(first_journey_pending_state);first_journey_pending_restore=false;
    first_journey_frame=first_journey_state.sample({cam.x,cam.y-1.75,cam.z});
    first_journey_dialogue_active=false;first_journey_clear_combat();
    first_journey_banner=first_journey_reward_banner=0;first_journey_notice.clear();opening_release();
};
first_journey_reset=[&](){
    if(!first_journey_enabled)return;
    weapon_sheathed=false;first_journey_state.reset();first_journey_frame={};first_journey_dialogue_active=false;
    first_journey_banner=first_journey_reward_banner=0;first_journey_notice.clear();first_journey_clear_combat();
};
auto first_journey_enemy_visible=[&](){
    if(battlefield_is_active())return battlefield_enemy_visible_fn&&battlefield_enemy_visible_fn();
    if(!first_journey_outdoors()||!first_journey_state.weapon_unlocked())return false;
    const auto stage=first_journey_state.stage();
    return (stage==world_first_journey::Stage::Road||stage==world_first_journey::Stage::Encounter||first_journey_had_fight||first_journey_restored_corpse)&&
        std::hypot(cam.x-first_journey_enemy_x,cam.z-first_journey_enemy_z)<75.;
};
auto first_journey_can_step=[&](double x,double z){
    if(battlefield_is_active())return battlefield_can_step_fn&&battlefield_can_step_fn(x,z);
    return !first_journey_enabled||!fight.engaged()||std::hypot(x-first_journey_state.config().encounter.x,z-first_journey_state.config().encounter.z)<=6.;
};
auto first_journey_combat_step=[&](float delta,bool paused){
    if(battlefield_is_active()){if(battlefield_combat_step_fn)battlefield_combat_step_fn(delta,paused);return;}
    if(!first_journey_enabled)return;
    bool moved=false;
    if(!paused&&fight.state==TrainingState::Fighting&&fight.enemy_phase==TrainingEnemyPhase::Measure&&fight.impact_pause<=0){
        const double dx=cam.x-first_journey_enemy_x,dz=cam.z-first_journey_enemy_z,distance=std::hypot(dx,dz);
        // The former2.12m stop relied on abstract damage. Close to the same
        //1.55m contact spacing used by source-geometry NPC duels, so a guarded
        // player can actually be struck while remaining outside body overlap.
        constexpr double contact_spacing=1.55;
        if(distance>contact_spacing){
            const double step=std::min(distance-contact_spacing,1.65*std::clamp(double(delta),0.,.05));
            const double nx=first_journey_enemy_x+dx/distance*step,nz=first_journey_enemy_z+dz/distance*step;
            const float floor=walking_height(nx,nz,nullptr);
            if(std::abs(floor-first_journey_enemy_feet)<.16&&first_journey_can_step(nx,nz)&&
               world_scenery_can_move(scenery,first_journey_enemy_x,first_journey_enemy_z,nx,nz,std::min(floor,first_journey_enemy_feet)+.06f,1.7f,.34f)){
                first_journey_enemy_x=nx;first_journey_enemy_z=nz;first_journey_enemy_feet=floor;moved=true;++first_journey_enemy_steps;
            }
        }
    }
    world_combat_motion_arena_enemy_walk(combat_motion,moved);first_journey_sync_enemy();
};
first_journey_tick=[&](const world_opening_prologue::Input &opening_input){
    if(!first_journey_enabled)return;
    if(battlefield_is_active()){
        // QTE A/E ownership must not leak into the camp or its quest clock.
        first_journey_click=first_journey_retry_click=first_journey_retry_down=false;
        first_journey_dialogue_active=first_journey_defeated=false;
        first_journey_frame.events=world_first_journey::None;
        first_journey_state.release_interaction();return;
    }
    using namespace world_first_journey;
    Input input;input.dt=opening_input.dt;input.player_feet={cam.x,cam.y-1.75,cam.z};
    input.enabled=first_journey_outdoors()&&!opening_state.camp().active();input.paused=opening_input.paused;
    input.focused=preview_pad.focused;input.camp_spoken=opening_state.camp().spoken();
    const bool preserve_battlefield_handoff=opening_battlefield_resume.take_journey(
        !input.enabled||input.paused||!input.focused||destination_loading||inventory_open||show_graphics||preview_screen!=1,
        battlefield_is_active());
    // Space is the prologue's cinematic-skip/jump key. Only Enter, E and
    // controller A may accept quest interactions; A already shares interact.
    input.interact_down=opening_input.interact_down||first_journey_click;input.confirm_down=opening_enter;
    input.cancel_down=opening_input.cancel_down;input.retry_down=first_journey_retry_down||first_journey_retry_click;
    if(first_journey_fight_token){
        auto status=fight.engaged()?CombatStatus::Running:fight.state==TrainingState::Won?CombatStatus::Won:fight.state==TrainingState::Lost?CombatStatus::Lost:fight.state==TrainingState::Cancelled?CombatStatus::Cancelled:CombatStatus::None;
        input.combat={first_journey_fight_token,status};
    }
    first_journey_frame=first_journey_state.tick(input);
    if(first_journey_frame.dialogue&&battlefield_has_sword()){
        const auto line=world_first_journey::dialogue_line(first_journey_frame.dialogue_kind,first_journey_frame.beat,true);
        first_journey_frame.speaker=line.speaker;first_journey_frame.line=line.text;
    }
    first_journey_click=first_journey_retry_click=false;first_journey_retry_down=false;
    const auto events=first_journey_frame.events;
    first_journey_dialogue_active=first_journey_frame.dialogue;first_journey_defeated=first_journey_frame.defeated;
    if(!input.paused){first_journey_banner=std::max(0.f,first_journey_banner-float(input.dt));first_journey_reward_banner=std::max(0.f,first_journey_reward_banner-float(input.dt));}
    first_journey_notice.tick(input.dt,input.paused);
    if(events&(WeaponGranted|EncounterWon|SuppliesRecovered|RewardGranted|Completed)){
        first_journey_notice.events(events);
        ++first_journey_notice_events[unsigned(first_journey_notice.card)];
    }
    if(events&EncounterRequested){
        equipment_cancel_pending();
        weapon_sheathed=false;
        first_journey_fight_token=first_journey_frame.encounter_token;
        if(!fight.engaged())fight.difficulty=preview_settings.difficulty;
        if(!fight.start_threat(preview_settings.story_assist,false)){fight.leave();fight.state=TrainingState::Cancelled;opening_tell("The encounter could not start. Approach the raider to try again.");return;}
        first_journey_had_fight=true;
        const auto &p=first_journey_state.config().encounter;
        first_journey_enemy_x=p.x;first_journey_enemy_z=p.z;first_journey_enemy_feet=walking_height(p.x,p.z,nullptr);first_journey_sync_enemy();
        // Starting a fight must not replace the player's chosen view or aim.
        ++first_journey_camera_preserved_starts[third_person?1:0];
        fight_mouse_x=fight_mouse_y=0;sword_stick.reset();++fight_starts;fight_begin_frame=launch->rendered_frames;fight_end_frame=0;
        opening_tell("Watch the windup. Match your guard, then answer with a different cut.");
    }
    if(events&EncounterWon){++first_journey_live_wins;opening_tell("The road is clear. Search the raider if you wish, then recover Rowan's satchel.");preview_audio.cue(2);}
    if(events&EncounterLost){++first_journey_live_losses;opening_tell("Take a breath. Restart this encounter when you're ready.");}
    if(events&RetryRequested){
        const bool chosen_third_person=third_person;
        auto pose=opening_state.config().meadow_spawn;const auto &p=first_journey_state.config().retry;
        pose.feet={p.x,std::max(world_atlas_walk_height(p.x,p.z),0.f),p.z};pose.yaw=cam.yaw;pose.pitch=cam.pitch;
        if(!opening_place(pose,false)){opening_tell("The encounter retry point is not supported.");return;}
        first_journey_clear_combat();third_person=chosen_third_person;++first_journey_camera_preserved_retries[third_person?1:0];temporal_cut=true;
    }
    if(events&WeaponGranted){weapon_sheathed=false;first_journey_banner=5.5f;opening_tell(battlefield_has_sword()?"Keep the sword you escaped with. I / View: inspect your equipment. Follow the road to the raider.":"Bram's loan received. I / View: inspect your equipment. Follow the road to the raider.");preview_audio.cue(2);}
    if(events&SuppliesRecovered){opening_tell("Provisions recovered. Rowan is waiting by the fire.");preview_audio.cue(2);}
    if(events&RewardGranted){first_journey_reward_banner=7;first_journey_reward_health_before=fight.health;fight.health=fight.stamina=100;first_journey_reward_health_after=fight.health;opening_tell("A hot meal restores your strength. Twelve copper for the road.");preview_audio.cue(3);}
    if(events&Completed){first_journey_banner=8;opening_tell("A place to rest. Hugh has pointed you toward Mara. Your journey has begun.");preview_audio.cue(4);}
    if(input.enabled&&(events&ReleaseControls)){
        const bool resume_only=events==ReleaseControls&&first_journey_frame.controls_enabled&&
            !first_journey_frame.dialogue&&!first_journey_frame.defeated;
        if(preserve_battlefield_handoff&&resume_only)opening_release_actions();
        else opening_release();
        sword_stick.reset();
    }
    if(events&(DialogueOpened|DialogueClosed))temporal_cut=true;
    world_player_inventory::reconcile(player_inventory,first_journey_state.weapon_unlocked(),first_journey_state.supplies_recovered(),first_journey_state.reward_granted());
    if(events&CheckpointReached)opening_save();
    if(demo_presentation_enabled&&(events&Completed)){preview_screen=5;opening_panel.clear();opening_release();}
    if(!first_journey_outdoors()||first_journey_state.stage()==Stage::Travelers)return;
    for(unsigned i=0;i<studio.live.placements.size();++i)if(studio.live.placements[i].alias=="nm_journey_satchel"){
        const unsigned key=WORLD_ADDED_KEY_BASE+i;
        if(key<studio.applied_records.size()&&studio.applied_records[key]!=SIZE_MAX)world_scenery_runtime_active(scenery,studio.applied_records[key],!first_journey_state.supplies_recovered());
    }
    opening_frame.chapter=first_journey_frame.title;opening_frame.objective=first_journey_frame.objective;
    opening_frame.objective_position={first_journey_frame.objective_position.x,first_journey_frame.objective_position.y,first_journey_frame.objective_position.z};
    opening_frame.interaction=first_journey_frame.interaction;opening_frame.interaction_available=first_journey_frame.interaction_available;
    opening_frame.controls_enabled=first_journey_frame.controls_enabled;
    if(first_journey_frame.dialogue){
        opening_frame.camp_actor=first_journey_frame.dialogue_actor<3?first_journey_frame.dialogue_actor:-1;
        const auto &p=first_journey_frame.dialogue_target;const double dx=cam.x-p.x,dz=cam.z-p.z,d=std::max(.1,std::hypot(dx,dz));
        opening_frame.camp_dialogue=true;opening_frame.camera.eye={p.x+dx/d*2.35,p.y+1.60,p.z+dz/d*2.35};
        opening_frame.camera.target={p.x,p.y+1.35,p.z};opening_frame.camera.fov=.86;opening_frame.camera.letterbox=.055;
        // The visual host advances both camps once, after the final dialogue
        // frame is known. A second focus tick here would double the head turn.
    }
};
first_journey_draw=[&](bool loading){
    if(battlefield_is_active())return false;
    if(!first_journey_outdoors()||loading||first_journey_state.stage()==world_first_journey::Stage::Travelers)return false;
    const auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.5f),scale=u*preview_settings.text_scale;
    auto *dl=ImGui::GetForegroundDrawList();const auto ivory=IM_COL32(244,238,224,255),gold=IM_COL32(226,196,130,255);
    const bool pad=preview_pad.using_pad&&preview_pad.get();
    auto text=[&](float x,float y,float size,ImU32 color,const char *s){dl->AddText(ImGui::GetFont(),size*scale,{x,y},color,s);};
    auto center=[&](float y,float size,ImU32 color,const std::string &s){const auto extent=ImGui::GetFont()->CalcTextSizeA(size*scale,10000,0,s.c_str());const float x=(w-extent.x)*.5f;text(x,y,size,color,s.c_str());opening_hud_regions.add({x-6*u,y-4*u,x+extent.x+6*u,y+extent.y+4*u});};
    const std::string confirm=pad?preview_pad.button(SDL_CONTROLLER_BUTTON_A):"E";
    if(first_journey_frame.dialogue){
        text(32*u,32*u,14,gold,first_journey_frame.dialogue_actor==3?"HUGH | THE COPPER LANTERN":first_journey_frame.dialogue_actor==1?"BRAM | THE ROADWARDEN":"ROWAN | THE CAMPFIRE");
        // Anchor to the bottom so wrapped dialogue never grows off-screen.
        ImGui::SetNextWindowPos({w*.5f,h-24*u},ImGuiCond_Always,{.5f,1});
        ImGui::SetNextWindowSize({std::min(w-48*u,900*u),0});
        ImGui::SetNextWindowSizeConstraints({0,0},{w-48*u,h*.45f});ImGui::SetNextWindowBgAlpha(.95f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{26*u,20*u});
        ImGui::Begin("###first-journey-dialogue",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoNavInputs);
        ImGui::SetWindowFontScale(std::max(.85f,1.15f*scale));ImGui::TextColored({.9f,.79f,.55f,1},"%s",first_journey_frame.speaker);ImGui::TextWrapped("%s",first_journey_frame.line);
        ImGui::Spacing();
        const char *action=battlefield_has_sword()&&first_journey_frame.dialogue_kind==world_first_journey::Dialogue::Offer?(first_journey_frame.beat+1==world_first_journey::dialogue_length(first_journey_frame.dialogue_kind)?"Help the camp":"Continue"):world_journey_presentation::dialogue_action(first_journey_frame.dialogue_kind,first_journey_frame.beat);
        if(ImGui::Button(action,{std::max(180*u,ImGui::CalcTextSize(action).x+28*u),38*u}))first_journey_click=true;
        ImGui::TextWrapped("%s: continue   %s: leave   |   %u / %u",confirm.c_str(),pad?preview_pad.button(SDL_CONTROLLER_BUTTON_B):"ESC",first_journey_frame.beat+1,world_first_journey::dialogue_length(first_journey_frame.dialogue_kind));
        ImGui::End();ImGui::PopStyleVar();return true;
    }
    const float objective_wrap=std::min(w-76*u,560*u);
    const float objective_size=17*scale,objective_y=59*u;
    const auto objective_extent=ImGui::GetFont()->CalcTextSizeA(objective_size,10000,objective_wrap,first_journey_frame.objective);
    const float objective_footer=objective_y+objective_extent.y+10*u;
    const auto &goal=first_journey_frame.objective_position;const double dx=goal.x-cam.x,dz=goal.z-cam.z,distance=std::hypot(dx,dz);
    const auto stage=first_journey_state.stage();
    const auto goal_name=world_journey_presentation::heading(world_journey_presentation::destination(stage),dx,dz,cam.yaw);
    char range[180];std::snprintf(range,sizeof range,"%s%s   %.0f m   |   %s",world_journey_presentation::road_departure(stage)?"Follow the road: ":"",goal_name.c_str(),distance,world_currency::format(player_profile_balance()).c_str());
    // Completed chapters omit a stale distance to the last interaction.
    if(first_journey_state.town_reached())std::snprintf(range,sizeof range,"Checkpoint saved   |   %s",world_currency::format(player_profile_balance()).c_str());
    const auto range_extent=ImGui::GetFont()->CalcTextSizeA(12*scale,10000,objective_wrap,range);
    const float rail_y=objective_footer+range_extent.y+15*u,objective_bottom=rail_y+5*u+10*scale+11*u;
    dl->AddRectFilled({22*u,22*u},{std::min(w-22*u,620*u),objective_bottom},IM_COL32(7,15,19,215),5*u);
    opening_hud_regions.add({22*u,22*u,std::min(w-22*u,620*u),objective_bottom});
    text(38*u,34*u,12,gold,first_journey_state.town_reached()?"FIRST CHAPTER COMPLETE":"A PLACE AT THE FIRE");
    dl->AddText(ImGui::GetFont(),objective_size,{38*u,objective_y},ivory,first_journey_frame.objective,nullptr,objective_wrap);
    dl->AddText(ImGui::GetFont(),12*scale,{38*u,objective_footer},gold,range,nullptr,objective_wrap);
    const unsigned step=world_journey_presentation::milestone(stage);
    const float rail_width=std::min(548*u,w-100*u),gap=rail_width/5;
    for(unsigned i=0;i<world_journey_presentation::milestones;++i){
        const float x=38*u+gap*i,y=rail_y;
        const ImU32 c=i<=step?gold:IM_COL32(126,140,144,220);
        dl->AddCircleFilled({x+3*u,y},3*u,c);
        if(i+1<5)dl->AddLine({x+12*u,y},{x+gap-8*u,y},i<step?gold:IM_COL32(91,106,111,190),u);
        text(x+12*u,y+5*u,10,c,world_journey_presentation::milestone_name(i));
    }
    ++first_journey_guidance_frames;first_journey_guidance_stage_mask|=1u<<unsigned(stage);
    // Interaction, reward and completion each own one message. Keep the
    // optional stow reminder out of those moments rather than stacking it.
    if(first_journey_state.weapon_unlocked()&&!fight.engaged()&&!first_journey_frame.defeated&&!first_journey_frame.interaction_available&&!first_journey_state.town_reached()&&first_journey_notice.card==world_journey_presentation::Card::None)center(h-38*u,12,ivory,world_journey_presentation::weapon_action(unsigned(fight.weapon),weapon_sheathed,pad));
    if(first_journey_frame.defeated){
        ImGui::SetNextWindowPos({w*.5f,h*.34f},ImGuiCond_Always,{.5f,0});ImGui::SetNextWindowSize({480*u,0});ImGui::SetNextWindowBgAlpha(.96f);
        ImGui::Begin("###first-journey-defeat",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("TAKE A BREATH");ImGui::Separator();ImGui::TextWrapped("%s",battlefield_has_sword()?"The road is dangerous. Your completed conversations and the sword you escaped with are safe.":"The road is dangerous. Your completed conversations and loaned sword are safe.");
        if(ImGui::Button("RETRY ENCOUNTER",{-1,42*u}))first_journey_retry_click=true;ImGui::SetItemDefaultFocus();
        ImGui::TextWrapped("%s / R: retry   Start / Esc: pause",confirm.c_str());ImGui::End();return true;
    }
    if(fight.engaged()){
        const float left=24*u,top=h-153*u,width=290*u;
        dl->AddRectFilled({left,top},{left+width,top+124*u},IM_COL32(6,13,17,225),5*u);
        char status[80];std::snprintf(status,sizeof status,"HEALTH %.0f   STAMINA %.0f",fight.health,fight.stamina);text(left+14*u,top+12*u,12,ivory,status);
        dl->AddRectFilled({left+14*u,top+36*u},{left+14*u+(width-28*u)*std::clamp(fight.health/100.f,0.f,1.f),top+43*u},IM_COL32(184,67,63,255));
        dl->AddRectFilled({left+14*u,top+49*u},{left+14*u+(width-28*u)*std::clamp(fight.stamina/100.f,0.f,1.f),top+56*u},IM_COL32(104,168,122,255));
        std::snprintf(status,sizeof status,"YOUR ANGLE: %s",world_training_direction_name(fight.selected));text(left+14*u,top+66*u,12,gold,status);
        std::snprintf(status,sizeof status,"RAIDER GUARD: %s",world_training_direction_name(fight.enemy_guard));text(left+14*u,top+88*u,12,ivory,status);
        const float enemy_width=std::min(240*u,w*.25f),enemy_left=(w-enemy_width)*.5f;
        dl->AddRectFilled({enemy_left-12*u,24*u},{enemy_left+enemy_width+12*u,70*u},IM_COL32(6,13,17,215),4*u);
        center(31*u,12,gold,"ROAD RAIDER");
        dl->AddRectFilled({enemy_left,53*u},{enemy_left+enemy_width,60*u},IM_COL32(54,40,39,255));
        dl->AddRectFilled({enemy_left,53*u},{enemy_left+enemy_width*std::clamp(fight.enemy_health/std::max(1.f,fight.enemy_max_health),0.f,1.f),60*u},IM_COL32(184,67,63,255));
        const auto lesson=world_journey_presentation::combat_hint(pad,fight.enemy_phase==TrainingEnemyPhase::Windup,fight.enemy_phase==TrainingEnemyPhase::Stagger||fight.enemy_phase==TrainingEnemyPhase::Recovery,fight.player_phase!=TrainingPlayerPhase::Ready,fight.stamina,world_training_direction_name(fight.enemy_direction));
        const float lesson_width=ImGui::GetFont()->CalcTextSizeA(16*scale,10000,0,lesson.c_str()).x+28*u;
        dl->AddRectFilled({(w-lesson_width)*.5f,h*.19f-7*u},{(w+lesson_width)*.5f,h*.19f+23*u},IM_COL32(7,15,19,225),4*u);
        center(h*.19f,16,gold,lesson);++first_journey_context_hint_frames;
        center(h-30*u,11,ivory,pad?"Right stick: cut  |  LT + direction: guard  |  B: dodge  |  RS click: camera":"Mouse / arrows: direction  |  LMB: cut  |  RMB: guard  |  Space: dodge  |  V: camera");
    }else{
        const bool corpse_prompt=first_journey_loot_prompt_available&&first_journey_loot_prompt_available();
        if(first_journey_frame.interaction_available&&!corpse_prompt)center(h*.74f,18,ivory,"[ "+confirm+" ]  "+first_journey_frame.interaction);
        if(first_journey_frame.interaction_available&&corpse_prompt)++first_journey_loot_prompt_suppressed_frames;
        if(first_journey_notice.card!=world_journey_presentation::Card::None){
            const auto kind=first_journey_notice.card;
            const float width=std::min(w-44*u,598*u),wrap=width-36*u;
            const bool existing_sword=kind==world_journey_presentation::Card::Equipment&&battlefield_has_sword();
            const char *body=existing_sword?(pad?"Your stolen sword is ready. View: inspect your equipment. Follow the road to the raider.":"Your stolen sword is ready. I: inspect your equipment. Follow the road to the raider."):world_journey_presentation::detail(kind,pad);
            const auto body_size=ImGui::GetFont()->CalcTextSizeA(14*scale,10000,wrap,body);
            const auto card=world_journey_presentation::notice_rect(w,u,objective_bottom,body_size.y);
            const float left=card.left,top=card.top;
            dl->AddRectFilled({left,top},{card.right,card.bottom},IM_COL32(7,15,19,225),5*u);
            opening_hud_regions.add(card);
            text(left+18*u,top+13*u,17,gold,existing_sword?"READY FOR THE ROAD":world_journey_presentation::title(kind));
            dl->AddText(ImGui::GetFont(),14*scale,{left+18*u,top+42*u},ivory,body,nullptr,wrap);++first_journey_notice_frames;
        }else if(opening_notice_seconds>0)center(h*.85f,14,gold,opening_notice);
        // Chapter completion already has a timed event card and a saved
        // checkpoint in the persistent objective. No duplicate bottom toast.
    }
    return true;
};
