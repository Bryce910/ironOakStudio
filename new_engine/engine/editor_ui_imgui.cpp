#include "engine/editor_ui_imgui.h"
#include "engine/editor_ui_imgui_submission.h"
#include "engine/project_runtime.h"

#include "third_party/imgui/imgui.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h"
#include "third_party/imgui/backends/imgui_impl_opengl3.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <chrono>

static bool g_imgui_ready=false;
static bool g_passive_frame=false;

// The reference backend remains available for same-binary native A/B tests.
// This latch changes no input/frame lifecycle or nonempty UI submission.
static bool g_empty_ui_fast_path=false;
static constexpr std::size_t ui_trace_capacity=8192;
struct UiTraceStamp {std::uint64_t wall=0;std::uint64_t cpu_ns=0;bool cpu_valid=false;};
struct UiTraceRow {
    std::uint64_t serial=0;int imgui_frame=0,lists=0,vertices=0,indices=0;
    unsigned pending_textures=0;bool passive=false,skipped=false,cpu_available=false;
    double render_wall_ms=0,render_cpu_ms=-1,backend_wall_ms=0,backend_cpu_ms=-1;
};
static std::array<UiTraceRow,ui_trace_capacity> g_ui_trace_rows;
static std::size_t g_ui_trace_size=0;
static std::uint64_t g_ui_trace_serial=0,g_ui_trace_dropped=0;
static char g_ui_trace_path[1024]={};
static UiTraceStamp ui_trace_stamp(){
    UiTraceStamp result;
    result.wall=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
#ifdef CLOCK_THREAD_CPUTIME_ID
    timespec now={};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&now)==0&&now.tv_sec>=0){
        result.cpu_ns=std::uint64_t(now.tv_sec)*1000000000ull+std::uint64_t(now.tv_nsec);
        result.cpu_valid=true;
    }
#endif
    return result;
}
static double ui_trace_cpu_ms(const UiTraceStamp &begin,const UiTraceStamp &end){
    return begin.cpu_valid&&end.cpu_valid&&end.cpu_ns>=begin.cpu_ns?
        double(end.cpu_ns-begin.cpu_ns)*.000001:-1.;
}
static void ui_trace_initialize(){
    const char *disabled=std::getenv("VALWOOD_QA_DISABLE_EMPTY_UI_FAST_PATH");
    // Only a valid, genuinely empty passive frame can skip submission. The
    // explicit disable flag retains the full-backend reference for native A/B.
    // The former positive QA flag is redundant; disable=1 always wins.
    g_empty_ui_fast_path=!(disabled&&std::strcmp(disabled,"1")==0);
    g_ui_trace_size=0;g_ui_trace_serial=g_ui_trace_dropped=0;g_ui_trace_path[0]=0;
    const char *path=std::getenv("VALWOOD_UI_TRACE_PATH");
    if(path&&*path){
        if(std::strlen(path)>=sizeof(g_ui_trace_path)){
            std::fprintf(stderr,"[UI trace] Output path too long; tracing disabled\n");
        }else{
            std::snprintf(g_ui_trace_path,sizeof(g_ui_trace_path),"%s",path);
        }
    }
}
static void ui_trace_flush(){
    if(!g_ui_trace_path[0])return;
    FILE *file=std::fopen(g_ui_trace_path,"w");
    if(!file){std::fprintf(stderr,"[UI trace] Cannot write %s\n",g_ui_trace_path);g_ui_trace_path[0]=0;return;}
    std::fprintf(file,"{\n  \"schema_version\": 1, \"capacity\": %zu, \"recorded_frames\": %zu, \"total_frames\": %llu, \"dropped_frames\": %llu,\n  \"empty_fast_path_enabled\": %s,\n",
        ui_trace_capacity,g_ui_trace_size,static_cast<unsigned long long>(g_ui_trace_serial),
        static_cast<unsigned long long>(g_ui_trace_dropped),g_empty_ui_fast_path?"true":"false");
    std::fputs("  \"thread_cpu_note\": \"Calling thread only; unavailable samples use -1. Wall minus thread CPU includes waiting and scheduling, not exclusively GPU time.\",\n  \"columns\": [\"ui_frame\",\"imgui_frame\",\"passive\",\"backend_skipped\",\"draw_lists\",\"vertices\",\"indices\",\"pending_textures\",\"render_wall_ms\",\"render_thread_cpu_ms\",\"backend_wall_ms\",\"backend_thread_cpu_ms\",\"thread_clock_available\"],\n  \"frames\": [",file);
    for(std::size_t i=0;i<g_ui_trace_size;i++){
        const auto &r=g_ui_trace_rows[i];
        std::fprintf(file,"%s\n    [%llu,%d,%d,%d,%d,%d,%d,%u,%.6f,%.6f,%.6f,%.6f,%d]",
            i?",":"",static_cast<unsigned long long>(r.serial),r.imgui_frame,int(r.passive),int(r.skipped),
            r.lists,r.vertices,r.indices,r.pending_textures,r.render_wall_ms,r.render_cpu_ms,
            r.backend_wall_ms,r.backend_cpu_ms,int(r.cpu_available));
    }
    std::fputs("\n  ]\n}\n",file);
    const bool okay=std::ferror(file)==0;
    const int closed=std::fclose(file);
    std::fprintf(stderr,"[UI trace] %s %s: %zu frames, %llu dropped (shutdown only)\n",
        okay&&closed==0?"Wrote":"Failed",g_ui_trace_path,g_ui_trace_size,
        static_cast<unsigned long long>(g_ui_trace_dropped));
    g_ui_trace_path[0]=0;
}
static bool g_show_world_forge=true;
static bool g_show_workflow=true;
static bool g_show_inspector=true;
static bool g_show_outliner=true;
static bool g_show_prop_window=true;
static bool g_show_npc_window=true;
static bool g_show_help=true;
static bool g_show_frame_debugger=true;
static bool g_prev_frame_debugger_enabled=false;
static int g_raw_mouse_x=0;
static int g_raw_mouse_y=0;
static bool g_raw_mouse_down[5]={false,false,false,false,false};
static bool g_raw_mouse_clicked[5]={false,false,false,false,false};
static ImGuiID g_shell_button_raw_id=0;
bool editor_ui_imgui_init(void *window,void *gl_context){
    if(g_imgui_ready) return true;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io=ImGui::GetIO();
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename="studio-layout.ini";
    io.FontGlobalScale=1.0f;
    FILE *font=std::fopen("fonts/DejaVuSans.ttf","rb");
    if(font){std::fclose(font);io.Fonts->AddFontFromFileTTF("fonts/DejaVuSans.ttf",15.0f);}
    ImGui::StyleColorsDark();
    ImGuiStyle &style=ImGui::GetStyle();
    style.WindowRounding=5.0f; style.FrameRounding=4.0f;
    style.WindowPadding=ImVec2(12,10); style.FramePadding=ImVec2(8,5);
    style.ItemSpacing=ImVec2(8,7); style.ScrollbarSize=12;
    style.Colors[ImGuiCol_WindowBg]=ImVec4(.055f,.065f,.080f,.98f);
    style.Colors[ImGuiCol_TitleBg]=ImVec4(.07f,.085f,.105f,1);
    style.Colors[ImGuiCol_TitleBgActive]=ImVec4(.10f,.13f,.16f,1);
    style.Colors[ImGuiCol_Header]=ImVec4(.15f,.23f,.27f,1);
    style.Colors[ImGuiCol_HeaderHovered]=ImVec4(.20f,.32f,.36f,1);
    style.Colors[ImGuiCol_Button]=ImVec4(.15f,.23f,.27f,1);
    style.Colors[ImGuiCol_ButtonHovered]=ImVec4(.24f,.38f,.40f,1);
    style.Colors[ImGuiCol_ButtonActive]=ImVec4(.30f,.48f,.48f,1);
    style.Colors[ImGuiCol_CheckMark]=ImVec4(.83f,.68f,.39f,1);
    style.Colors[ImGuiCol_SliderGrab]=ImVec4(.76f,.61f,.34f,1);
    if(!ImGui_ImplSDL2_InitForOpenGL((SDL_Window*)window,gl_context)) return false;
    if(!ImGui_ImplOpenGL3_Init("#version 330")) return false;
    g_imgui_ready=true;
    g_passive_frame=false;
    ui_trace_initialize();
    return true;
}

void editor_ui_imgui_shutdown(void){
    if(!g_imgui_ready) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    ui_trace_flush(); // No trace IO occurs in a frame or timed render stage.
    g_imgui_ready=false;
    g_passive_frame=false;
    g_empty_ui_fast_path=false;
}

bool editor_ui_imgui_process_event(const SDL_Event *event){
    if(!g_imgui_ready||!event) return false;
    return ImGui_ImplSDL2_ProcessEvent(event);
}

void editor_ui_imgui_begin_frame(void){
    if(!g_imgui_ready) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    // SDL's backend clock did not advance while hidden. Resume with one normal
    // step rather than injecting the entire hidden interval into UI timers.
    if(g_passive_frame)ImGui::GetIO().DeltaTime=1.f/60.f;
    g_passive_frame=false;
    {
        ImGuiIO &io=ImGui::GetIO();
        Uint32 mouse_buttons=SDL_GetMouseState(&g_raw_mouse_x,&g_raw_mouse_y);
        // On X11 the SDL local snapshot can remain stale after a relative-mode
        // grab while the backend/desktop pointer has moved. Keep the raw-click
        // fallback in the same focused-window coordinates as ImGui's backend.
        // Ordinary gameplay's passive UI path never makes this round trip.
        SDL_Window *focused=SDL_GetKeyboardFocus();
        const char *driver=SDL_GetCurrentVideoDriver();
        if(focused&&driver&&!std::strcmp(driver,"x11")&&!SDL_GetRelativeMouseMode()){
            int gx=0,gy=0,wx=0,wy=0;
            mouse_buttons=SDL_GetGlobalMouseState(&gx,&gy);
            SDL_GetWindowPosition(focused,&wx,&wy);
            g_raw_mouse_x=gx-wx;g_raw_mouse_y=gy-wy;
        }

        io.AddMousePosEvent((float)g_raw_mouse_x,(float)g_raw_mouse_y);
        const Uint32 masks[5]={
            SDL_BUTTON(SDL_BUTTON_LEFT),
            SDL_BUTTON(SDL_BUTTON_RIGHT),
            SDL_BUTTON(SDL_BUTTON_MIDDLE),
            SDL_BUTTON(SDL_BUTTON_X1),
            SDL_BUTTON(SDL_BUTTON_X2)
        };
        for(int i=0;i<5;i++){
            bool down=(mouse_buttons&masks[i])!=0;
            g_raw_mouse_clicked[i]=down&&!g_raw_mouse_down[i];
            if(down!=g_raw_mouse_down[i]){
                io.AddMouseButtonEvent(i,down);
                g_raw_mouse_down[i]=down;
            }
        }
    }
    ImGui::NewFrame();
}

void editor_ui_imgui_begin_passive_frame(float seconds,int width,int height){
    if(!g_imgui_ready)return;
    if(!g_passive_frame)SDL_CaptureMouse(SDL_FALSE);
    ImGui_ImplOpenGL3_NewFrame();
    ImGuiIO &io=ImGui::GetIO();
    io.DisplaySize=ImVec2(float(width>0?width:1),float(height>0?height:1));
    io.DisplayFramebufferScale=ImVec2(1,1);
    io.DeltaTime=std::isfinite(seconds)&&seconds>0?fminf(seconds,.05f):1.f/60.f;
    // NewFrame still drains the backend's queued events and updates capture
    // flags. Keep the normal render/end path and GL presentation behavior.
    // No hidden widgets require SDL global-mouse/cursor/gamepad round trips.
    g_passive_frame=true;
    ImGui::NewFrame();
}

void editor_ui_imgui_render(void){
    if(!g_imgui_ready) return;
    const bool tracing=g_ui_trace_path[0]!=0;
    UiTraceStamp begin={},rendered={},submitted={};
    if(tracing)begin=ui_trace_stamp();
    ImGui::Render();
    if(tracing)rendered=ui_trace_stamp();
    ImDrawData *draw=ImGui::GetDrawData();
    const unsigned pending=tracing?world_imgui_pending_texture_work(draw):0;
    const bool skipped=world_imgui_can_skip_empty_backend(g_empty_ui_fast_path,g_passive_frame,draw);
    // Keep the complete backend for visible UI, callbacks, geometry, invalid
    // draw data, and every pending texture create/update/destroy request.
    // A genuine empty passive frame has no draw or texture work, so the
    // backend's save/setup/restore sequence has no observable pixel/state work.
    // The explicit QA flag permits native pacing comparison without recompiling.
    if(!skipped)ImGui_ImplOpenGL3_RenderDrawData(draw);
    if(tracing){
        submitted=ui_trace_stamp();g_ui_trace_serial++;
        if(g_ui_trace_size<ui_trace_capacity){
            auto &r=g_ui_trace_rows[g_ui_trace_size++];
            r.serial=g_ui_trace_serial;r.imgui_frame=ImGui::GetFrameCount();
            r.passive=g_passive_frame;r.skipped=skipped;r.pending_textures=pending;
            r.lists=draw?draw->CmdListsCount:0;r.vertices=draw?draw->TotalVtxCount:0;r.indices=draw?draw->TotalIdxCount:0;
            r.render_wall_ms=double(rendered.wall-begin.wall)*.000001;
            r.backend_wall_ms=double(submitted.wall-rendered.wall)*.000001;
            r.render_cpu_ms=ui_trace_cpu_ms(begin,rendered);r.backend_cpu_ms=ui_trace_cpu_ms(rendered,submitted);
            r.cpu_available=r.render_cpu_ms>=0&&r.backend_cpu_ms>=0;
        }else g_ui_trace_dropped++;
    }
    /* Sync SDL text input state with ImGui's needs.
       The SDL2 backend no longer calls these itself, so we must. */
    static bool s_was_text_input=false;
    bool want=ImGui::GetIO().WantTextInput;
    if(want&&!s_was_text_input) SDL_StartTextInput();
    else if(!want&&s_was_text_input) SDL_StopTextInput();
    s_was_text_input=want;
}

bool editor_ui_imgui_wants_mouse(void){
    if(!g_imgui_ready) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool editor_ui_imgui_wants_keyboard(void){
    if(!g_imgui_ready) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}

static float frame_history_value_getter(void *data,int idx){
    const EditorFrameDebuggerPanelData *dbg=(const EditorFrameDebuggerPanelData*)data;
    int cap=(int)(sizeof(dbg->frame_history)/sizeof(dbg->frame_history[0]));
    int count=dbg->frame_history_count;
    if(!dbg||idx<0||idx>=count||cap<=0) return 0.0f;
    {
        int start=(dbg->frame_history_head-count+cap)%cap;
        int slot=(start+idx)%cap;
        return dbg->frame_history[slot];
    }
}

void editor_ui_imgui_draw_frame_debugger(EditorFrameDebuggerPanelData *data){
    if(!g_imgui_ready||!data){
        g_prev_frame_debugger_enabled=false;
        return;
    }
    if(data->enabled&&!g_prev_frame_debugger_enabled) g_show_frame_debugger=true;
    g_prev_frame_debugger_enabled=data->enabled;
    if(!data->enabled) return;
    ImGuiWindowFlags flags=ImGuiWindowFlags_NoCollapse;
    ImGui::SetNextWindowSize(ImVec2(430.0f,560.0f),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(16.0f,48.0f),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Frame Debugger",&g_show_frame_debugger,flags)){
        ImGui::End();
        return;
    }
    if(!g_show_frame_debugger){
        data->enabled=false;
        g_prev_frame_debugger_enabled=false;
        ImGui::End();
        return;
    }

    ImGui::Text("FPS %.1f",data->fps);
    ImGui::SameLine();
    ImGui::TextDisabled("(%.2f ms)",data->frame_ms);
    ImGui::Checkbox("Cartoon Shader",&data->toon_shading);
    ImGui::SameLine();
    ImGui::TextDisabled("%s",data->gpu_timers_supported?"GPU timers: on":"GPU timers: off");

    if(data->frame_history_count>1){
        float max_ms=16.7f;
        for(int i=0;i<data->frame_history_count;i++){
            float v=frame_history_value_getter((void*)data,i);
            if(v>max_ms) max_ms=v;
        }
        max_ms=std::max(max_ms*1.10f,16.7f);
        ImGui::PlotLines("Frame ms",frame_history_value_getter,(void*)data,
                         data->frame_history_count,0,nullptr,0.0f,max_ms,ImVec2(0.0f,120.0f));
    }else{
        ImGui::TextDisabled("Collecting frame history...");
    }

    ImGui::SeparatorText("Update");
    ImGui::Text("Update: %.2f ms",data->update_ms);
    ImGui::Text("Stream: %.2f ms (%d up)",data->stream_upload_ms,data->stream_uploads);
    ImGui::Text("Actors upd: %.2f ms",data->update_actor_ms);
    ImGui::Text("FX upd: %.2f ms",data->update_fx_ms);
    ImGui::Text("Particles upd: %.2f ms",data->update_particles_ms);
    ImGui::Text("World upd: %.2f ms",data->update_world_ms);

    ImGui::SeparatorText("Render");
    if(data->gpu_timers_supported){
        ImGui::Text("Shadow: %.2f cpu / %.2f gpu",data->shadow_ms,data->shadow_gpu_ms);
        ImGui::Text("Reflection: %.2f cpu / %.2f gpu",data->reflection_ms,data->reflection_gpu_ms);
        ImGui::Text("Terrain: %.2f cpu / %.2f gpu",data->terrain_ms,data->terrain_gpu_ms);
        ImGui::Text("Props: %.2f cpu / %.2f gpu",data->props_ms,data->props_gpu_ms);
        ImGui::Text("Grass: %.2f cpu / %.2f gpu",data->grass_ms,data->grass_gpu_ms);
        ImGui::Text("Forest: %.2f cpu / %.2f gpu",data->forest_ms,data->forest_gpu_ms);
        ImGui::Text("Characters: %.2f cpu / %.2f gpu",data->characters_ms,data->characters_gpu_ms);
        ImGui::Text("Birds: %.2f cpu / %.2f gpu",data->birds_ms,data->birds_gpu_ms);
        ImGui::Text("Animals: %.2f cpu / %.2f gpu",data->animals_ms,data->animals_gpu_ms);
        ImGui::Text("Actors: %.2f cpu / %.2f gpu",data->actors_ms,data->actors_gpu_ms);
        ImGui::Text("Particles: %.2f cpu / %.2f gpu",data->particles_ms,data->particles_gpu_ms);
        ImGui::Text("Weather: %.2f cpu / %.2f gpu",data->weather_ms,data->weather_gpu_ms);
        ImGui::Text("PostFX: %.2f cpu / %.2f gpu",data->postfx_ms,data->postfx_gpu_ms);
        ImGui::TextColored(ImVec4(1.0f,0.72f,0.35f,1.0f),"Render total: %.2f cpu / %.2f gpu",data->render_total_ms,data->render_total_gpu_ms);
    }else{
        ImGui::Text("Shadow: %.2f ms",data->shadow_ms);
        ImGui::Text("Reflection: %.2f ms",data->reflection_ms);
        ImGui::Text("Terrain: %.2f ms",data->terrain_ms);
        ImGui::Text("Props: %.2f ms",data->props_ms);
        ImGui::Text("Grass: %.2f ms",data->grass_ms);
        ImGui::Text("Forest: %.2f ms",data->forest_ms);
        ImGui::Text("Characters: %.2f ms",data->characters_ms);
        ImGui::Text("Birds: %.2f ms",data->birds_ms);
        ImGui::Text("Animals: %.2f ms",data->animals_ms);
        ImGui::Text("Actors: %.2f ms",data->actors_ms);
        ImGui::Text("Particles: %.2f ms",data->particles_ms);
        ImGui::Text("Weather: %.2f ms",data->weather_ms);
        ImGui::Text("PostFX: %.2f ms",data->postfx_ms);
        ImGui::TextColored(ImVec4(1.0f,0.72f,0.35f,1.0f),"Render total: %.2f ms",data->render_total_ms);
    }

    ImGui::SeparatorText("Scene");
    ImGui::Text("Terrain tris: %d",data->terrain_tris);
    ImGui::Text("Props: %d / %d",data->active_props,data->prop_count);
    ImGui::Text("Trees: %d",data->forest_count);
    ImGui::Text("Particles: %d",data->particle_count);
    ImGui::Text("Emitters: %d",data->emitter_count);
    ImGui::Text("Lights: %d",data->point_light_count);

    ImGui::End();
}

static bool raw_left_click_in_last_item(void){
    if(!g_raw_mouse_clicked[0]) return false;
    ImVec2 min=ImGui::GetItemRectMin();
    ImVec2 max=ImGui::GetItemRectMax();
    return (float)g_raw_mouse_x>=min.x&&(float)g_raw_mouse_x<=max.x&&
           (float)g_raw_mouse_y>=min.y&&(float)g_raw_mouse_y<=max.y;
}

static bool shell_button(const char *label,ImVec2 size){
    ImGuiID id=ImGui::GetID(label);
    bool pressed=ImGui::Button(label,size);
    if(raw_left_click_in_last_item()){
        g_shell_button_raw_id=id;
        return true;
    }
    if(pressed){
        if(g_shell_button_raw_id==id){
            g_shell_button_raw_id=0;
            return false;
        }
        return true;
    }
    return false;
}

static void draw_mode_button(const EditorWorldForgeShellState *state,const char *label,bool active,
                             EditorWorldForgeMode mode){
    if(active) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.36f,0.28f,0.12f,1.0f));
    bool pressed=shell_button(label,ImVec2(-1.0f,0.0f));
    if(pressed&&state->activate_mode){
        state->activate_mode(state->action_ctx,mode);
    }
    if(active) ImGui::PopStyleColor();
}

static void draw_tool_button(const EditorWorldForgeShellState *state,const char *label,EditorWorldForgeAction action){
    if(shell_button(label,ImVec2(-1.0f,0.0f))&&state->tool_action){
        state->tool_action(state->action_ctx,action);
    }
}

static void draw_tool_pair(const EditorWorldForgeShellState *state,
                           const char *left_label,EditorWorldForgeAction left_action,
                           const char *right_label,EditorWorldForgeAction right_action){
    float width=(ImGui::GetContentRegionAvail().x-6.0f)*0.5f;
    if(width<32.0f) width=32.0f;
    if(shell_button(left_label,ImVec2(width,0.0f))&&state->tool_action){
        state->tool_action(state->action_ctx,left_action);
    }
    ImGui::SameLine();
    if(shell_button(right_label,ImVec2(width,0.0f))&&state->tool_action){
        state->tool_action(state->action_ctx,right_action);
    }
}

static bool outliner_filter_match(const char *label,const char *filter){
    if(!filter||!filter[0]) return true;
    if(!label) return false;
    for(const char *s=label;*s;s++){
        const char *a=s;
        const char *b=filter;
        while(*a&&*b){
            char ca=*a,cb=*b;
            if(ca>='A'&&ca<='Z') ca=(char)(ca-'A'+'a');
            if(cb>='A'&&cb<='Z') cb=(char)(cb-'A'+'a');
            if(ca!=cb) break;
            ++a; ++b;
        }
        if(!*b) return true;
    }
    return false;
}

static const char *browser_category_label(int cat){
    switch(cat){
        case 1: return "Structure";
        case 2: return "Nature";
        case 3: return "Set Dressing";
        case 4: return "Lighting / FX";
        case 5: return "Gameplay";
        default: return "All";
    }
}

static int find_surface_material_slot(const EditorWorldForgeShellState *state,const char *needle_a,const char *needle_b){
    if(!state) return -1;
    for(int i=0;i<state->surface_material_count&&i<64;i++){
        const char *label=state->surface_material_labels[i];
        if(!label) continue;
        if((needle_a&&outliner_filter_match(label,needle_a))||
           (needle_b&&outliner_filter_match(label,needle_b)))
            return i;
    }
    return -1;
}

static void apply_surface_preset(const EditorWorldForgeShellState *state,int kind,float width,float height,float thickness,int coll_type){
    if(!state) return;
    if(state->surface_set_kind) state->surface_set_kind(state->action_ctx,kind);
    if(state->surface_set_width) state->surface_set_width(state->action_ctx,width);
    if(state->surface_set_height) state->surface_set_height(state->action_ctx,height);
    if(state->surface_set_thickness) state->surface_set_thickness(state->action_ctx,thickness);
    if(state->surface_set_collision) state->surface_set_collision(state->action_ctx,coll_type);
    if(state->surface_material_select){
        int stone_slot=find_surface_material_slot(state,"dark stone","stone");
        if(stone_slot>=0) state->surface_material_select(state->action_ctx,state->surface_material_indices[stone_slot]);
    }
}

static void draw_world_forge_tool_panel(const EditorWorldForgeShellState *state){
    if(!state) return;

    ImGui::TextDisabled("%s",state->tool_title?state->tool_title:"Tool");
    if(state->tool_line_a&&state->tool_line_a[0]) ImGui::TextWrapped("%s",state->tool_line_a);
    if(state->tool_line_b&&state->tool_line_b[0]) ImGui::TextWrapped("%s",state->tool_line_b);
    if(state->tool_line_c&&state->tool_line_c[0]) ImGui::TextWrapped("%s",state->tool_line_c);
    ImGui::Separator();

    if(state->path_edit){
        draw_tool_pair(state,"Previous Path",EDITOR_WF_ACTION_PATH_PREV,"Next Path",EDITOR_WF_ACTION_PATH_NEXT);
        draw_tool_pair(state,"Width -",EDITOR_WF_ACTION_PATH_WIDTH_DEC,"Width +",EDITOR_WF_ACTION_PATH_WIDTH_INC);
        draw_tool_pair(state,"Surface <",EDITOR_WF_ACTION_PATH_STYLE_PREV,"Surface >",EDITOR_WF_ACTION_PATH_STYLE_NEXT);
        draw_tool_button(state,"New Path",EDITOR_WF_ACTION_PATH_NEW);
        draw_tool_button(state,"Save Path",EDITOR_WF_ACTION_PATH_SAVE);
        draw_tool_pair(state,"Undo Point",EDITOR_WF_ACTION_PATH_UNDO,"Clear Path",EDITOR_WF_ACTION_PATH_CLEAR);
        return;
    }

    if(state->river_edit){
        if(state->river_set_width){
            float width=state->river_width;
            if(ImGui::SliderFloat("River Width",&width,10.0f,400.0f,"%.1f"))
                state->river_set_width(state->action_ctx,width);
        }
        if(state->river_set_bank_softness){
            float softness=state->river_bank_softness;
            if(ImGui::SliderFloat("Bank Softness",&softness,0.55f,2.50f,"%.2f"))
                state->river_set_bank_softness(state->action_ctx,softness);
        }
        if(state->river_has_selection){
            ImGui::Separator();
            ImGui::TextDisabled("Selected Point %d",state->river_selected_point+1);
            if(state->river_set_selected_point_position){
                float px=state->river_point_x;
                float pz=state->river_point_z;
                if(ImGui::DragFloat("Point X",&px,0.5f))
                    state->river_set_selected_point_position(state->action_ctx,0,px);
                if(ImGui::DragFloat("Point Z",&pz,0.5f))
                    state->river_set_selected_point_position(state->action_ctx,1,pz);
            }
            if(state->river_set_selected_point_width_scale){
                float width_scale=state->river_point_width_scale;
                if(ImGui::SliderFloat("Width Profile",&width_scale,0.35f,2.50f,"%.2f"))
                    state->river_set_selected_point_width_scale(state->action_ctx,width_scale);
            }
            if(state->river_set_selected_point_depth_scale){
                float depth_scale=state->river_point_depth_scale;
                if(ImGui::SliderFloat("Depth Profile",&depth_scale,0.35f,2.50f,"%.2f"))
                    state->river_set_selected_point_depth_scale(state->action_ctx,depth_scale);
            }
            if(state->river_set_selected_point_flow_speed){
                float flow_speed=state->river_point_flow_speed;
                if(ImGui::SliderFloat("Flow Speed",&flow_speed,0.20f,3.00f,"%.2f"))
                    state->river_set_selected_point_flow_speed(state->action_ctx,flow_speed);
            }
            if(state->river_set_selected_point_foam){
                float foam=state->river_point_foam;
                if(ImGui::SliderFloat("Foam / Rapids",&foam,0.0f,1.0f,"%.2f"))
                    state->river_set_selected_point_foam(state->action_ctx,foam);
            }
        }else{
            ImGui::TextDisabled("Click an existing point to profile it.");
        }
        draw_tool_pair(state,"Width -",EDITOR_WF_ACTION_RIVER_WIDTH_DEC,"Width +",EDITOR_WF_ACTION_RIVER_WIDTH_INC);
        draw_tool_pair(state,"Surface <",EDITOR_WF_ACTION_RIVER_STYLE_PREV,"Surface >",EDITOR_WF_ACTION_RIVER_STYLE_NEXT);
        draw_tool_button(state,"Save River",EDITOR_WF_ACTION_RIVER_SAVE);
        draw_tool_pair(state,"Undo Point",EDITOR_WF_ACTION_RIVER_UNDO,"Clear River",EDITOR_WF_ACTION_RIVER_CLEAR);
        return;
    }

    if(state->wzone_edit&&!state->wf_edit_mode){
        draw_tool_pair(state,"Radius -",EDITOR_WF_ACTION_WZONE_RADIUS_DEC,"Radius +",EDITOR_WF_ACTION_WZONE_RADIUS_INC);
        draw_tool_pair(state,"Level -",EDITOR_WF_ACTION_WZONE_LEVEL_DEC,"Level +",EDITOR_WF_ACTION_WZONE_LEVEL_INC);
        draw_tool_pair(state,"Surface <",EDITOR_WF_ACTION_WZONE_PRESET_PREV,"Surface >",EDITOR_WF_ACTION_WZONE_PRESET_NEXT);
        draw_tool_button(state,"Waterfalls",EDITOR_WF_ACTION_WZONE_WATERFALLS);
        draw_tool_button(state,"Save Water",EDITOR_WF_ACTION_WZONE_SAVE);
        draw_tool_button(state,"Clear Zones",EDITOR_WF_ACTION_WZONE_CLEAR);
        return;
    }

    if(state->wzone_edit&&state->wf_edit_mode){
        draw_tool_pair(state,"Width -",EDITOR_WF_ACTION_WF_WIDTH_DEC,"Width +",EDITOR_WF_ACTION_WF_WIDTH_INC);
        draw_tool_pair(state,"Height -",EDITOR_WF_ACTION_WF_HEIGHT_DEC,"Height +",EDITOR_WF_ACTION_WF_HEIGHT_INC);
        draw_tool_pair(state,"Yaw -",EDITOR_WF_ACTION_WF_YAW_DEC,"Yaw +",EDITOR_WF_ACTION_WF_YAW_INC);
        draw_tool_button(state,"Back Zones",EDITOR_WF_ACTION_WF_BACK_ZONES);
        draw_tool_button(state,"Save Falls",EDITOR_WF_ACTION_WF_SAVE);
        return;
    }

    if(state->sculpt_edit){
        draw_tool_pair(state,"Mode <",EDITOR_WF_ACTION_SCULPT_MODE_PREV,"Mode >",EDITOR_WF_ACTION_SCULPT_MODE_NEXT);
        draw_tool_pair(state,"Radius -",EDITOR_WF_ACTION_SCULPT_RADIUS_DEC,"Radius +",EDITOR_WF_ACTION_SCULPT_RADIUS_INC);
        draw_tool_pair(state,"Power -",EDITOR_WF_ACTION_SCULPT_POWER_DEC,"Power +",EDITOR_WF_ACTION_SCULPT_POWER_INC);
        draw_tool_pair(state,"Undo Stroke",EDITOR_WF_ACTION_SCULPT_UNDO,"Save Terrain",EDITOR_WF_ACTION_SCULPT_SAVE);
        return;
    }

    if(state->forest_edit){
        draw_tool_pair(state,"Brush <",EDITOR_WF_ACTION_FOREST_BRUSH_PREV,"Brush >",EDITOR_WF_ACTION_FOREST_BRUSH_NEXT);
        draw_tool_pair(state,"Radius -",EDITOR_WF_ACTION_FOREST_RADIUS_DEC,"Radius +",EDITOR_WF_ACTION_FOREST_RADIUS_INC);
        draw_tool_pair(state,"Density -",EDITOR_WF_ACTION_FOREST_DENSITY_DEC,"Density +",EDITOR_WF_ACTION_FOREST_DENSITY_INC);
        draw_tool_pair(state,"Undo Stroke",EDITOR_WF_ACTION_FOREST_UNDO,"Save Brush",EDITOR_WF_ACTION_FOREST_SAVE_PRIMARY);
        if(state->forest_brush_kind==0){
            if(state->forest_set_tree_scale){
                float scale=state->tree_brush_scale;
                if(ImGui::SliderFloat("Tree Scale",&scale,0.25f,20.0f,"%.2f"))
                    state->forest_set_tree_scale(state->action_ctx,scale);
            }
            draw_tool_pair(state,"Model <",EDITOR_WF_ACTION_FOREST_MODEL_PREV,"Model >",EDITOR_WF_ACTION_FOREST_MODEL_NEXT);
            draw_tool_button(state,"Random Model",EDITOR_WF_ACTION_FOREST_MODEL_RANDOM);
        }else if(state->forest_brush_kind==1){
            draw_tool_pair(state,"Size -",EDITOR_WF_ACTION_FOREST_GRASS_SIZE_DEC,"Size +",EDITOR_WF_ACTION_FOREST_GRASS_SIZE_INC);
            draw_tool_pair(state,"Style",EDITOR_WF_ACTION_FOREST_GRASS_STYLE_NEXT,"Tint",EDITOR_WF_ACTION_FOREST_GRASS_TINT_NEXT);
            draw_tool_pair(state,"HMin -",EDITOR_WF_ACTION_FOREST_GRASS_HMIN_DEC,"HMin +",EDITOR_WF_ACTION_FOREST_GRASS_HMIN_INC);
            draw_tool_pair(state,"HMax -",EDITOR_WF_ACTION_FOREST_GRASS_HMAX_DEC,"HMax +",EDITOR_WF_ACTION_FOREST_GRASS_HMAX_INC);
        }else if(state->forest_brush_kind==2){
            draw_tool_pair(state,"Size -",EDITOR_WF_ACTION_FOREST_FLOWER_SIZE_DEC,"Size +",EDITOR_WF_ACTION_FOREST_FLOWER_SIZE_INC);
            draw_tool_pair(state,"Type <",EDITOR_WF_ACTION_FOREST_FLOWER_TYPE_PREV,"Type >",EDITOR_WF_ACTION_FOREST_FLOWER_TYPE_NEXT);
        }else if(state->forest_brush_kind==4){
            if(state->forest_set_wall_spacing){
                float spacing=state->wall_brush_spacing;
                if(ImGui::SliderFloat("Fence Spacing",&spacing,1.0f,24.0f,"%.2f"))
                    state->forest_set_wall_spacing(state->action_ctx,spacing);
            }
            if(state->forest_set_wall_scale){
                float scale=state->wall_brush_scale;
                if(ImGui::SliderFloat("Fence Scale",&scale,0.25f,20.0f,"%.2f"))
                    state->forest_set_wall_scale(state->action_ctx,scale);
            }
            draw_tool_pair(state,"Spacing -",EDITOR_WF_ACTION_FOREST_WALL_SPACING_DEC,"Spacing +",EDITOR_WF_ACTION_FOREST_WALL_SPACING_INC);
            draw_tool_pair(state,"Scale -",EDITOR_WF_ACTION_FOREST_WALL_SCALE_DEC,"Scale +",EDITOR_WF_ACTION_FOREST_WALL_SCALE_INC);
        }else if(state->forest_brush_kind==5){
            draw_tool_pair(state,"Power -",EDITOR_WF_ACTION_FOREST_TERRAIN_POWER_DEC,"Power +",EDITOR_WF_ACTION_FOREST_TERRAIN_POWER_INC);
            draw_tool_button(state,"Cycle Style",EDITOR_WF_ACTION_FOREST_TERRAIN_STYLE_NEXT);
        }else if(state->forest_brush_kind==6){
            draw_tool_pair(state,"Power -",EDITOR_WF_ACTION_FOREST_TERRAIN_POWER_DEC,"Power +",EDITOR_WF_ACTION_FOREST_TERRAIN_POWER_INC);
        }
        return;
    }

    if(state->wind_edit){
        draw_tool_pair(state,"Angle -",EDITOR_WF_ACTION_WIND_ANGLE_DEC,"Angle +",EDITOR_WF_ACTION_WIND_ANGLE_INC);
        draw_tool_pair(state,"Radius -",EDITOR_WF_ACTION_WIND_RADIUS_DEC,"Radius +",EDITOR_WF_ACTION_WIND_RADIUS_INC);
        draw_tool_pair(state,"Power -",EDITOR_WF_ACTION_WIND_POWER_DEC,"Power +",EDITOR_WF_ACTION_WIND_POWER_INC);
        draw_tool_pair(state,"Gust -",EDITOR_WF_ACTION_WIND_GUST_DEC,"Gust +",EDITOR_WF_ACTION_WIND_GUST_INC);
        draw_tool_button(state,"Save Wind",EDITOR_WF_ACTION_WIND_SAVE);
        draw_tool_button(state,"Clear Wind Fields",EDITOR_WF_ACTION_WIND_CLEAR);
        return;
    }

    if(state->marker_edit){
        draw_tool_pair(state,"Type <",EDITOR_WF_ACTION_MARKER_TYPE_PREV,"Type >",EDITOR_WF_ACTION_MARKER_TYPE_NEXT);
        draw_tool_pair(state,"Radius -",EDITOR_WF_ACTION_MARKER_RADIUS_DEC,"Radius +",EDITOR_WF_ACTION_MARKER_RADIUS_INC);
        draw_tool_pair(state,"Y -",EDITOR_WF_ACTION_MARKER_Y_DEC,"Y +",EDITOR_WF_ACTION_MARKER_Y_INC);
        draw_tool_button(state,"Save Markers",EDITOR_WF_ACTION_MARKER_SAVE);
        draw_tool_button(state,"Delete Marker",EDITOR_WF_ACTION_MARKER_DELETE);
        return;
    }

    if(state->spawn_edit_mode){
        draw_tool_pair(state,"Type <",EDITOR_WF_ACTION_SPAWN_TYPE_PREV,"Type >",EDITOR_WF_ACTION_SPAWN_TYPE_NEXT);
        draw_tool_pair(state,"Mdl <",EDITOR_WF_ACTION_SPAWN_MODEL_PREV,"Mdl >",EDITOR_WF_ACTION_SPAWN_MODEL_NEXT);
        draw_tool_button(state,"Save Spawns",EDITOR_WF_ACTION_SPAWN_SAVE);
        ImGui::Separator();
        ImGui::TextDisabled("LMB near spawn=select  LMB empty=place");
        if(state->spawn_has_selection)
            ImGui::TextColored(ImVec4(0.4f,1.0f,0.5f,1.0f),"Selected: %s  See inspector ->",
                state->spawn_sel_type_name?state->spawn_sel_type_name:"?");
        else
            ImGui::TextDisabled("No spawn selected");
        return;
    }

    if(state->npc_edit_mode){
        ImGui::TextDisabled("See NPC Editor window");
        return;
    }

    if(state->particle_fx_edit){
        draw_tool_pair(state,"World",EDITOR_WF_ACTION_FX_TAB_WORLD,"Weather",EDITOR_WF_ACTION_FX_TAB_WEATHER);
        draw_tool_button(state,"Post",EDITOR_WF_ACTION_FX_TAB_POST);
        ImGui::Separator();
        draw_tool_pair(state,"Natural",EDITOR_WF_ACTION_VISUAL_LOOK_NATURAL,"Cinematic",EDITOR_WF_ACTION_VISUAL_LOOK_CINEMATIC);
        draw_tool_pair(state,"Storm",EDITOR_WF_ACTION_VISUAL_LOOK_STORM,"Low Light",EDITOR_WF_ACTION_VISUAL_LOOK_LOW_LIGHT);
        draw_tool_pair(state,"Reset Look",EDITOR_WF_ACTION_VISUAL_RESET_PROFILE,"Save Look",EDITOR_WF_ACTION_VISUAL_SAVE_PROFILE);
        ImGui::Separator();
        if(state->tool_line_a&&strstr(state->tool_line_a,"FX Tab: World")){
            draw_tool_pair(state,"Preset <",EDITOR_WF_ACTION_FX_PRESET_PREV,"Preset >",EDITOR_WF_ACTION_FX_PRESET_NEXT);
            draw_tool_pair(state,"Scale -",EDITOR_WF_ACTION_FX_SCALE_DEC,"Scale +",EDITOR_WF_ACTION_FX_SCALE_INC);
            draw_tool_pair(state,"Height -",EDITOR_WF_ACTION_FX_HEIGHT_DEC,"Height +",EDITOR_WF_ACTION_FX_HEIGHT_INC);
            draw_tool_pair(state,"Glow -",EDITOR_WF_ACTION_FX_GLOW_DEC,"Glow +",EDITOR_WF_ACTION_FX_GLOW_INC);
            draw_tool_pair(state,"Radius -",EDITOR_WF_ACTION_FX_LIGHT_RADIUS_DEC,"Radius +",EDITOR_WF_ACTION_FX_LIGHT_RADIUS_INC);
            draw_tool_pair(state,"R -",EDITOR_WF_ACTION_FX_COLOR_R_DEC,"R +",EDITOR_WF_ACTION_FX_COLOR_R_INC);
            draw_tool_pair(state,"G -",EDITOR_WF_ACTION_FX_COLOR_G_DEC,"G +",EDITOR_WF_ACTION_FX_COLOR_G_INC);
            draw_tool_pair(state,"B -",EDITOR_WF_ACTION_FX_COLOR_B_DEC,"B +",EDITOR_WF_ACTION_FX_COLOR_B_INC);
            draw_tool_pair(state,"Light",EDITOR_WF_ACTION_FX_LIGHT_TOGGLE,"Save FX",EDITOR_WF_ACTION_FX_SAVE);
            draw_tool_pair(state,"Forward",EDITOR_WF_ACTION_FX_MOVE_FWD,"Back",EDITOR_WF_ACTION_FX_MOVE_BACK);
            draw_tool_pair(state,"Left",EDITOR_WF_ACTION_FX_MOVE_LEFT,"Right",EDITOR_WF_ACTION_FX_MOVE_RIGHT);
            draw_tool_pair(state,"Up",EDITOR_WF_ACTION_FX_MOVE_UP,"Down",EDITOR_WF_ACTION_FX_MOVE_DOWN);
        }else if(state->tool_line_a&&strstr(state->tool_line_a,"FX Tab: Weather")){
            draw_tool_pair(state,"Auto",EDITOR_WF_ACTION_FX_WEATHER_AUTO,"Rain",EDITOR_WF_ACTION_FX_WEATHER_RAIN);
            draw_tool_pair(state,"Snow",EDITOR_WF_ACTION_FX_WEATHER_SNOW,"Off",EDITOR_WF_ACTION_FX_WEATHER_OFF);
            draw_tool_pair(state,"WX Toggle",EDITOR_WF_ACTION_FX_WEATHER_TOGGLE,"Save FX",EDITOR_WF_ACTION_FX_SAVE);
            draw_tool_pair(state,"Rate -",EDITOR_WF_ACTION_FX_RATE_DEC,"Rate +",EDITOR_WF_ACTION_FX_RATE_INC);
            draw_tool_pair(state,"Wet -",EDITOR_WF_ACTION_FX_WET_DEC,"Wet +",EDITOR_WF_ACTION_FX_WET_INC);
            draw_tool_pair(state,"Fog -",EDITOR_WF_ACTION_FX_FOG_DEC,"Fog +",EDITOR_WF_ACTION_FX_FOG_INC);
            draw_tool_pair(state,"Cloud -",EDITOR_WF_ACTION_FX_CLOUD_DEC,"Cloud +",EDITOR_WF_ACTION_FX_CLOUD_INC);
        }else{
            draw_tool_pair(state,"Bloom",EDITOR_WF_ACTION_FX_BLOOM_TOGGLE,"Shader",EDITOR_WF_ACTION_FX_SHADER_TOGGLE);
            draw_tool_pair(state,"Exposure -",EDITOR_WF_ACTION_FX_EXPOSURE_DEC,"Exposure +",EDITOR_WF_ACTION_FX_EXPOSURE_INC);
            draw_tool_pair(state,"Bloom -",EDITOR_WF_ACTION_FX_POST_BLOOM_DEC,"Bloom +",EDITOR_WF_ACTION_FX_POST_BLOOM_INC);
            draw_tool_pair(state,"SSAO -",EDITOR_WF_ACTION_FX_SSAO_DEC,"SSAO +",EDITOR_WF_ACTION_FX_SSAO_INC);
            draw_tool_pair(state,"Apply FX",EDITOR_WF_ACTION_FX_POST_APPLY,"Save FX",EDITOR_WF_ACTION_FX_SAVE);
            draw_tool_pair(state,"TOD -1h",EDITOR_WF_ACTION_FX_TOD_DEC,"TOD +1h",EDITOR_WF_ACTION_FX_TOD_INC);
            draw_tool_pair(state,"TOD Noon",EDITOR_WF_ACTION_FX_TOD_NOON,"FXAA",EDITOR_WF_ACTION_FX_FXAA_TOGGLE);
        }
        return;
    }

    if(state->shader_edit){
        draw_tool_pair(state,"Shader <",EDITOR_WF_ACTION_SHADER_PREV,"Shader >",EDITOR_WF_ACTION_SHADER_NEXT);
        draw_tool_pair(state,"Reload",EDITOR_WF_ACTION_SHADER_RELOAD,"Rescan",EDITOR_WF_ACTION_SHADER_RESCAN);
        draw_tool_pair(state,"Enable/Disable",EDITOR_WF_ACTION_SHADER_TOGGLE,"Apply",EDITOR_WF_ACTION_SHADER_APPLY);
        return;
    }

    if(state->interior_edit){
        draw_tool_pair(state,"Target <",EDITOR_WF_ACTION_INTERIOR_TARGET_PREV,"Target >",EDITOR_WF_ACTION_INTERIOR_TARGET_NEXT);
        draw_tool_pair(state,"Material <",EDITOR_WF_ACTION_INTERIOR_MATERIAL_PREV,"Material >",EDITOR_WF_ACTION_INTERIOR_MATERIAL_NEXT);
        ImGui::Separator();
        ImGui::TextDisabled("Materials");
        if(ImGui::BeginChild("InteriorMaterials",ImVec2(0.0f,180.0f),true)){
            for(int i=0;i<state->interior_material_count&&i<64;i++){
                const char *label=state->interior_material_labels[i]?state->interior_material_labels[i]:"Material";
                bool selected=(i==state->interior_material_selected);
                if(ImGui::Selectable(label,selected)&&state->interior_material_select){
                    state->interior_material_select(state->action_ctx,state->interior_material_indices[i]);
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndChild();
        draw_tool_button(state,"Save Interior Materials",EDITOR_WF_ACTION_INTERIOR_SAVE);
        return;
    }

    if(state->surface_edit){
        draw_tool_pair(state,"Select",EDITOR_WF_ACTION_PROP_MODE_SELECT,"Move",EDITOR_WF_ACTION_PROP_MODE_MOVE);
        draw_tool_pair(state,"Rotate",EDITOR_WF_ACTION_PROP_MODE_ROTATE,"Scale",EDITOR_WF_ACTION_PROP_MODE_SCALE);
        draw_tool_pair(state,"Coll None",EDITOR_WF_ACTION_PROP_COLL_NONE,"Coll AABB",EDITOR_WF_ACTION_PROP_COLL_AABB);
        ImGui::TextDisabled("Move: drag selected piece or axis");
        ImGui::TextDisabled("Rotate: drag selected piece or Y axis");
        ImGui::TextDisabled("Scale: X=width  Y=height/thickness  Z=depth");
        return;
    }

    if(state->plight_edit){
        draw_tool_pair(state,"Radius -",EDITOR_WF_ACTION_PLIGHT_RADIUS_DEC,"Radius +",EDITOR_WF_ACTION_PLIGHT_RADIUS_INC);
        draw_tool_pair(state,"Int -",EDITOR_WF_ACTION_PLIGHT_INTENSITY_DEC,"Int +",EDITOR_WF_ACTION_PLIGHT_INTENSITY_INC);
        draw_tool_pair(state,"Y -",EDITOR_WF_ACTION_PLIGHT_Y_DEC,"Y +",EDITOR_WF_ACTION_PLIGHT_Y_INC);
        draw_tool_pair(state,"R -",EDITOR_WF_ACTION_PLIGHT_R_DEC,"R +",EDITOR_WF_ACTION_PLIGHT_R_INC);
        draw_tool_pair(state,"G -",EDITOR_WF_ACTION_PLIGHT_G_DEC,"G +",EDITOR_WF_ACTION_PLIGHT_G_INC);
        draw_tool_pair(state,"B -",EDITOR_WF_ACTION_PLIGHT_B_DEC,"B +",EDITOR_WF_ACTION_PLIGHT_B_INC);
        draw_tool_pair(state,"Flicker",EDITOR_WF_ACTION_PLIGHT_FLICKER_TOGGLE,"Save",EDITOR_WF_ACTION_PLIGHT_SAVE);
        draw_tool_button(state,"Delete Selected",EDITOR_WF_ACTION_PLIGHT_DELETE);
        return;
    }

    if(state->prop_edit){
        draw_tool_pair(state,"Select",EDITOR_WF_ACTION_PROP_MODE_SELECT,"Move",EDITOR_WF_ACTION_PROP_MODE_MOVE);
        draw_tool_pair(state,"Rotate",EDITOR_WF_ACTION_PROP_MODE_ROTATE,"Scale",EDITOR_WF_ACTION_PROP_MODE_SCALE);
        draw_tool_pair(state,"Y -",EDITOR_WF_ACTION_PROP_Y_DEC,"Y +",EDITOR_WF_ACTION_PROP_Y_INC);
        draw_tool_pair(state,"Ground",EDITOR_WF_ACTION_PROP_GROUND,"Save",EDITOR_WF_ACTION_PROP_SAVE);
        draw_tool_pair(state,"Undo",EDITOR_WF_ACTION_PROP_UNDO,"Delete",EDITOR_WF_ACTION_PROP_DELETE);
        draw_tool_pair(state,state->prop_snap_y?"SnapY On":"SnapY Off",EDITOR_WF_ACTION_PROP_SNAP_TOGGLE,"Coll:None",EDITOR_WF_ACTION_PROP_COLL_NONE);
        draw_tool_pair(state,"Coll None",EDITOR_WF_ACTION_PROP_COLL_NONE,"Coll AABB",EDITOR_WF_ACTION_PROP_COLL_AABB);
        draw_tool_button(state,"Coll Mesh",EDITOR_WF_ACTION_PROP_COLL_MESH);
        return;
    }

    if(state->prop_place_mode){
        draw_tool_pair(state,state->placer_prefab_mode?"Prefab":"Model",EDITOR_WF_ACTION_PLACER_MODE_TOGGLE,
                       state->placer_snap_y?"SnapY On":"SnapY Off",EDITOR_WF_ACTION_PLACER_SNAP_TOGGLE);
        draw_tool_pair(state,"Yaw -",EDITOR_WF_ACTION_PLACER_ROT_DEC,"Yaw +",EDITOR_WF_ACTION_PLACER_ROT_INC);
        draw_tool_pair(state,"Scale -",EDITOR_WF_ACTION_PLACER_SCALE_DEC,"Scale +",EDITOR_WF_ACTION_PLACER_SCALE_INC);
        draw_tool_pair(state,"Save Props",EDITOR_WF_ACTION_PLACER_SAVE,"Rescan",EDITOR_WF_ACTION_PLACER_RESCAN);
        draw_tool_pair(state,"Coll None",EDITOR_WF_ACTION_PLACER_COLL_NONE,"Coll AABB",EDITOR_WF_ACTION_PLACER_COLL_AABB);
        draw_tool_button(state,"Coll Mesh",EDITOR_WF_ACTION_PLACER_COLL_MESH);
        if(state->placer_prefab_mode){
            ImGui::Separator();
            ImGui::TextDisabled("Prefab Library");
            ImGui::Text("Templates %d",state->prefab_library_count);
            if(state->prefab_selected_index>=0){
                ImGui::TextWrapped("%s",state->prefab_selected_name?state->prefab_selected_name:"Prefab");
                ImGui::Text("Props %d   FX %d",state->prefab_selected_prop_count,state->prefab_selected_fx_count);
                if(state->prefab_name_buffer&&state->prefab_name_buffer_capacity>0){
                    ImGui::InputText("Prefab Name",state->prefab_name_buffer,(size_t)state->prefab_name_buffer_capacity);
                    if(ImGui::IsItemDeactivatedAfterEdit()&&state->prefab_commit_name)
                        state->prefab_commit_name(state->action_ctx);
                }
            }else{
                ImGui::TextDisabled("No prefab selected");
            }
            if(state->prefab_capture_radius_ptr){
                ImGui::SliderFloat("Capture Radius",state->prefab_capture_radius_ptr,2.0f,48.0f,"%.1f");
            }else{
                ImGui::Text("Capture Radius %.1f",state->prefab_capture_radius);
            }
            ImGui::TextDisabled(state->prefab_capture_uses_multiselect?
                "Capture uses current multi-selection for props":
                "Capture uses selected prop as anchor");
            if(state->prefab_capture_selection){
                bool can_capture=state->prefab_capture_has_anchor;
                if(!can_capture) ImGui::BeginDisabled();
                if(ImGui::Button("Create From Selection",ImVec2(-1.0f,0.0f)))
                    state->prefab_capture_selection(state->action_ctx);
                if(!can_capture) ImGui::EndDisabled();
            }
            if(state->prefab_duplicate_selected&&state->prefab_selected_index>=0){
                if(ImGui::Button("Duplicate Prefab",ImVec2(-1.0f,0.0f)))
                    state->prefab_duplicate_selected(state->action_ctx);
            }
            if(state->prefab_delete_selected&&state->prefab_selected_index>=0){
                if(ImGui::Button("Delete Prefab",ImVec2(-1.0f,0.0f)))
                    state->prefab_delete_selected(state->action_ctx);
            }
            if(state->prefab_save_library){
                if(ImGui::Button("Save Prefabs",ImVec2(-1.0f,0.0f)))
                    state->prefab_save_library(state->action_ctx);
            }
            ImGui::Separator();
        }
        ImGui::TextDisabled("Content Browser");
        if(state->browser_toggle_favorite&&ImGui::Button(state->browser_selected_favorite?"Unfavorite Selected":"Favorite Selected",ImVec2(-1.0f,0.0f)))
            state->browser_toggle_favorite(state->action_ctx);
        if(state->browser_cycle_category_filter&&ImGui::Button(browser_category_label(state->browser_category_filter),ImVec2(-1.0f,0.0f)))
            state->browser_cycle_category_filter(state->action_ctx);
        if(state->browser_toggle_favorites_only){
            if(ImGui::Button(state->browser_favorites_only?"Favorites Only":"Show Favorites",ImVec2(-1.0f,0.0f)))
                state->browser_toggle_favorites_only(state->action_ctx);
        }
        if(state->browser_toggle_recent_only){
            if(ImGui::Button(state->browser_recent_only?"Recent Only":"Show Recent",ImVec2(-1.0f,0.0f)))
                state->browser_toggle_recent_only(state->action_ctx);
        }
        if(state->browser_selected_category&&state->browser_selected_category[0]){
            ImGui::Separator();
            ImGui::Text("Category: %s",state->browser_selected_category);
            ImGui::Text("Status: %s",state->browser_selected_status?state->browser_selected_status:"Unknown");
            if(state->browser_selected_tags&&state->browser_selected_tags[0])
                ImGui::TextWrapped("Tags: %s",state->browser_selected_tags);
            if(state->browser_selected_recent)
                ImGui::TextDisabled("Recent asset");
        }
        ImGui::Separator();
        ImGui::TextDisabled("Bulk Authoring");
        if(state->bulk_scatter_radius_ptr)
            ImGui::SliderFloat("Scatter Radius",state->bulk_scatter_radius_ptr,2.0f,64.0f,"%.1f");
        if(state->bulk_scatter_count_ptr)
            ImGui::SliderInt("Scatter Count",state->bulk_scatter_count_ptr,1,64);
        if(state->bulk_path_spacing_ptr)
            ImGui::SliderFloat("Path Spacing",state->bulk_path_spacing_ptr,1.0f,24.0f,"%.1f");
        if(state->bulk_scatter_current&&ImGui::Button(state->placer_prefab_mode?"Scatter Prefab":"Scatter Model",ImVec2(-1.0f,0.0f)))
            state->bulk_scatter_current(state->action_ctx);
        if(state->bulk_stamp_path_current&&ImGui::Button(state->placer_prefab_mode?"Stamp Prefab On Path":"Stamp Model On Path",ImVec2(-1.0f,0.0f)))
            state->bulk_stamp_path_current(state->action_ctx);
        if(state->multi_prop_count>0){
            ImGui::Text("Selected Props %d",state->multi_prop_count);
            if(state->bulk_copy_selection&&ImGui::Button("Copy Selection",ImVec2(-1.0f,0.0f)))
                state->bulk_copy_selection(state->action_ctx);
            if(state->bulk_paste_selection){
                bool can_paste=state->multi_prop_clipboard_count>0;
                if(!can_paste) ImGui::BeginDisabled();
                if(ImGui::Button("Paste Selection",ImVec2(-1.0f,0.0f)))
                    state->bulk_paste_selection(state->action_ctx);
                if(!can_paste) ImGui::EndDisabled();
            }
            if(state->bulk_duplicate_selection&&ImGui::Button("Duplicate Selection",ImVec2(-1.0f,0.0f)))
                state->bulk_duplicate_selection(state->action_ctx);
            if(state->bulk_replace_selection&&ImGui::Button("Replace Selection",ImVec2(-1.0f,0.0f)))
                state->bulk_replace_selection(state->action_ctx);
            if(state->bulk_align_x&&ImGui::Button("Align X",ImVec2(-1.0f,0.0f)))
                state->bulk_align_x(state->action_ctx);
            if(state->bulk_align_y&&ImGui::Button("Align Y",ImVec2(-1.0f,0.0f)))
                state->bulk_align_y(state->action_ctx);
            if(state->bulk_align_z&&ImGui::Button("Align Z",ImVec2(-1.0f,0.0f)))
                state->bulk_align_z(state->action_ctx);
            if(state->bulk_distribute_x&&ImGui::Button("Distribute X",ImVec2(-1.0f,0.0f)))
                state->bulk_distribute_x(state->action_ctx);
            if(state->bulk_distribute_z&&ImGui::Button("Distribute Z",ImVec2(-1.0f,0.0f)))
                state->bulk_distribute_z(state->action_ctx);
            if(state->bulk_clear_selection&&ImGui::Button("Clear Multi-Select",ImVec2(-1.0f,0.0f)))
                state->bulk_clear_selection(state->action_ctx);
        }else{
            if(state->bulk_select_all&&ImGui::Button("Select All Props",ImVec2(-1.0f,0.0f)))
                state->bulk_select_all(state->action_ctx);
            if(state->bulk_paste_selection){
                bool can_paste=state->multi_prop_clipboard_count>0;
                if(!can_paste) ImGui::BeginDisabled();
                if(ImGui::Button("Paste Clipboard",ImVec2(-1.0f,0.0f)))
                    state->bulk_paste_selection(state->action_ctx);
                if(!can_paste) ImGui::EndDisabled();
            }
        }
        ImGui::TextDisabled("Shift+click or drag-box to build a group. Paste uses the ghost/camera target.");
        ImGui::Separator();
        if(state->placer_search_buffer){
            if(ImGui::InputText("Search",state->placer_search_buffer,(size_t)state->placer_search_buffer_capacity)){
                if(!state->placer_prefab_mode&&state->browser_filter)
                    state->browser_filter(state->action_ctx);
            }
        }
        ImGui::TextDisabled("%s: %d",state->placer_prefab_mode?"Prefabs":"Models",state->placer_item_count);
        ImGui::Separator();
        float avail=ImGui::GetContentRegionAvail().x;
        int columns=(int)(avail/145.0f); if(columns<2) columns=2;
        float cell=(avail-(columns-1)*8.0f)/(float)columns;
        if(cell<48.0f) cell=48.0f;
        float thumb_sz=fminf(cell-4.0f,116.0f);
        ImGui::BeginChild("##model_browser",ImVec2(0.0f,0.0f),false,ImGuiWindowFlags_None);
        int visible_idx=0;
        for(int i=0;i<state->placer_item_count;i++){
            bool selected=(i==state->placer_item_selected);
            const char *full=state->placer_item_labels[i]?state->placer_item_labels[i]:"";
            const char *name=strrchr(full,'/');
            name=name?name+1:full;
            if(!outliner_filter_match(full,state->placer_search_buffer)) continue;
            if(state->browser_favorites_only&&!state->placer_item_favorite[i]) continue;
            if(state->browser_recent_only&&!state->placer_item_recent[i]) continue;
            if(state->browser_category_filter>0&&state->placer_item_category[i]!=state->browser_category_filter) continue;
            char short_name[40];
            strncpy(short_name,name,39); short_name[39]='\0';
            char *dot=strrchr(short_name,'.'); if(dot)*dot='\0';
            unsigned int tid=state->placer_item_thumb_ids[i];
            if((visible_idx%columns)!=0) ImGui::SameLine();
            ImGui::PushID(i);
            ImGui::BeginGroup();
            if(selected) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.36f,0.28f,0.12f,1.0f));
            bool clicked=false;
            if(tid){
                clicked=ImGui::ImageButton("##t",(ImTextureID)(unsigned long long)tid,ImVec2(thumb_sz,thumb_sz));
            }else{
                clicked=ImGui::Button("##b",ImVec2(thumb_sz,thumb_sz));
                ImVec2 bmin=ImGui::GetItemRectMin();
                ImVec2 bmax=ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddText(ImVec2(bmin.x+4,bmin.y+4),selected?IM_COL32(255,230,120,255):IM_COL32(200,200,200,255),short_name);
                (void)bmax;
            }
            if(selected) ImGui::PopStyleColor();
            if(clicked&&state->browser_select)
                state->browser_select(state->action_ctx,i,state->placer_prefab_mode);
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s%s%s",full,
                browser_category_label(state->placer_item_category[i]),
                state->placer_item_valid[i]?"  Ready":"  Missing",
                state->placer_item_favorite[i]?"  Favorite":"");
            ImGui::SetNextItemWidth(cell);
            ImGui::TextUnformatted(short_name);
            if(state->placer_item_recent[i]) ImGui::TextDisabled("Recent");
            else if(state->placer_item_favorite[i]) ImGui::TextDisabled("Favorite");
            else ImGui::TextDisabled("%s",state->placer_item_valid[i]?"Ready":"Missing");
            ImGui::EndGroup();
            ImGui::PopID();
            visible_idx++;
        }
        ImGui::EndChild();
        return;
    }

    ImGui::TextDisabled("This mode still uses custom in-world tooling.");
}

void editor_ui_imgui_draw_world_forge_shell(const EditorWorldForgeShellState *state,
                                            int win_w,int win_h){
    if(!g_imgui_ready||!state) return;
    bool editor_context_active=
        state->editor_mode||
        state->path_edit||state->wzone_edit||state->river_edit||
        state->prop_edit||state->surface_edit||state->collision_edit||state->particle_fx_edit||state->marker_edit||
        state->forest_edit||state->wind_edit||state->sculpt_edit||
        state->npc_edit_mode||state->spawn_edit_mode||state->shader_edit||
        state->prop_place_mode||state->quest_edit_mode||state->main_character_edit||state->interior_edit;
    if(!editor_context_active) return;
    bool any_primary_open=false;

    if(ImGui::BeginMainMenuBar()){
        ImGui::TextColored(ImVec4(.88f,.75f,.48f,1),"VALWOOD STUDIO");
        ImGui::SameLine(190.0f);
        if(ImGui::BeginMenu("File")){
            if(ImGui::MenuItem("Save World")&&state->save_world) state->save_world(state->action_ctx);
            if(ImGui::MenuItem("Import GLB Model")&&state->activate_mode) state->activate_mode(state->action_ctx,EDITOR_WF_MODE_PROP_PLACE);
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("View")){
            ImGui::MenuItem("Authoring Tools",nullptr,&g_show_world_forge);
            ImGui::MenuItem("Scene Outliner",nullptr,&g_show_outliner);
            ImGui::MenuItem("Inspector",nullptr,&g_show_inspector);
            ImGui::MenuItem("Toolbar",nullptr,&g_show_workflow);
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Create")){
            if(ImGui::MenuItem("Model")&&state->activate_mode) state->activate_mode(state->action_ctx,EDITOR_WF_MODE_PROP_PLACE);
            if(ImGui::MenuItem("Landscape")&&state->activate_mode) state->activate_mode(state->action_ctx,EDITOR_WF_MODE_SCULPT);
            if(ImGui::MenuItem("Light")&&state->activate_mode) state->activate_mode(state->action_ctx,EDITOR_WF_MODE_POINT_LIGHT);
            if(ImGui::MenuItem("Water")&&state->activate_mode) state->activate_mode(state->action_ctx,EDITOR_WF_MODE_WATER_ZONE);
            ImGui::EndMenu();
        }
        ImGui::SameLine(); ImGui::TextDisabled("%s",g_project.name);
        ImGui::EndMainMenuBar();
    }

    if(g_show_world_forge){
        const bool session_active[]={
            state->editor_mode&&!state->path_edit&&!state->wzone_edit&&!state->river_edit&&!state->prop_edit&&!state->surface_edit&&!state->collision_edit&&!state->particle_fx_edit&&!state->marker_edit&&!state->forest_edit&&!state->wind_edit&&!state->sculpt_edit&&!state->npc_edit_mode&&!state->spawn_edit_mode&&!state->shader_edit&&!state->interior_edit,
            false,
            false
        };

        ImGui::SetNextWindowPos(ImVec2(8.0f,36.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(240.0f,(float)win_h*0.59f-44.0f),ImGuiCond_Always);
        ImGuiWindowFlags flags=ImGuiWindowFlags_NoResize;
        if(ImGui::Begin("Authoring Tools",&g_show_world_forge,flags)){
            any_primary_open=true;
            ImGui::TextColored(ImVec4(0.95f,0.72f,0.30f,1.0f),"EDITOR SUITE");
            ImGui::Separator();
            ImGui::TextDisabled("Pick a workflow, then choose a tool on the left.");
            if(ImGui::CollapsingHeader("Build",ImGuiTreeNodeFlags_DefaultOpen)){
                draw_mode_button(state,"Terrain Sculpt",state->sculpt_edit,EDITOR_WF_MODE_SCULPT);
                draw_mode_button(state,"Forest Brush",state->forest_edit,EDITOR_WF_MODE_FOREST);
                draw_mode_button(state,"Fence Builder",state->forest_edit&&state->forest_brush_kind==4,EDITOR_WF_MODE_FENCE);
                draw_mode_button(state,"Wind Fields",state->wind_edit,EDITOR_WF_MODE_WIND);
                draw_mode_button(state,"World FX",state->particle_fx_edit,EDITOR_WF_MODE_WORLD_FX);
                draw_mode_button(state,"Shader Lab",state->shader_edit,EDITOR_WF_MODE_SHADER);
            }
            if(ImGui::CollapsingHeader("Layout",ImGuiTreeNodeFlags_DefaultOpen)){
                draw_mode_button(state,"Paths",state->path_edit,EDITOR_WF_MODE_PATH);
                draw_mode_button(state,"Ponds and Lakes",state->wzone_edit&&!state->wf_edit_mode,EDITOR_WF_MODE_WATER_ZONE);
                draw_mode_button(state,"Waterfalls",state->wzone_edit&&state->wf_edit_mode,EDITOR_WF_MODE_WATERFALL);
                draw_mode_button(state,"Rivers",state->river_edit,EDITOR_WF_MODE_RIVER);
                draw_mode_button(state,"Prop Edit",state->prop_edit,EDITOR_WF_MODE_PROP_EDIT);
                draw_mode_button(state,"Model Placer",state->prop_place_mode,EDITOR_WF_MODE_PROP_PLACE);
                draw_mode_button(state,"Surface Pieces",state->surface_edit,EDITOR_WF_MODE_SURFACE_PIECE);
                draw_mode_button(state,"Collision Volumes",state->collision_edit,EDITOR_WF_MODE_COLLISION_VOLUME);
                draw_mode_button(state,"Markers",state->marker_edit,EDITOR_WF_MODE_MARKER);
                draw_mode_button(state,"Point Lights",state->plight_edit,EDITOR_WF_MODE_POINT_LIGHT);
                draw_mode_button(state,"Decals",state->decal_edit,EDITOR_WF_MODE_DECAL);
                draw_mode_button(state,"Sound Zones",state->szone_edit,EDITOR_WF_MODE_SOUND_ZONE);
                draw_mode_button(state,"Interior Paint",state->interior_edit,EDITOR_WF_MODE_INTERIOR_MATERIAL);
            }
            if(ImGui::CollapsingHeader("Populate",ImGuiTreeNodeFlags_DefaultOpen)){
                draw_mode_button(state,"NPC Editor",state->npc_edit_mode,EDITOR_WF_MODE_NPC);
                draw_mode_button(state,"Spawn Editor",state->spawn_edit_mode,EDITOR_WF_MODE_SPAWN);
                draw_mode_button(state,"Quest Editor",state->quest_edit_mode,EDITOR_WF_MODE_QUEST);
            }
            if(ImGui::CollapsingHeader("Session",ImGuiTreeNodeFlags_DefaultOpen)){
                draw_mode_button(state,"Editor Mode",session_active[0],EDITOR_WF_MODE_SESSION);
                draw_mode_button(state,"Main Character",state->main_character_edit,EDITOR_WF_MODE_MAIN_CHARACTER);
                if(ImGui::Button("Exit Editor",ImVec2(-1.0f,0.0f))&&state->activate_mode){
                    state->activate_mode(state->action_ctx,EDITOR_WF_MODE_EXIT);
                }
                if(ImGui::Button("Save World",ImVec2(-1.0f,0.0f))&&state->save_world){
                    state->save_world(state->action_ctx);
                }
                if(ImGui::Button("Hide Editor UI",ImVec2(-1.0f,0.0f))){
                    g_show_world_forge=false;
                    g_show_workflow=false;
                    g_show_inspector=false;
                    g_show_outliner=false;
                    g_show_prop_window=false;
                    g_show_npc_window=false;
                    g_show_help=false;
                }
            }
        }
        ImGui::End();
    }

    if(g_show_workflow){
        ImGui::SetNextWindowPos(ImVec2(256.0f,36.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2((float)win_w-580.0f,96.0f),ImGuiCond_FirstUseEver);
        ImGuiWindowFlags flags=ImGuiWindowFlags_NoResize;
        if(ImGui::Begin("Workflow",&g_show_workflow,flags)){
            any_primary_open=true;
            if(ImGui::Button("Play Area")&&state->activate_mode) state->activate_mode(state->action_ctx,EDITOR_WF_MODE_EXIT);
            ImGui::SameLine();
            if(ImGui::Button("Save World")&&state->save_world) state->save_world(state->action_ctx);
            ImGui::SameLine();
            if(ImGui::Button("Models")&&state->activate_mode) state->activate_mode(state->action_ctx,EDITOR_WF_MODE_PROP_PLACE);
            ImGui::SameLine();
            if(ImGui::Button("Landscape")&&state->activate_mode) state->activate_mode(state->action_ctx,EDITOR_WF_MODE_SCULPT);
            ImGui::TextDisabled("%s",state->active_mode_text?state->active_mode_text:"Scene editing");
            if(state->save_slot){
                if(ImGui::Button("Save Slot 1")) state->save_slot(state->action_ctx,0);
                ImGui::SameLine();
                if(ImGui::Button("Save Slot 2")) state->save_slot(state->action_ctx,1);
                ImGui::SameLine();
                if(ImGui::Button("Save Slot 3")) state->save_slot(state->action_ctx,2);
            }
        }
        ImGui::End();
    }

    if(g_show_inspector){
        ImGui::SetNextWindowPos(ImVec2((float)win_w-316.0f,36.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(308.0f,(float)win_h-44.0f),ImGuiCond_FirstUseEver);
        ImGuiWindowFlags flags=ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse;
        if(ImGui::Begin("Inspector",&g_show_inspector,flags)){
            any_primary_open=true;
            ImGui::TextDisabled("Active Mode");
            ImGui::TextWrapped("%s",state->active_mode_text);
            ImGui::Separator();
            ImGui::TextDisabled("Editor UI");
            ImGui::TextWrapped("%s",state->editor_ui_text);
            ImGui::Separator();
            ImGui::TextDisabled("World Content");
            ImGui::TextWrapped("%s",state->world_content_text);
            ImGui::Separator();
            ImGui::TextDisabled("Population");
            ImGui::TextWrapped("%s",state->population_text);
            ImGui::Separator();
            if(state->main_character_edit&&(state->player_char_set_model||state->player_char_set_anim_slot)){
                ImGui::TextDisabled("Main Character");
                if(state->player_char_select_preset&&state->player_char_preset_count>0){
                    const char *preset_label=(state->player_char_preset_selected>=0&&state->player_char_preset_selected<state->player_char_preset_count&&
                                              state->player_char_preset_labels[state->player_char_preset_selected]&&
                                              state->player_char_preset_labels[state->player_char_preset_selected][0])
                        ?state->player_char_preset_labels[state->player_char_preset_selected]
                        :"Character";
                    if(ImGui::BeginCombo("Preset",preset_label)){
                        for(int i=0;i<state->player_char_preset_count&&i<32;i++){
                            bool selected=(i==state->player_char_preset_selected);
                            const char *display_label=(state->player_char_preset_labels[i]&&state->player_char_preset_labels[i][0])
                                ?state->player_char_preset_labels[i]
                                :"Character";
                            char item_id[128];
                            snprintf(item_id,sizeof(item_id),"%s##player_char_preset_%d",display_label,i);
                            if(ImGui::Selectable(item_id,selected))
                                state->player_char_select_preset(state->action_ctx,i);
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                if(state->player_char_add_preset&&ImGui::Button("Add Preset",ImVec2(-1.0f,0.0f)))
                    state->player_char_add_preset(state->action_ctx);
                if(state->player_char_duplicate_preset&&ImGui::Button("Duplicate Preset",ImVec2(-1.0f,0.0f)))
                    state->player_char_duplicate_preset(state->action_ctx);
                if(state->player_char_delete_preset&&ImGui::Button("Delete Preset",ImVec2(-1.0f,0.0f)))
                    state->player_char_delete_preset(state->action_ctx);
                if(state->player_char_set_active_preset){
                    bool is_active=(state->player_char_preset_selected==state->player_char_active_preset);
                    if(is_active) ImGui::TextDisabled("Active Preset");
                    else if(ImGui::Button("Set Active Preset",ImVec2(-1.0f,0.0f)))
                        state->player_char_set_active_preset(state->action_ctx);
                }
                if(state->player_char_commit_name&&state->player_char_name_buf&&state->player_char_name_cap>0){
                    std::string name_id="Preset Name##player_char_name";
                    if(ImGui::InputText(name_id.c_str(),state->player_char_name_buf,(size_t)state->player_char_name_cap))
                        state->player_char_commit_name(state->action_ctx,state->player_char_name_buf);
                }
                if(state->player_char_set_model&&ImGui::BeginCombo("Model",state->player_char_model_label?state->player_char_model_label:"models/main_guy.glb")){
                    for(int i=0;i<state->player_char_model_count&&i<32;i++){
                        const char *display_label=(state->player_char_model_labels[i]&&state->player_char_model_labels[i][0])
                            ?state->player_char_model_labels[i]
                            :"Model";
                        const char *path=state->player_char_model_paths[i]?state->player_char_model_paths[i]:"";
                        bool selected=(path[0]&&state->player_char_model_path&&strcmp(path,state->player_char_model_path)==0);
                        char item_id[160];
                        snprintf(item_id,sizeof(item_id),"%s##player_char_model_%d",display_label,i);
                        if(ImGui::Selectable(item_id,selected))
                            state->player_char_set_model(state->action_ctx,i);
                        if(selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if(state->player_char_set_scale){
                    float scale=state->player_char_scale;
                    if(ImGui::DragFloat("Scale",&scale,0.01f,0.01f,10.0f,"%.2f"))
                        state->player_char_set_scale(state->action_ctx,scale);
                }
                if(state->player_char_set_anim_slot){
                    static const char *slot_labels[PLAYER_CHAR_SLOT_COUNT]={
                        "Idle","Walk","Run","Attack","Attack 2","Jump","Death","Roll","Block","Hit"
                    };
                    for(int slot=0;slot<PLAYER_CHAR_SLOT_COUNT;slot++){
                        char combo_id[64];
                        char sound_combo_id[64];
                        const char *label=slot_labels[slot];
                        int selected_idx=state->player_char_slot_selected[slot];
                        const char *selected_label=(selected_idx>=0&&selected_idx<state->player_char_anim_count&&state->player_char_anim_labels[selected_idx])
                            ?state->player_char_anim_labels[selected_idx]
                            :"(Auto)";
                        snprintf(combo_id,sizeof(combo_id),"%s##player_char_slot",label);
                        if(ImGui::BeginCombo(combo_id,selected_label)){
                            for(int i=0;i<state->player_char_anim_count&&i<128;i++){
                                const char *anim_label=(state->player_char_anim_labels[i]&&state->player_char_anim_labels[i][0])
                                    ?state->player_char_anim_labels[i]
                                    :"(Auto)";
                                bool selected=(i==selected_idx);
                                char item_id[192];
                                snprintf(item_id,sizeof(item_id),"%s##player_char_anim_%d_%d",anim_label,slot,i);
                                if(ImGui::Selectable(item_id,selected))
                                    state->player_char_set_anim_slot(state->action_ctx,slot,i);
                                if(selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        if(state->player_char_set_anim_sound_slot){
                            int selected_sound_idx=state->player_char_slot_sound_selected[slot];
                            const char *selected_sound_label=(selected_sound_idx>=0&&selected_sound_idx<state->player_char_sound_count&&state->player_char_sound_labels[selected_sound_idx])
                                ?state->player_char_sound_labels[selected_sound_idx]
                                :"(None)";
                            snprintf(sound_combo_id,sizeof(sound_combo_id),"%s Sound##player_char_sound_slot",label);
                            if(ImGui::BeginCombo(sound_combo_id,selected_sound_label)){
                                for(int i=0;i<state->player_char_sound_count&&i<128;i++){
                                    const char *sound_label=(state->player_char_sound_labels[i]&&state->player_char_sound_labels[i][0])
                                        ?state->player_char_sound_labels[i]
                                        :"(None)";
                                    bool selected=(i==selected_sound_idx);
                                    char item_id[192];
                                    snprintf(item_id,sizeof(item_id),"%s##player_char_sound_%d_%d",sound_label,slot,i);
                                    if(ImGui::Selectable(item_id,selected))
                                        state->player_char_set_anim_sound_slot(state->action_ctx,slot,i);
                                    if(selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                        }
                    }
                }
                if(state->player_char_save&&ImGui::Button("Save Character Setup",ImVec2(-1.0f,0.0f)))
                    state->player_char_save(state->action_ctx);
                ImGui::Separator();
            }
            ImGui::TextDisabled("%s",state->inspect_label?state->inspect_label:"Active Selection");
            if(state->inspect_line_a&&state->inspect_line_a[0]) ImGui::TextWrapped("%s",state->inspect_line_a);
            if(state->inspect_line_b&&state->inspect_line_b[0]) ImGui::TextWrapped("%s",state->inspect_line_b);
            if(state->inspect_line_c&&state->inspect_line_c[0]) ImGui::TextWrapped("%s",state->inspect_line_c);
            if(state->selection.kind!=EDITOR_SELECTION_NONE&&state->tool_action){
                const bool gizmo_supported=
                    state->selection.kind==EDITOR_SELECTION_PROP||
                    state->selection.kind==EDITOR_SELECTION_SURFACE||
                    state->selection.kind==EDITOR_SELECTION_COLLISION_VOLUME||
                    state->selection.kind==EDITOR_SELECTION_MARKER||
                    state->selection.kind==EDITOR_SELECTION_SPAWN||
                    state->selection.kind==EDITOR_SELECTION_NPC;
                if(gizmo_supported){
                    draw_tool_pair(state,"Select",EDITOR_WF_ACTION_PROP_MODE_SELECT,"Move",EDITOR_WF_ACTION_PROP_MODE_MOVE);
                    if(state->selection.kind==EDITOR_SELECTION_MARKER||state->selection.kind==EDITOR_SELECTION_SPAWN){
                        draw_tool_pair(state,"Scale",EDITOR_WF_ACTION_PROP_MODE_SCALE,
                                       state->gizmo_local_space?"Local Space":"World Space",EDITOR_WF_ACTION_GIZMO_SPACE_TOGGLE);
                        draw_tool_button(state,state->gizmo_snap_enabled?"Snap On":"Snap Off",EDITOR_WF_ACTION_GIZMO_SNAP_TOGGLE);
                    }else{
                        draw_tool_pair(state,"Rotate",EDITOR_WF_ACTION_PROP_MODE_ROTATE,"Scale",EDITOR_WF_ACTION_PROP_MODE_SCALE);
                        draw_tool_pair(state,state->gizmo_local_space?"Local Space":"World Space",EDITOR_WF_ACTION_GIZMO_SPACE_TOGGLE,
                                       state->gizmo_snap_enabled?"Snap On":"Snap Off",EDITOR_WF_ACTION_GIZMO_SNAP_TOGGLE);
                    }
                    ImGui::TextDisabled("Gizmo: drag axis handles in the world view");
                }
                ImGui::Spacing();
                if(ImGui::Button("Frame Selected"))
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_SELECTION_FRAME);
                ImGui::SameLine();
                if(ImGui::Button("Clear Selection"))
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_SELECTION_CLEAR);
                if(ImGui::Button("Delete Selected"))
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_SELECTION_DELETE);
                draw_tool_pair(state,"Prev",EDITOR_WF_ACTION_SELECTION_PREV,"Next",EDITOR_WF_ACTION_SELECTION_NEXT);
                draw_tool_button(state,"Duplicate Selected",EDITOR_WF_ACTION_SELECTION_DUPLICATE);
                draw_tool_pair(state,"Dup Fwd",EDITOR_WF_ACTION_SELECTION_DUPLICATE_FWD,"Dup Right",EDITOR_WF_ACTION_SELECTION_DUPLICATE_RIGHT);
                draw_tool_button(state,"Dup Grounded",EDITOR_WF_ACTION_SELECTION_DUPLICATE_GROUNDED);
                draw_tool_pair(state,"Y -",EDITOR_WF_ACTION_SELECTION_Y_DEC,"Y +",EDITOR_WF_ACTION_SELECTION_Y_INC);
                draw_tool_pair(state,"Move Fwd",EDITOR_WF_ACTION_SELECTION_MOVE_FWD,"Move Back",EDITOR_WF_ACTION_SELECTION_MOVE_BACK);
                draw_tool_pair(state,"Move Left",EDITOR_WF_ACTION_SELECTION_MOVE_LEFT,"Move Right",EDITOR_WF_ACTION_SELECTION_MOVE_RIGHT);
                if(state->selection.kind==EDITOR_SELECTION_PROP||state->selection.kind==EDITOR_SELECTION_SURFACE||state->selection.kind==EDITOR_SELECTION_NPC)
                    draw_tool_pair(state,"Yaw -",EDITOR_WF_ACTION_SELECTION_YAW_DEC,"Yaw +",EDITOR_WF_ACTION_SELECTION_YAW_INC);
                if(state->selection.kind==EDITOR_SELECTION_PROP||
                   state->selection.kind==EDITOR_SELECTION_SURFACE||
                   state->selection.kind==EDITOR_SELECTION_MARKER||
                   state->selection.kind==EDITOR_SELECTION_SPAWN)
                    draw_tool_pair(state,"Scale -",EDITOR_WF_ACTION_SELECTION_SCALE_DEC,"Scale +",EDITOR_WF_ACTION_SELECTION_SCALE_INC);
                draw_tool_pair(state,"Snap Grid",EDITOR_WF_ACTION_SELECTION_SNAP_GRID,"Face Camera",EDITOR_WF_ACTION_SELECTION_FACE_CAMERA);
                draw_tool_button(state,"Ground Selected",EDITOR_WF_ACTION_SELECTION_GROUND);
            }
            if(state->selection.kind==EDITOR_SELECTION_MARKER&&state->marker_has_selection){
                ImGui::Separator();
                ImGui::TextDisabled("Marker");
                draw_tool_pair(state,"Kind <",EDITOR_WF_ACTION_MARKER_TYPE_PREV,"Kind >",EDITOR_WF_ACTION_MARKER_TYPE_NEXT);
                if(state->marker_set_radius){
                    float radius=state->marker_radius;
                    if(ImGui::SliderFloat("Radius##marker_insp",&radius,2.0f,200.0f,"%.1f"))
                        state->marker_set_radius(state->action_ctx,radius);
                }
                if(state->marker_set_y){
                    float y=state->marker_pos_y;
                    if(ImGui::DragFloat("Height##marker_insp",&y,0.1f))
                        state->marker_set_y(state->action_ctx,y);
                }
                if(ImGui::Button("Save Marker")&&state->tool_action)
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_MARKER_SAVE);
            }else if(state->selection.kind==EDITOR_SELECTION_SPAWN&&state->spawn_has_selection){
                ImGui::Separator();
                ImGui::TextDisabled("Spawn");
                ImGui::TextWrapped("%s",state->spawn_sel_type_name?state->spawn_sel_type_name:"Enemy");
                if(state->spawn_sel_model_idx){
                    int cur=*state->spawn_sel_model_idx;
                    const char *cur_label=(cur<=0||state->spawn_model_count==0)?"Default":
                        (cur-1<state->spawn_model_count&&state->spawn_model_names[cur-1]?
                         state->spawn_model_names[cur-1]:"Default");
                    if(ImGui::BeginCombo("Model##spawn_insp",cur_label)){
                        bool def_sel=(cur<=0);
                        if(ImGui::Selectable("Default",def_sel)){
                            if(state->spawn_set_model_idx) state->spawn_set_model_idx(state->action_ctx,0);
                        }
                        if(def_sel) ImGui::SetItemDefaultFocus();
                        for(int _mi=0;_mi<state->spawn_model_count;_mi++){
                            bool sel=(cur==_mi+1);
                            const char *nm=state->spawn_model_names[_mi]?state->spawn_model_names[_mi]:"?";
                            if(ImGui::Selectable(nm,sel)){
                                if(state->spawn_set_model_idx) state->spawn_set_model_idx(state->action_ctx,_mi+1);
                            }
                            if(sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                if(state->spawn_model_scale){
                    float value=*state->spawn_model_scale;
                    if(state->spawn_set_model_scale&&ImGui::SliderFloat("Model Scale##spawn_insp",&value,0.01f,5.0f,"%.3f"))
                        state->spawn_set_model_scale(state->action_ctx,value);
                }
                if(state->spawn_sel_hp&&state->spawn_set_hp_override){
                    float value=*state->spawn_sel_hp;
                    if(ImGui::InputFloat("HP##spawn_insp",&value,0,0,"%.1f"))
                        state->spawn_set_hp_override(state->action_ctx,value);
                }
                if(state->spawn_sel_dmg&&state->spawn_set_damage_override){
                    float value=*state->spawn_sel_dmg;
                    if(ImGui::InputFloat("Damage##spawn_insp",&value,0,0,"%.1f"))
                        state->spawn_set_damage_override(state->action_ctx,value);
                }
                if(state->spawn_sel_speed&&state->spawn_set_speed_override){
                    float value=*state->spawn_sel_speed;
                    if(ImGui::InputFloat("Speed##spawn_insp",&value,0,0,"%.2f"))
                        state->spawn_set_speed_override(state->action_ctx,value);
                }
                if(state->spawn_sel_chase_r&&state->spawn_set_chase_radius_override){
                    float value=*state->spawn_sel_chase_r;
                    if(ImGui::InputFloat("Chase R##spawn_insp",&value,0,0,"%.1f"))
                        state->spawn_set_chase_radius_override(state->action_ctx,value);
                }
                if(state->spawn_sel_atk_r&&state->spawn_set_attack_radius_override){
                    float value=*state->spawn_sel_atk_r;
                    if(ImGui::InputFloat("Atk R##spawn_insp",&value,0,0,"%.1f"))
                        state->spawn_set_attack_radius_override(state->action_ctx,value);
                }
                if(state->spawn_sel_atk_cd&&state->spawn_set_attack_cooldown_override){
                    float value=*state->spawn_sel_atk_cd;
                    if(ImGui::InputFloat("Atk CD##spawn_insp",&value,0,0,"%.2f"))
                        state->spawn_set_attack_cooldown_override(state->action_ctx,value);
                }
                if(state->spawn_sel_scale&&state->spawn_set_scale_override){
                    float value=*state->spawn_sel_scale;
                    if(ImGui::InputFloat("Scale##spawn_insp",&value,0,0,"%.2f"))
                        state->spawn_set_scale_override(state->action_ctx,value);
                }
                if(ImGui::Button("Save Spawn")&&state->tool_action)
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_SPAWN_SAVE);
            }else if(state->selection.kind==EDITOR_SELECTION_SURFACE||state->surface_edit){
                ImGui::Separator();
                ImGui::TextDisabled("Surface Piece");
                const char *kind_label=(state->surface_kind==0)?"Wall":"Floor";
                if(state->surface_set_kind&&ImGui::BeginCombo("Kind##surface_insp",kind_label)){
                    bool wall_sel=(state->surface_kind==0);
                    bool floor_sel=(state->surface_kind==1);
                    if(ImGui::Selectable("Wall",wall_sel)) state->surface_set_kind(state->action_ctx,0);
                    if(wall_sel) ImGui::SetItemDefaultFocus();
                    if(ImGui::Selectable("Floor",floor_sel)) state->surface_set_kind(state->action_ctx,1);
                    ImGui::EndCombo();
                }
                if(state->surface_set_width){
                    float width=state->surface_width;
                    if(ImGui::SliderFloat("Width##surface_insp",&width,0.25f,64.0f,"%.2f"))
                        state->surface_set_width(state->action_ctx,width);
                }
                if(state->surface_set_height){
                    float height=state->surface_height;
                    if(ImGui::SliderFloat(state->surface_kind==0?"Height##surface_insp":"Depth##surface_insp",&height,0.25f,64.0f,"%.2f"))
                        state->surface_set_height(state->action_ctx,height);
                }
                if(state->surface_set_thickness){
                    float thickness=state->surface_thickness;
                    if(ImGui::SliderFloat("Thickness##surface_insp",&thickness,0.05f,8.0f,"%.2f"))
                        state->surface_set_thickness(state->action_ctx,thickness);
                }
                if(state->surface_set_collision){
                    const char *coll_label=state->surface_coll_name?state->surface_coll_name:"AABB";
                    if(ImGui::BeginCombo("Collision##surface_insp",coll_label)){
                        bool none_sel=(state->surface_coll_selected==0);
                        bool aabb_sel=(state->surface_coll_selected==1);
                        if(ImGui::Selectable("None",none_sel))
                            state->surface_set_collision(state->action_ctx,0);
                        if(none_sel) ImGui::SetItemDefaultFocus();
                        if(ImGui::Selectable("AABB",aabb_sel))
                            state->surface_set_collision(state->action_ctx,1);
                        ImGui::EndCombo();
                    }
                }
                if(state->surface_material_select&&ImGui::BeginCombo("Material##surface_insp",
                    (state->surface_material_selected>=0&&state->surface_material_selected<state->surface_material_count&&
                     state->surface_material_labels[state->surface_material_selected])?
                     state->surface_material_labels[state->surface_material_selected]:"Material")){
                    for(int i=0;i<state->surface_material_count&&i<64;i++){
                        const char *label=state->surface_material_labels[i]?state->surface_material_labels[i]:"Material";
                        bool selected=(i==state->surface_material_selected);
                        if(ImGui::Selectable(label,selected))
                            state->surface_material_select(state->action_ctx,state->surface_material_indices[i]);
                        if(selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::SeparatorText("Presets");
                if(ImGui::Button("Stone Cliff Wall",ImVec2(-1.0f,0.0f)))
                    apply_surface_preset(state,0,18.0f,10.0f,3.0f,1);
                if(ImGui::Button("Stone Door Frame",ImVec2(-1.0f,0.0f)))
                    apply_surface_preset(state,0,8.0f,12.0f,1.25f,1);
                if(ImGui::Button("Stone Floor Slab",ImVec2(-1.0f,0.0f)))
                    apply_surface_preset(state,1,10.0f,8.0f,0.75f,1);
                ImGui::TextDisabled("Use Wall + stone for cliffs, sealed entrances, and portal facades.");
                if(state->save_world&&ImGui::Button("Save Surfaces",ImVec2(-1.0f,0.0f)))
                    state->save_world(state->action_ctx);
            }else if(state->selection.kind==EDITOR_SELECTION_COLLISION_VOLUME||state->collision_edit){
                ImGui::Separator();
                ImGui::TextDisabled("Collision Volume");
                if(state->collision_set_shape){
                    int shape=state->collision_shape_selected;
                    if(ImGui::RadioButton("Box##collision_shape",shape==0))
                        state->collision_set_shape(state->action_ctx,0);
                    ImGui::SameLine();
                    if(ImGui::RadioButton("Oval##collision_shape",shape==1))
                        state->collision_set_shape(state->action_ctx,1);
                }
                if(state->collision_set_width){
                    float width=state->collision_width;
                    if(ImGui::SliderFloat("Width##collision_insp",&width,0.25f,64.0f,"%.2f"))
                        state->collision_set_width(state->action_ctx,width);
                }
                if(state->collision_set_height){
                    float height=state->collision_height;
                    if(ImGui::SliderFloat("Height##collision_insp",&height,0.25f,64.0f,"%.2f"))
                        state->collision_set_height(state->action_ctx,height);
                }
                if(state->collision_set_depth){
                    float depth=state->collision_depth;
                    if(ImGui::SliderFloat("Depth##collision_insp",&depth,0.25f,64.0f,"%.2f"))
                        state->collision_set_depth(state->action_ctx,depth);
                }
                if(state->collision_set_yaw){
                    float yaw=state->collision_yaw;
                    if(ImGui::SliderFloat("Yaw##collision_insp",&yaw,-180.0f,180.0f,"%.0f deg"))
                        state->collision_set_yaw(state->action_ctx,yaw);
                }
                if(state->collision_set_pitch&&state->collision_shape_selected==0){
                    float pitch=state->collision_pitch;
                    if(ImGui::SliderFloat("Pitch X##collision_insp",&pitch,-89.0f,89.0f,"%.0f deg"))
                        state->collision_set_pitch(state->action_ctx,pitch);
                }
                if(state->collision_set_enabled){
                    bool enabled=state->collision_enabled;
                    if(ImGui::Checkbox("Collision Enabled",&enabled))
                        state->collision_set_enabled(state->action_ctx,enabled);
                }
                if(state->collision_set_nav_blocker){
                    bool nav_blocker=state->collision_nav_blocker;
                    if(ImGui::Checkbox("Blocks Nav",&nav_blocker))
                        state->collision_set_nav_blocker(state->action_ctx,nav_blocker);
                }
                if(state->collision_set_motion_enabled){
                    bool motion_enabled=state->collision_motion_enabled;
                    if(ImGui::Checkbox("Moving Platform",&motion_enabled))
                        state->collision_set_motion_enabled(state->action_ctx,motion_enabled);
                }
                if(state->collision_set_motion_axis){
                    float move_x=state->collision_motion_x;
                    float move_y=state->collision_motion_y;
                    float move_z=state->collision_motion_z;
                    if(ImGui::DragFloat("Move X##collision_insp",&move_x,0.1f,-64.0f,64.0f,"%.2f"))
                        state->collision_set_motion_axis(state->action_ctx,0,move_x);
                    if(ImGui::DragFloat("Move Y##collision_insp",&move_y,0.1f,-64.0f,64.0f,"%.2f"))
                        state->collision_set_motion_axis(state->action_ctx,1,move_y);
                    if(ImGui::DragFloat("Move Z##collision_insp",&move_z,0.1f,-64.0f,64.0f,"%.2f"))
                        state->collision_set_motion_axis(state->action_ctx,2,move_z);
                }
                if(state->collision_set_motion_period){
                    float period=state->collision_motion_period;
                    if(ImGui::SliderFloat("Motion Period##collision_insp",&period,0.1f,20.0f,"%.2f s"))
                        state->collision_set_motion_period(state->action_ctx,period);
                }
                if(state->collision_set_motion_phase){
                    float phase=state->collision_motion_phase;
                    if(ImGui::SliderFloat("Motion Phase##collision_insp",&phase,-3.14159f,3.14159f,"%.2f rad"))
                        state->collision_set_motion_phase(state->action_ctx,phase);
                }
                if(state->save_world&&ImGui::Button("Save Collision Volumes",ImVec2(-1.0f,0.0f)))
                    state->save_world(state->action_ctx);
            }else if(state->plight_edit){
                ImGui::Separator();
                ImGui::TextDisabled("Point Light");
                if(state->tool_action){
                    draw_tool_pair(state,"Select",EDITOR_WF_ACTION_PROP_MODE_SELECT,"Move",EDITOR_WF_ACTION_PROP_MODE_MOVE);
                    draw_tool_button(state,state->gizmo_snap_enabled?"Snap On":"Snap Off",EDITOR_WF_ACTION_GIZMO_SNAP_TOGGLE);
                    ImGui::TextDisabled("Move mode: drag axis handles in the world view");
                }
                if(state->plight_has_selection){
                    ImGui::Text("Light %d",state->plight_selected_index+1);
                    ImGui::TextWrapped("Pos %.1f %.1f %.1f",state->plight_pos_x,state->plight_pos_y+state->plight_y_offset,state->plight_pos_z);
                    if(state->plight_set_position){
                        float x=state->plight_pos_x;
                        float y=state->plight_pos_y;
                        float z=state->plight_pos_z;
                        if(ImGui::DragFloat("X##plight",&x,0.1f,-8192.0f,8192.0f,"%.2f")) state->plight_set_position(state->action_ctx,0,x);
                        if(ImGui::DragFloat("Y##plight",&y,0.1f,-8192.0f,8192.0f,"%.2f")) state->plight_set_position(state->action_ctx,1,y);
                        if(ImGui::DragFloat("Z##plight",&z,0.1f,-8192.0f,8192.0f,"%.2f")) state->plight_set_position(state->action_ctx,2,z);
                    }
                }else{
                    ImGui::TextWrapped("No light selected. Click an existing light to edit it, or click empty ground to place one.");
                }
                ImGui::Separator();
                ImGui::TextDisabled("Directional Rig");
                if(state->world_light_set_override){
                    bool enabled=state->world_light_override;
                    if(ImGui::Checkbox("Manual Sun Override",&enabled))
                        state->world_light_set_override(state->action_ctx,enabled);
                }
                if(state->world_light_set_angles){
                    float yaw=state->world_light_yaw;
                    float pitch=state->world_light_pitch;
                    if(ImGui::SliderFloat("Sun Yaw##world_light",&yaw,-180.0f,180.0f,"%.0f deg")|
                       ImGui::SliderFloat("Sun Pitch##world_light",&pitch,-89.0f,89.0f,"%.0f deg")){
                        state->world_light_set_angles(state->action_ctx,yaw,pitch);
                    }
                }
                if(state->world_light_set_sun_color){
                    float r=state->world_light_sun_r;
                    float g=state->world_light_sun_g;
                    float b=state->world_light_sun_b;
                    if(ImGui::SliderFloat("Sun Red##world_light",&r,0.0f,3.0f,"%.2f")) state->world_light_set_sun_color(state->action_ctx,0,r);
                    if(ImGui::SliderFloat("Sun Green##world_light",&g,0.0f,3.0f,"%.2f")) state->world_light_set_sun_color(state->action_ctx,1,g);
                    if(ImGui::SliderFloat("Sun Blue##world_light",&b,0.0f,3.0f,"%.2f")) state->world_light_set_sun_color(state->action_ctx,2,b);
                }
                if(state->world_light_set_ambient_color){
                    float r=state->world_light_ambient_r;
                    float g=state->world_light_ambient_g;
                    float b=state->world_light_ambient_b;
                    if(ImGui::SliderFloat("Ambient Red##world_light",&r,0.0f,1.5f,"%.2f")) state->world_light_set_ambient_color(state->action_ctx,0,r);
                    if(ImGui::SliderFloat("Ambient Green##world_light",&g,0.0f,1.5f,"%.2f")) state->world_light_set_ambient_color(state->action_ctx,1,g);
                    if(ImGui::SliderFloat("Ambient Blue##world_light",&b,0.0f,1.5f,"%.2f")) state->world_light_set_ambient_color(state->action_ctx,2,b);
                }
                ImGui::Separator();
                ImGui::TextDisabled("Point Light");
                if(state->plight_set_type){
                    static const char *light_type_labels[]={"Point","Spot"};
                    int type=state->plight_type_selected;
                    const char *type_label=(type>=0&&type<2)?light_type_labels[type]:"Point";
                    if(ImGui::BeginCombo("Type##plight",type_label)){
                        for(int i=0;i<2;i++){
                            bool selected=(type==i);
                            if(ImGui::Selectable(light_type_labels[i],selected))
                                state->plight_set_type(state->action_ctx,i);
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                if(state->plight_set_color){
                    float r=state->plight_color_r;
                    float g=state->plight_color_g;
                    float b=state->plight_color_b;
                    if(ImGui::SliderFloat("Red##plight",&r,0.0f,1.0f,"%.2f")) state->plight_set_color(state->action_ctx,0,r);
                    if(ImGui::SliderFloat("Green##plight",&g,0.0f,1.0f,"%.2f")) state->plight_set_color(state->action_ctx,1,g);
                    if(ImGui::SliderFloat("Blue##plight",&b,0.0f,1.0f,"%.2f")) state->plight_set_color(state->action_ctx,2,b);
                }
                if(state->plight_set_radius){
                    float radius=state->plight_radius;
                    if(ImGui::SliderFloat("Radius##plight",&radius,2.0f,60.0f,"%.1f")) state->plight_set_radius(state->action_ctx,radius);
                }
                if(state->plight_set_intensity){
                    float intensity=state->plight_intensity;
                    if(ImGui::SliderFloat("Intensity##plight",&intensity,0.05f,4.0f,"%.2f")) state->plight_set_intensity(state->action_ctx,intensity);
                }
                if(state->plight_set_y_offset){
                    float yoff=state->plight_y_offset;
                    if(ImGui::SliderFloat("Y Offset##plight",&yoff,0.0f,20.0f,"%.2f")) state->plight_set_y_offset(state->action_ctx,yoff);
                }
                if(state->plight_set_flicker){
                    bool flicker=state->plight_flicker;
                    if(ImGui::Checkbox("Flicker##plight",&flicker)) state->plight_set_flicker(state->action_ctx,flicker);
                }
                if(state->plight_set_flicker_speed){
                    float speed=state->plight_flicker_speed;
                    if(ImGui::SliderFloat("Flicker Speed##plight",&speed,0.5f,20.0f,"%.2f")) state->plight_set_flicker_speed(state->action_ctx,speed);
                }
                if(state->plight_type_selected==1){
                    if(state->plight_set_spot_angles){
                        float yaw=state->plight_spot_yaw;
                        float pitch=state->plight_spot_pitch;
                        if(ImGui::SliderFloat("Spot Yaw##plight",&yaw,-180.0f,180.0f,"%.0f deg")|
                           ImGui::SliderFloat("Spot Pitch##plight",&pitch,-89.0f,89.0f,"%.0f deg")){
                            state->plight_set_spot_angles(state->action_ctx,yaw,pitch);
                        }
                    }
                    if(state->plight_set_spot_cone){
                        float inner_deg=state->plight_spot_inner_deg;
                        float outer_deg=state->plight_spot_outer_deg;
                        bool changed=false;
                        changed|=ImGui::SliderFloat("Inner Cone##plight",&inner_deg,1.0f,85.0f,"%.0f deg");
                        changed|=ImGui::SliderFloat("Outer Cone##plight",&outer_deg,2.0f,89.0f,"%.0f deg");
                        if(changed) state->plight_set_spot_cone(state->action_ctx,inner_deg,outer_deg);
                    }
                }
                if(state->plight_delete_selected&&state->plight_has_selection&&ImGui::Button("Delete Light",ImVec2(-1.0f,0.0f))){
                    state->plight_delete_selected(state->action_ctx);
                }
                if(state->save_world&&ImGui::Button("Save Lights",ImVec2(-1.0f,0.0f)))
                    state->save_world(state->action_ctx);
            }else if(state->szone_edit){
                ImGui::Separator();
                ImGui::TextDisabled("Sound Zone");
                if(state->szone_has_selection){
                    ImGui::Text("Zone %d",state->szone_selected_index+1);
                    ImGui::TextWrapped("Click an existing zone ring to select it, or click empty ground to place a new one.");
                    if(state->szone_set_position){
                        float x=state->szone_pos_x;
                        float z=state->szone_pos_z;
                        if(ImGui::DragFloat("X##szone",&x,0.1f,-8192.0f,8192.0f,"%.2f"))
                            state->szone_set_position(state->action_ctx,0,x);
                        if(ImGui::DragFloat("Z##szone",&z,0.1f,-8192.0f,8192.0f,"%.2f"))
                            state->szone_set_position(state->action_ctx,2,z);
                    }
                }else{
                    ImGui::TextWrapped("No zone selected. Click empty ground to place a zone using the settings below.");
                }
                if(state->szone_set_type){
                    const char *type_label=(state->szone_type_selected==1)?"Music":"Ambient";
                    if(ImGui::BeginCombo("Type##szone",type_label)){
                        bool ambient_selected=(state->szone_type_selected==0);
                        bool music_selected=(state->szone_type_selected==1);
                        if(ImGui::Selectable("Ambient",ambient_selected))
                            state->szone_set_type(state->action_ctx,0);
                        if(ImGui::Selectable("Music",music_selected))
                            state->szone_set_type(state->action_ctx,1);
                        ImGui::EndCombo();
                    }
                }
                if(state->szone_type_selected==1){
                    if(state->szone_music_count>0&&state->szone_set_music){
                        const char *music_label=(state->szone_music_selected>=0&&state->szone_music_selected<state->szone_music_count&&
                                                 state->szone_music_labels[state->szone_music_selected]&&state->szone_music_labels[state->szone_music_selected][0])
                            ?state->szone_music_labels[state->szone_music_selected]
                            :"Music";
                        if(ImGui::BeginCombo("Song##szone",music_label)){
                            for(int i=0;i<state->szone_music_count;i++){
                                bool selected=(i==state->szone_music_selected);
                                const char *label=(state->szone_music_labels[i]&&state->szone_music_labels[i][0])?state->szone_music_labels[i]:"Music";
                                if(ImGui::Selectable(label,selected))
                                    state->szone_set_music(state->action_ctx,i);
                                if(selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }else{
                        ImGui::TextDisabled("No songs found in background_music/");
                    }
                }else if(state->szone_ambient_sound_count>0&&state->szone_set_ambient_sound){
                    const char *ambient_label=(state->szone_ambient_sound_selected>=0&&state->szone_ambient_sound_selected<state->szone_ambient_sound_count&&
                                               state->szone_ambient_sound_labels[state->szone_ambient_sound_selected]&&state->szone_ambient_sound_labels[state->szone_ambient_sound_selected][0])
                        ?state->szone_ambient_sound_labels[state->szone_ambient_sound_selected]
                        :"Ambient";
                    if(ImGui::BeginCombo("Sound##szone",ambient_label)){
                        for(int i=0;i<state->szone_ambient_sound_count;i++){
                            bool selected=(i==state->szone_ambient_sound_selected);
                            const char *label=(state->szone_ambient_sound_labels[i]&&state->szone_ambient_sound_labels[i][0])?state->szone_ambient_sound_labels[i]:"Ambient";
                            if(ImGui::Selectable(label,selected))
                                state->szone_set_ambient_sound(state->action_ctx,i);
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                if(state->szone_set_radius){
                    float radius=state->szone_radius;
                    if(ImGui::SliderFloat("Radius##szone",&radius,4.0f,160.0f,"%.1f"))
                        state->szone_set_radius(state->action_ctx,radius);
                }
                if(state->szone_set_volume){
                    float volume=state->szone_volume;
                    if(ImGui::SliderFloat("Volume##szone",&volume,0.0f,100.0f,"%.0f"))
                        state->szone_set_volume(state->action_ctx,volume);
                }
                if(state->szone_set_fade_length){
                    float fade_len=state->szone_fade_length;
                    if(ImGui::SliderFloat("Fade Length##szone",&fade_len,0.0f,state->szone_radius,"%.1f"))
                        state->szone_set_fade_length(state->action_ctx,fade_len);
                }
                if(state->szone_delete_selected&&state->szone_has_selection&&ImGui::Button("Delete Zone",ImVec2(-1.0f,0.0f))){
                    state->szone_delete_selected(state->action_ctx);
                }
                if(state->save_world&&ImGui::Button("Save Sound Zones",ImVec2(-1.0f,0.0f)))
                    state->save_world(state->action_ctx);
            }else if(state->selection.kind==EDITOR_SELECTION_NPC&&state->npc_has_focus){
                ImGui::Separator();
                ImGui::TextDisabled("NPC");
                if(state->npc_focus_name_buffer&&state->npc_focus_name_capacity>0){
                    char buffer[128];
                    size_t cap=(size_t)state->npc_focus_name_capacity;
                    if(cap>sizeof(buffer)) cap=sizeof(buffer);
                    snprintf(buffer,cap,"%s",state->npc_focus_name_buffer);
                    if(ImGui::InputText("Name##npc_insp",buffer,cap)
                       &&state->npc_commit_focus_name){
                        state->npc_commit_focus_name(state->action_ctx,buffer);
                    }
                }
                if(state->npc_focus_backstory_buffer&&state->npc_focus_backstory_capacity>0){
                    char buffer[512];
                    size_t cap=(size_t)state->npc_focus_backstory_capacity;
                    if(cap>sizeof(buffer)) cap=sizeof(buffer);
                    snprintf(buffer,cap,"%s",state->npc_focus_backstory_buffer);
                    if(ImGui::InputTextMultiline("Backstory##npc_insp",buffer,
                                                 cap,ImVec2(-1,72.0f))
                       &&state->npc_commit_focus_backstory){
                        state->npc_commit_focus_backstory(state->action_ctx,buffer);
                    }
                }
                if(state->npc_set_focus_model_index&&state->npc_type_count>0){
                    const char *current=(state->npc_focus_model_idx>=0&&state->npc_focus_model_idx<state->npc_type_count&&state->npc_type_names[state->npc_focus_model_idx])
                        ?state->npc_type_names[state->npc_focus_model_idx]:"NPC";
                    if(ImGui::BeginCombo("Model##npc_insp",current)){
                        for(int t=0;t<state->npc_type_count&&t<EDITOR_UI_NPC_TYPE_MAX;t++){
                            bool sel=(t==state->npc_focus_model_idx);
                            const char *lbl=state->npc_type_names[t]?state->npc_type_names[t]:"NPC";
                            if(ImGui::Selectable(lbl,sel))
                                state->npc_set_focus_model_index(state->action_ctx,t);
                            if(sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                ImGui::Text("Speed %.1f   Wait %.1fs   Waypoints %d",
                    state->npc_patrol_speed,state->npc_patrol_wait,state->npc_waypoint_count);
                draw_tool_pair(state,"Merchant",EDITOR_WF_ACTION_NPC_MERCHANT_TOGGLE,"Waypoints",EDITOR_WF_ACTION_NPC_WAYPOINT_TOGGLE);
                draw_tool_pair(state,"Dialogue",state->npc_dialogue_mode?EDITOR_WF_ACTION_NPC_DIALOGUE_CLOSE:EDITOR_WF_ACTION_NPC_DIALOGUE_OPEN,
                                     "Tree",state->npc_tree_mode?EDITOR_WF_ACTION_NPC_TREE_CLOSE:EDITOR_WF_ACTION_NPC_TREE_OPEN);
                draw_tool_button(state,"Save NPCs",EDITOR_WF_ACTION_NPC_SAVE);
            }
            ImGui::Separator();
            draw_world_forge_tool_panel(state);
        }
        ImGui::End();
    }

    if(g_show_outliner){
        static char outliner_filter[64]="";
        ImGui::SetNextWindowPos(ImVec2(8.0f,(float)win_h*0.59f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(240.0f,(float)win_h*0.41f-8.0f),ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Scene Outliner",&g_show_outliner)){
            any_primary_open=true;
            ImGui::InputText("Filter",outliner_filter,sizeof(outliner_filter));
            ImGui::Separator();
            ImGui::BeginChild("##outliner_list",ImVec2(0,0),true);
            for(int i=0;i<state->outliner_count;i++){
                const char *label=state->outliner_labels[i]?state->outliner_labels[i]:"Item";
                bool selected=(state->selection.kind==state->outliner_kinds[i]&&state->selection.index==state->outliner_indices[i]);
                if(!outliner_filter_match(label,outliner_filter)) continue;
                ImGui::PushID(i);
                if(ImGui::Selectable(label,selected)&&state->selection_set)
                    state->selection_set(state->action_ctx,state->outliner_kinds[i],state->outliner_indices[i],false);
                if(ImGui::IsItemHovered()&&ImGui::IsMouseDoubleClicked(0)&&state->selection_set)
                    state->selection_set(state->action_ctx,state->outliner_kinds[i],state->outliner_indices[i],true);
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    if((state->prop_edit||state->prop_place_mode)&&g_show_prop_window){
        ImGui::SetNextWindowPos(ImVec2(256.0f,(float)win_h-330.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2((float)win_w-580.0f,322.0f),ImGuiCond_FirstUseEver);
        ImGuiWindowFlags flags=0;
        if(ImGui::Begin(state->prop_edit?"Transform":"Model Browser",&g_show_prop_window,flags)){
            any_primary_open=true;
            if(state->prop_place_mode&&state->browser_import){
                static char import_path[1024]="";
                static char import_message[256]="Drop a GLB into the window, or enter its absolute path.";
                ImGui::SetNextItemWidth(-160.0f);
                ImGui::InputTextWithHint("##import_path","Path to a self contained .glb",import_path,sizeof(import_path));
                ImGui::SameLine();
                if(ImGui::Button("Import GLB")) state->browser_import(state->action_ctx,import_path,import_message,sizeof(import_message));
                ImGui::SameLine();
                if(ImGui::Button("Rescan")&&state->browser_refresh) state->browser_refresh(state->action_ctx);
                ImGui::TextWrapped("%s",import_message);
                ImGui::Separator();
            }
            if(state->prop_has_selection){
                ImGui::TextDisabled("Selection");
                ImGui::TextWrapped("%s",state->prop_name?state->prop_name:"Prop");
                ImGui::TextWrapped("Pos %.1f %.1f %.1f",state->prop_pos_x,state->prop_pos_y,state->prop_pos_z);
                if(state->prop_edit&&state->prop_set_rotation_y){
                    float yaw=state->prop_rot_y;
                    ImGui::TextUnformatted("Yaw");
                    if(ImGui::SliderFloat("##prop_yaw",&yaw,-180.0f,180.0f,"%.0f deg")){
                        state->prop_set_rotation_y(state->action_ctx,yaw);
                    }
                }else{
                    ImGui::TextWrapped("Yaw %.1f deg",state->prop_rot_y);
                }
                if(state->prop_edit&&state->prop_set_scale){
                    float scale=state->prop_scale;
                    ImGui::TextUnformatted("Scale");
                    if(ImGui::SliderFloat("##prop_scale",&scale,0.05f,20.0f,"%.2f")){
                        state->prop_set_scale(state->action_ctx,scale);
                    }
                }else{
                    ImGui::TextWrapped("Scale %.2f",state->prop_scale);
                }
                {
                    static const char *coll_labels[]={"None","AABB","Mesh"};
                    int coll_idx=state->prop_coll_selected;
                    const char *coll_label=(coll_idx>=0&&coll_idx<3)?coll_labels[coll_idx]:(state->prop_coll_name?state->prop_coll_name:"None");
                    if(state->prop_edit&&state->prop_set_collision_mode&&ImGui::BeginCombo("Default Collision",coll_label)){
                        for(int i=0;i<3;i++){
                            bool selected=(coll_idx==i);
                            if(ImGui::Selectable(coll_labels[i],selected))
                                state->prop_set_collision_mode(state->action_ctx,i);
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }else{
                        ImGui::TextWrapped("Collision %s",state->prop_coll_name?state->prop_coll_name:"None");
                    }
                }
                if(state->prop_edit&&state->prop_set_custom_collision_enabled){
                    bool custom_enabled=state->prop_custom_coll_enabled;
                    if(ImGui::Checkbox("Use Custom Collision",&custom_enabled))
                        state->prop_set_custom_collision_enabled(state->action_ctx,custom_enabled);
                    ImGui::SameLine();
                    ImGui::TextDisabled("%d shapes",state->prop_custom_coll_count);
                    if(state->prop_add_custom_collision_box&&ImGui::Button("Add Collision Box"))
                        state->prop_add_custom_collision_box(state->action_ctx);
                    if(state->prop_custom_coll_count>0){
                        ImGui::SameLine();
                        if(state->prop_remove_custom_collision_box&&ImGui::Button("Remove Selected Box"))
                            state->prop_remove_custom_collision_box(state->action_ctx);
                        int selected_box=state->prop_custom_coll_selected;
                        std::string selected_label=(selected_box>=0)?("Box "+std::to_string(selected_box+1)):"None";
                        if(state->prop_select_custom_collision_box&&ImGui::BeginCombo("Collision Box",selected_label.c_str())){
                            for(int i=0;i<state->prop_custom_coll_count;i++){
                                bool selected=(i==selected_box);
                                std::string label="Box "+std::to_string(i+1);
                                if(ImGui::Selectable(label.c_str(),selected))
                                    state->prop_select_custom_collision_box(state->action_ctx,i);
                                if(selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        if(selected_box>=0&&selected_box<state->prop_custom_coll_count){
                            int shape=state->prop_custom_box_shape;
                            static const char *shape_labels[]={"Box","Sphere","Circle","Oval"};
                            if(state->prop_set_custom_collision_box_gizmo_mode){
                                bool gizmo_mode=state->prop_custom_box_gizmo_mode;
                                if(ImGui::Checkbox("Edit Box In World",&gizmo_mode))
                                    state->prop_set_custom_collision_box_gizmo_mode(state->action_ctx,gizmo_mode);
                                ImGui::TextDisabled("Move/Rotate/Scale modes target the selected box when enabled.");
                            }
                            float center[3]={state->prop_custom_box_center_x,state->prop_custom_box_center_y,state->prop_custom_box_center_z};
                            float half[3]={state->prop_custom_box_half_x,state->prop_custom_box_half_y,state->prop_custom_box_half_z};
                            float yaw=state->prop_custom_box_yaw;
                            const char *shape_label=(shape>=0&&shape<4)?shape_labels[shape]:"Box";
                            if(state->prop_set_custom_collision_box_shape&&ImGui::BeginCombo("Collision Shape",shape_label)){
                                for(int i=0;i<4;i++){
                                    bool selected=(shape==i);
                                    if(ImGui::Selectable(shape_labels[i],selected))
                                        state->prop_set_custom_collision_box_shape(state->action_ctx,selected_box,i);
                                    if(selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            if(state->prop_set_custom_collision_box_center){
                                if(ImGui::DragFloat3("Box Center",center,0.05f))
                                    for(int axis=0;axis<3;axis++)
                                        state->prop_set_custom_collision_box_center(state->action_ctx,selected_box,axis,center[axis]);
                            }
                            if(state->prop_set_custom_collision_box_half){
                                if(shape==1){
                                    float radius=half[0];
                                    if(ImGui::DragFloat("Sphere Radius",&radius,0.05f,0.05f,64.0f)){
                                        for(int axis=0;axis<3;axis++)
                                            state->prop_set_custom_collision_box_half(state->action_ctx,selected_box,axis,radius);
                                    }
                                }else if(shape==2){
                                    float radius=half[0];
                                    float half_height=half[1];
                                    if(ImGui::DragFloat("Circle Radius",&radius,0.05f,0.05f,64.0f)){
                                        state->prop_set_custom_collision_box_half(state->action_ctx,selected_box,0,radius);
                                        state->prop_set_custom_collision_box_half(state->action_ctx,selected_box,2,radius);
                                    }
                                    if(ImGui::DragFloat("Circle Half Height",&half_height,0.05f,0.05f,64.0f))
                                        state->prop_set_custom_collision_box_half(state->action_ctx,selected_box,1,half_height);
                                }else if(shape==3){
                                    if(ImGui::DragFloat3("Oval Half Extents",half,0.05f,0.05f,64.0f))
                                        for(int axis=0;axis<3;axis++)
                                            state->prop_set_custom_collision_box_half(state->action_ctx,selected_box,axis,half[axis]);
                                }else{
                                    if(ImGui::DragFloat3("Box Half Extents",half,0.05f,0.05f,64.0f))
                                        for(int axis=0;axis<3;axis++)
                                            state->prop_set_custom_collision_box_half(state->action_ctx,selected_box,axis,half[axis]);
                                }
                            }
                            if((shape==0||shape==3)&&state->prop_set_custom_collision_box_yaw){
                                const char *yaw_label=(shape==3)?"Oval Yaw":"Box Yaw";
                                if(ImGui::SliderFloat(yaw_label,&yaw,-180.0f,180.0f,"%.0f deg"))
                                    state->prop_set_custom_collision_box_yaw(state->action_ctx,selected_box,yaw);
                            }
                        }
                    }
                }
                if(state->prop_edit&&state->prop_set_pickable){
                    int pickable_idx=state->prop_pickable?1:0;
                    const char *pickable_label=state->prop_pickable?"Yes":"No";
                    if(ImGui::BeginCombo("Pickable",pickable_label)){
                        for(int i=0;i<2;i++){
                            bool selected=(pickable_idx==i);
                            const char *label=i?"Yes":"No";
                            if(ImGui::Selectable(label,selected))
                                state->prop_set_pickable(state->action_ctx,i!=0);
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if(state->prop_pickable){
                        const char *current_item=(state->prop_pickup_item>=0&&
                                                  state->prop_pickup_item<state->prop_pickup_item_count&&
                                                  state->prop_pickup_item_names[state->prop_pickup_item])
                            ?state->prop_pickup_item_names[state->prop_pickup_item]
                            :"None";
                        if(state->prop_set_pickup_item&&ImGui::BeginCombo("Pickup Item",current_item)){
                            for(int i=0;i<state->prop_pickup_item_count&&i<64;i++){
                                const char *label=state->prop_pickup_item_names[i]?state->prop_pickup_item_names[i]:"Item";
                                bool selected=(i==state->prop_pickup_item);
                                if(ImGui::Selectable(label,selected))
                                    state->prop_set_pickup_item(state->action_ctx,i);
                                if(selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        if(state->prop_set_pickup_count){
                            int count=state->prop_pickup_count;
                            if(ImGui::SliderInt("Pickup Count",&count,1,99))
                                state->prop_set_pickup_count(state->action_ctx,count);
                        }
                    }
                }
                if(state->prop_edit&&state->prop_set_interact_type){
                    const char *current_interact=state->prop_interact_name?state->prop_interact_name:"None";
                    if(ImGui::BeginCombo("Interaction",current_interact)){
                        for(int i=0;i<2;i++){
                            bool selected=(i==state->prop_interact_type);
                            const char *label=i?"Door":"None";
                            if(ImGui::Selectable(label,selected))
                                state->prop_set_interact_type(state->action_ctx,i);
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if(state->prop_interact_type!=0||state->prop_action_count>0){
                        if(state->prop_set_interact_radius){
                            float radius=state->prop_interact_radius;
                            if(ImGui::SliderFloat("Use Radius",&radius,0.5f,12.0f,"%.2f"))
                                state->prop_set_interact_radius(state->action_ctx,radius);
                        }
                        if(state->prop_interact_type==1&&state->prop_set_door_open_angle){
                            float open_angle=state->prop_door_open_angle;
                            if(ImGui::SliderFloat("Door Open Angle",&open_angle,-170.0f,170.0f,"%.0f deg"))
                                state->prop_set_door_open_angle(state->action_ctx,open_angle);
                        }
                    }
                }
                if(state->prop_edit){
                    ImGui::SeparatorText("Actions");
                    if(state->prop_add_action&&ImGui::Button("Add Action",ImVec2(-1.0f,0.0f)))
                        state->prop_add_action(state->action_ctx);
                    if(state->prop_action_count>0){
                        if(state->prop_action_count>1&&state->prop_select_action){
                            char current_action_slot[32];
                            snprintf(current_action_slot,sizeof(current_action_slot),"Action %d",state->prop_action_selected+1);
                            if(ImGui::BeginCombo("Selected Action",current_action_slot)){
                                for(int i=0;i<state->prop_action_count;i++){
                                    char label[32];
                                    bool selected=(i==state->prop_action_selected);
                                    snprintf(label,sizeof(label),"Action %d",i+1);
                                    if(ImGui::Selectable(label,selected))
                                        state->prop_select_action(state->action_ctx,i);
                                    if(selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                        }
                        if(state->prop_remove_action&&ImGui::Button("Remove Selected Action",ImVec2(-1.0f,0.0f)))
                            state->prop_remove_action(state->action_ctx);
                        if(state->prop_set_action_type){
                            const char *current_action=state->prop_action_name?state->prop_action_name:"None";
                            if(ImGui::BeginCombo("Action Type",current_action)){
                                for(int i=0;i<9;i++){
                                    bool selected=(i==state->prop_action_type);
                                    const char *label=
                                        (i==1)?"Toggle Nearest Light":
                                        (i==2)?"Toggle Target Light":
                                        (i==3)?"Toggle Target Collision":
                                        (i==4)?"Toggle Target Door":
                                        (i==5)?"Set World Flag":
                                        (i==6)?"Clear World Flag":
                                        (i==7)?"Play Sound":
                                        (i==8)?"Read Prompt":
                                        "None";
                                    if(ImGui::Selectable(label,selected))
                                        state->prop_set_action_type(state->action_ctx,state->prop_action_selected,i);
                                    if(selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                        }
                        if(state->prop_set_action_target_index){
                            bool target_action=(state->prop_action_type==2||
                                                state->prop_action_type==3||
                                                state->prop_action_type==4||
                                                state->prop_action_type==5||
                                                state->prop_action_type==6);
                            if(target_action){
                                if(state->prop_action_type==5||state->prop_action_type==6){
                                    int target_index=state->prop_action_target_index;
                                    if(ImGui::InputInt("Flag Index",&target_index,1,4))
                                        state->prop_set_action_target_index(state->action_ctx,state->prop_action_selected,target_index);
                                }else{
                                    const char *current_target=state->prop_action_target_label?state->prop_action_target_label:"None";
                                    if(ImGui::BeginCombo("Target",current_target)){
                                        for(int i=0;i<state->prop_action_target_count;i++){
                                            bool selected=(state->prop_action_target_ids[i]==state->prop_action_target_index);
                                            const char *label=state->prop_action_target_labels[i]?state->prop_action_target_labels[i]:"None";
                                            if(ImGui::Selectable(label,selected))
                                                state->prop_set_action_target_index(state->action_ctx,state->prop_action_selected,state->prop_action_target_ids[i]);
                                            if(selected) ImGui::SetItemDefaultFocus();
                                        }
                                        ImGui::EndCombo();
                                    }
                                }
                            }
                        }
                        if(state->prop_set_action_radius){
                            float radius=state->prop_action_radius;
                            const char *radius_label=(state->prop_action_type==7)?"Sound Range":"Action Radius";
                            if((state->prop_action_type==1||state->prop_action_type==7)&&ImGui::SliderFloat(radius_label,&radius,0.5f,30.0f,"%.2f"))
                                state->prop_set_action_radius(state->action_ctx,state->prop_action_selected,radius);
                        }
                        if(state->prop_action_type==7){
                            if(state->prop_set_action_sound_idx&&state->prop_action_sound_count>0){
                                const char *current_sound=state->prop_action_sound_name?state->prop_action_sound_name:"Menu Click";
                                if(ImGui::BeginCombo("Sound",current_sound)){
                                    for(int i=0;i<state->prop_action_sound_count&&i<32;i++){
                                        bool selected=(i==state->prop_action_sound_idx);
                                        const char *label=state->prop_action_sound_names[i]?state->prop_action_sound_names[i]:"Sound";
                                        if(ImGui::Selectable(label,selected))
                                            state->prop_set_action_sound_idx(state->action_ctx,state->prop_action_selected,i);
                                        if(selected) ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }
                            }
                            if(state->prop_set_action_sound_volume){
                                float volume=state->prop_action_sound_volume;
                                if(ImGui::SliderFloat("Sound Volume",&volume,1.0f,128.0f,"%.0f"))
                                    state->prop_set_action_sound_volume(state->action_ctx,state->prop_action_selected,volume);
                            }
                            if(state->prop_action_sound_path&&state->prop_action_sound_path_capacity>0){
                                ImGui::TextDisabled("Optional custom file path overrides the built-in sound.");
                                if(ImGui::InputText("Custom Sound File",state->prop_action_sound_path,
                                                    (size_t)state->prop_action_sound_path_capacity)&&
                                   state->prop_set_action_sound_path){
                                    state->prop_set_action_sound_path(state->action_ctx,state->prop_action_selected,
                                                                      state->prop_action_sound_path);
                                }
                            }
                        }
                        if(state->prop_set_action_condition_type){
                            const char *current_condition=state->prop_action_condition_name?state->prop_action_condition_name:"Always";
                            if(ImGui::BeginCombo("Condition",current_condition)){
                                for(int i=0;i<3;i++){
                                    bool selected=(i==state->prop_action_condition_type);
                                    const char *label=i==1?"If Flag Set":(i==2?"If Flag Clear":"Always");
                                    if(ImGui::Selectable(label,selected))
                                        state->prop_set_action_condition_type(state->action_ctx,state->prop_action_selected,i);
                                    if(selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                        }
                        if(state->prop_action_condition_type!=0&&state->prop_set_action_condition_flag){
                            int flag_index=state->prop_action_condition_flag;
                            if(ImGui::InputInt("Condition Flag",&flag_index,1,4))
                                state->prop_set_action_condition_flag(state->action_ctx,state->prop_action_selected,flag_index);
                        }
                        if(state->prop_action_type==8&&state->prop_action_prompt_text&&state->prop_action_prompt_text_capacity>0){
                            if(ImGui::InputTextMultiline("Prompt Text",state->prop_action_prompt_text,
                                                         (size_t)state->prop_action_prompt_text_capacity,ImVec2(-1.0f,96.0f))&&
                               state->prop_set_action_prompt_text){
                                state->prop_set_action_prompt_text(state->action_ctx,state->prop_action_selected,
                                                                   state->prop_action_prompt_text);
                            }
                        }
                    }else{
                        ImGui::TextDisabled("No actions on this prop.");
                    }
                }
                ImGui::Separator();
            }
            draw_world_forge_tool_panel(state);
        }
        ImGui::End();
    }

    if(state->npc_edit_mode&&g_show_npc_window){
        ImGui::SetNextWindowPos(ImVec2((float)win_w-520.0f,8.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(510.0f,(float)win_h-16.0f),ImGuiCond_FirstUseEver);
        if(ImGui::Begin("NPC Editor",&g_show_npc_window)){
            any_primary_open=true;
            if(ImGui::CollapsingHeader("Placement",ImGuiTreeNodeFlags_DefaultOpen)){
                bool place_mode=state->npc_place_mode;
                if(ImGui::Checkbox("Place New NPC",&place_mode)&&state->npc_set_place_mode){
                    state->npc_set_place_mode(state->action_ctx,place_mode);
                }
                ImGui::TextDisabled(place_mode
                    ?"LMB ground = place   LMB NPC = select"
                    :"LMB NPC = select/edit");
                if(state->npc_type_count>0){
                    const char *current=(state->npc_sel_type>=0&&state->npc_sel_type<state->npc_type_count&&state->npc_type_names[state->npc_sel_type])
                        ?state->npc_type_names[state->npc_sel_type]:"NPC";
                    if(ImGui::BeginCombo("Place Model",current)){
                        for(int t=0;t<state->npc_type_count&&t<EDITOR_UI_NPC_TYPE_MAX;t++){
                            bool sel=(t==state->npc_sel_type);
                            char lbl[96];
                            snprintf(lbl,sizeof(lbl),"%s%s (%d)",
                                state->npc_type_names[t]?state->npc_type_names[t]:"NPC",
                                state->npc_type_loaded[t]?"":" [load failed]",
                                state->npc_type_placed_counts[t]);
                            if(ImGui::Selectable(lbl,sel)&&state->tool_action){
                                int cur=state->npc_sel_type;
                                int steps=t-cur;
                                if(steps>0) for(int s=0;s<steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TYPE_NEXT);
                                else if(steps<0) for(int s=0;s<-steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TYPE_PREV);
                            }
                            if(sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                draw_tool_pair(state,"Yaw -",EDITOR_WF_ACTION_NPC_YAW_DEC,"Yaw +",EDITOR_WF_ACTION_NPC_YAW_INC);
                draw_tool_button(state,"Save NPCs",EDITOR_WF_ACTION_NPC_SAVE);
            }
            ImGui::Separator();
            if(ImGui::CollapsingHeader("Selected NPC",ImGuiTreeNodeFlags_DefaultOpen)){
                if(state->npc_has_focus){
                ImGui::TextColored(ImVec4(0.4f,1.0f,0.5f,1.0f),"%s #%d  %s",
                    state->npc_has_selection?"Selected":"Focus",
                    state->npc_focus_index+1,
                    state->npc_focus_name&&state->npc_focus_name[0]?state->npc_focus_name:"NPC");
                if(state->npc_focus_name_buffer&&state->npc_focus_name_capacity>0){
                    char buffer[128];
                    size_t cap=(size_t)state->npc_focus_name_capacity;
                    if(cap>sizeof(buffer)) cap=sizeof(buffer);
                    snprintf(buffer,cap,"%s",state->npc_focus_name_buffer);
                    if(ImGui::InputText("Name",buffer,cap)
                       &&state->npc_commit_focus_name){
                        state->npc_commit_focus_name(state->action_ctx,buffer);
                    }
                }
                if(state->npc_focus_backstory_buffer&&state->npc_focus_backstory_capacity>0){
                    char buffer[512];
                    size_t cap=(size_t)state->npc_focus_backstory_capacity;
                    if(cap>sizeof(buffer)) cap=sizeof(buffer);
                    snprintf(buffer,cap,"%s",state->npc_focus_backstory_buffer);
                    if(ImGui::InputTextMultiline("Backstory",buffer,
                                                 cap,
                                                 ImVec2(-1,88.0f))
                       &&state->npc_commit_focus_backstory){
                        state->npc_commit_focus_backstory(state->action_ctx,buffer);
                    }
                }
                if(state->npc_set_focus_model_index&&state->npc_type_count>0){
                    const char *current=(state->npc_focus_model_idx>=0&&state->npc_focus_model_idx<state->npc_type_count&&state->npc_type_names[state->npc_focus_model_idx])
                        ?state->npc_type_names[state->npc_focus_model_idx]:"NPC";
                    if(ImGui::BeginCombo("Assigned Model",current)){
                        for(int t=0;t<state->npc_type_count&&t<EDITOR_UI_NPC_TYPE_MAX;t++){
                            bool sel=(t==state->npc_focus_model_idx);
                            char lbl[96];
                            snprintf(lbl,sizeof(lbl),"%s%s",
                                state->npc_type_names[t]?state->npc_type_names[t]:"NPC",
                                state->npc_type_loaded[t]?"":" [load failed]");
                            if(ImGui::Selectable(lbl,sel)){
                                state->npc_set_focus_model_index(state->action_ctx,t);
                            }
                            if(sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                if(state->npc_set_focus_scale){
                    float scale=state->npc_focus_scale;
                    if(ImGui::DragFloat("Scale##npc_focus",&scale,0.01f,0.01f,10.0f,"%.2f"))
                        state->npc_set_focus_scale(state->action_ctx,scale);
                }
                if(state->npc_set_focus_visual_yaw_offset){
                    float visual_yaw_deg=state->npc_focus_visual_yaw_offset*57.2957795f;
                    if(ImGui::DragFloat("Facing Offset##npc_focus",&visual_yaw_deg,1.0f,-360.0f,360.0f,"%.0f deg"))
                        state->npc_set_focus_visual_yaw_offset(state->action_ctx,visual_yaw_deg*0.0174532925f);
                }
                if(state->npc_set_focus_collision_enabled){
                    bool collision_enabled=state->npc_focus_collision_enabled;
                    if(ImGui::Checkbox("Collision##npc_focus",&collision_enabled))
                        state->npc_set_focus_collision_enabled(state->action_ctx,collision_enabled);
                }
                if(state->npc_set_focus_patrol_loop){
                    bool patrol_loop=state->npc_focus_patrol_loop;
                    if(ImGui::Checkbox("Loop Path##npc_focus",&patrol_loop))
                        state->npc_set_focus_patrol_loop(state->action_ctx,patrol_loop);
                }
                if(state->npc_set_focus_patrol_speed){
                    float speed=state->npc_patrol_speed;
                    if(ImGui::DragFloat("Patrol Speed",&speed,0.05f,0.2f,6.0f,"%.2f"))
                        state->npc_set_focus_patrol_speed(state->action_ctx,speed);
                }
                if(state->npc_set_focus_patrol_wait){
                    float wait_time=state->npc_patrol_wait;
                    if(ImGui::DragFloat("Patrol Wait",&wait_time,0.1f,0.0f,15.0f,"%.1f s"))
                        state->npc_set_focus_patrol_wait(state->action_ctx,wait_time);
                }
                if(state->npc_set_focus_merchant_enabled){
                    bool merchant=state->npc_is_merchant;
                    if(ImGui::Checkbox("Merchant##npc_focus",&merchant))
                        state->npc_set_focus_merchant_enabled(state->action_ctx,merchant);
                }
                if(state->npc_is_merchant&&state->npc_set_focus_shop_id){
                    int shop_id=state->npc_shop_id;
                    if(ImGui::InputInt("Shop ID",&shop_id,1,1))
                        state->npc_set_focus_shop_id(state->action_ctx,shop_id);
                    ImGui::TextDisabled("%s",state->npc_shop_name&&state->npc_shop_name[0]?state->npc_shop_name:"(none)");
                }
                ImGui::Text("Waypoints: %d",state->npc_waypoint_count);
                draw_tool_pair(state,"Prev NPC",EDITOR_WF_ACTION_NPC_SELECT_PREV,"Next NPC",EDITOR_WF_ACTION_NPC_SELECT_NEXT);
                draw_tool_pair(state,"Clear Sel",EDITOR_WF_ACTION_NPC_SELECT_CLEAR,"Delete NPC",EDITOR_WF_ACTION_NPC_DELETE);
                float hw=(ImGui::GetContentRegionAvail().x-4.0f)*0.5f;
                bool wp=state->npc_waypoint_mode;
                if(wp) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.1f,0.1f,0.5f,1.0f));
                if(ImGui::Button(wp?"Waypoint Editing ON":"Edit Waypoints",ImVec2(hw,0))&&state->tool_action)
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_WAYPOINT_TOGGLE);
                if(wp) ImGui::PopStyleColor();
                if(wp){
                    ImGui::TextColored(ImVec4(0.3f,1.0f,0.4f,1.0f),"Click floor/ground to add waypoints");
                    ImGui::BeginChild("##npc_waypoints",ImVec2(0,100),true);
                    for(int wi=0;wi<state->npc_waypoint_count&&wi<EDITOR_UI_NPC_WAYPOINT_MAX;wi++){
                        ImGui::Text("%d:  %.2f  %.2f  %.2f",
                                    wi+1,
                                    state->npc_waypoint_x[wi],
                                    state->npc_waypoint_y[wi],
                                    state->npc_waypoint_z[wi]);
                    }
                    if(state->npc_waypoint_count==0)
                        ImGui::TextDisabled("No waypoints yet.");
                    ImGui::EndChild();
                    draw_tool_button(state,"Remove Last Waypoint",EDITOR_WF_ACTION_NPC_WAYPOINT_REMOVE_LAST);
                }
                /* Dialogue / Tree open buttons */
                draw_tool_pair(state,
                    state->npc_dialogue_mode?"Close Dialogue":"Open Dialogue",
                    state->npc_dialogue_mode?EDITOR_WF_ACTION_NPC_DIALOGUE_CLOSE:EDITOR_WF_ACTION_NPC_DIALOGUE_OPEN,
                    state->npc_tree_mode?"Close Tree":"Open Tree",
                    state->npc_tree_mode?EDITOR_WF_ACTION_NPC_TREE_CLOSE:EDITOR_WF_ACTION_NPC_TREE_OPEN);
                }else{
                    ImGui::TextDisabled("Click an NPC in the world to select it");
                }
            }
            /* ── Flat Dialogue Editor ── */
            if(state->npc_dialogue_mode&&state->npc_dialogue_buffer){
                ImGui::Separator();
                ImGui::TextDisabled("Flat Dialogue Lines");
                ImGui::BeginChild("##dlg_lines",ImVec2(0,120),true);
                for(int i=0;i<state->npc_dialogue_count&&i<EDITOR_UI_NPC_DIALOGUE_MAX;i++){
                    bool sel=(i==state->npc_dialogue_sel);
                    ImGui::PushID(i);
                    if(sel) ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(0.1f,0.2f,0.4f,1.0f));
                    char row[100];
                    snprintf(row,sizeof(row),"%d: %s",i+1,state->npc_dialogue_lines[i]?state->npc_dialogue_lines[i]:"");
                    if(ImGui::Selectable(row,sel,ImGuiSelectableFlags_AllowDoubleClick)&&state->npc_set_dialogue_selection){
                        state->npc_set_dialogue_selection(state->action_ctx,i);
                    }
                    if(sel) ImGui::PopStyleColor();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                if(ImGui::Button("New Line")&&state->npc_set_dialogue_selection)
                    state->npc_set_dialogue_selection(state->action_ctx,-1);
                ImGui::SameLine();
                ImGui::InputText("##dlg_in",state->npc_dialogue_buffer,(size_t)state->npc_dialogue_buffer_capacity);
                if(state->npc_commit_dialogue_line&&ImGui::Button("Save Line"))
                    state->npc_commit_dialogue_line(state->action_ctx);
                ImGui::SameLine();
                if(state->npc_delete_dialogue_line&&ImGui::Button("Delete Line"))
                    state->npc_delete_dialogue_line(state->action_ctx);
            }
            /* ── Dialogue Tree Editor ── */
            if(state->npc_tree_mode){
                ImGui::Separator();
                ImGui::TextDisabled("Dialogue Tree  (%d nodes  root=%d)",state->npc_tree_count,state->npc_tree_root);
                /* Node list */
                ImGui::BeginChild("##tree_nodes",ImVec2(0,120),true);
                if(state->npc_tree_count==0){
                    ImGui::TextDisabled("(empty — use Add Node)");
                }
                for(int ni=0;ni<state->npc_tree_count;ni++){
                    bool sel=(ni==state->npc_tree_sel);
                    bool is_root=(ni==state->npc_tree_root);
                    ImGui::PushID(ni);
                    if(sel) ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(0.05f,0.2f,0.1f,1.0f));
                    char node_lbl[64];
                    snprintf(node_lbl,sizeof(node_lbl),"%s%d",is_root?"[R] ":"    ",ni);
                    if(ImGui::Selectable(node_lbl,sel,0,ImVec2(36,0))&&state->tool_action){
                        int steps=ni-state->npc_tree_sel;
                        if(steps>0) for(int s=0;s<steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_NEXT);
                        else if(steps<0) for(int s=0;s<-steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_PREV);
                    }
                    if(sel) ImGui::PopStyleColor();
                    /* Show text preview on same line */
                    ImGui::SameLine();
                    /* get text from npc_tree_text only if this is the selected node */
                    if(sel&&state->npc_tree_text&&state->npc_tree_text[0]){
                        char preview[48]={0}; strncpy(preview,state->npc_tree_text,47);
                        ImGui::TextColored(ImVec4(1,1,0.4f,1),"%s",preview);
                    }else{
                        ImGui::TextDisabled("...");
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
                draw_tool_pair(state,"Add Node",EDITOR_WF_ACTION_NPC_TREE_ADD,"Delete Node",EDITOR_WF_ACTION_NPC_TREE_DELETE);
                draw_tool_pair(state,"Set Root",EDITOR_WF_ACTION_NPC_TREE_ROOT,"Prev Node",EDITOR_WF_ACTION_NPC_TREE_PREV);
                /* Current node editing */
                if(state->npc_tree_sel>=0&&state->npc_tree_sel<state->npc_tree_count){
                    ImGui::TextDisabled("Node %d",state->npc_tree_sel);
                    if(state->npc_tree_text&&state->npc_tree_text_capacity>0){
                        if(ImGui::InputTextMultiline("##node_txt",state->npc_tree_text,(size_t)state->npc_tree_text_capacity,ImVec2(-1,64))&&state->tool_action)
                            state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
                    }
                    if(state->npc_tree_speaker&&state->npc_tree_speaker_capacity>0){
                        ImGui::SetNextItemWidth(-1);
                        if(ImGui::InputText("##speaker",state->npc_tree_speaker,(size_t)state->npc_tree_speaker_capacity)&&state->tool_action)
                            state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
                        ImGui::SameLine(0,0); ImGui::TextDisabled(" Speaker");
                    }
                    if(state->npc_tree_anim_trigger){
                        ImGui::SetNextItemWidth(80);
                        if(ImGui::InputInt("Anim",state->npc_tree_anim_trigger)&&state->tool_action)
                            state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
                        ImGui::SameLine(); ImGui::TextDisabled("0=idle 1=walk 2=talk -1=none");
                    }
                    /* All choices */
                    ImGui::TextDisabled("Choices (%d / %d max)",state->npc_tree_choice_count,EDITOR_UI_NPC_TREE_CHOICES_MAX);
                    for(int ci=0;ci<state->npc_tree_choice_count&&ci<EDITOR_UI_NPC_TREE_CHOICES_MAX;ci++){
                        bool csel=(ci==state->npc_tree_choice_sel);
                        ImGui::PushID(100+ci);
                        if(csel) ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0.05f,0.15f,0.05f,1.0f));
                        /* Select this choice */
                        char clbl[8]; snprintf(clbl,sizeof(clbl),"%d",ci);
                        if(ImGui::Selectable(clbl,csel,0,ImVec2(16,0))&&state->tool_action){
                            int s=ci-state->npc_tree_choice_sel;
                            if(s>0) for(int ss=0;ss<s;ss++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_CHOICE_NEXT);
                            else if(s<0) for(int ss=0;ss<-s;ss++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_CHOICE_PREV);
                        }
                        ImGui::SameLine(0,4);
                        /* Destination label */
                        if(state->npc_tree_choice_next[ci]){
                            int dest=*state->npc_tree_choice_next[ci];
                            char dest_str[24];
                            if(dest==-1)      snprintf(dest_str,sizeof(dest_str),"[CLOSE]");
                            else if(dest==-2) snprintf(dest_str,sizeof(dest_str),"[QUEST]");
                            else if(dest==-3) snprintf(dest_str,sizeof(dest_str),"[SHOP]");
                            else              snprintf(dest_str,sizeof(dest_str),"->%d",dest);
                            ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1.0f),"%s",dest_str);
                            ImGui::SameLine(0,4);
                        }
                        /* Label input */
                        if(state->npc_tree_choice_labels[ci]){
                            ImGui::SetNextItemWidth(-1);
                            if(ImGui::InputText("##cl",state->npc_tree_choice_labels[ci],64)&&state->tool_action)
                                state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
                        }
                        /* Dest input (only for selected choice) */
                        if(csel&&state->npc_tree_choice_next[ci]){
                            ImGui::SetNextItemWidth(80);
                            if(ImGui::InputInt("Dest##cn",state->npc_tree_choice_next[ci])&&state->tool_action)
                                state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
                            ImGui::SameLine(); ImGui::TextDisabled("-1=close -2=quest -3=shop");
                        }
                        if(csel) ImGui::PopStyleColor();
                        ImGui::PopID();
                    }
                    draw_tool_pair(state,"Add Choice",EDITOR_WF_ACTION_NPC_TREE_CHOICE_ADD,"Delete Choice",EDITOR_WF_ACTION_NPC_TREE_CHOICE_DELETE);
                }
            }
        }
        ImGui::End();
    }

    /* ════════════════════════════════════════════════════════════════
       DIALOGUE GRAPH WINDOW
       Visual node-graph canvas: shown whenever the tree editor is open.
       ════════════════════════════════════════════════════════════════ */
    if(state->npc_tree_mode&&state->npc_tree_node_canvas_x&&state->npc_tree_node_canvas_y){

        /* ── Per-session canvas state ── */
        static float  s_gpan_x=20.0f,s_gpan_y=20.0f;
        static float  s_gzoom=1.0f;
        static int    s_gdrag_node=-1;
        static float  s_gdrag_ox=0.0f,s_gdrag_oy=0.0f;
        static int    s_gwire_node=-1,s_gwire_choice=-1;
        static float  s_grclick_wx=120.0f,s_grclick_wy=40.0f;
        static bool   s_g_needs_layout=true;
        static float *s_g_last_canvas=NULL;
        static bool   s_show_dlg_graph=true;
        /* ── Inline text edit state ── */
        static int    s_edit_node=-1;
        static int    s_edit_field=0;  /* 0=dialogue text  1=speaker  2=choice label */
        static int    s_edit_choice=-1;
        static char   s_edit_buf[200]={0};
        static bool   s_edit_focused=false;
        /* ── Canvas origin cache (for inline-edit popup positioning) ── */
        static ImVec2 s_cp0_cache={0,0};
        static float  s_zoom_cache=1.0f;

        /* Detect NPC change → reset pan/zoom/wire, trigger layout, re-show window */
        if(s_g_last_canvas!=state->npc_tree_node_canvas_x){
            s_g_last_canvas=state->npc_tree_node_canvas_x;
            s_gpan_x=20.0f; s_gpan_y=20.0f; s_gzoom=1.0f;
            s_gdrag_node=-1; s_gwire_node=-1; s_gwire_choice=-1;
            s_edit_node=-1; s_edit_focused=false;
            s_g_needs_layout=true;
            s_show_dlg_graph=true;
        }

        /* Trigger layout when all positions are zero (never been set) */
        if(!s_g_needs_layout&&state->npc_tree_count>1){
            bool all_zero=true;
            for(int _i=0;_i<state->npc_tree_count;_i++){
                if(state->npc_tree_node_canvas_x[_i]!=0.0f||
                   state->npc_tree_node_canvas_y[_i]!=0.0f){ all_zero=false; break; }
            }
            if(all_zero) s_g_needs_layout=true;
        }

        /* ── Auto-layout (BFS from root) ── */
        if(s_g_needs_layout&&state->npc_tree_count>0){
            s_g_needs_layout=false;
            int root=(state->npc_tree_root>=0&&state->npc_tree_root<state->npc_tree_count)
                      ?state->npc_tree_root:0;
            int depth[EDITOR_UI_NPC_TREE_MAX];
            for(int _i=0;_i<EDITOR_UI_NPC_TREE_MAX;_i++) depth[_i]=-1;
            int bfsq[EDITOR_UI_NPC_TREE_MAX]; int bfsh=0,bfst=0;
            depth[root]=0; bfsq[bfst++]=root;
            while(bfsh<bfst){
                int ni=bfsq[bfsh++];
                int nc=state->npc_tree_all_choice_count[ni];
                for(int ci=0;ci<nc;ci++){
                    int dst=state->npc_tree_all_choice_next[ni][ci];
                    if(dst>=0&&dst<state->npc_tree_count&&depth[dst]<0){
                        depth[dst]=depth[ni]+1;
                        bfsq[bfst++]=dst;
                    }
                }
            }
            /* Rows per depth column */
            int row_in_col[EDITOR_UI_NPC_TREE_MAX+1];
            for(int _i=0;_i<=EDITOR_UI_NPC_TREE_MAX;_i++) row_in_col[_i]=0;
            int max_col=0;
            for(int _i=0;_i<state->npc_tree_count;_i++)
                if(depth[_i]>max_col) max_col=depth[_i];
            int orphan_col=max_col+1, orphan_row=0;
            for(int ni=0;ni<state->npc_tree_count;ni++){
                int d,r;
                if(depth[ni]>=0){ d=depth[ni]; r=row_in_col[d]++; }
                else             { d=orphan_col; r=orphan_row++; }
                state->npc_tree_node_canvas_x[ni]=(float)d*250.0f+10.0f;
                state->npc_tree_node_canvas_y[ni]=(float)r*160.0f+10.0f;
            }
            /* Save new layout */
            if(state->tool_action)
                state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
            s_gpan_x=20.0f; s_gpan_y=20.0f;
        }

        /* ── Open window ── */
        ImGui::SetNextWindowSize(ImVec2(840.0f,560.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(290.0f,60.0f),ImGuiCond_FirstUseEver);
        ImGuiWindowFlags gflags=ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
        if(ImGui::Begin("Dialogue Graph",&s_show_dlg_graph,gflags)){
            any_primary_open=true;

            /* ── Toolbar ── */
            float tw=(ImGui::GetContentRegionAvail().x-18.0f)*0.25f;
            if(tw<40.0f) tw=40.0f;
            if(ImGui::Button("Auto Layout",ImVec2(tw,0))) s_g_needs_layout=true;
            ImGui::SameLine(0,4);
            if(ImGui::Button("+ Node",ImVec2(tw,0))&&state->tool_action){
                /* Position new node at last right-click world position */
                int ni=state->npc_tree_count;
                if(ni<EDITOR_UI_NPC_TREE_MAX){
                    state->npc_tree_node_canvas_x[ni]=s_grclick_wx;
                    state->npc_tree_node_canvas_y[ni]=s_grclick_wy;
                }
                state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_ADD);
            }
            ImGui::SameLine(0,4);
            if(ImGui::Button("Save",ImVec2(tw,0))&&state->tool_action)
                state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
            ImGui::SameLine(0,4);
            if(ImGui::Button("Close Graph",ImVec2(tw,0))&&state->tool_action)
                state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_CLOSE);
            ImGui::TextDisabled(
                "Nodes: %d  Root: %d  |  LMB title=move  output-pin=wire  RMB=menu  Scroll=zoom  Alt+LMB/MMB=pan",
                state->npc_tree_count,state->npc_tree_root);

            /* ── Canvas child region ── */
            ImVec2 csz=ImGui::GetContentRegionAvail();
            if(csz.x<50.0f) csz.x=50.0f;
            if(csz.y<50.0f) csz.y=50.0f;

            ImGui::InvisibleButton("##dlg_canvas",csz,
                ImGuiButtonFlags_MouseButtonLeft|
                ImGuiButtonFlags_MouseButtonRight|
                ImGuiButtonFlags_MouseButtonMiddle);
            bool chov=ImGui::IsItemHovered();
            ImVec2 cp0=ImGui::GetItemRectMin();
            ImVec2 cp1=ImGui::GetItemRectMax();

            ImDrawList *dl=ImGui::GetWindowDrawList();
            dl->PushClipRect(cp0,cp1,true);

            /* Cache canvas origin + zoom for the inline-edit popup (rendered after ImGui::End) */
            s_cp0_cache=cp0; s_zoom_cache=s_gzoom;

            /* Background */
            dl->AddRectFilled(cp0,cp1,IM_COL32(28,28,34,255));

            /* Dot grid */
            float gs=40.0f*s_gzoom;
            float ogx=cp0.x+fmodf(s_gpan_x*s_gzoom,gs);
            float ogy=cp0.y+fmodf(s_gpan_y*s_gzoom,gs);
            for(float gx=ogx;gx<cp1.x;gx+=gs)
                dl->AddLine(ImVec2(gx,cp0.y),ImVec2(gx,cp1.y),IM_COL32(44,44,54,255));
            for(float gy=ogy;gy<cp1.y;gy+=gs)
                dl->AddLine(ImVec2(cp0.x,gy),ImVec2(cp1.x,gy),IM_COL32(44,44,54,255));

            /* Coordinate helpers */
            ImGuiIO &io=ImGui::GetIO();
            /* W2S: world → screen */
            auto W2S=[&](float wx,float wy)->ImVec2{
                return ImVec2(cp0.x+(wx+s_gpan_x)*s_gzoom,
                              cp0.y+(wy+s_gpan_y)*s_gzoom);
            };
            /* S2W: screen → world */
            auto S2W=[&](float sx,float sy)->ImVec2{
                return ImVec2((sx-cp0.x)/s_gzoom-s_gpan_x,
                              (sy-cp0.y)/s_gzoom-s_gpan_y);
            };

            /* Node dimensions (world units) */
            const float NW  =210.0f; /* node width          */
            const float TH  = 22.0f; /* title bar height    */
            const float BH  = 52.0f; /* body text height    */
            const float CH  = 20.0f; /* choice row height   */
            const float PR  =  5.0f; /* pin radius          */

            auto node_h=[&](int ni)->float{
                int nc=(ni>=0&&ni<state->npc_tree_count)
                       ?state->npc_tree_all_choice_count[ni]:0;
                return TH+BH+(nc>0?(float)nc:1.0f)*CH+6.0f;
            };
            auto ipin_pos=[&](int ni)->ImVec2{
                return W2S(state->npc_tree_node_canvas_x[ni],
                           state->npc_tree_node_canvas_y[ni]+TH*0.5f);
            };
            auto opin_pos=[&](int ni,int ci)->ImVec2{
                return W2S(state->npc_tree_node_canvas_x[ni]+NW,
                           state->npc_tree_node_canvas_y[ni]+TH+BH+(ci+0.5f)*CH);
            };

            /* ── Draw bezier connections ── */
            for(int ni=0;ni<state->npc_tree_count;ni++){
                int nc=state->npc_tree_all_choice_count[ni];
                for(int ci=0;ci<nc;ci++){
                    int dst=state->npc_tree_all_choice_next[ni][ci];
                    if(dst>=0&&dst<state->npc_tree_count){
                        ImVec2 p0=opin_pos(ni,ci);
                        ImVec2 p3=ipin_pos(dst);
                        float cx=(p0.x+p3.x)*0.5f;
                        dl->AddBezierCubic(p0,ImVec2(cx,p0.y),ImVec2(cx,p3.y),p3,
                            IM_COL32(100,180,255,200),2.0f);
                    } else if(dst<0){
                        /* Terminal stub */
                        ImVec2 p0=opin_pos(ni,ci);
                        ImVec2 p1=ImVec2(p0.x+28.0f*s_gzoom,p0.y);
                        ImU32 tc=(dst==-1)?IM_COL32(230,80, 80,255):
                                 (dst==-2)?IM_COL32(170,100,230,255):
                                 (dst==-3)?IM_COL32(80, 210,210,255):
                                           IM_COL32(230,150, 60,255);
                        dl->AddLine(p0,p1,tc,2.0f);
                        dl->AddCircleFilled(p1,4.0f*s_gzoom,tc);
                    }
                }
            }

            /* ── In-progress wire ── */
            if(s_gwire_node>=0&&s_gwire_choice>=0&&s_gwire_node<state->npc_tree_count){
                ImVec2 p0=opin_pos(s_gwire_node,s_gwire_choice);
                ImVec2 p3=io.MousePos;
                float cx=(p0.x+p3.x)*0.5f;
                dl->AddBezierCubic(p0,ImVec2(cx,p0.y),ImVec2(cx,p3.y),p3,
                    IM_COL32(255,220,80,220),2.0f);
            }

            /* ── Draw nodes ── */
            for(int ni=0;ni<state->npc_tree_count;ni++){
                float wx=state->npc_tree_node_canvas_x[ni];
                float wy=state->npc_tree_node_canvas_y[ni];
                float nh=node_h(ni);
                ImVec2 tl=W2S(wx,wy);
                ImVec2 br=W2S(wx+NW,wy+nh);
                float screen_w=br.x-tl.x;
                bool show_text=screen_w>60.0f;
                bool is_sel=(ni==state->npc_tree_sel);
                bool is_root=(ni==state->npc_tree_root);

                /* Body */
                dl->AddRectFilled(tl,br,IM_COL32(42,42,52,255),6.0f);

                /* Title bar */
                ImVec2 tbr=W2S(wx+NW,wy+TH);
                ImU32 tc=is_root?IM_COL32(25,80,42,255):IM_COL32(28,45,85,255);
                if(is_sel) tc=is_root?IM_COL32(35,105,55,255):IM_COL32(42,72,140,255);
                dl->AddRectFilled(tl,tbr,tc,6.0f,ImDrawFlags_RoundCornersTop);
                /* Square off bottom half of title so body joins cleanly */
                ImVec2 tmid=ImVec2(tl.x,tl.y+(tbr.y-tl.y)*0.5f);
                dl->AddRectFilled(tmid,tbr,tc);

                /* Border */
                ImU32 bc=is_sel?IM_COL32(130,185,255,255):IM_COL32(72,72,88,255);
                dl->AddRect(tl,br,bc,6.0f,0,is_sel?2.0f:1.0f);

                if(show_text){
                    /* Title text */
                    const char *spkr=state->npc_tree_all_speaker[ni];
                    if(!spkr||!spkr[0]) spkr="NPC";
                    char ttxt[56];
                    if(is_root) snprintf(ttxt,sizeof(ttxt),"[R] %d: %s",ni,spkr);
                    else         snprintf(ttxt,sizeof(ttxt),"%d: %s",ni,spkr);
                    dl->AddText(ImVec2(tl.x+6.0f,tl.y+4.0f),IM_COL32(220,220,220,255),ttxt);

                    /* Body text (two 28-char lines) */
                    const char *btxt=state->npc_tree_all_text[ni];
                    if(btxt&&btxt[0]){
                        char line1[32]={0},line2[32]={0};
                        int blen=(int)strlen(btxt);
                        if(blen<=28){ strncpy(line1,btxt,28); }
                        else{
                            strncpy(line1,btxt,28);
                            int rem=blen-28; if(rem>27) rem=27;
                            strncpy(line2,btxt+28,(size_t)rem);
                        }
                        float ty=tl.y+TH*s_gzoom+4.0f;
                        dl->AddText(ImVec2(tl.x+6.0f,ty),IM_COL32(195,195,185,255),line1);
                        if(line2[0])
                            dl->AddText(ImVec2(tl.x+6.0f,ty+14.0f),IM_COL32(175,175,165,255),line2);
                    }

                    /* Choice rows */
                    int nc=state->npc_tree_all_choice_count[ni];
                    int draw_nc=(nc>0)?nc:1;
                    for(int ci=0;ci<draw_nc;ci++){
                        float cyw=wy+TH+BH+(float)ci*CH;
                        ImVec2 ctl=W2S(wx+4.0f,cyw+2.0f);
                        ImVec2 cbr=W2S(wx+NW-4.0f,cyw+CH-2.0f);
                        dl->AddRectFilled(ctl,cbr,IM_COL32(33,33,43,255),3.0f);
                        if(ci<nc){
                            int dst=state->npc_tree_all_choice_next[ni][ci];
                            /* Label text (left side, truncated to 18 chars) */
                            const char *lbl=state->npc_tree_all_choice_labels[ni][ci];
                            char lstr[22]={0};
                            if(lbl&&lbl[0]){
                                int ll=(int)strlen(lbl);
                                if(ll>15){ strncpy(lstr,lbl,14); lstr[14]='.'; lstr[15]='.'; lstr[16]='.'; lstr[17]=0; }
                                else strncpy(lstr,lbl,18);
                            } else { lstr[0]='"'; lstr[1]='"'; lstr[2]=0; }
                            dl->AddText(ImVec2(ctl.x+4.0f,ctl.y+3.0f),IM_COL32(235,210,120,255),lstr);
                            /* Destination indicator (right-aligned) */
                            char dstr[16];
                            if     (dst==-1) snprintf(dstr,sizeof(dstr),"[X]");
                            else if(dst==-2) snprintf(dstr,sizeof(dstr),"[Q]");
                            else if(dst==-3) snprintf(dstr,sizeof(dstr),"[S]");
                            else if(dst==-4) snprintf(dstr,sizeof(dstr),"[SC]");
                            else             snprintf(dstr,sizeof(dstr),"->%d",dst);
                            ImU32 dc=(dst==-1)?IM_COL32(220,100,100,255):
                                     (dst==-2)?IM_COL32(180,120,240,255):
                                     (dst==-3)?IM_COL32( 80,210,210,255):
                                     (dst>=0) ?IM_COL32(120,200,255,255):
                                               IM_COL32(140,140,140,255);
                            /* Right-align the destination string */
                            float dw=(float)strlen(dstr)*7.0f;
                            dl->AddText(ImVec2(cbr.x-dw-4.0f,ctl.y+3.0f),dc,dstr);
                        } else {
                            dl->AddText(ImVec2(ctl.x+4.0f,ctl.y+3.0f),
                                IM_COL32(70,70,80,255),"(no choices)");
                        }
                    }
                } /* show_text */

                /* Input pin (left edge, title-bar middle) */
                ImVec2 ip=ipin_pos(ni);
                dl->AddCircleFilled(ip,PR*s_gzoom,IM_COL32(140,140,220,200));
                dl->AddCircle(ip,PR*s_gzoom,IM_COL32(180,180,255,255),0,1.5f);

                /* Output pins */
                int nc2=state->npc_tree_all_choice_count[ni];
                for(int ci=0;ci<nc2;ci++){
                    ImVec2 op=opin_pos(ni,ci);
                    int dst=state->npc_tree_all_choice_next[ni][ci];
                    ImU32 oc=(dst>=0)?IM_COL32(80,200,100,220):IM_COL32(100,100,115,200);
                    dl->AddCircleFilled(op,PR*s_gzoom,oc);
                    dl->AddCircle(op,PR*s_gzoom,IM_COL32(160,220,160,255),0,1.5f);
                }
            } /* node loop */

            dl->PopClipRect();

            /* ── Mouse interactions (processed after drawing) ── */

            /* Zoom: scroll wheel */
            if(chov&&io.MouseWheel!=0.0f){
                ImVec2 mw=S2W(io.MousePos.x,io.MousePos.y);
                float _z=s_gzoom*(1.0f+io.MouseWheel*0.1f);
                s_gzoom=(_z<0.25f)?0.25f:(_z>3.0f)?3.0f:_z;
                /* Zoom toward cursor */
                s_gpan_x=(io.MousePos.x-cp0.x)/s_gzoom-mw.x;
                s_gpan_y=(io.MousePos.y-cp0.y)/s_gzoom-mw.y;
            }

            /* Pan: middle-drag or Alt+LMB drag */
            if(chov&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle,1.0f)){
                s_gpan_x+=io.MouseDelta.x/s_gzoom;
                s_gpan_y+=io.MouseDelta.y/s_gzoom;
            }
            if(chov&&ImGui::IsMouseDragging(ImGuiMouseButton_Left,1.0f)&&io.KeyAlt){
                s_gpan_x+=io.MouseDelta.x/s_gzoom;
                s_gpan_y+=io.MouseDelta.y/s_gzoom;
            }

            /* LMB click: wire start (selected node's output pins) or node drag / select */
            if(chov&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&!io.KeyAlt){
                ImVec2 mw=S2W(io.MousePos.x,io.MousePos.y);
                s_gdrag_node=-1; s_gwire_node=-1; s_gwire_choice=-1;

                /* Priority 1: output pins of CURRENTLY SELECTED node → wire */
                int sel=state->npc_tree_sel;
                if(sel>=0&&sel<state->npc_tree_count){
                    int nc=state->npc_tree_all_choice_count[sel];
                    float pick_r=(PR*s_gzoom+6.0f);
                    for(int ci=0;ci<nc&&s_gwire_node<0;ci++){
                        ImVec2 op=opin_pos(sel,ci);
                        float dx=io.MousePos.x-op.x, dy=io.MousePos.y-op.y;
                        if(dx*dx+dy*dy<=pick_r*pick_r){
                            s_gwire_node=sel; s_gwire_choice=ci;
                        }
                    }
                }

                /* Priority 2: title bar → node drag + select; body → select */
                if(s_gwire_node<0){
                    for(int ni=state->npc_tree_count-1;ni>=0;ni--){
                        float wx=state->npc_tree_node_canvas_x[ni];
                        float wy=state->npc_tree_node_canvas_y[ni];
                        if(mw.x>=wx&&mw.x<=wx+NW){
                            if(mw.y>=wy&&mw.y<=wy+TH){
                                s_gdrag_node=ni;
                                s_gdrag_ox=mw.x-wx; s_gdrag_oy=mw.y-wy;
                                /* Also select */
                                if(ni!=state->npc_tree_sel&&state->tool_action){
                                    int steps=ni-state->npc_tree_sel;
                                    if(steps>0) for(int s=0;s<steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_NEXT);
                                    else        for(int s=0;s<-steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_PREV);
                                }
                                break;
                            } else if(mw.y>=wy&&mw.y<=wy+node_h(ni)){
                                /* Body click: select only */
                                if(ni!=state->npc_tree_sel&&state->tool_action){
                                    int steps=ni-state->npc_tree_sel;
                                    if(steps>0) for(int s=0;s<steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_NEXT);
                                    else        for(int s=0;s<-steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_PREV);
                                }
                                break;
                            }
                        }
                    }
                }
            }

            /* LMB double-click: enter inline edit mode */
            if(chov&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)&&!io.KeyAlt&&s_gwire_node<0){
                ImVec2 mw=S2W(io.MousePos.x,io.MousePos.y);
                for(int ni=state->npc_tree_count-1;ni>=0;ni--){
                    float wx=state->npc_tree_node_canvas_x[ni];
                    float wy=state->npc_tree_node_canvas_y[ni];
                    if(mw.x>=wx&&mw.x<=wx+NW){
                        /* Navigate to this node first */
                        if(ni!=state->npc_tree_sel&&state->tool_action){
                            int steps=ni-state->npc_tree_sel;
                            if(steps>0) for(int ss=0;ss<steps;ss++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_NEXT);
                            else        for(int ss=0;ss<-steps;ss++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_PREV);
                        }
                        if(mw.y>=wy&&mw.y<wy+TH){
                            /* Double-click title → edit speaker */
                            s_edit_node=ni; s_edit_field=1; s_edit_choice=-1;
                            strncpy(s_edit_buf,state->npc_tree_all_speaker[ni],sizeof(s_edit_buf)-1);
                            s_edit_buf[sizeof(s_edit_buf)-1]=0;
                            s_edit_focused=false;
                        } else if(mw.y>=wy+TH&&mw.y<wy+TH+BH){
                            /* Double-click body → edit dialogue text */
                            s_edit_node=ni; s_edit_field=0; s_edit_choice=-1;
                            strncpy(s_edit_buf,state->npc_tree_all_text[ni],sizeof(s_edit_buf)-1);
                            s_edit_buf[sizeof(s_edit_buf)-1]=0;
                            s_edit_focused=false;
                        } else {
                            /* Double-click choice row → edit choice label */
                            int nc=state->npc_tree_all_choice_count[ni];
                            for(int ci=0;ci<nc;ci++){
                                float cy0=wy+TH+BH+(float)ci*CH;
                                if(mw.y>=cy0&&mw.y<cy0+CH){
                                    s_edit_node=ni; s_edit_field=2; s_edit_choice=ci;
                                    strncpy(s_edit_buf,state->npc_tree_all_choice_labels[ni][ci],sizeof(s_edit_buf)-1);
                                    s_edit_buf[sizeof(s_edit_buf)-1]=0;
                                    s_edit_focused=false;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }

            /* Node drag: update position while LMB held */
            if(s_gdrag_node>=0&&s_gdrag_node<state->npc_tree_count){
                if(ImGui::IsMouseDown(ImGuiMouseButton_Left)&&!io.KeyAlt){
                    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left,1.0f)){
                        ImVec2 mw=S2W(io.MousePos.x,io.MousePos.y);
                        state->npc_tree_node_canvas_x[s_gdrag_node]=mw.x-s_gdrag_ox;
                        state->npc_tree_node_canvas_y[s_gdrag_node]=mw.y-s_gdrag_oy;
                    }
                } else {
                    /* Released → save layout */
                    if(state->tool_action)
                        state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
                    s_gdrag_node=-1;
                }
            }

            /* Wire drag: complete on LMB release over an input pin */
            if(s_gwire_node>=0&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                float pick_r=(PR*s_gzoom+9.0f);
                for(int ni=0;ni<state->npc_tree_count;ni++){
                    if(ni==s_gwire_node) continue;
                    ImVec2 ip=ipin_pos(ni);
                    float dx=io.MousePos.x-ip.x, dy=io.MousePos.y-ip.y;
                    if(dx*dx+dy*dy<=pick_r*pick_r){
                        /* Connect: must be selected node (choice_next[] points into sel node) */
                        if(state->npc_tree_sel==s_gwire_node&&
                           s_gwire_choice<EDITOR_UI_NPC_TREE_CHOICES_MAX&&
                           state->npc_tree_choice_next[s_gwire_choice]){
                            *state->npc_tree_choice_next[s_gwire_choice]=ni;
                            if(state->tool_action)
                                state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
                        }
                        break;
                    }
                }
                s_gwire_node=-1; s_gwire_choice=-1;
            }

            /* RMB context menus */
            if(chov&&ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
                ImVec2 mw=S2W(io.MousePos.x,io.MousePos.y);
                s_grclick_wx=mw.x; s_grclick_wy=mw.y;
                int rni=-1;
                for(int ni=0;ni<state->npc_tree_count;ni++){
                    float wx=state->npc_tree_node_canvas_x[ni];
                    float wy=state->npc_tree_node_canvas_y[ni];
                    if(mw.x>=wx&&mw.x<=wx+NW&&mw.y>=wy&&mw.y<=wy+node_h(ni)){
                        rni=ni; break;
                    }
                }
                if(rni>=0){
                    /* Select the right-clicked node */
                    if(rni!=state->npc_tree_sel&&state->tool_action){
                        int steps=rni-state->npc_tree_sel;
                        if(steps>0) for(int s=0;s<steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_NEXT);
                        else        for(int s=0;s<-steps;s++) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_PREV);
                    }
                    ImGui::OpenPopup("##gnode_ctx");
                } else {
                    ImGui::OpenPopup("##gcanvas_ctx");
                }
            }

            if(ImGui::BeginPopup("##gnode_ctx")){
                if(ImGui::MenuItem("Set as Root")&&state->tool_action)
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_ROOT);
                ImGui::Separator();
                if(ImGui::MenuItem("Add Choice")&&state->tool_action)
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_CHOICE_ADD);
                ImGui::Separator();
                if(ImGui::MenuItem("Delete Node")&&state->tool_action)
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_DELETE);
                ImGui::EndPopup();
            }

            if(ImGui::BeginPopup("##gcanvas_ctx")){
                if(ImGui::MenuItem("Add Node Here")&&state->tool_action){
                    int ni=state->npc_tree_count;
                    if(ni<EDITOR_UI_NPC_TREE_MAX){
                        state->npc_tree_node_canvas_x[ni]=s_grclick_wx;
                        state->npc_tree_node_canvas_y[ni]=s_grclick_wy;
                    }
                    state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_TREE_ADD);
                }
                ImGui::Separator();
                if(ImGui::MenuItem("Auto Layout")) s_g_needs_layout=true;
                ImGui::EndPopup();
            }

        }
        ImGui::End(); /* Dialogue Graph */

        /* ── Inline node edit popup (rendered outside canvas clip rect) ── */
        if(s_edit_node>=0&&s_edit_node<state->npc_tree_count&&
           s_edit_node==state->npc_tree_sel){
            /* Only proceed if the needed pointer is live */
            bool ptr_ok=false;
            if(s_edit_field==0&&state->npc_tree_text) ptr_ok=true;
            if(s_edit_field==1&&state->npc_tree_speaker) ptr_ok=true;
            if(s_edit_field==2&&s_edit_choice>=0&&s_edit_choice<EDITOR_UI_NPC_TREE_CHOICES_MAX&&
               state->npc_tree_choice_labels[s_edit_choice]) ptr_ok=true;

            if(ptr_ok){
                /* Compute screen position using cached canvas origin from this frame */
                float wx=state->npc_tree_node_canvas_x[s_edit_node];
                float wy=state->npc_tree_node_canvas_y[s_edit_node];
                float ex=s_cp0_cache.x+(wx+s_gpan_x)*s_zoom_cache;
                float ey=s_cp0_cache.y+(wy+s_gpan_y)*s_zoom_cache;
                const float TH2=22.0f, BH2=52.0f, CH2=20.0f;
                if(s_edit_field==0)      ey=s_cp0_cache.y+(wy+TH2+s_gpan_y)*s_zoom_cache;
                else if(s_edit_field==1) ey=s_cp0_cache.y+(wy+s_gpan_y)*s_zoom_cache;
                else{
                    float cry=wy+TH2+BH2+(float)s_edit_choice*CH2;
                    ey=s_cp0_cache.y+(cry+s_gpan_y)*s_zoom_cache;
                }
                float eW=210.0f*s_zoom_cache;
                float eH=22.0f;

                ImGui::SetNextWindowPos(ImVec2(ex,ey),ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(eW,eH+4.0f),ImGuiCond_Always);
                ImGuiWindowFlags ef=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
                                    ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoMove|
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;
                ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.08f,0.10f,0.16f,0.97f));
                ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0.50f,0.72f,1.0f,1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(2,2));
                if(ImGui::Begin("##node_inline_edit",nullptr,ef)){
                    ImGui::SetNextItemWidth(eW-6.0f);
                    if(!s_edit_focused){ ImGui::SetKeyboardFocusHere(); s_edit_focused=true; }
                    bool commit=ImGui::InputText("##iebuf",s_edit_buf,(int)sizeof(s_edit_buf),
                                                 ImGuiInputTextFlags_EnterReturnsTrue|
                                                 ImGuiInputTextFlags_AutoSelectAll);
                    bool lost=s_edit_focused&&!ImGui::IsItemActive()&&!ImGui::IsWindowFocused();
                    if(commit||lost){
                        if(s_edit_field==0&&state->npc_tree_text){
                            int cap=state->npc_tree_text_capacity-1; if(cap<0)cap=0;
                            strncpy(state->npc_tree_text,s_edit_buf,(size_t)cap);
                            state->npc_tree_text[cap]=0;
                        } else if(s_edit_field==1&&state->npc_tree_speaker){
                            int cap=state->npc_tree_speaker_capacity-1; if(cap<0)cap=0;
                            strncpy(state->npc_tree_speaker,s_edit_buf,(size_t)cap);
                            state->npc_tree_speaker[cap]=0;
                        } else if(s_edit_field==2&&s_edit_choice>=0&&
                                  s_edit_choice<EDITOR_UI_NPC_TREE_CHOICES_MAX&&
                                  state->npc_tree_choice_labels[s_edit_choice]){
                            strncpy(state->npc_tree_choice_labels[s_edit_choice],s_edit_buf,63);
                            state->npc_tree_choice_labels[s_edit_choice][63]=0;
                        }
                        if(state->tool_action)
                            state->tool_action(state->action_ctx,EDITOR_WF_ACTION_NPC_SAVE);
                        s_edit_node=-1; s_edit_focused=false;
                    }
                    if(ImGui::IsKeyPressed(ImGuiKey_Escape)){
                        s_edit_node=-1; s_edit_focused=false;
                    }
                }
                ImGui::End();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
            } else {
                /* Pointer not yet live (1-frame lag after select) — wait */
            }
        }
    }
    /* ════════════════════════════════════════════════════════════════ */

    if(state->spawn_edit_mode&&state->spawn_has_selection){
        static bool s_show_spawn_insp=true;
        ImGui::SetNextWindowPos(ImVec2((float)win_w-320.0f,8.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(310.0f,380.0f),ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Enemy Spawn Inspector",&s_show_spawn_insp)){
            any_primary_open=true;
            ImGui::TextColored(ImVec4(1.0f,0.6f,0.2f,1.0f),"Type: %s",
                state->spawn_sel_type_name?state->spawn_sel_type_name:"Enemy");
            /* ── Model dropdown ── */
            ImGui::Separator();
            ImGui::TextDisabled("Model");
            if(state->spawn_sel_model_idx){
                int cur=*state->spawn_sel_model_idx;
                const char *cur_label=(cur<=0||state->spawn_model_count==0)?"Default":
                    (cur-1<state->spawn_model_count&&state->spawn_model_names[cur-1]?
                     state->spawn_model_names[cur-1]:"Default");
                ImGui::SetNextItemWidth(-1);
                if(ImGui::BeginCombo("##spawnmodel",cur_label)){
                    bool def_sel=(cur<=0);
                    if(ImGui::Selectable("Default",def_sel)){
                        if(state->spawn_set_model_idx) state->spawn_set_model_idx(state->action_ctx,0);
                    }
                    if(def_sel) ImGui::SetItemDefaultFocus();
                    for(int _mi=0;_mi<state->spawn_model_count;_mi++){
                        bool sel=(cur==_mi+1);
                        const char *nm=state->spawn_model_names[_mi]?state->spawn_model_names[_mi]:"?";
                        if(ImGui::Selectable(nm,sel)){
                            if(state->spawn_set_model_idx) state->spawn_set_model_idx(state->action_ctx,_mi+1);
                        }
                        if(sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if(state->spawn_model_count==0)
                    ImGui::TextDisabled("(drop .fbx/.glb in models/enemy/ to add models)");
            }
            /* ── Per-model visual scale ── */
            if(state->spawn_model_scale){
                ImGui::SetNextItemWidth(-1);
                {
                    float value=*state->spawn_model_scale;
                    if(state->spawn_set_model_scale&&ImGui::SliderFloat("Model Scale##sms",&value,0.01f,5.0f,"%.3f"))
                        state->spawn_set_model_scale(state->action_ctx,value);
                }
                ImGui::TextDisabled("Applies to all spawns using this model");
            }
            /* ── Stat overrides ── */
            ImGui::Separator();
            ImGui::TextDisabled("Stat overrides  (-1 = type default)");
            float hw=ImGui::GetContentRegionAvail().x*0.5f-4.0f;
            if(state->spawn_sel_hp){
                char hint[32]; snprintf(hint,sizeof(hint),"def %.0f",state->spawn_def_hp);
                ImGui::SetNextItemWidth(hw);
                {
                    float value=*state->spawn_sel_hp;
                    if(state->spawn_set_hp_override&&ImGui::InputFloat("HP##shp",&value,0,0,"%.1f"))
                        state->spawn_set_hp_override(state->action_ctx,value);
                }
                ImGui::SameLine(0,4); ImGui::TextDisabled("%s",hint);
            }
            if(state->spawn_sel_dmg){
                char hint[32]; snprintf(hint,sizeof(hint),"def %.0f",state->spawn_def_dmg);
                ImGui::SetNextItemWidth(hw);
                {
                    float value=*state->spawn_sel_dmg;
                    if(state->spawn_set_damage_override&&ImGui::InputFloat("Damage##sdmg",&value,0,0,"%.1f"))
                        state->spawn_set_damage_override(state->action_ctx,value);
                }
                ImGui::SameLine(0,4); ImGui::TextDisabled("%s",hint);
            }
            if(state->spawn_sel_speed){
                char hint[32]; snprintf(hint,sizeof(hint),"def %.2f",state->spawn_def_speed);
                ImGui::SetNextItemWidth(hw);
                {
                    float value=*state->spawn_sel_speed;
                    if(state->spawn_set_speed_override&&ImGui::InputFloat("Speed##sspd",&value,0,0,"%.2f"))
                        state->spawn_set_speed_override(state->action_ctx,value);
                }
                ImGui::SameLine(0,4); ImGui::TextDisabled("%s",hint);
            }
            if(state->spawn_sel_chase_r){
                char hint[32]; snprintf(hint,sizeof(hint),"def %.1f",state->spawn_def_chase_r);
                ImGui::SetNextItemWidth(hw);
                {
                    float value=*state->spawn_sel_chase_r;
                    if(state->spawn_set_chase_radius_override&&ImGui::InputFloat("Chase R##scr",&value,0,0,"%.1f"))
                        state->spawn_set_chase_radius_override(state->action_ctx,value);
                }
                ImGui::SameLine(0,4); ImGui::TextDisabled("%s",hint);
            }
            if(state->spawn_sel_atk_r){
                char hint[32]; snprintf(hint,sizeof(hint),"def %.1f",state->spawn_def_atk_r);
                ImGui::SetNextItemWidth(hw);
                {
                    float value=*state->spawn_sel_atk_r;
                    if(state->spawn_set_attack_radius_override&&ImGui::InputFloat("Atk R##sar",&value,0,0,"%.1f"))
                        state->spawn_set_attack_radius_override(state->action_ctx,value);
                }
                ImGui::SameLine(0,4); ImGui::TextDisabled("%s",hint);
            }
            if(state->spawn_sel_atk_cd){
                char hint[32]; snprintf(hint,sizeof(hint),"def %.2f",state->spawn_def_atk_cd);
                ImGui::SetNextItemWidth(hw);
                {
                    float value=*state->spawn_sel_atk_cd;
                    if(state->spawn_set_attack_cooldown_override&&ImGui::InputFloat("Atk CD##sacd",&value,0,0,"%.2f"))
                        state->spawn_set_attack_cooldown_override(state->action_ctx,value);
                }
                ImGui::SameLine(0,4); ImGui::TextDisabled("%s",hint);
            }
            if(state->spawn_sel_scale){
                char hint[32]; snprintf(hint,sizeof(hint),"def %.2f",state->spawn_def_scale);
                ImGui::SetNextItemWidth(hw);
                {
                    float value=*state->spawn_sel_scale;
                    if(state->spawn_set_scale_override&&ImGui::InputFloat("Scale##ssc",&value,0,0,"%.2f"))
                        state->spawn_set_scale_override(state->action_ctx,value);
                }
                ImGui::SameLine(0,4); ImGui::TextDisabled("%s",hint);
            }
            ImGui::Separator();
            if(ImGui::Button("Save Spawns")&&state->tool_action)
                state->tool_action(state->action_ctx,EDITOR_WF_ACTION_SPAWN_SAVE);
            ImGui::SameLine(); ImGui::TextDisabled("stat overrides apply on next load");
        }
        ImGui::End();
    }

    if(state->quest_edit_mode){
        static bool s_show_quest=true;
        ImGui::SetNextWindowPos(ImVec2((float)win_w-520.0f,8.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(510.0f,(float)win_h-16.0f),ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Quest Editor",&s_show_quest)){
            any_primary_open=true;
            /* ── Quest list header ── */
            ImGui::TextDisabled("Quests (%d / 8)",state->quest_count);
            ImGui::SameLine();
            if(ImGui::SmallButton("+##qa")&&state->tool_action) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_QUEST_ADD);
            ImGui::SameLine();
            if(ImGui::SmallButton("Del##qd")&&state->tool_action) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_QUEST_DELETE);
            ImGui::SameLine();
            if(ImGui::SmallButton("Save")&&state->tool_action) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_QUEST_SAVE);
            /* ── Quest selector ── */
            draw_tool_pair(state,"< Quest",EDITOR_WF_ACTION_QUEST_PREV,"Quest >",EDITOR_WF_ACTION_QUEST_NEXT);
            if(state->quest_count>0){
                ImGui::TextColored(ImVec4(0.9f,0.75f,0.2f,1),"Quest %d / %d",
                    state->quest_sel+1,state->quest_count);
                ImGui::Separator();
                /* Name */
                if(state->quest_name_buf&&state->quest_name_buf_cap>0){
                    ImGui::SetNextItemWidth(-1);
                    if(ImGui::InputText("##qname",state->quest_name_buf,(size_t)state->quest_name_buf_cap)&&state->tool_action){
                        /* Sync name back to quest */
                        state->tool_action(state->action_ctx,EDITOR_WF_ACTION_QUEST_SAVE);
                    }
                    ImGui::SameLine(0,0); ImGui::TextDisabled(" Name");
                }
                /* Description */
                if(state->quest_desc_buf&&state->quest_desc_buf_cap>0){
                    if(ImGui::InputTextMultiline("##qdesc",state->quest_desc_buf,(size_t)state->quest_desc_buf_cap,ImVec2(-1,48))&&state->tool_action){
                        state->tool_action(state->action_ctx,EDITOR_WF_ACTION_QUEST_SAVE);
                    }
                }
                /* Gold and XP */
                float hw=(ImGui::GetContentRegionAvail().x-4)*0.5f;
                if(state->quest_gold_ptr){
                    ImGui::SetNextItemWidth(hw);
                    ImGui::InputInt("##qgold",state->quest_gold_ptr);
                    ImGui::SameLine(0,4);
                }
                if(state->quest_xp_ptr){
                    ImGui::SetNextItemWidth(hw);
                    ImGui::InputInt("##qxp",state->quest_xp_ptr);
                    ImGui::SameLine(0,0); ImGui::TextDisabled(" XP");
                }
                if(state->quest_gold_ptr) ImGui::TextDisabled("Gold / XP rewards");
                ImGui::Separator();
                /* ── Objectives ── */
                ImGui::TextDisabled("Objectives (%d / 4)",state->quest_obj_count);
                ImGui::SameLine();
                if(ImGui::SmallButton("+Kill")&&state->tool_action) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_QUEST_OBJ_ADD_KILL);
                ImGui::SameLine();
                if(ImGui::SmallButton("+Reach")&&state->tool_action) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_QUEST_OBJ_ADD_REACH);
                ImGui::SameLine();
                if(ImGui::SmallButton("Del##od")&&state->tool_action) state->tool_action(state->action_ctx,EDITOR_WF_ACTION_QUEST_OBJ_DELETE);
                draw_tool_pair(state,"< Obj",EDITOR_WF_ACTION_QUEST_OBJ_PREV,"Obj >",EDITOR_WF_ACTION_QUEST_OBJ_NEXT);
                if(state->quest_obj_count>0){
                    ImGui::TextColored(ImVec4(0.5f,0.9f,0.5f,1),"Obj %d / %d  (%s)",
                        state->quest_obj_sel+1,state->quest_obj_count,
                        state->quest_obj_type==0?"Kill":"Reach");
                    /* Objective description */
                    if(state->quest_obj_desc_buf&&state->quest_obj_desc_buf_cap>0){
                        ImGui::SetNextItemWidth(-1);
                        if(ImGui::InputText("##odesc",state->quest_obj_desc_buf,(size_t)state->quest_obj_desc_buf_cap)&&state->tool_action)
                            state->tool_action(state->action_ctx,EDITOR_WF_ACTION_QUEST_SAVE);
                        ImGui::SameLine(0,0); ImGui::TextDisabled(" Desc");
                    }
                    if(state->quest_obj_type==0){
                        /* Kill objective */
                        if(state->quest_obj_required_ptr){
                            ImGui::SetNextItemWidth(80);
                            ImGui::InputInt("Required##oq",state->quest_obj_required_ptr);
                        }
                        if(state->quest_obj_etype_ptr){
                            ImGui::SetNextItemWidth(80);
                            ImGui::InputInt("Enemy Type##oe",state->quest_obj_etype_ptr);
                            ImGui::SameLine(); ImGui::TextDisabled("-1=any");
                        }
                    }else{
                        /* Reach objective */
                        ImGui::Text("Loc: %.1f  %.1f  %.1f  R: %.1f",
                            state->quest_obj_loc_x,state->quest_obj_loc_y,
                            state->quest_obj_loc_z,state->quest_obj_radius);
                        ImGui::TextDisabled("(Place a reach marker at current camera position)");
                        if(state->quest_obj_required_ptr){
                            ImGui::SetNextItemWidth(80);
                            ImGui::InputInt("Radius##orad",(int*)&state->quest_obj_radius);
                        }
                    }
                }
            }else{
                ImGui::TextDisabled("No quests yet. Click + to add one.");
            }
        }
        ImGui::End();
    }

    if(g_show_help){
        ImGui::SetNextWindowPos(ImVec2(296.0f,(float)win_h-56.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2((float)win_w-592.0f,48.0f),ImGuiCond_FirstUseEver);
        ImGuiWindowFlags flags=ImGuiWindowFlags_NoResize;
        if(ImGui::Begin("Editor Help",&g_show_help,flags)){
            any_primary_open=true;
            ImGui::TextDisabled("Esc Exit Editor | LMB Select or Place | RMB Remove or Back | Middle drag pan | Scroll zoom");
        }
        ImGui::End();
    }
    if(!any_primary_open){
        ImGui::SetNextWindowPos(ImVec2(8.0f,8.0f),ImGuiCond_FirstUseEver);
        ImGuiWindowFlags flags=ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoCollapse;
        if(ImGui::Begin("Editor Toggle",nullptr,flags)){
            if(ImGui::Button("Open Editor UI")){
                g_show_world_forge=true;
                g_show_workflow=true;
                g_show_inspector=true;
                g_show_outliner=true;
                g_show_prop_window=true;
                g_show_npc_window=true;
                g_show_help=true;
            }
        }
        ImGui::End();
    }
    if(!g_raw_mouse_down[0]) g_shell_button_raw_id=0;
}
