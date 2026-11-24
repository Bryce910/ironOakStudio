#ifndef VALWOOD_WORLD_CHARACTER_QUALITY_H
#define VALWOOD_WORLD_CHARACTER_QUALITY_H

#include "engine/world_graphics_settings.h"
#include <filesystem>
#include <string>

// Resolve the protagonist's geometry from the same saved preference used by
// texture loading. The launcher supplies only the immutable bank root; it must
// not parse graphics.settings or independently choose a potentially stale tier.
namespace world_character_quality {
struct StoryAppearances {
    std::string civilian,armored;
    bool selected=false;
};

inline bool resolve_story(const char *root,WorldCharacterTextures quality,
                          StoryAppearances &out,std::string &error){
    StoryAppearances next;
    if(!root||!*root){out=next;error.clear();return true;}
    const char *name=world_character_textures_name(quality);
    if(!name){error="Invalid character/model quality";return false;}
    const std::filesystem::path bank(root);
    if(!bank.is_absolute()){error="Character/model quality bank must have an absolute path";return false;}
    const auto story=bank/name/"story_hero";
    std::error_code ec;
    for(const char *outfit:{"civilian","armored"}){
        const auto appearance=story/outfit/"appearance.cfg",body=story/outfit/"body.glb";
        if(!std::filesystem::is_regular_file(appearance,ec)||ec||
           !std::filesystem::is_regular_file(body,ec)||ec){
            error="Selected character/model quality is incomplete: "+(story/outfit).string();return false;
        }
        (outfit[0]=='c'?next.civilian:next.armored)=appearance.string();
    }
    next.selected=true;out=std::move(next);error.clear();return true;
}
}
#endif
