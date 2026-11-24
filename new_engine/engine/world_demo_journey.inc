    world_town::State town_state;
    bool journey_open=false;
    std::string journey_notice;
    auto progress_file=[&](){const char *qa=launch->frames?std::getenv("VALWOOD_QA_PROGRESS_FILE"):nullptr;return qa&&*qa?std::string(qa):(std::filesystem::path(studio.project_directory)/(presentation?"saves/presentation.progress":"saves/demo.progress")).string();};
    auto save_journey=[&](){
        if(!studio.components_ready||!studio.components_runtime||fight.engaged()||destination_loading||studio.production_playing()||studio.production_freeze()){journey_notice="Finish training or loading before saving progress.";return false;}
        if(cam.fly){journey_notice="Return to walking before saving player progress.";return false;}
        const auto path=progress_file();std::error_code ec;std::filesystem::create_directories(std::filesystem::path(path).parent_path(),ec);if(ec){journey_notice=ec.message();return false;}
        world_runtime::Runtime::Player player;player.feet={cam.x,cam.y-1.75,cam.z};player.yaw=cam.yaw;player.pitch=cam.pitch;player.third_person=third_person;player.town=town_state;
        if(!studio.components_runtime->save_progress(path.c_str(),player,journey_notice))return false;
        journey_notice="Progress saved: player, journal, rewards, NPC experiences, living world and physical props.";return true;
    };
    auto load_journey=[&](){
        if(!studio.components_ready||!studio.components_runtime||fight.engaged()||destination_loading||studio.production_playing()||studio.production_freeze()){journey_notice="Finish training or loading before restoring progress.";return false;}
        world_runtime::Runtime::Player player;if(!studio.components_runtime->load_progress(progress_file().c_str(),player,Studio::components_wall(),journey_notice))return false;
        town_state=player.town;
        cam.x=player.feet.x;cam.y=player.feet.y+1.75;cam.z=player.feet.z;cam.yaw=float(player.yaw);cam.pitch=float(player.pitch);cam.vy=0;cam.fly=false;cam.grounded=true;third_person=player.third_person;
        studio.components_runtime->read_frame(*studio.components_frame);live_training_encounter=0;fight.leave();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
        temporal_cut=true;destination_loading=true;current_name="Saved journey";journey_notice="Progress restored. NPC memories and the journal are ready.";return true;
    };
    auto draw_journey=[&](){
        if(!journey_open||studio.open||!studio.components_runtime)return;
        ImGui::SetNextWindowSize({410,0},ImGuiCond_FirstUseEver);ImGui::SetNextWindowPos({18,74},ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Journey | Valwood",&journey_open,ImGuiWindowFlags_AlwaysAutoResize)){
            const auto *document=studio.components_runtime->gameplay_document();
            if(document)for(const auto &quest:document->quests){const auto progress=studio.components_runtime->quest(quest.id);const char *status=progress==world_gameplay::Progress::Complete?"COMPLETE":progress==world_gameplay::Progress::Active?"ACTIVE":progress==world_gameplay::Progress::Failed?"FAILED":"AVAILABLE";
                ImGui::TextColored({.95f,.79f,.48f,1},"%s | %s",quest.name.c_str(),status);
                if(progress==world_gameplay::Progress::Inactive)ImGui::TextWrapped("Speak to the master of arms in the west training court to begin.");
                for(const auto &objective:quest.objectives){const int have=std::min(objective.required,studio.components_runtime->counter(objective.counter));ImGui::TextWrapped("%s %s (%d/%d)",have>=objective.required?"[x]":"[ ]",objective.text.c_str(),have,objective.required);}
            }
            ImGui::Separator();ImGui::Text("Purse: %d coin | Watch reputation: %d",studio.components_runtime->counter(world_runtime_demo::coin_counter),studio.components_runtime->counter(world_runtime_demo::renown_counter));
            ImGui::TextWrapped("J: journal | E: talk | V: first/third person\nF5: save progress | F8: load progress | F9: next demo location\nThe journal objective is optional. Free exploration and sparring remain available.");
            ImGui::BeginDisabled(fight.engaged()||destination_loading);if(ImGui::Button("Save progress (F5)"))save_journey();ImGui::SameLine();if(ImGui::Button("Load progress (F8)"))load_journey();ImGui::EndDisabled();
            if(!journey_notice.empty())ImGui::TextWrapped("%s",journey_notice.c_str());
        }ImGui::End();
    };
