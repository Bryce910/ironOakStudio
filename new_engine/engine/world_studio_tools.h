#ifndef VALWOOD_WORLD_STUDIO_TOOLS_H
#define VALWOOD_WORLD_STUDIO_TOOLS_H
#include "engine/world_studio.h"
#include <limits>
namespace world_studio {
constexpr unsigned added_key=8192,selection_limit=128;
struct Vec {double x=0,y=0,z=0;};
struct Ray {Vec origin,direction;};
struct Pose {std::string alias;Vec root;double yaw=0,scale=1;float burial=0,elevation=0,offset_y=0;Vec minimum,maximum;bool hidden=false;double pitch=0,roll=0,sx=1,sy=1,sz=1;};
// True oriented model bounds, not nearest-root or screen-center selection.
bool ray_bounds(const Ray &,const Pose &,double &distance);
bool ray_plane(const Ray &,Vec point,Vec normal,Vec &hit);
bool effective_pose(const Document &,const std::vector<Pose> &source,unsigned key,Pose &);
bool transform_selection(Document &,const std::vector<Pose> &,const std::vector<unsigned> &,
                         Vec pivot,Vec translation,double yaw_degrees,double scale,std::string &);
bool duplicate_selection(Document &,const std::vector<Pose> &,const std::vector<unsigned> &,
                         Vec translation,std::vector<unsigned> &created,std::string &);
void erase_placement(Document &,unsigned index);
bool delete_selection(Document &,const std::vector<Pose> &,const std::vector<unsigned> &,std::string &);
}
#endif
