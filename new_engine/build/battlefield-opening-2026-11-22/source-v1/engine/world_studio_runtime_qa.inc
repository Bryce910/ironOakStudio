        if(qa_runtime){
            if(qa_runtime_phase==0&&frame>30){
                studio.checkpoint();double ox,oz;world_atlas_village_origin(&ox,&oz);
                world_studio::NavigationArea area;area.name="QA street navigation";area.x=ox-131;area.z=oz-6;area.span=64;area.cell=2;studio.draft.navigation.push_back(area);
                for(unsigned i=0;i<2;i++){world_studio::Actor a;a.name=i?"QA returning traveler":"QA street traveler";a.appearance=i?5:3;a.navigation=0;a.points={{{ox-130-i*1.0,oz-18}},{{ox-130-i*1.0,oz+8}}};studio.draft.actors.push_back(a);}
                world_studio::Actor hidden;hidden.name="QA disabled source";hidden.source=8;hidden.appearance=1;hidden.enabled=false;hidden.points={{{ox-35,oz-72}}};studio.draft.actors.push_back(hidden);
                studio.edit_particle_source(0);studio.draft.particles.back().rate=.75f;studio.draft.particles.back().lifetime=1.3f;
                world_studio::Particle fire;fire.kind=2;fire.x=ox-145;fire.y=24.2;fire.z=oz-69;fire.radius=.18;fire.height=.55;fire.ceiling=27;fire.light=4;fire.seed=98211;studio.draft.particles.push_back(fire);
                world_studio::Water water;water.source=2;water.waves=.85f;water.roughness=1.1f;studio.draft.waters.push_back(water);
                bool pond=false;for(int zz=320;zz<600&&!pond;zz+=20)for(int xx=280;xx<600&&!pond;xx+=20){double x=ox+xx,z=oz+zz;float y=world_atlas_height(x,z);if(y<6||world_atlas_village_weight(x,z)>.001||world_atlas_local_water(x,z,nullptr,nullptr)||!world_scenery_can_move(scenery,x,z,x,z,y+.01f,1.75f,10))continue;bool flat=true;for(unsigned i=0;i<16;i++){double a=i*6.283185307/16;flat&=std::abs(world_atlas_height(x+std::cos(a)*10,z+std::sin(a)*10)-y)<.8;}if(!flat)continue;auto next=studio.draft;if(!world_studio::carve_pond(next,x,z,world_atlas_base_height,studio.message))continue;studio.draft=std::move(next);qa_pond_x=x;qa_pond_z=z;pond=true;}
                if(!pond){std::fprintf(stderr,"[Runtime QA] Cannot find a safe closed-basin fixture\n");launch->exit_code=1;running=false;}
                studio.draft.graphs.push_back(world_graph::preset("bep_prop_barrel",0));studio.draft.graphs.push_back(world_graph::preset("bep_prop_bucket",2));
                studio.graph_selection=0;studio.requested_tab=13;studio.open=true;studio.changed(true);studio.apply=true;qa_runtime_phase=1;
            }else if(qa_runtime_phase==1&&studio.applied==1){
                float level=0,depth=0;if(!world_atlas_local_water(qa_pond_x,qa_pond_z,&level,&depth)||depth<.08||world_village_population_stats(population).residents<141){std::fprintf(stderr,"[Runtime QA] Pond or character commit failed\n");launch->exit_code=1;running=false;}
                studio.checkpoint();const auto *pool=world_atlas_rainforest_pool();studio.draft.actors[0].points[0]={pool->x,pool->z};studio.changed();studio.apply=true;qa_runtime_phase=2;
            }else if(qa_runtime_phase==2&&studio.rejected==1){
                studio.restore_history(false);studio.apply=true;qa_runtime_phase=3;
            }else if(qa_runtime_phase==3&&studio.applied==2){
                studio.checkpoint();studio.draft.waters.back().level+=10;studio.changed(true);studio.apply=true;qa_runtime_phase=4;
            }else if(qa_runtime_phase==4&&studio.rejected==2){
                studio.restore_history(false);studio.checkpoint();studio.draft.graphs[0].nodes[0].op=world_graph::Op::Multiply;studio.draft.graphs[0].nodes[0].inputs={2,2,-1};studio.changed();studio.apply=true;qa_runtime_phase=5;
            }else if(qa_runtime_phase==5&&studio.rejected==3){
                studio.restore_history(false);studio.reload_shaders=true;studio.fail_shader_test=true;studio.apply=true;qa_runtime_phase=6;
            }else if(qa_runtime_phase==6&&studio.rejected==4){
                studio.reload_shaders=true;studio.apply=true;qa_runtime_phase=7;
            }else if(qa_runtime_phase==7&&studio.applied==3){
                world_studio::Document check;std::string error;if(!studio.save()||!world_studio::load(studio.path.c_str(),check,error)||check.actors.size()!=3||check.particles.size()!=2||check.waters.size()!=2||check.graphs.size()!=2){std::fprintf(stderr,"[Runtime QA] Save/reload failed: %s\n",error.c_str());launch->exit_code=1;running=false;}qa_runtime_phase=8;studio.runtime_qa_ready=true;studio.requested_tab=13;std::printf("[Runtime QA] PASS:3 commits/4 rejects, routes, animation, particles, water, graph and v3 reload\n");std::fflush(stdout);
            }
        }
