// Extended authoring regression. Only actual SDL/ImGui input may commit an
// edit; camera, tab and selection setup do not fabricate document contents.
        if(qa_landscape_refine&&qa_landscape&&qa_landscape_passed&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_EDITOR_LANDSCAPE_REOPEN")==nullptr){
            struct RefineQA {
                unsigned phase=0,checks=0,layer=1,commits=0,applies=0,focus=0;std::uint64_t since=0,started=0;
                bool failed=false,pending=false,cover_wait_logged=false;size_t history=0,areas=0,water=0,ribbon=0;
                ImVec2 requested{},point{};world_studio::Document before,after;world_studio::Water old_water;
                double x=0,z=0,restore_height=0,water_height=0,old_point_x=0,old_point_z=0,new_x=0,new_z=0;
                std::array<int,3> owners{};std::array<float,3> densities{};float radius_before=0,depth_before=0;
                std::array<std::vector<WorldModelInstance>,3> roots;
                std::array<double,3> anchor_x{},anchor_z{};std::array<std::uint64_t,3> generation{};
            };static RefineQA rq;
            auto check=[&](bool good,const char *label){++rq.checks;if(!good){std::fprintf(stderr,"[Landscape Refine QA] FAIL phase %u: %s | %s | requested=(%.1f,%.1f), layers=%u, pond_commits=%u rejects=%u move=%d, point_commits=%u rejects=%u move=%d preview=%d: %s\n",rq.phase,label,studio.message.c_str(),rq.requested.x,rq.requested.y,studio.landscape_layer_mask,studio.water_geometry_commits,studio.water_geometry_rejections,int(studio.water_move_armed),studio.surface_point_commits,studio.surface_point_rejections,int(studio.surface_point_move_armed),int(studio.surface_point_preview_valid),studio.surface_point_preview_error.c_str());std::fflush(stderr);rq.failed=true;launch->exit_code=1;running=false;}return good;};
            auto bytes=[&](const world_studio::Document &doc){std::string text,error;check(world_studio::encode(doc,text,error),"encode observed document");return text;};
            auto emit=[&](ImVec2 at,int button){SDL_WarpMouseInWindow(window,int(at.x),int(at.y));SDL_PumpEvents();SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window);e.motion.which=0x56414c54u;e.motion.x=int(at.x);e.motion.y=int(at.y);SDL_PushEvent(&e);ImGui::GetIO().AddMousePosEvent(at.x,at.y);if(button>=0){e={};e.type=button?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.which=0x56414c54u;e.button.button=SDL_BUTTON_LEFT;e.button.state=button?SDL_PRESSED:SDL_RELEASED;e.button.x=int(at.x);e.button.y=int(at.y);SDL_PushEvent(&e);ImGui::GetIO().AddMouseButtonEvent(0,button!=0);}};
            auto mouse=[&](ImVec2 at,int button){rq.requested=at;rq.pending=button==1;emit(at,button==1?-1:button);rq.since=frame;};
            auto release=[&](){mouse(rq.requested,0);};
            auto key=[&](SDL_Keycode code,bool ctrl=false,bool shift=false){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.unused=0x56414c54u;e.key.keysym.mod=SDL_Keymod((ctrl?KMOD_CTRL:0)|(shift?KMOD_SHIFT:0));SDL_PushEvent(&e);e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;e.key.keysym.mod=KMOD_NONE;SDL_PushEvent(&e);rq.since=frame;};
            auto visible=[&](ImVec2 at){const auto &io=ImGui::GetIO();if(at.x<=0||at.y<=0)return false;if(at.y>160&&at.y<io.DisplaySize.y-80)return true;if(frame-rq.since<4)return false;emit({io.DisplaySize.x-100,io.DisplaySize.y*.65f},-1);SDL_Event e{};e.type=SDL_MOUSEWHEEL;e.wheel.windowID=SDL_GetWindowID(window);e.wheel.which=0x56414c54u;e.wheel.y=at.y<160?4:-4;e.wheel.direction=SDL_MOUSEWHEEL_NORMAL;
#if SDL_VERSION_ATLEAST(2,0,18)
                e.wheel.preciseY=float(e.wheel.y);
#endif
                SDL_PushEvent(&e);rq.since=frame;return false;};
            auto screen=[&](double x,double z,ImVec2 &at){auto &io=ImGui::GetIO();return studio.project_point(cam,{x,world_atlas_height(x,z)+.02,z},at)&&studio.designer_viewport(io.DisplaySize.x,io.DisplaySize.y).contains(at.x,at.y);};
            auto frame_at=[&](double x,double z,double radius){cam.pitch=-1.2f;cam.fly=true;studio.designer_frame_center={x,world_atlas_height(x,z),z};studio.designer_frame_radius=radius;studio.designer_frame_reveal=false;studio.designer_frame_pending=true;rq.since=frame;};
            // Framing moves the real camera without entering destination_loading.
            // A terrain-ready frame can therefore still contain the previous
            // camera's published Rock/Fern fields while the near cover stage
            // works through grass. Observe only the current completed fields.
            WorldCoverMetrics cover_metrics{};world_cover_metrics(dressing.cover,&cover_metrics);
            bool cover_ready=dressing.cover&&!cover_metrics.near_building&&!cover_metrics.tree_building;
            for(unsigned k=1;k<4;++k){const auto &field=dressing.fields[k];const double step=k==WORLD_COVER_TREES?128.:24.;
                const double expected_x=std::floor(render_cam.x/step+.5)*step,expected_z=std::floor(render_cam.z/step+.5)*step;
                const auto expected_generation=k==WORLD_COVER_TREES?cover_metrics.tree_generation:cover_metrics.near_generation;
                cover_ready=cover_ready&&field.generation==expected_generation&&field.anchor_x==expected_x&&field.anchor_z==expected_z;
            }
            auto trace_fields=[&](const char *label){
                std::fprintf(stderr,"[Landscape Refine QA] FIELDS %s phase=%u cam=(%.6f,%.6f) near=(%llu,%d,%zu/%zu) trees=(%llu,%d,%zu/%zu)\n",label,rq.phase,render_cam.x,render_cam.z,(unsigned long long)cover_metrics.near_generation,int(cover_metrics.near_building),cover_metrics.near_done,cover_metrics.near_total,(unsigned long long)cover_metrics.tree_generation,int(cover_metrics.tree_building),cover_metrics.tree_done,cover_metrics.tree_total);
                for(unsigned k=1;k<4;++k){const auto &field=dressing.fields[k];const double step=k==WORLD_COVER_TREES?128.:24.;std::uint64_t hash=1469598103934665603ull;
                    const auto *raw=reinterpret_cast<const unsigned char *>(field.instances);for(size_t i=0;i<field.count*sizeof(WorldModelInstance);++i){hash^=raw[i];hash*=1099511628211ull;}
                    std::fprintf(stderr,"[Landscape Refine QA] FIELD kind=%u generation=%llu anchor=(%.0f,%.0f) expected=(%.0f,%.0f) count=%zu hash=%016llx before_generation=%llu before_anchor=(%.0f,%.0f) before_count=%zu\n",k,(unsigned long long)field.generation,field.anchor_x,field.anchor_z,std::floor(render_cam.x/step+.5)*step,std::floor(render_cam.z/step+.5)*step,field.count,(unsigned long long)hash,(unsigned long long)rq.generation[k-1],rq.anchor_x[k-1],rq.anchor_z[k-1],rq.roots[k-1].size());
                }std::fflush(stderr);
            };
            if(rq.phase==1&&frame-rq.since>6&&!cover_ready&&!rq.cover_wait_logged){trace_fields("waiting-for-current-camera");rq.cover_wait_logged=true;}
            const bool settled=cover_ready&&!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.dirty&&!studio.apply&&!studio.designer_frame_pending&&!studio.production_freeze();
            auto trace_water=[&](const char *label){
                const auto &io=ImGui::GetIO();const bool selected=studio.water_selection>=0&&unsigned(studio.water_selection)<studio.draft.waters.size();
                std::fprintf(stderr,"[Landscape Refine QA] WATER %s phase=%u selected=%d revision=%llu buffered=(radius=%.6f,depth=%.6f) before=(radius=%.6f,depth=%.6f) draft=(radius=%.6f,depth=%.6f) mouse=(%.1f,%.1f) radius_track=(%.1f,%.1f) depth_track=(%.1f,%.1f) ctrl=%d shift=%d mouse_down=%d active=%d\n",label,rq.phase,studio.water_selection,(unsigned long long)studio.revision,double(studio.water_geometry_radius),double(studio.water_geometry_depth),double(rq.radius_before),double(rq.depth_before),selected?double(studio.draft.waters[unsigned(studio.water_selection)].radius_x):-1.,selected?double(studio.draft.waters[unsigned(studio.water_selection)].basin_depth):-1.,io.MousePos.x,io.MousePos.y,studio.water_geometry_radius_slider.x,studio.water_geometry_radius_slider.y,studio.water_geometry_depth_slider.x,studio.water_geometry_depth_slider.y,int(io.KeyCtrl),int(io.KeyShift),int(io.MouseDown[0]),int(ImGui::IsAnyItemActive()));std::fflush(stderr);
            };
            const bool focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0&&!ImGui::GetIO().AppFocusLost;
            if(!rq.started)rq.started=SDL_GetTicks64();
            if(!focused){rq.focus=0;if(frame%30==0){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);}rq.since=frame;}
            else if(++rq.focus>=2&&!rq.failed){
                if(rq.pending&&frame>rq.since){emit(rq.requested,1);rq.pending=false;rq.since=frame;}
                if(rq.phase==0&&settled){
                    if(check(!studio.live.ecology.empty()&&studio.live.waters.size()>=2&&!studio.live.surfaces.ribbons.empty(),"existing rendered fixture contains forest, Pond, Lake and river")){
                        rq.before=studio.draft;const auto &area=studio.live.ecology.back();rq.x=area.x;rq.z=area.z;rq.areas=studio.live.ecology.size();
                        // The original river crosses the old forest centre.
                        // Find an unprotected point inside that same region;
                        // no alternate document or authored geometry is used.
                        bool plant_spot=false;for(unsigned i=0;i<32&&!plant_spot;++i){const double a=i*6.283185307179586/32,x=area.x+std::cos(a)*area.radius*.65,z=area.z+std::sin(a)*area.radius*.65;if(!studio.landscape_live_protected(0,x,z,&studio)){rq.x=x;rq.z=z;plant_spot=true;}}
                        check(plant_spot,"existing forest has clear ground outside its river banks");
                        for(size_t i=studio.live.waters.size()-2;i<studio.live.waters.size();++i){const auto &w=studio.live.waters[i];check(w.enabled&&w.source==3&&w.basin_depth>0,"both real-click water bodies coexist as derived basins");bool dry=true;for(unsigned j=0;j<8;++j){const double a=j*6.283185307179586/8,x=w.x+std::cos(a)*w.radius_x,z=w.z+std::sin(a)*w.radius_z;dry&=world_atlas_height(x,z)>w.level+.02&&!world_atlas_local_water(x,z,nullptr,nullptr);}check(dry&&world_atlas_height(w.x,w.z)<w.level-.25,"replayed Pond and Lake each retain submerged floor and full dry rim");}
                        for(int k=1;k<4;++k){rq.owners[unsigned(k-1)]=world_ecology::at_layer(studio.live.ecology.data(),studio.live.ecology.size(),rq.x,rq.z,k);const int owner=rq.owners[unsigned(k-1)];rq.densities[unsigned(k-1)]=owner<0?0:world_ecology::density(studio.live.ecology[unsigned(owner)],k,rq.x,rq.z);}
                        // The preceding legacy regression ended with a synthetic
                        // Ctrl+Shift+S. Release its modifiers through real SDL
                        // key-up events before this independent mouse workflow.
                        for(const auto code:{SDLK_LCTRL,SDLK_LSHIFT}){SDL_Event e{};e.type=SDL_KEYUP;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_RELEASED;e.key.keysym.sym=code;e.key.keysym.mod=KMOD_NONE;e.key.keysym.unused=0x56414c54u;SDL_PushEvent(&e);}
                        studio.open=true;studio.designer_content=false;studio.designer_tools=true;studio.designer_immersive=false;studio.tab=studio.requested_tab=15;studio.landscape_page=1;frame_at(rq.x,rq.z,48);rq.phase=1;
                    }
                }else if(rq.phase==1&&settled&&frame-rq.since>6){
                    if(check(studio.landscape_layer_mask==15,"independent brush layers initially enabled")){
                        // Copy the published renderer inputs, not just region
                        // metadata. The camera stays fixed through phase9.
                        for(unsigned k=1;k<4;++k){const auto &field=dressing.fields[k];rq.anchor_x[k-1]=field.anchor_x;rq.anchor_z[k-1]=field.anchor_z;rq.generation[k-1]=field.generation;if(field.count)rq.roots[k-1].assign(field.instances,field.instances+field.count);else rq.roots[k-1].clear();}
                        trace_fields("before-grass-paint");check(cover_ready,"pre-paint snapshot contains completed current-camera cover generations");check(!rq.roots[0].empty(),"forest has actual generated tree instances before grass-only paint");rq.phase=2;
                    }
                }else if(rq.phase==2&&frame-rq.since>4){if(visible(studio.landscape_layer_buttons[rq.layer])){mouse(studio.landscape_layer_buttons[rq.layer],1);rq.phase=3;}}
                else if(rq.phase==3&&frame-rq.since>1){release();rq.phase=4;}
                else if(rq.phase==4&&frame-rq.since>4){check(!(studio.landscape_layer_mask&(1u<<rq.layer)),"real layer checkbox independently disables its layer");if(++rq.layer<4)rq.phase=2;else{check(studio.landscape_layer_mask==1,"actual checkbox clicks select grass only");rq.phase=5;}}
                else if(rq.phase==5&&frame-rq.since>4){if(visible(studio.landscape_buttons[8])){mouse(studio.landscape_buttons[8],1);rq.phase=6;}}
                else if(rq.phase==6&&frame-rq.since>1){release();rq.phase=7;}
                else if(rq.phase==7&&settled&&frame-rq.since>4){check(studio.landscape_plant==4,"real Grass brush button selected");rq.history=studio.history.back.size();rq.applies=studio.applied;if(check(screen(rq.x,rq.z,rq.point),"forest centre projects into actual viewport")){mouse(rq.point,1);rq.phase=8;}}
                else if(rq.phase==8&&frame-rq.since>3){release();rq.phase=9;}
                else if(rq.phase==9&&settled&&frame-rq.since>8){
                    trace_fields("after-grass-paint");check(cover_ready,"post-paint observation contains completed current-camera cover generations");
                    check(studio.live.ecology.size()==rq.areas+1&&world_ecology::layer_mask(studio.live.ecology.back())==1,"grass paint publishes an independently owned grass area");
                    check(studio.applied==rq.applies+1&&studio.history.back.size()==std::min(size_t(32),rq.history+1),"grass paint commits exactly one Apply and Undo");
                    for(int k=1;k<4;++k){const int owner=world_ecology::at_layer(studio.live.ecology.data(),studio.live.ecology.size(),rq.x,rq.z,k);check(owner==rq.owners[unsigned(k-1)],"grass paint preserves live tree/rock/fern field ownership");if(owner>=0)check(world_ecology::same(studio.live.ecology[unsigned(owner)],rq.before.ecology[unsigned(owner)])&&world_ecology::density(studio.live.ecology[unsigned(owner)],k,rq.x,rq.z)==rq.densities[unsigned(k-1)],"grass paint preserves live tree/rock/fern field contents and density");}
                    for(unsigned k=1;k<4;++k){const auto &field=dressing.fields[k];const auto &before=rq.roots[k-1];const bool anchors=field.anchor_x==rq.anchor_x[k-1]&&field.anchor_z==rq.anchor_z[k-1],count=field.count==before.size();check(anchors&&count,"grass-only paint preserves actual tree/rock/fern renderer anchors and instance counts");check(anchors&&count&&(!field.count||std::memcmp(field.instances,before.data(),field.count*sizeof(WorldModelInstance))==0),"grass-only paint preserves every byte of actual generated tree/rock/fern renderer instances");}
                    key(SDLK_z,true);rq.phase=10;
                }else if(rq.phase==10&&frame-rq.since>4){key(SDLK_s,true,true);rq.phase=11;}
                else if(rq.phase==11&&settled&&frame-rq.since>6){
                    check(bytes(studio.draft)==bytes(rq.before),"one real Undo restores complete pre-grass document");
                    rq.water=studio.live.waters.size()-1;rq.old_water=studio.live.waters[rq.water];check(rq.old_water.basin_depth>0,"saved Lake is an editable derived basin");
                    studio.tab=studio.requested_tab=12;studio.water_selection=int(rq.water);frame_at(rq.old_water.x,rq.old_water.z,100);rq.phase=20;
                }else if(rq.phase==20&&settled&&frame-rq.since>6){
                    rq.radius_before=studio.water_geometry_radius;rq.depth_before=studio.water_geometry_depth;
                    trace_water("before-slider-input");check(!ImGui::GetIO().KeyCtrl&&!ImGui::GetIO().KeyShift,"shortcut key-up releases modifiers before mouse slider editing");
                    if(visible(studio.water_geometry_radius_slider)){mouse(studio.water_geometry_radius_slider,1);rq.phase=21;}
                }else if(rq.phase==21&&frame-rq.since>1){auto at=studio.water_geometry_radius_slider;at.x-=48;mouse(at,-1);rq.phase=211;}
                else if(rq.phase==211&&frame-rq.since>2){release();rq.phase=22;}
                else if(rq.phase==22&&frame-rq.since>4){if(visible(studio.water_geometry_depth_slider)){trace_water("after-radius-drag");mouse(studio.water_geometry_depth_slider,1);rq.phase=23;}}
                else if(rq.phase==23&&frame-rq.since>1){auto at=studio.water_geometry_depth_slider;at.x-=48;mouse(at,-1);rq.phase=231;}
                else if(rq.phase==231&&frame-rq.since>2){release();rq.phase=24;}
                else if(rq.phase==24&&frame-rq.since>4){
                    trace_water("after-depth-drag");
                    check(std::abs(studio.water_geometry_radius-rq.radius_before)>.1&&std::abs(studio.water_geometry_depth-rq.depth_before)>.1,"real held slider drags change both buffered basin radius and depth");
                    check(bytes(studio.draft)==bytes(rq.before)&&!studio.dirty,"basin slider preview leaves document and terrain unchanged until Apply");
                    if(visible(studio.water_geometry_button)){rq.history=studio.history.back.size();rq.applies=studio.applied;rq.commits=studio.water_geometry_commits;mouse(studio.water_geometry_button,1);rq.phase=25;}
                }else if(rq.phase==25&&frame-rq.since>1){release();rq.phase=26;}
                else if(rq.phase==26&&settled&&frame-rq.since>6){
                    trace_water("after-basin-apply");
                    check(studio.water_geometry_commits==rq.commits+1&&studio.applied==rq.applies+1&&studio.history.back.size()==std::min(size_t(32),rq.history+1),"Apply basin size publishes exactly one geometry edit and Undo");
                    const auto &water=studio.live.waters[rq.water];check(std::abs(water.radius_x-rq.radius_before)>.1&&std::abs(water.basin_depth-rq.depth_before)>.1,"live basin radius and depth follow buffered Apply");
                    check(world_atlas_height(water.x,water.z)<water.level-.25&&std::abs(world_atlas_walk_height(water.x,water.z)-world_atlas_height(water.x,water.z))<.02,"resized basin rebuilds matching submerged rendered and collision floors");
                    rq.after=studio.draft;rq.water_height=world_atlas_height(water.x,water.z);auto restored=studio.live;restored.waters[rq.water].enabled=false;std::string error;
                    if(check(world_pond_field::prepare(restored,{world_atlas_base_height},error),"read-only reference replay removes selected pond"))rq.restore_height=world_atlas_base_height(water.x,water.z)+world_studio::height_delta(restored,water.x,water.z)+(restored.surface_field?world_surface::terrain_offset(*restored.surface_field,water.x,water.z):0);
                    bool found=false;auto source=studio.landscape_pond_source();
                    const double clearance=water.radius_x+std::max(4.,double(water.radius_x)*.25)+2.9,distance=water.radius_x*2+25;
                    for(unsigned i=0;i<32&&!found;++i){const double a=i*6.283185307179586/32,x=water.x+std::cos(a)*distance,z=water.z+std::sin(a)*distance;auto next=studio.draft;next.waters[rq.water].x=x;next.waters[rq.water].z=z;std::string why;if(studio.landscape_water_footprint(x,z,clearance,why,int(rq.water))&&world_pond_field::prepare(next,source,why)){rq.new_x=x;rq.new_z=z;found=true;}}
                    if(check(found,"read-only preflight finds a clear nonoverlapping pond destination")){frame_at((water.x+rq.new_x)*.5,(water.z+rq.new_z)*.5,130);rq.phase=27;}
                }else if(rq.phase==27&&settled&&frame-rq.since>6){if(visible(studio.water_move_button)){mouse(studio.water_move_button,1);rq.phase=28;}}
                else if(rq.phase==28&&frame-rq.since>1){release();rq.phase=29;}
                else if(rq.phase==29&&frame-rq.since>4){check(studio.water_move_armed,"actual Pick new pond position button arms move");rq.history=studio.history.back.size();rq.applies=studio.applied;rq.commits=studio.water_geometry_commits;if(check(screen(rq.new_x,rq.new_z,rq.point),"new pond position visible in viewport")){mouse(rq.point,1);rq.phase=30;}}
                else if(rq.phase==30&&frame-rq.since>3){release();rq.phase=31;}
                else if(rq.phase==31&&settled&&frame-rq.since>6){
                    trace_water("after-pond-move");
                    const auto &water=studio.live.waters[rq.water];check(std::hypot(water.x-rq.new_x,water.z-rq.new_z)<1,"armed viewport click moves existing pond to actual picked ground");
                    check(studio.water_geometry_commits==rq.commits+1&&studio.applied==rq.applies+1&&studio.history.back.size()==std::min(size_t(32),rq.history+1)&&studio.live.waters.size()==rq.before.waters.size(),"pond move changes existing object in one Undo without duplication");
                    check(std::abs(world_atlas_height(rq.old_water.x,rq.old_water.z)-rq.restore_height)<.02&&std::abs(world_atlas_walk_height(rq.old_water.x,rq.old_water.z)-rq.restore_height)<.02,"moving pond restores original rendered and collision ground at former basin");
                    check(world_atlas_height(water.x,water.z)<water.level-.25,"moved water has a newly carved submerged basin");key(SDLK_z,true);rq.phase=32;
                }else if(rq.phase==32&&frame-rq.since>4){key(SDLK_s,true,true);rq.phase=33;}
                else if(rq.phase==33&&settled&&frame-rq.since>6){check(bytes(studio.draft)==bytes(rq.after),"one Undo restores resized pond at original location");check(std::abs(world_atlas_height(rq.old_water.x,rq.old_water.z)-rq.water_height)<.02,"pond move Undo restores previous submerged floor");key(SDLK_z,true);rq.phase=34;}
                else if(rq.phase==34&&frame-rq.since>4){key(SDLK_s,true,true);rq.phase=35;}
                else if(rq.phase==35&&settled&&frame-rq.since>6){
                    check(bytes(studio.draft)==bytes(rq.before),"second Undo restores complete original Pond and Lake state");rq.ribbon=studio.live.surfaces.ribbons.size()-1;const auto &r=studio.live.surfaces.ribbons[rq.ribbon];
                    if(check(r.points.size()>=2,"existing river has numbered editable points")){studio.tab=studio.requested_tab=23;studio.surface_ribbon=int(rq.ribbon);studio.surface_point_selection=0;rq.old_point_x=r.points[1].x;rq.old_point_z=r.points[1].z;const double dx=r.points[0].x-r.points[1].x,dz=r.points[0].z-r.points[1].z,length=std::hypot(dx,dz);rq.new_x=r.points[1].x+dx/length;rq.new_z=r.points[1].z+dz/length;frame_at((r.points[0].x+r.points[1].x)*.5,(r.points[0].z+r.points[1].z)*.5,42);rq.phase=40;}
                }else if(rq.phase==40&&settled&&frame-rq.since>6){const auto &p=studio.live.surfaces.ribbons[rq.ribbon].points[1];if(check(studio.project_point(cam,{p.x,p.y+.3,p.z},rq.point),"numbered existing river handle projects into view")){mouse(rq.point,1);rq.phase=41;}}
                else if(rq.phase==41&&frame-rq.since>1){release();rq.phase=42;}
                else if(rq.phase==42&&frame-rq.since>4){check(studio.surface_point_selection==1,"actual numbered viewport handle selects existing point two");if(visible(studio.surface_point_move_button)){mouse(studio.surface_point_move_button,1);rq.phase=43;}}
                else if(rq.phase==43&&frame-rq.since>1){release();rq.phase=44;}
                else if(rq.phase==44&&frame-rq.since>4){check(studio.surface_point_move_armed&&studio.surface_point_move_ribbon==int(rq.ribbon),"real point position button arms the selected river handle");rq.history=studio.history.back.size();rq.applies=studio.applied;rq.commits=studio.surface_point_commits;if(check(screen(rq.new_x,rq.new_z,rq.point),"new river handle terrain position visible")){mouse(rq.point,1);rq.phase=45;}}
                else if(rq.phase==45&&frame-rq.since>3){release();rq.phase=46;}
                else if(rq.phase==46&&settled&&frame-rq.since>6){
                    const auto &p=studio.live.surfaces.ribbons[rq.ribbon].points[1];check(std::hypot(p.x-rq.old_point_x,p.z-rq.old_point_z)>.25&&std::hypot(p.x-rq.new_x,p.z-rq.new_z)<1,"real terrain click moves existing selected river point");
                    check(studio.surface_point_commits==rq.commits+1&&studio.applied==rq.applies+1&&studio.history.back.size()==std::min(size_t(32),rq.history+1)&&studio.live.surfaces.ribbons.size()==rq.before.surfaces.ribbons.size(),"river point move publishes one Undo and retains the original river object");
                    check(p.y==rq.before.surfaces.ribbons[rq.ribbon].points[1].y&&!studio_surface_gpu.rivers.ranges.empty(),"river handle edit preserves channel height and rebuilds rendered river");key(SDLK_z,true);rq.phase=47;
                }else if(rq.phase==47&&frame-rq.since>4){key(SDLK_s,true,true);rq.phase=48;}
                else if(rq.phase==48&&settled&&frame-rq.since>6){check(bytes(studio.draft)==bytes(rq.before),"one real Undo restores complete pre-handle document");if(visible(studio.surface_point_move_button)){rq.history=studio.history.back.size();rq.applies=studio.applied;rq.commits=studio.surface_point_commits;mouse(studio.surface_point_move_button,1);rq.phase=49;}}
                else if(rq.phase==49&&frame-rq.since>1){release();rq.phase=50;}
                else if(rq.phase==50&&frame-rq.since>4){check(studio.surface_point_move_armed,"second river preview arms through actual button");if(screen(rq.new_x,rq.new_z,rq.point))mouse(rq.point,-1);key(SDLK_ESCAPE);rq.phase=51;}
                else if(rq.phase==51&&frame-rq.since>6){check(!studio.surface_point_move_armed&&bytes(studio.draft)==bytes(rq.before),"Escape cancels river preview and leaves complete document unchanged");check(studio.history.back.size()==rq.history&&studio.applied==rq.applies&&studio.surface_point_commits==rq.commits,"Escape adds no history, Apply or point commit");qa_refine_passed=!rq.failed;rq.phase=200;std::fprintf(stderr,"[Landscape Refine QA] PASS %u checks; real layer checkboxes/grass stroke, buffered pond radius/depth Apply, armed pond move/restored ground, numbered river handle move, Undo and Escape\n",rq.checks);std::fflush(stderr);}
            }
            qa_refine_phase=rq.phase;qa_refine_checks=rq.checks;
            if(rq.phase!=200&&!rq.failed&&SDL_GetTicks64()-rq.started>540000){check(false,"extended authoring regression completes within nine minutes");qa_refine_phase=999;}
        }
