#include "engine/world_surface_field.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace world_surface;
float ground(double x,double z,void *){return float(20+x*.01+z*.005);}
int main(){
 Data d;Ribbon road;road.points={{112,23,0},{144,23,0}};road.width=6;road.shoulder=4;d.ribbons.push_back(road);TerrainField field;std::string error;
 assert(bake_terrain(d,ground,nullptr,field,error));assert(std::abs(terrain_offset(field,128,0)+ground(128,0,nullptr)-23)<1e-5);
 assert(std::abs(terrain_offset(field,128,12))<1e-5);assert(std::abs(terrain_offset(field,128-1e-4,0)-terrain_offset(field,128+1e-4,0))<1e-4);
 const TerrainTile *left=nullptr,*right=nullptr;for(const auto &t:field.tiles){if(t.x==0&&t.z==0)left=&t;if(t.x==1&&t.z==0)right=&t;}assert(left&&right);for(unsigned i=0;i<paint_side;i++)assert(left->delta[i*paint_side+64]==right->delta[i*paint_side]);
 assert(road_mask(field,128,0)>.999f&&road_mask(field,128,12)==0);
 assert(road_mask(field,128,5)>0&&road_mask(field,128,5)<1);
 assert(std::abs(road_mask(field,128-1e-4,0)-road_mask(field,128+1e-4,0))<1e-4);
 float level=0,shore=0;assert(!river(field,128,0,level,shore));
 d.ribbons[0].kind=RibbonKind::River;d.ribbons[0].width=8;d.ribbons[0].depth=2;
 assert(bake_terrain(d,ground,nullptr,field,error));assert(river(field,128,0,level,shore)&&level==23&&shore==1);assert(road_mask(field,128,0)==0);assert(ground(128,0,nullptr)+terrain_offset(field,128,0)<level-1.9);
 assert(!river(field,128,5,level,shore));assert(ground(128,4,nullptr)+terrain_offset(field,128,4)>23);
 const auto before=field.tiles.size();d.ribbons[0].width=2;assert(!bake_terrain(d,ground,nullptr,field,error)&&field.tiles.size()==before);
 d.ribbons.clear();assert(bake_terrain(d,ground,nullptr,field,error)&&field.tiles.empty()&&terrain_offset(field,128,0)==0&&road_mask(field,128,0)==0);
 std::puts("PASS derived roads/rivers: non-destructive removal, exact seams, conforming grade, carved channel/dry bank, shared water query and failed-bake rollback");
}
