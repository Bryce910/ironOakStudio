#ifndef VALWOOD_WORLD_NAVIGATION_H
#define VALWOOD_WORLD_NAVIGATION_H
#include "engine/world_runtime_editors.h"
#include <vector>
#include <string>
namespace world_navigation {
struct Point {double x=0,y=0,z=0;};
struct Query {void *data=nullptr;bool (*ground)(void *,double,double,double &)=nullptr;bool (*clear)(void *,const Point &,const Point &,float)=nullptr;};
struct Cell {float y=0;bool open=false;unsigned char edges=0;};
struct Grid {world_studio::NavigationArea area;unsigned side=0;std::vector<Cell> cells;unsigned open=0;};
bool bake(const world_studio::NavigationArea &,const Query &,Grid &,std::string &);
bool path(const Grid *,const Query &,const std::vector<std::array<double,2>> &,bool loop,float radius,std::vector<Point> &,std::string &);
}
#endif
