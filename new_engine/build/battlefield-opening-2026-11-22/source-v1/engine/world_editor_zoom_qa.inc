// Real SDL wheel dispatch against an explicitly isolated /tmp authoring file.
// Only camera/UI setup is direct; every navigation action uses the production
// event handler. No terrain, objects, history entries or Applies are created.
        if(qa_editor_zoom&&studio.path.rfind("/tmp/",0)==0){
            ImGui::GetIO().IniFilename=nullptr;
            struct EditorZoomQA {
                unsigned phase=0,checks=0,test=0,focus_frames=0,applies=0,rejected=0;
                std::uint64_t since=0,next_focus=0,dolly=0,speed_events=0,revision=0,policy_changes=0;
                size_t back=0,forward=0;
                bool failed=false,dirty=false,rebuild=false;
                float speed=40;
                double full_step=0;
                Camera base,before;
                ImVec2 cursor;
                std::string draft,live;
            };static EditorZoomQA q;
            auto check=[&](bool good,const char *label){
                ++q.checks;
                if(!good){
                    int mx=0,my=0;const auto buttons=SDL_GetMouseState(&mx,&my);const auto &io=ImGui::GetIO();
                    std::fprintf(stderr,"[Editor zoom QA] FAIL phase %u test %u: %s | focus=%d capture=%d active=%d mouse=(%d,%d) buttons=%u dolly=%llu speed=%llu | %s\n",q.phase,q.test,label,int((SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0),int(io.WantCaptureMouse),int(ImGui::IsAnyItemActive()),mx,my,unsigned(buttons),(unsigned long long)editor_dolly_events,(unsigned long long)editor_speed_events,studio.message.c_str());
                    std::fflush(stderr);q.failed=true;launch->exit_code=1;running=false;
                }
                return good;
            };
            auto bytes=[&](const world_studio::Document &doc){std::string text,error;if(!world_studio::encode(doc,text,error))check(false,"encode authoring state");return text;};
            auto hover=[&](ImVec2 p){
                q.cursor=p;SDL_WarpMouseInWindow(window,int(p.x),int(p.y));SDL_PumpEvents();
                SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window);e.motion.which=0x56414c54u;e.motion.x=int(p.x);e.motion.y=int(p.y);SDL_PushEvent(&e);
                ImGui::GetIO().AddMousePosEvent(p.x,p.y);q.since=frame;
            };
            auto viewport_point=[&](){const auto v=studio.designer_viewport(ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y);return ImVec2{v.x+v.w*.5f,v.y+v.h*.5f};};
            auto wheel=[&](int x,int y,float precise_y,bool flipped=false){
                SDL_Event e{};e.type=SDL_MOUSEWHEEL;e.wheel.windowID=SDL_GetWindowID(window);e.wheel.which=0x56414c54u;e.wheel.x=x;e.wheel.y=y;e.wheel.direction=flipped?SDL_MOUSEWHEEL_FLIPPED:SDL_MOUSEWHEEL_NORMAL;
#if SDL_VERSION_ATLEAST(2,0,18)
                e.wheel.preciseX=float(x);e.wheel.preciseY=precise_y;
#else
                (void)precise_y;
#endif
#if SDL_VERSION_ATLEAST(2,26,0)
                e.wheel.mouseX=int(q.cursor.x);e.wheel.mouseY=int(q.cursor.y);
#endif
                check(SDL_PushEvent(&e)==1,"submit actual tagged wheel event");q.since=frame;
            };
            auto button=[&](bool down){
                SDL_Event e{};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.which=0x56414c54u;e.button.button=SDL_BUTTON_RIGHT;e.button.state=down?SDL_PRESSED:SDL_RELEASED;e.button.x=int(q.cursor.x);e.button.y=int(q.cursor.y);
                check(SDL_PushEvent(&e)==1,"submit actual tagged RMB event");q.since=frame;
            };
            auto key=[&](SDL_Keycode code,bool ctrl=false){
                SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.mod=ctrl?KMOD_CTRL:KMOD_NONE;e.key.keysym.unused=0x56414c54u;check(SDL_PushEvent(&e)==1,"submit tagged shortcut press");
                e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;e.key.keysym.mod=KMOD_NONE;check(SDL_PushEvent(&e)==1,"submit tagged shortcut release");q.since=frame;
            };
            auto snapshot=[&](){q.before=cam;q.speed=studio.fly_speed;q.dolly=editor_dolly_events;q.speed_events=editor_speed_events;};
            auto camera_unchanged=[&](){return cam.x==q.before.x&&cam.y==q.before.y&&cam.z==q.before.z&&cam.yaw==q.before.yaw&&cam.pitch==q.before.pitch&&cam.fov==q.before.fov;};
            auto directional=[&](int sign){
                const auto f=q.before.forward();const double dx=cam.x-q.before.x,dy=cam.y-q.before.y,dz=cam.z-q.before.z;
                const double distance=std::sqrt(dx*dx+dy*dy+dz*dz),along=dx*f.x+dy*f.y+dz*f.z;
                check(distance>1.e-5&&along*sign>1.e-5,"wheel moves in the requested camera direction");
                check(std::abs(std::abs(along)-distance)<std::max(1.e-5,distance*2.e-6),"dolly follows view forward without a sideways jump");
                check(cam.yaw==q.before.yaw&&cam.pitch==q.before.pitch&&cam.fov==q.before.fov,"dolly preserves yaw, pitch and field of view");
                check(studio.fly_speed==q.speed&&editor_speed_events==q.speed_events,"plain wheel preserves flight speed");
                check(editor_dolly_events==q.dolly+1,"one wheel event produces one dolly");
                check(cam.fly&&!cam.grounded&&cam.vy==0,"dolly owns the independent flying editor camera");
                return distance;
            };
            auto stationary=[&](){
                check(camera_unchanged(),"blocked or empty wheel leaves camera unchanged");
                check(studio.fly_speed==q.speed,"blocked or empty wheel leaves flight speed unchanged");
                check(editor_dolly_events==q.dolly&&editor_speed_events==q.speed_events,"blocked or empty wheel dispatches no navigation");
            };
            const bool focus=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0&&!ImGui::GetIO().AppFocusLost;
            const bool settled=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.apply&&!studio.designer_frame_pending&&!studio.production_freeze();
            if(!q.failed&&q.phase!=200){
                if(!focus){
                    q.focus_frames=0;q.since=frame;
                    if(frame>=q.next_focus){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();q.next_focus=frame+30;}
                }else if(++q.focus_frames>=3){
                    if(q.phase==0&&frame>50&&settled){
                        check(!studio.dirty&&!studio.drag.active&&!studio.landscape_painting,"isolated scene starts without an active edit");
                        studio.open=true;studio.designer_tools=false;studio.designer_immersive=false;studio.tab=studio.requested_tab=2;
                        look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);studio.fly_speed=40;
                        cam.y=world_atlas_height(cam.x,cam.z)+80;cam.yaw=.35f;cam.pitch=-.65f;cam.fov=1.08f;cam.fly=true;cam.grounded=false;cam.vy=0;q.base=cam;
                        q.draft=bytes(studio.draft);q.live=bytes(studio.live);q.back=studio.history.back.size();q.forward=studio.history.forward.size();q.applies=studio.applied;q.rejected=studio.rejected;q.revision=studio.revision;q.dirty=studio.dirty;q.rebuild=studio.rebuild;
                        hover(viewport_point());q.phase=1;
                    }else if(q.phase==1&&settled&&frame-q.since>5){
                        check(!editor_ui_wants_mouse(&ui)&&!ImGui::IsAnyItemActive(),"viewport hover belongs to navigation");snapshot();wheel(0,1,1);q.phase=3;q.test=0;
                    }else if(q.phase==2&&settled&&frame-q.since>4){
                        const auto v=studio.designer_viewport(ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y);
                        check(q.test==2?!v.contains(q.cursor.x,q.cursor.y):v.contains(q.cursor.x,q.cursor.y),"test wheel belongs to the intended panel or viewport");
                        if(q.test!=2)check(!editor_ui_wants_mouse(&ui)&&!ImGui::IsAnyItemActive(),"viewport input settled before wheel dispatch");
                        snapshot();
                        if(q.test==1||q.test==7)wheel(0,-1,-1);
                        else if(q.test==2)wheel(0,1,1);
                        else if(q.test==3)wheel(1,0,0);
                        else if(q.test==4)wheel(0,0,0);
                        else if(q.test==5)wheel(0,1,1,true);
                        else wheel(0,0,.5f);
                        q.phase=3;
                    }else if(q.phase==3&&frame-q.since>4){
                        if(q.test==0)q.full_step=directional(1);
                        else if(q.test==1||q.test==5)directional(-1);
                        else if(q.test==7){
                            const double indoor_step=directional(-1);
                            check(indoor_step>.1&&indoor_step<2.5,
                                  "one apartment wheel tick stays within a small interior step");
                        }
                        else if(q.test>=2&&q.test<=4)stationary();
                        else{
#if SDL_VERSION_ATLEAST(2,0,18)
                            const double half_step=directional(1);check(half_step>q.full_step*.2&&half_step<q.full_step*.8,"fractional preciseY creates a smaller real dolly");
#else
                            stationary();
#endif
                        }
                        if(!q.failed){
                            std::fprintf(stderr,"[Editor zoom QA] PASS wheel case %u (%u checks)\n",q.test,q.checks);std::fflush(stderr);++q.test;cam=q.base;
                            if(q.test<8){
                                if(q.test==7){q.base.x=-3498.;q.base.y=601.75;q.base.z=-11005.;q.base.yaw=3.14159265f;q.base.pitch=-.03f;cam=q.base;}
                                const auto v=studio.designer_viewport(ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y);
                                hover(q.test==2?ImVec2{std::max(8.f,v.x*.5f),v.y+std::min(80.f,v.h*.25f)}:viewport_point());q.phase=2;
                            }
                            else{hover(viewport_point());q.phase=4;}
                        }
                    }else if(q.phase==4&&settled&&frame-q.since>5){
                        check(!editor_ui_wants_mouse(&ui)&&!ImGui::IsAnyItemActive(),"viewport ready for an owned RMB press");button(true);q.phase=5;
                    }else if(q.phase==5&&frame-q.since>4){
                        check(look_active,"tagged viewport RMB acquires camera navigation");snapshot();wheel(0,1,1);q.phase=6;
                    }else if(q.phase==6&&frame-q.since>4){
                        check(camera_unchanged(),"owned RMB wheel adjusts speed without moving camera");
                        check(studio.fly_speed>q.speed&&editor_speed_events==q.speed_events+1,"owned RMB wheel changes flight speed exactly once");
                        check(editor_dolly_events==q.dolly,"speed adjustment produces no dolly");button(false);q.phase=7;
                    }else if(q.phase==7&&frame-q.since>4){
                        check(!look_active&&SDL_GetRelativeMouseMode()==SDL_FALSE,"RMB release returns normal pointer ownership");hover(viewport_point());key(SDLK_p,true);q.phase=8;
                    }else if(q.phase==8&&frame-q.since>5){
                        check(studio.designer_palette_open&&studio.designer_blocks_viewport(),"real Ctrl+P opens a blocking editor modal");snapshot();wheel(0,1,1);q.phase=9;
                    }else if(q.phase==9&&frame-q.since>4){
                        stationary();key(SDLK_ESCAPE);q.phase=10;
                    }else if(q.phase==10&&frame-q.since>5){
                        check(!studio.designer_palette_open&&!studio.designer_blocks_viewport(),"Escape closes the real command palette");hover(viewport_point());q.phase=11;
                    }else if(q.phase==11&&settled&&frame-q.since>5){
                        check(!editor_ui_wants_mouse(&ui)&&!ImGui::IsAnyItemActive(),"viewport ownership returns after modal closes");snapshot();wheel(0,1,1);q.phase=12;
                    }else if(q.phase==12&&frame-q.since>4){
                        directional(1);
                        check(bytes(studio.draft)==q.draft&&bytes(studio.live)==q.live,"all wheel navigation preserves exact encoded draft and live documents");
                        check(studio.history.back.size()==q.back&&studio.history.forward.size()==q.forward,"navigation creates no Undo or Redo entries");
                        check(studio.applied==q.applies&&studio.rejected==q.rejected&&!studio.apply,"navigation creates no scene Apply or rejection");
                        check(studio.revision==q.revision&&studio.dirty==q.dirty&&studio.rebuild==q.rebuild,"navigation leaves authoring revision and dirty flags unchanged");
                        if(!q.failed){
                            check(opening_interior.enabled,"apartment interior policy is available");
                            q.policy_changes=world_scenery_streaming_stats(scenery).interior_policy_changes;
                            cam.x=-3494.27;cam.y=603.66;cam.z=-11006.42;q.since=frame;q.phase=13;
                        }
                    }else if(q.phase==13&&frame-q.since>5){
                        const auto state=world_scenery_streaming_stats(scenery);
                        check(state.interior_city_suspended&&state.interior_policy_changes==q.policy_changes,
                              "near-ceiling apartment camera keeps the interior scenery domain");
                        check(house_shadow,"near-ceiling apartment camera keeps the indoor shadow scale");
                        if(!q.failed){cam.x=-3445.;cam.y=601.75;cam.z=-11005.;q.since=frame;q.phase=14;}
                    }else if(q.phase==14&&frame-q.since>5){
                        const auto state=world_scenery_streaming_stats(scenery);
                        check(!state.interior_city_suspended&&state.interior_policy_changes==q.policy_changes+1,
                              "camera beyond the apartment margin restores exterior scenery once");
                        check(!house_shadow,"camera beyond the apartment margin restores outdoor shadows");
                        if(!q.failed){q.phase=200;qa_zoom_passed=true;std::fprintf(stderr,"[Editor zoom QA] PASS %u checks; wheel, indoor scenery boundary, exact authoring documents unchanged\n",q.checks);std::fflush(stderr);}
                    }
                }
            }
            if(q.phase!=200&&!q.failed&&(frame>1400||frame+2>=uint64_t(launch->frames))){check(false,"navigation fixture completed before its bounded frame deadline");q.phase=999;}
            qa_zoom_phase=q.phase;qa_zoom_checks=q.checks;
        }
