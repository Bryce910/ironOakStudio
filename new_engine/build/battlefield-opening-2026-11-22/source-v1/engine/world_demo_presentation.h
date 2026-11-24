#ifndef VALWOOD_DEMO_PRESENTATION_H
#define VALWOOD_DEMO_PRESENTATION_H
#include <algorithm>
#include <cmath>
#include <string>
#include "third_party/imgui/imgui.h"

namespace world_demo_presentation {
inline const char *known_issues(bool battlefield=false){
    if(battlefield){
        static const std::string issues=std::string("The battlefield background fighting is staged. The three soldiers on your escape route use the live combat system. Paired tackle/disarm animations are still being polished.\n\n")+known_issues(false);
        return issues.c_str();
    }
    return
    "The three camp travelers use authored skeletal idles. Legacy town and distant NPC idles are still limited.\n\n"
    "Some distant tree silhouettes repeat. The camp now includes more tree forms and source-baked far cards.\n\n"
    "Camp props and ground detail have been improved; some close-up geometry and shoreline transitions still need art polish.\n\n"
    "Performance depends heavily on graphics settings and hardware. Dense foliage remains expensive on the 3 GB RX 5300; use Performance or Balanced settings for playtesting.\n\n"
    "This is a work-in-progress playtest, with unfinished character animation and rendering.";
}
inline void centered(ImDrawList *dl,float w,float y,float size,ImU32 color,const char *s){
    const auto e=ImGui::GetFont()->CalcTextSizeA(size,10000,0,s);
    dl->AddText(ImGui::GetFont(),size,{(w-e.x)*.5f,y},color,s);
}
// A location plate, not a fabricated percentage. Startup stages are real
// engine milestones, and model loading calls redraw at bounded intervals.
inline void loading(ImTextureID logo,const char *title,const char *phase,bool outdoors,double seconds,bool battlefield=false){
    const auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float u=std::max(.5f,std::min(w/1600.f,h/900.f));auto *d=ImGui::GetForegroundDrawList();
    const auto gold=IM_COL32(210,185,133,255),ivory=IM_COL32(238,235,221,255);
    d->AddRectFilledMultiColor({0,0},{w,h},IM_COL32(8,15,20,255),IM_COL32(12,24,29,255),IM_COL32(4,11,15,255),IM_COL32(5,12,16,255));
    const float side=std::min(280*u,h*.32f),top=h*.13f;
    if(logo)d->AddImage(logo,{(w-side)*.5f,top},{(w+side)*.5f,top+side});
    centered(d,w,h*.48f,17*u,gold,"IRON OAK GAME STUDIO");
    centered(d,w,h*.55f,36*u,ivory,title);
    centered(d,w,h*.62f,15*u,IM_COL32(159,177,181,255),phase);
    const float width=260*u,left=(w-width)*.5f,y=h*.69f;
    d->AddLine({left,y},{left+width,y},IM_COL32(58,75,79,255),2*u);
    // Indeterminate activity mark: intentionally never looks like a % meter.
    const float p=float(std::fmod(std::max(0.,seconds)*.35,1.));
    d->AddCircleFilled({left+width*p,y},3*u,gold);
    centered(d,w,h*.79f,15*u,ivory,outdoors?(battlefield?"Survive the battlefield. Escape into the trees, then follow the smoke.":"Follow the smoke. Speak to the three travelers by the fire."):"Read the bills, then play a new game on the television.");
    centered(d,w,h*.89f,12*u,IM_COL32(133,151,158,255),"EARLY PLAYTEST  |  Please report rough edges and confusing moments.");
}
inline void controls(bool pad,bool battlefield=false){
    if(ImGui::BeginTable("controls-table",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)){
        ImGui::TableSetupColumn("Action",ImGuiTableColumnFlags_WidthStretch,.8f);
        ImGui::TableSetupColumn(pad?"Controller":"Keyboard / mouse",ImGuiTableColumnFlags_WidthStretch,1.5f);ImGui::TableHeadersRow();
        const char *actions[]={"Move","Look","Interact / advance dialogue","Jump / dodge","First / third person","Draw / stow weapon","Inventory","Journal","Pause","Sword cut","Directional guard","Swim / dive / rise"};
        const char *keys[]={"W A S D","Mouse","E / Enter","Space","V","Z","I","J / Tab","Esc","Mouse / arrows: direction; left click: cut","Right click + direction","WASD / Ctrl or C / Space"};
        const char *buttons[]={"Left stick","Right stick (outside combat)","A","X: jump; B: dodge","Right stick click","D-pad down","View / Back","Y","Start / Menu","Right stick: swing in that direction","Hold LT + right stick direction","Left stick / LT / RT"};
        for(unsigned i=0;i<12;++i){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextWrapped("%s",actions[i]);ImGui::TableSetColumnIndex(1);ImGui::TextWrapped("%s",pad?buttons[i]:keys[i]);}
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextWrapped("%s",battlefield?"Opening: read the bills, use the TV, then choose NEW GAME. Survive the battlefield, take the sword, fight through the escape route and cross into the forest. Speak to Rowan, Bram and Ellis by the fire.":"Opening: read the bills, use the TV, then choose NEW GAME. In Valois, speak to Rowan, Bram and Ellis. Bram lends you a weapon before the road encounter.");
    if(battlefield){
        ImGui::TextWrapped("%s",pad?"Battlefield: press X when the duck prompt opens; tap A to wrestle the sword free. A retries after defeat.":"Battlefield: press Q when the duck prompt opens; tap F to wrestle the sword free. Enter or R retries after defeat.");
        ImGui::TextWrapped("Pause settings include a hold-button alternative to repeated struggle taps. The battlefield prompts are interactive and cannot be skipped.");
    }
    ImGui::TextWrapped("Hold Enter / Space or controller A to skip the apartment or dialogue cinematic. Release the button before starting another action.");
}
}
#endif
