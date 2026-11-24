#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_scenery.h"
#include "engine/world_scenery_duplicate_index.h"
#include "engine/world_scenery_pivot.h"
#include "engine/world_opening_interior.h"
#include "engine/world_studio.h"
#include "engine/world_transform_math.h"
#include "engine/world_cover.h"
#include "engine/world_ground_cover_quality.h"
#include "engine/world_castle_creek.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include "engine/world_model_cpu.h"
#include "engine/world_regions.h"
#include "engine/world_rooted_grass.h"
#include "engine/world_scenery_shadow_visibility.h"

#include <algorithm>
#include <limits.h>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <limits>
#include <new>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {
// The scene cap spans every source. Each shared alias/LOD still fits the
// existing bounded model instance buffer; extra districts add families rather
// than silently overflowing one draw or doubling every pose's staging memory.
constexpr std::size_t record_cap = WORLD_SOURCE_RECORD_LIMIT, prepared_record_cap = record_cap + world_studio::placement_limit, alias_record_cap = WORLD_SCENERY_ALIAS_RECORD_LIMIT, source_cap = WORLD_SCENERY_SOURCE_FILE_LIMIT;
// Full precision editor transactions preserve every source root and TRS.
// Each prepared record retains its256-byte legacy base; an optional,
// validated item-wind tail adds at most64 bytes, including its separator.
// Four MiB admits those bounded records plus ordinary comments/newlines.
constexpr std::size_t source_byte_cap = 4ull*1024*1024;
constexpr std::size_t legacy_record_bytes=256,item_wind_tail_bytes=64;
constexpr double pi = 3.14159265358979323846;
enum class Grounding { Root, Support, Foundation, GroundTop };
enum class Collision { None, Trunk, Box, Circle, Gate, Village, Drawbridge };
struct Alias {
    const char *name, *stem;
    bool tree;
    float max_burial, distance;
    int vegetation, lods;
    Grounding grounding;
    Collision collision;
    float trunk_radius = 0, max_elevation = 0, lod_split = 80;
    bool prop_lod_fade = false, cast_shadow = false;
    bool named_near_file = false; // Imported pairs use explicit _near/_mid names.
    bool distance_fade = false; // Authored mesh pairs fade before the bounded draw range.
};
bool battlefield_corpse_alias(const char *name){
    return std::strcmp(name,"battlefield_corpse_back")==0||
        std::strcmp(name,"battlefield_corpse_side")==0;
}
float scenery_alias_detail_scale(const Alias &alias,float detail){
    return alias.tree||alias.vegetation==3||alias.vegetation==5||alias.vegetation==6||
        battlefield_corpse_alias(alias.name)?detail:1.f;
}
constexpr Alias builtins[] = {
    {"pine", "cinematic_pine", true, 1.f, 1680, 1, 3, Grounding::Root, Collision::Trunk, .38f},
    {"fir", "cinematic_fir", true, 1.f, 1680, 1, 3, Grounding::Root, Collision::Trunk, .42f},
    {"broadleaf", "cinematic_broadleaf", true, .5f, 1680, 1, 3, Grounding::Root, Collision::Trunk, .20f},
    {"pine_young", "cinematic_pine_young", true, .25f, 1680, 1, 3, Grounding::Root, Collision::Trunk, .08f},
    {"rock0", "cinematic_rock_0", false, 2.f, 240, 0, 1, Grounding::Support, Collision::Box},
    {"rock1", "cinematic_rock_1", false, 2.f, 240, 0, 1, Grounding::Support, Collision::Box},
    {"rock2", "cinematic_rock_2", false, 2.f, 240, 0, 1, Grounding::Support, Collision::Box},
    {"fern0", "woodland_fern_0", false, .2f, 115, 3, 1, Grounding::Root, Collision::None},
    {"fern1", "woodland_fern_1", false, .2f, 115, 3, 1, Grounding::Root, Collision::None},
    {"fern2", "woodland_fern_2", false, .2f, 115, 3, 1, Grounding::Root, Collision::None},
    {"village_crop_corn", "village_crop_corn", false, .03f, 320, 6, 2, Grounding::Root, Collision::None, 0, 0, 60, true, true, true},
    {"village_crop_wheat", "village_crop_wheat", false, .03f, 320, 6, 2, Grounding::Root, Collision::None, 0, 0, 60, true, true, true},
    {"deadwood", "cinematic_deadwood", false, .3f, 260, 0, 2, Grounding::Support, Collision::Box},
    {"cliff", "cinematic_cliff_01", false, 10.f, 1000, 0, 1, Grounding::Support, Collision::Box},
    {"starting_gate", "starting_gate", false, .5f, 700, 0, 1, Grounding::Foundation, Collision::Gate},
    {"starting_wall", "starting_wall", false, .5f, 550, 0, 1, Grounding::Foundation, Collision::Box},
    {"starting_tower", "starting_tower", false, .5f, 1000, 0, 1, Grounding::Foundation, Collision::Circle},
    {"starting_platform", "starting_platform", false, .5f, 320, 0, 1, Grounding::GroundTop, Collision::None},
    {"starting_table", "starting_table", false, .3f, 220, 0, 2, Grounding::Foundation, Collision::Box, 0, .05f},
    {"starting_crate", "starting_crate", false, .3f, 200, 0, 1, Grounding::Support, Collision::Box, 0, 3.f},
    {"starting_barrel", "starting_barrel", false, .3f, 200, 0, 2, Grounding::Support, Collision::Circle, 0, 3.f},
    {"starting_bucket", "starting_bucket", false, .15f, 90, 0, 1, Grounding::Support, Collision::None, 0, 3.f},
    {"starting_lantern", "starting_lantern", false, .15f, 110, 0, 1, Grounding::Support, Collision::None, 0, 3.f},
    {"starting_axe", "starting_axe", false, .15f, 70, 0, 1, Grounding::Support, Collision::None, 0, 3.f},
    {"starting_stump", "starting_stump", false, .3f, 200, 0, 2, Grounding::Support, Collision::Circle, 0, .1f},
    {"starting_firepit", "starting_firepit", false, .3f, 200, 0, 1, Grounding::Support, Collision::Circle},
    {"starting_celandine", "starting_celandine", false, .15f, 90, 3, 1, Grounding::Root, Collision::None},
    {"starting_wildflower", "starting_wildflower", false, .15f, 90, 3, 2, Grounding::Root, Collision::None, 0, 0, 18},
    {"showcase_roots", "showcase_roots", false, .45f, 230, 0, 2, Grounding::Support, Collision::Box, 0, 0, 65, true},
    {"showcase_stump", "showcase_stump", false, .20f, 180, 0, 2, Grounding::Support, Collision::Box, 0, 0, 55, true},
    {"showcase_shrub", "showcase_shrub", false, .10f, 145, 3, 2, Grounding::Root, Collision::None, 0, 0, 50, true, true},
    {"showcase_shrub_b", "showcase_shrub_b", false, .10f, 145, 3, 2, Grounding::Root, Collision::None, 0, 0, 50, true, true},
    {"showcase_sorrel", "showcase_sorrel", false, .03f, 90, 3, 2, Grounding::Root, Collision::None, 0, 0, 24, true},
    {"village_house_a", "village_house_a", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 90, true, true},
    {"village_house_b", "village_house_b", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 90, true, true},
    {"village_inn", "village_inn", false, .10f, 1100, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 90, true, true},
    {"village_smithy", "village_smithy", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 90, true, true},
    {"village_stall_red", "village_stall_red", false, .10f, 320, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 50, true, true},
    {"village_stall_blue", "village_stall_blue", false, .10f, 320, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 50, true, true},
    {"village_stall_gold", "village_stall_gold", false, .10f, 320, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 50, true, true},
    {"village_keep", "village_keep", false, .10f, 2400, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 180, true, true},
    {"village_well", "village_well", false, .10f, 500, 0, 1, Grounding::Foundation, Collision::Box},
    {"village_banner", "village_banner", false, .10f, 450, 0, 1, Grounding::Foundation, Collision::None},
    {"village_cart", "village_cart", false, .10f, 350, 0, 1, Grounding::Foundation, Collision::Box},
    {"purchased_house_a", "purchased_village_house_a", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"purchased_house_b", "purchased_village_house_b", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"purchased_shop_a", "purchased_village_stall_a", false, .10f, 400, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 60, true, true, true},
    {"purchased_shop_b", "purchased_village_stall_b", false, .25f, 400, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 60, true, true, true},
    {"purchased_house_c", "purchased_village_house_c", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"purchased_house_d", "purchased_village_house_d", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"purchased_house_e", "purchased_village_house_e", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"purchased_house_f", "purchased_village_house_f", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"purchased_shop_c", "purchased_village_stall_c", false, .25f, 400, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 60, true, true, true},
    {"purchased_shop_d", "purchased_village_stall_d", false, .25f, 400, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 60, true, true, true},
    {"purchased_castle", "purchased_village_castle", false, .10f, 2400, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 180, true, true, true},
    {"purchased_gate", "purchased_village_gate", false, .10f, 1000, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"purchased_wall", "purchased_village_wall", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"purchased_barrel", "purchased_village_barrel", false, .10f, 180, 0, 2, Grounding::Foundation, Collision::Box, 0, .05f, 40, true, true, true},
    {"purchased_crate", "purchased_village_crate", false, .10f, 180, 0, 2, Grounding::Foundation, Collision::Box, 0, 3.f, 40, true, true, true},
    {"purchased_goods_bread", "purchased_goods_bread", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 22, true, true, true},
    {"purchased_goods_cheese", "purchased_goods_cheese", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 22, true, true, true},
    {"purchased_goods_chicken", "purchased_goods_chicken", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 22, true, true, true},
    {"purchased_goods_tankard", "purchased_goods_tankard", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 22, true, true, true},
    {"purchased_prop_lamp", "purchased_prop_lamp", false, .01f, 160, 0, 2, Grounding::Foundation, Collision::None, 0, 12.f, 35, true, true, true},
    {"purchased_prop_flag_a", "purchased_prop_flag_a", false, .01f, 240, 0, 2, Grounding::Foundation, Collision::None, 0, 16.f, 55, true, true, true},
    {"purchased_prop_flag_b", "purchased_prop_flag_b", false, .01f, 240, 0, 2, Grounding::Foundation, Collision::None, 0, 16.f, 55, true, true, true},
    {"purchased_prop_sack", "purchased_prop_sack", false, .01f, 130, 0, 2, Grounding::Foundation, Collision::Box, 0, 3.f, 30, true, true, true},
    {"purchased_prop_logs", "purchased_prop_logs", false, .01f, 150, 0, 2, Grounding::Foundation, Collision::Box, 0, 3.f, 35, true, true, true},
    {"purchased_prop_rope", "purchased_prop_rope", false, .01f, 180, 0, 2, Grounding::Foundation, Collision::None, 0, 16.f, 40, true, true, true},
    {"purchased_prop_barrel_small", "purchased_prop_barrel_small", false, .01f, 170, 0, 2, Grounding::Foundation, Collision::Box, 0, 3.f, 40, true, true, true},
    {"purchased_torch_pole", "purchased_torch_pole", false, .01f, 320, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 45, true, true, true},
    {"purchased_torch_wall", "purchased_torch_wall", false, .01f, 320, 0, 2, Grounding::Foundation, Collision::Box, 0, 2.f, 40, true, true, true},
    {"purchased_torch_brazier", "purchased_torch_brazier", false, .01f, 320, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 40, true, true, true},
    {"village_toolkit_axe", "village_toolkit_axe", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_chisel", "village_toolkit_chisel", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_claw_hammer", "village_toolkit_claw_hammer", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_crowbar", "village_toolkit_crowbar", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_firepoker", "village_toolkit_firepoker", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_hatchet", "village_toolkit_hatchet", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_hoe", "village_toolkit_hoe", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_machete", "village_toolkit_machete", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_pickaxe", "village_toolkit_pickaxe", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_pitchfork", "village_toolkit_pitchfork", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_scythe", "village_toolkit_scythe", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_toolkit_shovel", "village_toolkit_shovel", false, .01f, 110, 0, 2, Grounding::Foundation, Collision::None, 0, 3.f, 24, true, true, true},
    {"village_horseman", "village_horseman", false, .01f, 280, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 65, true, true, true},
    {"castle_monument", "castle_monument", false, .10f, 1000, 0, 1, Grounding::Foundation, Collision::Village},
    {"castle_training_dummy", "castle_training_dummy", false, .10f, 260, 0, 1, Grounding::Foundation, Collision::Village},
    {"castle_archery_target", "castle_archery_target", false, .10f, 260, 0, 1, Grounding::Foundation, Collision::Village},
    {"castle_weapon_rack", "castle_weapon_rack", false, .10f, 230, 0, 1, Grounding::Foundation, Collision::Village},
    {"castle_yard_bench", "castle_yard_bench", false, .10f, 260, 0, 1, Grounding::Foundation, Collision::Village},
    {"castle_training_fence", "castle_training_fence", false, .10f, 300, 0, 1, Grounding::Foundation, Collision::Village},
    {"castle_drawbridge", "castle_drawbridge", false, .75f, 550, 0, 2, Grounding::Foundation, Collision::Drawbridge, 0, 0, 65, true, true},
    {"castle_barracks", "purchased_village_house_a", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"castle_quartermaster", "purchased_village_house_b", false, .10f, 900, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 90, true, true, true},
    {"village_tavern", "village_tavern", false, .01f, 850, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 90, true, true, true},
    {"castle_residents_shelter", "castle_residents_shelter", false, .01f, 400, 0, 2, Grounding::Foundation, Collision::Village, 0, 0, 60, true, true, true},
    {"castle_residents_bedroll", "castle_residents_bedroll", false, .01f, 130, 0, 2, Grounding::Foundation, Collision::None, 0, .1f, 35, true, true, true},
    {"demo_sign_baker", "demo_sign_baker", false, .10f, 125, 0, 2, Grounding::Foundation, Collision::None, 0, 8.f, 28, true, true, true},
    {"demo_sign_tavern", "demo_sign_tavern", false, .10f, 125, 0, 2, Grounding::Foundation, Collision::None, 0, 8.f, 28, true, true, true},
    {"demo_sign_herbalist", "demo_sign_herbalist", false, .10f, 125, 0, 2, Grounding::Foundation, Collision::None, 0, 8.f, 28, true, true, true},
    {"bep_nature_oak", "bep_nature_oak", true, .12f, 950, 1, 2, Grounding::Root, Collision::Trunk, .42f, 0, 80, true, true, true, true},
    {"bep_nature_beech", "bep_nature_beech", true, .12f, 1050, 1, 2, Grounding::Root, Collision::Trunk, .40f, 0, 85, true, true, true, true},
    {"bep_nature_bush_a", "bep_nature_bush_a", false, .08f, 180, 3, 2, Grounding::Root, Collision::None, 0, 0, 45, true, true, true, true},
    {"bep_nature_bush_b", "bep_nature_bush_b", false, .08f, 180, 3, 2, Grounding::Root, Collision::None, 0, 0, 45, true, true, true, true},
    {"bep_nature_grass_long", "bep_nature_grass_long", false, .025f, 105, 3, 2, Grounding::Root, Collision::None, 0, 0, 28, true, false, true, true},
    {"bep_nature_grass_broad", "bep_nature_grass_broad", false, .025f, 105, 3, 2, Grounding::Root, Collision::None, 0, 0, 28, true, false, true, true},
    {"bep_nature_fern", "bep_nature_fern", false, .025f, 125, 3, 2, Grounding::Root, Collision::None, 0, 0, 32, true, false, true, true},
    {"bep_nature_reed", "bep_nature_reed", false, .025f, 155, 3, 2, Grounding::Root, Collision::None, 0, 0, 40, true, false, true, true},
    {"bep_nature_clover", "bep_nature_clover", false, .015f, 75, 3, 2, Grounding::Root, Collision::None, 0, 0, 20, true, false, true, true},
    {"bep_nature_palm", "bep_nature_palm", false, .08f, 240, 3, 2, Grounding::Root, Collision::None, 0, 0, 55, true, true, true, true},
    {"bep_prop_barrel", "bep_prop_barrel", false, .06f, 180, 0, 2, Grounding::Support, Collision::Circle, 0, .05f, 40, true, true, true, true},
    {"bep_prop_chest", "bep_prop_chest", false, .06f, 180, 0, 2, Grounding::Support, Collision::Box, 0, .05f, 40, true, true, true, true},
    {"bep_prop_bench", "bep_prop_bench", false, .04f, 200, 0, 2, Grounding::Support, Collision::Box, 0, 0, 45, true, true, true, true},
    {"bep_prop_bucket", "bep_prop_bucket", false, .04f, 100, 0, 2, Grounding::Support, Collision::None, 0, .05f, 25, true, true, true, true},
    {"bep_prop_fence", "bep_prop_fence", false, .10f, 260, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 60, true, true, true, true},
    {"bep_prop_fence_broken", "bep_prop_fence_broken", false, .10f, 260, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 60, true, true, true, true},
    {"bep_structure_well", "bep_structure_well", false, 1.1f, 450, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 80, true, true, true, true},
    {"bep_structure_storehouse", "bep_structure_storehouse", false, 1.1f, 650, 0, 2, Grounding::Foundation, Collision::Box, 0, 0, 100, true, true, true, true}
};
constexpr std::size_t builtin_count=sizeof(builtins)/sizeof(builtins[0]),custom_limit=WORLD_CUSTOM_ASSET_LIMIT;
constexpr std::size_t alias_count=builtin_count+custom_limit;
constexpr std::size_t group_cap=[] {std::size_t count=custom_limit*3;for(const auto &a:builtins)count+=a.lods;return count;}();
std::array<Alias,alias_count> aliases=[] {std::array<Alias,alias_count> a={};std::copy(std::begin(builtins),std::end(builtins),a.begin());for(size_t i=builtin_count;i<alias_count;i++)a[i].name=a[i].stem="";return a;}();
std::array<std::string,WORLD_CUSTOM_ASSET_LIMIT> registered_names,registered_stems;
std::size_t active_alias_count=builtin_count;
std::uint64_t alias_revision=1;
struct Placement { unsigned alias; double x, z; float y, yaw, scale, burial, elevation, offset_y; unsigned line, source;bool active=true;std::size_t collider_first=0,collider_count=0,source_record=0;float pitch=0,roll=0,sx=1,sy=1,sz=1,pivot_y=std::numeric_limits<float>::quiet_NaN(),wind_sway=1,wind_flutter=1; };
struct Source { std::string path; std::size_t instances = 0, trees = 0, props = 0, aliases = 0, colliders = 0; };
struct Collider {
    double x = 0, z = 0, min_x = 0, max_x = 0, min_z = 0, max_z = 0;
    double cosine = 1, sine = 0;
    float xmin = 0, xmax = 0, zmin = 0, zmax = 0, ymin = 0, ymax = 0, radius = 0;
    bool circle = false;
    std::size_t record=SIZE_MAX;
};
struct Uniforms {unsigned long long epoch=0;
    GLint studio_tint=-1,studio_normal=-1,studio_preview=-1,studio_graph=-1,studio_surface=-1;
    GLuint program = 0;
    GLint offset = -1, anchor = -1, grass = -1, vegetation = -1, lod = -1, wind = -1, tree_height = -1, prop_lod = -1, distance_fade = -1, source_distance = -1;
    GLint rooted_grass_wind = -1,item_wind=-1;
};
struct Group {
    std::string asset_path,alias_name;int graph=-1,surface=-1;
    std::uint64_t source_stamp=0;
    WorldModelAsset *asset = nullptr, *shadow_asset = nullptr;
    float tint[3]={1,1,1},roughness=1,normal=1,uv=1;
    std::size_t borrowed_from=std::numeric_limits<std::size_t>::max();
    unsigned source_lod=0;
    unsigned alias = 0;
    int lod = -1, vegetation = 0, prop_lod = 0;
    float base_lo = 0, base_hi = 0, lo = 0, hi = 0, padding = 0, height = 0, radius = 0, sphere_radius = 0;
    float minimum[3] = {}, maximum[3] = {};
    float lod_minimum[3] = {}, lod_maximum[3] = {}; // union of the source LOD pair
    bool shadow = false, rooted_grass = false;
    double anchor_x = 0, anchor_z = 0;
    std::vector<WorldModelInstance> next, hidden, uploaded;
    WorldModelInstanceBatch *shadow_batch=nullptr;
    WorldModelAsset *shadow_batch_target=nullptr;
    bool shadow_batch_failed=false;
    std::vector<WorldModelInstance> shadow_selected,shadow_uploaded;
    float shadow_minimum[3]={},shadow_maximum[3]={};
    std::size_t visible_count = 0;
    bool fallback=false;
    std::vector<std::size_t> records;
};

void error_text(char *out, std::size_t capacity, const std::string &message) {
    if (out && capacity) std::snprintf(out, capacity, "%s", message.c_str());
    std::fprintf(stderr, "[Scenery] %s\n", message.c_str());
}
double trail_clearance(double x, double z) {
    std::size_t count = 0;
    const auto *trail = world_atlas_trail(&count);
    double clearance = 1.e30;
    for (std::size_t k = 1; k < count; ++k) {
        const auto &a = trail[k - 1], &b = trail[k];
        double dx = b.x - a.x, dz = b.z - a.z;
        double t = std::clamp(((x - a.x) * dx + (z - a.z) * dz) / (dx * dx + dz * dz), 0., 1.);
        double width = a.half_width + t * (b.half_width - a.half_width);
        clearance = std::min(clearance, std::hypot(x - a.x - dx * t, z - a.z - dz * t) - width);
    }
    return clearance;
}
vw_transform::M placement_matrix(const Placement &a){return vw_transform::matrix({0,0,0,a.yaw*180/pi,a.scale,a.pitch*180/pi,a.roll*180/pi,a.sx,a.sy,a.sz});}
bool extended(const Placement &a){return a.pitch!=0||a.roll!=0||a.sx!=1||a.sy!=1||a.sz!=1;}
bool rigid(unsigned alias) { return aliases[alias].grounding == Grounding::Support; }
bool source_chunk(unsigned alias){return std::strncmp(aliases[alias].name,"nm_scene_",9)==0;}
bool source_backdrop(unsigned alias){return std::strncmp(aliases[alias].name,"nm_scene_backdrop_",18)==0;}
struct SourceDistances {double nearest,farthest;};
SourceDistances source_distances(const Placement &a,const float *minimum,const float *maximum,double x,double y,double z){
    double xmin=1e30,ymin=1e30,zmin=1e30,xmax=-1e30,ymax=-1e30,zmax=-1e30;
    if(std::abs(a.yaw)<1.e-7&&!extended(a)){
        xmin=a.x+minimum[0]*a.scale;xmax=a.x+maximum[0]*a.scale;
        zmin=a.z+minimum[2]*a.scale;zmax=a.z+maximum[2]*a.scale;
        ymin=a.y+minimum[1]*a.scale;ymax=a.y+maximum[1]*a.scale;
    }else{
        const auto matrix=placement_matrix(a);
        for(unsigned k=0;k<8;k++){
            const auto q=vw_transform::vector(matrix,{k&1?maximum[0]:minimum[0],k&2?maximum[1]:minimum[1],k&4?maximum[2]:minimum[2]});
            xmin=std::min(xmin,a.x+q[0]);xmax=std::max(xmax,a.x+q[0]);
            zmin=std::min(zmin,a.z+q[2]);zmax=std::max(zmax,a.z+q[2]);
            ymin=std::min(ymin,a.y+q[1]);ymax=std::max(ymax,a.y+q[1]);
        }
    }
    const double near_y=std::isfinite(y)?y-std::clamp(y,ymin,ymax):0;
    const double far_y=std::isfinite(y)?std::max(std::abs(y-ymin),std::abs(y-ymax)):0;
    return {std::hypot(std::hypot(x-std::clamp(x,xmin,xmax),z-std::clamp(z,zmin,zmax)),near_y),
        std::hypot(std::hypot(std::max(std::abs(x-xmin),std::abs(x-xmax)),std::max(std::abs(z-zmin),std::abs(z-zmax))),far_y)};
}
double streaming_distance(const Placement &a,const float *minimum,const float *maximum,double x,double z) {
    // Spatial source batches use their actual transformed AABB, including
    // bridges and narrow walls, rather than empty enclosing-circle space.
    if(source_chunk(a.alias))return source_distances(a,minimum,maximum,x,std::nan(""),z).nearest;
    const double radius=std::hypot(std::max(std::abs(minimum[0]),std::abs(maximum[0])),std::max(std::abs(minimum[2]),std::abs(maximum[2])));
    return std::max(0.,std::hypot(a.x-x,a.z-z)-radius*a.scale);
}
float supported_height(const Placement &a, const Group &g,WorldSceneryGroundHeight sample=nullptr,const void *context=nullptr) {
    auto height=[&](double x,double z){return sample?float(sample(x,z,context)):world_atlas_height(x,z);};
    if(extended(a)){const auto m=placement_matrix(a);float lowest=a.y+a.burial;double bottom=1e30;for(unsigned k=0;k<8;k++){auto q=vw_transform::vector(m,{k&1?g.maximum[0]:g.minimum[0],k&2?g.maximum[1]:g.minimum[1],k&4?g.maximum[2]:g.minimum[2]});bottom=std::min(bottom,q[1]);}for(int iz=0;iz<=4;iz++)for(int ix=0;ix<=4;ix++){auto q=vw_transform::vector(m,{g.minimum[0]+ix*.25*(g.maximum[0]-g.minimum[0]),g.minimum[1],g.minimum[2]+iz*.25*(g.maximum[2]-g.minimum[2])});const float h=height(a.x+q[0],a.z+q[2]);if(!std::isfinite(h))return h;lowest=std::min(lowest,h);}return lowest-float(bottom)-a.burial-.05f;}
    // Load-time only. A rigid model must intersect terrain along its whole
    // rotated/scaled base footprint, not merely touch at its root. This matters
    // especially for the 92m coastal cliff and horizontal fallen trunks.
    const int steps_x = a.alias == 11 ? 8 : 4;
    const int steps_z = 4;
    float lowest = a.y + a.burial;
    double c = std::cos(a.yaw), s = std::sin(a.yaw);
    for (int iz = 0; iz <= steps_z; ++iz) for (int ix = 0; ix <= steps_x; ++ix) {
        double x = (g.minimum[0] + double(ix) / steps_x * (g.maximum[0] - g.minimum[0])) * a.scale;
        double z = (g.minimum[2] + double(iz) / steps_z * (g.maximum[2] - g.minimum[2])) * a.scale;
        float sampled = height(a.x + c * x + s * z, a.z - s * x + c * z);
        if (!std::isfinite(sampled)) return sampled;
        lowest = std::min(lowest, sampled);
    }
    return lowest - a.burial - .05f;
}
struct ViewAngles { double cy,sy,cp,sp,tan_h,tan_v,plane_h,plane_v,pool_surface; };
bool visible(const Placement &a, const Group &g, double x, double y, double z, const ViewAngles &angles) {
    // Validate both the main and vertically mirrored camera. Shadows select
    // all nearby casters independently, so an offscreen tree still casts.
    double dx = a.x - x, dz = a.z - z, cy=angles.cy,sy=angles.sy,cp=angles.cp,sp=angles.sp;
    double horizontal = dx * sy - dz * cy, across = dx * cy + dz * sy;
    double center_y = a.y + g.height * a.scale * .5;
    double radius = g.sphere_radius * a.scale + 2;
    if(extended(a)){center_y=a.y;radius=(g.sphere_radius+g.height)*a.scale*std::max({a.sx,a.sy,a.sz})+2;}
    for (int mirror = 0; mirror < (std::isfinite(angles.pool_surface) ? 3 : 2); ++mirror) {
        const double level = mirror == 2 ? angles.pool_surface : 0.;
        double dy = mirror ? center_y + y - 2*level : center_y - y;
        double sine = mirror ? -sp : sp;
        double depth = horizontal * cp + dy * sine;
        double vertical = dy * cp - horizontal * sine;
        // Plane normals need their lengths here: a plain +radius margin on
        // the sloped planes would incorrectly reject intersecting spheres.
        if (depth + radius >= .4 && std::abs(across) <= depth * angles.tan_h + radius * angles.plane_h &&
            std::abs(vertical) <= depth * angles.tan_v + radius * angles.plane_v) return true;
    }
    return false;
}
void append_collider(std::vector<Collider> &out, const Placement &a, float xmin, float xmax,
                     float zmin, float zmax, float ymin, float ymax, float circle_radius = 0) {
    if(extended(a)){
        if(circle_radius>0){xmin=zmin=-circle_radius;xmax=zmax=circle_radius;}
        const auto m=placement_matrix(a);double lo[3]={1e30,1e30,1e30},hi[3]={-1e30,-1e30,-1e30};
        for(unsigned k=0;k<8;k++){auto q=vw_transform::vector(m,{k&1?xmax:xmin,k&2?ymax:ymin,k&4?zmax:zmin});for(unsigned j=0;j<3;j++){lo[j]=std::min(lo[j],q[j]);hi[j]=std::max(hi[j],q[j]);}}
        Collider c;c.x=a.x;c.z=a.z;c.cosine=1;c.sine=0;c.xmin=lo[0];c.xmax=hi[0];c.zmin=lo[2];c.zmax=hi[2];c.ymin=a.y+lo[1];c.ymax=a.y+hi[1];c.circle=false;c.min_x=a.x+lo[0];c.max_x=a.x+hi[0];c.min_z=a.z+lo[2];c.max_z=a.z+hi[2];out.push_back(c);return;
    }
    Collider c;
    c.x = a.x; c.z = a.z; c.cosine = std::cos(a.yaw); c.sine = std::sin(a.yaw);
    c.xmin = xmin * a.scale; c.xmax = xmax * a.scale;
    c.zmin = zmin * a.scale; c.zmax = zmax * a.scale;
    c.ymin = a.y + ymin * a.scale; c.ymax = a.y + ymax * a.scale;
    c.circle = circle_radius > 0; c.radius = circle_radius * a.scale;
    if (c.circle) {
        c.min_x = c.x - c.radius; c.max_x = c.x + c.radius;
        c.min_z = c.z - c.radius; c.max_z = c.z + c.radius;
    } else {
        c.min_x = c.min_z = 1.e30; c.max_x = c.max_z = -1.e30;
        for (int iz = 0; iz < 2; ++iz) for (int ix = 0; ix < 2; ++ix) {
            double x = ix ? c.xmax : c.xmin, z = iz ? c.zmax : c.zmin;
            double wx = c.x + c.cosine*x + c.sine*z, wz = c.z - c.sine*x + c.cosine*z;
            c.min_x = std::min(c.min_x, wx); c.max_x = std::max(c.max_x, wx);
            c.min_z = std::min(c.min_z, wz); c.max_z = std::max(c.max_z, wz);
        }
    }
    out.push_back(c);
}
void placement_colliders(std::vector<Collider> &out, const Placement &a, const Group &g) {
    const auto &alias = aliases[a.alias];
    if (alias.collision == Collision::None) return;
    // Purchased construction assemblies need their measured wall footprints;
    // one box around a complete gate or building would seal its doorway.
    if(std::strncmp(alias.name,"nm_",3)==0){
        struct VillageCollisionBox {const char *alias;float xmin,xmax,zmin,zmax,ymin,ymax;};
#include "engine/world_nature_colliders.inc"
        bool measured=false;
        for(const auto &box:nature_collision_boxes)if(box.alias&&std::strcmp(alias.name,box.alias)==0){
            append_collider(out,a,box.xmin,box.xmax,box.zmin,box.zmax,box.ymin,box.ymax);measured=true;
        }
        if(measured)return;
    }
    if (alias.collision == Collision::Village) {
        // New village walls are collidable pieces; front doorways/interiors
        // stay open. The source cooker supplies matching measured local boxes.
        struct VillageCollisionBox {const char *alias;float xmin,xmax,zmin,zmax,ymin,ymax;};
#include "engine/world_village_colliders.inc"
#include "engine/world_castle_tavern_colliders.inc"
        for(const auto &box:village_collision_boxes)if(std::strcmp(alias.name,box.alias)==0)
            append_collider(out,a,box.xmin,box.xmax,box.zmin,box.zmax,box.ymin,box.ymax);
        for(const auto &box:castle_tavern_collision_boxes)if(std::strcmp(alias.name,box.alias)==0)
            append_collider(out,a,box.xmin,box.xmax,box.zmin,box.zmax,box.ymin,box.ymax);
    } else if (alias.collision == Collision::Drawbridge) {
#include "engine/world_castle_drawbridge_colliders.inc"
        for(const auto &box:castle_drawbridge_collision_boxes)
            append_collider(out,a,box[0],box[1],box[2],box[3],box[4],box[5]);
    } else if (alias.collision == Collision::Gate) {
        // Measured original fort gate: 3.50133m clear opening, straight jambs
        // through4.2m. Conservative header begins at the arch spring line.
        constexpr float opening_half = 3.50133f*.5f, opening_height = 4.2f;
        append_collider(out,a,g.minimum[0],-opening_half,g.minimum[2],g.maximum[2],0,g.maximum[1]);
        append_collider(out,a,opening_half,g.maximum[0],g.minimum[2],g.maximum[2],0,g.maximum[1]);
        append_collider(out,a,-opening_half,opening_half,g.minimum[2],g.maximum[2],opening_height,g.maximum[1]);
    } else if (alias.collision == Collision::Trunk) {
        append_collider(out,a,0,0,0,0,0,g.maximum[1],alias.trunk_radius);
    } else if (alias.collision == Collision::Circle) {
        float radius = std::max({std::abs(g.minimum[0]),std::abs(g.maximum[0]),std::abs(g.minimum[2]),std::abs(g.maximum[2])});
        append_collider(out,a,0,0,0,0,g.minimum[1],g.maximum[1],radius);
    } else append_collider(out,a,g.minimum[0],g.maximum[0],g.minimum[2],g.maximum[2],g.minimum[1],g.maximum[1]);
}
double point_segment_distance2(double x, double z, double ax, double az, double bx, double bz) {
    double dx=bx-ax,dz=bz-az,length=dx*dx+dz*dz;
    double t=length>0?std::clamp(((x-ax)*dx+(z-az)*dz)/length,0.,1.):0;
    double px=x-ax-dx*t,pz=z-az-dz*t; return px*px+pz*pz;
}
double segment_distance2(double ax,double az,double bx,double bz,double cx,double cz,double dx,double dz) {
    double ux=bx-ax,uz=bz-az,vx=dx-cx,vz=dz-cz,den=ux*vz-uz*vx;
    if(std::abs(den)>1.e-16) {
        double wx=cx-ax,wz=cz-az,t=(wx*vz-wz*vx)/den,u=(wx*uz-wz*ux)/den;
        if(t>=0&&t<=1&&u>=0&&u<=1)return 0;
    }
    return std::min({point_segment_distance2(ax,az,cx,cz,dx,dz),point_segment_distance2(bx,bz,cx,cz,dx,dz),
                     point_segment_distance2(cx,cz,ax,az,bx,bz),point_segment_distance2(dx,dz,ax,az,bx,bz)});
}
double rectangle_distance2(double ax,double az,double bx,double bz,const Collider &c) {
    if((ax>=c.xmin&&ax<=c.xmax&&az>=c.zmin&&az<=c.zmax)||
       (bx>=c.xmin&&bx<=c.xmax&&bz>=c.zmin&&bz<=c.zmax))return 0;
    return std::min({segment_distance2(ax,az,bx,bz,c.xmin,c.zmin,c.xmax,c.zmin),
                     segment_distance2(ax,az,bx,bz,c.xmax,c.zmin,c.xmax,c.zmax),
                     segment_distance2(ax,az,bx,bz,c.xmax,c.zmax,c.xmin,c.zmax),
                     segment_distance2(ax,az,bx,bz,c.xmin,c.zmax,c.xmin,c.zmin)});
}
bool escaping_rectangle(double x,double z,double tx,double tz,const Collider &c,double radius) {
    double dx=tx-x,dz=tz-z;
    if(dx*dx+dz*dz<1.e-20)return false;
    double nx=x-std::clamp(x,double(c.xmin),double(c.xmax));
    double nz=z-std::clamp(z,double(c.zmin),double(c.zmax));
    double distance2=nx*nx+nz*nz;
    if(distance2>0)return distance2<radius*radius&&nx*dx+nz*dz>=-1.e-10;
    // A point inside a solid can only retain or reduce its penetration. This
    // lets a player recover after landing/teleporting inside a low prop while
    // still rejecting every outside-to-inside continuous sweep.
    double margins[]={x-c.xmin,c.xmax-x,z-c.zmin,c.zmax-z};
    double best=*std::min_element(std::begin(margins),std::end(margins));
    double outward[]={-dx,dx,-dz,dz};
    for(int side=0;side<4;side++)if(margins[side]<=best+1.e-10&&outward[side]>=-1.e-10)return true;
    return false;
}

}

struct SceneryStreaming;
struct WorldScenery {
    std::uint64_t activity_revision=0;
    std::vector<Placement> placements;
    std::vector<Placement> editor_originals;
    std::vector<std::size_t> editor_records;
    std::vector<std::uint32_t> editor_seen;
    std::uint32_t editor_epoch=0;
    std::vector<Group> groups;
    std::vector<Source> sources;
    std::vector<Collider> colliders;
    std::array<Uniforms, 2> uniforms = {};
    // Color-only indirection leaves model ownership, alias indices and the
    // original shadow submission sequence intact. Fixed arrays never allocate
    // when the camera moves or a filtered/reflected draw changes its order.
    std::array<std::size_t, group_cap> color_order = {};
    std::array<double, group_cap> color_distance2 = {};
    unsigned next_uniform = 0;
    float foliage_detail_scale=1.f;
    unsigned max_texture_edge=0;
    WorldSceneryStats stats;
    WorldScenery *borrow_source=nullptr;std::string model_directory;size_t preview_record=SIZE_MAX;int preview_lod=-1;
    bool shadow_lod = std::getenv("VALWOOD_DISABLE_SHADOW_LOD")==nullptr;
    bool source_horizon=std::getenv("VALWOOD_QA_DISABLE_SOURCE_HORIZON")==nullptr;
    bool source_bounds_lod=std::getenv("VALWOOD_QA_DISABLE_SOURCE_BOUNDS_LOD")==nullptr;
    bool color_group_sort = std::getenv("VALWOOD_QA_DISABLE_SCENERY_GROUP_SORT")==nullptr;
    bool shadow_clip_active=false;
    std::array<float,16> shadow_clip_vp={};
    std::unique_ptr<SceneryStreaming> streaming;
    bool stream_requested=false;WorldSceneryStreamingConfig stream_config;
    double stream_camera_x=0,stream_camera_z=0;

    Uniforms &locations(GLuint program) {
        for (auto &u : uniforms) if (u.program==program&&u.epoch==world_program_epoch) return u;
        auto &u = uniforms[next_uniform++ % uniforms.size()];
        u.program=program;u.epoch=world_program_epoch;
        u.studio_preview=glGetUniformLocation(program,"uStudioLodPreview");
        u.studio_graph=glGetUniformLocation(program,"uStudioGraphIndex");u.studio_surface=glGetUniformLocation(program,"uSurfaceBinding");
        u.studio_tint=glGetUniformLocation(program,"uStudioTintRough");
        u.studio_normal=glGetUniformLocation(program,"uStudioNormalUV");
        u.offset = glGetUniformLocation(program, "uOffset");
        u.anchor = glGetUniformLocation(program, "uWorldAnchor");
        u.grass = glGetUniformLocation(program, "uGrass");
        u.vegetation = glGetUniformLocation(program, "uVegetation");
        u.lod = glGetUniformLocation(program, "uLodLevel");
        u.wind = glGetUniformLocation(program, "uWind");
        u.rooted_grass_wind=glGetUniformLocation(program,"uRootedGrassWind");
        u.item_wind=glGetUniformLocation(program,"uItemWindEnabled");
        u.tree_height = glGetUniformLocation(program, "uTreeHeight");
        u.prop_lod = glGetUniformLocation(program, "uPropLodRange");
        u.distance_fade = glGetUniformLocation(program, "uSceneryDistanceFade");
        u.source_distance = glGetUniformLocation(program,"uSourceChunkDistance");
        return u;
    }
};
#include "engine/world_model_texture_budget.h"
#include "engine/world_scenery_streaming.inc"

static std::uint64_t scenery_model_stamp(const std::string &path){
    std::error_code ec;const auto file=std::filesystem::u8path(path);const auto size=std::filesystem::file_size(file,ec);if(ec)return 0;
    const auto time=std::filesystem::last_write_time(file,ec);if(ec)return 0;const auto ticks=std::uint64_t(time.time_since_epoch().count());return (std::uint64_t(size)*1099511628211ull)^ticks;
}
WorldScenery *world_scenery_load(const char *path, const char *models, char *error, std::size_t capacity) {
    return world_scenery_load_files(&path,1,models,error,capacity);
}
static WorldScenery *load_scene_files(const char *const *paths,std::size_t count,const char *models,char *error,std::size_t capacity,WorldScenery *reuse,const WorldSceneryStreamingConfig *stream=nullptr,double camera_x=0,double camera_z=0,std::size_t maximum_records=record_cap,unsigned max_texture_edge=0);
WorldScenery *world_scenery_load_files(const char *const *paths, std::size_t file_count, const char *models,
                                      char *error, std::size_t capacity) {
    return load_scene_files(paths,file_count,models,error,capacity,nullptr);
}
WorldScenery *world_scenery_load_files_budgeted(const char *const *paths,std::size_t count,const char *models,unsigned edge,char *error,std::size_t capacity){
    if(!world_model_texture_budget::valid(edge)){error_text(error,capacity,"Invalid environment texture limit");return nullptr;}
    return load_scene_files(paths,count,models,error,capacity,nullptr,nullptr,0,0,record_cap,edge);
}
WorldScenery *world_scenery_load_files_streamed(const char *const *paths,std::size_t count,const char *models,double x,double z,const WorldSceneryStreamingConfig &config,char *error,std::size_t capacity){
    if(!std::isfinite(x)||!std::isfinite(z)||std::abs(x)>1e8||std::abs(z)>1e8){error_text(error,capacity,"Invalid streaming start camera");return nullptr;}
    if(!scenery_stream_config_valid(config)){error_text(error,capacity,"Invalid scenery streaming budgets");return nullptr;}
    auto *scene=load_scene_files(paths,count,models,error,capacity,nullptr,&config,x,z);if(scene&&!world_scenery_streaming(scene,true,config,error,capacity)){world_scenery_free(scene);return nullptr;}return scene;
}
WorldScenery *world_scenery_prepare_files(WorldScenery *current,const char *const *paths,std::size_t count,const char *models,char *error,std::size_t capacity){
    if(current&&current->streaming)current->streaming.reset();
    auto *next=load_scene_files(paths,count,models,error,capacity,current,current&&current->stream_requested?&current->stream_config:nullptr,current?current->stream_camera_x:0,current?current->stream_camera_z:0,prepared_record_cap,current?current->max_texture_edge:0);
    // A failed Apply may still have a temporary alias registry installed.
    // Resume at the next update, after the caller has restored that registry.
    return next;
}
static WorldScenery *load_scene_files(const char *const *paths,std::size_t file_count,const char *models,char *error,std::size_t capacity,WorldScenery *reuse,const WorldSceneryStreamingConfig *stream,double camera_x,double camera_z,std::size_t maximum_records,unsigned max_texture_edge){
    if(error&&capacity)error[0]=0;
    if(!paths||!file_count||file_count>source_cap||!models||!models[0]) {
        error_text(error,capacity,"Missing/invalid instance/model paths or source-file count");return nullptr;
    }
    for(std::size_t i=0;i<file_count;i++)if(!paths[i]||!paths[i][0]) {
        error_text(error,capacity,"Missing instance file path");return nullptr;
    }
    WorldScenery *scene=new(std::nothrow) WorldScenery;
    if(!scene)return nullptr;
    auto fail=[&](const std::string &message) {
        error_text(error,capacity,message);world_scenery_free(scene);return static_cast<WorldScenery *>(nullptr);
    };
    try {
        char directory[PATH_MAX];scene->model_directory=realpath(models,directory)?directory:models;
        if(reuse&&(reuse->model_directory!=scene->model_directory||reuse->borrow_source))reuse=nullptr;
        scene->max_texture_edge=stream?stream->max_texture_edge:max_texture_edge;
        if(reuse&&reuse->max_texture_edge!=scene->max_texture_edge)reuse=nullptr;
        scene->borrow_source=reuse;
        if(stream){scene->stream_requested=true;scene->stream_config=*stream;scene->stream_camera_x=camera_x;scene->stream_camera_z=camera_z;}
        scene->placements.reserve(maximum_records);scene->groups.reserve(group_cap);scene->sources.reserve(file_count);
        std::array<std::size_t,alias_count> counts={};
        WorldSceneryDuplicateIndex duplicate_roots;
        for(std::size_t source=0;source<file_count;source++) {
            std::ifstream file(paths[source],std::ios::binary|std::ios::ate);
            if(!file||file.tellg()<0||file.tellg()>std::streamoff(source_byte_cap))return fail("Missing or oversized instance file: "+std::string(paths[source]));
            file.seekg(0);scene->sources.push_back({paths[source],0,0,0});
            auto &source_stats=scene->sources.back();
            std::array<bool,alias_count> source_aliases={};
            std::string line;unsigned line_number=0;
            // Only this generated supplement can discard impossible roots.
            // Authored sources retain every strict validation failure.
            const bool generated_dressing=std::filesystem::u8path(paths[source]).filename()=="creek_dressing_instances.txt";
            std::size_t generated_skipped=0;
            while(std::getline(file,line)) {
                ++line_number;
                auto at_line=[&](const char *message){return std::string(paths[source])+": line "+std::to_string(line_number)+": "+message;};
                const size_t authored_bytes=line.size();
                if(authored_bytes>legacy_record_bytes+item_wind_tail_bytes)return fail(at_line("record exceeds 320 characters"));
                line.erase(line.find('#')==std::string::npos?line.size():line.find('#'));
                std::istringstream fields(line);std::string name,extra;
                if(!(fields>>name)){if(authored_bytes>legacy_record_bytes)return fail(at_line("record exceeds 256 legacy characters"));continue;}
                double u,v,yaw,scale,burial,elevation=0,offset_y=0,pitch=0,roll=0,sx=1,sy=1,sz=1,pivot=std::numeric_limits<double>::quiet_NaN(),wind_sway=1,wind_flutter=1;
                if(!(fields>>u>>v>>yaw>>scale>>burial))return fail(at_line("expected alias u v yaw_degrees scale burial_meters [elevation_meters]"));
                std::array<double,8> numeric={};size_t suffix=0;bool have_wind=false;
                while(true){
                    fields>>std::ws;const auto extra_begin=fields.tellg();
                    if(!(fields>>extra))break;
                    if(extra=="wind"){
                        if(extra_begin<=0||!std::isspace(static_cast<unsigned char>(line[size_t(extra_begin)-1])))return fail(at_line("item wind tail requires a separator"));
                        // Count exactly one separator as part of the extension.
                        // Additional pre-tag whitespace remains in the legacy
                        // base budget; comments cannot supply an extension tag.
                        const size_t base_bytes=size_t(extra_begin)-1;
                        if(base_bytes>legacy_record_bytes)return fail(at_line("item wind base exceeds 256 characters"));
                        if(authored_bytes-base_bytes>item_wind_tail_bytes)return fail(at_line("item wind tail exceeds 64 characters"));
                        if(have_wind||!(fields>>wind_sway>>wind_flutter)||(fields>>extra)||!std::isfinite(wind_sway)||wind_sway<0||wind_sway>2||!std::isfinite(wind_flutter)||wind_flutter<0||wind_flutter>2)return fail(at_line("invalid item wind response"));
                        have_wind=true;break;
                    }
                    if(suffix==numeric.size())return fail(at_line("too many numeric placement fields"));
                    std::istringstream value(extra);char leftover;
                    if(!(value>>numeric[suffix])||(value>>leftover)||!std::isfinite(numeric[suffix]))return fail(at_line("invalid numeric placement field"));
                    ++suffix;
                }
                if(!have_wind&&authored_bytes>legacy_record_bytes)return fail(at_line("record exceeds 256 legacy characters"));
                if(suffix!=0&&suffix!=1&&suffix!=2&&suffix!=7&&suffix!=8)return fail(at_line("expected elevation, offset, full transform and optional pivot"));
                if(suffix>=1)elevation=numeric[0];
                if(suffix>=2)offset_y=numeric[1];
                if(suffix>=7){pitch=numeric[2];roll=numeric[3];sx=numeric[4];sy=numeric[5];sz=numeric[6];}
                if(suffix==8){pivot=numeric[7];if(pivot< -2000||pivot>12000)return fail(at_line("invalid explicit pivot height"));}
                if(!vw_transform::extra_valid(pitch,roll,sx,sy,sz))return fail(at_line("invalid full transform"));
                auto alias=std::find_if(std::begin(aliases),std::end(aliases),[&](const Alias &a){return name==a.name;});
                if(alias==std::end(aliases))return fail(at_line("unknown model alias"));
                if(!std::isfinite(u)||!std::isfinite(v)||!std::isfinite(yaw)||!std::isfinite(scale)||!std::isfinite(burial)||!std::isfinite(elevation)||!std::isfinite(offset_y)||offset_y<-WORLD_OBJECT_OFFSET_LIMIT||offset_y>WORLD_OBJECT_OFFSET_LIMIT||
                   u<0||u>1||v<0||v>1||yaw<-360||yaw>360||scale<.15||
                   scale>(name=="broadleaf"?2.5:(alias->tree?2.:4.))||burial<0||burial>alias->max_burial||elevation<0||elevation>alias->max_elevation)
                    return fail(at_line("non-finite/out-of-range UV, yaw, scale, burial or elevation"));
                if(scene->placements.size()==maximum_records)return fail(at_line((std::to_string(maximum_records)+"-record limit exceeded").c_str()));
                if(counts[std::size_t(alias-std::begin(aliases))]==alias_record_cap)
                    return fail(at_line("4096-instance alias limit exceeded"));
                Placement placement={};placement.alias=unsigned(alias-std::begin(aliases));placement.source=unsigned(source);
                placement.source_record=source_stats.instances;
                world_atlas_uv_to_world(u,v,&placement.x,&placement.z);
                WorldAtlasSample sample=world_atlas_sample(placement.x,placement.z);
                const bool absolute_pivot=std::isfinite(pivot);
                const double support=absolute_pivot?pivot:sample.height;
                if(!std::isfinite(support)||support<.1f||(!absolute_pivot&&sample.shore_distance<.5f)){if(generated_dressing){generated_skipped++;continue;}return fail(at_line("root must be on dry, finite terrain or an explicit elevated pivot"));}
                float local_surface=0,local_depth=0;
                if(world_atlas_local_water(placement.x,placement.z,&local_surface,&local_depth)&&(!absolute_pivot||pivot<=local_surface+.1)){if(generated_dressing){generated_skipped++;continue;}return fail(at_line("root must be above local water on dry terrain"));}
                placement.y=sample.height-float(burial);placement.yaw=float(yaw*pi/180.);
                placement.scale=float(scale);placement.burial=float(burial);placement.elevation=float(elevation);placement.offset_y=float(offset_y);placement.line=line_number;placement.pitch=float(pitch*pi/180);placement.roll=float(roll*pi/180);placement.sx=float(sx);placement.sy=float(sy);placement.sz=float(sz);placement.pivot_y=float(pivot);placement.wind_sway=float(wind_sway);placement.wind_flutter=float(wind_flutter);
                if(!duplicate_roots.insert(placement.alias,placement.x,placement.z,placement.elevation,placement.offset_y)){
                    if(generated_dressing){generated_skipped++;continue;}return fail(at_line("duplicate alias/root"));
                }
                scene->placements.push_back(placement);counts[placement.alias]++;source_stats.instances++;
                if(!source_aliases[placement.alias]){source_aliases[placement.alias]=true;source_stats.aliases++;}
                if(alias->tree){scene->stats.source_trees++;source_stats.trees++;}else{scene->stats.source_props++;source_stats.props++;}
            }
            if(file.bad())return fail("Read error in instance file: "+std::string(paths[source]));
            if(generated_dressing)std::printf("[Scenery] %s: %zu accepted, %zu generated roots skipped (water/duplicate)\n",paths[source],source_stats.instances,generated_skipped);
            if(!source_stats.instances)return fail("Instance file has no placements: "+std::string(paths[source]));
        }
        for(unsigned alias=0;alias<alias_count;alias++) {
            if(!counts[alias])continue;
            scene->stats.source_aliases++;
            for(int lod=0;lod<aliases[alias].lods;lod++) {
                Group group;group.source_lod=unsigned(lod);group.alias=alias;group.lod=aliases[alias].tree?lod:-1;
                group.vegetation=aliases[alias].tree&&lod==2?4:aliases[alias].vegetation;
                // Crop shadows use only the near group's independent mid-mesh
                // proxy and fade with its56–64m interval. Far crop color never
                // turns every cluster into an all-distance shadow caster.
                group.shadow=aliases[alias].vegetation==6?lod==0:
                    (aliases[alias].tree?lod!=2:(aliases[alias].vegetation!=3||aliases[alias].cast_shadow));
                // Source MID is pinned and supplies the stable shadow mesh at
                // every distance. HIGH is color refinement, with no duplicate
                // MID geometry/material preparation or second shadow draw.
                if(scene->source_bounds_lod&&source_chunk(alias)&&!source_backdrop(alias))group.shadow=lod==1;
                const char *suffixes[]={"_near.glb","_mid.glb","_far.glb"};
                std::string asset_path=std::string(models)+"/"+aliases[alias].stem+
                    (aliases[alias].tree?suffixes[lod]:(lod?"_mid.glb":
                        (aliases[alias].named_near_file?"_near.glb":".glb")));
                group.asset_path=asset_path;group.alias_name=aliases[alias].name;group.source_stamp=scenery_model_stamp(asset_path);
                if(lod==0&&(aliases[alias].tree||aliases[alias].cast_shadow)&&!(scene->source_bounds_lod&&source_chunk(alias)))group.source_stamp^=scenery_model_stamp(std::string(models)+"/"+aliases[alias].stem+"_mid.glb")*1099511628211ull;
                const Group *previous=nullptr;std::size_t previous_index=SIZE_MAX;
                if(reuse)for(std::size_t i=0;i<reuse->groups.size();i++)if(reuse->groups[i].alias==alias&&reuse->groups[i].source_lod==unsigned(lod)&&reuse->groups[i].asset_path==asset_path&&reuse->groups[i].alias_name==group.alias_name&&reuse->groups[i].source_stamp==group.source_stamp){previous=&reuse->groups[i];previous_index=i;break;}
                bool skip=false;
                const bool interior_skip=stream&&stream->interior_release_city_fallbacks&&
                    ((stream->interior_start_active&&world_opening_interior::city_alias(group.alias_name.c_str()))||
                     (!stream->interior_start_active&&world_opening_interior::apartment_alias(group.alias_name.c_str())));
                if(interior_skip||((stream||(scene->source_horizon&&source_backdrop(alias)))&&lod==0&&aliases[alias].lods>1)){
                    if(previous){std::copy(previous->minimum,previous->minimum+3,group.minimum);std::copy(previous->maximum,previous->maximum+3,group.maximum);}
                    else{world_model_cpu::Bounds bounds;world_model_cpu::Options options;if(stream)options.cpu_bytes=stream->cpu_bytes;std::string message;if(!world_model_cpu::bounds_only(asset_path.c_str(),options,bounds,message))return fail("Required model bounds failed: "+asset_path+": "+message);std::copy(bounds.minimum,bounds.minimum+3,group.minimum);std::copy(bounds.maximum,bounds.maximum+3,group.maximum);}
                    double nearest=1e20;
                    for(const auto &p:scene->placements)if(p.alias==alias)nearest=std::min(nearest,streaming_distance(p,group.minimum,group.maximum,camera_x,camera_z));
                    const float range=aliases[alias].tree&&aliases[alias].lods==3?WORLD_TREE_NEAR_FADE_END:aliases[alias].lod_split+(aliases[alias].prop_lod_fade?4.f:0.f);
                    // The publisher's large HIGH batches must enter through the
                    // coordinator's geometry/texture reservations. Eager HIGH
                    // uploads used to overfill the budget before adoption.
                    skip=interior_skip||(scene->source_horizon&&source_backdrop(alias))||
                        (stream&&scene->source_bounds_lod&&source_chunk(alias))||
                        (stream&&nearest>range+stream->preload_meters);
                }
                const bool borrowed=!skip&&previous&&previous->asset;
                if(borrowed)group.borrowed_from=previous_index;
                std::unique_ptr<WorldModelAsset,decltype(&world_model_free)> asset(skip||borrowed?nullptr:world_model_load_budgeted(asset_path.c_str(),world_scenery_interior_residency::texture_edge(scene->max_texture_edge,unsigned(lod),false,scene->stream_config.texture_bytes)),world_model_free);
                if(!skip&&!borrowed&&!asset)return fail("Required model failed to load: "+asset_path);
                std::unique_ptr<WorldModelAsset,decltype(&world_model_free)> shadow_asset(nullptr,world_model_free);
                if(!skip&&!borrowed&&lod==0&&(aliases[alias].tree||aliases[alias].cast_shadow)&&!(scene->source_bounds_lod&&source_chunk(alias))) {
                    const std::string shadow_path=std::string(models)+"/"+aliases[alias].stem+"_mid.glb";
                    shadow_asset.reset(world_model_load_budgeted(shadow_path.c_str(),world_scenery_interior_residency::texture_edge(scene->max_texture_edge,1,true,scene->stream_config.texture_bytes)));
                    if(!shadow_asset)return fail("Required model shadow proxy failed to load: "+shadow_path);
                }
                if(!skip&&!world_model_bounds(borrowed?previous->asset:asset.get(),group.minimum,group.maximum))return fail("Missing model bounds: "+asset_path);
                group.height=group.maximum[1];
                group.rooted_grass=world_rooted_grass::capability(aliases[alias].stem,
                    group.vegetation,aliases[alias].grounding==Grounding::Root,
                    aliases[alias].collision==Collision::None,group.minimum,group.maximum);
                group.radius=std::hypot(std::max(std::abs(group.minimum[0]),std::abs(group.maximum[0])),
                                        std::max(std::abs(group.minimum[2]),std::abs(group.maximum[2])));
                group.sphere_radius=std::hypot(group.radius,group.height*.5f);
                // Existing three-stage trees retain their original crown/card
                // transition. Purchased two-mesh trees use the same stable,
                // complementary intervals as other authored mesh pairs.
                if(aliases[alias].tree&&aliases[alias].lods==3) {
                    const float starts[]={0,WORLD_TREE_NEAR_FADE_BEGIN,215},ends[]={WORLD_TREE_NEAR_FADE_END,265,1680};group.lo=starts[lod];group.hi=ends[lod];
                    if(lod==2)group.padding=alias==0?.75f:(alias==1?.66f:0.f);
                }else {
                    const auto &config=aliases[alias];
                    const float overlap=config.prop_lod_fade?4.f:0.f;
                    group.lo=lod?config.lod_split-overlap:0.f;
                    group.hi=config.lods>1&&!lod?config.lod_split+overlap:config.distance;
                    group.prop_lod=config.prop_lod_fade?int(lod+1):0;
                }
                group.base_lo=group.lo;group.base_hi=group.hi;
                group.next.reserve(counts[alias]);group.hidden.reserve(counts[alias]);
                group.uploaded.reserve(counts[alias]);group.records.reserve(counts[alias]);
                group.shadow_selected.reserve(counts[alias]);group.shadow_uploaded.reserve(counts[alias]);
                for(std::size_t i=0;i<scene->placements.size();i++)if(scene->placements[i].alias==alias)group.records.push_back(i);
                group.asset=borrowed?previous->asset:asset.get();group.shadow_asset=borrowed?previous->shadow_asset:shadow_asset.get();
                if(borrowed)scene->stats.reused_groups++;
                if(group.shadow_asset)scene->stats.shadow_proxy_groups++;
                scene->groups.push_back(std::move(group));asset.release();
                shadow_asset.release();
            }
        }
        // Both tiers classify the same authored spatial footprint; otherwise
        // simplified LOD bounds can leave a CPU selection gap in their overlap.
        for(auto &g:scene->groups){
            std::copy(g.minimum,g.minimum+3,g.lod_minimum);std::copy(g.maximum,g.maximum+3,g.lod_maximum);
            if(source_chunk(g.alias))for(const auto &other:scene->groups)if(other.alias==g.alias)
                for(unsigned k=0;k<3;k++){g.lod_minimum[k]=std::min(g.lod_minimum[k],other.minimum[k]);g.lod_maximum[k]=std::max(g.lod_maximum[k],other.maximum[k]);}
            std::copy(g.minimum,g.minimum+3,g.shadow_minimum);std::copy(g.maximum,g.maximum+3,g.shadow_maximum);
            // The near group's shadow proxy may differ from its color mesh.
            // All alias LOD bounds enclose both, even while HIGH is streamed.
            for(const auto &other:scene->groups)if(other.alias==g.alias)for(unsigned k=0;k<3;++k){g.shadow_minimum[k]=std::min(g.shadow_minimum[k],other.minimum[k]);g.shadow_maximum[k]=std::max(g.shadow_maximum[k],other.maximum[k]);}
            if(g.shadow_asset){float lo[3],hi[3];if(world_model_bounds(g.shadow_asset,lo,hi))for(unsigned k=0;k<3;++k){g.shadow_minimum[k]=std::min(g.shadow_minimum[k],lo[k]);g.shadow_maximum[k]=std::max(g.shadow_maximum[k],hi[k]);}}
        }
        scene->colliders.reserve(scene->placements.size()*3);
        for(auto &placement:scene->placements) {
            const auto group=std::find_if(scene->groups.begin(),scene->groups.end(),[&](const Group &g){return g.alias==placement.alias;});
            const bool trail_intrusion=aliases[placement.alias].collision!=Collision::Gate&&aliases[placement.alias].grounding!=Grounding::GroundTop&&trail_clearance(placement.x,placement.z)<group->radius*placement.scale+.35;
            const bool over_buried=placement.burial>group->height*placement.scale*.6f;
            if((trail_intrusion||over_buried)&&std::filesystem::u8path(scene->sources[placement.source].path).filename()=="creek_dressing_instances.txt"){
                placement.active=false;continue;
            }
            if(trail_intrusion)
                return fail(scene->sources[placement.source].path+": line "+std::to_string(placement.line)+": model footprint intrudes on the authored trail");
            if(over_buried)
                return fail(scene->sources[placement.source].path+": line "+std::to_string(placement.line)+": burial hides too much of the model");
            if(rigid(placement.alias)) {
                placement.y=supported_height(placement,*group);
                if(!std::isfinite(placement.y))return fail("Non-finite terrain below model footprint");
            }
            if(aliases[placement.alias].grounding==Grounding::GroundTop)
                placement.y+=.02f-group->maximum[1]*placement.scale;
            placement.y+=placement.elevation+placement.offset_y;if(std::isfinite(placement.pivot_y))placement.y=placement.pivot_y;
            std::size_t first_collider=scene->colliders.size();
            placement_colliders(scene->colliders,placement,*group);
            placement.collider_first=first_collider;placement.collider_count=scene->colliders.size()-first_collider;
            for(std::size_t c=first_collider;c<scene->colliders.size();c++)scene->colliders[c].record=std::size_t(&placement-scene->placements.data());
            scene->sources[placement.source].colliders+=scene->colliders.size()-first_collider;
            if(aliases[placement.alias].collision==Collision::Gate) {
                std::size_t trail_count=0;const auto *trail=world_atlas_trail(&trail_count);
                for(std::size_t k=1;k<trail_count;k++)for(std::size_t post=0;post<2;post++) {
                    const auto &c=scene->colliders[first_collider+post];
                    double ax=trail[k-1].x-c.x,az=trail[k-1].z-c.z,bx=trail[k].x-c.x,bz=trail[k].z-c.z;
                    if(rectangle_distance2(c.cosine*ax-c.sine*az,c.sine*ax+c.cosine*az,
                                           c.cosine*bx-c.sine*bz,c.sine*bx+c.cosine*bz,c)<.37*.37)
                        return fail("Gate pier intrudes on the authored trail walking capsule");
                }
                if(trail_clearance(placement.x,placement.z)<group->radius*placement.scale+.35&&
                   (4.2f*placement.scale-placement.burial<1.85f||3.50133f*placement.scale<.74f))
                    return fail("Gate opening is too small for the authored trail walking capsule");
            }
        }
        scene->editor_originals.resize(scene->placements.size());
        scene->editor_records.reserve(scene->placements.size());
        scene->editor_seen.resize(scene->placements.size());
        scene->stats.source_instances=scene->placements.size();scene->stats.loaded_groups=scene->groups.size();
        scene->stats.source_files=scene->sources.size();scene->stats.source_colliders=scene->colliders.size();
        return scene;
    }catch(const std::bad_alloc &){return fail("Scenery allocation failed");}
}

void world_scenery_update(WorldScenery *scene, double camera_x, double camera_y, double camera_z,
                          float yaw, float pitch, float aspect, float tan_vertical_half_fov, float foliage_detail_scale,
                          float foliage_density) {
    if (!scene||scene->borrow_source) return;
    const float detail=std::isfinite(foliage_detail_scale)?std::clamp(foliage_detail_scale,.65f,2.f):1.f;
    if(scene->foliage_detail_scale!=detail){
        scene->foliage_detail_scale=detail;
        for(auto &g:scene->groups){
            const auto &a=aliases[g.alias];
            const float scale=scenery_alias_detail_scale(a,detail);
            g.lo=g.base_lo*scale;
            // Keep distant silhouettes at their original horizon range.
            g.hi=a.tree&&a.lods==3&&g.source_lod==2?g.base_hi:g.base_hi*scale;
        }
    }
    if(std::isfinite(camera_x)&&std::isfinite(camera_z)){scene->stream_camera_x=camera_x;scene->stream_camera_z=camera_z;}
    if(scene->stream_requested&&!scene->streaming)(void)world_scenery_streaming(scene,true,scene->stream_config);
    if(scene->streaming)scene->streaming->update(camera_x,camera_y,camera_z);
    // A retained shadow batch must not keep an evicted/replaced model alive
    // beyond this owner update. A freshly prepared scene owns fresh batches.
    for(auto &g:scene->groups){auto *target=scene->shadow_lod&&g.shadow_asset?g.shadow_asset:g.asset;if(g.shadow_batch_target!=target){world_model_instance_batch_free(g.shadow_batch);g.shadow_batch=nullptr;g.shadow_batch_target=nullptr;g.shadow_batch_failed=false;g.shadow_uploaded.clear();}}
    auto &stats = scene->stats;
    stats.near_trees = stats.mid_trees = stats.far_trees = stats.props = stats.largest_batch = stats.uploads_this_update = 0;
    stats.color_instances = stats.shadow_instances = 0;
    stats.foliage_density_candidates = stats.foliage_density_rejected = 0;
    const float density=std::isfinite(foliage_density)?std::clamp(foliage_density,0.f,1.f):1.f;
    if (!std::isfinite(camera_x) || !std::isfinite(camera_y) || !std::isfinite(camera_z) ||
        !std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(aspect) || aspect <= 0 ||
        !std::isfinite(tan_vertical_half_fov) || tan_vertical_half_fov < 0) return;
    double anchor_x = std::floor(camera_x / 128.) * 128., anchor_z = std::floor(camera_z / 128.) * 128.;
    // Zero is the existing normal-view projection, evaluated with the exact
    // previous double-precision expression. Probe captures can select a full
    // 90 degree face using tangent 1 without changing ordinary callers.
    const double tan_v = tan_vertical_half_fov > 0 ? double(tan_vertical_half_fov) : std::tan(.54);
    const double tan_h = tan_v * aspect;
    const auto *pool=world_atlas_rainforest_pool();
    double pool_dx=camera_x-pool->x,pool_dz=camera_z-pool->z;
    double pool_surface=pool_dx*pool_dx+pool_dz*pool_dz<=220.*220.?pool->surface_y:std::nan("");
    double village_x,village_z;world_atlas_village_origin(&village_x,&village_z);
    const double local_x=camera_x-village_x,local_z=camera_z-village_z;
    if(local_x>WORLD_CASTLE_CREEK_MIN_X-120&&local_x<WORLD_CASTLE_CREEK_MAX_X+120&&
       local_z>WORLD_CASTLE_CREEK_MIN_Z-120&&local_z<WORLD_CASTLE_CREEK_MAX_Z+120)
        pool_surface=WORLD_CASTLE_CREEK_SURFACE_Y;
    const ViewAngles angles={std::cos(yaw),std::sin(yaw),std::cos(pitch),std::sin(pitch),
                             tan_h,tan_v,std::hypot(1.,tan_h),std::hypot(1.,tan_v),pool_surface};
    for (auto &group : scene->groups) {
        if(!group.asset){group.visible_count=0;continue;}
        group.fallback=false;
        if(group.source_lod==1&&!(scene->source_horizon&&source_backdrop(group.alias)))for(const auto &other:scene->groups)if(other.alias==group.alias&&other.source_lod==0&&!other.asset){group.fallback=true;break;}
        group.next.clear();
        group.hidden.clear();
        for (const auto index : group.records) {
            const auto &placement = scene->placements[index];
            if(!placement.active)continue;
            const auto &alias=aliases[placement.alias];
            const bool density_eligible=!alias.tree&&alias.collision==Collision::None&&
                (alias.vegetation==2||alias.vegetation==3||alias.vegetation==5);
            if(density_eligible&&index!=scene->preview_record){
                ++stats.foliage_density_candidates;
                // Source/record identity is shared by all LODs and independent
                // of camera, source root transforms and current visibility.
                // Default1 bypasses hashing and preserves every old batch.
                const std::uint64_t seed=std::uint64_t(placement.source)*0x9e3779b97f4a7c15ull^
                                         std::uint64_t(placement.source_record);
                if(density<1.f&&!world_ground_cover_quality::keep(seed,density)){
                    ++stats.foliage_density_rejected;continue;
                }
            }
            double dx = placement.x - camera_x, dz = placement.z - camera_z, distance2 = dx * dx + dz * dz;
            bool lod_color_visible=true;
            const bool preview=index==scene->preview_record&&scene->preview_lod>=0;
            if(preview){if(int(group.source_lod)!=scene->preview_lod||distance2>128.*128.)continue;}
            else if(scene->source_horizon&&source_backdrop(group.alias)){
                // Source mountain LOD0 is intentionally identical to MID.
                // Its authored root can be kilometres outside the visible mesh.
                if(group.source_lod!=1)continue;
            }else if(scene->source_bounds_lod&&source_chunk(group.alias)&&!source_backdrop(group.alias)){
                const auto range=source_distances(placement,group.lod_minimum,group.lod_maximum,camera_x,camera_y,camera_z);
                if(range.nearest>=group.hi)continue;
                if(range.farthest<(group.fallback?0:group.lo)){
                    if(group.source_lod==1&&group.shadow)lod_color_visible=false;else continue;
                }
            }else if (distance2 < (group.fallback?0:group.lo * group.lo) || distance2 >= group.hi * group.hi) continue;
            bool color_visible = lod_color_visible&&visible(placement, group, camera_x, camera_y, camera_z, angles);
            if (!group.shadow && !color_visible) continue;
            auto &partition = color_visible ? group.next : group.hidden;
            const auto shift=extended(placement)?vw_transform::vector(placement_matrix(placement),{0,-group.padding,0}):vw_transform::V{0,-group.padding*placement.scale,0};
            partition.push_back({float(placement.x - anchor_x+shift[0]), float(placement.y+shift[1]),
                                 float(placement.z - anchor_z+shift[2]), placement.yaw, placement.scale,placement.pitch,placement.roll,placement.sx-1,placement.sy-1,placement.sz-1,placement.wind_sway,placement.wind_flutter});
        }
        const std::size_t visible_count = group.next.size();
        // Cutout models write depth. Keep the visible prefix near-to-far so
        // nearer leaves can reject hidden fragments, without dropping casters.
        // Use the existing reserved buffer and a deterministic tie-breaker;
        // stationary views therefore retain byte-identical uploaded batches.
        const double relative_x=camera_x-anchor_x,relative_z=camera_z-anchor_z;
        std::sort(group.next.begin(),group.next.end(),[&](const WorldModelInstance &a,const WorldModelInstance &b) {
            const double ax=double(a.x)-relative_x,az=double(a.z)-relative_z;
            const double bx=double(b.x)-relative_x,bz=double(b.z)-relative_z;
            const double da=ax*ax+az*az,db=bx*bx+bz*bz;
            if(da!=db)return da<db;
            return std::tie(a.x,a.z,a.y,a.yaw,a.scale)<std::tie(b.x,b.z,b.y,b.yaw,b.scale);
        });
        group.next.insert(group.next.end(), group.hidden.begin(), group.hidden.end());
        bool changed = group.next.size() != group.uploaded.size() || (!group.next.empty() &&
            std::memcmp(group.next.data(), group.uploaded.data(), group.next.size() * sizeof(WorldModelInstance)) != 0);
        if (changed && world_model_upload_instances(group.asset, group.next.data(), group.next.size())) {
            const bool proxy_ready=!group.shadow_asset||world_model_upload_instances(group.shadow_asset,group.next.data(),group.next.size());
            if(proxy_ready) {
                group.uploaded = group.next; group.anchor_x = anchor_x; group.anchor_z = anchor_z;
                stats.uploads_this_update++; stats.uploads_total++;
                group.visible_count = visible_count;
            } else {
                // Preserve the old committed anchor and both instance batches
                // if the second upload fails. Valid retained input cannot fail
                // validation and the reusable staging is already allocated.
                (void)world_model_upload_instances(group.asset,group.uploaded.data(),group.uploaded.size());
            }
        }
        if (!changed) group.visible_count = visible_count;
        stats.largest_batch = std::max(stats.largest_batch, group.uploaded.size());
        stats.color_instances += group.visible_count;
        if (group.shadow) stats.shadow_instances += group.uploaded.size();
        if (!aliases[group.alias].tree) stats.props += group.uploaded.size();
        else if (group.lod == 0) stats.near_trees += group.uploaded.size();
        else if (group.lod == 1) stats.mid_trees += group.uploaded.size();
        else stats.far_trees += group.uploaded.size();
    }
}

void world_scenery_shadow_clip(WorldScenery *scene,const float *vp){
    if(!scene)return;
    scene->shadow_clip_active=vp!=nullptr;
    if(vp)for(unsigned k=0;k<16;++k)if(!std::isfinite(vp[k])){scene->shadow_clip_active=false;break;}
    if(scene->shadow_clip_active)std::copy(vp,vp+16,scene->shadow_clip_vp.begin());
    scene->stats.shadow_clip_enabled|=scene->shadow_clip_active;
}

static void scenery_draw_shadow_clipped(WorldScenery *scene,Group &g,WorldModelAsset *asset,
                                       unsigned program,double x,double y,double z){
    auto &stats=scene->stats;const size_t count=g.uploaded.size();stats.shadow_clip_tested+=count;
    auto original=[&](){stats.shadow_clip_submitted+=count;world_model_draw_instances(asset,program,count);};
    // Material vertex graphs have no certified displacement bound. Billboard
    // cards can rotate toward a capture camera; neither uses a rigid box test.
    if(g.graph>=0||g.vegetation==4||g.shadow_batch_failed){stats.shadow_clip_fail_open+=count;original();return;}
    const double ox=float(g.anchor_x-x),oy=float(-y),oz=float(g.anchor_z-z);
    g.shadow_selected.clear();
    for(const auto &a:g.uploaded){
        // Wind is applied AFTER full source TRS, with absolute shader caps:
        // ordinary gust/lean/flutter plus contact stay below2.3m even at the
        // maximum .65 wind and2x item channels; rooted grass is capped .30m,
        // crops below .7m. Scaling this guard by mesh size needlessly retains
        // off-map crowns. Four world metres enclose every certified branch;
        // transform() separately adds its coordinate-sized float margin.
        // Unbounded custom vertex graphs/billboards retain their fail-open path.
        constexpr double padding=4.;
        world_scenery_shadow_visibility::Bounds box;
        const bool bounded=world_scenery_shadow_visibility::transform(g.shadow_minimum,g.shadow_maximum,a,ox,oy,oz,padding,box);
        if(!bounded){++stats.shadow_clip_fail_open;g.shadow_selected.push_back(a);}
        else if(world_scenery_shadow_visibility::visible(scene->shadow_clip_vp.data(),box))g.shadow_selected.push_back(a);
    }
    if(g.shadow_selected.size()==count){original();return;}
    if(g.shadow_selected.empty()){stats.shadow_clip_rejected+=count;return;}
    if(g.shadow_batch_target!=asset){world_model_instance_batch_free(g.shadow_batch);g.shadow_batch=nullptr;g.shadow_batch_target=asset;g.shadow_uploaded.clear();}
    if(!g.shadow_batch)g.shadow_batch=world_model_instance_batch_create(asset,std::max<size_t>(1,g.records.size()));
    const bool changed=g.shadow_selected.size()!=g.shadow_uploaded.size()||
        (!g.shadow_selected.empty()&&std::memcmp(g.shadow_selected.data(),g.shadow_uploaded.data(),g.shadow_selected.size()*sizeof(WorldModelInstance))!=0);
    if(!g.shadow_batch||(changed&&!world_model_instance_batch_upload(g.shadow_batch,g.shadow_selected.data(),g.shadow_selected.size()))){
        ++stats.shadow_clip_upload_failures;stats.shadow_clip_fail_open+=count;g.shadow_batch_failed=true;original();return;
    }
    if(changed)g.shadow_uploaded.assign(g.shadow_selected.begin(),g.shadow_selected.end());
    stats.shadow_clip_rejected+=count-g.shadow_selected.size();stats.shadow_clip_submitted+=g.shadow_selected.size();
    world_model_instance_batch_draw(g.shadow_batch,program);
}

void world_scenery_draw_filtered(WorldScenery *scene, unsigned program, double x, double y, double z,
                                 bool shadow, const char *only_alias, const char *exclude_alias) {
    if (!scene || scene->borrow_source || !program) return;
    auto &u = scene->locations(program);
    glUseProgram(program);
    glUniform1i(u.grass, 0);
    std::size_t color_groups = 0;
    if (!shadow && scene->color_group_sort) {
        for (std::size_t index = 0; index < scene->groups.size(); ++index) {
            const auto &group = scene->groups[index];
            if (!group.visible_count) continue;
            double nearest = std::numeric_limits<double>::infinity();
            for (std::size_t instance = 0; instance < group.visible_count; ++instance) {
                const auto &a = group.uploaded[instance];
                // The source's root-centered XZ circle and actual vertical
                // interval enclose every rotated/scaled vertex. Distance to
                // this cylinder is a conservative lower bound, including tall
                // roofs and asymmetric tree crowns. Shadow-only suffix roots
                // never pull an otherwise distant color group to the front.
                const double dx = group.anchor_x + a.x - x;
                const double dz = group.anchor_z + a.z - z;
                const double radial = std::max(0., std::hypot(dx, dz) - group.radius * a.scale);
                const double low = a.y + group.minimum[1] * a.scale;
                const double high = a.y + group.maximum[1] * a.scale;
                const double vertical = std::max({low - y, 0., y - high});
                nearest = std::min(nearest, radial * radial + vertical * vertical);
            }
            scene->color_distance2[index] = nearest;
            scene->color_order[color_groups++] = index;
        }
        std::sort(scene->color_order.begin(), scene->color_order.begin() + color_groups,
                  [&](std::size_t a, std::size_t b) {
            if (scene->color_distance2[a] != scene->color_distance2[b])
                return scene->color_distance2[a] < scene->color_distance2[b];
            const auto &aa = scene->groups[a];
            const auto &bb = scene->groups[b];
            return std::tie(aa.alias, aa.lod, a) < std::tie(bb.alias, bb.lod, b);
        });
    }
    const std::size_t submissions = !shadow && scene->color_group_sort ? color_groups : scene->groups.size();
    for (std::size_t index = 0; index < submissions; ++index) {
        auto &group = scene->groups[!shadow && scene->color_group_sort ? scene->color_order[index] : index];
        const char *name=aliases[group.alias].name;
        if((only_alias&&std::strcmp(name,only_alias)!=0)||(exclude_alias&&std::strcmp(name,exclude_alias)==0))continue;
        const std::size_t count = shadow ? (group.shadow ? group.uploaded.size() : 0) : group.visible_count;
        if (!count||!group.asset) continue;
        glUniform3f(u.offset, float(group.anchor_x - x), float(-y), float(group.anchor_z - z));
        glUniform2f(u.anchor, float(group.anchor_x), float(group.anchor_z));
        if(u.studio_preview>=0){const bool preview=!shadow&&scene->preview_record<scene->placements.size()&&scene->placements[scene->preview_record].alias==group.alias;if(preview){const auto &p=scene->placements[scene->preview_record];glUniform3f(u.studio_preview,float(p.x-x),float(p.z-z),1);}else glUniform3f(u.studio_preview,0,0,0);}
        glUniform1i(u.vegetation, group.vegetation);
        glUniform1i(u.lod, group.fallback?-1:(aliases[group.alias].tree&&aliases[group.alias].lods==2?-1:group.lod));
        const bool horizon=scene->source_horizon&&source_backdrop(group.alias);
        glUniform1i(u.source_distance,scene->source_bounds_lod&&source_chunk(group.alias)&&!source_backdrop(group.alias));
        const bool source_mid_shadow=shadow&&scene->source_bounds_lod&&source_chunk(group.alias)&&group.source_lod==1;
        const auto &alias=aliases[group.alias];
        const float detail=scenery_alias_detail_scale(alias,scene->foliage_detail_scale);
        glUniform3f(u.prop_lod, horizon?0.f:alias.lod_split*detail, 4.f*detail, group.fallback||horizon||source_mid_shadow?0.f:float(group.prop_lod));
        const float end=horizon?0.f:aliases[group.alias].distance*(alias.tree&&alias.lods==3?1.f:detail);
        glUniform2f(u.distance_fade, aliases[group.alias].distance_fade&&!horizon?end-std::max(10.f,end*.12f):0.f,
                    aliases[group.alias].distance_fade&&!horizon?end:0.f);
        glUniform1f(u.wind, aliases[group.alias].tree ? .65f : (group.vegetation == 6 ? .60f : (group.vegetation == 3 ? .45f : 0.f)));
        glUniform1i(u.rooted_grass_wind,group.rooted_grass);
        glUniform1i(u.item_wind,1);
        const float physical_tree_heights[]={25.f,22.f,10.f,3.2f};
        glUniform1f(u.tree_height, group.alias<4?physical_tree_heights[group.alias]:group.height);
        if(u.studio_graph>=0)glUniform1i(u.studio_graph,group.graph);
        if(u.studio_surface>=0)glUniform1i(u.studio_surface,group.surface);
        glUniform4f(u.studio_tint,group.tint[0],group.tint[1],group.tint[2],group.roughness);
        glUniform2f(u.studio_normal,group.normal,group.uv);
        auto *asset=shadow&&scene->shadow_lod&&group.shadow_asset?group.shadow_asset:group.asset;
        if(shadow&&scene->shadow_clip_active)scenery_draw_shadow_clipped(scene,group,asset,program,x,y,z);
        else world_model_draw_instances(asset, program, count);
    }
    // Programs are shared with procedural dressing; no prop fade may leak into
    // the next frame or another geometry producer.
    if(u.studio_graph>=0)glUniform1i(u.studio_graph,-1);
    if(u.studio_surface>=0)glUniform1i(u.studio_surface,-1);
    glUniform4f(u.studio_tint,1,1,1,1);glUniform2f(u.studio_normal,1,1);
    glUniform1i(u.source_distance,0);
    glUniform1i(u.rooted_grass_wind,0);
    glUniform1i(u.item_wind,0);
    glUniform3f(u.prop_lod, 0.f, 0.f, 0.f);
    glUniform2f(u.distance_fade, 0.f, 0.f);
    if(u.studio_preview>=0)glUniform3f(u.studio_preview,0,0,0);
}

void world_scenery_draw(WorldScenery *scene, unsigned program, double x, double y, double z, bool shadow) {
    world_scenery_draw_filtered(scene,program,x,y,z,shadow,nullptr,nullptr);
}

WorldSceneryStats world_scenery_stats(const WorldScenery *scene) {if(!scene)return {};auto result=scene->stats;result.loaded_groups=result.shadow_proxy_groups=0;for(const auto &g:scene->groups){result.loaded_groups+=g.asset!=nullptr;result.shadow_proxy_groups+=g.shadow_asset!=nullptr;}return result;}
bool world_scenery_explicit_pivot(const WorldScenery *scene,std::size_t index) {
    return scene&&index<scene->placements.size()&&std::isfinite(scene->placements[index].pivot_y);
}
bool world_scenery_placement(const WorldScenery *scene, std::size_t index, WorldSceneryPlacement *out) {
    if (!scene || !out || index >= scene->placements.size()) return false;
    const auto &a = scene->placements[index];
    auto group = std::find_if(scene->groups.begin(), scene->groups.end(), [&](const Group &g) { return g.alias == a.alias; });
    if (group == scene->groups.end()) return false;
    out->alias = aliases[a.alias].name; out->x = a.x; out->z = a.z;
    out->y = a.y; out->yaw = a.yaw; out->scale = a.scale; out->burial = a.burial;
    out->rigid = rigid(a.alias); out->foundation = aliases[a.alias].grounding == Grounding::Foundation || aliases[a.alias].grounding == Grounding::GroundTop;
    // Palm cards use the prop rendering path, but still form a real canopy.
    out->tree = aliases[a.alias].tree || std::strcmp(out->alias,"bep_nature_palm")==0;
    out->elevation = a.elevation;out->offset_y=a.offset_y;out->pitch=a.pitch;out->roll=a.roll;out->sx=a.sx;out->sy=a.sy;out->sz=a.sz;
    out->source_file = a.source;
    out->source_record=a.source_record;out->source_line=a.line;out->wind_sway=a.wind_sway;out->wind_flutter=a.wind_flutter;
    std::copy(group->minimum, group->minimum + 3, out->minimum);
    std::copy(group->maximum, group->maximum + 3, out->maximum);
    return true;
}
bool world_scenery_placement_active(const WorldScenery*scene,std::size_t index) noexcept {
    return scene&&index<scene->placements.size()&&scene->placements[index].active;
}
std::uint64_t world_scenery_activity_revision(const WorldScenery*scene) noexcept {return scene?scene->activity_revision:0;}
WorldScenerySourceStats world_scenery_source_stats(const WorldScenery *scene,std::size_t index) {
    if(!scene||index>=scene->sources.size())return {};
    const auto &source=scene->sources[index];return {source.path.c_str(),source.instances,source.trees,source.props,source.aliases,source.colliders};
}
bool world_scenery_can_move(const WorldScenery *scene,double from_x,double from_z,double to_x,double to_z,
                            float feet_y,float player_height,float radius) {
    if(!std::isfinite(from_x)||!std::isfinite(from_z)||!std::isfinite(to_x)||!std::isfinite(to_z)||
       !std::isfinite(feet_y)||!std::isfinite(player_height)||!std::isfinite(radius)||player_height<=0||player_height>100||radius<0||radius>10||
       std::max({std::abs(from_x),std::abs(from_z),std::abs(to_x),std::abs(to_z)})>1.e9)return false;
    if(!scene)return true;
    double min_x=std::min(from_x,to_x)-radius,max_x=std::max(from_x,to_x)+radius;
    double min_z=std::min(from_z,to_z)-radius,max_z=std::max(from_z,to_z)+radius;
    for(const auto &c:scene->colliders) {
        if(c.record<scene->placements.size()&&!scene->placements[c.record].active)continue;
        if(feet_y>=c.ymax||feet_y+player_height<=c.ymin||max_x<c.min_x||min_x>c.max_x||max_z<c.min_z||min_z>c.max_z)continue;
        if(c.circle) {
            double combined=double(radius)+c.radius;
            double dx=from_x-c.x,dz=from_z-c.z,move_x=to_x-from_x,move_z=to_z-from_z;
            if(dx*dx+dz*dz<combined*combined&&move_x*move_x+move_z*move_z>1.e-20&&dx*move_x+dz*move_z>=-1.e-10)continue;
            if(point_segment_distance2(c.x,c.z,from_x,from_z,to_x,to_z)<combined*combined-1.e-10)return false;
        }else {
            double ax=from_x-c.x,az=from_z-c.z,bx=to_x-c.x,bz=to_z-c.z;
            if(escaping_rectangle(c.cosine*ax-c.sine*az,c.sine*ax+c.cosine*az,
                                   c.cosine*bx-c.sine*bz,c.sine*bx+c.cosine*bz,c,radius))continue;
            double distance=rectangle_distance2(c.cosine*ax-c.sine*az,c.sine*ax+c.cosine*az,
                                                c.cosine*bx-c.sine*bz,c.sine*bx+c.cosine*bz,c);
            if(distance<=0||distance<double(radius)*radius-1.e-10)return false;
        }
    }
    return true;
}
void world_scenery_free(WorldScenery *scene) {
    if (!scene) return;
    scene->streaming.reset();
    for (auto &group : scene->groups) {
        world_model_instance_batch_free(group.shadow_batch);group.shadow_batch=nullptr;
        if(group.borrowed_from!=std::numeric_limits<std::size_t>::max())continue;
        if(group.shadow_asset)world_model_free(group.shadow_asset);
        if(group.asset)world_model_free(group.asset);
    }
    delete scene;
}

std::size_t world_scenery_alias_count(){return active_alias_count;}
std::uint64_t world_scenery_alias_revision() noexcept{return alias_revision;}
WorldSceneryAlias world_scenery_alias(std::size_t i){if(i>=active_alias_count)return {};const auto &a=aliases[i];unsigned collision=a.collision==Collision::None?0:(a.collision==Collision::Circle?2:(a.collision==Collision::Trunk?3:((a.collision==Collision::Box)?1:4)));return {a.name,a.max_burial,a.max_elevation,a.tree?2.f:4.f,a.tree,a.stem,unsigned(a.lods),collision,a.tree?2u:(a.vegetation==3?1u:0u),a.distance,a.lod_split,a.named_near_file,a.tree||a.vegetation==3||a.vegetation==6};}
void world_scenery_materials(WorldScenery *scene,const WorldSceneryMaterial *m,std::size_t count){
    if(!scene||count>64||(count&&!m))return;
    for(auto &g:scene->groups){g.tint[0]=g.tint[1]=g.tint[2]=g.roughness=g.normal=g.uv=1;
        for(std::size_t i=0;i<count;i++)if(m[i].alias&&std::strcmp(m[i].alias,aliases[g.alias].name)==0){
            std::copy(m[i].tint,m[i].tint+3,g.tint);g.roughness=m[i].roughness;g.normal=m[i].normal;g.uv=m[i].uv;break;
        }
    }
}

void world_scenery_graphs(WorldScenery *scene,const char *const *names,std::size_t count){if(!scene||count>8||(count&&!names))return;for(auto &g:scene->groups){g.graph=-1;for(unsigned i=0;i<count;i++)if(names[i]&&g.alias_name==names[i]){g.graph=int(i);break;}}}
void world_scenery_surfaces(WorldScenery *scene,const char *const *names,std::size_t count){if(!scene||count>64||(count&&!names))return;for(auto &g:scene->groups){g.surface=-1;for(unsigned i=0;i<count;i++)if(names[i]&&g.alias_name==names[i]){g.surface=int(i);break;}}}


bool world_scenery_prepared_valid(const WorldScenery *prepared,const WorldScenery *current){
    if(!prepared||prepared==current)return false;
    if(!prepared->borrow_source)return true;
    if(prepared->borrow_source!=current)return false;
    const auto none=std::numeric_limits<std::size_t>::max();
    // Validate every transfer before detaching even one resource.
    for(const auto &g:prepared->groups)if(g.borrowed_from!=none){if(g.borrowed_from>=current->groups.size())return false;const auto &old=current->groups[g.borrowed_from];if(old.asset!=g.asset||old.shadow_asset!=g.shadow_asset)return false;}
    return true;
}
bool world_scenery_commit_prepared(WorldScenery *prepared,WorldScenery *current){
    if(!world_scenery_prepared_valid(prepared,current))return false;
    const auto none=std::numeric_limits<std::size_t>::max();
    for(auto &g:prepared->groups)if(g.borrowed_from!=none){auto &old=current->groups[g.borrowed_from];old.asset=old.shadow_asset=nullptr;g.borrowed_from=none;}
    prepared->borrow_source=nullptr;return true;
}

bool world_scenery_alias_bounds(const WorldScenery *scene,const char *name,float minimum[3],float maximum[3]){
 if(!scene||!name||!minimum||!maximum)return false;
 for(const auto &g:scene->groups)if(g.source_lod==0&&g.alias_name==name){std::copy(g.minimum,g.minimum+3,minimum);std::copy(g.maximum,g.maximum+3,maximum);return true;}
 return false;
}
bool world_scenery_grounded_pivot(const WorldScenery *scene,const WorldSceneryPlacement &pose,WorldSceneryGroundHeight sample,const void *context,double &out){
 if(!scene||!pose.alias||!sample)return false;
 const auto group=std::find_if(scene->groups.begin(),scene->groups.end(),[&](const Group &g){return g.source_lod==0&&g.alias_name==pose.alias;});
 if(group==scene->groups.end())return false;
 Placement p{};p.alias=group->alias;p.x=pose.x;p.z=pose.z;p.yaw=pose.yaw;p.scale=pose.scale;p.pitch=pose.pitch;p.roll=pose.roll;p.sx=pose.sx;p.sy=pose.sy;p.sz=pose.sz;p.burial=pose.burial;p.elevation=pose.elevation;
 p.y=float(sample(p.x,p.z,context))-p.burial;
 if(rigid(p.alias))p.y=supported_height(p,*group,sample,context);
 if(aliases[p.alias].grounding==Grounding::GroundTop)p.y+=.02f-group->maximum[1]*p.scale;
 const double root=double(p.y)+p.elevation;if(!std::isfinite(root))return false;out=root;return true;
}
bool world_scenery_configure(const WorldSceneryDefinition *defs,size_t count,char *error,size_t capacity){
 if(count>WORLD_CUSTOM_ASSET_LIMIT||(count&&!defs)){error_text(error,capacity,"Asset definition limit1024");return false;}
 auto next=aliases;for(size_t i=0;i<builtin_count;i++)next[i]=builtins[i];for(size_t i=builtin_count;i<alias_count;i++){next[i]={};next[i].name=next[i].stem="";}
 std::array<std::string,WORLD_CUSTOM_ASSET_LIMIT> names,stems;size_t active=builtin_count;
 auto token=[](const char *p,bool path){if(!p||!*p||std::strlen(p)>(path?230u:95u)||*p=='/'||std::strstr(p,".."))return false;for(;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||(path&&*p=='/')))return false;return true;};
 for(size_t i=0;i<count;i++){const auto &d=defs[i];if(!token(d.name,false)||!token(d.stem,true)||d.lods<1||d.lods>3||d.collision>4||d.vegetation>2||!std::isfinite(d.distance)||d.distance<30||d.distance>2000||!std::isfinite(d.split)||d.split<10||d.split>500||d.split+4>=d.distance||(d.vegetation==2&&d.lods<2)||(d.vegetation!=2&&d.lods>2)){error_text(error,capacity,"Invalid asset/LOD/collision definition");return false;}for(size_t j=0;j<i;j++)if(std::strcmp(d.name,defs[j].name)==0){error_text(error,capacity,"Duplicate asset definition");return false;}
  size_t index=builtin_count;for(size_t k=0;k<builtin_count;k++)if(std::strcmp(builtins[k].name,d.name)==0){index=k;break;}if(index==builtin_count)index=active++;
  names[i]=d.name;stems[i]=d.stem;Alias a=index<builtin_count?builtins[index]:Alias{"","",false,2,220,0,1,Grounding::Support,Collision::Box};
  const unsigned original_type=a.tree?2u:(a.vegetation==3?1u:0u);if(index>=builtin_count||d.vegetation!=original_type){a.tree=d.vegetation==2;a.vegetation=a.tree?1:(d.vegetation==1?3:0);}a.lods=int(d.lods);a.distance=d.distance;a.lod_split=d.split;a.prop_lod_fade=d.lods==2;a.distance_fade=true;if(d.lods==1)a.cast_shadow=false;
  if(d.collision!=4)a.collision=d.collision==0?Collision::None:(d.collision==2?Collision::Circle:(d.collision==3?Collision::Trunk:Collision::Box));
  a.trunk_radius=std::max(a.trunk_radius,.3f);
  if(index>=builtin_count){a.cast_shadow=d.lods>1&&(std::strncmp(d.name,"nm_",3)==0||battlefield_corpse_alias(d.name));a.grounding=d.vegetation?Grounding::Root:Grounding::Support;a.max_elevation=3;a.named_near_file=a.tree;}
  next[index]=a;next[index].name=nullptr;next[index].stem=nullptr;
 }
 registered_names=std::move(names);registered_stems=std::move(stems);
 for(size_t i=0;i<count;i++){size_t index=0;for(;index<builtin_count;index++)if(registered_names[i]==builtins[index].name)break;if(index==builtin_count){for(index=builtin_count;index<active;index++)if(!next[index].name)break;}
  next[index].name=index<builtin_count?builtins[index].name:registered_names[i].c_str();next[index].stem=registered_stems[i].c_str();}
 aliases=next;active_alias_count=active;if(++alias_revision==0)alias_revision=1;if(error&&capacity)*error=0;return true;
}

void world_scenery_preview(WorldScenery *scene,size_t record,int lod){
 if(!scene)return;
 if(record>=scene->placements.size()||lod<0||lod>=aliases[scene->placements[record].alias].lods){scene->preview_record=SIZE_MAX;scene->preview_lod=-1;return;}
 scene->preview_record=record;scene->preview_lod=lod;
}
void world_scenery_editor_clear(WorldScenery *scene){
    if(!scene||scene->borrow_source)return;
    for(const auto record:scene->editor_records)scene->placements[record]=scene->editor_originals[record];
    scene->editor_records.clear();scene->stats.editor_preview_instances=0;
}
bool world_scenery_editor_transforms(WorldScenery *scene,const WorldSceneryEditorTransform *values,size_t count){
    if(!scene||scene->borrow_source||count>scene->placements.size()||(count&&!values))return false;
    if(++scene->editor_epoch==0){std::fill(scene->editor_seen.begin(),scene->editor_seen.end(),0);++scene->editor_epoch;}
    // Validate every input before changing the current preview. Epoch marks
    // reject duplicate records in linear time without allocating a scratch set.
    for(size_t i=0;i<count;i++){
        const auto &v=values[i];
        if(v.record>=scene->placements.size()||scene->editor_seen[v.record]==scene->editor_epoch||
           !std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.yaw)||
           std::abs(v.x)>1e8||std::abs(v.z)>1e8||std::abs(v.y)>1e6||
           !std::isfinite(v.scale)||v.scale<=0||v.scale>32||
           !std::isfinite(v.offset_y)||std::abs(v.offset_y)>WORLD_OBJECT_OFFSET_LIMIT||
           !vw_transform::extra_valid(v.pitch*180/pi,v.roll*180/pi,v.sx,v.sy,v.sz))return false;
        scene->editor_seen[v.record]=scene->editor_epoch;
    }
    world_scenery_editor_clear(scene);
    for(size_t i=0;i<count;i++){
        const auto &v=values[i];auto &p=scene->placements[v.record];
        scene->editor_originals[v.record]=p;scene->editor_records.push_back(v.record);
        p.x=v.x;p.y=v.y;p.z=v.z;p.yaw=v.yaw;p.scale=v.scale;
        p.pitch=v.pitch;p.roll=v.roll;p.sx=v.sx;p.sy=v.sy;p.sz=v.sz;
        p.offset_y=v.offset_y;
    }
    scene->stats.editor_preview_instances=count;if(count)++scene->stats.editor_preview_updates;
    return true;
}
bool world_scenery_runtime_active(WorldScenery *scene,std::size_t record,bool active){if(!scene||scene->borrow_source||record>=scene->placements.size())return false;auto&value=scene->placements[record].active;if(value!=active){value=active;++scene->activity_revision;}return true;}
bool world_scenery_find_source_record(const WorldScenery *scene,std::size_t file,std::size_t source_record,std::size_t *record){if(!scene||!record)return false;for(std::size_t i=0;i<scene->placements.size();i++)if(scene->placements[i].source==file&&scene->placements[i].source_record==source_record){*record=i;return true;}return false;}
bool world_scenery_runtime_pose(WorldScenery *scene,std::size_t record,double x,float y,double z,float yaw){
    if(!scene||scene->borrow_source||record>=scene->placements.size()||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)||std::abs(x)>1e8||std::abs(z)>1e8||std::abs(y)>1e6)return false;
    auto &p=scene->placements[record];const float dy=y-p.y;p.x=x;p.y=y;p.z=z;p.yaw=yaw;
    for(std::size_t i=p.collider_first;i<p.collider_first+p.collider_count;i++){
        auto &c=scene->colliders[i];c.x=x;c.z=z;c.ymin+=dy;c.ymax+=dy;c.cosine=std::cos(yaw);c.sine=std::sin(yaw);
        if(c.circle){c.min_x=x-c.radius;c.max_x=x+c.radius;c.min_z=z-c.radius;c.max_z=z+c.radius;}
        else{c.min_x=c.min_z=1e30;c.max_x=c.max_z=-1e30;for(int iz=0;iz<2;iz++)for(int ix=0;ix<2;ix++){const double lx=ix?c.xmax:c.xmin,lz=iz?c.zmax:c.zmin,wx=x+c.cosine*lx+c.sine*lz,wz=z-c.sine*lx+c.cosine*lz;c.min_x=std::min(c.min_x,wx);c.max_x=std::max(c.max_x,wx);c.min_z=std::min(c.min_z,wz);c.max_z=std::max(c.max_z,wz);}}
    }return true;
}
bool world_scenery_editor_commit_moves(WorldScenery *scene,const WorldSceneryEditorTransform *values,size_t count){
    if(!count||!world_scenery_editor_transforms(scene,values,count))return false;
    for(const auto record:scene->editor_records){
        const auto &p=scene->placements[record],&old=scene->editor_originals[record];const auto &a=aliases[p.alias];
        // Authored interior decorations have an absolute pivot and no movement
        // collider. They must not be tested against outdoor ground support:
        // that used to force an entire scenery rebuild for every furniture move.
        const bool free_decoration=a.collision==Collision::None&&std::isfinite(old.pivot_y);
        if(free_decoration){
            const auto sample=world_atlas_sample(p.x,p.z);
            const auto g=std::find_if(scene->groups.begin(),scene->groups.end(),[&](const Group &group){return group.alias==p.alias&&group.source_lod==0;});
            float water_surface=0,water_depth=0;
            if(!std::isfinite(sample.height)||sample.height<.1f||
               g==scene->groups.end()||trail_clearance(p.x,p.z)<g->radius*p.scale+.35f||
               (world_atlas_local_water(p.x,p.z,&water_surface,&water_depth)&&p.y<=water_surface+.1f))return false;
        }else{
            if(extended(p)||extended(old)||p.scale!=old.scale||a.collision==Collision::Gate||a.collision==Collision::Village||a.collision==Collision::Drawbridge)return false;
            const auto sample=world_atlas_sample(p.x,p.z);
            if(!std::isfinite(sample.height)||sample.height<.1f||sample.shore_distance<.5f||world_atlas_local_water(p.x,p.z,nullptr,nullptr))return false;
            const auto g=std::find_if(scene->groups.begin(),scene->groups.end(),[&](const Group &group){return group.alias==p.alias&&group.source_lod==0;});
            if(g==scene->groups.end()||trail_clearance(p.x,p.z)<g->radius*p.scale+.35)return false;
            Placement grounded=p;grounded.y=sample.height-p.burial;
            if(rigid(p.alias))grounded.y=supported_height(grounded,*g);
            if(a.grounding==Grounding::GroundTop)grounded.y+=.02f-g->maximum[1]*p.scale;
            grounded.y+=p.elevation+p.offset_y;
            if(!std::isfinite(grounded.y)||std::abs(grounded.y-p.y)>.0001f)return false;
        }
        for(size_t i=0;i<scene->placements.size();i++)if(i!=record){const auto &other=scene->placements[i];if(other.alias==p.alias&&std::abs(other.elevation+other.offset_y-p.elevation-p.offset_y)<.01f&&std::hypot(other.x-p.x,other.z-p.z)<.01)return false;}
    }
    // The same validated yaw/root update used by runtime props preserves the
    // original local collision boxes and allocates no replacement resources.
    for(const auto record:scene->editor_records){
        const auto target=scene->placements[record];scene->placements[record]=scene->editor_originals[record];
        auto &committed=scene->placements[record];
        if(aliases[target.alias].collision==Collision::None&&std::isfinite(committed.pivot_y)){
            committed=target;committed.pivot_y=target.y;
        }else{
            (void)world_scenery_runtime_pose(scene,record,target.x,target.y,target.z,target.yaw);
            committed.offset_y=target.offset_y;
        }
    }
    scene->editor_records.clear();scene->stats.editor_preview_instances=0;return true;
}
