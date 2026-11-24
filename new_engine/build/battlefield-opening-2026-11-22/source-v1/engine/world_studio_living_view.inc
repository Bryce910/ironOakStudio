// Included inside Studio. Persistent logical simulation has an explicit save;
// no model inference, disk read or scene reconfiguration occurs in frame ticks.
    world_living::World living_world;
    world_living::Definition living_design;
    std::future<std::pair<bool,std::string>> living_save_job;
    std::string living_path,living_template_path;
    double living_clock_poll=0,living_save_clock=0;
    bool living_ready=false,living_modified=false,living_template_dirty=false;
    world_micro_policy::Model living_model;
    world_micro_policy::Context living_policy;
    bool living_model_ready=false,living_model_enabled=false;
    char living_model_file[512]="ai/economic_policy.vwml";
    unsigned living_pending=0;
    world_living::World living_play_world;
    world_living::Definition living_play_design;
    bool living_play_ready=false,living_play_modified=false,living_play_template_dirty=false;
    void living_play_begin(){living_play_world=living_world;living_play_design=living_design;living_play_ready=living_ready;living_play_modified=living_modified;living_play_template_dirty=living_template_dirty;}
    void living_play_stop(world_editor::StopChoice choice){if(choice==world_editor::StopChoice::DiscardPlayChanges){living_world=living_play_world;living_design=living_play_design;living_ready=living_play_ready;living_modified=living_play_modified;living_template_dirty=living_play_template_dirty;std::string ignored;living_world.rebase_clock(std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(),ignored);living_clock_poll=production_now()+1;living_save_clock=production_now();}}

    void living_init(){
        living_template_path=(std::filesystem::path(project_directory)/"world.living").string();
        living_path=(std::filesystem::path(project_directory)/"saves/world.living").string();
        std::error_code ec;const bool saved=std::filesystem::exists(living_path,ec),authored=std::filesystem::exists(living_template_path,ec);
        const bool newer_template=saved&&authored&&std::filesystem::last_write_time(living_template_path,ec)>std::filesystem::last_write_time(living_path,ec);
        std::string error;
        if(saved&&!newer_template){if(living_world.load(living_path,error)){living_design=living_world.definition();living_ready=true;}else message="Living world was not loaded: "+error;}
        else if(authored){world_living::World initial;if(initial.load(living_template_path,error)){
            const double wall=std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
            if(living_world.configure(initial.definition(),wall,error)){living_design=living_world.definition();living_ready=living_modified=true;}
        }if(!error.empty())message="Living-world design was not loaded: "+error;}
    }
    void living_save(bool synchronous=false){
        if(production_playing()||production_freeze()){message="Stop Play before saving persistent world state.";return;}
        if(!living_ready||living_save_job.valid())return;
        auto snapshot=living_world;const auto file=living_path,source=living_template_path;const bool publish_template=living_template_dirty;
        auto store=[snapshot,file,source,publish_template](){
            std::string error;std::error_code ec;std::filesystem::create_directories(std::filesystem::path(file).parent_path(),ec);if(ec)return std::make_pair(false,ec.message());
            if(publish_template){world_living::World initial;if(!initial.configure(snapshot.definition(),0,error)||!initial.save(source,error))return std::make_pair(false,error);}
            const bool ok=snapshot.save(file,error);return std::make_pair(ok,error);
        };
        if(synchronous){auto result=store();if(!result.first)message=result.second;else living_modified=living_template_dirty=false;return;}
        living_save_job=std::async(std::launch::async,store);
        living_save_clock=production_now();living_modified=living_template_dirty=false;
    }
    void living_service(bool runtime_allowed,bool allow_io){
        if(living_save_job.valid()&&living_save_job.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){auto result=living_save_job.get();if(!result.first){message="Living-world save failed: "+result.second;living_modified=living_template_dirty=true;}}
        if(!living_ready||!runtime_allowed)return;
        const double now=production_now();std::string error;
        if(now>=living_clock_poll){
            const double wall=std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
            if(!living_world.observe_clock(wall,error)){message=error;return;}living_clock_poll=now+1;
        }
        const auto result=living_world.process(4,living_model_ready&&living_model_enabled?world_micro_policy::policy:nullptr,&living_policy);living_pending=unsigned(std::min<std::uint64_t>(result.pending_steps,UINT_MAX));living_modified|=result.steps>0;
        if(allow_io&&living_modified&&!production_playing()&&now-living_save_clock>=30)living_save();
    }
    void living_shutdown(bool allow_save){
        if(living_save_job.valid()){auto result=living_save_job.get();if(!result.first)living_modified=living_template_dirty=true;}
        if(allow_save&&living_modified&&!production_playing()&&!production_freeze())living_save(true);
    }
    void living_seed_design(){
        world_living::Definition d;d.enabled=true;d.factions.push_back({1,"Valwood",{}});
        world_living::Settlement castle;castle.id=1;castle.faction=1;castle.name="Castle district";castle.stock={500,100,80,250};castle.reserve={50000,20000,15000,0};castle.capacity={2000,2000,2000,2000};
        auto camp=castle;camp.id=2;camp.name="Jungle camp";camp.stock={120,50,10,20};d.settlements={castle,camp};
        for(unsigned i=0;population_ptr&&i<world_village_resident_count;i++){
            WorldVillageResident person;if(!world_village_population_resident(population_ptr,i,&person))continue;
            world_living::Actor actor;actor.id=i+1;actor.faction=actor.home=1;actor.name=person.name?person.name:("Resident "+std::to_string(i+1));
            actor.guard=person.role&&(std::strstr(person.role,"knight")||std::strstr(person.role,"archer"));actor.work_resource=world_living::Resource(i%3);
            if(i%11==0&&!actor.guard)actor.trade_target=2;
            d.actors.push_back(std::move(actor));
        }
        living_design=std::move(d);message="Living-world design created from the current residents. Review it, then explicitly start/reset the simulation.";
    }
    void living_panel(){
        if(!ImGui::BeginTabItem("Living world",nullptr,requested_tab==16?ImGuiTabItemFlags_SetSelected:0))return;
        tab=16;
        ImGui::TextWrapped("Persistent factions, settlement resources and NPC schedules. Offline time is processed on resume with a bounded catch-up queue. The optional581-parameter policy learns synthetic economic rules; authoritative world rules validate every proposal.");
        ImGui::InputText("Small policy file",living_model_file,sizeof living_model_file);
        if(ImGui::Button("Load small economic policy")){auto file=std::filesystem::path(living_model_file);if(file.is_relative())file=std::filesystem::path(project_directory)/file;std::string error;if(world_micro_policy::load(file.u8string().c_str(),living_model,error)){living_model_ready=true;living_policy={&living_model};message="Small economic policy loaded. Enable it explicitly to use its proposals.";}else message=error;}
        if(living_model_ready){ImGui::Checkbox("Use learned economic policy",&living_model_enabled);ImGui::Text("Inference %llu | feasible proposals %llu | rule fallbacks %llu",(unsigned long long)living_policy.inferences,(unsigned long long)living_policy.accepted,(unsigned long long)living_policy.fallbacks);}
        if(ImGui::Button("Create design from castle residents"))living_seed_design();
        ImGui::Checkbox("Simulation enabled",&living_design.enabled);
        int minute=int(living_design.start_minute),debt=int(living_design.maximum_debt_seconds/86400);
        if(ImGui::SliderInt("Initial minute of day",&minute,0,1439))living_design.start_minute=unsigned(minute);
        if(ImGui::SliderInt("Offline catch-up cap (days)",&debt,1,30))living_design.maximum_debt_seconds=unsigned(debt)*86400;
        for(unsigned i=0;i<living_design.actors.size();i++){
            auto &a=living_design.actors[i];ImGui::PushID(int(i));if(ImGui::TreeNode(a.name.c_str())){
                int wake=int(a.wake_minute),start=int(a.work_start),end=int(a.work_end),sleep=int(a.sleep_minute),resource=int(a.work_resource);
                if(ImGui::SliderInt("Wake minute",&wake,0,1439))a.wake_minute=unsigned(wake);
                if(ImGui::SliderInt("Work begins",&start,0,1439))a.work_start=unsigned(start);
                if(ImGui::SliderInt("Work ends",&end,0,1439))a.work_end=unsigned(end);
                if(ImGui::SliderInt("Sleep minute",&sleep,0,1439))a.sleep_minute=unsigned(sleep);
                ImGui::Checkbox("Guard duty",&a.guard);
                if(ImGui::Combo("Work resource",&resource,"Food\0Wood\0Ore\0Coin\0"))a.work_resource=world_living::Resource(resource);
                ImGui::TreePop();
            }ImGui::PopID();
        }
        if(ImGui::Button("Start / reset to this design")){
            std::string error;const double wall=std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
            if(production_playing()||production_freeze())message="Stop Play before resetting persistent world state.";
            else if(living_save_job.valid())message="Wait for the current world save before resetting.";
            else if(living_world.configure(living_design,wall,error)){living_ready=living_modified=living_template_dirty=true;living_clock_poll=0;living_save_clock=production_now();message="Persistent simulation started. Save stores its authored starting design and independent player state.";}else message=error;
        }
        if(ImGui::Button("Save living world"))living_save();
        ImGui::SameLine();
        if(ImGui::Button("Reload living world")&&!living_save_job.valid()&&!production_playing()&&!production_freeze()){
            std::string error;if(living_world.load(living_path,error)){living_ready=true;living_modified=false;living_design=living_world.definition();living_clock_poll=0;message="Living world loaded. Elapsed time will catch up in bounded steps.";}else message=error;
        }
        if(living_ready){
            const auto &state=living_world.state();const auto &definition=living_world.definition();
            ImGui::Text("Day %llu | %02u:%02u | pending %u minutes",(unsigned long long)living_world.day(),living_world.minute()/60,living_world.minute()%60,living_pending);
            if(state.dropped_milliseconds)ImGui::TextWrapped("Elapsed time beyond the configured debt cap: %.1f hours",state.dropped_milliseconds/3600000.);
            for(unsigned i=0;i<definition.settlements.size();i++){const auto &s=state.settlements[i];ImGui::Text("%s | food %llu | wood %llu | ore %llu | coin %llu",definition.settlements[i].name.c_str(),(unsigned long long)s.stock[0],(unsigned long long)s.stock[1],(unsigned long long)s.stock[2],(unsigned long long)s.stock[3]);}
            ImGui::BeginChild("Schedule state",{0,150},true);for(unsigned i=0;i<definition.actors.size();i++)ImGui::Text("%s | %s | actions %llu",definition.actors[i].name.c_str(),world_living::activity_name(living_world.activity(i)),(unsigned long long)state.actors[i].actions);ImGui::EndChild();
        }
        memory_panel();
        ImGui::TextWrapped("These are logical schedules and resource accounts. Physical work locations and home routes need explicit navigation bindings. The policy handles resource actions; it does not generate language or navigation. Food and natural reserves are finite.");
        ImGui::EndTabItem();
    }
