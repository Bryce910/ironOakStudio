// Native correctness fixture only. Include in the frame loop near runtime QA.
// Every file it creates belongs to its own private /tmp directory. Apply and
// Stop go through the real host transactions; no FPS claim follows from this.
        if(qa_production&&!qa_production_failed&&qa_production_phase<9){
            struct ProductionFixture {
                std::string directory;
                world_studio::Document baseline,play_draft;
                world_prefab::Instance instance;
                world_living::State living_before;
                unsigned expected_apply=0,expected_reject=0;
                std::uint64_t phase_frame=0;
                unsigned placement=0,light=0,actor=0,particle=0,residents=0;
                world_entity::Id original_model=0;
                double x=0,y=0,z=0;
            };
            static ProductionFixture fixture;
            std::string error;
            const auto fail=[&](const char *what){
                qa_production_failed=true;launch->exit_code=1;running=false;
                std::fprintf(stderr,"[Production QA] FAIL phase %u: %s | %s | %s\n",qa_production_phase,what,studio.message.c_str(),error.c_str());
                std::fflush(stderr);
            };
            const auto check=[&](bool condition,const char *what){++qa_production_checks;if(!condition)fail(what);return condition;};
            const auto phase=[&](unsigned next,const char *what){
                qa_production_phase=next;fixture.phase_frame=frame;
                std::printf("[Production QA] phase %u: %s (%u checks)\n",next,what,qa_production_checks);std::fflush(stdout);
            };
            const auto bytes=[](const std::string &path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());};
            const auto same_document=[&](const world_studio::Document &a,const world_studio::Document &b){
                std::string error;const auto left=fixture.directory+"/compare-left.studio",right=fixture.directory+"/compare-right.studio";
                if(!world_studio::save(left.c_str(),a,error)||!world_studio::save(right.c_str(),b,error)){studio.message=error;return false;}
                return bytes(left)==bytes(right);
            };
            const auto node_id=[](const world_studio::Document &d,world_entity::Binding binding){for(const auto &n:d.entities.nodes)if(n.binding==binding)return n.id;return world_entity::Id(0);};
            const auto do_step=[&](){
                const char *enabled=std::getenv("VALWOOD_QA_PRODUCTION_WORKFLOW");
                if(!(enabled&&std::strcmp(enabled,"1")==0&&launch->frames&&!launch->benchmark)){fail("fixture requires explicit environment flag and bounded correctness launch");return;}
                if(qa_production_phase&&frame-fixture.phase_frame>1800){fail("transaction did not complete within 1800 rendered frames");return;}
                if(destination_loading||!terrain.view_ready(cam.x,cam.z,cam.fly)||frame<=30)return;
                switch(qa_production_phase){
                case 0:{
                    std::error_code ec;const auto private_path=std::filesystem::weakly_canonical(studio.path,ec);
                    const auto within=private_path.lexically_relative("/tmp");bool confined=!ec&&!within.empty()&&!within.is_absolute();
                    for(const auto &part:within)confined&=part!="..";
                    if(!check(confined&&studio.editor_session&&!studio.living_save_job.valid(),"Studio must already use a private temporary document without an active living save"))return;
                    char directory[]="/tmp/valwood-production-workflow-XXXXXX";
                    if(!check(mkdtemp(directory)!=nullptr,"create private workflow fixture directory"))return;
                    fixture.directory=directory;
                    const auto private_document=fixture.directory+"/workflow.studio";
                    auto private_session=std::make_unique<world_editor::Session>();
                    if(!check(private_session->open(private_document,studio.live,Studio::production_now(),error),"open private production session")){studio.message=error;return;}
                    studio.editor_session=std::move(private_session);studio.path=private_document;
                    studio.living_path=fixture.directory+"/player.living";studio.living_template_path=fixture.directory+"/template.living";
                    studio.open=false;studio.selection_keys.clear();studio.prefab_attachments.clear();
                    fixture.x=cam.x+55;fixture.z=cam.z+25;fixture.y=world_atlas_height(fixture.x,fixture.z);
                    fixture.placement=unsigned(studio.draft.placements.size());fixture.light=unsigned(studio.draft.lights.size());fixture.actor=unsigned(studio.draft.actors.size());fixture.particle=unsigned(studio.draft.particles.size());
                    if(!check(population&&fixture.placement+3<=world_studio::placement_limit&&fixture.light+2<=world_studio::light_limit&&fixture.actor+2<=32&&fixture.particle+2<=16,"fixture component budgets and resident library"))return;
                    fixture.residents=world_village_population_stats(population).residents;
                    // A bounded search keeps both original and translated idle
                    // actors on actual dry, capsule-clear navigation ground.
                    double actor_x=0,actor_z=0;bool walkable=false;
                    auto query=studio.nav_query(scenery);
                    for(int row=-4;row<=4&&!walkable;++row)for(int col=-4;col<=4&&!walkable;++col){
                        const double px=fixture.x+6+col*3,pz=fixture.z+4+row*3;
                        if(std::hypot(px-fixture.x,pz-fixture.z)<3||std::hypot(px-fixture.x-12,pz-fixture.z-4)<3)continue;
                        std::vector<world_navigation::Point> route;
                        const std::vector<std::array<double,2>> first={{{px,pz}}},second={{{px+12,pz+4}}};
                        if(world_navigation::path(nullptr,query,first,false,.32f,route,error)&&world_navigation::path(nullptr,query,second,false,.32f,route,error)){actor_x=px;actor_z=pz;walkable=true;}
                    }
                    if(!check(walkable,"find two real capsule-clear stationary actor positions")){studio.message=error;return;}
                    studio.checkpoint();
                    world_studio::Placement sentinel;sentinel.alias="starting_crate";sentinel.x=fixture.x-4;sentinel.z=fixture.z;studio.draft.placements.push_back(sentinel);
                    auto crate=sentinel;crate.x=fixture.x;studio.draft.placements.push_back(crate);
                    world_studio::Light light;light.x=fixture.x+1;light.y=fixture.y+1.8;light.z=fixture.z+1;light.power=3;light.radius=6;light.flicker=true;studio.draft.lights.push_back(light);
                    world_studio::Particle fire;fire.kind=2;fire.x=fixture.x+2;fire.y=fixture.y+.2;fire.z=fixture.z+2;fire.height=.5f;fire.radius=.15f;fire.power=.5f;fire.ceiling=fire.y+3;fire.light=1;fire.seed=918321;studio.draft.particles.push_back(fire);
                    world_studio::Actor actor;actor.name="Production workflow traveler";actor.appearance=3;actor.points={{{actor_x,actor_z}}};studio.draft.actors.push_back(actor);
                    studio.changed(true);
                    if(!check(studio.production_sync(error),"register real authored component IDs")){studio.message=error;return;}
                    fixture.original_model=node_id(studio.draft,{world_entity::Kind::Placement,fixture.placement+1});
                    if(!check(fixture.original_model!=0,"authored model received a persistent ID"))return;
                    studio.checkpoint();
                    if(!check(world_authoring::erase_record(studio.draft,{world_entity::Kind::Placement,fixture.placement},error),"erase pre-compaction placement")){studio.message=error;return;}
                    studio.changed(true);
                    const auto *compacted=world_entity::find(studio.draft.entities,fixture.original_model);
                    if(!check(compacted&&compacted->binding==world_entity::Binding{world_entity::Kind::Placement,fixture.placement},"compaction preserves surviving model ID"))return;
                    studio.restore_history(false);
                    if(!check(node_id(studio.draft,{world_entity::Kind::Placement,fixture.placement+1})==fixture.original_model,"Undo restores original binding without changing its ID"))return;
                    studio.checkpoint();world_entity::Id temporary=0;
                    if(!check(world_entity::add(studio.draft.entities,0,"Temporary QA identity",{}, {},temporary,error),"allocate temporary graph identity"))return;
                    studio.changed();studio.restore_history(false);
                    if(!check(!world_entity::find(studio.draft.entities,temporary)&&studio.draft.entities.next_id>temporary,"Undo retains ID high-water mark"))return;
                    world_entity::Id retained=0;
                    if(!check(world_entity::add(studio.draft.entities,0,"Retained QA identity",{}, {},retained,error)&&retained>temporary,"next identity cannot recycle an undone ID"))return;
                    world_prefab::Selection selection;selection.ground={Studio::production_ground,&studio.draft};
                    selection.objects={{world_entity::Kind::Placement,fixture.placement+1},{world_entity::Kind::Light,fixture.light},{world_entity::Kind::Actor,fixture.actor},{world_entity::Kind::Particle,fixture.particle}};
                    world_prefab::Prefab captured,portable;
                    if(!check(world_prefab::capture(studio.draft,selection,{fixture.x,fixture.y,fixture.z,0,1},"Native production workflow",captured,error),"capture mixed-component connected prefab")){studio.message=error;return;}
                    const auto prefab_path=fixture.directory+"/workflow.vwprefab";
                    if(!check(world_prefab::save(prefab_path.c_str(),captured,error)&&world_prefab::load(prefab_path.c_str(),portable,error),"portable prefab atomic save and validated reload")){studio.message=error;return;}
                    world_prefab::Ground ground{Studio::production_ground,&studio.draft};
                    if(!check(world_prefab::instantiate(portable,studio.draft,studio.draft.entities,{fixture.x+10,fixture.y,fixture.z,0,1},0,fixture.instance,error,ground),"instantiate portable connected prefab with fresh IDs")){studio.message=error;return;}
                    if(!check(world_prefab::transform_instance(studio.draft,studio.draft.entities,fixture.instance.root,{fixture.x+12,world_atlas_height(fixture.x+12,fixture.z+4),fixture.z+4,0,1},error,ground),"transform connected model/light/actor/fire bundle")){studio.message=error;return;}
                    if(!check(node_id(studio.draft,{world_entity::Kind::Placement,fixture.placement+2})!=fixture.original_model&&fixture.instance.objects.size()==4,"cloned components receive independent persistent IDs"))return;
                    if(!check(world_studio::valid(studio.draft,error),"complete authored fixture validates")){studio.message=error;return;}
                    studio.changed(true);fixture.expected_apply=studio.applied+1;fixture.expected_reject=studio.rejected;studio.apply=true;
                    phase(1,"authored prefab staged through actual Apply");break;
                }
                case 1:
                    if(studio.rejected!=fixture.expected_reject){fail("initial authored fixture was rejected");return;}
                    if(studio.applied<fixture.expected_apply)return;
                    if(!check(world_entity::find(studio.live.entities,fixture.instance.root)&&studio.live.actors.size()==fixture.actor+2&&studio.live.particles.size()==fixture.particle+2&&studio.live.lights.size()==fixture.light+2,"actual committed mixed-component prefab"))return;
                    if(!check(world_village_population_stats(population).residents>=fixture.residents+2,"both authored stationary NPCs entered the runtime population"))return;
                    fixture.baseline=studio.live;studio.checkpoint();
                    {auto missing=studio.draft.placements.back();missing.alias="qa_missing_production_model";studio.draft.placements.push_back(missing);}
                    studio.changed(true);fixture.expected_reject=studio.rejected+1;studio.apply=true;
                    phase(2,"invalid model staged to verify failed Apply preserves live world");break;
                case 2:
                    if(studio.applied!=fixture.expected_apply){fail("invalid scene unexpectedly committed");return;}
                    if(studio.rejected<fixture.expected_reject)return;
                    if(!check(same_document(studio.live,fixture.baseline),"failed Apply preserves complete prior live document"))return;
                    studio.restore_history(false);studio.apply=true;fixture.expected_apply=studio.applied+1;
                    phase(3,"Undo restored valid draft for a normal Apply");break;
                case 3:{
                    if(studio.rejected!=fixture.expected_reject){fail("repaired scene was rejected");return;}
                    if(studio.applied<fixture.expected_apply)return;
                    fixture.baseline=studio.live;
                    if(!check(studio.save(),"save baseline to private Studio path"))return;
                    world_studio::Document saved;
                    if(!check(world_studio::load(studio.path.c_str(),saved,error)&&same_document(saved,fixture.baseline),"baseline Studio5 save/reload preserves graph and all components")){studio.message=error;return;}
                    world_living::Definition design=studio.living_ready?studio.living_world.definition():world_living::Definition{};design.enabled=true;design.start_minute=600;
                    if(!studio.living_ready){
                        design.factions.push_back({1,"Private fixture",{}});
                        world_living::Settlement settlement;settlement.id=settlement.faction=1;settlement.name="Private fixture settlement";settlement.stock={20,20,20,20};settlement.reserve={1000,1000,1000,0};design.settlements.push_back(settlement);
                        world_living::Actor resident;resident.id=resident.faction=resident.home=1;resident.name="Private fixture worker";design.actors.push_back(resident);
                    }
                    const double wall=std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
                    if(!check(studio.living_world.configure(design,wall,error),"seed logical living world only in private process memory")){studio.message=error;return;}
                    studio.living_design=design;studio.living_ready=true;studio.living_modified=studio.living_template_dirty=false;studio.living_clock_poll=Studio::production_now()+1e9;
                    fixture.living_before=studio.living_world.state();
                    studio.checkpoint();studio.draft.look.exposure=fixture.baseline.look.exposure<1.5f?fixture.baseline.look.exposure+.125f:fixture.baseline.look.exposure-.125f;studio.changed();
                    if(!check(studio.production_begin(),"begin actual editor Play with unapplied draft edits"))return;
                    fixture.play_draft=studio.draft;
                    const auto saved_bytes=bytes(studio.path);
                    if(!check(studio.production_playing()&&!studio.production_can_save()&&!studio.save()&&bytes(studio.path)==saved_bytes,"project Save is blocked during Play without changing the private file"))return;
                    studio.checkpoint();studio.draft.look.exposure=fixture.play_draft.look.exposure<1.5f?fixture.play_draft.look.exposure+.25f:fixture.play_draft.look.exposure-.25f;studio.changed();
                    fixture.expected_apply=studio.applied+1;studio.apply=true;
                    phase(4,"Play edit staged through normal runtime Apply");break;
                }
                case 4:{
                    if(studio.rejected!=fixture.expected_reject){fail("Apply during Play was rejected");return;}
                    if(studio.applied<fixture.expected_apply)return;
                    if(!check(studio.production_playing()&&studio.live.look.exposure!=fixture.baseline.look.exposure,"actual Play edit committed to live state"))return;
                    const double later=fixture.living_before.last_wall_milliseconds/1000.+180;
                    if(!check(studio.living_world.observe_clock(later,error)&&studio.living_world.process(2).steps==2&&studio.living_world.state().tick>fixture.living_before.tick,"logical simulation advances during Play")){studio.message=error;return;}
                    if(!check(studio.editor_session->request_stop(),"request actual editor Stop"))return;
                    studio.production_stop(world_editor::StopChoice::DiscardPlayChanges);
                    if(!check(studio.production_restore&&studio.production_freeze()&&!studio.production_can_save(),"Discard stages restoration and holds gameplay/Save"))return;
                    fixture.expected_apply=studio.applied+1;
                    phase(5,"Discard waiting for normal restoration transaction");break;
                }
                case 5:{
                    if(studio.rejected!=fixture.expected_reject||studio.production_restore_failed){fail("Discard restoration Apply failed");return;}
                    if(studio.applied<fixture.expected_apply||studio.production_restore)return;
                    if(!check(!studio.production_playing()&&studio.editor_session->stats().mode==world_editor::Mode::Editing,"Stop returns editor to Editing"))return;
                    if(!check(same_document(studio.live,fixture.baseline),"Discard restores the complete pre-Play live baseline"))return;
                    if(!check(same_document(studio.draft,fixture.play_draft)&&studio.dirty,"Stop retains the separate pre-Play unapplied draft"))return;
                    const auto &state=studio.living_world.state();
                    if(!check(state.tick==fixture.living_before.tick&&state.debt_milliseconds==fixture.living_before.debt_milliseconds&&state.consumed==fixture.living_before.consumed&&state.settlements[0].stock==fixture.living_before.settlements[0].stock&&state.settlements[0].reserve==fixture.living_before.settlements[0].reserve,"Discard restores logical simulation balances, schedules and debt"))return;
                    if(!check(!studio.save(),"retained unapplied draft still requires Apply before Save"))return;
                    studio.apply=true;fixture.expected_apply=studio.applied+1;
                    phase(6,"retained authoring draft explicitly staged for final Apply");break;
                }
                case 6:{
                    if(studio.rejected!=fixture.expected_reject){fail("final retained-draft Apply was rejected");return;}
                    if(studio.applied<fixture.expected_apply)return;
                    if(!check(studio.save(),"final private project save after Apply"))return;
                    world_studio::Document loaded;
                    if(!check(world_studio::load(studio.path.c_str(),loaded,error)&&same_document(loaded,studio.live),"final durable IDs, hierarchy and attachments survive Studio5 reload")){studio.message=error;return;}
                    if(!check(bytes(studio.path).rfind("VALWOOD_STUDIO 5\n",0)==0&&!loaded.entities.nodes.empty(),"saved file contains actual Studio5 entity metadata"))return;
                    std::printf("[Production QA] PASS: %u checks, %u actual commits, %u expected rejection(s); private fixture %s. Correctness only; no FPS result.\n",qa_production_checks,studio.applied,studio.rejected,fixture.directory.c_str());std::fflush(stdout);
                    phase(9,"completed production workflow smoke");break;
                }
                default:fail("unknown fixture phase");break;
                }
            };
            do_step();
        }
