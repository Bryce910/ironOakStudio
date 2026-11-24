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

bool graphics_panel(SDL_Window *window,bool &open,WorldGraphicsSettings &draft,const WorldGraphicsSettings &active,
                    const QualityController &quality,float fps,int display_w,int display_h,int render_w,int render_h,
                    const char *message){
    if(!open)return false;
    bool apply=false;
    ImGui::SetNextWindowSize({500,700},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({480,300},{650,std::max(300.f,ImGui::GetIO().DisplaySize.y-24.f)});
    if(ImGui::Begin("Graphics and display [F10]",&open)){
        const bool appearing=ImGui::IsWindowAppearing();
        if(appearing){ImGui::SetWindowFocus();ImGui::SetNavCursorVisible(true);}
        ImGui::PushItemWidth(240.f);
        ImGui::TextUnformatted("Choose visual quality for your computer.");
        ImGui::Text("Frame rate %.1f FPS | Target %d FPS",fps,active.target_fps);
        int preset=int(draft.preset);
        if(ImGui::Combo("Preset",&preset,"Performance\0Balanced\0Cinematic\0Custom\0"))
            world_graphics_settings_apply_preset(&draft,WorldGraphicsPreset(preset));
        if(appearing)ImGui::SetItemDefaultFocus();
        ImGui::TextWrapped("Performance: lighter rendering. Balanced: everyday play. Cinematic: 4K shadows and full-resolution reflections for faster graphics cards.");
        ImGui::SeparatorText("Display");
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
        }else ImGui::TextWrapped("Uses this monitor's native desktop resolution, including 1440p and 4K displays.");
        ImGui::Checkbox("VSync",&draft.vsync);
        ImGui::TextWrapped("For the sharpest picture on a faster computer, choose Cinematic and turn Automatic image quality off. Image scale 100%% renders every display pixel.");
        ImGui::Separator();
        ImGui::Checkbox("Automatic image quality",&draft.auto_quality);
        const int targets[]={45,60,90,120};int target=1;for(int i=0;i<4;i++)if(targets[i]==draft.target_fps)target=i;
        if(ImGui::Combo("Target frame rate",&target,"45 FPS\0 60 FPS\0 90 FPS\0 120 FPS\0"))draft.target_fps=targets[target];
        ImGui::TextWrapped("Automatic mode adjusts image resolution toward your target, with slow recovery to prevent visible quality pumping.");
        bool custom=false;
        float maximum_percent=draft.render_scale*100.f;
        if(ImGui::SliderFloat("Maximum image scale",&maximum_percent,70.f,100.f,"%.0f%%",ImGuiSliderFlags_NoInput)){draft.render_scale=maximum_percent*.01f;custom=true;}
        draft.min_render_scale=std::min(draft.min_render_scale,draft.render_scale);
        if(draft.auto_quality){float minimum_percent=draft.min_render_scale*100.f;
            if(ImGui::SliderFloat("Minimum image scale",&minimum_percent,70.f,maximum_percent,"%.0f%%",ImGuiSliderFlags_NoInput)){draft.min_render_scale=minimum_percent*.01f;custom=true;}}
        const int sides[]={1024,2048,3072,4096};int side=0;for(int i=0;i<4;i++)if(sides[i]==draft.shadow_side)side=i;
        if(ImGui::Combo("Shadow detail",&side,"Low (1024)\0Medium (2048)\0High (3072)\0Ultra (4096)\0")){draft.shadow_side=sides[side];custom=true;}
        custom|=ImGui::Checkbox("Water reflections",&draft.reflections);
        if(draft.reflections){int r=draft.reflection_scale==1?2:draft.reflection_scale==.5f?1:0;
            if(ImGui::Combo("Reflection detail",&r,"Quarter\0Half\0Full\0")){draft.reflection_scale=r==2?1.f:r==1?.5f:.25f;custom=true;}}
        custom|=ImGui::Checkbox("Contact shadows and ambient shading",&draft.contact_shadows);
        custom|=ImGui::Checkbox("Volumetric mist",&draft.volumetric_fog);
        custom|=ImGui::Checkbox("Volumetric clouds",&draft.clouds);
        if(custom)draft.preset=WorldGraphicsPreset::Custom;
        ImGui::Separator();
        if(ImGui::Button("Apply and save"))apply=true;
        ImGui::SameLine();if(ImGui::Button("Undo edits"))draft=active;
        ImGui::SameLine();if(ImGui::Button("Recommended"))draft=world_graphics_settings_default();
        if(message&&*message)ImGui::TextWrapped("%s",message);
        const auto s=quality.status();
        ImGui::Separator();ImGui::Text("Display %d x %d | Scene %d x %d",display_w,display_h,render_w,render_h);
        ImGui::Text("Current image scale %.0f%%",100.f*float(render_w)/std::max(1,display_w));
        if(s.ready)ImGui::Text("GPU %.2f ms | Target budget %.2f ms",s.filtered_gpu_ms,s.frame_budget_ms);
        if(s.floor_limited)ImGui::TextWrapped("At the selected quality floor. Performance preset can provide more headroom.");
        if(s.cpu_limited)ImGui::TextWrapped("World updates are limiting performance; lowering image resolution will not resolve that load.");
        ImGui::TextWrapped("45 FPS minimum is the performance goal. Close this window to resume automatic adjustments during play.");
        ImGui::PopItemWidth();
    }
    ImGui::End();return apply;
}
