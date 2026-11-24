// Viewer-owned GPU timings: ordered, fresh, epoch-tagged, never a blocking read.
struct AdaptiveGPUQueries {
    struct Slot {GLuint start=0,end=0;bool pending=false;uint64_t serial=0,epoch=0;double dt=0,cpu=0;bool eligible=false;};
    std::array<Slot,8> slots={};int active=-1;uint64_t begun_frames=0;
    void init(){for(auto &s:slots){glGenQueries(1,&s.start);glGenQueries(1,&s.end);}}
    bool poll(QualityController &controller){
        bool changed=false;
        for(unsigned n=0;n<slots.size();n++){
            Slot *oldest=nullptr;
            for(auto &s:slots)if(s.pending&&(!oldest||s.serial<oldest->serial))oldest=&s;
            if(!oldest)break;
            GLint ready=0;glGetQueryObjectiv(oldest->end,GL_QUERY_RESULT_AVAILABLE,&ready);if(!ready)break;
            GLuint64 a=0,b=0;glGetQueryObjectui64v(oldest->start,GL_QUERY_RESULT,&a);glGetQueryObjectui64v(oldest->end,GL_QUERY_RESULT,&b);
            changed=controller.submit({oldest->serial,oldest->epoch,double(b-a)*.000001,oldest->cpu,oldest->eligible,oldest->dt})||changed;
            oldest->pending=false;
            if(changed)break; // Other old-size results cannot vote on a new size.
        }
        return changed;
    }
    void begin(uint64_t frame,uint64_t epoch,double dt,bool eligible){
        active=-1;
        for(unsigned i=0;i<slots.size();i++)if(!slots[i].pending){
            auto &s=slots[i];s.serial=frame;s.epoch=epoch;s.dt=dt;s.eligible=eligible;
            glQueryCounter(s.start,GL_TIMESTAMP);begun_frames++;active=int(i);break;
        }
    }
    void cadence(uint64_t serial,double elapsed){for(auto &s:slots)if(s.pending&&s.serial==serial){s.dt=elapsed;break;}}
    void end(double cpu){if(active>=0){auto &s=slots[unsigned(active)];s.cpu=cpu;glQueryCounter(s.end,GL_TIMESTAMP);s.pending=true;active=-1;}}
    void release(){for(auto &s:slots){glDeleteQueries(1,&s.start);glDeleteQueries(1,&s.end);}slots={};active=-1;}
};

// Borderless uses the desktop mode; it never switches the monitor to an
// unsupported exclusive mode. Saved window sizes are bounded on a new display.
bool graphics_display(SDL_Window *window,const WorldGraphicsSettings &settings){
    const int display=std::max(0,SDL_GetWindowDisplayIndex(window));SDL_Rect usable={};
    if(SDL_GetDisplayUsableBounds(display,&usable))usable={0,0,1920,1080};
    const Uint32 fullscreen=settings.display_mode==WorldDisplayMode::Borderless?SDL_WINDOW_FULLSCREEN_DESKTOP:0;
    if(SDL_SetWindowFullscreen(window,fullscreen))return false;
    if(!fullscreen&&settings.window_width>0){
        SDL_SetWindowSize(window,std::min(settings.window_width,usable.w),std::min(settings.window_height,usable.h));
        SDL_SetWindowPosition(window,SDL_WINDOWPOS_CENTERED_DISPLAY(display),SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    }
    if(SDL_GL_GetSwapInterval()!=int(settings.vsync)&&SDL_GL_SetSwapInterval(settings.vsync?1:0))return false;
    return true;
}

// Unknown refresh leaves the user's adaptive target unchanged.
int graphics_refresh_hz(SDL_Window *window){
    SDL_DisplayMode mode={};const int display=SDL_GetWindowDisplayIndex(window);
    return display>=0&&SDL_GetCurrentDisplayMode(display,&mode)==0?std::max(0,mode.refresh_rate):0;
}

bool graphics_panel(SDL_Window *window,bool &open,WorldGraphicsSettings &draft,const WorldGraphicsSettings &active,
                    const QualityController &quality,float fps,int display_w,int display_h,int render_w,int render_h,
                    const char *message,WorldCharacterTextures resident_character_textures,WorldEnvironmentTextures resident_environment_textures,std::uint64_t resident_texture_pool_bytes){
    if(!open)return false;
    bool apply=false,custom=false;
    ImGui::SetNextWindowSize({540,690},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({480,340},{680,std::max(340.f,ImGui::GetIO().DisplaySize.y-24.f)});
    if(ImGui::Begin("Graphics and display [F10]",&open)){
        const bool appearing=ImGui::IsWindowAppearing();
        if(appearing){ImGui::SetWindowFocus();ImGui::SetNavCursorVisible(true);}
        const int refresh=graphics_refresh_hz(window);
        ImGui::Text("%.1f FPS | Scene %d x %d | Scale %.0f%%",fps,render_w,render_h,100.f*float(render_w)/std::max(1,display_w));
        // Scroll only the options: actions stay reachable on a small display.
        const float footer=ImGui::GetFrameHeightWithSpacing()+ImGui::GetTextLineHeightWithSpacing()*3.5f;
        if(ImGui::BeginChild("Graphics options",{0,-footer},false)){
            ImGui::PushItemWidth(230.f);
            if(ImGui::BeginTabBar("Graphics tabs")){
                if(ImGui::BeginTabItem("Quality")){
                    int preset=int(draft.preset);
                    if(ImGui::Combo("Preset",&preset,"Performance\0Balanced\0Cinematic\0Custom\0"))
                        world_graphics_settings_apply_preset(&draft,WorldGraphicsPreset(preset));
                    if(appearing)ImGui::SetItemDefaultFocus();
                    ImGui::TextWrapped("Performance reduces grass, shadows and effects. Balanced keeps richer scenery. Cinematic favors visual detail on faster graphics cards.");
                    ImGui::SeparatorText("Characters / models");
                    int textures=int(draft.character_textures);
                    if(ImGui::Combo("Character detail",&textures,"Low\0Medium\0High\0Ultra\0")){draft.character_textures=WorldCharacterTextures(textures);custom=true;}
                    ImGui::TextWrapped("Low and Medium reduce model and texture detail. High uses the full character mesh; Ultra adds finer source textures. Higher levels use more graphics memory. Apply and restart to load your selection.");
                    ImGui::Text("Loaded character detail: %s",world_character_textures_name(resident_character_textures));
                    if(draft.character_textures!=resident_character_textures)ImGui::TextWrapped("Restart the game after applying to load this character/model quality.");
                    ImGui::SeparatorText("Environment textures");
                    int environment_textures=int(draft.environment_textures);
                    if(ImGui::Combo("Scenery texture detail",&environment_textures,"Low\0Medium\0High\0Ultra\0")){draft.environment_textures=WorldEnvironmentTextures(environment_textures);custom=true;}
                    ImGui::TextWrapped("Low limits scenery textures to 512 px, Medium 1K, High 2K and Ultra 4K. High retains the camp's 2K source detail. Ultra favors close detail on graphics cards with more memory. Characters have their own setting.");
                    ImGui::Text("Loaded environment textures: %s",world_environment_textures_name(resident_environment_textures));
                    ImGui::Text("Auto shared texture pool: %llu MiB",static_cast<unsigned long long>(resident_texture_pool_bytes/(1024*1024)));
                    ImGui::TextWrapped("Auto uses supported startup memory hints to allow a larger pool on graphics cards with ample free memory. This admission limit is not a promise of available memory.");
                    if(draft.environment_textures!=resident_environment_textures)ImGui::TextWrapped("Apply and restart the game to load this environment texture quality and memory budget.");
                    ImGui::SeparatorText("Vegetation");
                    int amount=int(draft.foliage_density);
                    if(ImGui::Combo("Grass and undergrowth detail",&amount,"Low\0Medium\0High\0Ultra\0")){draft.foliage_density=WorldFoliageDensity(amount);custom=true;}
                    ImGui::TextWrapped("Higher levels grow and draw denser grass, ferns and small plants. Ultra favors the fullest undergrowth; lower levels improve performance. Trees and solid rocks stay in place. Apply to update the scene.");
                    int distance=int(draft.foliage_distance);
                    if(ImGui::Combo("Foliage detail distance",&distance,"Low\0Medium\0High\0Ultra\0")){draft.foliage_distance=WorldFoliageDistance(distance);custom=true;}
                    ImGui::TextWrapped("Higher levels keep detailed vegetation visible farther away. Nearby grass stays detailed at every level.");
                    ImGui::SeparatorText("Environmental effects");
                    int effects=int(draft.environment_effects);
                    if(ImGui::Combo("Smoke, atmosphere and life",&effects,"Low\0Medium\0High\0Ultra\0")){draft.environment_effects=WorldEnvironmentEffects(effects);custom=true;}
                    ImGui::TextWrapped("Controls wind-driven fire smoke, drifting leaves, insects, dusk fireflies and light-shaft detail. Higher settings add detail and range. Water and vegetation have separate controls.");
                    ImGui::SeparatorText("Water");
                    int water_quality=int(draft.water_quality);
                    if(ImGui::Combo("Water quality",&water_quality,"Low\0Medium\0High\0Ultra\0")){draft.water_quality=WorldWaterQuality(water_quality);draft.reflection_scale=1.f;custom=true;}
                    ImGui::TextWrapped("Low uses simpler water. Medium balances detail and speed. High adds richer reflections; Ultra favors the most detailed water on faster graphics cards.");
                    custom|=ImGui::Checkbox("Water reflections",&draft.reflections);
                    const auto water_profile=world_water_quality_profile(draft.water_quality);
                    ImGui::BeginDisabled(!draft.reflections||!water_profile.planar);
                    int reflection_detail=draft.reflection_scale==1?2:draft.reflection_scale==.5f?1:0;
                    if(ImGui::Combo("Advanced reflection resolution cap",&reflection_detail,"Quarter\0Half\0Full\0")){draft.reflection_scale=reflection_detail==2?1.f:reflection_detail==1?.5f:.25f;custom=true;}
                    ImGui::EndDisabled();
                    ImGui::TextWrapped("Choosing a water quality level restores its full reflection budget. This advanced cap can lower reflection resolution further to improve speed. Apply changes below; a restart is not needed.");
                    ImGui::SeparatorText("Image quality");
                    ImGui::Checkbox("Automatic image quality",&draft.auto_quality);
                    const int targets[]={30,45,60,90,120,144};int target=0;for(int i=0;i<6;i++)if(targets[i]==draft.target_fps)target=i;
                    ImGui::BeginDisabled(!draft.auto_quality);
                    if(ImGui::Combo("Adaptive target",&target,"30 FPS\0 45 FPS\0 60 FPS\0 90 FPS\0 120 FPS\0 144 FPS\0"))draft.target_fps=targets[target];
                    ImGui::EndDisabled();
                    ImGui::TextWrapped("Automatic mode adjusts image resolution toward the target. A lower frame limit or VSync refresh also limits the target.");
                    float maximum=draft.render_scale*100.f;
                    if(ImGui::SliderFloat("Maximum image scale",&maximum,70.f,100.f,"%.0f%%",ImGuiSliderFlags_NoInput)){draft.render_scale=maximum*.01f;custom=true;}
                    draft.min_render_scale=std::min(draft.min_render_scale,draft.render_scale);
                    if(draft.auto_quality){float minimum=draft.min_render_scale*100.f;
                        if(ImGui::SliderFloat("Minimum image scale",&minimum,70.f,maximum,"%.0f%%",ImGuiSliderFlags_NoInput)){draft.min_render_scale=minimum*.01f;custom=true;}}
                    ImGui::TextWrapped("100%% gives the sharpest image. Automatic quality resumes when this window closes.");
                    if(ImGui::CollapsingHeader("Lighting and effects")){
                        const int sides[]={1024,2048,3072,4096};int side=0;for(int i=0;i<4;i++)if(sides[i]==draft.shadow_side)side=i;
                        if(ImGui::Combo("Shadow detail",&side,"Low\0Medium\0High\0Ultra\0")){draft.shadow_side=sides[side];custom=true;}
                        custom|=ImGui::Checkbox("Contact, local shadows and ambient shading",&draft.contact_shadows);
                        custom|=ImGui::Checkbox("Volumetric mist",&draft.volumetric_fog);
                        custom|=ImGui::Checkbox("Volumetric clouds",&draft.clouds);
                    }
                    const auto status=quality.status();
                    ImGui::Separator();
                    if(status.ready)ImGui::Text("GPU %.2f ms | Adaptive budget %.2f ms",status.filtered_gpu_ms,status.frame_budget_ms);
                    if(status.floor_limited)ImGui::TextWrapped("At the selected image-quality floor. Try less grass, a shorter detail distance, or Performance.");
                    if(status.cpu_limited)ImGui::TextWrapped("World updates are limiting performance; reducing image resolution will not resolve that load.");
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Display and frame rate")){
                    int mode=int(draft.display_mode);
                    if(ImGui::Combo("Display mode",&mode,"Windowed\0Borderless fullscreen\0"))draft.display_mode=WorldDisplayMode(mode);
                    if(draft.display_mode==WorldDisplayMode::Windowed){
                        const int widths[]={0,1280,1600,1920,2560,3840},heights[]={0,720,900,1080,1440,2160};
                        const char *labels[]={"Keep current size","1280 x 720","1600 x 900","1920 x 1080","2560 x 1440","3840 x 2160"};
                        int selected=0;for(int i=1;i<6;i++)if(draft.window_width==widths[i]&&draft.window_height==heights[i])selected=i;
                        SDL_Rect usable={};const bool known=SDL_GetDisplayUsableBounds(std::max(0,SDL_GetWindowDisplayIndex(window)),&usable)==0;
                        if(ImGui::BeginCombo("Window resolution",labels[selected])){
                            for(int i=0;i<6;i++){
                                const bool supported=!known||i==0||(widths[i]<=usable.w&&heights[i]<=usable.h);
                                ImGui::BeginDisabled(!supported);
                                if(ImGui::Selectable(labels[i],selected==i)){draft.window_width=widths[i];draft.window_height=heights[i];}
                                if(selected==i)ImGui::SetItemDefaultFocus();
                                ImGui::EndDisabled();
                            }
                            ImGui::EndCombo();
                        }
                    }else ImGui::TextWrapped("Uses this monitor's native desktop resolution.");
                    ImGui::SeparatorText("Frame pacing");
                    const int limits[]={0,30,45,60,90,120,144};int limit=0;for(int i=0;i<7;i++)if(limits[i]==draft.frame_rate_limit)limit=i;
                    if(ImGui::Combo("Frame-rate limit",&limit,"Off\0 30 FPS\0 45 FPS\0 60 FPS\0 90 FPS\0 120 FPS\0 144 FPS\0"))draft.frame_rate_limit=limits[limit];
                    ImGui::Checkbox("VSync",&draft.vsync);
                    if(refresh>0)ImGui::Text("Monitor refresh: %d Hz",refresh);
                    ImGui::TextWrapped("A frame limit reduces excess GPU work. VSync prevents tearing. For even pacing with VSync, choose a limit that matches your monitor or divides its refresh evenly.");
                    ImGui::Text("Effective adaptive target: %d FPS",world_graphics_effective_target_fps(draft,refresh));
                    ImGui::Text("Current display: %d x %d",display_w,display_h);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::PopItemWidth();
        }
        ImGui::EndChild();
        if(custom)draft.preset=WorldGraphicsPreset::Custom;
        ImGui::Separator();
        if(ImGui::Button("Apply and save"))apply=true;
        ImGui::SameLine();if(ImGui::Button("Undo edits"))draft=active;
        ImGui::SameLine();if(ImGui::Button("Recommended")){
            world_graphics_settings_apply_preset(&draft,WorldGraphicsPreset::Balanced);
        }
        if(message&&*message)ImGui::TextWrapped("%s",message);
        else ImGui::TextWrapped("Changes take effect when you apply them. Your frame-rate and display choices are kept when switching presets.");
    }
    ImGui::End();return apply;
}
