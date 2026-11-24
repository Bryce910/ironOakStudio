#ifndef VALWOOD_WORLD_DESIGNER_WORKSPACE_H
#define VALWOOD_WORLD_DESIGNER_WORKSPACE_H
#include "engine/world_designer_viewport.h"
#include <string>
#include <vector>
namespace world_designer_workspace {
struct Preferences {
    world_designer_viewport::Layout layout;
    int tool=2,category=0;
    bool grid=true,favorites_only=false,ground_grid=false;
    std::vector<std::string> favorites,recent;
};
bool encode(const Preferences &,std::string &,std::string &error);
bool decode(const std::string &,Preferences &,std::string &error);
bool load(const std::string &path,Preferences &,std::string &error);
bool save(const std::string &path,const Preferences &,std::string &error);
bool matches(const std::string &,const char *query);
}
#endif
