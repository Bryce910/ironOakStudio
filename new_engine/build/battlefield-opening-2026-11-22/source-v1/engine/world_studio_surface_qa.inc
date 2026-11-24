// Frame-bounded, temporary-document-only workflow. Never changes world.studio.
const bool qa_surfaces=launch->frames&&studio.path!="world.studio"&&std::getenv("VALWOOD_QA_SURFACE_EDITORS");
unsigned qa_surface_phase=0,qa_surface_wait=0;GLuint qa_surface_texture=0;double qa_surface_x=0,qa_surface_z=0;float qa_surface_height=0;
auto surface_workflow=[&](unsigned frame){
    if(!qa_surfaces)return;
    auto fail=[&](const char *reason){std::fprintf(stderr,"[Surface QA] FAIL phase%u: %s | %s\n",qa_surface_phase,reason,studio.message.c_str());launch->exit_code=1;running=false;qa_surface_phase=99;};
    if(qa_surface_phase==0&&frame>120){
        bool found=false;const auto previous_radius=studio.radius;studio.radius=30;
        for(int z=-2;z<=2&&!found;z++)for(int x=-2;x<=2&&!found;x++){
            studio.x=cam.x+x*96;studio.z=cam.z+z*96;studio.y=world_atlas_height(studio.x,studio.z);
            if(studio.safe_brush()){qa_surface_x=studio.x;qa_surface_z=studio.z;found=true;}
        }
        studio.radius=previous_radius;if(!found){fail("No protected-route-safe test plot");return;}
        studio.checkpoint();auto &doc=studio.draft;doc.surfaces={};doc.graphs.clear();
        const char *sources[]={"ground/village_stone_BC.png","ground/village_stone_N.png","ground/village_stone_ARM.png"};
        for(unsigned i=0;i<3;i++){auto imported=world_surface::import_image(std::filesystem::current_path().string(),sources[i],world_surface::ImageRole(i));if(!imported.ok){studio.message=imported.error;fail("Image import");return;}imported.image.name="QA image "+std::to_string(i);doc.surfaces.images.push_back(imported.image);}
        world_surface::Layer layer;layer.name="QA masonry";layer.color=0;layer.normal=1;layer.arm=2;layer.flip_normal_y=true;layer.metres=1;layer.roughness=1;doc.surfaces.layers.push_back(layer);
        const double px=qa_surface_x,pz=qa_surface_z,ground=world_atlas_height(px,pz);
        doc.surfaces.paint.push_back({px,pz,8,.8f,0,false});world_surface::Projection projection;projection.position={px,ground,pz};projection.layer=0;projection.opacity=.3f;doc.surfaces.projections.push_back(projection);
        world_surface::Wind wind;wind.center={px,ground+3,pz};wind.radius={32,12,32};wind.speed=2;doc.surfaces.winds.push_back(wind);
        world_surface::Ribbon road;road.layer=0;road.width=6;road.shoulder=3;road.points={{px-14,ground,pz+10},{px+14,ground,pz+10}};doc.surfaces.ribbons.push_back(road);
        world_surface::Ribbon river;river.kind=world_surface::RibbonKind::River;river.layer=-1;river.width=8;river.shoulder=3;river.depth=1.4f;river.points={{px-14,ground+.1,pz-10},{px+14,ground+.1,pz-10}};doc.surfaces.ribbons.push_back(river);
        world_studio::Actor actor;actor.name="Surface QA resident";actor.appearance=0;actor.points.push_back({px+4,pz+12});doc.actors.push_back(actor);
        doc.graphs.push_back(world_graph::preset("__terrain__",0));doc.graphs.push_back(world_graph::preset("__water_pond__",0));doc.graphs.push_back(world_graph::preset("__npc_0__",0));
        studio.changed(true);studio.apply=true;studio.open=true;studio.requested_tab=22;qa_surface_phase=1;qa_surface_wait=frame;
    }else if(qa_surface_phase==1&&studio.applied>=1){
        if(studio.rejected||!studio.live.surface_field||studio.live.surfaces.images.size()!=3||studio_surface_gpu.rivers.ranges.size()!=1||!studio_surface_gpu.texture){fail("First Apply did not connect all surface resources");return;}
        float water=0,depth=0;if(!world_atlas_local_water(qa_surface_x,qa_surface_z-10,&water,&depth)||depth<1){fail("River query is not connected to carved terrain");return;}
        if(std::abs(world_atlas_height(qa_surface_x,qa_surface_z+10)-studio.live.surfaces.ribbons[0].points[0].y)>.02){fail("Road grade differs from rendered/collision height");return;}
        qa_surface_texture=studio_surface_gpu.texture;qa_surface_height=world_atlas_height(qa_surface_x,qa_surface_z+10);
        // Verify original roof/sky/canopy data survives the combined texture.
        GLint active=0,old=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&old);
        std::vector<unsigned char> original(513*513*6*4),combined(size_t(studio_surface_gpu.bytes));glBindTexture(GL_TEXTURE_2D_ARRAY,landscape.masks);glGetTexImage(GL_TEXTURE_2D_ARRAY,0,GL_RGBA,GL_UNSIGNED_BYTE,original.data());glBindTexture(GL_TEXTURE_2D_ARRAY,studio_surface_gpu.texture);glGetTexImage(GL_TEXTURE_2D_ARRAY,0,GL_RGBA,GL_UNSIGNED_BYTE,combined.data());glBindTexture(GL_TEXTURE_2D_ARRAY,GLuint(old));glActiveTexture(GLenum(active));
        if(!std::equal(original.begin(),original.end(),combined.begin())){fail("Base landscape field bytes changed");return;}
        studio.checkpoint();studio.draft.surfaces.images[0].path="surfaces/missing_qa.img";studio.changed();studio.apply=true;qa_surface_phase=2;qa_surface_wait=frame;
    }else if(qa_surface_phase==2&&studio.rejected==1){
        if(studio_surface_gpu.texture!=qa_surface_texture||world_atlas_height(qa_surface_x,qa_surface_z+10)!=qa_surface_height||!studio.history.undo(studio.draft)){fail("Missing image rollback");return;}
        studio.changed();studio.apply=true;qa_surface_phase=3;
    }else if(qa_surface_phase==3&&studio.applied==2){
        qa_surface_texture=studio_surface_gpu.texture;studio.reload_shaders=true;studio.fail_shader_test=true;studio.apply=true;qa_surface_phase=4;
    }else if(qa_surface_phase==4&&studio.rejected==2){
        if(studio_surface_gpu.texture!=qa_surface_texture||world_atlas_height(qa_surface_x,qa_surface_z+10)!=qa_surface_height){fail("Shader rejection changed live surfaces");return;}
        studio.checkpoint();studio.draft.surfaces.ribbons[0].points[0].y+=100;studio.changed(true);studio.apply=true;qa_surface_phase=5;
    }else if(qa_surface_phase==5&&studio.rejected==3){
        if(!studio.history.undo(studio.draft)||world_atlas_height(qa_surface_x,qa_surface_z+10)!=qa_surface_height){fail("Unsafe grade rollback");return;}
        studio.changed(true);studio.apply=true;qa_surface_phase=6;
    }else if(qa_surface_phase==6&&studio.applied==3){
        world_studio::Document loaded;std::string error;if(!studio.save()||!world_studio::load(studio.path.c_str(),loaded,error)||loaded.surfaces.ribbons.size()!=2||loaded.surfaces.images.size()!=3||loaded.graphs.size()!=3||!loaded.surfaces.layers[0].flip_normal_y){studio.message=error;fail("Studio5/surface2 save/reload");return;}
        cam.x=qa_surface_x+12;cam.z=qa_surface_z+22;cam.y=world_atlas_height(cam.x,cam.z)+5;cam.yaw=std::atan2(-12.f,22.f);cam.pitch=-.20f;cam.fly=true;cam.grounded=false;third_person=false;temporal_cut=true;studio.requested_tab=22;qa_surface_phase=7;launch->frames=int(frame)+40;
        std::printf("[Surface QA] PASS:3 applies/3 rejected transactions, immutable imports, landscape field bytes, road/collision height, river query/mesh, wind/material/graph resources, undo and Studio5/surface2 persistence\n");std::fflush(stdout);
    }
    if(qa_surface_phase>0&&qa_surface_phase<7&&frame>qa_surface_wait+650){fail("Workflow timeout");}
};
