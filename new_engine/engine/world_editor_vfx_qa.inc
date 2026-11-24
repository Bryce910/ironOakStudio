// Bounded native integration against an isolated /tmp document. Button and
// slider input enters through ImGui's public mouse API, never private IDs.
        if(launch->frames&&!launch->benchmark&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_EDITOR_VFX")){
            ImGui::GetIO().IniFilename=nullptr;
            struct VfxQA {unsigned phase=0,checks=0;std::uint64_t since=0;size_t history=0;unsigned applies=0;double paused=0;float power=0;bool failed=false;ImVec4 drag;};static VfxQA q;
            auto check=[&](bool good,const char *label){++q.checks;if(!good){std::fprintf(stderr,"[VFX QA] FAIL phase %u: %s | %s\n",q.phase,label,studio.message.c_str());std::fflush(stderr);launch->exit_code=1;q.failed=true;}return good;};
            auto mouse=[&](ImVec4 rect,bool down,float fraction=.5f){auto &io=ImGui::GetIO();const float mx=rect.x+(rect.z-rect.x)*fraction,my=(rect.y+rect.w)*.5f;SDL_WarpMouseInWindow(window,int(mx),int(my));io.AddMousePosEvent(mx,my);io.AddMouseButtonEvent(0,down);q.since=frame;};
            auto click=[&](unsigned control){mouse(studio.vfx_controls[control],true);};
            auto release=[&](){ImGui::GetIO().AddMouseButtonEvent(0,false);q.since=frame;};
            auto key=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.mod=KMOD_CTRL;SDL_PushEvent(&e);e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;SDL_PushEvent(&e);q.since=frame;};
            const bool settled=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.dirty&&!studio.apply&&!studio.designer_frame_pending;
            if(q.phase==0&&frame>40&&settled){studio.open=true;studio.designer_tools=true;studio.tab=studio.requested_tab=11;studio.designer_content=false;studio.vfx_library_open=true;q.phase=1;q.since=frame;}
            else if(q.phase==1&&frame-q.since>3){std::fprintf(stderr,"[VFX QA] Add rect %.1f %.1f %.1f %.1f, tool %d, tools %d\n",studio.vfx_controls[0].x,studio.vfx_controls[0].y,studio.vfx_controls[0].z,studio.vfx_controls[0].w,studio.tab,studio.designer_tools);click(0);q.phase=2;}
            else if(q.phase==2&&frame-q.since>=1){release();q.phase=3;}
            else if(q.phase==3&&settled&&frame-q.since>5&&studio.draft.particles.size()==1){
                check(studio.live.particles.size()==1&&studio.live.particles[0].kind==4,"real Add button commits arcane wisps");check(studio.vfx_name(0)=="Arcane wisps","effect has a persistent outliner name");check(studio.vfx_previews>0&&studio.vfx_previewing,"bounded live preview is active");check(world_scene_particles_stats(scene_particles).visible_emitters>0,"particle draw has visible emitters");
                click(1);q.phase=4;
            }else if(q.phase==4&&frame-q.since>=1){release();q.phase=5;}
            else if(q.phase==5&&frame-q.since>3){check(!studio.vfx_clock.playing,"Pause button freezes simulation");q.paused=studio.vfx_clock.time;q.since=frame;q.phase=6;}
            else if(q.phase==6&&frame-q.since>8){check(studio.vfx_clock.time==q.paused,"paused preview clock stays fixed");mouse(studio.vfx_controls[16],true,.25);q.phase=7;}
            else if(q.phase==7&&frame-q.since>=1){release();q.phase=8;}
            else if(q.phase==8&&frame-q.since>3){check(studio.vfx_clock.time>12&&studio.vfx_clock.time<20&&!studio.vfx_clock.playing,"timeline scrubs to a held preview time");q.power=studio.draft.particles[0].power;q.history=studio.history.back.size();q.applies=studio.applied;q.drag=studio.vfx_controls[5];mouse(q.drag,true,.25);q.phase=9;}
            else if(q.phase==9&&frame-q.since>1){mouse(q.drag,true,.38);q.phase=10;}
            else if(q.phase==10&&frame-q.since>1){check(studio.applied==q.applies,"slider drag does not rebuild scene mid-gesture");check(studio.draft.particles[0].power!=q.power&&studio.vfx_previews>1,"slider changes draft and immediate preview");release();q.phase=11;}
            else if(q.phase==11&&settled&&frame-q.since>3){check(studio.history.back.size()==q.history+1,"one slider gesture is one undo step");check(studio.applied==q.applies+1,"finished gesture commits once");key(SDLK_z);q.phase=12;}
            else if(q.phase==12&&settled&&frame-q.since>8){check(std::abs(studio.draft.particles[0].power-q.power)<.001,"Ctrl+Z restores intensity");key(SDLK_y);q.phase=13;}
            else if(q.phase==13&&settled&&frame-q.since>8){check(std::abs(studio.draft.particles[0].power-q.power)>.01,"Ctrl+Y restores edited intensity");click(3);q.phase=14;}
            else if(q.phase==14&&frame-q.since>=1){release();q.phase=15;}
            else if(q.phase==15&&settled&&frame-q.since>5&&studio.draft.particles.size()==2){check(studio.live.particles.size()==2&&studio.live.particles[1].seed!=studio.live.particles[0].seed,"Duplicate button creates a separate live variation");click(2);q.phase=16;}
            else if(q.phase==16&&frame-q.since>=1){release();q.phase=17;}
            else if(q.phase==17&&frame-q.since>4){Studio::RuntimePrepared solo;std::string error;check(studio.vfx_solo&&studio.prepare_vfx(studio.draft,solo,error,studio.particle_selection),"Solo checkbox prepares selected effect");unsigned enabled=0;for(const auto &p:solo.particles)enabled+=p.strength>0;for(const auto &p:solo.flames)enabled+=p.intensity>0;check(enabled==1,"solo excludes other authored and ambient emitters");click(4);q.phase=18;}
            else if(q.phase==18&&frame-q.since>=1){release();q.phase=19;}
            else if(q.phase==19&&settled&&studio.draft.particles.size()==1){check(studio.live.particles.size()==1&&!studio.vfx_solo,"Remove button updates runtime and clears solo");
                studio.vfx_library_open=true;studio.vfx_preset=1;q.phase=23;q.since=frame;
            }else if(q.phase==23&&frame-q.since>3){click(18);q.phase=24;}
            else if(q.phase==24&&frame-q.since>=1){release();q.phase=25;}
            else if(q.phase==25&&frame-q.since>3){check(studio.vfx_placing,"Place on ground button arms viewport placement");const auto &p=studio.draft.particles[0];ImVec2 at;check(studio.project_point(cam,{p.x,world_atlas_height(p.x,p.z),p.z},at),"ground below effect is visible");mouse({at.x-1,at.y-1,at.x+1,at.y+1},true);q.phase=26;}
            else if(q.phase==26&&frame-q.since>=1){release();q.phase=27;}
            else if(q.phase==27&&settled&&studio.draft.particles.size()==2){const auto &p=studio.draft.particles[1];check(p.kind==3&&std::abs(p.y-world_atlas_height(p.x,p.z)-.08)<.01&&!studio.vfx_placing,"real viewport click places selected VFX on terrain");key(SDLK_z);q.phase=28;}
            else if(q.phase==28&&settled&&frame-q.since>8){check(studio.draft.particles.size()==1,"terrain VFX placement undoes as one action");
                // Exercise every renderer/preset at real world coordinates.
                const auto p=studio.draft.particles[0];for(unsigned i=1;i<8;++i)check(studio.vfx_add(i,p.x+(int(i%4)-1)*2.8,p.y,p.z+(i/4)*3.5,false),"place remaining VFX preset");studio.particle_selection=0;studio.vfx_frame();q.phase=20;q.since=frame;
            }else if(q.phase==20&&settled&&frame-q.since>6){check(studio.live.particles.size()==8&&studio.rejected==0,"all eight presets pass scene and renderer validation");check(studio.save(),"save authored effects");world_studio::Document loaded;std::string error;check(world_studio::load(studio.path.c_str(),loaded,error)&&loaded.particles.size()==8&&loaded.particles[0].tint==studio.live.particles[0].tint&&loaded.particles[0].glow==studio.live.particles[0].glow,"saved effects reload with tint and glow");
                studio.tab=studio.requested_tab=2;q.phase=21;q.since=frame;
            }else if(q.phase==21&&frame-q.since>4){check(!studio.vfx_previewing,"leaving VFX restores the normal simulation clock");studio.tab=studio.requested_tab=11;studio.designer_tools=true;studio.vfx_library_open=false;q.phase=22;q.since=frame;
                std::fprintf(stderr,"[VFX QA] PASS %u checks; presets, real Add/Pause/Scrub/Drag/Duplicate/Solo/Remove buttons, Undo/Redo, live preview and save/reload\n",q.checks);std::fflush(stderr);
            }
            if(q.phase!=22&&q.phase!=99&&frame>1000){check(false,"fixture completed before deadline");q.phase=99;}
            if(q.failed)q.phase=99;
        }
