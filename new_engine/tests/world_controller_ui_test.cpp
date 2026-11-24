#include "engine/world_controller_ui.h"
#include <cstdio>
#include <string>
// Headless Dear ImGui regression for a real failure found in the rendered
// controller test: returning from gameplay left a focused but inert button.
int main(){
    if(world_controller::button_label("RESUME     [ESC]")!="RESUME###RESUME     [ESC]"||
       world_controller::button_label("Take the [iron] shield")!="Take the [iron] shield###Take the [iron] shield"||
       world_controller::button_label("Choose [strength]")!="Choose [strength]###Choose [strength]")return 1;
    ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;
    io.DisplaySize={1280,720};io.DeltaTime=.02f;io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    unsigned char *pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    int screen=0,showcases=0,journeys=0;std::string panel;
    for(int frame=0;frame<120;++frame){
        io.ConfigFlags=screen==0?ImGuiConfigFlags_NavEnableGamepad:0;
        if(frame==10)io.AddKeyEvent(ImGuiKey_GamepadDpadDown,true);
        if(frame==13)io.AddKeyEvent(ImGuiKey_GamepadDpadDown,false);
        if(frame==30||frame==80)io.AddKeyEvent(ImGuiKey_GamepadFaceDown,true);
        if(frame==33||frame==83)io.AddKeyEvent(ImGuiKey_GamepadFaceDown,false);
        if(frame==60)screen=0;
        ImGui::NewFrame();
        if(screen==0){
            const char *name="###title";bool pending=panel!=name;
            if(pending){ImGui::SetNextWindowFocus();panel=name;}
            ImGui::SetNextWindowPos({70,330});ImGui::SetNextWindowSize({430,0});
            ImGui::Begin(name,nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize);
            auto button=[&](const char *text){bool selected=ImGui::Button(text,{-1,42});world_controller::default_focus(pending);return selected;};
            if(button("Begin")){screen=2;++journeys;}
            ImGui::BeginDisabled();button("Continue");ImGui::EndDisabled();
            if(button("Showcase")){screen=1;++showcases;}
            button("Quit");ImGui::End();
        }else{panel.clear();ImGui::Begin("Review");ImGui::Button("Back");ImGui::End();}
        ImGui::Render();
    }
    ImGui::DestroyContext();
    if(showcases!=1||journeys!=1||screen!=2){std::fprintf(stderr,"Controller UI focus failed: showcase=%d journey=%d screen=%d\n",showcases,journeys,screen);return 1;}
    std::puts("world_controller_ui: disabled-option navigation, focused confirmation, gameplay return and first-press activation passed");
}
