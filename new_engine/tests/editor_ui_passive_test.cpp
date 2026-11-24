// Exercise the production C bridge and ImGui wrapper with real ImGui input,
// font building and frame lifecycle. Only SDL/GL platform entry points are
// replaced: this is a deterministic CPU correctness test, not a GPU benchmark.
extern "C" {
#include "engine/editor_ui.h"
}
#include "engine/editor_ui_imgui.h"
#include "engine/editor_ui_imgui_submission.h"
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_internal.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h"
#include "third_party/imgui/backends/imgui_impl_opengl3.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>

namespace {
unsigned sdl_frames, gl_frames, submissions, mouse_polls, capture_releases;
unsigned text_starts, text_stops, processed_events, font_builds;
unsigned rendered_frames,skipped_frames,texture_requests,callbacks;
Uint32 mouse_buttons;
float platform_dt = .025f;
bool text_active,reference_mode,x11_focus=false;
unsigned global_mouse_polls=0;

void render_checked(const EditorUIState &ui,bool expected_backend) {
    const auto previous=submissions;
    editor_ui_render(&ui);
    assert(submissions==previous+unsigned(expected_backend));
    rendered_frames++;skipped_frames+=!expected_backend;
}
void render_empty(const EditorUIState &ui,bool passive=true) {
    render_checked(ui,reference_mode||!passive);
    const ImDrawData *draw = ImGui::GetDrawData();
    assert(draw && draw->Valid && draw->CmdListsCount == 0);
    assert(world_imgui_pending_texture_work(draw)==0);
    assert(world_imgui_can_skip_empty_backend(!reference_mode,passive,draw)==(!reference_mode&&passive));
}
void callback(const ImDrawList *,const ImDrawCmd *) {callbacks++;}

void key_event(const EditorUIState &ui, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.keysym.sym = SDLK_a;
    assert(editor_ui_process_event(&ui, &event));
}

void text_window(const EditorUIState &ui, bool focus) {
    static char value[64] = "Valwood";
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_Always);
    ImGui::Begin("Passive frame fixture", nullptr, ImGuiWindowFlags_NoSavedSettings);
    if (focus)
        ImGui::SetKeyboardFocusHere();
    ImGui::InputText("Name", value, sizeof(value));
    ImGui::End();
    render_checked(ui,true);
}
}

bool ImGui_ImplSDL2_InitForOpenGL(SDL_Window *, void *) { return true; }
void ImGui_ImplSDL2_Shutdown() {}
void ImGui_ImplSDL2_NewFrame() {
    ++sdl_frames;
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(640, 360);
    io.DisplayFramebufferScale = ImVec2(2, 2);
    io.DeltaTime = platform_dt;
}
bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event *event) {
    ++processed_events;
    ImGuiIO &io = ImGui::GetIO();
    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        assert(event->key.keysym.sym == SDLK_a);
        io.AddKeyEvent(ImGuiKey_A, event->type == SDL_KEYDOWN);
    } else if (event->type == SDL_MOUSEBUTTONUP || event->type == SDL_MOUSEBUTTONDOWN) {
        assert(event->button.button == SDL_BUTTON_LEFT);
        io.AddMouseButtonEvent(0, event->type == SDL_MOUSEBUTTONDOWN);
    } else if (event->type == SDL_TEXTINPUT) {
        io.AddInputCharactersUTF8(event->text.text);
    }
    return true;
}
bool ImGui_ImplOpenGL3_Init(const char *) { return true; }
void ImGui_ImplOpenGL3_Shutdown() {}
void ImGui_ImplOpenGL3_NewFrame() {
    ++gl_frames;
    ImFontAtlas *fonts = ImGui::GetIO().Fonts;
    if (!fonts->IsBuilt()) {
        unsigned char *pixels = nullptr;
        int width = 0, height = 0;
        fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        assert(pixels && width > 0 && height > 0);
        fonts->SetTexID(ImTextureID(1));
        // The mock font upload is complete; real backend requests are tested
        // separately below with an actual externally registered texture.
        fonts->TexData->SetStatus(ImTextureStatus_OK);
        ++font_builds;
    }
}
void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData *draw) {
    assert(draw && draw->Valid);
    ++submissions;
    if(draw->Textures)for(auto *texture:*draw->Textures) {
        assert(texture);
        if(texture->Status==ImTextureStatus_WantCreate||texture->Status==ImTextureStatus_WantUpdates) {
            texture->SetTexID(ImTextureID(2));texture->SetStatus(ImTextureStatus_OK);texture_requests++;
        } else if(texture->Status==ImTextureStatus_WantDestroy) {
            texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);texture_requests++;
        }
    }
    for(const auto *list:draw->CmdLists)for(const auto &command:list->CmdBuffer)
        if(command.UserCallback&&command.UserCallback!=ImDrawCallback_ResetRenderState)
            command.UserCallback(list,&command);
}
extern "C" Uint32 SDL_GetMouseState(int *x, int *y) {
    ++mouse_polls;
    *x = 0;
    *y = 0;
    return mouse_buttons;
}
extern "C" SDL_Window *SDL_GetKeyboardFocus() { return x11_focus?reinterpret_cast<SDL_Window *>(1):nullptr; }
extern "C" const char *SDL_GetCurrentVideoDriver() { return x11_focus?"x11":"mock"; }
extern "C" SDL_bool SDL_GetRelativeMouseMode() { return SDL_FALSE; }
extern "C" Uint32 SDL_GetGlobalMouseState(int *x,int *y) { ++global_mouse_polls;*x=1032;*y=588;return mouse_buttons; }
extern "C" void SDL_GetWindowPosition(SDL_Window *,int *x,int *y) { *x=400;*y=200; }
extern "C" int SDL_CaptureMouse(SDL_bool enabled) {
    assert(enabled == SDL_FALSE);
    ++capture_releases;
    return 0;
}
extern "C" void SDL_StartTextInput() {
    assert(!text_active);
    text_active = true;
    ++text_starts;
}
extern "C" void SDL_StopTextInput() {
    assert(text_active);
    text_active = false;
    ++text_stops;
}

void run_mode(bool reference,const char *disabled,const char *positive,const char *policy) {
    reference_mode=reference;
    if(disabled)assert(setenv("VALWOOD_QA_DISABLE_EMPTY_UI_FAST_PATH",disabled,1)==0);
    else assert(unsetenv("VALWOOD_QA_DISABLE_EMPTY_UI_FAST_PATH")==0);
    if(positive)assert(setenv("VALWOOD_QA_EMPTY_UI_FAST_PATH",positive,1)==0);
    else assert(unsetenv("VALWOOD_QA_EMPTY_UI_FAST_PATH")==0);
    sdl_frames=gl_frames=submissions=mouse_polls=capture_releases=0;
    text_starts=text_stops=processed_events=font_builds=0;
    rendered_frames=skipped_frames=texture_requests=callbacks=0;
    mouse_buttons=0;platform_dt=.025f;text_active=false;
    EditorUIState ui{};
    ui.backend = EDITOR_UI_BACKEND_IMGUI;
    assert(editor_ui_startup(&ui, nullptr, nullptr));
    ImGui::GetIO().IniFilename = nullptr; // No repository settings writes.

    // Cold hidden startup must build fonts and balance NewFrame/Render without
    // touching the SDL frame backend or raw mouse polling.
    editor_ui_begin_passive_frame(&ui, .02f, 1280, 720);
    assert(sdl_frames == 0 && mouse_polls == 0 && gl_frames == 1 && font_builds == 1);
    assert(capture_releases == 1);
    assert(std::abs(ImGui::GetIO().DeltaTime - .02f) < .000001f);
    assert(ImGui::GetIO().DisplaySize.x == 1280 && ImGui::GetIO().DisplaySize.y == 720);
    assert(ImGui::GetIO().DisplayFramebufferScale.x == 1);
    render_empty(ui);
    // Configuration is latched at initialization, so a changed environment
    // cannot change submission policy midway through an input/frame sequence.
    if(reference) {
        assert(unsetenv("VALWOOD_QA_DISABLE_EMPTY_UI_FAST_PATH")==0);
        assert(setenv("VALWOOD_QA_EMPTY_UI_FAST_PATH","1",1)==0);
    } else {
        assert(setenv("VALWOOD_QA_DISABLE_EMPTY_UI_FAST_PATH","1",1)==0);
        assert(setenv("VALWOOD_QA_EMPTY_UI_FAST_PATH","0",1)==0);
    }

    // Real input queues are drained, with no unbounded hidden-frame backlog.
    // One edge per frame intentionally matches ImGui's input-trickle policy.
    for (unsigned frame = 0; frame < 400; ++frame) {
        const bool down = (frame % 2) == 0;
        key_event(ui, down);
        editor_ui_begin_passive_frame(&ui, 1.f / 60.f, 1280, 720);
        assert(ImGui::IsKeyDown(ImGuiKey_A) == down);
        assert(ImGui::GetCurrentContext()->InputEventsQueue.Size <= 1);
        render_empty(ui);
    }
    assert(processed_events == 400 && sdl_frames == 0 && mouse_polls == 0);
    assert(capture_releases == 1 && gl_frames == 401);
    assert(submissions == (reference?401u:0u)&&skipped_frames==(reference?0u:401u));

    // Invalid timing/dimensions are sanitized by the production API.
    editor_ui_begin_passive_frame(&ui, std::numeric_limits<float>::quiet_NaN(), 0, -2);
    assert(std::abs(ImGui::GetIO().DeltaTime - 1.f / 60.f) < .000001f);
    assert(ImGui::GetIO().DisplaySize.x == 1 && ImGui::GetIO().DisplaySize.y == 1);
    render_empty(ui);
    editor_ui_begin_passive_frame(&ui, 10.f, 1280, 720);
    assert(ImGui::GetIO().DeltaTime <= .05f);
    render_empty(ui);

    // A long inactive platform clock is neutralized for exactly the first
    // resumed UI frame; subsequent frames use the backend's normal timing.
    platform_dt = 8.f;
    editor_ui_begin_frame(&ui);
    assert(std::abs(ImGui::GetIO().DeltaTime - 1.f / 60.f) < .000001f);
    assert(ImGui::GetIO().DisplaySize.x == 640 && ImGui::GetIO().DisplayFramebufferScale.x == 2);
    text_window(ui, true);
    platform_dt = .025f;
    for (unsigned i = 0; i < 6 && !text_active; ++i) {
        editor_ui_begin_frame(&ui);
        assert(std::abs(ImGui::GetIO().DeltaTime - platform_dt) < .000001f);
        text_window(ui, i == 0);
    }
    assert(text_active && text_starts == 1 && ImGui::GetIO().WantTextInput);

    const unsigned before_sdl = sdl_frames, before_mouse = mouse_polls;
    const unsigned before_capture = capture_releases;
    // Removing the real active InputText naturally retires text capture. SDL
    // text input must stop, even though the next UI frames have no windows.
    for (unsigned i = 0; i < 4; ++i) {
        editor_ui_begin_passive_frame(&ui, .02f, 1280, 720);
        render_empty(ui);
    }
    assert(!ImGui::GetIO().WantTextInput && !text_active && text_stops == 1);
    assert(sdl_frames == before_sdl && mouse_polls == before_mouse);
    assert(capture_releases == before_capture + 1);

    // Mouse release events continue through a hidden frame and reconcile with
    // the wrapper's next raw poll; no phantom click appears on UI resume.
    editor_ui_begin_frame(&ui);
    render_empty(ui,false);
    mouse_buttons = SDL_BUTTON(SDL_BUTTON_LEFT);
    editor_ui_begin_frame(&ui);
    assert(ImGui::IsMouseDown(0));
    render_empty(ui,false);
    mouse_buttons = 0;
    SDL_Event released{};
    released.type = SDL_MOUSEBUTTONUP;
    released.button.button = SDL_BUTTON_LEFT;
    assert(editor_ui_process_event(&ui, &released));
    editor_ui_begin_passive_frame(&ui, .02f, 1280, 720);
    assert(!ImGui::IsMouseDown(0));
    render_empty(ui);
    editor_ui_begin_frame(&ui);
    assert(!ImGui::IsMouseDown(0) && !ImGui::IsMouseClicked(0));
    text_window(ui, false);
    assert(gl_frames==rendered_frames&&submissions+skipped_frames==rendered_frames&&mouse_polls==sdl_frames);

    // Passive does not mean empty. A callback-only list has no vertices or
    // indices but must still execute through the full backend in both modes.
    editor_ui_begin_passive_frame(&ui,.02f,1280,720);
    ImGui::GetForegroundDrawList()->AddCallback(callback,nullptr);
    const auto before_callback=callbacks;render_checked(ui,true);
    const auto *callback_draw=ImGui::GetDrawData();
    assert(callback_draw->CmdListsCount>0&&callback_draw->TotalVtxCount==0&&callback_draw->TotalIdxCount==0);
    assert(callbacks==before_callback+1&&!world_imgui_can_skip_empty_backend(true,true,callback_draw));
    editor_ui_begin_passive_frame(&ui,.02f,1280,720);
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(10,10),ImVec2(50,60),IM_COL32_WHITE);
    render_checked(ui,true);
    assert(ImGui::GetDrawData()->TotalVtxCount>0&&ImGui::GetDrawData()->TotalIdxCount>0);

    // Real pending texture work is part of RenderDrawData even for empty UI.
    // Register a CPU texture with ImGui so EndFrame supplies the actual public
    // texture list to the production guard and the counted backend below.
    {
        ImTextureData texture;texture.Create(ImTextureFormat_RGBA32,2,2);
        ImGui::RegisterUserTexture(&texture);
        for(auto status:{ImTextureStatus_WantCreate,ImTextureStatus_WantUpdates,ImTextureStatus_WantDestroy}) {
            editor_ui_begin_passive_frame(&ui,.02f,1280,720);
            texture.WantDestroyNextFrame=status==ImTextureStatus_WantDestroy;
            texture.SetStatus(status);const auto before_requests=texture_requests;
            render_checked(ui,true);
            assert(ImGui::GetDrawData()->CmdListsCount==0&&texture_requests==before_requests+1);
            assert(texture.Status==(status==ImTextureStatus_WantDestroy?ImTextureStatus_Destroyed:ImTextureStatus_OK));
        }
        ImGui::UnregisterUserTexture(&texture);
    }
    editor_ui_begin_passive_frame(&ui,.02f,1280,720);render_empty(ui);
    assert(gl_frames==rendered_frames&&submissions+skipped_frames==rendered_frames);
    assert(reference?skipped_frames==0:skipped_frames>=405);
    assert(!text_active);

    // The native X11 fallback must convert desktop coordinates to the focused
    // client, while passive walking never queries the global pointer.
    const unsigned global_before=global_mouse_polls;x11_focus=true;
    editor_ui_begin_passive_frame(&ui,.02f,1280,720);render_empty(ui);
    assert(global_mouse_polls==global_before);
    editor_ui_begin_frame(&ui);
    assert(global_mouse_polls==global_before+1);
    assert(ImGui::GetIO().MousePos.x==632&&ImGui::GetIO().MousePos.y==388);
    render_empty(ui,false);x11_focus=false;

    // HUD/no-backend and null C bridge calls remain harmless no-ops.
    const unsigned calls = gl_frames;
    EditorUIState hud{};
    hud.backend = EDITOR_UI_BACKEND_HUD;
    editor_ui_begin_passive_frame(&hud, .02f, 1280, 720);
    editor_ui_begin_passive_frame(nullptr, .02f, 1280, 720);
    assert(gl_frames == calls);
    editor_ui_shutdown(&ui);
    assert(!ImGui::GetCurrentContext());
    std::printf("PASS production passive UI lifecycle (%s): %u queued events, %u frames/%u submissions/%u empty skips; callbacks and texture requests retained; SDL/raw polls only on %u visible frames, capture released once per transition.\n",
        policy,processed_events,rendered_frames,submissions,skipped_frames,sdl_frames);
}

int main() {
    assert(unsetenv("VALWOOD_UI_TRACE_PATH")==0); // No trace/settings writes from this CPU fixture.
    run_mode(false,nullptr,nullptr,"default empty fast path");
    run_mode(true,"1",nullptr,"explicit full-backend reference");
    run_mode(true,"1","1","disable overrides positive QA flag");
    run_mode(false,"0","0","zero QA values retain default fast path");
    run_mode(false,"","1","empty disable and redundant positive QA flag");
    assert(unsetenv("VALWOOD_QA_DISABLE_EMPTY_UI_FAST_PATH")==0);
    assert(unsetenv("VALWOOD_QA_EMPTY_UI_FAST_PATH")==0);
}
