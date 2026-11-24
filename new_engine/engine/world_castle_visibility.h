#ifndef VALWOOD_WORLD_CASTLE_VISIBILITY_H
#define VALWOOD_WORLD_CASTLE_VISIBILITY_H
#include <algorithm>
#include <cmath>

// Camera selection is cached for <.75m horizontal / .25m vertical movement,
// <.01rad yaw and <.005rad pitch. Four metres covers that drift at the grass
// shader's90m limit, plus floating-point rebasing and a margin beyond each edge.
// radius is the COMPLETE source geometry + wind radius, not a root capsule.
struct WorldVegetationVisibilityView {
    double x=0,y=0,z=0,fx=0,fy=0,fz=-1,rx=1,rz=0,ux=0,uy=1,uz=0;
    double tan_h=1,tan_v=.5994296232,plane_h=1.4142135624,plane_v=1.165897211;
    double guard=4;bool valid=false;
};
inline WorldVegetationVisibilityView world_vegetation_visibility_view(double x,double y,double z,double yaw,double pitch,double aspect,double guard=4.) {
    WorldVegetationVisibilityView out;
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)||!std::isfinite(pitch)||
       !std::isfinite(aspect)||aspect<=0||!std::isfinite(guard)||guard<4)return out;
    out.x=x;out.y=y;out.z=z;out.rx=std::cos(yaw);out.rz=std::sin(yaw);
    const double cp=std::cos(pitch),sp=std::sin(pitch);
    out.fx=out.rz*cp;out.fy=sp;out.fz=-out.rx*cp;
    out.ux=-out.rz*sp;out.uy=cp;out.uz=out.rx*sp;
    out.tan_v=std::tan(.54);out.tan_h=out.tan_v*aspect;
    out.plane_h=std::hypot(1.,out.tan_h);out.plane_v=std::hypot(1.,out.tan_v);
    out.guard=guard;out.valid=true;return out;
}
// fade_distance must match the existing shader's root-distance endpoint:
// procedural grass90m; photographed grass70m. This removes only already-zero
// coverage or wholly offscreen geometry; it never changes density/fade/rank.
inline bool world_vegetation_sphere_outside(const WorldVegetationVisibilityView &v,double x,double y,double z,double radius,double fade_distance) {
    if(!v.valid||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(radius)||radius<0||!std::isfinite(fade_distance)||fade_distance<=0)return false;
    const double dx=x-v.x,dy=y-v.y,dz=z-v.z;
    const double far=fade_distance+v.guard;
    if(dx*dx+dz*dz>far*far)return true;
    const double along=dx*v.fx+dy*v.fy+dz*v.fz,r=radius+v.guard;
    if(along < -r)return true;
    if(std::abs(dx*v.rx+dz*v.rz)>std::max(0.,along)*v.tan_h+r*v.plane_h)return true;
    return std::abs(dx*v.ux+dy*v.uy+dz*v.uz)>std::max(0.,along)*v.tan_v+r*v.plane_v;
}

// Actual camera eye, not the cached vegetation camera. Local XZ is relative
// to the village origin and Y is absolute. The source receiving hall has
// inner faces at X+/-11.6, Z[-153.6,-136.4], a floor at24.015 and the first
// ceiling at30.995. Keep generous guards around every face and the open south
// entrance. From this box only the north moat passes its127.8m distance gate;
// both actual keep LODs block those directions, including the opaque recessed
// lancet backs. This is a bounded interior classification, not a general
// occluder for arbitrary water or a reason to remove exterior shadow casters.
inline bool world_castle_hall_enclosed_eye(double local_x,double absolute_y,double local_z) {
    return std::isfinite(local_x)&&std::isfinite(absolute_y)&&std::isfinite(local_z)&&
           std::abs(local_x)<=10.5&&local_z>=-152.5&&local_z<=-137.5&&
           absolute_y>=24.5&&absolute_y<=30.5;
}

// Local east/south metres from the village origin; height remains absolute Y.
// This helper is intended for NON-SHADOW grass/groundcover only. Removing a
// shadow caster would need an independent light-view visibility proof.
struct WorldCastleOcclusionView {
    double x=0,y=0,z=0,horizontal_guard=1.5,vertical_guard=.5;bool active=false;
    unsigned exterior_sides=0;bool inside=false;
};
inline WorldCastleOcclusionView world_castle_occlusion_view(double local_x,double y,double local_z,double horizontal_guard=1.5,double vertical_guard=.5) {
    WorldCastleOcclusionView out;
    if(!std::isfinite(local_x)||!std::isfinite(y)||!std::isfinite(local_z)||!std::isfinite(horizontal_guard)||horizontal_guard<1.5||!std::isfinite(vertical_guard)||vertical_guard<.5)return out;
    out.x=local_x;out.y=y;out.z=local_z;out.horizontal_guard=horizontal_guard;out.vertical_guard=vertical_guard;
    out.active=std::abs(local_x)<320&&local_z>-310&&local_z<100;
    // A segment with both ends outside the same face of the castle's convex
    // envelope cannot meet any wall. Preclassify the full cached-camera box.
    if(local_x-horizontal_guard>65.5)out.exterior_sides|=1;
    if(local_x+horizontal_guard< -65.5)out.exterior_sides|=2;
    if(local_z-horizontal_guard> -54.5)out.exterior_sides|=4;
    if(local_z+horizontal_guard< -195.5)out.exterior_sides|=8;
    out.inside=local_x-horizontal_guard> -64.5&&local_x+horizontal_guard<64.5&&
               local_z-horizontal_guard> -194.5&&local_z+horizontal_guard< -55.5;
    return out;
}
namespace world_castle_visibility_detail {
inline bool one_piece(double lo,double hi,double begin,double end,unsigned count) {
    if(lo<begin||hi>end||lo>hi)return false;
    const double step=(end-begin)/count;const unsigned index=unsigned((lo-begin)/step);
    if(index>=count)return false;
    // Each measured source wall has a continuous lower core but tiny join
    // tolerances. Retain a further1.25m beyond the cached-eye interval at
    // every piece edge; do not occlude across joins, tower ends, or the gate.
    return lo>=begin+index*step+1.25&&hi<=begin+(index+1)*step-1.25;
}
inline void project_interval(double tlo,double thi,double emin,double emax,double omin,double omax,double &lo,double &hi) {
    lo=std::min(emin*(1-tlo)+omin*tlo,emin*(1-thi)+omin*thi);
    hi=std::max(emax*(1-tlo)+omax*tlo,emax*(1-thi)+omax*thi);
}
inline bool wall(const WorldCastleOcclusionView &v,bool x_plane,double plane,double xmin,double ymin,double zmin,double xmax,double ymax,double zmax) {
    const double eye_a=x_plane?v.x:v.z,eye_b=x_plane?v.z:v.x;
    const double omin_a=x_plane?xmin:zmin,omax_a=x_plane?xmax:zmax;
    const double omin_b=x_plane?zmin:xmin,omax_b=x_plane?zmax:xmax;
    const double emin_a=eye_a-v.horizontal_guard,emax_a=eye_a+v.horizontal_guard;
    const bool ascending=emax_a<plane-.5&&omin_a>plane+.5;
    if(!ascending&&!(emin_a>plane+.5&&omax_a<plane-.5))return false;
    // Convex cross-axis rejection precedes division. For example front-bank
    // grass and an east-bank camera cannot meet the north/south wall patches.
    const double begin=x_plane?-195.:-65.,end=x_plane?-55.:65.;
    if(std::min(eye_b-v.horizontal_guard,omin_b)>end||std::max(eye_b+v.horizontal_guard,omax_b)<begin)return false;
    // Interval arithmetic encloses every ray from the complete cached-camera
    // motion box to the entire source/wind AABB, across the WHOLE1m wall
    // thickness containing both actual source faces. This also guards grazing
    // rays near wall ends. No sampled ray approximation.
    // t=(face-eye)/(object-eye) is monotonic in all three arguments
    // while the complete boxes lie on opposite sides. The exact same extrema
    // as all8corner combinations therefore require only two divisions.
    const double tlo=ascending?(plane-.5-emax_a)/(omax_a-emax_a):(plane+.5-emin_a)/(omin_a-emin_a);
    const double thi=ascending?(plane+.5-emin_a)/(omin_a-emin_a):(plane-.5-emax_a)/(omax_a-emax_a);
    if(tlo<=0||thi>=1)return false;
    double lo_b,hi_b,lo_y,hi_y;
    project_interval(tlo,thi,eye_b-v.horizontal_guard,eye_b+v.horizontal_guard,omin_b,omax_b,lo_b,hi_b);
    project_interval(tlo,thi,v.y-v.vertical_guard,v.y+v.vertical_guard,ymin,ymax,lo_y,hi_y);
    // Source lower wall core covers this entire conservative height band.
    // The actual base is23.9m; parapet gaps start above31.5m even on the
    // shortest south run. Canopies above this band are always retained.
    if(lo_y<24.50||hi_y>30.80)return false;
    if(x_plane)return one_piece(lo_b,hi_b,-195,-55,13);
    if(plane==-195)return one_piece(lo_b,hi_b,-65,65,12);
    return one_piece(lo_b,hi_b,-65,-5.9,6)||one_piece(lo_b,hi_b,5.9,65,6);
}
}
// AABB must cover every source vertex and wind displacement. Only reject if
// its entire projected silhouette lies inside ONE opaque core piece. The
// south gate, per-piece joins and anything reaching above the core stay drawn.
inline bool world_castle_wall_occludes(const WorldCastleOcclusionView &v,double xmin,double ymin,double zmin,double xmax,double ymax,double zmax) {
    if(!v.active)return false;
    if(((v.exterior_sides&1)&&xmin>65.5)||((v.exterior_sides&2)&&xmax< -65.5)||
       ((v.exterior_sides&4)&&zmin> -54.5)||((v.exterior_sides&8)&&zmax< -195.5))return false;
    if(v.inside&&xmin> -64.5&&xmax<64.5&&zmin> -194.5&&zmax< -55.5)return false;
    if(!std::isfinite(xmin)||!std::isfinite(ymin)||!std::isfinite(zmin)||!std::isfinite(xmax)||!std::isfinite(ymax)||!std::isfinite(zmax)||xmin>xmax||ymin>ymax||zmin>zmax)return false;
    using world_castle_visibility_detail::wall;
    return wall(v,true,65,xmin,ymin,zmin,xmax,ymax,zmax)||wall(v,true,-65,xmin,ymin,zmin,xmax,ymax,zmax)||
           wall(v,false,-195,xmin,ymin,zmin,xmax,ymax,zmax)||wall(v,false,-55,xmin,ymin,zmin,xmax,ymax,zmax);
}
#endif
