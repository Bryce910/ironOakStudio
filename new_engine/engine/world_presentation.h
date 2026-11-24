#ifndef VALWOOD_WORLD_PRESENTATION_H
#define VALWOOD_WORLD_PRESENTATION_H
#include "engine/world_gameplay.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace world_presentation {
constexpr std::uint64_t threat_counter=721006,threat_encounter=710002;
constexpr double crossing_offset_x=-21,crossing_offset_z=25;
// Retry can be selected on the very first defeated frame, before the host's
// ordinary next-frame acknowledgement. Commit that result before reopening.
template<class Runtime> inline bool queue_retry(Runtime &runtime,std::uint64_t &pending,std::uint64_t target){
    if(pending){if(!runtime.enqueue({world_gameplay::EventKind::EncounterFinished,0,pending,0,0}))return false;pending=0;}
    return runtime.enqueue({world_gameplay::EventKind::Interact,target});
}

enum class Chapter {Arrival,Trial,Expedition,Threat,Report,Homecoming,Complete};
inline Chapter chapter(bool accepted,int mastery,int threat,int report,int returned){
    if(returned)return Chapter::Complete;
    if(report)return Chapter::Homecoming;
    if(mastery&&threat)return Chapter::Report;
    if(mastery)return Chapter::Expedition;
    return accepted?Chapter::Trial:Chapter::Arrival;
}
inline const char *title(Chapter c){switch(c){
    case Chapter::Arrival:return "A PLACE IN THE WATCH";
    case Chapter::Trial:return "THE WATCH TRIAL";
    case Chapter::Expedition:return "INTO THE EMERALD WOOD";
    case Chapter::Threat:return "THE BROKEN OATH";
    case Chapter::Report:return "WORD FROM THE WAYFARERS";
    case Chapter::Homecoming:return "A PROMISE KEPT";
    case Chapter::Complete:return "WELCOME TO THE WATCH";}return "VALWOOD";}
inline const char *objective(Chapter c){switch(c){
    case Chapter::Arrival:return "Meet Master Corvin in the west training court.";
    case Chapter::Trial:return "Pass Corvin's three-round trial. Read the guard; strike the opening.";
    case Chapter::Expedition:return "Take the south trail to the wayfarers' clearing.";
    case Chapter::Threat:return "Defeat the deserter blocking the crossing.";
    case Chapter::Report:return "Speak to Warden Rowan and collect the trail report.";
    case Chapter::Homecoming:return "Return to Master Corvin with word from the wood.";
    case Chapter::Complete:return "Your deeds are remembered. The watch is yours to explore.";}return "";}
inline float stick(float value){if(!std::isfinite(value)||std::abs(value)<.18f)return 0;return std::copysign(std::min(1.f,(std::abs(value)-.18f)/.82f),value);}
inline float ease(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
inline std::uint64_t voice_line(std::uint64_t line,Chapter c){return line==730011?(c==Chapter::Homecoming?730012:c==Chapter::Complete?730013:730011):line;}
struct Settings {float sensitivity=1,text_scale=1,music=.60f,voice=1,deadzone=.18f;bool invert_y=false,reduced_motion=false,subtitles=true,story_assist=true,vibration=true;};
}
#endif
