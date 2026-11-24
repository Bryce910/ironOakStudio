/* CPU parser/selection tests with the real atlas. Model/GL stubs keep this
 * deterministic, check uploaded coordinates and permit allocation assertions.
 * Actual model bounds/import/rendering are separately verified on native GL.
 */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_scenery.h"
#include "engine/world_models.h"
#include "engine/world_model_cpu.h"
#include "engine/world_cover.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <utility>
#include <unistd.h>
#include <thread>
#include <chrono>

namespace {
bool track_allocations = false, fail_upload = false;
std::atomic<bool> fail_stream_prepare{false};
std::size_t allocations = 0, active_models = 0, uniform_queries = 0, draws = 0;
std::size_t drawn_instances = 0;
const char *fail_asset = nullptr, *last_draw_path = nullptr;
const WorldModelAsset *last_draw_asset = nullptr;
std::size_t last_draw_count = 0;
int fail_upload_after = -1;
float last_anchor[2] = {}, last_offset[3] = {};
float last_tree_height = 0, last_prop_lod[3] = {};
unsigned prop_near_draws=0,prop_mid_draws=0;
int last_vegetation = 0, last_lod = 0, last_source_distance=0;
float last_distance_fade[2]={};
double expected_x = 0, expected_z = 0;
bool check_single_root = false, capture_sort_uploads = false;
std::array<WorldModelInstance,8> sort_uploaded = {};
std::size_t sort_uploaded_count=0;
struct DrawRecord {
    const WorldModelAsset *asset=nullptr;
    std::size_t count=0;
    float anchor[2]={},offset[3]={},prop_lod[3]={},tree_height=0;
    int vegetation=0,lod=0,source_distance=0;
};
struct DrawCapture { std::array<DrawRecord,32> records={};std::size_t count=0; };
bool capture_draws=false;
DrawCapture captured_draws;
}
void *operator new(std::size_t size) {
    if (track_allocations) ++allocations;
    if (void *memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    if (track_allocations) ++allocations;
    return std::malloc(size ? size : 1);
}
void operator delete(void *memory, const std::nothrow_t &) noexcept { std::free(memory); }

struct WorldModelAsset {
    char path[512] = {};
    float minimum[3] = {-2, 0, -2}, maximum[3] = {2, 3, 2};
    float physical_height=3;
    std::array<WorldModelInstance, 4096> instances = {};
    std::size_t count = 0;
};
extern "C" WorldModelAsset *world_model_load(const char *path) {
    if (fail_asset && std::strstr(path, fail_asset)) return nullptr;
    assert(!std::strstr(path, "..") && (std::strstr(path, "models/cinematic_") == path || std::strstr(path, "models/woodland_") == path || std::strstr(path, "models/starting_") == path || std::strstr(path, "models/showcase_") == path || std::strstr(path,"models/village_")==path || std::strstr(path,"models/purchased_")==path || std::strstr(path,"models/castle_")==path));
    auto *asset = new WorldModelAsset;
    std::snprintf(asset->path, sizeof(asset->path), "%s", path);
    float radius = 2, height = 3;
    if (std::strstr(path, "pine_young")) { radius = 1; height = 3.2f; }
    else if (std::strstr(path, "broadleaf")) { radius = 3.2f; height = 10; }
    else if (std::strstr(path, "_pine_")) { radius = 6; height = 25; }
    else if (std::strstr(path, "_fir_")) { radius = 6; height = 22; }
    else if (std::strstr(path, "fern")) { radius = .8f; height = 1.2f; }
    else if (std::strstr(path, "deadwood")) { radius = 2.03f; height = 1.05f; }
    else if (std::strstr(path, "cliff")) { radius = 8; height = 10.264f; }
    asset->physical_height=height;
    // Canvas padding belongs to culling bounds, not projected canopy height.
    if(std::strstr(path,"_far")&&(std::strstr(path,"_pine_")||std::strstr(path,"_fir_")))height*=1.06f;
    asset->minimum[0] = asset->minimum[2] = -radius;
    asset->maximum[0] = asset->maximum[2] = radius;
    asset->maximum[1] = height;
    if (std::strstr(path, "cliff")) {
        asset->minimum[0] = -45.0571f; asset->maximum[0] = 46.9221f;
        asset->minimum[2] = -6.2058f; asset->maximum[2] = 4.7459f;
    } else if (std::strstr(path, "deadwood")) {
        asset->minimum[2] = -.52754f; asset->maximum[2] = .52754f;
    } else if (std::strstr(path, "broadleaf")) {
        asset->minimum[0] = -2.88794f; asset->maximum[0] = 3.53819f;
        asset->minimum[2] = -2.98674f; asset->maximum[2] = 6.36932f;
    }
    struct StartingBounds {const char *stem;float width,height,depth;};
    const StartingBounds starting[]={
        // Conservative imported bounds for CPU parser/collision fixtures. The
        // real GL fixture independently loads every actual GLB bound.
        {"purchased_village_house_a",10.f,15.f,9.f},{"purchased_village_house_b",10.f,15.f,9.f},
        {"purchased_village_stall_a",8.f,8.f,8.f},{"purchased_village_stall_b",8.f,8.f,8.f},
        {"purchased_village_house_c",12.510144f,13.245809f,18.491389f},
        {"purchased_village_house_d",10.237870f,17.239583f,18.619708f},
        {"purchased_village_house_e",10.587148f,9.659044f,5.794844f},
        {"purchased_village_house_f",4.107198f,16.736516f,4.152300f},
        {"purchased_village_stall_c",5.687575f,6.416613f,7.033214f},
        {"purchased_village_stall_d",4.323226f,3.955082f,5.140752f},
        {"purchased_village_castle",107.877198f,67.751878f,100.372499f},
        {"purchased_village_gate",6.031004f,24.982034f,10.489822f},
        {"purchased_village_wall",1.748113f,17.959060f,19.991455f},
        {"purchased_village_barrel",0.796994f,1.024315f,0.798423f},
        {"purchased_village_crate",1.013537f,0.995082f,1.013773f},
        {"purchased_goods_bread",.43f,.17f,.43f},{"purchased_goods_cheese",.31f,.16f,.31f},
        {"purchased_goods_chicken",.41f,.21f,.41f},{"purchased_goods_tankard",.27f,.25f,.20f},
        {"purchased_prop_lamp",1.207913f,3.063372f,.312797f},
        {"purchased_prop_flag_a",.398360f,1.350569f,.964010f},
        {"purchased_prop_flag_b",.450072f,1.589410f,1.070640f},
        {"purchased_prop_sack",.613180f,.880983f,.696440f},
        {"purchased_prop_logs",.609620f,1.360289f,2.219714f},
        {"purchased_prop_rope",.061788f,.058764f,.664334f},
        {"purchased_prop_barrel_small",.479480f,.579221f,.486148f},
        {"starting_gate",7.410238f,8.616226f,2.671970f},
        {"starting_wall",7.410239f,7.793655f,2.549778f},
        {"starting_tower",15.838447f,13.498152f,15.838447f},
        {"starting_platform",2.523872f,.079775f,2.202920f},
        {"starting_table",2.240976f,.745987f,3.022330f},
        {"starting_crate",.825273f,.349618f,.408954f},
        {"starting_barrel",.743f,.916781f,.743f},
        {"starting_bucket",.366971f,.549862f,.342221f},
        {"starting_lantern",.221048f,.530137f,.234724f},
        {"starting_axe",.050904f,.704205f,.225737f},
        {"starting_stump",1.424872f,.574167f,1.589672f},
        {"starting_firepit",1.448496f,.388326f,1.433234f},
        {"starting_celandine",.275727f,.185852f,.228236f},
        {"starting_wildflower",.342991f,.268670f,.273048f},
        {"village_house_a",9.893792f,12.0068f,11.970f},
        {"village_house_b",11.892f,14.f,13.970f},
        {"village_inn",17.869f,16.f,13.970f},
        {"village_smithy",11.862f,10.f,9.970f},
        {"village_stall_red",4.f,3.3f,3.f},{"village_stall_blue",4.f,3.3f,3.f},{"village_stall_gold",4.f,3.3f,3.f},
        {"village_keep",36.f,35.f,28.f},{"village_well",3.f,3.8f,3.f},
        {"village_banner",1.6f,4.8f,.5f},{"village_cart",3.2f,2.f,1.8f}
    };
    for(const auto &b:starting)if(std::strstr(path,b.stem)) {
        asset->minimum[0]=-b.width*.5f;asset->maximum[0]=b.width*.5f;
        asset->minimum[2]=-b.depth*.5f;asset->maximum[2]=b.depth*.5f;
        asset->maximum[1]=asset->physical_height=b.height;break;
    }
    if(std::strstr(path,"showcase_source_large")){
        asset->minimum[0]=0;asset->maximum[0]=150;asset->minimum[2]=-2;asset->maximum[2]=2;
    }else if(std::strstr(path,"showcase_source_horizon")){
        asset->minimum[0]=-9000;asset->maximum[0]=0;asset->minimum[2]=-500;asset->maximum[2]=500;asset->maximum[1]=asset->physical_height=400;
    }
    ++active_models; return asset;
}
extern "C" bool world_model_bounds(const WorldModelAsset *asset, float minimum[3], float maximum[3]) {
    std::copy(asset->minimum, asset->minimum + 3, minimum);
    std::copy(asset->maximum, asset->maximum + 3, maximum);
    return true;
}
extern "C" bool world_model_upload_instances(WorldModelAsset *asset, const WorldModelInstance *instances, std::size_t count) {
    assert(count <= 4096);
    if (fail_upload) { fail_upload = false; return false; }
    if(fail_upload_after==0){fail_upload_after=-1;return false;}
    if(fail_upload_after>0)--fail_upload_after;
    if(capture_sort_uploads){assert(count<=sort_uploaded.size());sort_uploaded_count=count;std::copy(instances,instances+count,sort_uploaded.begin());}
    for (std::size_t k = 0; k < count; ++k) {
        const auto &a = instances[k];
        assert(std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z) && std::isfinite(a.yaw) && a.scale > 0);
        asset->instances[k] = a;
    }
    asset->count = count; return true;
}
extern "C" void world_model_free(WorldModelAsset *asset) {if(!asset)return;assert(active_models); --active_models; delete asset; }
namespace world_model_cpu {
std::shared_ptr<const Asset> prepare(const char *path,const Options &,std::string &error){error.clear();if(fail_stream_prepare.load()){error="CPU prepare fixture failure";return {};}auto asset=std::make_shared<Asset>();asset->cpu_bytes=sizeof(Asset);asset->geometry_bytes=asset->upload_bytes=96;asset->minimum[0]=asset->minimum[2]=-6;asset->maximum[0]=asset->maximum[2]=6;asset->maximum[1]=25;if(std::strstr(path,"showcase_source_large")){asset->minimum[0]=0;asset->maximum[0]=150;asset->minimum[2]=-2;asset->maximum[2]=2;asset->maximum[1]=3;}return asset;}
bool bounds_only(const char *path,const Options &,Bounds &bounds,std::string &error){error.clear();if(std::strstr(path,"showcase_source_large")){bounds.minimum[0]=0;bounds.maximum[0]=150;bounds.minimum[2]=-2;bounds.maximum[2]=2;bounds.maximum[1]=3;return true;}if(std::strstr(path,"showcase_source_horizon")){bounds.minimum[0]=-9000;bounds.minimum[2]=-500;bounds.maximum[0]=0;bounds.maximum[2]=500;bounds.maximum[1]=400;return true;}assert(std::strstr(path,"cinematic_pine_near"));bounds.minimum[0]=bounds.minimum[2]=-6;bounds.maximum[0]=bounds.maximum[2]=6;bounds.maximum[1]=25;return true;}
}
struct WorldModelStaged {WorldModelAsset *asset=nullptr;std::size_t offset=0;};
WorldModelStaged *world_model_stage(std::shared_ptr<const world_model_cpu::Asset>,unsigned,unsigned){auto *s=new WorldModelStaged;s->asset=new WorldModelAsset;++active_models;return s;}
bool world_model_stage_step(WorldModelStaged *s,std::size_t budget,std::size_t *consumed,bool *complete){*consumed=std::min(budget,96-s->offset);s->offset+=*consumed;*complete=s->offset==96;return true;}
WorldModelAsset *world_model_stage_take(WorldModelStaged *s){auto *a=s->asset;s->asset=nullptr;return a;}
void world_model_stage_free(WorldModelStaged *s){if(s){world_model_free(s->asset);delete s;}}
std::size_t world_model_geometry_bytes(const WorldModelAsset *asset){return asset?96:0;}
std::size_t world_model_staged_new_texture_bytes(const WorldModelStaged *){return 0;}
bool world_model_warm(WorldModelAsset *,unsigned){return true;}
extern "C" void world_model_draw_instances(WorldModelAsset *asset, unsigned program, std::size_t count) {
    count = std::min(count, asset->count);
    if (!count) return;
    last_draw_asset=asset;last_draw_path=asset->path;last_draw_count=count;
    if(last_prop_lod[2]==1)++prop_near_draws;
    if(last_prop_lod[2]==2)++prop_mid_draws;
    assert(asset && program && asset->count && std::isfinite(last_offset[0]));
    assert(std::abs(last_tree_height - asset->physical_height) < .001f);
    if(capture_draws) {
        assert(captured_draws.count<captured_draws.records.size());
        auto &record=captured_draws.records[captured_draws.count++];
        record.asset=asset;record.count=count;
        std::copy(last_anchor,last_anchor+2,record.anchor);
        std::copy(last_offset,last_offset+3,record.offset);
        std::copy(last_prop_lod,last_prop_lod+3,record.prop_lod);
        record.tree_height=last_tree_height;record.vegetation=last_vegetation;record.lod=last_lod;record.source_distance=last_source_distance;
    }
    if (check_single_root) {
        assert(asset->count == 1);
        const auto &a = asset->instances[0];
        assert(std::abs(a.x + last_anchor[0] - expected_x) < .002);
        assert(std::abs(a.z + last_anchor[1] - expected_z) < .002);
        double expected_y = world_atlas_height(expected_x, expected_z) - .025;
        if (last_lod == 2) expected_y -= .75 * a.scale;
        assert(std::abs(a.y - expected_y) < .002);
        assert(last_vegetation == (last_lod == 2 ? 4 : 1));
    }
    ++draws;
    drawn_instances += count;
}
extern "C" void world_model_draw(WorldModelAsset *asset, unsigned program) {
    world_model_draw_instances(asset, program, asset->count);
}
extern "C" GLint glGetUniformLocation(GLuint, const GLchar *name) {
    ++uniform_queries;
    const char *names[] = {"uOffset", "uWorldAnchor", "uGrass", "uVegetation", "uLodLevel", "uWind", "uTreeHeight", "uPropLodRange", "uSceneryDistanceFade", "uStudioTintRough", "uStudioNormalUV", "uStudioLodPreview", "uStudioGraphIndex", "uSurfaceBinding", "uSourceChunkDistance"};
    for (int k = 0; k < 15; ++k) if (!std::strcmp(names[k], name)) return k + 1;
    assert(false); return -1;
}
extern "C" void glGetIntegerv(GLenum name,GLint *value){assert(name==GL_MAX_TEXTURE_SIZE);*value=8192;}
extern "C" void glUseProgram(GLuint program) { assert(program); }
extern "C" void glUniform1i(GLint location, GLint value) {
    if (location == 4) last_vegetation = value;
    if (location == 5) last_lod = value;
    if (location == 3) assert(value == 0);
    if(location==15)last_source_distance=value;
}
extern "C" void glUniform1f(GLint location, GLfloat value) {
    assert(std::isfinite(value)); if (location == 7) last_tree_height = value;
}
extern "C" void glUniform2f(GLint location, GLfloat x, GLfloat z) {
    if(location==9){last_distance_fade[0]=x;last_distance_fade[1]=z;}
    if(location==9||location==11){assert(std::isfinite(x)&&std::isfinite(z));return;}
    assert(location == 2); last_anchor[0] = x; last_anchor[1] = z;
}
extern "C" void glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {assert(location==10);assert(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&std::isfinite(w));}
extern "C" void glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
    if(location==12){assert(std::isfinite(x)&&std::isfinite(y)&&(z==0||z==1));return;}
    if(location==8){last_prop_lod[0]=x;last_prop_lod[1]=y;last_prop_lod[2]=z;return;}
    assert(location == 1); last_offset[0] = x; last_offset[1] = y; last_offset[2] = z;
}

namespace {
void write(const std::string &path, const std::string &text) { std::ofstream out(path); assert(out); out << text; }
std::string record(double x, double z, const char *alias = "pine", const char *tail = "0 1 .025") {
    double u, v; world_atlas_world_to_uv(x, z, &u, &v);
    char text[256]; std::snprintf(text, sizeof(text), "%s %.12f %.12f %s\n", alias, u, v, tail); return text;
}
void rejects(const std::string &path, const std::string &text, const char *reason) {
    write(path, text); char error[512] = {};
    WorldScenery *scene = world_scenery_load(path.c_str(), "models", error, sizeof(error));
    assert(!scene && std::strstr(error, reason) && active_models == 0);
}
DrawCapture capture(WorldScenery *scene,double x,double y,double z,bool shadow,
                    const char *only=nullptr,const char *exclude=nullptr,bool legacy=false) {
    captured_draws={};capture_draws=true;
    if(legacy)world_scenery_draw(scene,shadow?23:17,x,y,z,shadow);
    else world_scenery_draw_filtered(scene,shadow?23:17,x,y,z,shadow,only,exclude);
    capture_draws=false;
    assert(last_prop_lod[0]==0&&last_prop_lod[1]==0&&last_prop_lod[2]==0);
    return captured_draws;
}
bool same_draw(const DrawRecord &a,const DrawRecord &b) {
    return a.asset==b.asset&&a.count==b.count&&a.tree_height==b.tree_height&&
        a.vegetation==b.vegetation&&a.lod==b.lod&&
        std::equal(a.anchor,a.anchor+2,b.anchor)&&std::equal(a.offset,a.offset+3,b.offset)&&
        std::equal(a.prop_lod,a.prop_lod+3,b.prop_lod);
}
void same_capture(const DrawCapture &a,const DrawCapture &b) {
    assert(a.count==b.count);
    for(std::size_t k=0;k<a.count;++k)assert(same_draw(a.records[k],b.records[k]));
}
void exact_partition(const DrawCapture &all,const DrawCapture &only,const DrawCapture &except) {
    assert(all.count==only.count+except.count);
    for(std::size_t k=0;k<all.count;++k) {
        std::size_t matches=0;
        for(std::size_t j=0;j<only.count;++j)matches+=same_draw(all.records[k],only.records[j]);
        for(std::size_t j=0;j<except.count;++j)matches+=same_draw(all.records[k],except.records[j]);
        assert(matches==1);
    }
}
}

int main(int argc, char **argv) {
    if (argc > 1) assert(chdir(argv[1]) == 0);
    assert(world_atlas_init());
    unsetenv("VALWOOD_DISABLE_SHADOW_LOD");
    char temp[192]; std::snprintf(temp, sizeof(temp), "/tmp/valwood-scenery-test-%ld.txt", long(getpid()));
    std::string path(temp);
    std::size_t count; const auto *trail = world_atlas_trail(&count); assert(count == 9);
    double dx = trail[2].x - trail[1].x, dz = trail[2].z - trail[1].z, length = std::hypot(dx, dz);
    expected_x = trail[1].x - dz / length * 35;
    expected_z = trail[1].z + dx / length * 35;
    std::string valid = record(expected_x, expected_z);
    rejects(path, "../../escape .5 .5 0 1 0\n", "unknown model alias");
    rejects(path, "pine 1.1 .5 0 1 0\n", "out-of-range");
    rejects(path, record(expected_x, expected_z, "pine", "0 0 0"), "out-of-range");
    rejects(path, record(expected_x, expected_z, "pine", "0 1 -1"), "out-of-range");
    rejects(path, record(expected_x, expected_z, "fern0", "0 1 .5"), "out-of-range");
    rejects(path, record(expected_x, expected_z, "pine", "0 1 0 extra"), "expected alias");
    rejects(path, "pine nan .2 0 1 0\n", "expected alias");
    rejects(path, "pine .5\n", "expected alias");
    rejects(path, std::string(257, '#') + "\n", "256 characters");
    rejects(path, "# no placements\n", "no placements");
    rejects(path, valid + valid, "duplicate");
    rejects(path, record(trail[1].x, trail[1].z), "intrudes");
    rejects(path, "rock0 .02 .9 0 1 0\n", "dry");
    std::string overflow;
    for (unsigned k = 0; k < 4097; ++k) overflow += record(expected_x + k * .02, expected_z);
    rejects(path, overflow, "4096-instance alias");
    write(path, valid); fail_asset = "pine_mid";
    char error[512] = {};
    assert(!world_scenery_load(path.c_str(), "models", error, sizeof(error)) && std::strstr(error, "Required model") && !active_models);
    fail_asset = nullptr;

    write(path, "# valid strict data\n" + valid);
    WorldScenery *scene = world_scenery_load(path.c_str(), "models", error, sizeof(error)); assert(scene && !error[0]);
    assert(world_scenery_stats(scene).loaded_groups == 3);
    double camera_y = world_atlas_height(expected_x, expected_z) + 1.75;
    check_single_root = true;
    const double distances[] = {0, 70, 100, 240, 300};
    for (double distance : distances) {
        allocations = 0; track_allocations = true;
        world_scenery_update(scene, expected_x + distance, camera_y, expected_z, -1.5707963f, 0);
        track_allocations = false; assert(!allocations);
        auto stats = world_scenery_stats(scene);
        assert(stats.near_trees == std::size_t(distance < WORLD_TREE_NEAR_FADE_END));
        assert(stats.mid_trees == std::size_t(distance >= WORLD_TREE_NEAR_FADE_BEGIN && distance < 265));
        assert(stats.far_trees == std::size_t(distance >= 215));
        assert(stats.largest_batch <= 1);
        world_scenery_draw(scene, 17, expected_x + distance, camera_y, expected_z);
        world_scenery_draw(scene, 19, expected_x + distance, camera_y, expected_z, true);
        world_scenery_update(scene, expected_x + distance, camera_y, expected_z, -1.5707963f, 0);
        assert(world_scenery_stats(scene).uploads_this_update == 0);
    }
    assert(uniform_queries == 30 && draws > 0);

    // Offscreen near trees remain in the caster buffer but no longer reach
    // color draws. Changing visibility and aspect must not allocate.
    const double selection_x = expected_x, selection_z = expected_z + 40;
    const float selection_y = float(camera_y + 10.75);
    world_scenery_update(scene, selection_x, selection_y, selection_z, 0, 0);
    assert(world_scenery_stats(scene).color_instances == 1 && world_scenery_stats(scene).shadow_instances == 1);
    drawn_instances = 0; world_scenery_draw(scene, 17, selection_x, selection_y, selection_z);
    assert(drawn_instances == 1);
    world_scenery_update(scene, selection_x, selection_y, selection_z, 3.14159265f, 0);
    auto offscreen = world_scenery_stats(scene);
    assert(offscreen.near_trees == 1 && !offscreen.color_instances && offscreen.shadow_instances == 1);
    drawn_instances = 0; world_scenery_draw(scene, 17, selection_x, selection_y, selection_z);
    assert(!drawn_instances);
    world_scenery_draw(scene, 19, selection_x, selection_y, selection_z, true);
    assert(drawn_instances == 1);
    allocations = 0; track_allocations = true;
    for (int frame = 0; frame < 120; ++frame)
        world_scenery_update(scene, selection_x, selection_y, selection_z,
                             frame % 2 ? 0.f : 3.14159265f, 0, frame % 3 ? .7f : 2.4f);
    track_allocations = false; assert(!allocations);

    // A failed upload retains the previous batch's anchor; publishing the new
    // anchor early would teleport unchanged GPU instances by 128 meters.
    world_scenery_update(scene, expected_x, camera_y, expected_z, 0, 0);
    double new_camera_x = std::floor(expected_x / 128.) * 128. + 129.;
    assert(std::abs(new_camera_x - expected_x) < 82);
    fail_upload = true;
    world_scenery_update(scene, new_camera_x, camera_y, expected_z, 0, 0);
    world_scenery_draw(scene, 17, new_camera_x, camera_y, expected_z);
    world_scenery_update(scene, new_camera_x, camera_y, expected_z, 0, 0);
    world_scenery_draw(scene, 17, new_camera_x, camera_y, expected_z);
    world_scenery_free(scene); assert(!active_models); check_single_root = false;

    scene = world_scenery_load("projects/valwood_world/scenery/authored_instances.txt", "models", error, sizeof(error));
    if (!scene) std::fprintf(stderr, "Authored file: %s\n", error);
    assert(scene);
    auto stats = world_scenery_stats(scene);
    assert(stats.source_instances == 283 && stats.source_trees == 26 && stats.source_props == 257 && stats.loaded_groups == 21);
    float max_support_gap = -1.e30f, old_cliff_gap = -1.e30f;
    std::size_t rigid_count = 0;
    for (std::size_t k = 0; k < stats.source_instances; ++k) {
        WorldSceneryPlacement a; assert(world_scenery_placement(scene, k, &a));
        if (!a.rigid) continue;
        ++rigid_count;
        double c = std::cos(a.yaw), s = std::sin(a.yaw);
        for (int iz = 0; iz <= 8; ++iz) for (int ix = 0; ix <= 32; ++ix) {
            double x = (a.minimum[0] + ix / 32. * (a.maximum[0] - a.minimum[0])) * a.scale;
            double z = (a.minimum[2] + iz / 8. * (a.maximum[2] - a.minimum[2])) * a.scale;
            float terrain = world_atlas_height(a.x + c * x + s * z, a.z - s * x + c * z);
            max_support_gap = std::max(max_support_gap, a.y - terrain);
            if (!std::strcmp(a.alias, "cliff"))
                old_cliff_gap = std::max(old_cliff_gap, world_atlas_height(a.x, a.z) - a.burial - terrain);
        }
    }
    assert(rigid_count == 89 && max_support_gap <= .015f && old_cliff_gap > 2.f);
    assert(!world_scenery_placement(scene, stats.source_instances, nullptr));
    std::printf("Rigid grounding: %zu props, worst sampled support gap %.3fm; old center-only cliff gap %.3fm\n", rigid_count, max_support_gap, old_cliff_gap);
    allocations = 0; track_allocations = true;
    for (int k = 0; k < 120; ++k)
        world_scenery_update(scene, trail[1].x + k * .1, camera_y, trail[1].z, -.55f + k * .01f, .025f);
    track_allocations = false; assert(!allocations);
    stats = world_scenery_stats(scene); assert(stats.props && stats.near_trees && stats.largest_batch <= 4096);
    world_scenery_draw(scene, 17, trail[1].x + 11.9, camera_y, trail[1].z);
    world_scenery_free(scene); assert(!active_models);
    // New fixed aliases, optional stacking elevation and per-source accounting.
    std::string second=path+"-second";
    rejects(path,record(expected_x,expected_z,"starting_gate","0 1 .30 .5"),"out-of-range");
    rejects(path,record(expected_x,expected_z,"starting_lantern","0 1 0 3.01"),"out-of-range");
    rejects(path,record(expected_x,expected_z,"starting_lantern","0 1 0 -1"),"out-of-range");
    rejects(path,record(expected_x,expected_z,"starting_lantern","0 1 0 1 extra"),"vertical offset");
    auto load_one=[&](const char *alias,const char *tail="0 1 0") {
        write(path,record(expected_x,expected_z,alias,tail));
        WorldScenery *result=world_scenery_load(path.c_str(),"models",error,sizeof(error));
        if(!result)std::fprintf(stderr,"New alias failure: %s\n",error);
        assert(result);return result;
    };
    scene=load_one("starting_lantern","0 1 0 1.25");
    WorldSceneryPlacement pose;assert(world_scenery_placement(scene,0,&pose));
    assert(pose.elevation==1.25f&&pose.rigid&&!pose.foundation);
    assert(std::abs(pose.y-(world_atlas_height(expected_x,expected_z)+1.20f))<.1f);
    world_scenery_free(scene);assert(!active_models);
    scene=load_one("starting_platform","37 2 .005");assert(world_scenery_placement(scene,0,&pose));
    assert(pose.foundation&&!pose.rigid&&world_scenery_stats(scene).source_colliders==0);
    assert(std::abs(pose.y+pose.maximum[1]*pose.scale-(world_atlas_height(pose.x,pose.z)+.015f))<.0001f);
    assert(world_scenery_can_move(scene,pose.x,pose.z-10,pose.x,pose.z+10,float(world_atlas_height(pose.x,pose.z))));
    world_scenery_free(scene);assert(!active_models);

    // Submitted visible instances must be front-to-back even when authored in
    // a different order. An equally close pair has a deterministic position
    // tie-break; a nearby behind-camera caster stays in the hidden suffix.
    const double sort_x=std::round(expected_x+200),sort_z=std::round(expected_z+60);
    write(path,record(sort_x,sort_z-35,"starting_barrel","0 1 0")+
        record(sort_x+3,sort_z-20,"starting_barrel","0 1 0")+
        record(sort_x-3,sort_z-20,"starting_barrel","0 1 0")+
        record(sort_x,sort_z-8,"starting_barrel","0 1 0")+
        record(sort_x,sort_z+8,"starting_barrel","0 1 0"));
    scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    const double sort_y=world_atlas_height(sort_x,sort_z)+1.75;
    capture_sort_uploads=true;sort_uploaded_count=0;allocations=0;track_allocations=true;
    world_scenery_update(scene,sort_x,sort_y,sort_z,0,0);
    track_allocations=false;capture_sort_uploads=false;
    assert(!allocations&&sort_uploaded_count==5);
    const auto sorted_stats=world_scenery_stats(scene);
    assert(sorted_stats.color_instances==4&&sorted_stats.shadow_instances==5&&sorted_stats.largest_batch==5);
    const double sort_anchor_x=std::floor(sort_x/128)*128,sort_anchor_z=std::floor(sort_z/128)*128;
    const double expected_sorted[5][2]={{sort_x,sort_z-8},{sort_x-3,sort_z-20},{sort_x+3,sort_z-20},{sort_x,sort_z-35},{sort_x,sort_z+8}};
    for(std::size_t k=0;k<5;++k)assert(std::abs(sort_uploaded[k].x+sort_anchor_x-expected_sorted[k][0])<.002&&
        std::abs(sort_uploaded[k].z+sort_anchor_z-expected_sorted[k][1])<.002);
    auto before_sort_draw=drawn_instances;world_scenery_draw(scene,17,sort_x,sort_y,sort_z);
    assert(drawn_instances-before_sort_draw==4);
    before_sort_draw=drawn_instances;world_scenery_draw(scene,23,sort_x,sort_y,sort_z,true);
    assert(drawn_instances-before_sort_draw==5);
    allocations=0;track_allocations=true;
    for(int k=0;k<60;++k){world_scenery_update(scene,sort_x,sort_y,sort_z,0,0);assert(!world_scenery_stats(scene).uploads_this_update);}
    track_allocations=false;assert(!allocations);world_scenery_free(scene);assert(!active_models);

    // Global color groups use nearest visible source bounds, not registry
    // order or off-camera shadow-only roots. The shadow pass retains registry
    // order, filters are exact per call, reflected cameras can reverse order,
    // and neither sorting nor repeat draws allocate or upload instances.
    double group_x,group_z;world_atlas_village_origin(&group_x,&group_z);
    group_x-=85;group_z+=20;
    const double group_y=world_atlas_height(group_x,group_z)+1.75;
    write(path,record(group_x,group_z-48,"starting_wall","0 1 0")+
        record(group_x,group_z+8,"starting_wall","0 .15 0")+
        record(group_x,group_z-20,"starting_table","0 1 0")+
        record(group_x,group_z-8,"starting_barrel","0 1 0"));
    scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    world_scenery_update(scene,group_x,group_y,group_z,0,0);
    auto group_stats=world_scenery_stats(scene);
    assert(group_stats.color_instances==3&&group_stats.shadow_instances==4);
    auto ordered=capture(scene,group_x,group_y,group_z,false);
    assert(ordered.count==3);
    assert(std::strstr(ordered.records[0].asset->path,"starting_barrel.glb"));
    assert(std::strstr(ordered.records[1].asset->path,"starting_table.glb"));
    assert(std::strstr(ordered.records[2].asset->path,"starting_wall.glb"));
    auto shadow_registry=capture(scene,group_x,group_y,group_z,true);
    assert(shadow_registry.count==3&&shadow_registry.records[0].count==2);
    assert(std::strstr(shadow_registry.records[0].asset->path,"starting_wall.glb"));
    assert(std::strstr(shadow_registry.records[1].asset->path,"starting_table.glb"));
    assert(std::strstr(shadow_registry.records[2].asset->path,"starting_barrel.glb"));
    exact_partition(ordered,capture(scene,group_x,group_y,group_z,false,"starting_table"),
        capture(scene,group_x,group_y,group_z,false,nullptr,"starting_table"));
    const auto reflected_order=capture(scene,group_x,group_y,group_z-65,false);
    assert(reflected_order.count==3&&std::strstr(reflected_order.records[0].asset->path,"starting_wall.glb")&&
        std::strstr(reflected_order.records[2].asset->path,"starting_barrel.glb"));
    same_capture(shadow_registry,capture(scene,group_x,group_y,group_z,true));
    allocations=0;track_allocations=true;
    for(int repeat=0;repeat<60;++repeat){
        world_scenery_update(scene,group_x,group_y,group_z,0,0);
        assert(!world_scenery_stats(scene).uploads_this_update);
        same_capture(ordered,capture(scene,group_x,group_y,group_z,false));
        same_capture(shadow_registry,capture(scene,group_x,group_y,group_z,true));
    }
    track_allocations=false;assert(!allocations);
    world_scenery_free(scene);assert(!active_models);

    // A farther root with a larger/scaled source can cover a nearer small
    // prop. Bound distance must win over root distance. Equal-distance groups
    // retain alias then LOD ordering, independent of authoring line order.
    write(path,record(group_x,group_z-17,"starting_barrel","0 1 0")+
        record(group_x,group_z-24,"starting_wall","0 3 0")+
        record(group_x,group_z-20,"starting_table","0 1 0"));
    scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    world_scenery_update(scene,group_x,group_y,group_z,0,0);
    ordered=capture(scene,group_x,group_y,group_z,false);
    assert(ordered.count==3&&std::strstr(ordered.records[0].asset->path,"starting_wall.glb")&&
        std::strstr(ordered.records[1].asset->path,"starting_barrel.glb")&&
        std::strstr(ordered.records[2].asset->path,"starting_table.glb"));
    world_scenery_free(scene);assert(!active_models);
    write(path,record(group_x+.10,group_z-.10,"starting_lantern","0 1 0")+
        record(group_x-.10,group_z-.10,"starting_bucket","0 1 0"));
    scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    const double low_camera=world_atlas_height(group_x,group_z)+.2;
    world_scenery_update(scene,group_x,low_camera,group_z,0,0);
    ordered=capture(scene,group_x,low_camera,group_z,false);
    assert(ordered.count==2&&std::strstr(ordered.records[0].asset->path,"starting_bucket.glb")&&
        std::strstr(ordered.records[1].asset->path,"starting_lantern.glb"));
    world_scenery_free(scene);assert(!active_models);
    std::puts("PASS scenery color group ordering: nearest visible actual bounds/scale, hidden-caster exclusion, deterministic ties, reflected cameras, exact filters, unchanged shadow order and zero hot allocations/uploads");

    // Cube-face capture needs a genuine +/-45 degree vertical frustum. A
    // source 40m below the camera and 50m forward misses the normal half-angle
    // but belongs to a 90 degree face. All nearby shadows remain selected.
    write(path,record(group_x,group_z,"starting_barrel","0 1 0"));
    scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    assert(world_scenery_placement(scene,0,&pose));
    const double cube_y=pose.y+(pose.minimum[1]+pose.maximum[1])*.5*pose.scale+40;
    const double cube_z=pose.z+50;
    world_scenery_update(scene,pose.x,cube_y,cube_z,0,0,1.f);
    const auto default_color=capture(scene,pose.x,cube_y,cube_z,false);
    const auto default_shadow=capture(scene,pose.x,cube_y,cube_z,true);
    assert(!default_color.count&&default_shadow.count==1);
    world_scenery_update(scene,pose.x,cube_y,cube_z,0,0,1.f,0.f);
    same_capture(default_color,capture(scene,pose.x,cube_y,cube_z,false));
    same_capture(default_shadow,capture(scene,pose.x,cube_y,cube_z,true));
    assert(!world_scenery_stats(scene).uploads_this_update);
    world_scenery_update(scene,pose.x,cube_y,cube_z,0,0,1.f,1.f);
    const auto cube_color=capture(scene,pose.x,cube_y,cube_z,false);
    assert(cube_color.count==1&&cube_color.records[0].count==1);
    same_capture(default_shadow,capture(scene,pose.x,cube_y,cube_z,true));
    // Both the vertical and horizontal corner slopes are 0.8: cube projection
    // accepts them together, whereas the original square view accepts neither.
    world_scenery_update(scene,pose.x-40,cube_y,cube_z,0,0,1.f,1.f);
    assert(world_scenery_stats(scene).color_instances==1);
    world_scenery_update(scene,pose.x-40,cube_y,cube_z,0,0,1.f);
    assert(!world_scenery_stats(scene).color_instances);
    // Restore the original camera-relative anchor before tracking projection
    // switches; camera translation may legitimately cross a 128m anchor cell.
    world_scenery_update(scene,pose.x,cube_y,cube_z,0,0,1.f);
    allocations=0;track_allocations=true;
    for(int frame=0;frame<60;++frame) {
        const bool cube=(frame%2)!=0;
        world_scenery_update(scene,pose.x,cube_y,cube_z,0,0,1.f,cube?1.f:0.f);
        assert(world_scenery_stats(scene).color_instances==std::size_t(cube));
        assert(!world_scenery_stats(scene).uploads_this_update);
        same_capture(cube?cube_color:default_color,capture(scene,pose.x,cube_y,cube_z,false));
        same_capture(default_shadow,capture(scene,pose.x,cube_y,cube_z,true));
    }
    track_allocations=false;assert(!allocations);
    world_scenery_free(scene);assert(!active_models);
    std::puts("PASS scenery cube faces: explicit 90 degree vertical/corner coverage, legacy projection unchanged, identical nearby shadow set and zero extra uploads/allocations");

    // Newly downloaded rainforest families use fixed paths and two LODs each.
    // Only the rigid root/stump footprints collide; photographed plants keep
    // their leaf shading and root grounding without becoming solid boxes.
    const char *showcase_aliases[]={"showcase_roots","showcase_stump","showcase_shrub","showcase_shrub_b","showcase_sorrel"};
    const auto *pool=world_atlas_rainforest_pool();assert(pool);
    rejects(path,record(pool->x,pool->z,"showcase_roots"),"dry terrain");
    std::string showcase_registry;
    for(std::size_t k=0;k<std::size(showcase_aliases);++k)showcase_registry+=record(expected_x+160+k*18,expected_z+40,showcase_aliases[k]);
    write(path,showcase_registry);scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    stats=world_scenery_stats(scene);assert(stats.source_instances==5&&stats.source_aliases==5&&stats.loaded_groups==10&&stats.source_colliders==2);
    for(std::size_t k=0;k<5;++k){assert(world_scenery_placement(scene,k,&pose));assert(pose.rigid==(k<2)&&!pose.foundation);}
    allocations=0;track_allocations=true;
    for(int frame=0;frame<60;++frame)world_scenery_update(scene,expected_x+195, camera_y,expected_z+65,frame*.01f,0);
    track_allocations=false;assert(!allocations);world_scenery_free(scene);assert(!active_models);

    // The two taller downloaded shrubs explicitly cast masked leaf shadows.
    // All legacy low plants and sorrel retain the cheap no-sun-shadow policy.
    const char *plant_shadow_aliases[]={"showcase_shrub","showcase_shrub_b","showcase_sorrel",
        "fern0","fern1","fern2","starting_celandine","starting_wildflower"};
    for(std::size_t k=0;k<std::size(plant_shadow_aliases);++k) {
        scene=load_one(plant_shadow_aliases[k]);
        world_scenery_update(scene,expected_x,world_atlas_height(expected_x,expected_z)+1.75,expected_z-18,0,0);
        const auto selected=world_scenery_stats(scene);
        assert(selected.color_instances==0&&selected.shadow_instances==std::size_t(k<2));
        const auto before=drawn_instances;
        world_scenery_draw(scene,23,expected_x,camera_y,expected_z-18,true);
        assert(drawn_instances-before==std::size_t(k<2)&&last_prop_lod[2]==0);
        if(k<2)for(float distance:{45.f,50.f,55.f}) {
            world_scenery_update(scene,expected_x,camera_y,expected_z+distance,0,0);
            assert(world_scenery_stats(scene).shadow_instances==std::size_t(distance==50?2:1));
        }
        world_scenery_free(scene);assert(!active_models);
    }

    // Only showcase props get the new overlap; both LOD batches are selected
    // within split +/-4m, and each shared shader is reset before returning.
    scene=load_one("showcase_roots");
    for(float distance:{60.f,65.f,70.f}) {
        world_scenery_update(scene,expected_x,world_atlas_height(expected_x,expected_z)+1.75,expected_z+distance,0,0);
        assert(world_scenery_stats(scene).props==std::size_t(distance==65?2:1));
        const unsigned near_before=prop_near_draws,mid_before=prop_mid_draws;
        world_scenery_draw(scene,17,expected_x,camera_y,expected_z+distance);
        assert(prop_near_draws-near_before==unsigned(distance<69));
        assert(prop_mid_draws-mid_before==unsigned(distance>=61));
        assert(last_prop_lod[0]==0&&last_prop_lod[1]==0&&last_prop_lod[2]==0);
        world_scenery_draw(scene,23,expected_x,camera_y,expected_z+distance,true);
        assert(last_prop_lod[2]==0);
    }
    world_scenery_free(scene);assert(!active_models);
    // Per-call filtering partitions the actual committed draw groups without
    // changing their visible prefixes, LOD fade uniforms or shadow proxies.
    // The roots alias crosses both sides of its near/mid overlap, while pine
    // and barrel supply independent excluded geometry and a shadow proxy.
    write(path,record(expected_x,expected_z,"showcase_roots")+
        record(expected_x+9,expected_z,"pine")+record(expected_x-8,expected_z,"starting_barrel"));
    scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    for(float distance:{60.f,65.f,70.f}) {
        const double filter_z=expected_z+distance;
        world_scenery_update(scene,expected_x,camera_y,filter_z,0,0);
        const auto all=capture(scene,expected_x,camera_y,filter_z,false,nullptr,nullptr,true);
        const auto shadow_all=capture(scene,expected_x,camera_y,filter_z,true,nullptr,nullptr,true);
        assert(all.count>2&&shadow_all.count>1);
        const auto queries_before=uniform_queries;
        allocations=0;track_allocations=true;
        for(int repeat=0;repeat<30;++repeat) {
            for(bool shadow:{false,true}) {
                const auto only=capture(scene,expected_x,camera_y,filter_z,shadow,"showcase_roots");
                const auto except=capture(scene,expected_x,camera_y,filter_z,shadow,nullptr,"showcase_roots");
                exact_partition(shadow?shadow_all:all,only,except);
                assert(only.count==std::size_t(distance==65.f?2:1));
                unsigned near_count=0,mid_count=0;
                for(std::size_t k=0;k<only.count;++k) {
                    const auto &draw=only.records[k];
                    assert(std::strstr(draw.asset->path,"showcase_roots"));
                    near_count+=draw.prop_lod[2]==1;mid_count+=draw.prop_lod[2]==2;
                }
                assert(near_count==unsigned(distance<69)&&mid_count==unsigned(distance>=61));
                for(std::size_t k=0;k<except.count;++k)
                    assert(!std::strstr(except.records[k].asset->path,"showcase_roots"));
                assert(!capture(scene,expected_x,camera_y,filter_z,shadow,"showcase_roots","showcase_roots").count);
                assert(!capture(scene,expected_x,camera_y,filter_z,shadow,"missing_alias").count);
                same_capture(shadow?shadow_all:all,capture(scene,expected_x,camera_y,filter_z,shadow,nullptr,nullptr,true));
            }
            // An empty last filter must not leak into either subsequent pass.
            (void)capture(scene,expected_x,camera_y,filter_z,false,"missing_alias");
            same_capture(all,capture(scene,expected_x,camera_y,filter_z,false));
            same_capture(shadow_all,capture(scene,expected_x,camera_y,filter_z,true));
        }
        track_allocations=false;assert(!allocations&&uniform_queries==queries_before);
    }
    world_scenery_free(scene);assert(!active_models);
    std::puts("PASS scenery draw filters: exact only/exclude partition, near/mid overlap counts and uniforms, unchanged later color/shadow draws, zero hot allocations/queries");
    // Separate mid-mesh shadow assets preserve full-detail color geometry.
    // Their instance VBOs are independent but receive byte-identical batches.
    const char *proxy_aliases[]={"pine","fir","broadleaf","pine_young","showcase_shrub","showcase_shrub_b"};
    for(std::size_t k=0;k<std::size(proxy_aliases);++k) {
        unsetenv("VALWOOD_DISABLE_SHADOW_LOD");
        scene=load_one(proxy_aliases[k]);
        assert(world_scenery_stats(scene).shadow_proxy_groups==1&&active_models==std::size_t(k<4?4:3));
        world_scenery_update(scene,expected_x,camera_y,expected_z+30,0,0);
        assert(world_scenery_stats(scene).color_instances==1&&world_scenery_stats(scene).shadow_instances==1);
        world_scenery_draw(scene,17,expected_x,camera_y,expected_z+30);
        const auto *original=last_draw_asset;assert(last_draw_count==1);
        assert((std::strstr(last_draw_path,"_near.glb")!=nullptr)==(k<4));
        assert(!std::strstr(last_draw_path,"_mid.glb"));
        world_scenery_draw(scene,23,expected_x,camera_y,expected_z+30,true);
        assert(last_draw_asset!=original&&std::strstr(last_draw_path,"_mid.glb")&&last_draw_count==1);
        const auto *proxy=last_draw_asset;
        assert(original->count==proxy->count&&!std::memcmp(original->instances.data(),proxy->instances.data(),original->count*sizeof(WorldModelInstance)));
        // Read the A/B switch only at load: changing the environment cannot
        // silently switch an existing scene's render resource mid-frame.
        setenv("VALWOOD_DISABLE_SHADOW_LOD","1",1);
        world_scenery_draw(scene,23,expected_x,camera_y,expected_z+30,true);assert(last_draw_asset==proxy);
        allocations=0;track_allocations=true;
        for(int frame=0;frame<30;++frame){world_scenery_update(scene,expected_x,camera_y,expected_z+30,0,0);assert(!world_scenery_stats(scene).uploads_this_update);}
        track_allocations=false;assert(!allocations);world_scenery_free(scene);assert(!active_models);
        scene=load_one(proxy_aliases[k]);assert(world_scenery_stats(scene).shadow_proxy_groups==1);
        world_scenery_update(scene,expected_x,camera_y,expected_z+30,0,0);
        world_scenery_draw(scene,17,expected_x,camera_y,expected_z+30);original=last_draw_asset;
        world_scenery_draw(scene,23,expected_x,camera_y,expected_z+30,true);assert(last_draw_asset==original);
        world_scenery_free(scene);assert(!active_models);
    }
    unsetenv("VALWOOD_DISABLE_SHADOW_LOD");
    fail_asset="showcase_shrub_mid";write(path,record(expected_x,expected_z,"showcase_shrub","0 1 0"));
    assert(!world_scenery_load(path.c_str(),"models",error,sizeof(error))&&std::strstr(error,"shadow proxy")&&!active_models);fail_asset=nullptr;
    // If proxy upload fails after main upload, restore the prior main batch
    // before publishing a new origin. Both resources must remain coherent.
    scene=load_one("showcase_shrub");world_scenery_update(scene,expected_x,camera_y,expected_z+30,0,0);
    world_scenery_draw(scene,17,expected_x,camera_y,expected_z+30);const auto *committed_main=last_draw_asset;
    world_scenery_draw(scene,23,expected_x,camera_y,expected_z+30,true);const auto *committed_proxy=last_draw_asset;
    const auto committed_instance=committed_main->instances[0];const auto committed_uploads=world_scenery_stats(scene).uploads_total;
    const double proxy_cut_x=std::floor(expected_x/128)*128+129;
    fail_upload_after=1;world_scenery_update(scene,proxy_cut_x,camera_y,expected_z+30,0,0);
    assert(world_scenery_stats(scene).uploads_total==committed_uploads);
    assert(committed_main->count==1&&committed_proxy->count==1&&
        !std::memcmp(committed_main->instances.data(),&committed_instance,sizeof(committed_instance))&&
        !std::memcmp(committed_proxy->instances.data(),&committed_instance,sizeof(committed_instance)));
    world_scenery_update(scene,proxy_cut_x,camera_y,expected_z+30,0,0);
    assert(world_scenery_stats(scene).uploads_total>committed_uploads&&committed_main->count==committed_proxy->count&&
        !std::memcmp(committed_main->instances.data(),committed_proxy->instances.data(),committed_main->count*sizeof(WorldModelInstance)));
    world_scenery_free(scene);assert(!active_models);

    // A nearby raised-pool mirror is distinct from the old sea-level mirror.
    // This elevated small prop lies outside both old frusta for this pitch.
    write(path,record(pool->x+30,pool->z,"starting_lantern","0 1 0 3"));
    scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    world_scenery_update(scene,pool->x+30,pool->surface_y+1.75,pool->z+20,0,-.8f);
    assert(world_scenery_stats(scene).color_instances==1);
    world_scenery_free(scene);assert(!active_models);

    const char *new_aliases[]={"starting_gate","starting_wall","starting_tower","starting_platform","starting_table",
        "starting_crate","starting_barrel","starting_bucket","starting_lantern","starting_axe","starting_stump",
        "starting_firepit","starting_celandine","starting_wildflower"};
    std::string registry;
    for(std::size_t k=0;k<std::size(new_aliases);k++)registry+=record(expected_x+50+k*18,expected_z+20,new_aliases[k]);
    write(path,valid);write(second,registry);const char *sources[]={path.c_str(),second.c_str()};
    scene=world_scenery_load_files(sources,2,"models",error,sizeof(error));assert(scene);
    stats=world_scenery_stats(scene);
    assert(stats.source_files==2&&stats.source_aliases==15&&stats.loaded_groups==21&&stats.source_instances==15);
    auto first_source=world_scenery_source_stats(scene,0),last_source=world_scenery_source_stats(scene,1);
    assert(first_source.instances==1&&first_source.trees==1&&first_source.props==0&&!std::strcmp(first_source.path,path.c_str()));
    assert(last_source.instances==14&&last_source.trees==0&&last_source.props==14&&!std::strcmp(last_source.path,second.c_str()));
    assert(!world_scenery_source_stats(scene,2).path&&world_scenery_placement(scene,1,&pose)&&pose.source_file==1);
    allocations=0;track_allocations=true;
    for(int frame=0;frame<120;frame++) {
        world_scenery_update(scene,expected_x+80+frame*.1,camera_y,expected_z+30,-1.5f,0);
        (void)world_scenery_can_move(scene,expected_x+35,expected_z-10,expected_x+35,expected_z+10,float(camera_y-1.75));
    }
    track_allocations=false;assert(!allocations);
    world_scenery_free(scene);assert(!active_models);
    // A third file shares an existing tree family while adding one understory
    // family. It must not duplicate the tree's LOD/model groups or ownership.
    const std::string third=path+"-third";
    write(third,record(expected_x+360,expected_z+35)+record(expected_x+380,expected_z+35,"fern0"));
    const char *three_sources[]={path.c_str(),second.c_str(),third.c_str()};
    scene=world_scenery_load_files(three_sources,3,"models",error,sizeof(error));assert(scene);
    stats=world_scenery_stats(scene);auto third_stats=world_scenery_source_stats(scene,2);
    assert(stats.source_files==3&&stats.source_instances==17&&stats.source_aliases==16&&stats.loaded_groups==22);
    assert(third_stats.instances==2&&third_stats.trees==1&&third_stats.props==1&&third_stats.aliases==2&&third_stats.colliders==1);
    assert(world_scenery_source_stats(scene,0).instances==1&&world_scenery_source_stats(scene,1).instances==14);
    allocations=0;track_allocations=true;
    for(int frame=0;frame<60;++frame)world_scenery_update(scene,expected_x+345+frame*.1,camera_y,expected_z+65,0,0);
    track_allocations=false;assert(!allocations);world_scenery_free(scene);assert(!active_models);
    write(third,valid);assert(!world_scenery_load_files(three_sources,3,"models",error,sizeof(error))&&std::strstr(error,"duplicate")&&!active_models);
    std::remove(third.c_str());
    fail_asset="starting_gate";assert(!world_scenery_load_files(sources,2,"models",error,sizeof(error))&&!active_models);fail_asset=nullptr;
    write(second,valid);assert(!world_scenery_load_files(sources,2,"models",error,sizeof(error))&&std::strstr(error,"duplicate")&&!active_models);
    assert(!world_scenery_load_files(nullptr,2,"models",error,sizeof(error))&&!active_models);

    // Same-root elevated stacks are intentional; exact duplicate elevations
    // still reject. At the cap, actual uploads remain allocation-free.
    write(path,record(expected_x,expected_z,"starting_crate","0 1 0")+record(expected_x,expected_z,"starting_crate","0 1 0 .349618"));
    scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    WorldSceneryPlacement lower,upper;assert(world_scenery_placement(scene,0,&lower)&&world_scenery_placement(scene,1,&upper));
    assert(std::abs(upper.y-(lower.y+lower.maximum[1]))<.0001f&&world_scenery_stats(scene).source_colliders==2);
    world_scenery_free(scene);assert(!active_models);
    rejects(path,record(expected_x,expected_z,"starting_crate")+record(expected_x,expected_z,"starting_crate","0 1 0 .005"),"duplicate");
    // Small floor support offsets are explicit per alias, not a blanket
    // elevation exemption. Support-rooted stumps also compensate their5cm
    // grounding contact slack to meet the same measured hall floor.
    for(const auto &floor:std::array<std::pair<const char*,float>,3>{{
        {"starting_table",.05f},{"purchased_barrel",.05f},{"starting_stump",.1f}}}) {
        scene=load_one(floor.first,"0 1 0");assert(world_scenery_placement(scene,0,&pose));
        const float grounded_y=pose.y;world_scenery_free(scene);assert(!active_models);
        char supported[64];std::snprintf(supported,sizeof(supported),"0 1 0 %.6f",floor.second);
        scene=load_one(floor.first,supported);assert(world_scenery_placement(scene,0,&pose));
        assert(std::fabs(pose.y-(grounded_y+floor.second))<.0001f);
        world_scenery_free(scene);assert(!active_models);
        std::snprintf(supported,sizeof(supported),"0 1 0 %.6f",floor.second+.001f);
        rejects(path,record(expected_x,expected_z,floor.first,supported),"out-of-range");
    }
    rejects(path,record(expected_x,expected_z,"purchased_house_a","0 1 0 .001"),"out-of-range");
    std::string capacity_records;
    for(unsigned k=0;k<4096;k++)capacity_records+=record(expected_x+(k%64)*.02,expected_z+(k/64)*.02,"starting_celandine","0 1 0");
    write(path,capacity_records);scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    assert(world_scenery_stats(scene).source_instances==4096&&world_scenery_stats(scene).source_colliders==0);
    allocations=0;track_allocations=true;
    world_scenery_update(scene,expected_x+.64,world_atlas_height(expected_x+.64,expected_z+25)+1.75,expected_z+25,0,0);
    track_allocations=false;assert(!allocations&&world_scenery_stats(scene).largest_batch==4096);
    world_scenery_free(scene);assert(!active_models);
    // 8192 combined records are legal while each alias retains its 4096 model
    // buffer. Both bounds span source files; neither resets at a file boundary.
    std::string second_capacity;
    for(unsigned k=0;k<4096;k++)second_capacity+=record(expected_x+(k%64)*.02,expected_z+(k/64)*.02,"starting_wildflower","0 1 0");
    write(second,second_capacity);const char *capacity_sources[]={path.c_str(),second.c_str()};
    scene=world_scenery_load_files(capacity_sources,2,"models",error,sizeof(error));assert(scene);
    assert(world_scenery_stats(scene).source_instances==8192&&world_scenery_stats(scene).source_files==2);
    assert(world_scenery_source_stats(scene,0).instances==4096&&world_scenery_source_stats(scene,1).instances==4096);
    allocations=0;track_allocations=true;
    world_scenery_update(scene,expected_x+.64,world_atlas_height(expected_x+.64,expected_z+10)+1.75,expected_z+10,0,0);
    track_allocations=false;assert(!allocations&&world_scenery_stats(scene).largest_batch==4096);
    world_scenery_free(scene);assert(!active_models);
    write(third,valid);const char *overflow_sources[]={path.c_str(),second.c_str(),third.c_str()};
    assert(!world_scenery_load_files(overflow_sources,3,"models",error,sizeof(error))&&
        std::strstr(error,"8192-record")&&!active_models);
    // Studio adds at most1024 placements without renumbering the8192 source namespace.
    std::string studio_records;
    for(unsigned k=0;k<1024;k++)studio_records+=record(expected_x+4+(k%32)*.02,expected_z+(k/32)*.02,"fern0","0 1 0");
    write(third,studio_records);
    scene=world_scenery_prepare_files(nullptr,overflow_sources,3,"models",error,sizeof(error));assert(scene);
    assert(world_scenery_stats(scene).source_instances==9216);
    world_scenery_free(scene);assert(!active_models);
    write(third,studio_records+valid);
    assert(!world_scenery_prepare_files(nullptr,overflow_sources,3,"models",error,sizeof(error))&&std::strstr(error,"9216-record")&&!active_models);
    write(second,record(expected_x+4,expected_z,"starting_celandine","0 1 0"));
    assert(!world_scenery_load_files(capacity_sources,2,"models",error,sizeof(error))&&
        std::strstr(error,"4096-instance alias")&&!active_models);
    std::remove(third.c_str());

    // A true continuous sweep catches tunneling even when both endpoints are
    // free. Gate collision uses measured piers and header, leaving its opening.
    scene=load_one("starting_gate","0 1 .30");assert(world_scenery_placement(scene,0,&pose));
    assert(world_scenery_stats(scene).source_colliders==3);
    auto gate_move=[&](double x,double az,double bz,float feet) {
        return world_scenery_can_move(scene,pose.x+x,pose.z+az,pose.x+x,pose.z+bz,feet);
    };
    float gate_floor=float(world_atlas_height(pose.x,pose.z));
    assert(gate_move(0,-10,10,gate_floor));assert(gate_move(1.35,-10,10,gate_floor));
    assert(!gate_move(1.55,-10,10,gate_floor));
    assert(gate_move(2.5,-100,-100,gate_floor)&&gate_move(2.5,100,100,gate_floor));
    assert(!gate_move(2.5,-100,100,gate_floor));
    assert(gate_move(0,-10,10,pose.y+4.2f-1.75f-.01f));
    assert(!gate_move(0,-10,10,pose.y+4.2f-1.75f+.01f));
    assert(gate_move(2.5,-10,10,pose.y+8.7f));
    assert(gate_move(0,0,0,gate_floor)&&!gate_move(2.5,0,0,gate_floor));
    allocations=0;track_allocations=true;
    auto old_queries=uniform_queries;
    for(int k=0;k<2000;k++)assert(gate_move(0,-100,100,gate_floor));
    track_allocations=false;assert(!allocations&&uniform_queries==old_queries);
    world_scenery_free(scene);assert(!active_models);

    double gate_x=trail[1].x+dx/length*20,gate_z=trail[1].z+dz/length*20;
    char gate_tail[96];std::snprintf(gate_tail,sizeof(gate_tail),"%.9f 1 .30",std::atan2(-dx,-dz)*180/3.14159265358979323846);
    write(path,record(gate_x,gate_z,"starting_gate",gate_tail));
    scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);
    assert(world_scenery_can_move(scene,gate_x-dx/length*5,gate_z-dz/length*5,
        gate_x+dx/length*5,gate_z+dz/length*5,float(world_atlas_height(gate_x,gate_z))));
    world_scenery_free(scene);assert(!active_models);
    std::snprintf(gate_tail,sizeof(gate_tail),"%.9f 1 .30",std::atan2(-dx,-dz)*180/3.14159265358979323846+90);
    rejects(path,record(gate_x,gate_z,"starting_gate",gate_tail),"Gate pier intrudes");

    scene=load_one("starting_gate","37 1.2 .30");assert(world_scenery_placement(scene,0,&pose));
    double c=std::cos(pose.yaw),sn=std::sin(pose.yaw);
    auto local_move=[&](double ax,double az,double bx,double bz,float feet) {
        return world_scenery_can_move(scene,pose.x+c*ax+sn*az,pose.z-sn*ax+c*az,
            pose.x+c*bx+sn*bz,pose.z-sn*bx+c*bz,feet);
    };
    assert(local_move(0,-10,0,10,float(world_atlas_height(pose.x,pose.z))));
    assert(!local_move(2.1,-10,2.1,10,float(world_atlas_height(pose.x,pose.z))));
    world_scenery_free(scene);assert(!active_models);

    scene=load_one("starting_wall","45 .6 0");assert(world_scenery_placement(scene,0,&pose));
    c=std::cos(pose.yaw);sn=std::sin(pose.yaw);float wall_floor=pose.y;
    assert(!local_move(0,-10,0,10,wall_floor));
    double xmax=pose.maximum[0]*pose.scale,zmax=pose.maximum[2]*pose.scale;
    assert(local_move(xmax+.33,-10,xmax+.33,10,wall_floor));
    // Rounded capsule corners: an expanded rectangle would falsely block.25/.25.
    assert(local_move(xmax+.25,zmax+.25,xmax+.25,zmax+.25,wall_floor));
    assert(!local_move(xmax+.20,zmax+.20,xmax+.20,zmax+.20,wall_floor));
    world_scenery_free(scene);assert(!active_models);
    scene=load_one("pine");assert(world_scenery_placement(scene,0,&pose));
    assert(!world_scenery_can_move(scene,pose.x-3,pose.z,pose.x+3,pose.z,pose.y));
    assert(world_scenery_can_move(scene,pose.x-3,pose.z+.8,pose.x+3,pose.z+.8,pose.y));
    assert(world_scenery_can_move(scene,pose.x+.2,pose.z,pose.x+.3,pose.z,pose.y));
    assert(!world_scenery_can_move(scene,pose.x+.2,pose.z,pose.x+.1,pose.z,pose.y));
    assert(world_scenery_runtime_active(scene,0,false));
    assert(world_scenery_can_move(scene,pose.x-3,pose.z,pose.x+3,pose.z,pose.y));
    assert(world_scenery_runtime_active(scene,0,true));
    assert(world_scenery_runtime_pose(scene,0,pose.x+20,pose.y+2,pose.z,pose.yaw));
    assert(world_scenery_can_move(scene,pose.x-3,pose.z,pose.x+3,pose.z,pose.y));
    assert(!world_scenery_can_move(scene,pose.x+17,pose.z,pose.x+23,pose.z,pose.y+2));
    assert(!world_scenery_runtime_pose(scene,0,NAN,pose.y,pose.z,0));
    const auto colliders_before_stream=world_scenery_stats(scene).source_colliders;
    assert(world_scenery_streaming(scene,true));
    world_scenery_update(scene,pose.x+5000,pose.y,pose.z,0,0);
    auto streamed=world_scenery_streaming_stats(scene);assert(streamed.enabled&&streamed.resident==2&&streamed.evictions==1&&active_models==2);
    assert(world_scenery_stats(scene).source_colliders==colliders_before_stream);
    assert(!world_scenery_can_move(scene,pose.x+17,pose.z,pose.x+23,pose.z,pose.y+2));
    world_scenery_update(scene,pose.x+20,pose.y+2,pose.z,0,0);assert(world_scenery_streaming_stats(scene).critical_pending==1);
    for(unsigned i=0;i<2000&&world_scenery_streaming_stats(scene).resident!=3;i++){world_scenery_update(scene,pose.x+20,pose.y+2,pose.z,0,0);std::this_thread::sleep_for(std::chrono::microseconds(100));}
    streamed=world_scenery_streaming_stats(scene);assert(streamed.resident==3&&streamed.loads==1&&!streamed.failed&&!streamed.critical_pending&&streamed.upload_bytes<=2*1024*1024&&active_models==4);
    // Stop returns committed GPU ownership to the scene, preserving every asset
    // through an editor transaction; scene destruction remains exactly once.
    assert(world_scenery_streaming(scene,false));assert(active_models==4&&!world_scenery_streaming_stats(scene).enabled);
    world_scenery_free(scene);assert(!active_models);
    const char *stream_files[]={path.c_str()};
    scene=world_scenery_load_files_streamed(stream_files,1,"models",pose.x+5000,pose.z,{},error,sizeof(error));assert(scene&&active_models==2);
    assert(world_scenery_stats(scene).source_colliders==colliders_before_stream);
    assert(!world_scenery_can_move(scene,pose.x-3,pose.z,pose.x+3,pose.z,pose.y));
    auto *stream_edit=world_scenery_prepare_files(scene,stream_files,1,"models",error,sizeof(error));assert(stream_edit&&active_models==2);
    assert(world_scenery_commit_prepared(stream_edit,scene));world_scenery_free(scene);scene=stream_edit;
    world_scenery_update(scene,pose.x+5000,pose.y,pose.z,0,0);assert(world_scenery_streaming_stats(scene).resident==2);
    fail_stream_prepare=true;
    for(unsigned i=0;i<2000&&!world_scenery_streaming_stats(scene).failed;i++){world_scenery_update(scene,pose.x,pose.y,pose.z,0,0);std::this_thread::sleep_for(std::chrono::microseconds(100));}
    assert(world_scenery_streaming_stats(scene).failed==1&&!world_scenery_streaming_stats(scene).critical_pending&&active_models==2);
    world_scenery_update(scene,pose.x+5000,pose.y,pose.z,0,0);fail_stream_prepare=false;
    for(unsigned i=0;i<2000&&world_scenery_streaming_stats(scene).resident!=3;i++){world_scenery_update(scene,pose.x,pose.y,pose.z,0,0);std::this_thread::sleep_for(std::chrono::microseconds(100));}
    assert(world_scenery_streaming_stats(scene).resident==3&&active_models==4);world_scenery_free(scene);assert(!active_models);
    assert(world_scenery_can_move(nullptr,0,0,1,1,0));
    assert(!world_scenery_can_move(nullptr,NAN,0,1,1,0));
    assert(!world_scenery_can_move(nullptr,1.e300,0,1,1,0));
    assert(!world_scenery_can_move(nullptr,0,0,1,1,0,1.75f,-1));
    assert(!world_scenery_can_move(nullptr,0,0,1,1,0,INFINITY,.32f));
    world_atlas_village_origin(&expected_x,&expected_z);
    const char *village_aliases[]={"village_house_a","village_house_b","village_inn","village_smithy","village_keep"};
    const float front[]={-5.55f,-6.55f,-6.55f,-4.55f,-9.f},door_width[]={1.8f,1.8f,2.4f,3.f,4.6f},door_height[]={2.6f,2.6f,3.f,3.2f,6.f};
    for(size_t i=0;i<std::size(village_aliases);i++) {
        scene=load_one(village_aliases[i],"0 1 .02");assert(world_scenery_placement(scene,0,&pose));
        assert(world_scenery_stats(scene).source_colliders==(i==4?14u:7u));
        const float feet=pose.y+.02f;
        assert(world_scenery_can_move(scene,pose.x,pose.z+front[i]-2,pose.x,pose.z+front[i]+2,feet,1.75f,.37f));
        assert(!world_scenery_can_move(scene,pose.x+door_width[i]*.5+.4,pose.z+front[i]-2,pose.x+door_width[i]*.5+.4,pose.z+front[i]+2,feet,1.75f,.37f));
        assert(!world_scenery_can_move(scene,pose.x,pose.z+front[i]-2,pose.x,pose.z+front[i]+2,pose.y+door_height[i]-1.5f,1.75f,.37f));
        assert(!world_scenery_can_move(scene,pose.x,pose.z,pose.x,pose.z-front[i]+2,feet,1.75f,.37f));
        world_scenery_free(scene);assert(!active_models);
    }
    // Imported closed buildings must not inherit the prototype's open door
    // compound collider. Explicit near/mid names still load transactional LODs.
    for(const char *alias:{"purchased_house_a","purchased_house_b","purchased_house_c","purchased_house_d","purchased_house_e","purchased_house_f",
                          "purchased_shop_a","purchased_shop_b","purchased_shop_c","purchased_shop_d",
                          "purchased_castle","purchased_gate","purchased_wall","purchased_barrel","purchased_crate","castle_barracks","castle_quartermaster"}) {
        scene=load_one(alias);assert(world_scenery_placement(scene,0,&pose));
        assert(pose.foundation&&!pose.rigid&&world_scenery_stats(scene).source_colliders==1);
        assert(world_scenery_stats(scene).loaded_groups==2);
        assert(!world_scenery_can_move(scene,pose.x+pose.minimum[0]*pose.scale-2,pose.z,
            pose.x+pose.maximum[0]*pose.scale+2,pose.z,pose.y,1.75f,.37f));
        world_scenery_free(scene);assert(!active_models);
    }
    for(const char *alias:{"purchased_goods_bread","purchased_goods_cheese","purchased_goods_chicken","purchased_goods_tankard"}) {
        scene=load_one(alias,"0 1 0 .75");assert(world_scenery_placement(scene,0,&pose));
        assert(pose.foundation&&!pose.rigid&&pose.elevation==.75f);
        assert(world_scenery_stats(scene).source_colliders==0&&world_scenery_stats(scene).loaded_groups==2);
        assert(world_scenery_can_move(scene,pose.x-1,pose.z,pose.x+1,pose.z,24,1.75f,.37f));
        world_scenery_free(scene);assert(!active_models);
    }
    // The lowered drawbridge has an open deck and solid side rails. A full
    // model Box would block crossing; no collider would permit walking off it.
    scene=load_one("castle_drawbridge","0 1 .70");assert(world_scenery_placement(scene,0,&pose));
    assert(world_scenery_stats(scene).source_colliders==2&&world_scenery_stats(scene).loaded_groups==2);
    const float bridge_feet=pose.y+.72f;
    assert(world_scenery_can_move(scene,pose.x,pose.z-13,pose.x,pose.z+1,bridge_feet,1.75f,.37f));
    assert(!world_scenery_can_move(scene,pose.x,pose.z-6,pose.x+8,pose.z-6,bridge_feet,1.75f,.37f));
    assert(!world_scenery_can_move(scene,pose.x,pose.z-6,pose.x-8,pose.z-6,bridge_feet,1.75f,.37f));
    world_scenery_free(scene);assert(!active_models);
    // Real village author data runs through the same parser/capsules. Native
    // GL verifies all four files with evaluated model bounds separately; CPU
    // stubs retain building dimensions and use measured wall-piece colliders.
    scene=world_scenery_load("projects/valwood_world/scenery/castle_village_instances.txt","models",error,sizeof(error));assert(scene);
    const auto town_stats=world_scenery_source_stats(scene,0);
    std::ifstream village_input("projects/valwood_world/scenery/castle_village_instances.txt");std::string village_line;
    size_t authored_village_records=0;
    while(std::getline(village_input,village_line)){std::istringstream row(village_line);std::string alias;if(row>>alias&&alias[0]!='#')++authored_village_records;}
    assert(town_stats.instances==authored_village_records&&town_stats.instances>=783&&town_stats.instances<=8192-2611&&town_stats.aliases>=11);
    const double village_route[][2]={{0,220},{0,130},{0,55},{0,-50},{0,-88},{0,-50},{0,55},{-95,55},{-95,125},{0,125},{0,220}};
    size_t village_sweeps=0;
    for(size_t i=1;i<std::size(village_route);i++) {
        double dx=village_route[i][0]-village_route[i-1][0],dz=village_route[i][1]-village_route[i-1][1],length=std::hypot(dx,dz);int steps=int(std::ceil(length/.5));
        for(int j=1;j<=steps;j++)for(double lane:{-.65,0.,.65}) {
            double ox=expected_x+village_route[i-1][0]+dx*(j-1)/steps-dz/length*lane,oz=expected_z+village_route[i-1][1]+dz*(j-1)/steps+dx/length*lane;
            double nx=expected_x+village_route[i-1][0]+dx*j/steps-dz/length*lane,nz=expected_z+village_route[i-1][1]+dz*j/steps+dx/length*lane;
            assert(world_scenery_can_move(scene,ox,oz,nx,nz,24,1.75f,.37f));village_sweeps++;
        }
    }
    assert(village_sweeps==4836);
    size_t alley_sweeps=0;
    for(int step=0;step<172;++step)for(double lane:{-.8,0.,.8}) {
        const double x=expected_x+61.75+lane,z=expected_z+51.-step*.5;
        assert(world_scenery_can_move(scene,x,z,x,z-.5,24,1.75f,.37f));++alley_sweeps;
    }
    assert(alley_sweeps==516);world_scenery_free(scene);assert(!active_models);
    std::puts("PASS village: measured wall-piece door/head/back-wall collisions, full village parse,806m/4836 main and86m/516 authored alley three-lane capsule sweeps");
    // Preparing edits borrows immutable geometry without touching old batches.
    scene=load_one("pine","0 1 .025");const auto models_before=active_models;
    const char *edit_files[]={path.c_str()};
    auto *prepared=world_scenery_prepare_files(scene,edit_files,1,"models",error,sizeof(error));
    assert(prepared&&active_models==models_before);
    assert(world_scenery_stats(prepared).reused_groups==world_scenery_stats(prepared).loaded_groups);
    const auto draws_before_prepare=draws;
    world_scenery_update(prepared,expected_x,26,expected_z+10,0,0);
    world_scenery_draw(prepared,17,expected_x,26,expected_z+10);
    assert(draws==draws_before_prepare); // No borrowed instance-buffer mutation.
    world_scenery_free(prepared);assert(active_models==models_before);
    // This fails AFTER borrowed and new model groups have been staged.
    write(path,record(expected_x,expected_z,"pine")+record(expected_x+20,expected_z,"fern0","0 .15 .20"));
    prepared=world_scenery_prepare_files(scene,edit_files,1,"models",error,sizeof(error));
    assert(!prepared&&std::strstr(error,"burial hides")&&active_models==models_before);
    world_scenery_update(scene,expected_x,26,expected_z+10,0,0);
    world_scenery_draw(scene,17,expected_x,26,expected_z+10);assert(draws>draws_before_prepare);
    write(path,record(expected_x,expected_z,"pine")+record(expected_x+20,expected_z,"pine")+record(expected_x-20,expected_z,"starting_barrel"));
    prepared=world_scenery_prepare_files(scene,edit_files,1,"models",error,sizeof(error));
    assert(prepared&&active_models>models_before&&world_scenery_stats(prepared).source_instances==3);
    const auto prepared_models=active_models;
    assert(!world_scenery_commit_prepared(prepared,nullptr)&&active_models==prepared_models);
    assert(world_scenery_commit_prepared(prepared,scene));world_scenery_free(scene);assert(active_models==prepared_models);
    world_scenery_update(prepared,expected_x,26,expected_z+10,0,0);
    world_scenery_draw(prepared,17,expected_x,26,expected_z+10);
    world_scenery_free(prepared);assert(!active_models);
    std::puts("PASS editor geometry transactions: unchanged alias/LOD geometry reused; staged draw/update blocked; post-load failure and wrong owner retain old resources; accepted transfer plus new aliases release exactly once");
    // User model registration, metadata changes and immutable revision paths.
    const size_t builtins=world_scenery_alias_count();
    WorldSceneryDefinition definition={"qa_custom","starting_crate",2,1,0,220,60};
    assert(world_scenery_configure(&definition,1,error,sizeof error)&&world_scenery_alias_count()==builtins+1);
    scene=load_one("qa_custom","0 1 0 0 .75");WorldSceneryPlacement lifted;assert(world_scenery_placement(scene,0,&lifted)&&lifted.offset_y==.75f&&lifted.y>world_atlas_height(expected_x,expected_z));
    float minimum[3],maximum[3];assert(world_scenery_alias_bounds(scene,"qa_custom",minimum,maximum)&&maximum[1]>minimum[1]);
    world_scenery_update(scene,expected_x,26,expected_z+10,0,0);world_scenery_draw(scene,17,expected_x,26,expected_z+10);assert(std::strstr(last_draw_path,"starting_crate.glb"));
    world_scenery_preview(scene,0,1);world_scenery_update(scene,expected_x,26,expected_z+10,0,0);world_scenery_draw(scene,17,expected_x,26,expected_z+10);assert(std::strstr(last_draw_path,"starting_crate_mid.glb"));world_scenery_preview(scene,SIZE_MAX,-1);
    const auto unchanged_models=active_models;definition.collision=0;assert(world_scenery_configure(&definition,1,error,sizeof error));prepared=world_scenery_prepare_files(scene,edit_files,1,"models",error,sizeof error);assert(prepared&&active_models==unchanged_models&&world_scenery_stats(prepared).source_colliders==0);assert(world_scenery_commit_prepared(prepared,scene));world_scenery_free(scene);scene=prepared;
    definition.stem="starting_barrel";assert(world_scenery_configure(&definition,1,error,sizeof error));fail_asset="starting_barrel";prepared=world_scenery_prepare_files(scene,edit_files,1,"models",error,sizeof error);assert(!prepared&&active_models==unchanged_models);fail_asset=nullptr;
    prepared=world_scenery_prepare_files(scene,edit_files,1,"models",error,sizeof error);assert(prepared&&world_scenery_stats(prepared).reused_groups==0&&active_models>unchanged_models);assert(world_scenery_commit_prepared(prepared,scene));world_scenery_free(scene);world_scenery_free(prepared);assert(!active_models);assert(world_scenery_configure(nullptr,0,error,sizeof error)&&world_scenery_alias_count()==builtins);
    std::puts("PASS user assets: bounded registry, vertical placement, real per-record LOD preview, collision edits reuse geometry, changed revision reload and failed reimport retains current resources");
    std::remove(second.c_str());
    std::puts("PASS starting scenery:14 families/18 LOD assets, shared multi-source transaction, elevated props/platform tops, continuous trunk/rotated-wall/gate capsule sweeps, true doorway/head clearance and zero runtime allocations");
    // Full static transforms preserve the selected pivot and reach both the
    // upload stream and conservative collision bounds without hot allocations.
    const float pivot_y=world_atlas_height(expected_x,expected_z)+1;
    const auto full_fields=std::string("0 1 0 0 0 23 -17 1.2 0.8 1.15 ")+std::to_string(pivot_y);scene=load_one("starting_crate",full_fields.c_str());WorldSceneryPlacement tilted;assert(world_scenery_placement(scene,0,&tilted));assert(std::abs(tilted.y-pivot_y)<1e-5&&std::abs(tilted.pitch-23*.017453292519943295)<1e-6&&std::abs(tilted.sx-1.2)<1e-6);
    world_scenery_update(scene,expected_x,pivot_y+2,expected_z+15,0,0);world_scenery_draw(scene,17,expected_x,pivot_y+2,expected_z+15);assert(last_draw_asset&&last_draw_asset->count==1);assert(std::abs(last_draw_asset->instances[0].axis_x-.2)<1e-6&&std::abs(last_draw_asset->instances[0].roll+17*.017453292519943295)<1e-6);
    assert(!world_scenery_can_move(scene,expected_x-8,expected_z,expected_x+8,expected_z,pivot_y-.2f));allocations=0;track_allocations=true;for(unsigned i=0;i<100;i++)world_scenery_update(scene,expected_x,pivot_y+2,expected_z+15,0,0);track_allocations=false;assert(allocations==0);world_scenery_free(scene);assert(!active_models);
    std::puts("PASS full static scenery: pitch/roll/axis stream, explicit pivot, collision and zero hot allocations");
    // Editor motion updates real uploaded instances without reloading geometry,
    // moving static collision, allocating, or committing an authored scene.
    scene=load_one("starting_crate");WorldSceneryPlacement original;assert(world_scenery_placement(scene,0,&original));
    WorldSceneryEditorTransform edit{0,original.x,original.z,original.y,original.yaw,original.scale,.23f,-.17f,1.2f,.8f,1.15f};
    const auto models_before_preview=active_models;const auto groups_before_preview=world_scenery_stats(scene).loaded_groups;
    allocations=0;track_allocations=true;
    for(unsigned i=0;i<120;i++){
        edit.x=original.x+double(i)*.01;edit.y=original.y+.2f;edit.yaw=float(i)*.002f;
        assert(world_scenery_editor_transforms(scene,&edit,1));
        world_scenery_update(scene,original.x,original.y+2,original.z+15,0,0);
        world_scenery_draw(scene,17,original.x,original.y+2,original.z+15);
        assert(last_draw_asset&&last_draw_asset->count==1);
        const auto &uploaded=last_draw_asset->instances[0];
        assert(std::abs(uploaded.x-float(edit.x-std::floor(original.x/128.)*128.))<1e-5&&std::abs(uploaded.y-edit.y)<1e-5);
        assert(uploaded.yaw==edit.yaw&&uploaded.pitch==edit.pitch&&uploaded.roll==edit.roll&&std::abs(uploaded.axis_x-.2f)<1e-6);
    }
    track_allocations=false;assert(allocations==0&&active_models==models_before_preview&&world_scenery_stats(scene).loaded_groups==groups_before_preview);
    assert(world_scenery_stats(scene).editor_preview_instances==1&&world_scenery_stats(scene).editor_preview_updates==120);
    auto invalid_edit=edit;invalid_edit.scale=0;assert(!world_scenery_editor_transforms(scene,&invalid_edit,1));
    invalid_edit=edit;invalid_edit.pitch=NAN;assert(!world_scenery_editor_transforms(scene,&invalid_edit,1));
    invalid_edit=edit;invalid_edit.record=1;assert(!world_scenery_editor_transforms(scene,&invalid_edit,1));
    WorldSceneryPlacement preview;assert(world_scenery_placement(scene,0,&preview)&&preview.x==edit.x&&preview.yaw==edit.yaw);
    edit.x=original.x+20;assert(world_scenery_editor_transforms(scene,&edit,1));
    assert(!world_scenery_can_move(scene,original.x-6,original.z,original.x+6,original.z,original.y));
    assert(world_scenery_can_move(scene,original.x+14,original.z,original.x+26,original.z,original.y));
    world_scenery_editor_clear(scene);assert(world_scenery_placement(scene,0,&preview)&&preview.x==original.x&&preview.y==original.y&&preview.pitch==original.pitch&&preview.sx==original.sx);
    assert(world_scenery_stats(scene).editor_preview_instances==0);
    assert(world_scenery_editor_transforms(scene,nullptr,0));assert(!world_scenery_editor_transforms(nullptr,&edit,1));world_scenery_editor_clear(nullptr);
    world_scenery_free(scene);assert(!active_models);
    std::puts("PASS live editor transforms: 120 real instance uploads, full TRS, unchanged static collision, atomic rejection/cancel, no geometry reloads or hot allocations");
    scene=load_one("pine");assert(world_scenery_placement(scene,0,&original));
    WorldSceneryEditorTransform moved{0,original.x+20,original.z,world_atlas_height(original.x+20,original.z)-original.burial+original.elevation+original.offset_y,.7f,original.scale,0,0,1,1,1,original.offset_y};
    const auto retained_models=active_models;allocations=0;track_allocations=true;
    assert(world_scenery_editor_commit_moves(scene,&moved,1));
    track_allocations=false;assert(allocations==0&&active_models==retained_models&&world_scenery_stats(scene).editor_preview_instances==0);
    world_scenery_editor_clear(scene);assert(world_scenery_placement(scene,0,&preview)&&preview.x==moved.x&&preview.y==moved.y);
    assert(world_scenery_can_move(scene,original.x-3,original.z,original.x+3,original.z,original.y));
    assert(!world_scenery_can_move(scene,moved.x-3,moved.z,moved.x+3,moved.z,moved.y));
    auto bad_move=moved;bad_move.scale*=1.1f;assert(!world_scenery_editor_commit_moves(scene,&bad_move,1));world_scenery_editor_clear(scene);
    assert(world_scenery_placement(scene,0,&preview)&&preview.x==moved.x&&preview.scale==moved.scale);
    world_scenery_free(scene);assert(!active_models);
    std::puts("PASS fast editor move: in-place root/yaw and static collision commit, retained geometry, rejected shape edits, zero allocations");
    // Publisher chunks aggregate many modules. A camera beside the far end
    // of a 150m wall must receive HIGH even though its root is over100m away.
    const double retained_x=expected_x,retained_z=expected_z;expected_x=-43000;expected_z=-23000;
    const WorldSceneryDefinition source_defs[]={
        {"nm_scene_built_fixture","showcase_source_large",2,0,0,2000,35},
        {"nm_scene_backdrop_fixture","showcase_source_horizon",2,0,0,2000,35},
        {"qa_large_control","showcase_source_large",2,0,0,2000,35}};
    assert(world_scenery_configure(source_defs,3,error,sizeof(error)));
    scene=load_one("nm_scene_built_fixture","0 1 0 0 0 0 0 1 1 1 1400");
    capture_draws=true;captured_draws={};
    world_scenery_update(scene,expected_x+100,1402,expected_z+20,0,0);world_scenery_draw(scene,17,expected_x+100,1402,expected_z+20);
    assert(captured_draws.count==2&&prop_near_draws&&prop_mid_draws);
    for(size_t i=0;i<captured_draws.count;i++)assert(captured_draws.records[i].source_distance==1);
    assert(last_source_distance==0); // No source-only state leaks to other draws.
    captured_draws={};world_scenery_draw(scene,17,expected_x+100,1402,expected_z+20,true);
    assert(captured_draws.count==1&&std::strstr(captured_draws.records[0].asset->path,"_mid.glb")&&captured_draws.records[0].prop_lod[2]==0); // Pinned MID is the sole unfaded caster.
    captured_draws={};world_scenery_update(scene,expected_x+100,1402,expected_z+60,0,0);world_scenery_draw(scene,17,expected_x+100,1402,expected_z+60);
    assert(captured_draws.count==1&&std::strstr(captured_draws.records[0].asset->path,"_mid.glb"));
    captured_draws={};world_scenery_update(scene,expected_x+100,1602,expected_z+20,0,-1.45f);world_scenery_draw(scene,17,expected_x+100,1602,expected_z+20);
    assert(captured_draws.count==1&&std::strstr(captured_draws.records[0].asset->path,"_mid.glb")); // Another floor200m away is never HIGH.
    allocations=0;track_allocations=true;for(unsigned i=0;i<100;i++)world_scenery_update(scene,expected_x+100+i*.01,1402,expected_z+20,0,0);track_allocations=false;assert(!allocations);
    capture_draws=false;world_scenery_free(scene);assert(!active_models);
    // The A/B switch and non-source control retain original root selection.
    setenv("VALWOOD_QA_DISABLE_SOURCE_BOUNDS_LOD","1",1);
    scene=load_one("nm_scene_built_fixture","0 1 0 0 0 0 0 1 1 1 1400");capture_draws=true;captured_draws={};world_scenery_update(scene,expected_x+100,1402,expected_z+20,0,0);world_scenery_draw(scene,17,expected_x+100,1402,expected_z+20);assert(captured_draws.count==1&&captured_draws.records[0].source_distance==0);capture_draws=false;world_scenery_free(scene);unsetenv("VALWOOD_QA_DISABLE_SOURCE_BOUNDS_LOD");
    scene=load_one("qa_large_control","0 1 0 0 0 0 0 1 1 1 1400");capture_draws=true;captured_draws={};world_scenery_update(scene,expected_x+100,1402,expected_z+20,0,0);world_scenery_draw(scene,17,expected_x+100,1402,expected_z+20);assert(captured_draws.count==1&&captured_draws.records[0].source_distance==0);capture_draws=false;world_scenery_free(scene);
    // Initial source HIGH must not bypass geometry reservations through eager
    // adoption. With96 bytes occupied by pinned MID, a96-byte HIGH
    // request keeps the fallback and cannot allocate another GPU model.
    write(path,record(expected_x,expected_z,"nm_scene_built_fixture","0 1 0 0 0 0 0 1 1 1 1400"));
    const char *bounded_source_file[]={path.c_str()};WorldSceneryStreamingConfig bounded_config;bounded_config.geometry_bytes=144;
    scene=world_scenery_load_files_streamed(bounded_source_file,1,"models",expected_x+100,expected_z+20,bounded_config,error,sizeof(error));assert(scene&&world_scenery_stats(scene).loaded_groups==1&&active_models==1);
    for(unsigned i=0;i<50;i++){world_scenery_update(scene,expected_x+100,1402,expected_z+20,0,0);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(world_scenery_streaming_stats(scene).geometry_bytes<=144&&world_scenery_stats(scene).loaded_groups==1&&active_models==1&&world_scenery_streaming_stats(scene).fallback_groups==1&&world_scenery_streaming_stats(scene).failed==0&&world_scenery_streaming_stats(scene).wanted==2&&world_scenery_streaming_stats(scene).critical_pending==0);
    world_scenery_update(scene,expected_x+100,1802,expected_z+20,0,0);assert(world_scenery_streaming_stats(scene).critical_pending==0);
    world_scenery_free(scene);assert(!active_models);
    // A distant mountain's authored root is8km away while its actual surface
    // spans the viewer. Exactly one MID asset stays visible and pinned.
    write(path,record(expected_x+8000,expected_z,"nm_scene_backdrop_fixture","0 1 0 0 0 0 0 1 1 1 1400"));
    const char *source_file[]={path.c_str()};WorldSceneryStreamingConfig source_config;
    scene=world_scenery_load_files_streamed(source_file,1,"models",expected_x,expected_z,source_config,error,sizeof(error));assert(scene&&world_scenery_stats(scene).loaded_groups==1&&active_models==1);
    capture_draws=true;captured_draws={};world_scenery_update(scene,expected_x,1402,expected_z+20,0,0);world_scenery_draw(scene,17,expected_x,1402,expected_z+20);
    assert(captured_draws.count==1&&std::strstr(captured_draws.records[0].asset->path,"_mid.glb")&&captured_draws.records[0].prop_lod[2]==0&&captured_draws.records[0].source_distance==0);
    assert(last_distance_fade[0]==0&&last_distance_fade[1]==0);
    for(unsigned i=0;i<5;i++)world_scenery_update(scene,expected_x+i,1402,expected_z+20,0,0);
    assert(world_scenery_streaming_stats(scene).loads==0&&world_scenery_streaming_stats(scene).critical_pending==0&&world_scenery_streaming_stats(scene).wanted==1);
    assert(world_scenery_streaming(scene,false,source_config,error,sizeof(error))&&world_scenery_stats(scene).loaded_groups==1&&active_models==1);
    capture_draws=false;world_scenery_free(scene);assert(!active_models);
    setenv("VALWOOD_QA_DISABLE_SOURCE_HORIZON","1",1);scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));assert(scene);capture_draws=true;captured_draws={};world_scenery_update(scene,expected_x,1402,expected_z+20,0,0);world_scenery_draw(scene,17,expected_x,1402,expected_z+20);assert(captured_draws.count==0);capture_draws=false;world_scenery_free(scene);unsetenv("VALWOOD_QA_DISABLE_SOURCE_HORIZON");
    assert(world_scenery_configure(nullptr,0,error,sizeof(error))&&!active_models);expected_x=retained_x;expected_z=retained_z;
    std::puts("PASS source batches: surface-distance HIGH/MID selection, ordinary alias preservation, no hot allocations, distant MID-only horizon and no redundant streaming loads");
    world_scenery_free(nullptr); world_scenery_update(nullptr, 0, 0, 0, 0, 0);
    assert(world_scenery_stats(nullptr).source_instances == 0);
    std::remove(temp); world_atlas_shutdown();
    std::puts("PASS authored scenery: strict aliases/records, dry grounding, clear trail, transactional load/upload, LOD bands, stable batches, two-program cache, zero-update allocations, full authored composition and teardown");
}
