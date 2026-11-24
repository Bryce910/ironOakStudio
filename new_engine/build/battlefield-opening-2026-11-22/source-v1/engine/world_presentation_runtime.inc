    // Opt-in player-facing preview. Authoring and the ordinary demo stay separate.
    using PreviewChapter=world_presentation::Chapter;
    world_presentation::Settings preview_settings;
    const std::string preview_combat_settings_path=(std::filesystem::path(studio.project_directory)/"combat.settings").u8string();
    std::string preview_combat_settings_message;
    if(presentation){
        auto preferences=world_presentation::combat_preferences(preview_settings);
        const auto loaded=world_combat_difficulty::load(preview_combat_settings_path.c_str(),preferences,preview_combat_settings_message);
        if(loaded==world_combat_difficulty::LoadResult::Loaded)world_presentation::apply_combat_preferences(preview_settings,preferences);
        else if(loaded!=world_combat_difficulty::LoadResult::Missing)std::fprintf(stderr,"[Combat settings] %s\n",preview_combat_settings_message.c_str());
    }
    auto preview_combat_settings_ui=[&](){
        ImGui::TextUnformatted("COMBAT SETTINGS");
        const char *choices[]={"Story","Normal","Hard","Expert"};
        int choice=int(preview_settings.difficulty);bool changed=false;
        if(ImGui::Combo("Combat difficulty",&choice,choices,4)){
            preview_settings.difficulty=world_combat_difficulty::Difficulty(choice);changed=true;
        }
        ImGui::TextWrapped("%s",world_combat_difficulty::description(preview_settings.difficulty));
        if(fight.engaged())ImGui::TextWrapped("Difficulty applies to the next encounter.");
        changed=ImGui::Checkbox("Gentler incoming damage",&preview_settings.story_assist)||changed;
        if(fight.engaged())fight.story_assist=preview_settings.story_assist;
        if(changed){
            const auto preferences=world_presentation::combat_preferences(preview_settings);
            if(world_combat_difficulty::save(preview_combat_settings_path.c_str(),preferences,preview_combat_settings_message))preview_combat_settings_message.clear();
        }
        if(!preview_combat_settings_message.empty())ImGui::TextWrapped("%s",preview_combat_settings_message.c_str());
    };
    WorldPresentationAudio preview_audio;WorldPresentationVoices preview_voices;
    const bool jungle_expanded=access("jungle_expansion.json",F_OK)==0;
    const bool town_enabled=presentation&&!city_showcase&&std::filesystem::is_regular_file(std::filesystem::path(studio.project_directory)/"models/watch_quarter/frontages.glb");
    int town_venue=-1,town_destination=-1;std::string town_message;
    int preview_screen=presentation&&launch->benchmark?1:0; // title, play, pause, journal, ending, town service
    PreviewChapter preview_chapter=PreviewChapter::Arrival;
    float preview_chapter_time=0,preview_black=0,preview_notice_time=0,preview_elapsed=0;
    float preview_hit=0,preview_parry=0;
    int preview_travel_phase=0;const char *preview_destination=nullptr;
    bool preview_seen_ending=false,preview_pending_save=false,preview_have_checkpoint=false;
    std::string preview_notice;
    std::uint64_t preview_memory_line=0;
    std::unique_ptr<world_runtime::Snapshot> preview_initial;
    if(presentation&&studio.components_runtime){preview_initial=std::make_unique<world_runtime::Snapshot>();studio.components_runtime->snapshot(*preview_initial,studio.message);}
    double preview_arena_x=creek_origin_x,preview_arena_z=creek_origin_z;float preview_arena_feet=24;
    world_controller::Device preview_pad;
    world_controller_qa::Pad controller_test_pad;
    auto preview_release_mouse=[&](){look_active=false;fight.set_guard(false);fight.clear_buffer();preview_pad.stop_rumble();SDL_SetRelativeMouseMode(SDL_FALSE);};
    auto preview_blocked=[&](){return presentation&&(preview_screen!=1||preview_travel_phase!=0||(opening_enabled&&(!opening_controls_enabled||!opening_movement_armed)));};
    auto preview_controller_context=[&](){
        using C=world_controller::Context;
        if(milestone_review.active())return C::Review;
        if(preview_screen!=1||preview_travel_phase||destination_loading||studio.open||show_graphics)return C::Menu;
        if(opening_enabled){
            if(first_journey_enabled&&first_journey_dialogue_active)return C::Dialogue;
            if(first_journey_enabled&&first_journey_defeated)return C::Retry;
            if(first_journey_enabled&&fight.engaged())return C::Combat;
            return C::Explore;
        }
        if(arena_enabled){if(arena_game.state()==world_arena_combat::State::Won||arena_game.state()==world_arena_combat::State::Lost)return C::Retry;return fight.engaged()?C::Combat:C::Explore;}
        if(studio.components_runtime&&studio.components_runtime->current_line())return C::Dialogue;
        if(fight.engaged())return C::Combat;
        if(fight.state==TrainingState::Lost||fight.state==TrainingState::Cancelled||(playtest_enabled&&fight.state==TrainingState::Won))return C::Retry;
        return C::Explore;
    };
    auto preview_sync_controller_ui=[&](){
        if(!presentation)return;
        SDL_GameController *device=preview_pad.get();
        ImGui_ImplSDL2_SetGamepadMode(ImGui_ImplSDL2_GamepadMode_Manual,device?&device:nullptr,device?1:0);
    };
    auto preview_stage=[&](){
        if(!studio.components_runtime)return PreviewChapter::Arrival;
        auto &r=*studio.components_runtime;
        auto stage=world_presentation::chapter(r.quest(world_runtime_demo::journey_id)!=world_gameplay::Progress::Inactive,
            r.counter(world_runtime_demo::mastery_counter),r.counter(world_presentation::threat_counter),
            r.counter(world_runtime_demo::camp_counter),r.counter(world_runtime_demo::return_counter));
        if(stage==PreviewChapter::Expedition&&world_atlas_village_weight(cam.x,cam.z)<.1&&
            std::hypot(cam.x-WORLD_TRAIL_CAMP_X-world_presentation::crossing_offset_x,cam.z-WORLD_TRAIL_CAMP_Z-world_presentation::crossing_offset_z)<28)stage=PreviewChapter::Threat;
        return stage;
    };
    auto preview_travel=[&](const char *destination){
        if(destination_loading||fight.engaged()||preview_travel_phase)return;
        if(studio.components_runtime)studio.components_runtime->close_dialogue();
        preview_destination=destination;preview_travel_phase=1;preview_release_mouse();preview_screen=1;
    };
    auto preview_review_end=[&](){
        if(!milestone_review.active())return;
        milestone_review.view=world_milestone::View::None;cam=milestone_return_camera;third_person=milestone_return_third;
        preview_screen=milestone_return_screen;preview_pending_save=milestone_return_save;current_name=milestone_return_name;
        portrait_focus_distance=0;avatar_initialized=false;destination_loading=true;temporal_cut=true;preview_release_mouse();
    };
    auto preview_review=[&](world_milestone::View view){
        if(!milestone_finish||fight.engaged()||destination_loading||preview_travel_phase)return;
        if(!milestone_review.active()){
            milestone_return_camera=cam;milestone_return_screen=preview_screen;milestone_return_third=third_person;
            milestone_return_save=preview_pending_save;milestone_return_name=current_name;
        }
        if(studio.components_runtime)studio.components_runtime->close_dialogue();
        milestone_review.view=view;milestone_review.orbit=.24f;milestone_review.distance=1.95f;milestone_review.pitch=.04f;
        for(unsigned i=0;i<std::size(scenic_views);i++)if(!std::strcmp(scenic_views[i].id,world_milestone::scene(view))){scenic_travel(cam,i);current_name=scenic_views[i].name;break;}
        cam.fly=false;cam.vy=0;cam.grounded=true;cam.fov=.9f;third_person=milestone_review.portrait();avatar_initialized=false;
        preview_screen=1;preview_pending_save=false;show_tools=show_map=show_graphics=false;destination_loading=true;temporal_cut=true;preview_release_mouse();
    };
    auto preview_begin=[&](){
        if(playtest_enabled){if(playtest_restart)playtest_restart();return;}
        milestone_review.view=world_milestone::View::None;portrait_focus_distance=0;
        if(preview_initial&&studio.components_runtime&&!studio.components_runtime->restore(*preview_initial,Studio::components_wall(),studio.message)){preview_notice=studio.message;preview_notice_time=6;return;}
        town_state={};town_destination=-1;town_venue=-1;town_message.clear();
        fight=WorldTrainingCombat{};live_training_encounter=0;preview_screen=1;preview_seen_ending=false;
        preview_chapter=PreviewChapter::Arrival;preview_chapter_time=6;preview_elapsed=0;preview_pending_save=true;
        journey_open=false;show_tools=show_map=show_graphics=false;
        preview_travel(town_enabled?"castle-quarter":milestone_finish?"castle-milestone":"castle-training");preview_audio.cue(2);
    };
    auto preview_resume=[&](){
        if(load_journey()){preview_screen=1;preview_chapter=preview_stage();preview_chapter_time=5;preview_seen_ending=preview_chapter==PreviewChapter::Complete;preview_release_mouse();}
        else{preview_notice=journey_notice;preview_notice_time=7;}
    };
    auto preview_goal=[&](double &x,double &y,double &z,const char *&label){
        if(stronghold_finish)return false;
        if(milestone_review.active())return false;
        if(town_enabled&&town_destination>=0&&town_destination<4&&world_atlas_village_weight(cam.x,cam.z)>.2){const auto &venue=world_town::venues[town_destination];x=venue.entry_x;y=24;z=venue.entry_z;label=venue.name;return true;}
        world_runtime::Entity e;world_runtime::Id id=0;const auto c=preview_stage();
        if(c==PreviewChapter::Arrival||c==PreviewChapter::Trial||c==PreviewChapter::Homecoming){id=world_runtime_demo::instructor_id;label="MASTER CORVIN";}
        else if(c==PreviewChapter::Report){id=world_runtime_demo::jungle_actor_first;label="WARDEN ROWAN";}
        else if(c==PreviewChapter::Threat){id=world_runtime_demo::id_base+0x40;label="THE CROSSING";}
        else if(c==PreviewChapter::Expedition){
            if(world_atlas_village_weight(cam.x,cam.z)>.2){x=creek_origin_x-42.5;y=24;z=creek_origin_z-62;label="SOUTH TRAIL";return true;}
            x=WORLD_TRAIL_CAMP_X+world_presentation::crossing_offset_x;z=WORLD_TRAIL_CAMP_Z+world_presentation::crossing_offset_z;y=walking_height(x,z,nullptr);label="WAYFARERS' CROSSING";return true;
        }else return false;
        if(!studio.components_runtime||!studio.components_runtime->entity(id,e))return false;
        x=e.pose.position.x;y=e.pose.position.y;z=e.pose.position.z;return true;
    };
    auto preview_interact=[&](){
        if(playtest_enabled)return playtest_use&&playtest_use();
        if(!presentation||milestone_review.active()||preview_blocked()||destination_loading||fight.engaged()||!studio.components_runtime)return false;
        if(studio.components_runtime->current_line())return false;
        if(town_enabled){const int at=world_town::nearby(cam.x,cam.y-1.75,cam.z,cam.fly);if(at>=0){town_venue=at;town_message.clear();preview_screen=5;town_destination=-1;preview_release_mouse();return true;}}
        const auto c=preview_stage();
        if(c==PreviewChapter::Expedition&&world_atlas_village_weight(cam.x,cam.z)>.2&&std::hypot(cam.x-(creek_origin_x-42.5),cam.z-(creek_origin_z-62))<18){preview_travel("rainforest");return true;}
        if(c==PreviewChapter::Homecoming&&world_atlas_village_weight(cam.x,cam.z)<.1){preview_travel("castle-training-start");return true;}
        if(c==PreviewChapter::Threat){
            const double d=std::hypot(cam.x-WORLD_TRAIL_CAMP_X-world_presentation::crossing_offset_x,cam.z-WORLD_TRAIL_CAMP_Z-world_presentation::crossing_offset_z);
            if(d<17){studio.components_runtime->enqueue({world_gameplay::EventKind::Interact,world_runtime_demo::id_base+0x40});return true;}
        }
        return false;
    };
    auto town_transact=[&](world_town::Action action){
        if(!town_enabled||!studio.components_runtime||fight.engaged()||live_training_encounter||destination_loading)return false;
        const auto before=town_state;
        if(!world_town::transact(town_state,action,studio.components_runtime->counter(world_runtime_demo::coin_counter),town_message))return false;
        if(!save_journey()){town_state=before;town_message="Could not save this transaction: "+journey_notice;return false;}
        preview_have_checkpoint=true;preview_audio.cue(4);return true;
    };
    auto preview_key=[&](SDL_Keycode key){
        if(!presentation)return false;
        if(opening_enabled&&opening_key)return opening_key(key);
        if(playtest_enabled&&playtest_key)return playtest_key(key);
        if(milestone_review.active()){
            if(key==SDLK_ESCAPE||key==SDLK_F7){preview_review_end();return true;}
            if(key>=SDLK_1&&key<=SDLK_3){preview_review(world_milestone::View(int(key-SDLK_1)+1));return true;}
            if(milestone_review.portrait())return true;
            return key!=SDLK_w&&key!=SDLK_a&&key!=SDLK_s&&key!=SDLK_d&&key!=SDLK_v&&key!=SDLK_LSHIFT&&key!=SDLK_SPACE;
        }
        if(milestone_finish&&key==SDLK_F7){preview_review(world_milestone::View::Courtyard);return true;}
        if(key==SDLK_ESCAPE){
            if(show_graphics){show_graphics=false;return true;}
            if(studio.components_runtime&&studio.components_runtime->current_line()){studio.components_runtime->close_dialogue();preview_release_mouse();return true;}
            if(preview_screen!=0){preview_screen=preview_screen==1?2:1;preview_release_mouse();}return true;
        }
        if(preview_screen==0){if(key==SDLK_RETURN||key==SDLK_SPACE)preview_begin();return true;}
        if(key==SDLK_j||key==SDLK_TAB){preview_screen=preview_screen==3?1:3;preview_release_mouse();return true;}
        if(key==SDLK_F5){if(save_journey()){preview_notice="Journey saved";preview_have_checkpoint=true;}else preview_notice=journey_notice;preview_notice_time=4;return true;}
        if(key==SDLK_F8){preview_resume();return true;}
        if(preview_blocked())return true;
        if(studio.components_runtime&&studio.components_runtime->current_line()){
            if(key>=SDLK_1&&key<=SDLK_4){const auto choices=studio.components_runtime->choices();const unsigned i=unsigned(key-SDLK_1);if(i<choices.count)studio.components_runtime->choose(choices.indices[i]);}
            return true;
        }
        if((key==SDLK_RETURN||key==SDLK_KP_ENTER)&&(fight.state==TrainingState::Lost||fight.state==TrainingState::Cancelled)){
            const auto id=fight.trail_threat?world_runtime_demo::id_base+0x40:world_runtime_demo::instructor_id;
            if(world_presentation::queue_retry(*studio.components_runtime,live_training_encounter,id))fight.state=TrainingState::Idle;
            return true;
        }
        if(key==SDLK_e&&preview_interact())return true;
        if(key==SDLK_F9||key==SDLK_F11||key==SDLK_h||key==SDLK_f||(key>=SDLK_1&&key<=SDLK_9))return true;
        return false;
    };
    auto preview_tick=[&](float delta){
        if(!presentation)return;
        if(opening_enabled){if(opening_tick)opening_tick(delta);return;}
        if(playtest_enabled){if(playtest_tick)playtest_tick(delta);return;}
        if(stronghold_finish)return;
        if(milestone_review.active())return;
        const float d=std::clamp(delta,0.f,.1f);preview_notice_time=std::max(0.f,preview_notice_time-d);
        const auto *line=studio.components_runtime?studio.components_runtime->current_line():nullptr;
        if((line?line->id:0)!=preview_memory_line)temporal_cut=true;
        if(line&&preview_memory_line!=line->id&&studio.components_runtime->recollection().recognition)preview_audio.cue(4);
        preview_memory_line=line?line->id:0;
        preview_hit=std::max(0.f,preview_hit-d);preview_parry=std::max(0.f,preview_parry-d);
        if(preview_screen==1&&!destination_loading)preview_elapsed+=d;
        if(preview_travel_phase==1){
            preview_black=std::min(1.f,preview_black+d*2);
            if(preview_black>=1){
                bool found=false;
                for(unsigned i=0;i<std::size(scenic_views);i++)if(!std::strcmp(scenic_views[i].id,preview_destination)){
                    scenic_travel(cam,i);current_name=scenic_views[i].name;destination_loading=true;temporal_cut=true;found=true;break;}
                preview_travel_phase=found?2:0;preview_destination=nullptr;
            }
        }else if(preview_travel_phase==2&&!destination_loading){
            preview_black=std::max(0.f,preview_black-d*1.3f);
            if(preview_black<=0){preview_travel_phase=0;preview_chapter_time=5;preview_pending_save=true;preview_audio.cue(2);}
        }
        if(preview_screen==1&&!destination_loading&&!preview_travel_phase){
            const auto c=preview_stage();
            if(c!=preview_chapter){preview_chapter=c;preview_chapter_time=5;preview_pending_save=true;preview_audio.cue(c==PreviewChapter::Complete?3:2);}
            preview_chapter_time=std::max(0.f,preview_chapter_time-d);
            if(c==PreviewChapter::Complete&&!preview_seen_ending){preview_seen_ending=true;preview_screen=4;preview_release_mouse();}
        }
        if(preview_pending_save&&preview_screen!=0&&!destination_loading&&!preview_travel_phase&&!fight.engaged()&&!live_training_encounter&&
            studio.components_runtime&&!studio.components_runtime->current_line()){
            if(save_journey()){preview_pending_save=false;preview_have_checkpoint=true;preview_notice="Checkpoint saved";preview_notice_time=2.5f;}
        }
    };
    std::string preview_focus_panel;
    std::uint64_t preview_focus_line=0;
    bool preview_default_focus=false;
    auto preview_ui=[&](const Camera &eye,bool loading){
        if(!presentation||studio.open||inventory_open||(launch->no_hud&&source_review.active))return;
        if(opening_enabled){if(opening_draw)opening_draw(eye,loading);return;}
        if(playtest_enabled){if(playtest_draw)playtest_draw(eye,loading);return;}
        if(launch->no_hud&&preview_screen==1)return;
        if(show_graphics){preview_focus_panel.clear();return;}
        auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
        const float unit=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.4f),u=unit*preview_settings.text_scale;
        auto *dl=ImGui::GetForegroundDrawList();const ImU32 gold=IM_COL32(225,192,128,255),white=IM_COL32(241,237,225,255),muted=IM_COL32(178,187,184,255);
        auto text=[&](float x,float y,float size,ImU32 color,const char *s,float wrap=0.f){dl->AddText(ImGui::GetFont(),size*u,{x,y},color,s,nullptr,wrap);};
        auto center=[&](float y,float size,ImU32 color,const char *s){const float tw=ImGui::GetFont()->CalcTextSizeA(size*u,10000,0,s).x;text((w-tw)*.5f,y,size,color,s);};
        const bool pad_hints=preview_pad.using_pad&&preview_pad.get();
        auto hint=[&](const std::string &keyboard,const std::string &controller){return pad_hints?controller:keyboard;};
        auto pad_button=[&](SDL_GameControllerButton b){return std::string(preview_pad.button(b));};
        const auto confirm=pad_button(SDL_CONTROLLER_BUTTON_A),cancel=pad_button(SDL_CONTROLLER_BUTTON_B);
        const auto pause=pad_button(SDL_CONTROLLER_BUTTON_START),journal=pad_button(SDL_CONTROLLER_BUTTON_Y);
        const auto camera=pad_button(SDL_CONTROLLER_BUTTON_RIGHTSTICK),sprint=pad_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        const std::string thrust_keyboard=arena_enabled?"T + LMB / Q + MMB: thrust\nTap MMB: focus / cycle  |  Hold MMB: release focus":"MMB: thrust";
        const std::string thrust_controller=arena_enabled?
            pad_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)+" + "+pad_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)+": thrust\nTap "+pad_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)+": focus / cycle  |  Hold "+pad_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)+": release focus":
            pad_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)+": thrust";
        const auto *focus_line=studio.components_runtime?studio.components_runtime->current_line():nullptr;
        const std::uint64_t focus_line_id=focus_line?focus_line->id:0;
        if(!world_controller::ui_owns(preview_controller_context()))preview_focus_panel.clear();
        auto begin_panel=[&](const char *id,float x,float y,float width){
            ImGui::SetNextWindowPos({x,y},ImGuiCond_Always);ImGui::SetNextWindowSize({width,0},ImGuiCond_Always);
            ImGui::SetNextWindowSizeConstraints({width,0},{width,std::max(120.f,h-y-18*unit)});
            const bool interactive=world_controller::ui_owns(preview_controller_context());
            const std::string panel_key=std::string(id)+":"+std::to_string(preview_screen);
            preview_default_focus=interactive&&(preview_focus_panel!=panel_key||preview_focus_line!=focus_line_id);
            if(preview_default_focus){ImGui::SetNextWindowFocus();preview_focus_panel=panel_key;preview_focus_line=focus_line_id;}
            ImGui::PushStyleColor(ImGuiCol_WindowBg,{.035f,.055f,.059f,.96f});ImGui::PushStyleColor(ImGuiCol_Text,{.94f,.92f,.86f,1});
            ImGui::PushStyleColor(ImGuiCol_Button,{.16f,.21f,.21f,1});ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.26f,.33f,.31f,1});ImGui::PushStyleColor(ImGuiCol_ButtonActive,{.35f,.37f,.27f,1});
            ImGui::PushStyleColor(ImGuiCol_NavHighlight,{.88f,.75f,.50f,1});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{25*unit,22*unit});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*unit,12*unit});ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,5.f);
            ImGui::Begin(id,nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::SetWindowFontScale(std::max(.8f,1.10f*u));
        };
        auto end_panel=[&](){
            if(pad_hints&&world_controller::ui_owns(preview_controller_context()))ImGui::TextWrapped("Left stick / D-pad: navigate  |  %s: select  |  %s: back",confirm.c_str(),cancel.c_str());
            ImGui::End();ImGui::PopStyleVar(3);ImGui::PopStyleColor(6);};
        auto button=[&](const char *s){
            // Stable IDs and neutral labels survive input-device changes.
            const std::string label=world_controller::button_label(s);
            const bool pressed=ImGui::Button(label.c_str(),{-1,42*unit});
            if(qa_controller&&pressed)std::printf("[Controller QA] UI selected %s, screen %d\n",s,preview_screen);
            world_controller::default_focus(preview_default_focus);
            return pressed;
        };
        if(milestone_review.active()){
            const char *title=milestone_review.portrait()?"THE WATCH KNIGHT":milestone_review.view==world_milestone::View::Jungle?(jungle_expanded?"THE EMERALD EXPANSE":"THE EMERALD PATH"):"THE WATCH COURTYARD";
            text(42*unit,38*unit,13,gold,"LEGENDS OF VALWOOD");text(42*unit,62*unit,22,white,title);
            if(jungle_expanded&&milestone_review.view==world_milestone::View::Jungle){
                auto *map=ImGui::GetForegroundDrawList();const float mw=185*unit,mh=300*unit,mx=w-mw-25*unit,my=32*unit;
                map->AddRectFilled({mx-12*unit,my-12*unit},{mx+mw+12*unit,my+mh+32*unit},IM_COL32(8,24,21,220),6*unit);
                auto point=[&](double x,double z){return ImVec2(mx+float((x+26817.)/210.)*mw,my+float((9760.496705929329-z)/470.)*mh);};
                for(unsigned i=1;i<std::size(kJungleLoop);i++)map->AddLine(point(kJungleLoop[i-1][0],kJungleLoop[i-1][1]),point(kJungleLoop[i][0],kJungleLoop[i][1]),IM_COL32(176,157,92,230),2*unit);
                size_t n=0;const auto *old=world_atlas_rainforest_trail_points(&n);for(size_t i=1;i<n;i++)map->AddLine(point(old[i-1].x,old[i-1].z),point(old[i].x,old[i].z),IM_COL32(176,157,92,230),2*unit);
                const JunglePlace *nearest=nullptr;double near_distance=1e9;
                for(const auto &place:kJunglePlaces){map->AddCircleFilled(point(place.x,place.z),3*unit,IM_COL32(205,197,133,255));double d=std::hypot(cam.x-place.x,cam.z-place.z);if(d<near_distance){nearest=&place;near_distance=d;}}
                const auto here=point(cam.x,cam.z);map->AddCircleFilled(here,4*unit,IM_COL32(92,227,191,255));map->AddLine(here,{here.x+std::sin(cam.yaw)*12*unit,here.y+std::cos(cam.yaw)*12*unit},IM_COL32(160,255,223,255),2*unit);
                map->AddText({mx,my+mh+9*unit},IM_COL32(218,224,209,255),"EMERALD EXPANSE");
                if(nearest){char note[128];std::snprintf(note,sizeof(note),"%s  |  %.0f m",nearest->name,near_distance);text(42*unit,96*unit,14,gold,note);}
            }
            begin_panel("###milestone-review",std::max(16.f,(w-700*unit)*.5f),h-140*unit,700*unit);
            if(ImGui::Button("Courtyard",{158*unit,34*unit}))preview_review(world_milestone::View::Courtyard);
            ImGui::SameLine();
            if(ImGui::Button("Jungle path",{158*unit,34*unit}))preview_review(world_milestone::View::Jungle);
            ImGui::SameLine();
            if(ImGui::Button("Hero portrait",{184*unit,34*unit}))preview_review(world_milestone::View::Hero);
            ImGui::SameLine();
            if(ImGui::Button("Back",{142*unit,34*unit}))preview_review_end();
            const auto controls=hint(milestone_review.portrait()?"1/2/3: view   |   Right mouse: orbit   |   Wheel: frame   |   Esc: back":"1/2/3: view   |   WASD: walk   |   Right mouse: look   |   Esc: back",
                "LB / RB (L1 / R1): view  |  "+cancel+": back\n"+(milestone_review.portrait()?"RS: orbit  |  LT / RT (L2 / R2): frame":"LS: walk  |  RS: look  |  RS click: camera"));
            ImGui::TextWrapped("%s",controls.c_str());end_panel();return;
        }
        if(preview_screen==0){
            ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor({0,0},{w,h},IM_COL32(6,13,16,245),IM_COL32(6,13,16,35),IM_COL32(6,13,16,90),IM_COL32(6,13,16,245));
            dl->AddLine({80*unit,90*unit},{80*unit,390*unit},gold,2);
            text(110*unit,92*unit,14,gold,"AN ORIGINAL FANTASY WORLD");
            text(106*unit,135*unit,52,white,"LEGENDS OF");text(106*unit,195*unit,76,white,"VALWOOD");
            text(110*unit,304*unit,20,gold,"A PLACE IN THE WATCH");
            text(110*unit,346*unit,17,muted,"A blade can win a trial.\nA kept promise earns a place.",440*unit);
            begin_panel("###preview-title",88*unit,std::min(h-330*unit,420*unit),430*unit);
            if(button("BEGIN JOURNEY     [ENTER]"))preview_begin();
            const bool can_load=std::filesystem::is_regular_file(progress_file());ImGui::BeginDisabled(!can_load);
            if(button("CONTINUE JOURNEY"))preview_resume();
            ImGui::EndDisabled();
            if(milestone_finish&&button("VISUAL SHOWCASE     [F7]"))preview_review(world_milestone::View::Courtyard);
            if(button("GRAPHICS AND DISPLAY")){show_graphics=true;graphics_draft=graphics;preview_focus_panel.clear();}
            if(button("QUIT"))running=false;
            end_panel();
            text(110*unit,h-66*unit,13,muted,"PLAYABLE PREVIEW  /  OCTOBER 2026");
            text(w-430*unit,h-66*unit,13,muted,"Headphones recommended  |  10-15 minutes");
        }else if(preview_screen==5&&town_venue>=0&&town_venue<4){
            ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor({0,0},{w,h},IM_COL32(4,10,13,35),IM_COL32(4,10,13,210),IM_COL32(4,10,13,210),IM_COL32(4,10,13,35));
            const auto &venue=world_town::venues[town_venue];const float width=std::min(w-40*unit,600*unit);
            begin_panel("###town-service",w-width-32*unit,90*unit,width);
            ImGui::TextColored({.88f,.75f,.5f,1},"%s",venue.name);ImGui::TextWrapped("%s",venue.keeper);ImGui::Separator();ImGui::TextWrapped("%s",venue.greeting);
            ImGui::Text("Purse: %s",world_currency::format(studio.components_runtime?world_town::purse(town_state,studio.components_runtime->counter(world_runtime_demo::coin_counter)):0).c_str());
            if(town_venue==0){
                if(button("HONE MY BLADE  /  6 COPPER"))town_transact(world_town::Action::Sharpen);
                ImGui::TextWrapped("%s","+15% strike damage for one encounter. Your existing weapon stays equipped.");
                if(button("ASK ABOUT THE WATCH"))town_message="Corvin trains recruits in the west court. Match the incoming angle, parry at the last moment, then strike while the guard is open.";
            }else if(town_venue==1){
                if(!town_state.welcome&&button("COLLECT WELCOME LOAF  /  FREE"))town_transact(world_town::Action::Welcome);
                if(button("BUY BREAD  /  3 COPPER"))town_transact(world_town::Action::Bread);
                if(button("BUY ORCHARD FRUIT  /  2 COPPER"))town_transact(world_town::Action::Apples);
                ImGui::TextWrapped("Carry up to 8 of each. Eat provisions from your journal before an encounter. Food and a tavern meal give the same benefit.");
            }else{
                if(button("ORDER A HOT SUPPER  /  4 COPPER"))town_transact(world_town::Action::Meal);
                if(button("REST AND SAVE  /  5 COPPER"))town_transact(world_town::Action::Rest);
                ImGui::TextWrapped("%s","Supper: 10% less incoming damage. Rest: 25% faster stamina recovery. Each lasts for your next encounter.");
                if(button("HEAR THE LOCAL NEWS"))town_message="Mara's provisions stand is on the lane below the keep. Bram works beside the western barracks. The wayfarers say a deserter has been stopping travelers at the jungle crossing.";
            }
            if(!town_message.empty()){ImGui::Separator();ImGui::TextWrapped("%s",town_message.c_str());}
            if(button("LEAVE CONVERSATION")){preview_screen=1;preview_release_mouse();}
            end_panel();
        }else if(preview_screen==2||preview_screen==3||preview_screen==4){
            ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},{w,h},IM_COL32(4,10,13,195));
            const float panel_width=std::min(w-40*unit,600*unit);
            begin_panel("###preview-menu",(w-panel_width)*.5f,70*unit,panel_width);
            if(preview_screen==4){
                ImGui::TextColored({.88f,.75f,.5f,1},"A PROMISE KEPT");ImGui::SetWindowFontScale(1.7f*u);ImGui::TextWrapped("Welcome to the watch.");ImGui::SetWindowFontScale(1.1f*u);
                ImGui::TextWrapped("You proved your blade, secured the crossing, and brought the wayfarers' word home. Corvin remembers your victory. Rowan remembers who answered.");
                ImGui::Separator();ImGui::TextWrapped("WATCH SIGNET EARNED\n25 coin  /  +1 watch reputation\nWatch endurance: faster stamina recovery and sprinting.");
                ImGui::Text("Session %02d:%02d  |  Crossing hits %u  |  Parries %u",int(preview_elapsed)/60,int(preview_elapsed)%60,fight.hits,fight.parries);
                if(button("EXPLORE VALDOA")){preview_screen=1;preview_travel("castle-finished-arcade");}
                if(button("VISIT THE COPPER LANTERN")){preview_screen=1;preview_travel("castle-tavern-interior");}
                if(button("PLAY AGAIN"))preview_begin();
            }else if(preview_screen==3){
                ImGui::TextColored({.88f,.75f,.5f,1},"JOURNAL  /  A PLACE IN THE WATCH");
                ImGui::TextWrapped("%s",world_presentation::objective(preview_stage()));ImGui::Separator();
                if(studio.components_runtime){auto &r=*studio.components_runtime;
                    const char *labels[]={"Earn Corvin's trust","Secure the crossing","Collect Rowan's report","Take the watch oath"};
                    const std::uint64_t counters[]={world_runtime_demo::mastery_counter,world_presentation::threat_counter,world_runtime_demo::camp_counter,world_runtime_demo::return_counter};
                    for(unsigned i=0;i<4;i++)ImGui::TextColored(r.counter(counters[i])?ImVec4(.58f,.78f,.64f,1):ImVec4(.72f,.73f,.70f,1),"%s  %s",r.counter(counters[i])?"COMPLETE":"   -   ",labels[i]);
                    ImGui::Separator();ImGui::Text("Purse: %s  |  Watch reputation: %d",world_currency::format(world_town::purse(town_state,r.counter(world_runtime_demo::coin_counter))).c_str(),r.counter(world_runtime_demo::renown_counter));}
                if(button("RETURN TO JOURNEY     [J]"))preview_screen=1;
                if(town_enabled){
                    ImGui::Separator();ImGui::Text("PROVISIONS  /  Bread %d  |  Fruit %d",town_state.bread,town_state.apples);
                    ImGui::TextWrapped("Prepared for next encounter: %s%s%s%s",town_state.prepared&1?"Honed blade. ":"",town_state.prepared&2?"Well fed. ":"",town_state.prepared&4?"Rested. ":"",town_state.prepared?"":"None.");
                    ImGui::BeginDisabled(fight.engaged()||live_training_encounter||destination_loading);
                    if(town_state.bread&&button("EAT BREAD  /  10% LESS INCOMING DAMAGE"))town_transact(world_town::Action::EatBread);
                    if(town_state.apples&&button("EAT FRUIT  /  25% FASTER STAMINA RECOVERY"))town_transact(world_town::Action::EatApple);
                    ImGui::EndDisabled();
                    if(!town_message.empty())ImGui::TextWrapped("%s",town_message.c_str());
                    ImGui::Separator();ImGui::TextWrapped("WATCH QUARTER  /  Follow a walking marker to a service. Enter the tavern and approach the bar to speak to the innkeeper.");
                    for(int i=0;i<3;i++)if(button(world_town::venues[i].name)){town_destination=i;preview_screen=1;}
                    if(button("FOLLOW MAIN QUEST")){town_destination=-1;preview_screen=1;}
                }
            }else{
                ImGui::TextColored({.88f,.75f,.5f,1},"JOURNEY PAUSED");
                if(button("RESUME     [ESC]"))preview_screen=1;
                ImGui::BeginDisabled(fight.engaged());if(milestone_finish&&button("VISUAL SHOWCASE     [F7]"))preview_review(world_milestone::View::Courtyard);ImGui::EndDisabled();
                ImGui::BeginDisabled(fight.engaged());if(button("SAVE JOURNEY")){if(save_journey()){preview_notice="Journey saved";preview_have_checkpoint=true;}else preview_notice=journey_notice;preview_notice_time=4;}ImGui::EndDisabled();
                ImGui::BeginDisabled(!std::filesystem::is_regular_file(progress_file()));if(button("RESTORE CHECKPOINT")){fight.leave();live_training_encounter=0;preview_resume();}ImGui::EndDisabled();
                ImGui::Separator();preview_combat_settings_ui();ImGui::Separator();ImGui::SliderFloat("Look sensitivity",&preview_settings.sensitivity,.35f,2.f,"%.2f");ImGui::SliderFloat("Text size",&preview_settings.text_scale,.85f,1.3f,"%.2f");ImGui::SliderFloat("Music",&preview_settings.music,0,1,"%.2f");
                if(preview_voices.available)ImGui::SliderFloat("Dialogue voice",&preview_settings.voice,0,1,"%.2f");
                ImGui::SliderFloat("Controller deadzone",&preview_settings.deadzone,.05f,.35f,"%.2f");
                if(ImGui::Checkbox("Controller vibration",&preview_settings.vibration)&&!preview_settings.vibration)preview_pad.stop_rumble();
                ImGui::TextWrapped("Controller: %s",preview_pad.name());
                ImGui::Checkbox("Invert vertical look",&preview_settings.invert_y);ImGui::Checkbox("Reduced camera effects",&preview_settings.reduced_motion);
                const auto controls=hint(std::string("WASD: move  |  Hold right mouse: look\nShift: sprint  |  E: interact  |  V: camera\nMouse flick / arrows: attack angle\nLeft mouse: swing  |  Right mouse: guard\n")+thrust_keyboard+"\nSpace: dodge  |  J: journal  |  F5/F8: save / restore",
                    std::string("LS: move  |  RS: look / directional sword swing\nRS up: rising cut  |  RS down: overhead cut\nHold ")+preview_pad.guard_label()+" + RS: directional block\nCentered RS + "+preview_pad.guard_label()+": thrust guard; release / press for parry\nReturn RS to center between swings\n"+sprint+" / LS click: sprint  |  "+confirm+": interact\n"+preview_pad.attack_label()+" / "+pad_button(SDL_CONTROLLER_BUTTON_X)+": alternate swing\n"+thrust_controller+"\n"+cancel+": dodge  |  "+journal+": journal  |  "+camera+": camera  |  "+pause+": pause\nSave and restore checkpoints from this menu.");
                ImGui::TextWrapped("%s",controls.c_str());
                if(button("GRAPHICS AND DISPLAY")){show_graphics=true;graphics_draft=graphics;preview_focus_panel.clear();}
                if(button("TITLE SCREEN")){fight.leave();live_training_encounter=0;if(studio.components_runtime)studio.components_runtime->close_dialogue();preview_screen=0;}
                if(button("QUIT")){if(!fight.engaged())save_journey();running=false;}
            }end_panel();
        }else{
            // Compass and objective: screen-space marker always points to the real target.
            dl->AddRectFilledMultiColor({0,0},{w,150*unit},IM_COL32(4,10,13,185),IM_COL32(4,10,13,185),0,0);
            text(38*unit,26*unit,12,gold,"LEGENDS OF VALWOOD");text(38*unit,51*unit,19,white,city_showcase?"MEDIEVAL CITY":stronghold_finish?"GRANDSTONE STRONGHOLD":world_presentation::title(preview_stage()));
            const std::string shown_objective=city_showcase?"Explore the publisher city. F9 opens the authored viewpoints; F returns to walking.":source_showcase?"Visual review  -  Explore the castle, courtyard and great hall.":stronghold_finish?"Brandalhall / Great Mashito Mountains  -  Explore the mountain gate and dwarven hall.":town_enabled&&town_destination>=0?std::string("Walk to ")+world_town::venues[town_destination].name+". Journal: return to main quest.":world_presentation::objective(preview_stage());text(38*unit,83*unit,14,muted,shown_objective.c_str(),std::min(620*unit,w*.6f));
            const auto shortcuts=hint("J  JOURNAL     ESC  PAUSE",journal+"  JOURNAL   "+pause+"  PAUSE");
            text(w-330*unit,28*unit,13,muted,shortcuts.c_str());
            const bool talking=studio.components_runtime&&studio.components_runtime->current_line();
            if(!fight.engaged()&&!talking){const auto controls=source_showcase?
                hint("WASD  MOVE   RMB  LOOK   Q/E  HEIGHT   F9  SOURCE VIEWS   F10  GRAPHICS",
                     std::string("LS  MOVE   RS  LOOK   ")+preview_pad.guard_label()+" / "+preview_pad.attack_label()+"  HEIGHT   "+pad_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)+" / "+pad_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)+"  SOURCE VIEWS   "+pause+"  SETTINGS"):
                hint("WASD  MOVE    RMB  LOOK    SHIFT  SPRINT    V  CAMERA","LS  MOVE   RS  LOOK   "+sprint+"  SPRINT   "+pad_button(SDL_CONTROLLER_BUTTON_X)+"  JUMP   "+camera+"  CAMERA");
                if(stronghold_finish||city_showcase){dl->AddRectFilled({24*unit,h-56*unit},{std::min(w-24*unit,source_showcase?1120*unit:820*unit),h-14*unit},IM_COL32(4,10,13,180),5*unit);}
                text(38*unit,h-42*unit,13,muted,controls.c_str());}
            const int near_service=town_enabled&&!fight.engaged()&&!talking?world_town::nearby(cam.x,cam.y-1.75,cam.z,cam.fly):-1;
            if(near_service>=0){const auto prompt=hint("[E]  SPEAK", "["+confirm+"]  SPEAK");const std::string service_prompt=prompt+"  /  "+world_town::venues[near_service].keeper;center(h-118*unit,17,gold,service_prompt.c_str());}
            double gx=0,gy=0,gz=0;const char *label="";
            if(!city_showcase&&preview_goal(gx,gy,gz,label)&&!fight.engaged()&&!talking){
                const double dx=gx-eye.x,dy=gy+2.35-eye.y,dz=gz-eye.z;const auto f=eye.forward(),r=eye.right(),up=cross(r,f);
                const double depth=dx*f.x+dy*f.y+dz*f.z;float px=w*.5f,py=h*.5f;
                if(depth>.1){px+=float((dx*r.x+dy*r.y+dz*r.z)/depth/std::tan(eye.fov*.5f)/eye.aspect)*w*.5f;py-=float((dx*up.x+dy*up.y+dz*up.z)/depth/std::tan(eye.fov*.5f))*h*.5f;}
                else px=dx*r.x+dz*r.z>0?w-45*unit:45*unit;
                px=std::clamp(px,45*unit,w-45*unit);py=std::clamp(py,155*unit,h-160*unit);
                const double dist=std::hypot(gx-cam.x,gz-cam.z);const float radius=7*unit;
                dl->AddQuadFilled({px,py-radius},{px+radius,py},{px,py+radius},{px-radius,py},gold);
                char distance[48];std::snprintf(distance,sizeof distance,"%.0f m",dist);text(px-15*unit,py+16*unit,12,white,distance);if(town_destination>=0)text(std::clamp(px-90*unit,16.f,w-260*unit),py-28*unit,12,gold,label);
                if(near_service<0&&town_destination<0&&dist<18){
                    const char *prompt=preview_stage()==PreviewChapter::Expedition&&world_atlas_village_weight(cam.x,cam.z)>.2?"[E]  FOLLOW THE SOUTH TRAIL":preview_stage()==PreviewChapter::Homecoming&&world_atlas_village_weight(cam.x,cam.z)<.1?"[E]  RETURN TO VALDOA":preview_stage()==PreviewChapter::Threat?"[E]  CHALLENGE THE OATHBREAKER":dist<3.4?"[E]  SPEAK":"APPROACH TO SPEAK";
                    std::string shown=prompt;if(pad_hints&&shown.rfind("[E]",0)==0)shown.replace(0,3,"["+confirm+"]");
                    center(h-118*unit,17,gold,shown.c_str());
                }
            }
            if(near_service<0&&!talking&&preview_stage()==PreviewChapter::Homecoming&&world_atlas_village_weight(cam.x,cam.z)<.1){const auto prompt=hint("[E]  RETURN TO VALDOA","["+confirm+"]  RETURN TO VALDOA");center(h-118*unit,17,gold,prompt.c_str());}
            if(studio.components_runtime&&studio.components_runtime->current_line()){
                const auto *line=studio.components_runtime->current_line();const auto remembered=studio.components_runtime->recollection();
                const bool castle=world_atlas_village_weight(cam.x,cam.z)>.2;
                const float width=std::min(880*unit,w-48*unit);
                begin_panel("###preview-conversation",(w-width)*.5f,h-std::min(385*unit,h*.58f),width);
                ImGui::TextColored({.88f,.75f,.5f,1},"%s",castle?"MASTER CORVIN  /  CAPTAIN OF THE WATCH":"WARDEN ROWAN  /  WAYFARERS' STEWARD");
                if(remembered.recognition){ImGui::TextColored({.55f,.79f,.69f,1},"REMEMBERS YOU");ImGui::TextWrapped("%s",remembered.recognition);}
                if(castle&&preview_stage()==PreviewChapter::Homecoming)ImGui::TextWrapped("You kept your word. I remember the way you held your ground in the court. A watch is built from people who come back for one another. Give me Rowan's report, and take your place beside us.");
                else if(castle&&preview_stage()==PreviewChapter::Complete)ImGui::TextWrapped("Our newest watchkeeper. You will always have a place by this fire. Wear the signet well.");
                else ImGui::TextWrapped("%s",line->text.c_str());
                ImGui::Separator();const auto choices=studio.components_runtime->choices();
                for(unsigned i=0;i<choices.count;i++){const unsigned index=choices.indices[i];char choice[512];std::snprintf(choice,sizeof choice,"%u   %s",i+1,line->choices[index].text.c_str());if(button(choice)){studio.components_runtime->choose(index);break;}}
                if(ImGui::SmallButton(pad_hints?("Leave conversation  ["+cancel+"]###leave-conversation").c_str():"Leave conversation  [ESC]###leave-conversation"))studio.components_runtime->close_dialogue();
                end_panel();
            }else if(fight.engaged()){
                const float bw=std::min(560*unit,w-50*unit);begin_panel("###preview-combat",(w-bw)*.5f,h-230*unit,bw);
                ImGui::TextColored({.88f,.75f,.5f,1},"%s",fight.opponent_name());
                ImGui::ProgressBar(fight.enemy_health/std::max(1.f,fight.enemy_max_health),{-1,7*unit},"");
                ImGui::Text("HEALTH %.0f    STAMINA %.0f    %s",fight.health,fight.stamina,fight.trail_threat?"CROSSING ENCOUNTER":"WATCH TRIAL");
                if(fight.enemy_phase==TrainingEnemyPhase::Windup&&fight.state==TrainingState::Fighting){const bool now=fight.enemy_duration-fight.enemy_time<.24;ImGui::TextColored(now?ImVec4(.5f,.95f,.72f,1):ImVec4(1,.7f,.4f,1),"%s  %s",now?"PARRY NOW":"INCOMING",world_training_direction_name(fight.enemy_direction));}
                else ImGui::Text("YOUR ANGLE %s   |   ENEMY GUARD %s",world_training_direction_name(fight.selected),world_training_direction_name(fight.enemy_guard));
                if(fight.feedback_time>0)ImGui::TextWrapped("%s",fight.feedback);
                const auto controls=hint(std::string("Flick / arrows: angle   LMB: swing   RMB: guard\nSpace: dodge   Esc: pause\n")+thrust_keyboard,
                    std::string("RS: directional swing; return to center between cuts\nHold ")+preview_pad.guard_label()+" + RS: block; centered RS: thrust guard\n"+cancel+": dodge   "+pause+": pause\n"+thrust_controller);
                ImGui::TextWrapped("%s",controls.c_str());end_panel();
            }else if(fight.state==TrainingState::Lost||fight.state==TrainingState::Cancelled){
                begin_panel("###preview-retry",w*.5f-230*unit,h-200*unit,460*unit);
                ImGui::TextWrapped("%s",fight.trail_threat?"The crossing still needs you.":"Every watchkeeper begins here.");
                if(button("RETRY ENCOUNTER  [ENTER]")){
                    const auto id=fight.trail_threat?world_runtime_demo::id_base+0x40:world_runtime_demo::instructor_id;
                    if(world_presentation::queue_retry(*studio.components_runtime,live_training_encounter,id))fight.state=TrainingState::Idle;
                }end_panel();
            }
            if(preview_chapter_time>0&&!fight.engaged()&&!(studio.components_runtime&&studio.components_runtime->current_line())){
                const float opacity=std::min({1.f,preview_chapter_time,5-preview_chapter_time});
                center(h*.29f,13,IM_COL32(225,192,128,int(255*std::max(0.f,opacity))),"THE JOURNEY CONTINUES");
                center(h*.29f+30*unit,28,IM_COL32(241,237,225,int(255*std::max(0.f,opacity))),world_presentation::title(preview_chapter));
            }
        }
        if(preview_notice_time>0&&!(studio.components_runtime&&studio.components_runtime->current_line()))center(h-70*unit,14,gold,preview_notice.c_str());
        if(preview_hit>0)dl->AddRect({3,3},{w-3,h-3},IM_COL32(180,50,35,int(150*preview_hit/.35f)),0,0,8*unit);
        if(preview_parry>0&&!preview_settings.reduced_motion)dl->AddCircle({w*.5f,h*.5f},30*unit,IM_COL32(225,200,130,int(220*preview_parry/.3f)),32,2);
        if(preview_black>0||loading){
            const float alpha=loading?std::max(.88f,preview_black):preview_black;
            dl->AddRectFilled({0,0},{w,h},IM_COL32(5,13,17,int(alpha*255)));
            if(loading){center(h*.47f,24,gold,"THE ROAD THROUGH VALWOOD");center(h*.47f+46*unit,15,white,"Preparing the next chapter...");}
        }
    };
