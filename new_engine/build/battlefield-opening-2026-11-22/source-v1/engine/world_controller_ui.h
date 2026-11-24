#ifndef VALWOOD_WORLD_CONTROLLER_UI_H
#define VALWOOD_WORLD_CONTROLLER_UI_H
#include "third_party/imgui/imgui.h"
#include <string>
namespace world_controller {
inline std::string button_label(const char *text){
    std::string label=text;const auto bracket=label.rfind('[');
    if(bracket!=std::string::npos){
        const auto suffix=label.substr(bracket);
        if(suffix=="[ENTER]"||suffix=="[ESC]"||suffix=="[J]"||suffix=="[F7]")label.erase(bracket);
    }
    while(!label.empty()&&label.back()==' ')label.pop_back();
    label+="###";label+=text;return label;
}
inline void default_focus(bool &pending){
    if(!pending)return;
    ImGui::SetItemDefaultFocus();ImGui::SetKeyboardFocusHere(-1);
    // SetKeyboardFocusHere intentionally keeps the navigation cursor hidden.
    // After gameplay disabled navigation, that makes the first confirm inert.
    ImGui::SetNavCursorVisible(true);pending=false;
}
}
#endif
