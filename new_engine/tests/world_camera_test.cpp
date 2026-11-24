/* Exercise the actual viewer movement/route code without opening SDL or GL.
 * The scripted collision callback isolates camera integration; the real shape
 * sweeps/model grounding are covered by world_scenery tests separately.
 * Dead-section linking removes the unused renderer entry point:
 * g++ -std=c++17 -O2 -ffunction-sections -fdata-sections -I.
 * -Ithird_party/imgui -Ithird_party/imgui/backends $(pkg-config --cflags sdl2)
 * tests/world_camera_test.cpp engine/world_walk.c -Wl,--gc-sections
 * -o /tmp/valwood-camera-test
 */
#include "engine/world_viewer.cpp"
#include <cassert>

namespace {
enum class Surface { Flat, Steep, Diagonal };
Surface surface=Surface::Flat;
}
struct WorldScenery {
    enum Kind { Wall, PropTop, Ceiling, Closed } kind;
};
extern "C" float world_atlas_height(double x,double z) {
    return surface==Surface::Steep?float(2*x):(surface==Surface::Diagonal?float(.9*x+.9*z):0.f);
}
extern "C" float world_atlas_walk_height(double x,double z){return world_atlas_height(x,z);}

extern "C" void world_atlas_village_origin(double *x,double *z) {if(x)*x=-1310.4;if(z)*z=-10207.0;}
extern "C" double world_atlas_walk_speed() {return 3.5;}
extern "C" const WorldAtlasTrailPoint *world_atlas_rainforest_trail_points(size_t *count) {
    static const WorldAtlasTrailPoint path[]={{-15,95,1.6f},{2,60,1.6f},{-8,20,1.6f},{5,-25,1.6f},{-5,-65,1.6f},{12,-110,1.6f}};
    if(count)*count=6;
    return path;
}
bool world_scenery_can_move(const WorldScenery *scene,double from_x,double from_z,double to_x,double to_z,
                            float feet,float height,float radius) {
    if(!scene)return true;
    switch(scene->kind) {
        case WorldScenery::Wall:return std::max(from_x,to_x)+radius<=1.;
        case WorldScenery::PropTop:return feet>=1.f||feet+height<=0;
        case WorldScenery::Ceiling:return feet>=3.f||feet+height<=2.5f;
        case WorldScenery::Closed:return from_x==to_x&&from_z==to_z;
    }
    return false;
}

int main() {
    WorldScenery wall{WorldScenery::Wall},prop{WorldScenery::PropTop},ceiling{WorldScenery::Ceiling},closed{WorldScenery::Closed};
    Camera camera;camera.y=1.75;
    auto moved=move_camera(camera,&wall,2,1);
    assert(moved.collision&&moved.sliding&&camera.x==0&&camera.z==1);
    camera.fly=true;moved=move_camera(camera,&wall,2,2);
    assert(!moved.collision&&camera.x==2&&camera.z==2);
    camera=Camera{};camera.y=1.75;surface=Surface::Steep;
    moved=move_camera(camera,nullptr,1,0);
    assert(moved.slope&&camera.x==0&&camera.z==0);
    surface=Surface::Diagonal;
    moved=move_camera(camera,nullptr,1,1);
    assert(moved.slope&&moved.sliding&&camera.x==1&&camera.z==0);
    surface=Surface::Flat;
    camera=Camera{};camera.y=3.;camera.vy=-2;
    move_camera_vertical(camera,&prop,2.5,1.75);
    assert(camera.y>=2.75&&camera.y<2.751&&camera.grounded&&camera.vy==0);
    const double contact=camera.y;
    move_camera_vertical(camera,&prop,camera.y-.01,1.75);
    assert(camera.y>=2.75&&camera.y<=contact&&camera.grounded);
    camera=Camera{};camera.y=1.75;camera.vy=4;
    move_camera_vertical(camera,&ceiling,2.8,1.75);
    assert(camera.y<=2.5&&camera.y>2.498&&!camera.grounded&&camera.vy==0);
    StartingWalkRoute route;camera=Camera{};route.begin(camera);
    for(size_t frame=0;frame<10000&&!route.complete;frame++) {
        double x=camera.x,z=camera.z;route.advance(camera,nullptr);
        assert(std::hypot(camera.x-x,camera.z-z)<=3.5/60.+1.e-8);
    }
    assert(route.complete&&route.walking_frames==6005&&route.collision_blocked==0&&route.slope_blocked==0);
    assert(std::fabs(route.length()-350.25667161211504)<1.e-8);
    assert(std::fabs(route.actual-route.length())<1.e-6&&std::fabs(route.attempted-route.actual)<1.e-6);
    assert(std::hypot(camera.x-route.points[0][0],camera.z-route.points[0][1])<1.e-6);
    StartingWalkRoute blocked;blocked.begin(camera);
    for(size_t frame=0;frame<100;frame++)blocked.advance(camera,&closed);
    assert(!blocked.complete&&blocked.actual==0&&blocked.collision_blocked==100&&blocked.next==1);
    StartingWalkRoute rainforest;assert(rainforest.begin_rainforest(camera));
    assert(!std::strcmp(rainforest.id(),"old_tanthoon_rainforest_return")&&rainforest.point_count()==11);
    for(size_t frame=0;frame<10000&&!rainforest.complete;frame++) {
        double x=camera.x,z=camera.z;rainforest.advance(camera,nullptr);
        assert(std::hypot(camera.x-x,camera.z-z)<=3.5/60.+1.e-8);
    }
    assert(rainforest.complete&&rainforest.collision_blocked==0&&rainforest.slope_blocked==0&&rainforest.sliding_steps==0);
    assert(std::abs(rainforest.actual-rainforest.length())<1.e-6&&std::hypot(camera.x+15,camera.z-95)<1.e-6);
    StartingWalkRoute village;assert(village.begin_village(camera));
    assert(!std::strcmp(village.id(),"valdoa_castle_village_return")&&village.point_count()==11);
    for(size_t frame=0;frame<15000&&!village.complete;frame++) {
        double x=camera.x,z=camera.z;village.advance(camera,nullptr);
        assert(std::hypot(camera.x-x,camera.z-z)<=3.5/60.+1.e-8);
    }
    assert(village.complete&&village.walking_frames==13818);
    assert(village.collision_blocked==0&&village.slope_blocked==0&&village.sliding_steps==0);
    assert(std::abs(village.length()-806.)<1.e-8&&std::abs(village.actual-village.length())<1.e-6);
    assert(std::hypot(camera.x-village.point(0)[0],camera.z-village.point(0)[1])<1.e-6);
    StartingWalkRoute hero;assert(hero.begin_hero(camera));
    assert(!std::strcmp(hero.id(),"valdoa_market_hero_return")&&hero.point_count()==3);
    for(size_t frame=0;frame<2000&&!hero.complete;frame++) {
        double x=camera.x,z=camera.z;hero.advance(camera,nullptr);
        assert(std::hypot(camera.x-x,camera.z-z)<=3.5/60.+1.e-8);
    }
    assert(hero.complete&&hero.walking_frames==1509&&std::abs(hero.actual-88.)<1.e-6);
    assert(hero.collision_blocked==0&&hero.slope_blocked==0&&hero.sliding_steps==0);
    assert(std::hypot(camera.x-hero.point(0)[0],camera.z-hero.point(0)[1])<1.e-6);
    StartingWalkRoute expansion;assert(expansion.begin_expansion(camera));
    assert(!std::strcmp(expansion.id(),"valdoa_castle_training_return")&&expansion.point_count()==26);
    for(size_t frame=0;frame<16000&&!expansion.complete;frame++) {
        double x=camera.x,z=camera.z;expansion.advance(camera,nullptr);
        assert(std::hypot(camera.x-x,camera.z-z)<=3.5/60.+1.e-8);
    }
    assert(expansion.complete&&expansion.walking_frames==14554&&std::abs(expansion.length()-848.9417986638169)<1.e-6);
    assert(expansion.collision_blocked==0&&expansion.slope_blocked==0&&expansion.sliding_steps==0);
    assert(std::abs(expansion.actual-expansion.length())<1.e-6);
    assert(std::hypot(camera.x-expansion.point(0)[0],camera.z-expansion.point(0)[1])<1.e-6);
    // Creek remains visible when normal fly mode lifts the camera to174m.
    Camera creek_fly;double creek_x,creek_z;world_atlas_village_origin(&creek_x,&creek_z);
    creek_fly.x=creek_x+232;creek_fly.z=creek_z-112;creek_fly.y=174;creek_fly.yaw=-2.3932147f;creek_fly.pitch=-1.1f;
    auto creek_forward=creek_fly.forward(),creek_right=creek_fly.right(),creek_up=norm(cross(creek_right,creek_forward));
    assert(castle_creek_visible(creek_fly,creek_forward,creek_right,creek_up));
    creek_fly.x=creek_x+600;
    assert(!castle_creek_visible(creek_fly,creek_forward,creek_right,creek_up));
    // The exterior moat has a410m north run and205m south runs. A small
    // visible span between endpoint/midpoint probes must not disappear.
    // Aim at actual submerged centerline points, including narrow bends and
    // portrait views; the complete segment capsule must survive each frustum.
    size_t creek_span_checks=0;
    for(size_t i=1;i<std::size(world_castle_creek_points);i++) {
        const auto &a=world_castle_creek_points[i-1],&b=world_castle_creek_points[i];
        const double dx=b.x-a.x,dz=b.z-a.z,length=std::hypot(dx,dz);
        double nx=-dz/length,nz=dx/length;
        if(nx*(a.x+b.x)+nz*(a.z+b.z)<0) {nx=-nx;nz=-nz;}
        for(int station=1;station<8;station++) {
            const double t=station/8.,tx=a.x+dx*t,tz=a.z+dz*t;
            if(!world_castle_creek_sample_local(tx,tz,24).submerged)continue;
            for(float aspect:{.25f,16.f/9.f,4.f}) {
                Camera view;view.x=creek_x+tx+nx*6;view.z=creek_z+tz+nz*6;
                view.y=25.75;view.aspect=aspect;
                view.yaw=float(std::atan2(-nx,nz));
                view.pitch=float(std::atan2(WORLD_CASTLE_CREEK_SURFACE_Y-view.y,6.));
                const auto f=view.forward(),r=view.right(),u=norm(cross(r,f));
                assert(castle_creek_visible(view,f,r,u));creek_span_checks++;
            }
        }
    }
    assert(creek_span_checks>=300);
    Camera north_moat;north_moat.x=creek_x;north_moat.z=creek_z-330;
    north_moat.y=25.75;north_moat.yaw=3.14159265f;north_moat.pitch=-.03f;
    auto north_forward=north_moat.forward(),north_right=north_moat.right(),north_up=norm(cross(north_right,north_forward));
    assert(castle_creek_visible(north_moat,north_forward,north_right,north_up));
    north_moat.yaw=0;
    north_forward=north_moat.forward();north_right=north_moat.right();north_up=norm(cross(north_right,north_forward));
    assert(!castle_creek_visible(north_moat,north_forward,north_right,north_up));
    std::printf("PASS continuous moat visibility: %zu actual wet segment stations/aspects; far facing/away controls\n",creek_span_checks);
    unsigned counts[4]={};
    for(uint32_t root=0;root<10000;root++) {
        unsigned choice=root%10,original=choice<4?0:(choice<7?1:(choice<9?2:3));
        assert(Dressing::species(root,0)==original);counts[Dressing::species(root,1)]++;
    }
    assert(counts[0]==0&&counts[1]==4000&&counts[2]==5000&&counts[3]==1000);
    // Horizon buffer order cannot change world-root animation seeds. Select
    // exactly the same nearest bounded membership without sorting28k cards.
    std::vector<std::pair<double,size_t>> candidates;
    for(size_t i=0;i<7001;i++)candidates.push_back({double((i*7919)%3301),i});
    auto expected=candidates;std::sort(expected.begin(),expected.end());expected.resize(HorizonForest::bucket_cap);
    HorizonForest::bound_selection(candidates);std::sort(candidates.begin(),candidates.end());assert(candidates==expected);
    auto under=candidates;HorizonForest::bound_selection(candidates);assert(candidates==under);
    auto old_light=lighting(14,.28f,17),unchanged=local_lighting(14,.28f,17,0),humid=local_lighting(14,.28f,17,1);
    assert(!std::memcmp(&old_light,&unchanged,sizeof(old_light)));
    assert(std::fabs(humid.cloud-.48f)<1e-6f&&humid.haze>old_light.haze&&humid.sunColor.x<old_light.sunColor.x);
    // The authored south-facing facades must receive the same controllable sun
    // that drives the sky, cloud projection, reflections and shadow map.
    const auto afternoon=lighting(17.05f,.28f,22.f),opposite=lighting(17.05f,.28f,22.f,180.f);
    assert(afternoon.sun.z>0&&afternoon.sun.x<0&&afternoon.sun.y>0);
    assert(std::fabs(afternoon.sun.x+opposite.sun.x)<1e-6f&&std::fabs(afternoon.sun.z+opposite.sun.z)<1e-6f);
    assert(std::fabs(afternoon.sun.y-opposite.sun.y)<1e-6f);
    const auto sun_off=lighting(17.05f,.28f,22.f,0,0),fill_half=lighting(17.05f,.28f,22.f,0,1,.5f);
    assert(dot(sun_off.sunColor,sun_off.sunColor)==0&&sun_off.skyLight.x>0);
    assert(fill_half.sunColor.x==afternoon.sunColor.x&&std::fabs(fill_half.skyLight.z*2-afternoon.skyLight.z)<1e-6f);
    assert(std::fabs(fill_half.groundLight.x*2-afternoon.groundLight.x)<1e-6f);
    for(float hour=0;hour<=24;hour+=.25f) {
        const auto solar=lighting(hour,.28f,22.f,31.f,1.12f,1.f);
        assert(std::isfinite(solar.sun.x)&&std::isfinite(solar.sun.y)&&std::isfinite(solar.sun.z));
        assert(std::fabs(dot(solar.sun,solar.sun)-1)<1e-6f);
        if(hour<=5||hour>=19)assert(dot(solar.sunColor,solar.sunColor)==0&&solar.sun.y<0);
    }
    const auto controlled=lighting(14,.28f,17,27.f,1.3f,.8f),local_controlled=local_lighting(14,.28f,17,0,27.f,1.3f,.8f);
    assert(!std::memcmp(&controlled,&local_controlled,sizeof(controlled)));
    std::puts("PASS viewer lighting: south-facing afternoon sun, bearing rotation, separate sun/fill controls, normalized day/night arc and local forwarding");
    std::puts("PASS actual viewer camera: capsule/XZ sliding, fly bypass, slope composition, prop-top/ceiling contact, speed-bounded350m route and honest blocked-route reporting");
}
