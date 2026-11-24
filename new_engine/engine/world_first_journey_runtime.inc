// Production host for the connected opening. Story receipts come only from
// the same combat simulation and physical inputs used by the practice arena.
world_first_journey::State first_journey_pending_state;
bool first_journey_pending_restore=false,first_journey_had_fight=false;
bool first_journey_click=false,first_journey_retry_click=false;
double first_journey_enemy_x=0,first_journey_enemy_z=0;
float first_journey_enemy_feet=0,first_journey_banner=0,first_journey_reward_banner=0;
unsigned first_journey_enemy_steps=0,first_journey_live_wins=0,first_journey_live_losses=0;
float first_journey_reward_health_before=0,first_journey_reward_health_after=0;
std::uint64_t first_journey_fight_token=0;
auto first_journey_outdoors=[&](){
    return first_journey_enabled&&(opening_state.stage()==world_opening_prologue::Stage::MeadowExplore||opening_state.stage()==world_opening_prologue::Stage::Complete);
};
auto first_journey_sync_enemy=[&](){
    if(!first_journey_enabled)return;
    preview_arena_x=first_journey_enemy_x-fight.enemy_x;
    preview_arena_z=first_journey_enemy_z-fight.enemy_z;
    preview_arena_feet=first_journey_enemy_feet;
};
auto first_journey_clear_combat=[&](){
    fight=WorldTrainingCombat{};live_training_encounter=0;first_journey_had_fight=false;first_journey_fight_token=0;
    first_journey_defeated=false;first_journey_retry_down=false;first_journey_retry_click=false;
    sword_stick.reset();combat_vfx.clear();blood_effects.reset();combat_contact_kind=0;combat_contact_time=0;
    const auto &p=first_journey_state.config().encounter;
    first_journey_enemy_x=p.x;first_journey_enemy_z=p.z;first_journey_enemy_feet=float(p.y);
    first_journey_sync_enemy();
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
    first_journey_banner=first_journey_reward_banner=0;opening_release();
};
first_journey_reset=[&](){
    if(!first_journey_enabled)return;
    first_journey_state.reset();first_journey_frame={};first_journey_dialogue_active=false;
    first_journey_banner=first_journey_reward_banner=0;first_journey_clear_combat();
};
auto first_journey_enemy_visible=[&](){
    if(!first_journey_outdoors()||!first_journey_state.weapon_unlocked())return false;
    const auto stage=first_journey_state.stage();
    return (stage==world_first_journey::Stage::Road||stage==world_first_journey::Stage::Encounter||first_journey_had_fight)&&
        std::hypot(cam.x-first_journey_enemy_x,cam.z-first_journey_enemy_z)<75.;
};
auto first_journey_can_step=[&](double x,double z){
    return !first_journey_enabled||!fight.engaged()||std::hypot(x-first_journey_state.config().encounter.x,z-first_journey_state.config().encounter.z)<=6.;
};
auto first_journey_combat_step=[&](float delta,bool paused){
    if(!first_journey_enabled)return;
    bool moved=false;
    if(!paused&&fight.state==TrainingState::Fighting&&fight.enemy_phase==TrainingEnemyPhase::Measure&&fight.impact_pause<=0){
        const double dx=cam.x-first_journey_enemy_x,dz=cam.z-first_journey_enemy_z,distance=std::hypot(dx,dz);
        if(distance>2.12){
            const double step=std::min(distance-2.12,1.65*std::clamp(double(delta),0.,.05));
            const double nx=first_journey_enemy_x+dx/distance*step,nz=first_journey_enemy_z+dz/distance*step;
            const float floor=std::max(world_atlas_walk_height(nx,nz),0.f);
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
    using namespace world_first_journey;
    Input input;input.dt=opening_input.dt;input.player_feet={cam.x,cam.y-1.75,cam.z};
    input.enabled=first_journey_outdoors()&&!opening_state.camp().active();input.paused=opening_input.paused;
    input.focused=preview_pad.focused;input.camp_spoken=opening_state.camp().spoken();
    // Space is the prologue's cinematic-skip/jump key. Only Enter, E and
    // controller A may accept quest interactions; A already shares interact.
    input.interact_down=opening_input.interact_down||first_journey_click;input.confirm_down=opening_enter;
    input.cancel_down=opening_input.cancel_down;input.retry_down=first_journey_retry_down||first_journey_retry_click;
    if(first_journey_fight_token){
        auto status=fight.engaged()?CombatStatus::Running:fight.state==TrainingState::Won?CombatStatus::Won:fight.state==TrainingState::Lost?CombatStatus::Lost:fight.state==TrainingState::Cancelled?CombatStatus::Cancelled:CombatStatus::None;
        input.combat={first_journey_fight_token,status};
    }
    first_journey_frame=first_journey_state.tick(input);first_journey_click=first_journey_retry_click=false;first_journey_retry_down=false;
    const auto events=first_journey_frame.events;
    first_journey_dialogue_active=first_journey_frame.dialogue;first_journey_defeated=first_journey_frame.defeated;
    if(!input.paused){first_journey_banner=std::max(0.f,first_journey_banner-float(input.dt));first_journey_reward_banner=std::max(0.f,first_journey_reward_banner-float(input.dt));}
    if(events&EncounterRequested){
        first_journey_fight_token=first_journey_frame.encounter_token;
        if(!fight.start_threat(preview_settings.story_assist,false)){fight.leave();fight.state=TrainingState::Cancelled;opening_tell("The encounter could not start. Approach the raider to try again.");return;}
        first_journey_had_fight=true;
        const auto &p=first_journey_state.config().encounter;
        first_journey_enemy_x=p.x;first_journey_enemy_z=p.z;first_journey_enemy_feet=float(p.y);first_journey_sync_enemy();
        cam.yaw=float(std::atan2(p.x-cam.x,-(p.z-cam.z)));cam.pitch=-.12f;third_person=true;avatar_initialized=false;temporal_cut=true;
        fight_mouse_x=fight_mouse_y=0;sword_stick.reset();++fight_starts;fight_begin_frame=launch->rendered_frames;fight_end_frame=0;
        opening_tell("Watch the windup. Match your guard, then answer with a different cut.");
    }
    if(events&EncounterWon){++first_journey_live_wins;opening_tell("The road is clear. Find Rowan's provision satchel.");preview_audio.cue(3);}
    if(events&EncounterLost){++first_journey_live_losses;opening_tell("Take a breath. Restart this encounter when you're ready.");}
    if(events&RetryRequested){
        auto pose=opening_state.config().meadow_spawn;const auto &p=first_journey_state.config().retry;
        pose.feet={p.x,std::max(world_atlas_walk_height(p.x,p.z),0.f),p.z};pose.yaw=std::atan2(first_journey_state.config().encounter.x-p.x,-(first_journey_state.config().encounter.z-p.z));
        if(!opening_place(pose,false)){opening_tell("The encounter retry point is not supported.");return;}
        first_journey_clear_combat();third_person=true;temporal_cut=true;
    }
    if(events&WeaponGranted){first_journey_banner=5.5f;opening_tell("Bram's old sword received. The road follows the waystone.");preview_audio.cue(2);}
    if(events&SuppliesRecovered){opening_tell("Provisions recovered. Rowan is waiting by the fire.");preview_audio.cue(2);}
    if(events&RewardGranted){first_journey_reward_banner=7;first_journey_reward_health_before=fight.health;fight.health=fight.stamina=100;first_journey_reward_health_after=fight.health;opening_tell("A hot meal restores your strength. Twelve coins for the road.");preview_audio.cue(3);}
    if(events&Completed){first_journey_banner=8;opening_tell("A place to rest. A lead on Mara. Your journey has begun.");preview_audio.cue(3);}
    if(input.enabled&&(events&ReleaseControls)){opening_release();sword_stick.reset();}
    if(events&(DialogueOpened|DialogueClosed))temporal_cut=true;
    if(events&CheckpointReached)opening_save();
    if(!first_journey_outdoors()||first_journey_state.stage()==Stage::Travelers)return;
    for(unsigned i=0;i<studio.live.placements.size();++i)if(studio.live.placements[i].alias=="nm_journey_satchel"){
        const unsigned key=8192+i;
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
        if(first_journey_frame.dialogue_actor<3)world_trail_camp_focus(arrival_camp,first_journey_frame.dialogue_actor,cam.x,cam.z,float(input.dt));
        else world_trail_camp_focus(first_journey_townsfolk,0,cam.x,cam.z,float(input.dt));
    }
};
first_journey_draw=[&](bool loading){
    if(!first_journey_outdoors()||loading||first_journey_state.stage()==world_first_journey::Stage::Travelers)return false;
    const auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.5f),scale=u*preview_settings.text_scale;
    auto *dl=ImGui::GetForegroundDrawList();const auto ivory=IM_COL32(244,238,224,255),gold=IM_COL32(226,196,130,255);
    const bool pad=preview_pad.using_pad&&preview_pad.get();
    auto text=[&](float x,float y,float size,ImU32 color,const char *s){dl->AddText(ImGui::GetFont(),size*scale,{x,y},color,s);};
    auto center=[&](float y,float size,ImU32 color,const std::string &s){const auto width=ImGui::GetFont()->CalcTextSizeA(size*scale,10000,0,s.c_str()).x;text((w-width)*.5f,y,size,color,s.c_str());};
    const std::string confirm=pad?preview_pad.button(SDL_CONTROLLER_BUTTON_A):"E";
    if(first_journey_frame.dialogue){
        text(32*u,32*u,14,gold,first_journey_frame.dialogue_actor==3?"HUGH | THE COPPER LANTERN":first_journey_frame.dialogue_actor==1?"BRAM | THE ROADWARDEN":"ROWAN | THE CAMPFIRE");
        ImGui::SetNextWindowPos({w*.5f,h*.745f},ImGuiCond_Always,{.5f,0});ImGui::SetNextWindowSize({std::min(w-48*u,940*u),0});ImGui::SetNextWindowBgAlpha(.95f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{26*u,20*u});
        ImGui::Begin("###first-journey-dialogue",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoNavInputs);
        ImGui::SetWindowFontScale(std::max(.85f,1.15f*scale));ImGui::TextColored({.9f,.79f,.55f,1},"%s",first_journey_frame.speaker);ImGui::TextWrapped("%s",first_journey_frame.line);
        if(ImGui::Button("Continue",{150*u,34*u}))first_journey_click=true;
        ImGui::SameLine();ImGui::Text("%s: continue   %s: leave",confirm.c_str(),pad?preview_pad.button(SDL_CONTROLLER_BUTTON_B):"ESC");
        ImGui::End();ImGui::PopStyleVar();return true;
    }
    dl->AddRectFilled({22*u,22*u},{std::min(w-22*u,660*u),125*u},IM_COL32(7,15,19,215),5*u);
    text(38*u,34*u,13,gold,first_journey_state.town_reached()?"FIRST CHAPTER COMPLETE":"A PLACE AT THE FIRE");
    dl->AddText(ImGui::GetFont(),18*scale,{38*u,59*u},ivory,first_journey_frame.objective,nullptr,std::min(w-76*u,600*u));
    const auto &goal=first_journey_frame.objective_position;const double dx=goal.x-cam.x,dz=goal.z-cam.z,distance=std::hypot(dx,dz);
    char range[100];std::snprintf(range,sizeof range,"%.0f m   |   %u coins   %u provisions",distance,first_journey_frame.coins,first_journey_frame.food);text(38*u,102*u,12,gold,range);
    if(!fight.engaged()&&!first_journey_state.town_reached()&&distance>3){
        const double angle=std::remainder(std::atan2(dx,-dz)-cam.yaw,6.28318530718);
        const float x=w*.5f+float(std::clamp(angle/.95,-1.,1.))*std::min(w*.3f,420*u);
        dl->AddTriangleFilled({x,32*u},{x-6*u,22*u},{x+6*u,22*u},gold);
    }
    if(first_journey_frame.defeated){
        ImGui::SetNextWindowPos({w*.5f,h*.34f},ImGuiCond_Always,{.5f,0});ImGui::SetNextWindowSize({480*u,0});ImGui::SetNextWindowBgAlpha(.96f);
        ImGui::Begin("###first-journey-defeat",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("TAKE A BREATH");ImGui::Separator();ImGui::TextWrapped("The road is dangerous. Your completed conversations and loaned sword are safe.");
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
        const char *lesson=fight.enemy_phase==TrainingEnemyPhase::Windup?"MATCH THE INCOMING BLADE":fight.enemy_phase==TrainingEnemyPhase::Stagger?"OPENING! STRIKE ANOTHER ANGLE":"BREATHE. KEEP YOUR DISTANCE.";
        center(h*.18f,17,gold,lesson);center(h-30*u,12,ivory,pad?"Right stick: cut  |  LT + direction: guard  |  B: dodge  |  RS click: camera":"Mouse / arrows: cut direction  |  LMB: attack  |  RMB: guard  |  Space: dodge  |  V: camera");
        if(fight.enemy_phase==TrainingEnemyPhase::Windup){std::string incoming=std::string("INCOMING: ")+world_training_direction_name(fight.enemy_direction);center(h*.215f,22,ivory,incoming);}
        if(combat_contact_time>0){char cue[100];std::snprintf(cue,sizeof cue,combat_contact_kind==1?"BODY HIT  -%.0f":combat_contact_kind==2?"YOU WERE HIT  -%.0f":combat_contact_kind==4?"PARRY / STEEL":"STEEL BLOCK",combat_contact_damage);center(h*.43f,21,combat_contact_kind<=2?IM_COL32(235,100,83,255):gold,cue);}
    }else{
        if(first_journey_frame.interaction_available)center(h*.74f,19,ivory,"[ "+confirm+" ]  "+first_journey_frame.interaction);
        if(first_journey_banner>0)center(h*.21f,28,gold,first_journey_state.town_reached()?"A PLACE AT THE FIRE":"BRAM'S OLD SWORD");
        if(first_journey_reward_banner>0){center(h*.24f,23,gold,"PROVISIONS RETURNED");center(h*.29f,17,ivory,"12 coins  +  a hot meal  +  1 reputation");}
        if(opening_notice_seconds>0)center(h*.85f,15,gold,opening_notice);
        if(first_journey_state.town_reached())center(h-38*u,14,ivory,"Chapter complete. Your checkpoint is saved. Explore, or continue later.");
    }
    return true;
};
