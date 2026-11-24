#ifndef VALWOOD_WORLD_NPC_AUTHORING_H
#define VALWOOD_WORLD_NPC_AUTHORING_H
#include "engine/world_navigation.h"
#include "engine/world_npc_source_navigation.h"
#include <cmath>

// Shared by the artist's transaction preview and the final scene publication.
// Failed candidates never replace the actor's last committed route.
namespace world_npc_authoring {
inline bool route(const world_studio::Actor &actor,bool enabled,bool can_walk,
                  const world_navigation::Query &query,const world_navigation::Grid *grid,
                  std::vector<world_navigation::Point> &points,std::string &error){
    points.clear();
    if(actor.points.empty()||actor.points.size()>32){error="Choose one to 32 waypoints.";return false;}
    for(const auto &point:actor.points)if(!std::isfinite(point[0])||!std::isfinite(point[1])){error="Waypoints must have finite ground coordinates.";return false;}
    if(!enabled){
        // Disabled actors retain their authored path and can always be repaired,
        // even when their former position is now blocked or submerged.
        double y=0;if(query.ground)query.ground(query.data,actor.points[0][0],actor.points[0][1],y);
        if(!std::isfinite(y))y=0;
        points.push_back({actor.points[0][0],y,actor.points[0][1]});error.clear();return true;
    }
    if(actor.points.size()>1&&!can_walk){error="This appearance is stationary. Choose a walking appearance or clear its walking path.";return false;}
    if(actor.loop&&actor.points.size()>1&&std::hypot(actor.points.front()[0]-actor.points.back()[0],actor.points.front()[1]-actor.points.back()[1])<.1){error="The closed patrol already returns to its first point. Remove the repeated endpoint.";return false;}
    if(!world_navigation::path(grid,query,actor.points,actor.loop,grid?grid->area.radius:.32f,points,error))return false;
    error.clear();return true;
}
}
#endif
