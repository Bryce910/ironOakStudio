#include "engine/world_physics.h"
#include "engine/world_studio_tools.h"
#include "third_party/imgui/imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
double clock_seconds=0;
struct Direction {float x=0,y=0,z=-1;};
struct Camera {double x=0,y=2,z=5;Direction forward()const{return {};}};
struct PhysicsStudio {
    std::string project_directory,message;
    bool target=true,playing=false,freeze=false;double x=0,y=0,z=0;
    int tab=0,requested_tab=0;
    mutable std::vector<world_studio::Vec> projected_points;
    mutable bool finite_projection=true;
    bool project_point(const Camera &camera,world_studio::Vec point,ImVec2 &out)const{
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)){finite_projection=false;return false;}
        projected_points.push_back(point);
        const double dx=point.x-camera.x,dy=point.y-camera.y,dz=point.z-camera.z;
        out={float(700+8*dx+2*dz),float(500-8*dy+dz)};
        finite_projection&=std::isfinite(out.x)&&std::isfinite(out.y);return finite_projection;
    }
    bool production_playing()const{return playing;}
    bool production_freeze()const{return freeze;}
    static double production_now(){return clock_seconds;}
    #include "engine/world_studio_physics_view.inc"
};
unsigned checks=0;
#define CHECK(condition) do{++checks;if(!(condition)){std::fprintf(stderr,"Physics Studio failure at %d: %s\n",__LINE__,#condition);return 1;}}while(0)
}
int main(){
    char temporary[]="/tmp/valwood-physics-studio-XXXXXX";CHECK(mkdtemp(temporary));
    PhysicsStudio studio;studio.project_directory=temporary;std::string path;
    CHECK(studio.physics_path(path)&&path==std::string(temporary)+"/world.physics");
    for(const char *invalid:{"../outside.physics","/tmp/outside.physics","scene.txt","./world.physics"}){std::snprintf(studio.physics_file,sizeof studio.physics_file,"%s",invalid);CHECK(!studio.physics_path(path));}
    CHECK(symlink("/tmp",(std::string(temporary)+"/escape").c_str())==0);
    std::snprintf(studio.physics_file,sizeof studio.physics_file,"escape/world.physics");CHECK(!studio.physics_path(path));
    std::snprintf(studio.physics_file,sizeof studio.physics_file,"world.physics");
    studio.physics_add_body();CHECK(studio.physics_document.bodies.size()==1&&studio.physics_document.bodies[0].id==1&&studio.physics_undo.size()==1);
    studio.physics_history(false);CHECK(studio.physics_document.bodies.empty()&&studio.physics_redo.size()==1);
    studio.physics_add_body();CHECK(studio.physics_document.bodies[0].id==2&&studio.physics_redo.empty());
    auto &floor=studio.physics_document.bodies[0];floor.pose.position={0,-1,0};floor.size={20,1,20};
    studio.y=2;studio.physics_add_body();auto &ball=studio.physics_document.bodies.back();ball.shape=world_physics::Shape::Sphere;ball.motion=world_physics::Motion::Dynamic;ball.mass=1;const auto ball_id=ball.id;
    studio.physics_configure();CHECK(studio.physics_ready&&!studio.physics_stale());
    studio.physics_running=true;studio.physics_service(1,true);for(unsigned frame=1;frame<=300;++frame)studio.physics_service(1+frame*world_physics::fixed_step,true);
    world_physics::BodyState state;CHECK(studio.physics_preview->body_state(ball_id,state)&&std::abs(state.pose.position.y-.5)<.02);CHECK(studio.physics_event_total>0);
    const auto ticks=studio.physics_preview->stats().ticks;studio.physics_service(100,false);CHECK(studio.physics_preview->stats().ticks==ticks&&studio.physics_clock==0);studio.physics_service(100,true);CHECK(studio.physics_preview->stats().ticks==ticks);
    studio.physics_changed();CHECK(studio.physics_stale()&&!studio.physics_running);studio.physics_configure();CHECK(studio.physics_preview->body_state(ball_id,state)&&std::abs(state.pose.position.y-2.5)<1e-8);
    for(unsigned i=0;i<12;++i){studio.physics_checkpoint();studio.physics_document.gravity.x=i;studio.physics_changed();}CHECK(studio.physics_undo.size()==8);studio.physics_history(false);CHECK(studio.physics_document.gravity.x==10);
    studio.physics_selection=1;world_physics::Door door;door.body=ball_id;studio.physics_document.doors.push_back(door);studio.physics_remove_body();CHECK(studio.physics_document.bodies.size()==1&&studio.physics_document.doors.empty());studio.physics_history(false);CHECK(studio.physics_document.bodies.size()==2&&studio.physics_document.doors.size()==1);
    studio.physics_document.doors.clear();studio.physics_document.gravity.x=0;studio.physics_changed();
    CHECK(studio.physics_path(path)&&world_physics::save(path.c_str(),studio.physics_document,studio.message));

    // Render the real inspector widgets without an SDL/GPU context. Test both
    // unconfigured and configured state, invalid draft fields and Play locking.
    ImGui::CreateContext();auto &io=ImGui::GetIO();io.DisplaySize={1400,1000};io.DeltaTime=1.f/60;io.IniFilename=nullptr;io.LogFilename=nullptr;
    unsigned char *pixels=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    // Real ImGui background geometry, using a camera-relative projection stub
    // that records double world inputs. All collider shapes stay selected-only
    // and finite even fifty thousand kilometres away from the origin.
    PhysicsStudio overlay;overlay.project_directory=temporary;
    world_physics::Body collider;collider.id=1001;collider.pose.position={50000000,100,-50000000};collider.size={1,2,3};
    collider.pose.rotation={0,0,float(std::sqrt(.5)),float(std::sqrt(.5))};
    overlay.physics_document.bodies.push_back(collider);overlay.physics_selection=0;
    Camera debug_camera;debug_camera.x=collider.pose.position.x;debug_camera.y=collider.pose.position.y;debug_camera.z=collider.pose.position.z;
    auto overlay_begin=[&]{ImGui::NewFrame();overlay.projected_points.clear();overlay.finite_projection=true;};
    auto overlay_end=[&]{bool finite=overlay.finite_projection;const auto *draw=ImGui::GetBackgroundDrawList();
        for(const auto &vertex:draw->VtxBuffer)finite&=std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y)&&std::isfinite(vertex.uv.x)&&std::isfinite(vertex.uv.y);
        ImGui::Render();return finite;
    };
    overlay_begin();overlay.physics_draw_overlay(debug_camera);
    CHECK(overlay.physics_overlay_segments==12&&!overlay.physics_overlay_live&&overlay.projected_points.size()==24);
    CHECK(std::abs(overlay.projected_points[0].x-collider.pose.position.x-2)<1e-5&&std::abs(overlay.projected_points[0].y-collider.pose.position.y+1)<1e-5&&std::abs(overlay.projected_points[0].z-collider.pose.position.z+3)<1e-5);
    CHECK(overlay_end());
    auto &debug_body=overlay.physics_document.bodies[0];debug_body.pose.rotation={};debug_body.shape=world_physics::Shape::Sphere;debug_body.size.x=2;
    overlay_begin();overlay.physics_draw_overlay(debug_camera);CHECK(overlay.physics_overlay_segments==96&&overlay.projected_points.size()==192);
    for(auto point:overlay.projected_points)CHECK(std::abs(std::hypot(std::hypot(point.x-collider.pose.position.x,point.y-collider.pose.position.y),point.z-collider.pose.position.z)-2)<1e-7);
    CHECK(overlay_end());
    debug_body.shape=world_physics::Shape::Capsule;debug_body.size={.75,1.25,.5};
    overlay_begin();overlay.physics_draw_overlay(debug_camera);CHECK(overlay.physics_overlay_segments==132&&overlay.projected_points.size()==264);
    double low=1e30,high=-1e30;for(auto point:overlay.projected_points){low=std::min(low,point.y);high=std::max(high,point.y);}CHECK(std::abs(low-98)<1e-8&&std::abs(high-102)<1e-8);CHECK(overlay_end());
    debug_body.shape=world_physics::Shape::Mesh;debug_body.vertices={{0,0,0},{1,0,0},{0,1,0}};debug_body.triangles.assign(129,{0,1,2});
    overlay_begin();overlay.physics_draw_overlay(debug_camera);CHECK(overlay.physics_overlay_triangles==128&&overlay.physics_overlay_segments==384&&overlay.projected_points.size()==768);CHECK(overlay_end());
    debug_body.triangles[0]={999,1,2};overlay_begin();overlay.physics_draw_overlay(debug_camera);CHECK(overlay.physics_overlay_triangles==127&&overlay.physics_overlay_segments==381);CHECK(overlay_end());
    debug_body.pose.rotation={0,0,0,0};overlay_begin();overlay.physics_draw_overlay(debug_camera);CHECK(overlay.physics_overlay_segments==0&&overlay.projected_points.empty());CHECK(overlay_end());
    debug_body.pose.rotation={};overlay.physics_overlay=false;overlay_begin();overlay.physics_draw_overlay(debug_camera);CHECK(overlay.physics_overlay_segments==0&&overlay.projected_points.empty());CHECK(overlay_end());overlay.physics_overlay=true;
    overlay.physics_selection=-1;overlay_begin();overlay.physics_draw_overlay(debug_camera);CHECK(overlay.physics_overlay_segments==0);CHECK(overlay_end());overlay.physics_selection=0;
    debug_body.shape=world_physics::Shape::Sphere;debug_body.motion=world_physics::Motion::Dynamic;debug_body.vertices.clear();debug_body.triangles.clear();debug_body.linear_velocity={0,2,0};overlay.physics_document.gravity={};
    overlay.physics_configure();CHECK(overlay.physics_ready&&!overlay.physics_stale());
    world_physics::Pose moved=debug_body.pose;moved.position.x+=3;moved.position.y+=4;moved.position.z-=2;
    CHECK(overlay.physics_preview->teleport(debug_body.id,moved)&&overlay.physics_preview->step(world_physics::fixed_step));
    world_physics::BodyState actual;CHECK(overlay.physics_preview->body_state(debug_body.id,actual));
    overlay_begin();overlay.physics_draw_overlay(debug_camera);CHECK(overlay.physics_overlay_live&&overlay.physics_overlay_segments==96);
    CHECK(std::abs(overlay.projected_points[0].x-actual.pose.position.x-.75)<1e-7&&std::abs(overlay.projected_points[0].y-actual.pose.position.y)<1e-8&&std::abs(overlay.projected_points[0].z-actual.pose.position.z)<1e-8);CHECK(overlay_end());
    overlay.physics_changed();overlay_begin();overlay.physics_draw_overlay(debug_camera);CHECK(!overlay.physics_overlay_live&&overlay.physics_overlay_segments==96);
    CHECK(std::abs(overlay.projected_points[0].x-debug_body.pose.position.x-.75)<1e-7&&std::abs(overlay.projected_points[0].y-debug_body.pose.position.y)<1e-8);CHECK(overlay_end());
    Camera camera;
    for(unsigned frame=0;frame<5;++frame){clock_seconds=200+frame/60.;ImGui::NewFrame();ImGui::Begin("Physics inspector fixture");if(ImGui::BeginTabBar("Fixture tabs")){studio.physics_panel(camera);ImGui::EndTabBar();}ImGui::End();ImGui::Render();if(frame==0){CHECK(studio.tab==20);studio.physics_configure();studio.physics_selection=1;}if(frame==1)studio.physics_document.bodies[1].pose.rotation={0,0,0,0};if(frame==2)studio.playing=true;if(frame==3){studio.playing=false;studio.physics_preview.reset();studio.physics_ready=false;}}
    ImGui::DestroyContext();
    std::filesystem::remove_all(temporary);
    std::printf("world_physics_studio: %u path confinement/history/ID/actual preview/inspector and collider overlay contracts passed without GPU\n",checks);
}
