#ifndef VALWOOD_WORLD_CUTSCENES_HOST_H
#define VALWOOD_WORLD_CUTSCENES_HOST_H
#include "world_cinematics.h"
#include "world_cutscenes.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
namespace world_cutscenes_host {
struct InteractionContext {
    bool library_ready=false,audio_ready=false,progress_ready=false;
    bool editor=false,locked=false,return_neutral=false,opening_priority=false;
    bool loading=false,graphics=false,map=false,inventory=false,keyboard_owned=false;
    bool opening_dialogue=false,component_dialogue=false,journey_dialogue=false;
    bool camera_preview=false,production_freeze=false,flying=false,fighting=false,menu=false;
};
inline bool interaction_allowed(const InteractionContext &c){
    return c.library_ready&&c.audio_ready&&c.progress_ready&&!(c.editor||c.locked||c.return_neutral||c.opening_priority||
        c.loading||c.graphics||c.map||c.inventory||c.keyboard_owned||c.opening_dialogue||c.component_dialogue||c.journey_dialogue||
        c.camera_preview||c.production_freeze||c.flying||c.fighting||c.menu);
}
// Preserve the caller's previous completion history on both filesystem and
// signed-codec failure. A missing file is the only implicit empty-history case.
inline bool read_progress(const std::string &path,world_cutscenes::Progress &out,std::string &error){
    std::error_code ec;const auto status=std::filesystem::symlink_status(std::filesystem::u8path(path),ec);
    if(ec&&ec!=std::errc::no_such_file_or_directory){error="Cannot inspect the saved cutscene completion record: "+ec.message();return false;}
    if(std::filesystem::is_symlink(status)){error="The saved cutscene completion record cannot be a symlink.";return false;}
    const bool exists=std::filesystem::exists(status);
    world_cutscenes::Progress next;
    if(exists&&!world_cutscenes::load_progress(path.c_str(),next,error))return false;
    out=std::move(next);error.clear();return true;
}
// This testable gate never substitutes distance for actual host visibility.
// Visible receives the real eye→NPC chest segment and must query scene geometry.
template<class Visible>bool interaction_visible(world_cinematics::Vec3 eye,double yaw,
    world_cinematics::Vec3 feet,double authored_radius,Visible visible){
    for(double v:{eye.x,eye.y,eye.z,yaw,feet.x,feet.y,feet.z,authored_radius})if(!std::isfinite(v))return false;
    const double dx=feet.x-eye.x,dz=feet.z-eye.z,distance=std::hypot(dx,dz);
    if(distance>std::clamp(authored_radius,.5,8.)||std::abs(feet.y-(eye.y-1.75))>1.1)return false;
    if(distance>.2&&(dx*std::sin(yaw)-dz*std::cos(yaw))/distance<.25)return false;
    return visible(eye,world_cinematics::Vec3{feet.x,feet.y+1.35,feet.z});
}
}
#endif
