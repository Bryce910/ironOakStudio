// Copied CPU-only model/GL controls from tests/world_scenery_test.cpp.
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
extern "C" WorldModelAsset *world_model_load_budgeted(const char *path,unsigned edge) {
    assert(edge==0||edge==512||edge==1024||edge==2048||edge==4096);return world_model_load(path);
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
std::size_t world_model_staged_new_texture_bytes_union(WorldModelStaged *,WorldModelStaged *){return 0;}
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
